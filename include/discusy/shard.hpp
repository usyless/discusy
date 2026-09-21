#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <mutex>
#include <new>
#include <vector>
#include <utility>
#include <stop_token>

#include <boost/asio.hpp>
#include <boost/asio/stream_file.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

#include <glaze/glaze.hpp>

#include <usylibpp/strings.hpp>

#include "types.hpp"
#include "zstd.hpp"
#include "urls.hpp"
#include "opcode.hpp"
#include "gateway_events.hpp" // includes events
#include "json.hpp"
#include "io_context.hpp"
#include "log.hpp" // IWYU pragma: keep
#include "random.hpp"
#include "websocket_client.hpp"
#include "config.hpp"
#include "state.hpp"
#include "asio_helpers.hpp"

namespace discusy {

// Don't construct this class yourself, make a discusy::bot instead
// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
class shard {
public:
    // delete copy constructors
    shard(const shard& other) = delete;
    shard& operator=(const shard& other) = delete;

    explicit shard(ctx::io_context& ctx, const std::uint32_t shard_id, const std::uint32_t total_shards, const config& cfg, state& state) : 
        state_{state},
        io_ctx_{ctx},
        strand_{io_ctx_.make_strand()},
        ws_shared_{std::make_shared<ws_client_t>(strand_, state_.ssl_ctx)},
        ws_{*ws_shared_},
        shard_id_{shard_id},
        total_shards_{total_shards},
        cfg_{cfg},
        hb_timer_{strand_},
        reconnect_timeout_timer_{strand_}
    {
        setup_gateway_callbacks();
        ws_.pause();

        boost::asio::post(strand_, [this]() {
            reconnect(false); // gives us the logic in case of failure to open
            // already running via the pool
        });
    }

    void send_text_ratelimited(std::string text) {
        #ifdef DISCUSY_LOGGING
        log("Queing websocket send: {}", text);
        #endif
        boost::asio::dispatch(strand_, [this, text = std::move(text)]() mutable {
            ws_.send_ratelimited(std::move(text));
        });
    }

    template <typename P, typename T>
    [[nodiscard]] bool send_request(T& req) {
        P payload{req};
        std::string json;
        if (json::write_json(payload, json, json_logger)) return false; // not using ctx as not thread safe

        send_text_ratelimited(std::move(json));
        return true;
    }

    #pragma push_macro("MAKE_SEND_REQ")
    #undef MAKE_SEND_REQ

    #define MAKE_SEND_REQ(name) \
    [[nodiscard]] bool send_##name(auto&& req) { \
        return send_request<send_event::name##_payload>(req); \
    }

    MAKE_SEND_REQ(request_guild_members)
    MAKE_SEND_REQ(request_soundboard_sounds)
    MAKE_SEND_REQ(request_channel_info)
    MAKE_SEND_REQ(update_voice_state)
    // dont use this, re-use same json with send text ratelimited
    MAKE_SEND_REQ(update_presence)

    #undef MAKE_SEND_REQ
    #pragma pop_macro("MAKE_SEND_REQ")

    // must be called from its own strand
    [[nodiscard]] std::vector<std::string> extract_pending_payloads() {
        return ws_.extract_pending_payloads();
    }

    // must be called from its own strand, which is the websockets strand
    ~shard() {
        stop_heartbeat();
        ws_.stop();
        ws_.pause();

        ws_.on_connect_ready(connect_cb{nullptr});
        ws_.on_close(close_cb{nullptr});
        ws_.on_message(message_cb{nullptr});
        ws_.on_open(open_cb{nullptr});

        ws_.close();
        ws_.clear();
    }

private:
    #ifdef DISCUSY_LOGGING
    template <typename ...Args>
    void log(const std::format_string<Args...>& s, Args&&... args) {
        log::Logger{}("{}{}", ulp::str::concat_strings("[discusy shard ", ulp::str::to_string_view(shard_id_), "] "), std::format(s, std::forward<Args>(args)...));
    }
    #endif

    void stop() {
        reconnecting_ = true;
        stop_heartbeat();
        ws_.pause();
        reconnect_timeout_timer_.cancel();
    }

    void handle_ws_open() {
        #ifdef DISCUSY_LOGGING
        log("Websocket open");
        #endif
        reconnecting_ = false;
        reconnect_backoff_time_ = std::chrono::milliseconds{0};
    }

    void handle_ws_connect_ready() {
        awaiting_connect_ready_ = false;

        reconnecting_ = true;
        stop_heartbeat();
        ws_.pause();

        if (!try_resume_) {
            session_id_.clear();
            resume_gateway_url_.clear();
            seq_ = -1;
        }
        
        inflater_.reset();

        std::string_view connect_url = state_.gateway_url;
        if (try_resume_ && !resume_gateway_url_.empty()) {
            connect_url = resume_gateway_url_;
        }
        
        #ifdef DISCUSY_LOGGING
        log("Re/connecting to: {}", connect_url);
        #endif

        if (!ws_.connect(connect_url)) {
            #ifdef DISCUSY_LOGGING
            log("Failed to start a connection to {}, backing off", connect_url);
            #endif
            reconnect(try_resume_);
        }
    }

    void handle_ws_close(const std::uint16_t code) {
        #ifdef DISCUSY_LOGGING
        log("Websocket close: {}", code);
        #endif

        stop_heartbeat();

        if (awaiting_connect_ready_) {
            #ifdef DISCUSY_LOGGING
            log("Close was ours, waiting for connect ready");
            #endif
            return;
        }

        bool fatal = false;
        bool should_resume = true;

        switch (static_cast<CloseCode>(code)) {
            case CloseCode::InvalidIntents: {
                #ifdef DISCUSY_LOGGING
                log("Invalid intents sent!");
                #endif
                // no break on purpose
            }
            case CloseCode::DisallowedIntents: {
                #ifdef DISCUSY_LOGGING
                log("Disallowed intents sent!");
                #endif
                // no break on purpose
            }
            case CloseCode::AuthenticationFailed:
            case CloseCode::InvalidShard:
            case CloseCode::ShardingRequired:
            case CloseCode::InvalidApiVersion:
                fatal = true; 
                break;
            case CloseCode::NotAuthenticated:
            case CloseCode::AlreadyAuthenticated:
            case CloseCode::InvalidSeq:
            case CloseCode::SessionTimedOut:
                should_resume = false; 
                break;
            default:
                should_resume = true; 
                break;
        }

        if (fatal) {
            #ifdef DISCUSY_LOGGING
            log("Fatal close code {}, shutting down shard.", +code);
            #endif
            stop();
            if (static_cast<CloseCode>(code) == CloseCode::ShardingRequired) {
                state_.sharding_required->request_stop();
            } else {
                state_.fatal_error->request_stop();
            }
            return;
        }

        if (session_id_.empty()) {
            should_resume = false;
        }

        if (should_resume) {
            ++consecutive_resume_failures_;
            if (consecutive_resume_failures_ >= 3) {
                #ifdef DISCUSY_LOGGING
                log("Multiple resume attempts failed ({}), forcing a new session", consecutive_resume_failures_);
                #endif
                should_resume = false;
                consecutive_resume_failures_ = 0;
                session_id_.clear();
                resume_gateway_url_.clear();
                seq_ = -1;
            }
        } else {
            consecutive_resume_failures_ = 0;
            session_id_.clear();
            resume_gateway_url_.clear();
            seq_ = -1;
        }

        stop();

        reconnect(should_resume);
    }

    void handle_ws_message(const std::string_view msg, bool binary) {
        try {
            if (binary) {
                handle_gateway_payload(inflater_.push(msg));
            } else {
                handle_gateway_payload(std::string{msg});
            }
        }
        catch (const std::bad_alloc& e) {
            inflater_.reset();
            #ifdef DISCUSY_LOGGING
            log("on_message bad_alloc: {}", e.what());
            #endif
        }
        #ifdef DISCUSY_LOGGING
        catch (const std::exception& e) {
            log("on_message exception: {}", e.what());
        }
        #endif
        catch (...) {
            #ifdef DISCUSY_LOGGING
            log("on_message unknown exception");
            #endif
        }
    }

    void setup_gateway_callbacks() {
        ws_.on_open(open_cb{this});
        ws_.on_connect_ready(connect_cb{this});
        ws_.on_close(close_cb{this});
        ws_.on_message(message_cb{this});
    }

    void reconnect_internal() {
        reconnect_scheduled_ = false;

        reconnect_timeout_timer_.cancel();
        reconnect_timeout_timer_.expires_after(std::chrono::seconds{25});
        reconnect_timeout_timer_.async_wait([this](asio::ec_t ec) {
            if (ec) return;
            #ifdef DISCUSY_LOGGING
            log("Reconnect timeout expired, attempting to reconnect...");
            #endif

            stop();

            // maybe this should eventually fail and stop the bot? add compile time option for it?
            reconnect(try_resume_, true);
        });

        awaiting_connect_ready_ = true;
        ws_.close();
    }

    void reconnect(const bool try_resume, const bool force = false) {
        if (reconnect_scheduled_ && !force) {
            #ifdef DISCUSY_LOGGING
            log("Reconnect already scheduled, not stacking another");
            #endif
            return;
        }

        reconnecting_ = true;
        reconnect_timeout_timer_.cancel();

        const auto delay = reconnect_backoff_time_;

        if (reconnect_backoff_time_.count() == 0) {
            reconnect_backoff_time_ = std::chrono::milliseconds{1000};
        } else {
            reconnect_backoff_time_ *= 2;
            reconnect_backoff_time_ = std::chrono::milliseconds{std::min<decltype(reconnect_backoff_time_.count())>(20000, reconnect_backoff_time_.count())};
        }

        try_resume_ = try_resume;
        reconnect_scheduled_ = true;

        if (delay.count() == 0) {
            #ifdef DISCUSY_LOGGING
            log("Re/connecting immediately");
            #endif
            reconnect_internal();
        } else {
            #ifdef DISCUSY_LOGGING
            log("Re/connecting after: {}", delay);
            #endif
            reconnect_timeout_timer_.expires_after(delay);
            reconnect_timeout_timer_.async_wait([this](asio::ec_t ec) {
                if (!ec) reconnect_internal();
            });
        }
    }

    // NOLINTNEXTLINE(readability-function-size)
    void handle_gateway_payload(std::string&& json_payload) {
        #ifdef DISCUSY_LOGGING
        log("Handle gateway payload: {}", json_payload);
        #endif
        if (json_payload.empty()) return;

        recieve_event::payload_base e{};
        // try use quick parser first, then move onto glaze if it fails
        if (json::parse_payload_base(e, json_payload, json_logger)) {
            if (json::parse_json<json::glz_opts_partial_read>(e, json_payload, shared_json_ctx_, json_logger)) return;
        }

        if (e.s) seq_ = *e.s;

        switch (e.op) {
            case Opcode::Hello: {
                recieve_event::payload<recieve_event::hello> hello{};
                if (json::parse_json(hello, json_payload, shared_json_ctx_, json_logger) || hello.d.heartbeat_interval <= 0) {
                    stop();
                    reconnect(true, true);
                    return;
                }

                #ifdef DISCUSY_LOGGING
                log("HELLO interval={}",  hello.d.heartbeat_interval);
                #endif
                start_heartbeat(std::chrono::milliseconds{hello.d.heartbeat_interval});

                if (session_id_.empty()) { send_identify(); } 
                else { send_resume(); }
                
                reconnect_timeout_timer_.cancel();
                reconnect_scheduled_ = false;
                break;
            }

            case Opcode::Reconnect: {
                stop();
                reconnect(true, true);
                break;
            }

            case Opcode::HeartbeatAck: {
                recieved_heartbeat_ack_ = true;
                break;
            }

            case Opcode::Heartbeat: {
                #ifdef DISCUSY_LOGGING
                log("Recieved heartbeat from discord");
                #endif
                (void)send_heartbeat();
                break;
            }

            case Opcode::InvalidSession: {
                recieve_event::payload<recieve_event::invalid_session> invalid_session{};
                if (json::parse_json(invalid_session, json_payload, shared_json_ctx_, json_logger)) {
                    invalid_session.d = false;
                }

                stop();

                if (invalid_session.d) { // can resume
                    reconnect(true, true);
                } else {
                    session_id_.clear();
                    resume_gateway_url_.clear();
                    seq_ = -1;
                    try_resume_ = false;
                    consecutive_resume_failures_ = 0;

                    if (reconnect_backoff_time_ < std::chrono::seconds{2}) {
                        reconnect_backoff_time_ = std::chrono::seconds{2};
                    }
                    reconnect(false, true);
                }
                break;
            }

            case Opcode::Dispatch: {
                if (!e.t) break;
                switch (*e.t) {
                #define DISCUSY_X(UPPER, LOWER) \
                    case recieve_event::event::UPPER: { \
                        ([](auto* self, auto&& payload) { \
                            if constexpr (requires (typename recieve_event::payload<recieve_event::LOWER> p) { \
                                self->handle_##LOWER(p.d); \
                            }) { \
                                typename recieve_event::payload<recieve_event::LOWER> p{}; \
                                if (json::parse_json(p, payload, self->shared_json_ctx_, self->json_logger)) return; \
                                self->handle_##LOWER(p.d); \
                                self->state_.gateway_callbacks.on_##LOWER.fire(std::move(p.d)); \
                            } else { \
                                self->state_.gateway_callbacks.on_##LOWER.template fire_json<recieve_event::payload<recieve_event::LOWER>>(std::forward<decltype(payload)>(payload)); \
                            } \
                            if constexpr (requires { self->post_handle_##LOWER(); }) { \
                                self->post_handle_##LOWER(); \
                            } \
                        })(this, std::move(json_payload)); \
                        break; \
                    }

                DISCORD_GATEWAY_EVENTS
                #undef DISCUSY_X
                    default: {
                        #ifdef DISCUSY_LOGGING
                        log("Unknown dispatch recieved: {} -> How??? This shouldnt be possible", +*e.t);
                        #endif
                        break;
                    }
                }
                break;
            }

            default: {
                #ifdef DISCUSY_LOGGING
                log("Unknown opcode recieved: {}", +e.op);
                #endif
                break;
            }
        }
    }

    void send_identify() {
        send_event::identify identify{};
        send_event::identify_payload payload{identify};
        identify.token = cfg_.token;
        identify.intents = cfg_.intents;
        identify.shard = {shard_id_, total_shards_}; // maybe need to consider how new shards and stuff works?
        {
        std::shared_lock lock{state_.presence_mtx};
        if (state_.presence) {
            identify.presence.emplace(*state_.presence);
        }
        }

        std::string out;
        if (json::write_json(payload, out, shared_json_ctx_, json_logger)) {
            state_.fatal_error->request_stop(); // maybe too extreme?
            return;
        }

        #ifdef DISCUSY_LOGGING
        log("Sent IDENTIFY: {}", out);
        #endif
        ws_.send(std::move(out));
    }

    void send_resume() {
        send_event::resume resume{};
        send_event::resume_payload payload{resume};
        resume.token = cfg_.token;
        resume.session_id = session_id_;
        resume.seq = seq_;

        std::string out;
        if (json::write_json(payload, out, shared_json_ctx_, json_logger)) {
            state_.fatal_error->request_stop(); // maybe too extreme?
            return;
        }

        #ifdef DISCUSY_LOGGING
        log("Sent RESUME: {}", out);
        #endif
        ws_.send(std::move(out));
    }

    [[nodiscard]] bool send_heartbeat() {
        if (!recieved_heartbeat_ack_) {
            stop();
            reconnect(true, true);
            return false;
        }

        send_event::heartbeat hb{};
        send_event::heartbeat_payload payload{hb};
        const auto s = seq_;
        if (s >= 0) hb.emplace(s);

        std::string out;
        static constexpr glz::opts opts{.skip_null_members = false};
        if (json::write_json<opts>(payload, out, shared_json_ctx_, json_logger)) {
            state_.fatal_error->request_stop(); // maybe too extreme? but this will literally never happen
            return false;
        }

        #ifdef DISCUSY_LOGGING
        log("Sent heartbeat: {}", out);
        #endif
        recieved_heartbeat_ack_ = false;
        ws_.send(std::move(out));
        return true;
    }

    void start_heartbeat(const std::chrono::milliseconds interval) {
        recieved_heartbeat_ack_ = true; // initial state

        hb_timer_.expires_after(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::duration<double, std::milli>(interval) * rnd::jitter()));
        hb_timer_.async_wait([i = this, interval](this auto&& self, asio::ec_t ec) -> void {
            if (ec) return;

            if (i->send_heartbeat()) {
                i->hb_timer_.expires_at(i->hb_timer_.expiry() + interval);
                i->hb_timer_.async_wait(self);
            }
        });
    }

    void stop_heartbeat() {
        hb_timer_.cancel();
    }

    void handle_ready(const recieve_event::ready& d) {
        {
        std::scoped_lock lock{state_.details_mtx};
        state_.me = d.user;
        }

        std::call_once(*state_.ready_flag, [this, &d]() {
            std::scoped_lock lock{this->state_.details_mtx};
            this->state_.application_id = d.application.id.str();
        });

        const bool replaced_session = fired_shard_ready_;

        session_id_ = d.session_id;
        resume_gateway_url_ = ulp::str::concat_strings(d.resume_gateway_url, "/", urls::GATEWAY_QUERY_PARAMS);

        reconnect_backoff_time_ = std::chrono::milliseconds{0};
        consecutive_resume_failures_ = 0;
        try_resume_ = true;

        #ifdef DISCUSY_LOGGING
        log("READY app_id={}", state_.application_id);
        #endif

        ws_.resume();

        if (replaced_session) {
            state_.shard_session_invalidated.fire(std::uint32_t{shard_id_});
        }
    }

    void post_handle_ready() {
        if (!fired_shard_ready_) {
            fired_shard_ready_ = true;
            state_.shards_ready_counter->arrive();
        }
    }

    void handle_resumed(const recieve_event::resumed&) {
        reconnect_backoff_time_ = std::chrono::milliseconds{0};
        consecutive_resume_failures_ = 0;
        try_resume_ = true;
        #ifdef DISCUSY_LOGGING
        log("RESUMED");
        #endif
        ws_.resume();
    }

    #ifdef DISCUSY_LOGGING
    friend log::SelfLogger<shard>;
    log::SelfLogger<shard> json_logger{*this};
    #else
    log::Logger json_logger{};
    #endif

    state& state_;
    ctx::io_context& io_ctx_;
public:
    const ctx::io_context::strand_t strand_;
private:
    struct open_cb    { shard* s; void operator()() const { if(s != nullptr) s->handle_ws_open(); } };
    struct connect_cb { shard* s; void operator()() const { if(s != nullptr) s->handle_ws_connect_ready(); } };
    struct close_cb   { shard* s; void operator()(std::uint16_t c) const { if(s != nullptr) s->handle_ws_close(c); } };
    struct message_cb { shard* s; void operator()(std::string_view m, bool b) const { if(s != nullptr) s->handle_ws_message(m, b); } };

    using ws_client_t = ws::websocket_client<open_cb, close_cb, message_cb, connect_cb>;

    std::shared_ptr<ws_client_t> ws_shared_;
    ws_client_t& ws_;
    
    const std::uint32_t shard_id_;
    const std::uint32_t total_shards_;
    const config& cfg_;

    discusy::zstd::stream_decompressor inflater_;
    std::int64_t seq_{-1};

    ctx::io_context::strand_timer_t hb_timer_;
    std::string session_id_;
    std::string resume_gateway_url_;

    ctx::io_context::strand_timer_t reconnect_timeout_timer_;

    std::chrono::milliseconds reconnect_backoff_time_{0};

    bool reconnecting_{false};
    bool recieved_heartbeat_ack_{false};
    bool fired_shard_ready_{false};
    bool try_resume_{true};
    bool awaiting_connect_ready_{false};
    bool reconnect_scheduled_{false};
    std::uint32_t consecutive_resume_failures_{0};

    glz::context shared_json_ctx_{};
};

}