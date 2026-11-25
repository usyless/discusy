#pragma once

#include <algorithm>
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
#include <version>

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
#include "atomic_shared_ptr.hpp"
#include "state.hpp"
#include "http_api.hpp"
#include "shard.hpp"
#include "asio_helpers.hpp"
#ifndef DISCUSY_NO_CACHES
#include "caches.hpp" // IWYU pragma: export
#endif

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

    /**
     ** Constructor for the HTTP only bot, can assign a token to use the api calls using `bot::assign_token`
     */
    explicit bot(std::uint32_t threads = 1);

    /**
     ** Assign a token for a http only bot, used to use the api
     */
    void assign_token(std::string_view token);

    explicit bot(config cfg, bool add_voice_intent = false);

    explicit bot(const std::filesystem::path& cfg_path, opt<send_event::update_presence>&& initial_presence = std::nullopt, bool add_voice_intent = false);

    void run(bool wait = true);

    template <typename Rep, typename Period, discusy::asio::ctf<> CompletionToken = ctx::io_context::dct_t>
    auto sleep(std::chrono::duration<Rep, Period> time, CompletionToken&& token = ctx::io_context::dct_t()) {
        return io_ctx.sleep(std::move(time), std::forward<CompletionToken>(token));
    }

    auto make_strand() {
        return io_ctx.make_strand();
    }

    // need to test this and following methods on shard restarts to be sure
    bool send_request_guild_members(send_event::request_guild_members members);

    template <typename Rep, typename Period, discusy::asio::ctf<recieve_event::guild_members_chunk> CompletionToken = ctx::io_context::dct_t>
    auto request_guild_members(send_event::request_guild_members members, std::chrono::duration<Rep, Period> timeout, CompletionToken&& token = ctx::io_context::dct_t()) {
        return boost::asio::async_initiate<CompletionToken, void(boost::system::error_code, recieve_event::guild_members_chunk)>(
            [](discusy::asio::chf<recieve_event::guild_members_chunk> auto&& handler, bot* self, auto&& members, auto&& timeout) mutable {
                auto alloc = boost::asio::get_associated_allocator(
                    handler, 
                    boost::asio::recycling_allocator<void>{}
                );

                auto early_complete = [&handler, self](boost::system::error_code ec, recieve_event::guild_members_chunk result = {}) mutable -> void {
                    auto exec = boost::asio::get_associated_immediate_executor(handler, self->io_ctx.executor_);

                    boost::asio::dispatch(
                        exec,
                        boost::asio::append(std::move(handler), ec, std::move(result))
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

                    static void complete(std::shared_ptr<chunk_state> self_ptr, boost::system::error_code ec, recieve_event::guild_members_chunk&& data = {}) {
                        auto h = std::move(self_ptr->handler);
                        auto slot = boost::asio::get_associated_cancellation_slot(h);
                        if (slot.is_connected()) slot.clear();
                        self_ptr->work_guard_.reset();
                        self_ptr.reset();
                        std::move(h)(ec, std::move(data));
                    }

                    chunk_state(alloc_type a, const ctx::io_context::strand_t& strand, handler_t handler_, const ctx::io_context::executor_t& default_exec) 
                        : handler{std::move(handler_)}, chunks(a), received(a), timer{strand}, work_guard_{boost::asio::get_associated_executor(handler, default_exec)} {}
                };

                auto exec_strand = self->io_ctx.make_strand();
                std::shared_ptr<chunk_state> state = std::allocate_shared<chunk_state>(alloc, alloc, exec_strand, std::forward<decltype(handler)>(handler), self->io_ctx.executor_);
                state->accumulated.nonce = std::string(nonce.data(), nonce.size());

                if (!self->send_request_guild_members(std::forward<decltype(members)>(members))) {
                    auto h = std::move(state->handler);
                    auto work = std::move(state->work_guard_);
                    auto exec = boost::asio::get_associated_immediate_executor(h, self->io_ctx.executor_);
                    { auto drop_nonce = std::move(nonce); }
                    state.reset();

                    boost::asio::dispatch(
                        exec,
                        boost::asio::append(
                            std::move(h),
                            boost::asio::error::make_error_code(boost::asio::error::not_connected),
                            recieve_event::guild_members_chunk{}
                        )
                    );
                    return;
                }

                auto on_members_chunk = self->gateway_callbacks.on_guild_members_chunk.when([state, nonce = std::move(nonce), alloc](const recieve_event::guild_members_chunk& e) -> bool {
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

                auto slot = boost::asio::get_associated_cancellation_slot(state->handler);

                if (slot.is_connected()) {
                    slot.assign([weak_state = std::weak_ptr<chunk_state>{state}, exec_strand](boost::asio::cancellation_type type) {
                        if (std::shared_ptr<chunk_state> st = weak_state.lock()) {
                            if (type == boost::asio::cancellation_type::none) return;

                            boost::asio::post(exec_strand, [state_ptr = std::move(st), type] {
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
                                            boost::asio::bind_allocator(alloc, [self_ptr = std::move(state), ec] mutable {
                                                chunk_state::complete(std::move(self_ptr), ec);
                                            })
                                        );
                                    } else {
                                        recieve_event::guild_members_chunk data;
                                        {
                                        std::scoped_lock lock{state->mtx};
                                        data = std::move(state->accumulated);
                                        }
                                        boost::asio::dispatch(target_exec,
                                            boost::asio::bind_allocator(alloc, [self_ptr = std::move(state), data = std::move(data)] mutable {
                                                chunk_state::complete(std::move(self_ptr), boost::system::error_code{}, std::move(data));
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

    bool send_request_channel_info(send_event::request_channel_info info);

    template <typename Rep, typename Period, discusy::asio::ctf<recieve_event::channel_info> CompletionToken = ctx::io_context::dct_t>
    auto request_channel_info(send_event::request_channel_info info, std::chrono::duration<Rep, Period> timeout, CompletionToken&& token = ctx::io_context::dct_t()) {
        return boost::asio::async_initiate<CompletionToken, void(boost::system::error_code, recieve_event::channel_info)>(
            [](discusy::asio::chf<recieve_event::channel_info> auto&& handler, bot* self, auto&& info, auto&& timeout) mutable {
                auto alloc = boost::asio::get_associated_allocator(
                    handler, 
                    boost::asio::recycling_allocator<void>{}
                );

                auto early_complete = [&handler, self](boost::system::error_code ec, recieve_event::channel_info result = {}) mutable -> void {
                    auto exec = boost::asio::get_associated_immediate_executor(handler, self->io_ctx.executor_);

                    boost::asio::dispatch(
                        exec,
                        boost::asio::append(std::move(handler), ec, std::move(result))
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

                    static void complete(std::shared_ptr<request_state> self_ptr, boost::system::error_code ec, recieve_event::channel_info&& data = {}) {
                        auto h = std::move(self_ptr->handler);
                        auto slot = boost::asio::get_associated_cancellation_slot(h);
                        if (slot.is_connected()) slot.clear();
                        self_ptr->work_guard_.reset();
                        self_ptr.reset();
                        std::move(h)(ec, std::move(data));
                    }

                    request_state(const ctx::io_context::strand_t& strand, handler_t handler_, const ctx::io_context::executor_t& default_exec) 
                        : handler{std::move(handler_)}, timer{strand}, work_guard_{boost::asio::get_associated_executor(handler, default_exec)} {}
                };

                auto exec_strand = self->io_ctx.make_strand();
                std::shared_ptr<request_state> state = std::allocate_shared<request_state>(alloc, exec_strand, std::forward<decltype(handler)>(handler), self->io_ctx.executor_);

                const auto guild_id = info.guild_id;
                if (!self->send_request_channel_info(std::forward<decltype(info)>(info))) {
                    auto h = std::move(state->handler);
                    auto work = std::move(state->work_guard_);
                    auto exec = boost::asio::get_associated_immediate_executor(h, self->io_ctx.executor_);
                    state.reset();

                    boost::asio::dispatch(
                        exec,
                        boost::asio::append(
                            std::move(h),
                            boost::asio::error::make_error_code(boost::asio::error::not_connected),
                            recieve_event::channel_info{}
                        )
                    );
                    return;
                }

                auto channel_info = self->gateway_callbacks.on_channel_info.when([id = guild_id](const recieve_event::channel_info& e) -> bool {
                    return e.guild_id == id;
                }, boost::asio::bind_executor(exec_strand, boost::asio::bind_allocator(alloc, boost::asio::deferred)));

                auto slot = boost::asio::get_associated_cancellation_slot(state->handler);

                if (slot.is_connected()) {
                    slot.assign([weak_state = std::weak_ptr<request_state>{state}, exec_strand](boost::asio::cancellation_type type) {
                        if (std::shared_ptr<request_state> st = weak_state.lock()) {
                            if (type == boost::asio::cancellation_type::none) return;

                            boost::asio::post(exec_strand, [state_ptr = std::move(st), type] {
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
                                            boost::asio::bind_allocator(alloc, [self_ptr = std::move(state), ec] mutable {
                                                request_state::complete(std::move(self_ptr), ec);
                                            })
                                        );
                                    } else {
                                        boost::asio::dispatch(target_exec,
                                            boost::asio::bind_allocator(alloc, [self_ptr = std::move(state), data = std::move(data)] mutable {
                                                request_state::complete(std::move(self_ptr), boost::system::error_code{}, std::move(data));
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
    bool request_soundboard_sounds(send_event::request_soundboard_sounds sounds);

    bool set_presence(send_event::update_presence presence);

    // you probably shouldnt call this yourself
    // can lead to undefined consequences if your timing is bad enough
    void stop();

    ~bot();

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

                const auto alloc = boost::asio::get_associated_allocator(
                    handler,
                    boost::asio::recycling_allocator<void>{}
                );

                const auto ex = boost::asio::get_associated_executor(handler, self->io_ctx.executor_);
                using executor_type = std::decay_t<decltype(ex)>;

                struct state_t {
                    handler_t handler;
                    boost::asio::executor_work_guard<executor_type> work;
                    boost::asio::basic_stream_file<ctx::io_context::strand_t> file;
                    std::string data;

                    boost::asio::cancellation_signal child_signal;

                    state_t(handler_t h, const executor_type& e, const ctx::io_context::strand_t& exec)
                        : handler(std::move(h)), work(boost::asio::make_work_guard(e)), file(exec) {}
                };

                std::shared_ptr<state_t> state = std::allocate_shared<state_t>(
                    alloc,
                    std::forward<decltype(handler)>(handler),
                    ex,
                    self->io_ctx.make_strand()
                );

                auto slot = boost::asio::get_associated_cancellation_slot(state->handler);

                auto ex_imm = boost::asio::get_associated_immediate_executor(state->handler, self->io_ctx.executor_);

                const auto early_complete = [&state, &ex_imm](boost::system::error_code ec, std::string result = {}) {
                    auto h = std::move(state->handler);
                    const auto work = std::move(state->work); // keep work alive until dispatch submits
                    state.reset();                      // closes file & frees state_t before handler runs

                    boost::asio::dispatch(
                        ex_imm,
                        boost::asio::append(std::move(h), ec, std::move(result))
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
                            const auto exec = state->file.get_executor();
                            boost::asio::post(exec, [state = std::move(state), type] {
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
                            boost::system::error_code ignored;
                            state->file.close(ignored); // close on file's strand before handing off

                            boost::asio::dispatch(ex,
                                boost::asio::bind_allocator(alloc, [state = std::move(state), read_ec, bytes_transferred] mutable {
                                    auto h = std::move(state->handler);
                                    auto slot = boost::asio::get_associated_cancellation_slot(h);
                                    if (slot.is_connected()) slot.clear();

                                    boost::system::error_code ec;
                                    std::string result;
                                    if (read_ec && read_ec != boost::asio::error::eof) {
                                        ec = read_ec;
                                    } else {
                                        state->data.resize(bytes_transferred);
                                        result = std::move(state->data);
                                    }

                                    state->work.reset();
                                    state.reset(); // deallocate via alloc BEFORE calling h

                                    std::move(h)(ec, std::move(result));
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

    [[nodiscard]] std::string get_application_id();
    [[nodiscard]] snowflake get_user_id();
    [[nodiscard]] discusy::user::user get_me();

private:
    #ifdef DISCUSY_LOGGING
    template <typename ...Args>
    void log(const std::format_string<Args...>& s, Args&&... args) {
        log::Logger{}("{}{}", "[discusy bot] ", std::format(s, std::forward<Args>(args)...));
    }
    #endif

    bool update_voice_state(send_event::update_voice_state state);
    static config setup_cfg(std::uint32_t threads);
    static config setup_cfg(config& conf, bool add_voice_intent);
    config setup_cfg(const std::filesystem::path& cfg_path, opt<send_event::update_presence>&& initial_presence = std::nullopt, bool add_voice_intent = false);
    void construct();
    void worker();
    void sharding_required();
    void start_shards();
    void setup_clients(bool wait);
    void stop_shards();
    void stop_workers();
    void route_evacuated_payload(std::string&& payload);

    config cfg_;

    #ifdef DISCUSY_LOGGING
    friend log::SelfLogger<bot>;
    log::SelfLogger<bot> json_logger{*this};
    #else
    log::Logger json_logger{};
    #endif
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
    std::vector<std::shared_ptr<shard>> shards_;

    // always acquire shards_mutex before this
    std::mutex pending_shard_operations_mtx_;
    std::vector<
    #if defined(__cpp_lib_move_only_function) && __cpp_lib_move_only_function >= 202110L
        std::move_only_function<void()>
    #else
        std::function<void()>
    #endif
    > pending_shard_operations_queue_;

    callback_id on_user_update_id_{0};
    bool http_only_{false};

    std::vector<std::jthread> worker_threads;

#ifdef DISCUSY_VOICE
public:
    friend class discusy::voice::client<bot>;
#endif
};

}

#include "model_methods.hpp" // IWYU pragma: keep

#include "end_macros.hpp" // IWYU pragma: keep
