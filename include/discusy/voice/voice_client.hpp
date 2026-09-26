#pragma once

#include <memory>
#include <new>

#include <boost/asio.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/unordered/concurrent_flat_map.hpp>

#include "../types.hpp"
#include "../gateway_events.hpp" // includes events
#include "../enum_helpers.hpp"
#include "../urls.hpp"

#include "internal.hpp"
#include "voice_connection.hpp"
#include "../asio_helpers.hpp"

namespace discusy::voice {

struct voice_connection_data {
    snowflake channel_id{};
    std::string session_id{};
    std::shared_ptr<connection> conn{};
    bool refreshing{false};

    bool muted{false};
    bool deaf{false};
};

// using the template is janky but icba with forward references for now
template <typename botT>
class client {
private:
    botT& bot_;
    snowflake bot_id_{};
    
    void intents_check() {
        if (!contains_bit<intent::guild_voice_states>(bot_.cfg_.intents)) {
            throw std::runtime_error("Guild voice states intent required for voice! Add it yourself or set the using voice hint to true in the constructor of the bot");
        }

        if (!contains_bit<intent::guilds>(bot_.cfg_.intents)) {
            throw std::runtime_error("Guilds intent required for voice! Add it yourself or set the using voice hint to true in the constructor of the bot");
        }
    }

    struct connection_escalation_cb { 
        client* c;
        snowflake guild_id;
        
        void operator()(escalation_action ec) {
            boost::asio::post(c->bot_.io_ctx.executor_, [c = c, ec, guild_id = guild_id]() {
                if (ec == escalation_action::fatal) {
                    #ifdef DISCUSY_LOGGING
                    c->bot_.log("Fatal escalation for guild: {}", guild_id.value);
                    #endif
                    c->disconnect(guild_id);
                } else if (ec == escalation_action::requires_gateway_reconnect) {
                    #ifdef DISCUSY_LOGGING
                    c->bot_.log("Gateway reconnect escalation for guild: {}", guild_id.value);
                    #endif
                    c->request_session_refresh(guild_id);
                }
            });
        } 
    };

public:
    client(client&&) = delete;
    client& operator=(client&&) = delete;
    client(const client&) = delete;
    client& operator=(const client&) = delete;

    static constexpr auto required_intents = intent::guilds | intent::guild_voice_states;
    using connection = discusy::voice::connection;
    using connection_ref = std::shared_ptr<connection>;
    using data = voice_connection_data;

    // this does not mean the connection will be valid, just that it was created
    Callback<connection_ref> on_connection_created{bot_.io_ctx};

    struct join_options {
        bool muted{false};
        bool deaf{false};
        std::chrono::milliseconds timeout{5000};
        std::function<void(connection&)> on_create{};
    };
private:
    void request_session_refresh(const snowflake guild_id) {
        snowflake channel_id{};
        bool muted = false;
        bool deaf = false;
        bool found = false;

        connections_.visit(guild_id, [&](auto& entry) {
            if (entry.second.refreshing) return;
            entry.second.refreshing = true;
            channel_id = entry.second.channel_id;
            muted = entry.second.muted;
            deaf = entry.second.deaf;
            found = true;
        });

        if (!found) return;

        if (!channel_id) {
            clear_refreshing(guild_id);
            return;
        }

        #ifdef DISCUSY_LOGGING
        bot_.log("Requesting a fresh voice session for guild: {}", guild_id.value);
        #endif

        bot_.update_voice_state(send_event::update_voice_state{
            .guild_id{guild_id},
            .channel_id{channel_id},
            .self_mute = muted,
            .self_deaf = deaf,
        });
    }

    void clear_refreshing(const snowflake guild_id) {
        connections_.visit(guild_id, [](auto& entry) {
            entry.second.refreshing = false;
        });
    }

    static void destroy_connection(connection_ref conn) {
        if (!conn) return;

        auto strand = conn->strand_;
        boost::asio::post(strand, [conn = std::move(conn)]() mutable {
            conn->destruct();
            conn.reset();
        });
    }

    struct wait_audio_ready_op {
        connection_ref conn;
        ctx::io_context::strand_timer_t& timer;
        std::chrono::milliseconds timeout{};
        std::chrono::milliseconds waited{0};

        static constexpr auto interval = std::chrono::milliseconds{50};

        template <typename Self>
        void operator()(Self& self, boost::system::error_code ec = {}) {
            if (ec) {
                self.complete(ec, false);
                return;
            }

            if (!conn->is_ready_to_send()) {
                if (!conn->is_active()) {
                    self.complete(boost::asio::error::connection_aborted, false);
                    return;
                }
                if (waited >= timeout) {
                    self.complete(boost::asio::error::timed_out, false);
                    return;
                }

                waited += interval;
                timer.expires_after(interval);
                timer.async_wait(std::move(self));
                return;
            }

            self.complete(boost::system::error_code{}, true);
        }
    };

    template <discusy::asio::ctf<bool> CompletionToken = ctx::io_context::dct_t>
    static auto wait_audio_ready(
        connection_ref conn,
        ctx::io_context::strand_timer_t& timer,
        const std::chrono::milliseconds timeout,
        CompletionToken&& token = ctx::io_context::dct_t()) 
    {
        return boost::asio::async_compose<CompletionToken, void(boost::system::error_code, bool)>(
            wait_audio_ready_op{std::move(conn), timer, timeout},
            token,
            timer
        );
    }

    std::string make_endpoint(const std::string& endpoint) const {
        return endpoint.starts_with("wss")
            ? ulp::str::concat_strings(endpoint, urls::VOICE_GATEWAY_QUERY_PARAMS)
            : ulp::str::concat_strings("wss://", endpoint, urls::VOICE_GATEWAY_QUERY_PARAMS);
    }

    void register_persistent_listeners() {
        std::call_once(listeners_flag_, [this]() {
            on_voice_state_update_ = bot_.gateway_callbacks.on_voice_state_update.listen_system([this](const recieve_event::voice_state_update& e) {
                if ((e.user_id != bot_id_) || !e.guild_id) return;

                connection_ref dying;
                connection_ref needs_rebuild;
                connection_ref moved_conn;
                snowflake new_channel_id{};

                if (e.channel_id) {
                    bool matched = false;
                    connections_.visit(*e.guild_id, [&](auto& entry) {
                        matched = true;
                        auto& d = entry.second;
                        const bool was_refreshing = d.refreshing;
                        const bool same_session = (d.session_id == e.session_id);
                        const bool channel_changed = (d.conn && (d.conn->get_channel_id() != *e.channel_id)) || (d.channel_id != *e.channel_id);

                        d.channel_id = *e.channel_id;
                        d.session_id = e.session_id;
                        d.refreshing = false;

                        d.muted = e.self_mute;
                        d.deaf = e.self_deaf;

                        if (channel_changed && d.conn) {
                            moved_conn = d.conn;
                            new_channel_id = *e.channel_id;
                        }

                        if (was_refreshing && same_session) needs_rebuild = d.conn;
                    });

                    if (!matched || (!needs_rebuild && !moved_conn)) return;
                } else {
                    connections_.erase_if(*e.guild_id, [&](auto& entry) {
                        dying = std::move(entry.second.conn);
                        return true;
                    });
                }

                if (moved_conn) {
                    moved_conn->move_channel(new_channel_id);
                    bot_.api.get_channel(new_channel_id)([this, new_channel_id, conn = std::move(moved_conn)](const boost::system::error_code& ec, const auto& channel) {
                        if (ec || !channel || !channel->bitrate || (conn->get_channel_id() != new_channel_id)) return;

                        conn->set_bitrate(static_cast<std::uint32_t>(*channel->bitrate));
                    });
                }

                if (needs_rebuild) {
                    #ifdef DISCUSY_LOGGING
                    bot_.log("Refresh returned the same session for guild {}, rebuilding in place", e.guild_id->value);
                    #endif
                    needs_rebuild->rebuild_in_place();
                    return;
                }

                if (!dying) return;

                #ifdef DISCUSY_LOGGING
                bot_.log("Removed from voice in guild: {}", e.guild_id->value);
                #endif

                destroy_connection(std::move(dying));
            });

            on_voice_server_update_ = bot_.gateway_callbacks.on_voice_server_update.listen_system([this](const recieve_event::voice_server_update& e) {
                if (!e.endpoint) return;

                connection_ref conn;
                std::string session_id;
                snowflake channel_id{};

                connections_.visit(e.guild_id, [&](auto& entry) {
                    conn = entry.second.conn;
                    session_id = entry.second.session_id;
                    channel_id = entry.second.channel_id;
                    entry.second.refreshing = false;
                });

                if (!conn || session_id.empty()) return;

                conn->refresh(std::move(session_id), e.token, make_endpoint(*e.endpoint), channel_id);
            });

            on_channel_update_ = bot_.gateway_callbacks.on_channel_update.listen_system([this](const recieve_event::channel_update& e) {
                if (!e.bitrate || !e.guild_id) return;

                connection_ref conn;
                connections_.cvisit(*e.guild_id, [&](const auto& entry) {
                    if (entry.second.channel_id == e.id) {
                        conn = entry.second.conn;
                    }
                });

                if (conn) conn->set_bitrate(static_cast<std::uint32_t>(*e.bitrate));
            });

            on_shard_session_invalidated_ = bot_.state_.shard_session_invalidated([this](const std::uint32_t& shard_id) {
                const auto total_shards = bot_.total_shards_.load();
                if (total_shards == 0) return;

                std::vector<snowflake> affected;
                connections_.cvisit_all([&](const auto& entry) {
                    if (entry.first.guild_shard_id(total_shards) == shard_id) {
                        affected.emplace_back(entry.first);
                    }
                });

                for (const auto guild_id : affected) {
                    #ifdef DISCUSY_LOGGING
                    bot_.log("Shard {} re-identified, refreshing voice for guild: {}", shard_id, guild_id.value);
                    #endif
                    request_session_refresh(guild_id);
                }
            });
        });
    }

    template <typename Handler>
    struct join_state {
        Handler handler;
        client<botT>* self;
        snowflake guild_id;
        snowflake channel_id;
        join_options options;
        std::atomic<bool> completed{false};

        boost::asio::cancellation_type cancelled_type{boost::asio::cancellation_type::none};
        boost::asio::cancellation_signal child_signal;

        ctx::io_context::strand_t strand_;

        ctx::io_context::strand_timer_t timer;

        decltype(
            boost::asio::get_associated_allocator(handler, boost::asio::recycling_allocator<void>{})
        ) allocator;

        decltype(
            boost::asio::make_work_guard(boost::asio::get_associated_executor(handler, std::declval<ctx::io_context::executor_t>()))
        ) work_guard_;

        join_state(auto&& h, client<botT>* s, snowflake gid, snowflake cid, auto&& opt)
            : handler{std::forward<decltype(h)>(h)},
              self{s},
              guild_id{gid},
              channel_id{cid},
              options{std::forward<decltype(opt)>(opt)},
              strand_{s->bot_.io_ctx.make_strand()},
              timer{strand_},
              allocator{boost::asio::get_associated_allocator(handler, boost::asio::recycling_allocator<void>{})},
              work_guard_{boost::asio::make_work_guard(boost::asio::get_associated_executor(handler, s->bot_.io_ctx.executor_))} {}

        bool is_cancelled() const noexcept {
            return cancelled_type != boost::asio::cancellation_type::none;
        }

        void handle_cancel(std::shared_ptr<join_state> self_ptr, boost::asio::cancellation_type type) {
            if (type == boost::asio::cancellation_type::none) return;

            boost::asio::post(strand_, [self_ptr = std::move(self_ptr), type]() {
                self_ptr->cancelled_type = type;
                self_ptr->child_signal.emit(type);
            });
        }

        auto get_executor() {
            return boost::asio::get_associated_executor(handler, self->bot_.io_ctx.executor_);
        }

        auto get_immediate_executor() {
            return boost::asio::get_associated_immediate_executor(handler, self->bot_.io_ctx.executor_);
        }

        void complete(boost::system::error_code ec, connection_ref&& conn = nullptr) {
            auto slot = boost::asio::get_associated_cancellation_slot(handler);
            if (slot.is_connected()) slot.clear();
            work_guard_.reset();
            std::move(handler)(ec, std::move(conn));
        }

        void finish(std::shared_ptr<join_state> self_ptr, boost::system::error_code ec, connection_ref conn = nullptr) {
            if (completed.exchange(true, std::memory_order_acq_rel)) return;

            auto ex = get_executor();

            boost::asio::dispatch(ex,
                boost::asio::bind_allocator(allocator, [self_ptr = std::move(self_ptr), ec, conn = std::move(conn)]() mutable {
                    self_ptr->complete(ec, std::move(conn));
                })
            );
        }

        void operation_cancelled(std::shared_ptr<join_state> self_ptr) {
            self->disconnect(guild_id);
            finish(std::move(self_ptr), boost::asio::error::operation_aborted);
        }

        void early_finish(std::shared_ptr<join_state> self_ptr, boost::system::error_code ec, connection_ref conn = nullptr) {
            if (completed.exchange(true, std::memory_order_acq_rel)) return;

            auto ex_imm = get_immediate_executor();

            boost::asio::dispatch(ex_imm,
                boost::asio::bind_allocator(allocator, [self_ptr = std::move(self_ptr), ec, conn = std::move(conn)]() mutable {
                    self_ptr->complete(ec, std::move(conn));
                })
            );
        }

        void init(std::shared_ptr<join_state> self_ptr) {
            self->register_persistent_listeners();

            boost::asio::post(strand_, boost::asio::bind_allocator(allocator, [self_ptr = std::move(self_ptr)]() mutable {
                auto ptr = self_ptr.get();
                ptr->start(std::move(self_ptr));
            }));
        }

        void start(std::shared_ptr<join_state> self_ptr) {
            if (is_cancelled()) return operation_cancelled(std::move(self_ptr));

            connection_ref existing;
            self->connections_.cvisit(guild_id, [&](const auto& entry) {
                existing = entry.second.conn;
            });

            if (existing) {
                if (!self->bot_.update_voice_state(send_event::update_voice_state{
                    .guild_id{guild_id},
                    .channel_id{channel_id},
                    .self_mute = options.muted,
                    .self_deaf = options.deaf,
                })) {
                    return finish(std::move(self_ptr), boost::asio::error::not_connected);
                }

                if (existing) {
                    return step_wait_audio_ready_existing(std::move(self_ptr), std::move(existing));
                }

                return finish(std::move(self_ptr), boost::system::error_code{}, std::move(existing));
            }

            auto token_def = boost::asio::bind_executor(strand_, boost::asio::bind_allocator(allocator, boost::asio::deferred));

            auto voice_state_update = self->bot_.gateway_callbacks.on_voice_state_update.when_system([guild_id = guild_id, channel_id = channel_id, muted = options.muted, deaf = options.deaf, me = self->bot_id_](const recieve_event::voice_state_update& e) {
                return (e.user_id == me) && (e.guild_id.value_or(0) == guild_id) && (e.channel_id.value_or(0) == channel_id) && (e.self_mute == muted) && (e.self_deaf == deaf);
            }, token_def);

            auto voice_server_update = self->bot_.gateway_callbacks.on_voice_server_update.when_system([guild_id = guild_id](const recieve_event::voice_server_update& e) {
                return e.guild_id == guild_id;
            }, token_def);

            if (!self->bot_.update_voice_state(send_event::update_voice_state{
                .guild_id{guild_id},
                .channel_id{channel_id},
                .self_mute = options.muted,
                .self_deaf = options.deaf,
            })) {
                return finish(std::move(self_ptr), boost::asio::error::not_connected);
            }

            timer.expires_after(options.timeout);

            boost::asio::experimental::make_parallel_group(
                timer.async_wait(),
                boost::asio::experimental::make_parallel_group(
                    std::move(voice_state_update),
                    std::move(voice_server_update)
                ).async_wait(
                    boost::asio::experimental::wait_for_all(),
                    token_def
                )
            ).async_wait(
                boost::asio::experimental::wait_for_one(),
                boost::asio::bind_executor(strand_, 
                    boost::asio::bind_cancellation_slot(
                        child_signal.slot(),
                        boost::asio::bind_allocator(
                            allocator,
                            [self_ptr = std::move(self_ptr)](
                                auto order,
                                boost::system::error_code timer_ec,
                                auto events_order,
                                boost::system::error_code vs_ec,
                                recieve_event::voice_state_update&& voice_state_update_event,
                                boost::system::error_code vserver_ec,
                                recieve_event::voice_server_update&& voice_server_update_event
                            ) mutable {
                                auto ptr = self_ptr.get();
                                ptr->step_handle_parallel_result(
                                    std::move(self_ptr),
                                    order,
                                    timer_ec,
                                    events_order,
                                    vs_ec,
                                    std::move(voice_state_update_event),
                                    vserver_ec,
                                    std::move(voice_server_update_event)
                                );
                            }
                        )
                    )
                )
            );
        }

        void step_wait_audio_ready_existing(std::shared_ptr<join_state> self_ptr, connection_ref existing) {
            if (is_cancelled()) return operation_cancelled(std::move(self_ptr));

            self->wait_audio_ready(
                existing,
                timer,
                self->audio_ready_timeout_.load(std::memory_order_relaxed),
                boost::asio::bind_cancellation_slot(
                    child_signal.slot(),
                    boost::asio::bind_allocator(
                        allocator,
                        [self_ptr = std::move(self_ptr), existing = std::move(existing)](const boost::system::error_code& ready_ec, bool is_ready) mutable {
                            auto ptr = self_ptr.get();
                            if (ptr->is_cancelled()) return ptr->operation_cancelled(std::move(self_ptr));
                            if (ready_ec || !is_ready) {
                                return ptr->finish(std::move(self_ptr), ready_ec ? ready_ec : boost::asio::error::timed_out);
                            }
                            ptr->finish(std::move(self_ptr), boost::system::error_code{}, std::move(existing));
                        }
                    )
                )
            );
        }

        void step_handle_parallel_result(
            std::shared_ptr<join_state> self_ptr,
            auto order,
            boost::system::error_code timer_ec,
            auto,
            boost::system::error_code vs_ec,
            recieve_event::voice_state_update&& voice_state_update_event, //NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
            boost::system::error_code vserver_ec,
            recieve_event::voice_server_update&& voice_server_update_event //NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
        ) {
            if (is_cancelled()) return operation_cancelled(std::move(self_ptr));

            if (order[0] == 0 || vs_ec || vserver_ec || !voice_server_update_event.endpoint) {
                return finish(std::move(self_ptr), timer_ec ? timer_ec : (vs_ec ? vs_ec : (vserver_ec ? vserver_ec : boost::asio::error::timed_out)), nullptr);
            }

            connection_ref conn;
            try {
                conn = std::allocate_shared<connection>(
                    allocator,
                    self->bot_.io_ctx,
                    self->bot_.state_,
                    guild_id,
                    channel_id,
                    self->bot_.get_user_id(),
                    voice_state_update_event.session_id,
                    voice_server_update_event.token,
                    self->make_endpoint(*voice_server_update_event.endpoint), 
                    connection_escalation_cb{self, guild_id}
                );
            } 
            catch (const boost::system::system_error& e) {
                #ifdef DISCUSY_LOGGING
                log::Logger{}("[voice_client] system error: {}", e.what());
                #endif
                return finish(std::move(self_ptr), e.code(), nullptr);
            }
            catch (const std::bad_alloc& e) {
                #ifdef DISCUSY_LOGGING
                log::Logger{}("[voice_client] bad_alloc: {}", e.what());
                #endif
                return finish(std::move(self_ptr), boost::asio::error::no_memory, nullptr);
            }
            #ifdef DISCUSY_LOGGING
            catch (const std::exception& e) {
                log::Logger{}("[voice_client] error: {}", e.what());
                return finish(std::move(self_ptr), boost::asio::error::fault, nullptr);
            }
            #endif
            catch (...) {
                #ifdef DISCUSY_LOGGING
                log::Logger{}("[voice_client] unknown error");
                #endif
                return finish(std::move(self_ptr), boost::asio::error::fault, nullptr);
            }

            if (!conn) return finish(std::move(self_ptr), boost::asio::error::no_memory, nullptr);

            if (self_ptr->options.on_create) {
                self_ptr->options.on_create(*conn);
            }
            self->on_connection_created.fire(connection_ref{conn});
            conn->start();

            connection_ref replaced;
            data new_entry{
                .channel_id = channel_id,
                .session_id{std::move(voice_state_update_event.session_id)},
                .conn = conn,
                .refreshing = false,
                .muted = options.muted,
                .deaf = options.deaf,
            };

            self->connections_.try_emplace_or_visit(
                guild_id,
                new_entry,
                [&](auto& entry) {
                    replaced = std::move(entry.second.conn);
                    entry.second = std::move(new_entry);
                }
            );

            discusy::voice::client<botT>::destroy_connection(std::move(replaced));

            step_wait_audio_ready_new(std::move(self_ptr), std::move(conn));
        }

        void step_wait_audio_ready_new(std::shared_ptr<join_state> self_ptr, connection_ref conn) {
            if (is_cancelled()) return operation_cancelled(std::move(self_ptr));

            if (!conn) {
                return finish(std::move(self_ptr), boost::asio::error::fault, nullptr);
            }

            self->bot_.api.get_channel(channel_id)([channel_id = this->channel_id, conn](const boost::system::error_code& ec, const auto& channel) {
                if (ec || !channel || !channel->bitrate || (conn->get_channel_id() != channel_id)) return;

                conn->set_bitrate(static_cast<std::uint32_t>(*channel->bitrate));
            });

            self->wait_audio_ready(
                conn,
                timer,
                self->audio_ready_timeout_.load(std::memory_order_relaxed),
                boost::asio::bind_cancellation_slot(
                    child_signal.slot(),
                    boost::asio::bind_allocator(
                        allocator,
                        [self_ptr = std::move(self_ptr), conn](const boost::system::error_code& ready_ec, bool is_ready) mutable {
                            auto ptr = self_ptr.get();
                            if (ptr->is_cancelled()) return ptr->operation_cancelled(std::move(self_ptr));

                            if (ready_ec || !is_ready) {
                                #ifdef DISCUSY_LOGGING
                                ptr->self->bot_.log("Voice connection never became ready to transmit for guild: {}", ptr->guild_id.value);
                                #endif

                                ptr->self->disconnect(ptr->guild_id);
                                return ptr->finish(std::move(self_ptr), ready_ec ? ready_ec : boost::asio::error::timed_out, nullptr);
                            }

                            ptr->finish(std::move(self_ptr), boost::system::error_code{}, std::move(conn));
                        }
                    )
                )
            );
        }
    };

public:
    explicit client(botT& bot) noexcept : bot_{bot} {}

    // Things to take note: on_create is only called if a new connection is created and an old connection isnt being moved
    template <discusy::asio::ctf<connection_ref> CompletionToken = ctx::io_context::dct_t>
    [[nodiscard]] auto join(
        const snowflake guild_id, 
        const snowflake channel_id, 
        join_options options = join_options{},
        CompletionToken&& token = ctx::io_context::dct_t()
    ) {
        intents_check();

        return boost::asio::async_initiate<CompletionToken, void(boost::system::error_code, connection_ref)>(
            [](discusy::asio::chf<connection_ref> auto&& handler, client<botT>* self, snowflake guild_id, snowflake channel_id, auto&& options) {
                using handler_t = std::decay_t<decltype(handler)>;
                using state_type = join_state<handler_t>;

                auto alloc = boost::asio::get_associated_allocator(
                    handler,
                    boost::asio::recycling_allocator<void>{}
                );

                std::shared_ptr<state_type> state = std::allocate_shared<state_type>(
                    alloc,
                    std::forward<decltype(handler)>(handler),
                    self, guild_id, channel_id, std::forward<decltype(options)>(options)
                );

                auto slot = boost::asio::get_associated_cancellation_slot(state->handler);

                if (slot.is_connected()) {
                    slot.assign([weak_state = std::weak_ptr<state_type>{state}, raw = state.get()](boost::asio::cancellation_type type) {
                        if (std::shared_ptr<state_type> st = weak_state.lock()) raw->handle_cancel(std::move(st), type);
                    });
                }

                state_type* raw = state.get();
                raw->init(std::move(state));
            },
            token,
            this, guild_id, channel_id, std::move(options)
        );
    }

    void set_audio_ready_timeout(const std::chrono::milliseconds timeout) noexcept {
        audio_ready_timeout_.store(timeout, std::memory_order_relaxed);
    }

    bool disconnect(const snowflake guild_id) {
        connection_ref conn;
        connections_.erase_if(guild_id, [&](auto& entry) {
            conn = std::move(entry.second.conn);
            return true;
        });

        const auto success = bot_.update_voice_state(send_event::update_voice_state{
            .guild_id{guild_id}, // channel id is null by default
        });

        destroy_connection(std::move(conn));

        return success;
    }

    [[nodiscard]] connection_ref get(const snowflake guild_id) const {
        connection_ref conn;
        connections_.cvisit(guild_id, [&](const auto& entry) {
            conn = entry.second.conn;
        });
        return conn;
    }

    ~client() {
        if (on_voice_state_update_ != 0UZ) bot_.gateway_callbacks.on_voice_state_update.unregister(on_voice_state_update_);
        if (on_voice_server_update_ != 0UZ) bot_.gateway_callbacks.on_voice_server_update.unregister(on_voice_server_update_);
        if (on_channel_update_ != 0UZ) bot_.gateway_callbacks.on_channel_update.unregister(on_channel_update_);
        if (on_shard_session_invalidated_ != 0UZ) bot_.state_.shard_session_invalidated.unregister(on_shard_session_invalidated_);

        std::vector<connection_ref> dying;
        dying.reserve(connections_.size());
        connections_.erase_if([&](auto& entry) {
            if (entry.second.conn) {
                dying.emplace_back(std::move(entry.second.conn));
            }
            return true;
        });

        for (auto& conn : dying) {
            destroy_connection(std::move(conn));
        }
    }

    friend botT;
    
private:
    void set_id(snowflake id) {
        bot_id_ = id;
        register_persistent_listeners();
    }

    std::once_flag listeners_flag_{};
    callback_id on_voice_state_update_{0};
    callback_id on_voice_server_update_{0};
    callback_id on_channel_update_{0};
    callback_id on_shard_session_invalidated_{0};

    std::atomic<std::chrono::milliseconds> audio_ready_timeout_{std::chrono::seconds{10}};

    boost::unordered::concurrent_flat_map<snowflake, voice_connection_data> connections_;
};

}
