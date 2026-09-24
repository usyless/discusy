#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <mutex>
#include <filesystem>
#include <thread>
#include <vector>
#include <utility>
#include <memory>
#include <new>
#include <stop_token>
#include <fstream>
#include <functional>

#include <boost/asio.hpp>
#include <boost/asio/stream_file.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

#include <glaze/glaze.hpp>

#include <usylibpp/strings.hpp>

#include "macros.hpp" // IWYU pragma: keep
#include "types.hpp"
#include "urls.hpp"
#include "events.hpp"
#include "api_types.hpp"
#include "gateway_events.hpp" // includes events
#include "json.hpp"
#include "timers.hpp"
#include "coro.hpp"
#include "io_context.hpp"
#include "log.hpp" // IWYU pragma: keep
#include "ssl.hpp"
#include "config.hpp"
#include "state.hpp"
#include "http_api.hpp"
#include "shard.hpp"
#include "asio_helpers.hpp"

#include "api_constants.hpp" // IWYU pragma: export
#include <discusy/version.hpp> // IWYU pragma: export

#ifdef DISCUSY_VOICE
#include "voice/voice_client.hpp"
#endif

#ifdef WIN32
#include <windows.h>
#include <wincrypt.h>
#endif

using namespace boost::asio::experimental::awaitable_operators;

namespace discusy {

class bot {
public:
    // delete copy and move constructors
    bot(const bot& other) = delete;
    bot& operator=(const bot& other) = delete;
    bot(bot&& other) = delete;
    bot& operator=(bot&& other) = delete;

    explicit bot(std::uint32_t threads = 1) : 
        cfg_{setup_cfg(threads)}, 
        io_ctx{static_cast<int>(cfg_.threads)}, 
        state_{
            .client{state_.ssl_ctx, io_ctx},
            .timers{io_ctx},
            .gateway_callbacks{io_ctx},
            .shards_ready{io_ctx},
            .shard_session_invalidated{io_ctx},
        }, 
        gateway_callbacks{state_.gateway_callbacks},
        timers{state_.timers},
        on_shards_ready{state_.shards_ready},
        client{state_.client},
        api{state_, cfg_}
        #ifndef DISCUSY_NO_CACHES
        ,caches{state_.gateway_callbacks}
        #endif
        #ifdef DISCUSY_VOICE
        ,voice{*this}
        #endif
        ,http_only_{true}
    {
        construct();
    }

    explicit bot(config cfg, bool add_voice_intent = false) : 
        cfg_{setup_cfg(cfg, add_voice_intent)}, 
        io_ctx{static_cast<int>(cfg_.threads)}, 
        state_{
            .client{state_.ssl_ctx, io_ctx},
            .timers{io_ctx},
            .gateway_callbacks{io_ctx},
            .shards_ready{io_ctx},
            .shard_session_invalidated{io_ctx},
        }, 
        gateway_callbacks{state_.gateway_callbacks},
        timers{state_.timers},
        on_shards_ready{state_.shards_ready},
        client{state_.client},
        api{state_, cfg_}
        #ifndef DISCUSY_NO_CACHES
        ,caches{state_.gateway_callbacks}
        #endif
        #ifdef DISCUSY_VOICE
        ,voice{*this}
        #endif
    {
        construct();
    }

    explicit bot(const std::filesystem::path& cfg_path, opt<send_event::update_presence>&& initial_presence = std::nullopt, bool add_voice_intent = false) : 
        cfg_{setup_cfg(cfg_path, std::move(initial_presence), add_voice_intent)},
        io_ctx{static_cast<int>(cfg_.threads)}, 
        state_{
            .client{state_.ssl_ctx, io_ctx},
            .timers{io_ctx},
            .gateway_callbacks{io_ctx},
            .shards_ready{io_ctx},
            .shard_session_invalidated{io_ctx},
        }, 
        gateway_callbacks{state_.gateway_callbacks},
        timers{state_.timers},
        on_shards_ready{state_.shards_ready},
        client{state_.client},
        api{state_, cfg_}
        #ifndef DISCUSY_NO_CACHES
        ,caches{state_.gateway_callbacks}
        #endif
        #ifdef DISCUSY_VOICE
        ,voice{*this}
        #endif
    {
        construct();
    }

    void run(const bool wait = true) {
        state_.application_id.clear();
        state_.gateway_url.clear();

        state_.ready_flag.reset();
        state_.ready_flag.emplace();

        shards_restarting_.store(false);
        shards_stop_requested.store(false);

        state_.sharding_required.reset();
        state_.sharding_required.emplace();

        state_.sharding_required_cb.reset();

        state_.fatal_error.reset();
        state_.fatal_error.emplace();

        state_.fatal_error_cb.reset();

        state_.shards_ready_counter.reset();

        setup_clients(wait);
        if (!http_only_) {
            start_shards();
        }

        client.start();

        if (wait) {
            worker();
        } else {
            worker_threads.emplace_back(&bot::worker, this);
        }
    }

    template <typename Rep, typename Period, discusy::asio::ctf<> CompletionToken = ctx::io_context::dct_t>
    auto sleep(std::chrono::duration<Rep, Period> time, CompletionToken&& token = ctx::io_context::dct_t()) {
        return io_ctx.sleep(std::move(time), std::forward<CompletionToken>(token));
    }

    auto make_strand() {
        return io_ctx.make_strand();
    }

    // need to test this and following methods on shard restarts to be sure
    bool send_request_guild_members(send_event::request_guild_members members) {
        if (!members.guild_id) return false;

        {
        std::unique_lock lock{shards_mutex, std::chrono::milliseconds{500}};
        if (!lock.owns_lock()) return false;

        const auto total_shards = total_shards_.load();
        if (total_shards == 0 || shards_stop_requested.load() || shards_.empty() || (shards_.size() != total_shards)) {
            std::scoped_lock q_lock{pending_shard_operations_mtx_};
            pending_shard_operations_queue_.emplace_back([this, m = std::move(members)]() mutable {
                this->send_request_guild_members(std::move(m));
            });
            return true;
        }

        const auto shard_id = members.guild_id.guild_shard_id(total_shards);

        if (shard_id >= total_shards) return false;

        return shards_[shard_id]->send_request_guild_members(std::move(members));
        }
    }

    template <typename Rep, typename Period, discusy::asio::ctf<recieve_event::guild_members_chunk> CompletionToken = ctx::io_context::dct_t>
    auto request_guild_members(send_event::request_guild_members members, std::chrono::duration<Rep, Period> timeout, CompletionToken&& token = ctx::io_context::dct_t()) {
        return boost::asio::async_initiate<CompletionToken, void(boost::system::error_code, recieve_event::guild_members_chunk)>(
            [](discusy::asio::chf<recieve_event::guild_members_chunk> auto&& handler, bot* self, auto&& members, auto&& timeout) mutable {
                auto alloc = boost::asio::get_associated_allocator(
                    handler, 
                    boost::asio::recycling_allocator<void>{}
                );

                auto early_complete = [&handler, self, &alloc](boost::system::error_code ec, recieve_event::guild_members_chunk result = {}) mutable -> void {
                    auto exec = boost::asio::get_associated_immediate_executor(handler, self->io_ctx.executor_);

                    boost::asio::dispatch(exec,
                        boost::asio::bind_allocator(alloc, [h = std::move(handler), ec, res = std::move(result)]() mutable {
                            std::move(h)(ec, std::move(res));
                        })
                    );
                };

                if (!members.guild_id) return std::move(early_complete)(boost::asio::error::invalid_argument);

                using alloc_type = decltype(alloc);
                using char_alloc = std::allocator_traits<alloc_type>::template rebind_alloc<char>;
                using string_type = std::basic_string<char, std::char_traits<char>, char_alloc>;

                string_type nonce{alloc};
                if (!members.nonce || (members.nonce->size() > 32)) {
                    nonce.resize(32);
                    rnd::nonce(std::span<char, 32>{nonce.data(), 32});
                    members.nonce = std::string(nonce.data(), nonce.size());
                } else {
                    nonce.assign(members.nonce->data(), members.nonce->size());
                }

                struct chunk_data {
                    using member_alloc = std::allocator_traits<alloc_type>::template rebind_alloc<discusy::guild::guild_member>;
                    using not_found_alloc = std::allocator_traits<alloc_type>::template rebind_alloc<glz::raw_json_view>;
                    using presence_alloc = std::allocator_traits<alloc_type>::template rebind_alloc<discusy::recieve_event::presence::presence_update>;

                    std::vector<discusy::guild::guild_member, member_alloc> members;
                    opt<std::vector<glz::raw_json_view, not_found_alloc>> not_found{};
                    opt<std::vector<discusy::recieve_event::presence::presence_update, presence_alloc>> presences{};

                    using allocator_type = alloc_type;
                    chunk_data(const alloc_type& a = alloc_type{}) : members(a) {}
                    chunk_data(std::vector<discusy::guild::guild_member, member_alloc> m,
                               opt<std::vector<glz::raw_json_view, not_found_alloc>> nf,
                               opt<std::vector<discusy::recieve_event::presence::presence_update, presence_alloc>> pr,
                               const alloc_type& a = alloc_type{})
                        : members(std::move(m), a), not_found(std::move(nf)), presences(std::move(pr)) {}
                };

                using handler_t = std::decay_t<decltype(handler)>;

                struct chunk_state {
                    using chunk_data_alloc = std::allocator_traits<alloc_type>::template rebind_alloc<chunk_data>;
                    using bool_alloc = std::allocator_traits<alloc_type>::template rebind_alloc<bool>;

                    handler_t handler;
                    std::mutex mtx{};
                    recieve_event::guild_members_chunk accumulated{};
                    std::vector<chunk_data, chunk_data_alloc> chunks;
                    std::vector<bool, bool_alloc> received;
                    integer total_count{0};
                    integer received_count{0};
                    bool completed{false};
                    ctx::io_context::strand_timer_t timer;
                    boost::asio::cancellation_signal child_signal;

                    decltype(
                        boost::asio::make_work_guard(boost::asio::get_associated_executor(handler, std::declval<ctx::io_context::executor_t>()))
                    ) work_guard_;

                    void complete(boost::system::error_code ec, recieve_event::guild_members_chunk&& data = {}) {
                        auto slot = boost::asio::get_associated_cancellation_slot(handler);
                        if (slot.is_connected()) slot.clear();
                        work_guard_.reset();
                        std::move(handler)(ec, std::move(data));
                    }

                    chunk_state(alloc_type a, const ctx::io_context::strand_t& strand, handler_t handler_, const ctx::io_context::executor_t& default_exec) 
                        : handler{std::move(handler_)}, chunks(a), received(a), timer{strand}, work_guard_{boost::asio::get_associated_executor(handler, default_exec)} {}
                };

                auto exec_strand = self->io_ctx.make_strand();
                std::shared_ptr<chunk_state> state = std::allocate_shared<chunk_state>(alloc, alloc, exec_strand, std::forward<decltype(handler)>(handler), self->io_ctx.executor_);
                state->accumulated.nonce = std::string(nonce.data(), nonce.size());

                auto on_members_chunk = self->gateway_callbacks.on_guild_members_chunk.when_system([state, nonce = std::move(nonce), alloc](const recieve_event::guild_members_chunk& e) -> bool {
                    if (!e.nonce || (*e.nonce != std::string_view{nonce})) {
                        return false;
                    }

                    std::scoped_lock lock{state->mtx};
                    if (state->completed) return false;

                    if (state->accumulated.guild_id.value == 0) {
                        state->accumulated.guild_id = e.guild_id;
                    }

                    const auto idx = e.chunk_index;
                    const auto count = e.chunk_count;

                    if (count > 0 && state->chunks.size() < static_cast<size_t>(count)) {
                        state->chunks.resize(static_cast<size_t>(count), chunk_data{alloc});
                        state->received.resize(static_cast<size_t>(count), false);
                        state->total_count = count;
                    }

                    if (idx >= 0 && static_cast<size_t>(idx) < state->received.size()) {
                        if (!state->received[static_cast<size_t>(idx)]) {
                            using member_alloc = chunk_data::member_alloc;
                            using not_found_alloc = chunk_data::not_found_alloc;
                            using presence_alloc = chunk_data::presence_alloc;

                            state->received[static_cast<size_t>(idx)] = true;
                            state->chunks[static_cast<size_t>(idx)] = chunk_data{
                                std::vector<discusy::guild::guild_member, member_alloc>(e.members.begin(), e.members.end(), alloc),
                                e.not_found ? opt<std::vector<glz::raw_json_view, not_found_alloc>>(std::in_place, e.not_found->begin(), e.not_found->end(), alloc) : std::nullopt,
                                e.presences ? opt<std::vector<discusy::recieve_event::presence::presence_update, presence_alloc>>(std::in_place, e.presences->begin(), e.presences->end(), alloc) : std::nullopt,
                                alloc,
                            };
                            ++state->received_count;
                        }
                    } else {
                        state->accumulated.members.insert(
                            state->accumulated.members.end(),
                            e.members.begin(),
                            e.members.end()
                        );
                        ++state->received_count;
                    }

                    const bool all_received = (state->total_count > 0 && state->received_count >= state->total_count) ||
                                              (count <= 1) ||
                                              (idx + 1 >= count && state->received_count >= count);

                    if (all_received) {
                        state->completed = true;
                        state->accumulated.chunk_index = (count > 0) ? (count - 1) : 0;
                        state->accumulated.chunk_count = count;

                        for (auto& chunk : state->chunks) {
                            state->accumulated.members.insert(
                                state->accumulated.members.end(),
                                std::make_move_iterator(chunk.members.begin()),
                                std::make_move_iterator(chunk.members.end())
                            );
                            if (chunk.not_found) {
                                if (!state->accumulated.not_found) state->accumulated.not_found.emplace();
                                state->accumulated.not_found->insert(
                                    state->accumulated.not_found->end(),
                                    std::make_move_iterator(chunk.not_found->begin()),
                                    std::make_move_iterator(chunk.not_found->end())
                                );
                            }
                            if (chunk.presences) {
                                if (!state->accumulated.presences) state->accumulated.presences.emplace();
                                state->accumulated.presences->insert(
                                    state->accumulated.presences->end(),
                                    std::make_move_iterator(chunk.presences->begin()),
                                    std::make_move_iterator(chunk.presences->end())
                                );
                            }
                        }
                        state->chunks.clear();
                        return true;
                    }

                    return false;
                }, boost::asio::bind_executor(exec_strand, boost::asio::bind_allocator(alloc, boost::asio::deferred)));

                
                if (!self->send_request_guild_members(std::forward<decltype(members)>(members))) {
                    auto exec = boost::asio::get_associated_immediate_executor(state->handler, self->io_ctx.executor_);

                    boost::asio::dispatch(exec,
                        boost::asio::bind_allocator(alloc, [st = std::move(state)]() mutable {
                            st->complete(boost::asio::error::not_connected);
                        })
                    );
                    return;
                }

                auto slot = boost::asio::get_associated_cancellation_slot(state->handler);

                if (slot.is_connected()) {
                    slot.assign([weak_state = std::weak_ptr<chunk_state>{state}, exec_strand](boost::asio::cancellation_type type) {
                        if (std::shared_ptr<chunk_state> st = weak_state.lock()) {
                            if (type == boost::asio::cancellation_type::none) return;

                            boost::asio::post(exec_strand, [state_ptr = std::move(st), type]() {
                                state_ptr->child_signal.emit(type);
                            });
                        }
                    });
                }

                auto target_exec = boost::asio::get_associated_executor(state->handler, self->io_ctx.executor_);

                auto& timer = state->timer;
                timer.expires_after(timeout);
                boost::asio::cancellation_slot child_signal_slot = state->child_signal.slot();
                boost::asio::experimental::make_parallel_group(
                    timer.async_wait(),
                    std::move(on_members_chunk)
                ).async_wait(
                    boost::asio::experimental::wait_for_one(),
                    boost::asio::bind_executor(exec_strand, 
                        boost::asio::bind_cancellation_slot(
                            child_signal_slot,
                            boost::asio::bind_allocator(
                                alloc,
                                [state = std::move(state), target_exec = std::move(target_exec), alloc](
                                    auto order,
                                    boost::system::error_code timer_ec,
                                    boost::system::error_code members_chunk_ec,
                                    auto&&
                                ) mutable {
                                    {
                                    std::scoped_lock lock{state->mtx};
                                    state->completed = true;
                                    }

                                    if (order[0] == 0 || members_chunk_ec) { // timer won
                                        boost::system::error_code ec = (order[0] == 0)
                                            ? ((timer_ec == boost::asio::error::operation_aborted)
                                                ? boost::asio::error::operation_aborted
                                                : boost::asio::error::timed_out)
                                            : members_chunk_ec;
                                        boost::asio::dispatch(target_exec,
                                            boost::asio::bind_allocator(alloc, [self_ptr = std::move(state), ec]() mutable {
                                                self_ptr->complete(ec);
                                            })
                                        );
                                    } else {
                                        recieve_event::guild_members_chunk data;
                                        {
                                        std::scoped_lock lock{state->mtx};
                                        data = std::move(state->accumulated);
                                        }
                                        boost::asio::dispatch(target_exec,
                                            boost::asio::bind_allocator(alloc, [self_ptr = std::move(state), data = std::move(data)]() mutable {
                                                self_ptr->complete(boost::system::error_code{}, std::move(data));
                                            })
                                        );
                                    }
                                }
                            )
                        )
                    )
                );
            },
            token,
            this, std::move(members), std::move(timeout)
        );
    }

    template <discusy::asio::ctf<recieve_event::guild_members_chunk> CompletionToken = ctx::io_context::dct_t>
    auto request_guild_members(send_event::request_guild_members members, CompletionToken&& token = ctx::io_context::dct_t()) {
        return request_guild_members(std::move(members), std::chrono::seconds{15}, std::forward<CompletionToken>(token));
    }

    bool send_request_channel_info(send_event::request_channel_info info) {
        if (!info.guild_id) return false;

        {
        std::unique_lock lock{shards_mutex, std::chrono::milliseconds{500}};
        if (!lock.owns_lock()) return false;

        const auto total_shards = total_shards_.load();
        if (total_shards == 0 || shards_stop_requested.load() || shards_.empty() || (shards_.size() != total_shards)) {
            std::scoped_lock q_lock{pending_shard_operations_mtx_};
            pending_shard_operations_queue_.emplace_back([this, i = std::move(info)]() mutable {
                this->send_request_channel_info(std::move(i));
            });
            return true;
        }

        const auto shard_id = info.guild_id.guild_shard_id(total_shards);

        if (shard_id >= total_shards) return false;

        return shards_[shard_id]->send_request_channel_info(std::move(info));
        }
    }

    template <typename Rep, typename Period, discusy::asio::ctf<recieve_event::channel_info> CompletionToken = ctx::io_context::dct_t>
    auto request_channel_info(send_event::request_channel_info info, std::chrono::duration<Rep, Period> timeout, CompletionToken&& token = ctx::io_context::dct_t()) {
        return boost::asio::async_initiate<CompletionToken, void(boost::system::error_code, recieve_event::channel_info)>(
            [](discusy::asio::chf<recieve_event::channel_info> auto&& handler, bot* self, auto&& info, auto&& timeout) mutable {
                auto alloc = boost::asio::get_associated_allocator(
                    handler, 
                    boost::asio::recycling_allocator<void>{}
                );

                auto early_complete = [&handler, self, &alloc](boost::system::error_code ec, recieve_event::channel_info result = {}) mutable -> void {
                    auto exec = boost::asio::get_associated_immediate_executor(handler, self->io_ctx.executor_);

                    boost::asio::dispatch(exec,
                        boost::asio::bind_allocator(alloc, [h = std::move(handler), ec, res = std::move(result)]() mutable {
                            std::move(h)(ec, std::move(res));
                        })
                    );
                };

                if (!info.guild_id) return std::move(early_complete)(boost::asio::error::invalid_argument);

                using handler_t = std::decay_t<decltype(handler)>;

                struct request_state {
                    handler_t handler;
                    ctx::io_context::strand_timer_t timer;
                    boost::asio::cancellation_signal child_signal;

                    decltype(
                        boost::asio::make_work_guard(boost::asio::get_associated_executor(handler, std::declval<ctx::io_context::executor_t>()))
                    ) work_guard_;

                    void complete(boost::system::error_code ec, recieve_event::channel_info&& data = {}) {
                        auto slot = boost::asio::get_associated_cancellation_slot(handler);
                        if (slot.is_connected()) slot.clear();
                        work_guard_.reset();
                        std::move(handler)(ec, std::move(data));
                    }

                    request_state(const ctx::io_context::strand_t& strand, handler_t handler_, const ctx::io_context::executor_t& default_exec) 
                        : handler{std::move(handler_)}, timer{strand}, work_guard_{boost::asio::get_associated_executor(handler, default_exec)} {}
                };

                auto exec_strand = self->io_ctx.make_strand();
                std::shared_ptr<request_state> state = std::allocate_shared<request_state>(alloc, exec_strand, std::forward<decltype(handler)>(handler), self->io_ctx.executor_);

                auto channel_info = self->gateway_callbacks.on_channel_info.when_system([id = info.guild_id](const recieve_event::channel_info& e) -> bool {
                    return e.guild_id == id;
                }, boost::asio::bind_executor(exec_strand, boost::asio::bind_allocator(alloc, boost::asio::deferred)));

                if (!self->send_request_channel_info(std::forward<decltype(info)>(info))) {
                    auto exec = boost::asio::get_associated_immediate_executor(state->handler, self->io_ctx.executor_);

                    boost::asio::dispatch(exec,
                        boost::asio::bind_allocator(alloc, [st = std::move(state)]() mutable {
                            st->complete(boost::asio::error::not_connected);
                        })
                    );
                    return;
                }

                auto slot = boost::asio::get_associated_cancellation_slot(state->handler);

                if (slot.is_connected()) {
                    slot.assign([weak_state = std::weak_ptr<request_state>{state}, exec_strand](boost::asio::cancellation_type type) {
                        if (std::shared_ptr<request_state> st = weak_state.lock()) {
                            if (type == boost::asio::cancellation_type::none) return;

                            boost::asio::post(exec_strand, [state_ptr = std::move(st), type]() {
                                state_ptr->child_signal.emit(type);
                            });
                        }
                    });
                }

                auto target_exec = boost::asio::get_associated_executor(state->handler, self->io_ctx.executor_);

                auto& timer = state->timer;
                timer.expires_after(timeout);
                boost::asio::cancellation_slot child_signal_slot = state->child_signal.slot();
                boost::asio::experimental::make_parallel_group(
                    timer.async_wait(),
                    std::move(channel_info)
                ).async_wait(
                    boost::asio::experimental::wait_for_one(),
                    boost::asio::bind_executor(exec_strand, 
                        boost::asio::bind_cancellation_slot(
                            child_signal_slot,
                            boost::asio::bind_allocator(
                                alloc,
                                [state = std::move(state), target_exec = std::move(target_exec), alloc](
                                    auto order,
                                    boost::system::error_code timer_ec,
                                    boost::system::error_code channel_info_ec,
                                    recieve_event::channel_info&& data
                                ) mutable {
                                    if (order[0] == 0 || channel_info_ec) {
                                        boost::system::error_code ec = (order[0] == 0)
                                            ? ((timer_ec == boost::asio::error::operation_aborted)
                                                ? boost::asio::error::operation_aborted
                                                : boost::asio::error::timed_out)
                                            : channel_info_ec;
                                        boost::asio::dispatch(target_exec,
                                            boost::asio::bind_allocator(alloc, [self_ptr = std::move(state), ec]() mutable {
                                                self_ptr->complete(ec);
                                            })
                                        );
                                    } else {
                                        boost::asio::dispatch(target_exec,
                                            boost::asio::bind_allocator(alloc, [self_ptr = std::move(state), data = std::move(data)]() mutable {
                                                self_ptr->complete(boost::system::error_code{}, std::move(data));
                                            })
                                        );
                                    }
                                }
                            )
                        )
                    )
                );
            },
            token,
            this, std::move(info), std::move(timeout)
        );
    }

    template <discusy::asio::ctf<recieve_event::channel_info> CompletionToken = ctx::io_context::dct_t>
    auto request_channel_info(send_event::request_channel_info info, CompletionToken&& token = ctx::io_context::dct_t()) {
        return request_channel_info(std::move(info), std::chrono::seconds{15}, std::forward<CompletionToken>(token));
    }

    // you need to handle the callback event for this yourself, too many things to handle here
    bool request_soundboard_sounds(send_event::request_soundboard_sounds sounds) {
        if (sounds.guild_ids.empty()) return false;

        bool success = true;

        {
        std::unique_lock lock{shards_mutex, std::chrono::milliseconds{500}};
        if (!lock.owns_lock()) return false;

        const auto total_shards = total_shards_.load();
        if (total_shards == 0 || shards_stop_requested.load() || shards_.empty() || (shards_.size() != total_shards)) {
            std::scoped_lock q_lock{pending_shard_operations_mtx_};
            pending_shard_operations_queue_.emplace_back([this, s = std::move(sounds)]() mutable {
                this->request_soundboard_sounds(std::move(s));
            });
            return true;
        }

        std::vector<send_event::request_soundboard_sounds> shard_requests(total_shards);
        for (const auto& id : sounds.guild_ids) {
            const auto shard_id = id.guild_shard_id(total_shards);
            if (shard_id >= total_shards) return false;

            shard_requests[shard_id].guild_ids.emplace_back(id);
            }

        for (std::decay_t<decltype(total_shards)> i = 0; i < total_shards; ++i) {
            auto& request = shard_requests[i];
            if (request.guild_ids.empty()) continue;

            success &= shards_[i]->send_request_soundboard_sounds(std::move(request));
        }
        }
        
        return success;
    }

    bool set_presence(send_event::update_presence presence) {
        send_event::update_presence_payload payload{presence};
        std::string json;
        if (json::write_json(payload, json, json_logger)) return false;

        {
        std::unique_lock lock{shards_mutex, std::chrono::milliseconds{500}};
        if (!lock.owns_lock()) return false;

        {
        std::scoped_lock p_lock{state_.presence_mtx};
        state_.presence.emplace(presence);
        }

        const auto total_shards = total_shards_.load();
        if (total_shards == 0 || shards_stop_requested.load() || shards_.empty() || (shards_.size() != total_shards)) {
            std::scoped_lock q_lock{pending_shard_operations_mtx_};
            pending_shard_operations_queue_.emplace_back([this, p = std::move(presence)]() mutable {
                this->set_presence(std::move(p));
            });
            return true;
        }

        for (auto& shard : shards_) {
            shard->send_text_ratelimited(json);
        }
        }

        return true;
    }

    // you probably shouldnt call this yourself
    // can lead to undefined consequences if your timing is bad enough
    void stop() {
        if (shards_stop_requested.exchange(true)) return;

        timers.clear();

        if (!http_only_) {
            stop_shards();
        }
        stop_workers();
        client.stop();
    }

    ~bot() {
        stop();
        worker_threads.clear();
    }

    template <typename F>
    requires( std::invocable<F&> )
    void spawn(F&& cb) {
        io_ctx.handle_callback_coro_normal(std::forward<F>(cb));
    }

    // utility
    #if defined(WIN32) || defined(BOOST_ASIO_HAS_IO_URING)
    template <discusy::asio::ctf<std::string> CompletionToken = ctx::io_context::dct_t>
    auto read_file(std::filesystem::path path, CompletionToken&& token = ctx::io_context::dct_t()) {
        return boost::asio::async_initiate<CompletionToken, void(boost::system::error_code, std::string)>(
            [](discusy::asio::chf<std::string> auto&& handler, bot* self, auto&& path) mutable {
                using handler_t = std::decay_t<decltype(handler)>;

                auto alloc = boost::asio::get_associated_allocator(
                    handler,
                    boost::asio::recycling_allocator<void>{}
                );

                auto ex = boost::asio::get_associated_executor(handler, self->io_ctx.executor_);
                using executor_type = decltype(ex);

                struct state_t {
                    handler_t handler;
                    boost::asio::executor_work_guard<executor_type> work;
                    boost::asio::basic_stream_file<ctx::io_context::strand_t> file;
                    std::string data;
                    std::atomic<bool> completed{false};

                    boost::asio::cancellation_signal child_signal;

                    state_t(handler_t h, const executor_type& e, const ctx::io_context::strand_t& exec)
                        : handler(std::move(h)), work(boost::asio::make_work_guard(e)), file(exec) {}

                    void complete(boost::system::error_code ec, std::string&& res = {}) {
                        auto slot = boost::asio::get_associated_cancellation_slot(handler);
                        if (slot.is_connected()) slot.clear();
                        work.reset();
                        std::move(handler)(ec, std::move(res));
                    }
                };

                std::shared_ptr<state_t> state = std::allocate_shared<state_t>(
                    alloc,
                    std::forward<decltype(handler)>(handler),
                    ex,
                    self->io_ctx.make_strand()
                );

                auto slot = boost::asio::get_associated_cancellation_slot(state->handler);

                auto ex_imm = boost::asio::get_associated_immediate_executor(state->handler, self->io_ctx.executor_);

                auto early_complete = [&state, &ex_imm, &alloc](boost::system::error_code ec, std::string result = {}) mutable {
                    if (state->completed.exchange(true, std::memory_order_acq_rel)) return;
                    boost::asio::dispatch(ex_imm,
                        boost::asio::bind_allocator(alloc, [state, ec, res = std::move(result)]() mutable {
                            state->complete(ec, std::move(res));
                        })
                    );
                };

                if (path.empty()) return std::move(early_complete)(boost::asio::error::invalid_argument);

                boost::system::error_code ec;
                if constexpr (std::is_same_v<std::filesystem::path::value_type, char>) {
                    state->file.open(path.native(), boost::asio::stream_file::read_only, ec);
                } else {
                    state->file.open(path.string(), boost::asio::stream_file::read_only, ec);
                }
                if (ec) return std::move(early_complete)(ec);

                const auto size = state->file.size(ec);
                if (ec) return std::move(early_complete)(ec);
                if (size == 0) return std::move(early_complete)(boost::system::error_code{});

                try {
                    state->data.resize(static_cast<std::size_t>(size));
                } catch (const std::bad_alloc&) {
                    return std::move(early_complete)(boost::asio::error::no_memory);
                }

                if (slot.is_connected()) {
                    slot.assign([weak_state = std::weak_ptr<state_t>{state}](boost::asio::cancellation_type type) {
                        if (type == boost::asio::cancellation_type::none) return;

                        if (std::shared_ptr<state_t> state = weak_state.lock()) {
                            auto exec = state->file.get_executor();
                            boost::asio::post(exec, [state = std::move(state), type]() {
                                state->child_signal.emit(type);
                            });
                        }
                    });
                }

                boost::asio::async_read(
                    state->file,
                    boost::asio::buffer(state->data),
                    boost::asio::bind_cancellation_slot(
                        state->child_signal.slot(),
                        boost::asio::bind_allocator(alloc, [state, ex, alloc](const boost::system::error_code& read_ec, std::size_t bytes_transferred) mutable {
                            if (state->completed.exchange(true, std::memory_order_acq_rel)) return;

                            boost::asio::dispatch(ex,
                                boost::asio::bind_allocator(alloc, [state, read_ec, bytes_transferred]() mutable {
                                    if (read_ec && read_ec != boost::asio::error::eof) {
                                        state->complete(read_ec, std::string{});
                                    } else {
                                        state->data.resize(bytes_transferred);
                                        state->complete(boost::system::error_code{}, std::move(state->data));
                                    }
                                })
                            );
                        })
                    )
                );
            },
            token,
            this, std::move(path)
        );
    }
    #endif

    [[nodiscard]] std::string get_application_id() {
        std::shared_lock lock{this->state_.details_mtx};
        return this->state_.application_id;
    }

    [[nodiscard]] snowflake get_user_id() {
        std::shared_lock lock{this->state_.details_mtx};
        return this->state_.me.id;
    }

    [[nodiscard]] discusy::user::user get_me() {
        std::shared_lock lock{state_.details_mtx};
        return this->state_.me;
    }

private:
    #ifdef DISCUSY_LOGGING
    template <typename ...Args>
    void log(const std::format_string<Args...>& s, Args&&... args) {
        log::Logger{}("{}{}", "[discusy bot] ", std::format(s, std::forward<Args>(args)...));
    }
    #endif

    bool update_voice_state(send_event::update_voice_state state) {
        if (!state.guild_id) return false;

        {
        std::unique_lock lock{shards_mutex, std::chrono::milliseconds{500}};
        if (!lock.owns_lock()) return false;

        const auto total_shards = total_shards_.load();
        if (total_shards == 0 || shards_stop_requested.load() || shards_.empty() || (shards_.size() != total_shards)) {
            std::scoped_lock q_lock{pending_shard_operations_mtx_};
            pending_shard_operations_queue_.emplace_back([this, s = std::move(state)]() mutable {
                this->update_voice_state(std::move(s));
            });
            return true;
        }

        const auto shard_id = state.guild_id.guild_shard_id(total_shards);

        if (shard_id >= total_shards) return false;

        return shards_[shard_id]->send_update_voice_state(std::move(state));
        }
    }

    static config setup_cfg(std::uint32_t threads) {
        config conf;
        conf.threads = (threads < 1) ? 1 : threads;
        return conf;
    }

    config setup_cfg(config& conf, [[maybe_unused]] bool add_voice_intent) {
        if (conf.threads < 1) {
            conf.threads = 1;
            #ifdef DISCUSY_LOGGING
            log("Less than 1 thread provided, setting to 1 thread");
            #endif
        }
        #ifdef DISCUSY_VOICE
        if (add_voice_intent) conf.intents |= discusy::voice::client<bot>::required_intents;
        #endif
        return conf;
    }

    config setup_cfg(const std::filesystem::path& cfg_path, opt<send_event::update_presence>&& initial_presence = std::nullopt, bool add_voice_intent = false) {
        config conf;

        std::ifstream data_file(cfg_path);
        if (!data_file.is_open()) {
            throw std::runtime_error("Failed to open config path");
        }
        glz::basic_istream_buffer<std::ifstream> buffer(data_file);

        if (json::parse_json<json::glz_opts_non_minified_error_missing>(conf, buffer, json_logger)) {
            throw std::runtime_error("Failed to parse json from config path");
        }

        if (initial_presence) {
            conf.initial_presence = std::move(*initial_presence);
        }
        return setup_cfg(conf, add_voice_intent);
    }

    void construct() {
        state_.bot_ptr = this;
        state_.gateway_callbacks.set_bot(this);

        if (cfg_.threads < 1) {
            cfg_.threads = 1;
            #ifdef DISCUSY_LOGGING
            log("Less than 1 thread provided, setting to 1 thread");
            #endif
        }
        if (cfg_.initial_presence) {
            std::scoped_lock lock{state_.presence_mtx}; // probably not needed
            state_.presence.emplace(std::move(*cfg_.initial_presence));
        }
        if (cfg_.max_pool_size) {
            client.set_max_pool_size(*cfg_.max_pool_size);
        }
        #ifdef DISCUSY_VOICE
        if (cfg_.audio_threads) {
            ctx::audio_runtime::configure(*cfg_.audio_threads);
        }
        #endif

        // set up ssl ctx
        state_.ssl_ctx.set_default_verify_paths();
        state_.ssl_ctx.set_verify_mode(boost::asio::ssl::verify_peer);
        state_.ssl_ctx.set_options(
                        boost::asio::ssl::context::default_workarounds |
                        boost::asio::ssl::context::no_sslv2 |
                        boost::asio::ssl::context::no_sslv3);
        ssl::setup_ssl_context(state_.ssl_ctx);

        #ifndef DISCUSY_NO_CACHES
        if (!http_only_) {
            caches.attach();
        }
        #endif

        if (!http_only_ && on_user_update_id_ == 0) {
            on_user_update_id_ = state_.gateway_callbacks.on_user_update.listen_system([this](const recieve_event::user_update& u) -> bool {
                std::scoped_lock lock{state_.details_mtx};
                state_.me = u;
                return true;
            });
        }
    }

    void worker() {
        while (true) {
            try {
                io_ctx->run();
                break;
            }
            #ifdef DISCUSY_LOGGING
            catch (const std::exception& e) {
                log("Io context worker error: {}", e.what());
                if (io_ctx->stopped()) break;
                std::this_thread::sleep_for(std::chrono::milliseconds{100});
                io_ctx->restart();
            }
            #endif
            catch (...) {
                #ifdef DISCUSY_LOGGING
                log("Io context worker unknown error");
                #endif
                if (io_ctx->stopped()) break;
                std::this_thread::sleep_for(std::chrono::milliseconds{100});
                io_ctx->restart();
            }
        }
    }

    void sharding_required() {
        if (!shards_restarting_.exchange(true)) {
            stop_shards();

            state_.sharding_required.reset();
            state_.sharding_required.emplace();
            state_.sharding_required_cb.reset();

            shards_stop_requested.store(false);
            start_shards();
        }
    }

    void start_shards() {
        #ifdef DISCUSY_LOGGING
        log("Starting shards");
        #endif

        shards_stop_requested.store(false);

        io_ctx.co_launch_detached([this]() -> coro::awaitable<void> {
            auto [ec, resp] = co_await api.get_gateway_bot(token::as_tuple(token::deferred));
            
            if (ec || !resp) {
                #ifdef DISCUSY_LOGGING
                log("Failed to get bot params: {}", resp.error());
                #endif
                this->stop();
                co_return;
            }

            auto& bot = resp.value();

            state_.gateway_url = ulp::str::concat_strings(bot.url, "/", urls::GATEWAY_QUERY_PARAMS);

            #ifdef DISCUSY_LOGGING
            log("Total shards: {}", bot.shards);
            log("Remaining launches: {}", bot.session_start_limit.remaining);
            log("Launches reset after: {}ms", bot.session_start_limit.reset_after);
            log("Gateway URL: {}", state_.gateway_url);
            #endif

            if (bot.session_start_limit.remaining < bot.shards) {
                #ifdef DISCUSY_LOGGING
                log("Not enough remaining session starts. Waiting {}ms before launching.", 
                    bot.session_start_limit.reset_after);
                #endif
                
                ctx::io_context::executor_timer_t timer{io_ctx.executor_, std::chrono::milliseconds{bot.session_start_limit.reset_after}};
                co_await timer.async_wait();
                start_shards();
                co_return;
            }

            const auto max_concurrency = bot.session_start_limit.max_concurrency > 0 
                                                        ? bot.session_start_limit.max_concurrency 
                                                        : 1;
            
            #ifdef DISCUSY_LOGGING
            log("Shard start concurrency: {}", max_concurrency);
            #endif

            state_.sharding_required_cb.emplace(state_.sharding_required->get_token(), [this]() {
                #ifdef DISCUSY_LOGGING
                log("Sharding required callback");
                #endif
                io_ctx.post([this]() {
                    sharding_required();
                });
            });

            state_.fatal_error_cb.emplace(state_.fatal_error->get_token(), [this]() {
                #ifdef DISCUSY_LOGGING
                log("Fatal error callback");
                #endif
                io_ctx.post([this]() {
                    stop();
                });
            });
            const auto total_shards = bot.shards;
            {
            std::scoped_lock lock{shards_mutex};
            total_shards_.store(total_shards);
            shards_.reserve(total_shards);
            }

            state_.shards_ready_counter.emplace(total_shards, [this]() {
                io_ctx.post([this, me = state_.me]() mutable {
                    std::vector<std::move_only_function<void()>> to_execute;
                    {
                    std::scoped_lock q_lock{pending_shard_operations_mtx_};
                    to_execute = std::move(pending_shard_operations_queue_);
                    pending_shard_operations_queue_.clear();
                    }
                    for (auto& task : to_execute) {
                        std::invoke(std::move(task));
                    }
                    // this should be safe??? idk
                    #ifdef DISCUSY_VOICE
                    voice.set_id(me.id);
                    #endif
                    state_.shards_ready.fire(std::move(me));
                });
            });

            for (decltype(bot.shards) i = 0; i < total_shards; i += max_concurrency) {
                if (shards_stop_requested.load()) break;

                const bool is_last_iteration = (i + max_concurrency >= total_shards);

                {
                std::scoped_lock lock{shards_mutex};
                const auto upto = i + max_concurrency;
                for (decltype(bot.shards) j = i; (j < upto) && (j < total_shards); ++j) {
                    if (shards_stop_requested.load()) break;
                    
                    #ifdef DISCUSY_LOGGING
                    log("Starting shard: {}", j);
                    #endif
                    
                    shards_.emplace_back(std::make_unique<shard>(io_ctx, j, total_shards, cfg_, state_));

                    if (is_last_iteration && !shards_stop_requested.load()) {
                        shards_restarting_.store(false);
                        shards_stop_requested.store(false);
                    }
                }
                }
                
                if (shards_stop_requested.load()) break;
                
                if (!is_last_iteration) {
                    ctx::io_context::executor_timer_t timer{io_ctx.executor_, std::chrono::seconds{5}};
                    co_await timer.async_wait();
                }
            }
            co_return;
        });
    }

    void setup_clients(const bool wait) {
        worker_threads.clear();

        if (io_ctx->stopped()) {
            io_ctx->restart();
        }

        work_guard.emplace(boost::asio::make_work_guard(io_ctx.io_ctx_));

        const auto threads = cfg_.threads - 1;

        worker_threads.reserve(wait ? threads : cfg_.threads);

        for (std::remove_cvref_t<decltype(threads)> i = 0; i < threads; ++i) {
            worker_threads.emplace_back(&bot::worker, this);
        }
    }

    void stop_shards() {
        #ifdef DISCUSY_LOGGING
        log("Stopping shards");
        #endif
        shards_stop_requested.store(true);
        {
        std::scoped_lock lock{shards_mutex};
        total_shards_.store(0);
        for (auto& shard : shards_) {
            auto strand = shard->strand_;
            boost::asio::post(strand, [this, shard = std::move(shard)]() {
                auto payloads = shard->extract_pending_payloads();
            
                if (!payloads.empty()) {
                    std::scoped_lock q_lock{this->pending_shard_operations_mtx_};
                    for (auto& payload : payloads) {
                        this->pending_shard_operations_queue_.emplace_back(
                            [this, p = std::move(payload)]() mutable {
                                this->route_evacuated_payload(std::move(p));
                            }
                        );
                    }
                }
                // shard destructor runs on its own strand
            });
        }
        shards_.clear();
        }
    }

    void stop_workers() {
        #ifdef DISCUSY_LOGGING
        log("Stopping workers");
        #endif
        if (work_guard) {
            work_guard->reset();
            work_guard.reset();
        }

        io_ctx->stop(); // stops all the timers
    }

    void route_evacuated_payload(std::string&& payload) {
        const auto total_shards = total_shards_.load();
        if (total_shards == 0) {
            std::scoped_lock q_lock{pending_shard_operations_mtx_};
            pending_shard_operations_queue_.emplace_back([this, p = std::move(payload)]() mutable {
                this->route_evacuated_payload(std::move(p));
            });
            return;
        }

        send_event::rescue_payload obj{};
        if (json::parse_json(obj, payload, json_logger)) {
            return;
        }

        if (!obj.d.is_object()) {
            #ifdef DISCUSY_LOGGING
            log("Object in route_evacuated_payload isn't object??\n{}", payload);
            #endif
            return;
        }

        auto& d = obj.d.get_object();
        auto *const d_end = d.end();

        auto *const g_id_it = d.find("guild_id");
        const snowflake guild_id{(g_id_it != d_end && g_id_it->second.is_string()) ? ulp::str::to_number_or_default<decltype(snowflake::value)>(g_id_it->second.get_string()) : 0};

        auto *g_ids_it = d.find("guild_ids");
        std::vector<snowflake> guild_ids;
        if (g_ids_it != d_end && g_ids_it->second.is_array()) {
            for (const auto& j : g_ids_it->second.get_array()) {
                if (!j.is_string()) continue;

                const auto num = ulp::str::to_number<decltype(snowflake::value)>(j.get_string());
                if (num.value_or(0) == 0ULL) continue;

                guild_ids.emplace_back(*num);
            }

            g_ids_it->second.get_array().clear();
        }

        if ((!guild_id && guild_ids.empty()) || (guild_id && !guild_ids.empty())) {
            // how?
            #ifdef DISCUSY_LOGGING
            log("Object in route_evacuated_payload has no guild id or guild ids, or has both??\n{}", payload);
            #endif
            return;
        }

        std::scoped_lock lock{shards_mutex};
        if (shards_stop_requested.load() || shards_.empty() || (shards_.size() != total_shards)) {
            std::scoped_lock q_lock{pending_shard_operations_mtx_};
            pending_shard_operations_queue_.emplace_back([this, p = std::move(payload)]() mutable {
                this->route_evacuated_payload(std::move(p));
            });
            return;
        }

        if (guild_id) {
            const auto new_shard_id = guild_id.guild_shard_id(total_shards);
            if (new_shard_id < shards_.size()) {
                shards_[new_shard_id]->send_text_ratelimited(std::move(payload));
            }
        } else { // guild_ids
            std::vector<send_event::rescue_payload> shard_requests(total_shards);
            for (const auto& id : guild_ids) {
                const auto shard_id = id.guild_shard_id(total_shards);
                if (shard_id >= total_shards) continue;

                if (!shard_requests[shard_id].d.is_object()) {
                    shard_requests[shard_id].d = d;
                }

                // these will all exist
                shard_requests[shard_id].d.at("guild_ids").get_array().emplace_back(id.str());
            }

            std::string json_encoded;

            for (std::decay_t<decltype(total_shards)> i = 0; i < total_shards; ++i) {
                auto& request = shard_requests[i];
                if (!request.d.is_object()) continue;
                auto& g_ids = request.d.at("guild_ids");
                if (!g_ids.is_array() || g_ids.get_array().empty()) continue;

                json_encoded.clear();
                if (!json::write_json(request, json_encoded)) continue;

                if (i < shards_.size()) {
                    shards_[i]->send_text_ratelimited(std::move(json_encoded));
                }
            }
        }
    }

    config cfg_;

    #ifdef DISCUSY_LOGGING
    friend log::SelfLogger<bot>;
    log::SelfLogger<bot> json_logger{*this};
    #else
    log::Logger json_logger{};
    #endif

    std::vector<std::jthread> worker_threads;
public:
    ctx::io_context io_ctx;
private:
    std::optional<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_guard;

    state state_;

public:
    discusy::gateway_callbacks& gateway_callbacks;
    Timers& timers;
    // can be called multiple times during the lifetime of the bot
    Callback<discusy::user::user>& on_shards_ready;
    http_client& client;
    http_api api;
    #ifndef DISCUSY_NO_CACHES
    cache_manager caches;
    #endif
    #ifdef DISCUSY_VOICE
    using voice_client = discusy::voice::client<bot>;
    using voice_connection = discusy::voice::client<bot>::connection;
    voice_client voice;
    #endif
private:

    std::timed_mutex shards_mutex;
    std::atomic_bool shards_stop_requested{false};
    std::atomic_bool shards_restarting_{false};
    std::atomic<std::uint32_t> total_shards_{0};
    std::vector<std::unique_ptr<shard>> shards_;

    // always acquire shards_mutex before this
    std::mutex pending_shard_operations_mtx_;
    std::vector<std::move_only_function<void()>> pending_shard_operations_queue_;

    callback_id on_user_update_id_{0};
    bool http_only_{false};

#ifdef DISCUSY_VOICE
public:
    friend class discusy::voice::client<bot>;
#endif
};

}

#include "model_methods.hpp" // IWYU pragma: keep

#ifndef DISCUSY_NO_CACHES
#include "caches.hpp" // IWYU pragma: export
#endif

#include "end_macros.hpp" // IWYU pragma: keep
