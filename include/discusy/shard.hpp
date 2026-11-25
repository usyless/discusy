#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <glaze/glaze.hpp>

#include <usylibpp/strings.hpp>

#include "types.hpp"
#include "zstd.hpp"
#include "urls.hpp"
#include "opcode.hpp"
#include "gateway_events.hpp" // includes events
#ifdef DISCUSY_USE_ETF
#include "etf.hpp"
#else
#include "json.hpp"
#endif
#include "io_context.hpp"
#include "log.hpp" // IWYU pragma: keep
#include "random.hpp"
#include "websocket_client.hpp"
#include "config.hpp"
#include "state.hpp"
#include "asio_helpers.hpp"

namespace discusy {

#ifdef DISCUSY_USE_ETF
inline constexpr bool is_binary_payload = true;
namespace gateway_protocol = etf;
#else
inline constexpr bool is_binary_payload = false;
namespace gateway_protocol = json;
#endif

// Don't construct this class yourself, make a discusy::bot instead
class shard : public std::enable_shared_from_this<shard> {
public:
    shard(const shard& other) = delete;
    shard& operator=(const shard& other) = delete;
    shard(shard&& other) = delete;
    shard& operator=(shard&& other) = delete;

    explicit shard(ctx::io_context& ctx, std::uint32_t shard_id, std::uint32_t total_shards, const config& cfg, state& state);

    void start();

    void send_text_ratelimited(std::string text);

    [[nodiscard]] bool send_request_guild_members(const send_event::request_guild_members& req);
    [[nodiscard]] bool send_request_soundboard_sounds(const send_event::request_soundboard_sounds& req);
    [[nodiscard]] bool send_request_channel_info(const send_event::request_channel_info& req);
    [[nodiscard]] bool send_update_voice_state(const send_event::update_voice_state& req);
    // dont use this, re-use same json with send text ratelimited
    [[nodiscard]] bool send_update_presence(const send_event::update_presence& req);

    // must be called from its own strand
    [[nodiscard]] std::vector<std::string> extract_pending_payloads();

    // must be called from its own strand, which is the websockets strand
    void shutdown();

    // must be called from its own strand, which is the websockets strand
    ~shard() noexcept;

private:
    template <typename P, typename T>
    [[nodiscard]] bool send_request(const T& req);

    #ifdef DISCUSY_LOGGING
    template <typename ...Args>
    void log(const std::format_string<Args...>& s, Args&&... args) {
        log::Logger{}("{}{}", ulp::str::concat_strings("[discusy shard ", discusy::number_str{shard_id_}, "] "), std::format(s, std::forward<Args>(args)...));
    }
    #endif

    void stop();
    void handle_ws_open();
    void handle_ws_connect_ready();
    void handle_ws_close(std::uint16_t code);
    void handle_ws_message(std::string_view msg, bool);
    void setup_gateway_callbacks();
    void reconnect_internal();
    void reconnect(bool try_resume, bool force = false);
    void handle_gateway_payload(std::string& json_payload);
    void send_identify();
    void send_resume();
    [[nodiscard]] bool send_heartbeat(bool check_ack = true);
    void start_heartbeat(std::chrono::milliseconds interval);
    void stop_heartbeat();
    void handle_ready(const recieve_event::ready& d);
    void post_handle_ready();
    void handle_resumed(const recieve_event::resumed&);

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
    using ws_client_t = ws::websocket_client;

    std::shared_ptr<ws_client_t> ws_shared_;
    ws_client_t& ws_;

    const std::uint32_t shard_id_;
    const std::uint32_t total_shards_;
    const config& cfg_;

    discusy::zstd::stream_decompressor inflater_;
    std::string decompressed_buffer_;
    std::int64_t seq_{-1};

    ctx::io_context::strand_timer_t hb_timer_;
    std::uint64_t hb_timer_gen_{0};
    std::string session_id_;
    std::string resume_gateway_url_;

    ctx::io_context::strand_timer_t reconnect_timeout_timer_;
    std::uint64_t reconnect_timer_gen_{0};

    std::chrono::milliseconds reconnect_backoff_time_{0};

    bool destructed_{false};
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
