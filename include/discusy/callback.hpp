#pragma once

#include <algorithm>
#include <functional>
#include <utility>
#include <concepts>
#include <mutex>
#include <atomic>
#include <memory>

#include <boost/container/small_vector.hpp>

#include "coro.hpp"
#include "io_context.hpp"
#include "types.hpp"
#include "log.hpp" // IWYU pragma: keep
#include "json.hpp"
#include "asio_helpers.hpp"

namespace discusy {

enum class callback_priority : std::uint8_t {
    user = 0,
    system = 1,
};

class bot;

template <typename Obj>
inline void attach_bot(Obj& obj, bot* b) noexcept {
    if constexpr (requires { obj.set_bot_void(b); }) {
        obj.set_bot_void(b);
    }
}

// should callback unregister and register be dispatch or post?
// if im currently running callbacks and i unregister one should i unregister it immediately
// or unregister it after theyve all run?
//
// there will also be some mutex contention here but just dont register or unregister too much and its fine
template <typename T>
// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
class Callback {
    // NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
    struct callback_interface {
        std::atomic<bool> unregistered{false};

        virtual ~callback_interface() noexcept = default;
        [[nodiscard]] virtual bool is_async() const noexcept = 0;
        [[nodiscard]] virtual const boost::asio::any_io_executor& get_executor() const noexcept = 0;
        virtual bool invoke_sync(const T&) { return true; }
        virtual coro::awaitable<bool> invoke_async(const T&) { co_return true; }
        virtual void on_callback_destroyed() {}
    };

    template <typename F>
    struct sync_callback : callback_interface {
        F func;
        boost::asio::any_io_executor ex;

        template <typename FF>
        explicit sync_callback(FF&& f, boost::asio::any_io_executor executor) noexcept(std::is_nothrow_constructible_v<F, FF&&>)
            : func(std::forward<FF>(f)), ex(std::move(executor)) {}
        
        [[nodiscard]] bool is_async() const noexcept override final { return false; }
        [[nodiscard]] const boost::asio::any_io_executor& get_executor() const noexcept override final { return ex; }
        bool invoke_sync(const T& data) override final {
            using Result = std::invoke_result_t<F&, const T&>;
            if constexpr (std::is_same_v<Result, bool>) {
                return std::invoke(func, data);
            } else {
                std::invoke(func, data);
                return true;
            }
        }
    };

    template <typename F>
    struct async_callback : callback_interface {
        F func;
        boost::asio::any_io_executor ex;

        template <typename FF>
        explicit async_callback(FF&& f, boost::asio::any_io_executor executor) noexcept(std::is_nothrow_constructible_v<F, FF&&>)
            : func(std::forward<FF>(f)), ex(std::move(executor)) {}
        
        [[nodiscard]] bool is_async() const noexcept override final { return true; }
        [[nodiscard]] const boost::asio::any_io_executor& get_executor() const noexcept override final { return ex; }
        coro::awaitable<bool> invoke_async(const T& data) override final {
            using Result = std::invoke_result_t<F&, const T&>;
            if constexpr (std::is_same_v<Result, coro::awaitable<bool>>) {
                return std::invoke(func, data);
            } else {
                // reference is fine here for coro as itll live throughout and is in a shared ptr
                return [](auto& cb, const T& arg) -> coro::awaitable<bool> {
                    using Awaiter = coro::get_awaiter_t<Result>;
                    using ResumeResult = decltype(std::declval<Awaiter&>().await_resume());
                    if constexpr (std::is_same_v<ResumeResult, bool>) {
                        co_return co_await std::invoke(cb, arg);
                    } else {
                        co_await std::invoke(cb, arg);
                        co_return true;
                    }
                }(func, data);
            }
        }
    };

    struct entry {
        callback_id id{0};
        std::shared_ptr<callback_interface> func{};
        callback_priority prio{callback_priority::user};
    };

    struct Snapshot {
        boost::container::small_vector<std::shared_ptr<callback_interface>, 4> callbacks;
        std::size_t system_count{0};
        std::size_t async_count{0};
        std::size_t active_count{0};
    };

    struct Core {
        boost::asio::any_io_executor exec;
        boost::container::small_vector<entry, 4> callbacks;
        std::atomic<callback_id> cb_idx{0};
        size_t stale_count{0};
        std::atomic<std::shared_ptr<const Snapshot>> snapshot{nullptr};
        std::atomic<std::size_t> active_count{0};
        std::size_t async_count{0};
        std::mutex mtx;

        explicit Core(boost::asio::any_io_executor ex) : exec{std::move(ex)} {}

        void rebuild_snapshot() {
            if (stale_count > 0) {
                boost::container::small_vector<entry, 4> compacted;
                compacted.reserve(callbacks.size() - stale_count);
                for (auto& e : callbacks) {
                    if (e.func && !e.func->unregistered.load(std::memory_order_relaxed)) {
                        compacted.emplace_back(std::move(e));
                    }
                }
                callbacks = std::move(compacted);
                stale_count = 0;
            }

            auto snap = std::make_shared<Snapshot>();
            snap->callbacks.reserve(callbacks.size());
            std::size_t active = 0;
            std::size_t async_c = 0;
            std::size_t sys_c = 0;

            for (const auto& e : callbacks) {
                if (e.prio == callback_priority::system && e.func && !e.func->unregistered.load(std::memory_order_relaxed)) {
                    snap->callbacks.emplace_back(e.func);
                    ++active;
                    ++sys_c;
                    if (e.func->is_async() || (e.func->get_executor() != exec)) ++async_c;
                }
            }

            for (const auto& e : callbacks) {
                if (e.prio == callback_priority::user && e.func && !e.func->unregistered.load(std::memory_order_relaxed)) {
                    snap->callbacks.emplace_back(e.func);
                    ++active;
                    if (e.func->is_async() || (e.func->get_executor() != exec)) ++async_c;
                }
            }

            snap->system_count = sys_c;
            snap->async_count = async_c;
            snap->active_count = active;
            async_count = async_c;
            active_count.store(active, std::memory_order_release);
            snapshot.store(std::move(snap), std::memory_order_release);
        }

        bool unregister(const callback_id id) {
            if (id == 0) return false;
            
            std::shared_ptr<callback_interface> to_destroy;
            {
                std::scoped_lock lock{mtx};
                auto it = std::ranges::find_if(callbacks, [id](const auto& e) {
                    return e.id == id;
                });
                if (it == callbacks.end() || !it->func) return false;
                
                if (it->func->is_async() || (it->func->get_executor() != exec)) {
                    if (async_count > 0) --async_count;
                }

                it->func->unregistered.store(true, std::memory_order_release);
                to_destroy = std::move(it->func);
                ++stale_count;
                active_count.fetch_sub(1, std::memory_order_release);
                
                if ((stale_count * 2 >= callbacks.size() && callbacks.size() > 8) || active_count.load(std::memory_order_relaxed) == 0) {
                    rebuild_snapshot();
                }
            }
            if (to_destroy) {
                to_destroy->on_callback_destroyed();
            }
            return true;
        }

        [[nodiscard]] std::shared_ptr<const Snapshot> get_snapshot() const noexcept {
            return snapshot.load(std::memory_order_acquire);
        }
    };

    std::shared_ptr<Core> core_;
    ctx::io_context& io_ctx_;
    bot* bot_ptr_{nullptr};

    static bool invoke_entry_sync(callback_interface& entry, const T& data) {
        return entry.invoke_sync(data);
    }

    static coro::awaitable<bool> invoke_entry_async(callback_interface& entry, const T& data, const boost::asio::any_io_executor& current_ex) {
        const auto& target_ex = entry.get_executor();
        if (entry.is_async()) {
            if (target_ex == current_ex) {
                co_return co_await entry.invoke_async(data);
            } else {
                const auto [eptr, cont] = co_await boost::asio::co_spawn(
                    target_ex,
                    [&entry, &data]() -> coro::awaitable<bool> {
                        return entry.invoke_async(data);
                    },
                    boost::asio::as_tuple(boost::asio::deferred)
                );
                if (eptr) std::rethrow_exception(eptr);
                co_return cont;
            }
        } else {
            if (target_ex == current_ex) {
                co_return entry.invoke_sync(data);
            } else {
                const auto [eptr, cont] = co_await boost::asio::co_spawn(
                    target_ex,
                    [&entry, &data]() -> coro::awaitable<bool> {
                        co_return entry.invoke_sync(data);
                    },
                    boost::asio::as_tuple(boost::asio::deferred)
                );
                if (eptr) std::rethrow_exception(eptr);
                co_return cont;
            }
        }
    }

    static void fire_sync_internal(std::shared_ptr<const Snapshot> snap, const T& data) {
        if (!snap) return;

        for (std::size_t i = 0; i < snap->system_count; ++i) {
            const auto& entry_ptr = snap->callbacks[i];
            if (entry_ptr && !entry_ptr->unregistered.load(std::memory_order_acquire)) {
                try {
                    invoke_entry_sync(*entry_ptr, data);
                }
                #ifdef DISCUSY_LOGGING
                catch (const std::exception& e) {
                    log::Logger{}("System callback handler exception: {}", e.what());
                }
                #endif
                catch (...) {
                    #ifdef DISCUSY_LOGGING
                    log::Logger{}("System callback handler unknown exception");
                    #endif
                }
            }
        }

        for (std::size_t i = snap->system_count; i < snap->callbacks.size(); ++i) {
            const auto& entry_ptr = snap->callbacks[i];
            if (entry_ptr && !entry_ptr->unregistered.load(std::memory_order_acquire)) {
                try {
                    if (!invoke_entry_sync(*entry_ptr, data)) break;
                }
                #ifdef DISCUSY_LOGGING
                catch (const std::exception& e) {
                    log::Logger{}("User callback handler exception: {}", e.what());
                }
                #endif
                catch (...) {
                    #ifdef DISCUSY_LOGGING
                    log::Logger{}("User callback handler unknown exception");
                    #endif
                }
            }
        }
    }

    static coro::awaitable<void> fire_async_internal(std::shared_ptr<const Snapshot> snap, T data) {
        if (!snap) co_return;
        const auto current_ex = co_await boost::asio::this_coro::executor;

        for (std::size_t i = 0; i < snap->system_count; ++i) {
            const auto& entry_ptr = snap->callbacks[i];
            if (entry_ptr && !entry_ptr->unregistered.load(std::memory_order_acquire)) {
                try {
                    co_await invoke_entry_async(*entry_ptr, data, current_ex);
                }
                #ifdef DISCUSY_LOGGING
                catch (const std::exception& e) {
                    log::Logger{}("System callback handler exception: {}", e.what());
                }
                #endif
                catch (...) {
                    #ifdef DISCUSY_LOGGING
                    log::Logger{}("System callback handler unknown exception");
                    #endif
                }
            }
        }

        for (std::size_t i = snap->system_count; i < snap->callbacks.size(); ++i) {
            const auto& entry_ptr = snap->callbacks[i];
            if (entry_ptr && !entry_ptr->unregistered.load(std::memory_order_acquire)) {
                try {
                    if (!(co_await invoke_entry_async(*entry_ptr, data, current_ex))) break;
                }
                #ifdef DISCUSY_LOGGING
                catch (const std::exception& e) {
                    log::Logger{}("User callback handler exception: {}", e.what());
                }
                #endif
                catch (...) {
                    #ifdef DISCUSY_LOGGING
                    log::Logger{}("User callback handler unknown exception");
                    #endif
                }
            }
        }

        co_return;
    }

public:
    explicit Callback(ctx::io_context& ctx) : core_{std::make_shared<Core>(ctx.executor_)}, io_ctx_{ctx} {}

    void set_bot(bot* b) noexcept {
        bot_ptr_ = b;
    }

    ~Callback() {
        if (!core_) return;

        boost::container::small_vector<std::shared_ptr<callback_interface>, 4> pending;
        {
        std::scoped_lock lock{core_->mtx};
        if (!core_->callbacks.empty()) {
            pending.reserve(core_->callbacks.size() - core_->stale_count);
            for (auto& entry : core_->callbacks) {
                if (entry.func) {
                    entry.func->unregistered.store(true, std::memory_order_release);
                    pending.emplace_back(std::move(entry.func));
                }
            }
            core_->callbacks.clear();
            core_->stale_count = 0;
            core_->rebuild_snapshot();
        }
        }

        for (auto& func : pending) {
            func->on_callback_destroyed();
        }
    }

    [[nodiscard]] callback_id reserve_id() noexcept {
        return ++core_->cb_idx;
    }

    template <typename F>
    requires ( std::invocable<F&, const T&> )
    callback_id listen_with_id(const callback_id id, callback_priority prio, F&& cb, boost::asio::any_io_executor ex) {
        if (id == 0 || !core_) return 0;

        if constexpr (requires { !cb; }) {
            if (!cb) return 0;
        }

        std::shared_ptr<callback_interface> ptr;

        using Result = std::invoke_result_t<F&, const T&>;
        if constexpr (!coro::IsAwaitable<Result>) {
            ptr = std::make_shared<sync_callback<std::decay_t<F>>>(std::forward<F>(cb), std::move(ex));
        } else {
            ptr = std::make_shared<async_callback<std::decay_t<F>>>(std::forward<F>(cb), std::move(ex));
        }

        {
        std::scoped_lock lock{core_->mtx};
        core_->callbacks.emplace_back(id, std::move(ptr), prio);
        core_->rebuild_snapshot();
        }
        return id;
    }

    template <typename F>
    requires ( std::invocable<F&, const T&> )
    callback_id listen_with_id(const callback_id id, F&& cb, boost::asio::any_io_executor ex) {
        return listen_with_id(id, callback_priority::user, std::forward<F>(cb), std::move(ex));
    }

    template <typename F, typename Executor>
    requires ( std::invocable<F&, const T&> && std::is_constructible_v<boost::asio::any_io_executor, std::decay_t<Executor>> && !std::is_same_v<std::decay_t<Executor>, boost::asio::any_io_executor> )
    callback_id listen_with_id(const callback_id id, callback_priority prio, F&& cb, Executor&& ex) {
        return listen_with_id(id, prio, std::forward<F>(cb), boost::asio::any_io_executor(std::forward<Executor>(ex)));
    }

    template <typename F, typename Executor>
    requires ( std::invocable<F&, const T&> && std::is_constructible_v<boost::asio::any_io_executor, std::decay_t<Executor>> && !std::is_same_v<std::decay_t<Executor>, boost::asio::any_io_executor> )
    callback_id listen_with_id(const callback_id id, F&& cb, Executor&& ex) {
        return listen_with_id(id, callback_priority::user, std::forward<F>(cb), std::forward<Executor>(ex));
    }

    template <typename Executor, typename F>
    requires ( std::invocable<F&, const T&> && std::is_constructible_v<boost::asio::any_io_executor, std::decay_t<Executor>> )
    callback_id listen_with_id(const callback_id id, callback_priority prio, Executor&& ex, F&& cb) {
        return listen_with_id(id, prio, std::forward<F>(cb), boost::asio::any_io_executor(std::forward<Executor>(ex)));
    }

    template <typename Executor, typename F>
    requires ( std::invocable<F&, const T&> && std::is_constructible_v<boost::asio::any_io_executor, std::decay_t<Executor>> )
    callback_id listen_with_id(const callback_id id, Executor&& ex, F&& cb) {
        return listen_with_id(id, callback_priority::user, std::forward<Executor>(ex), std::forward<F>(cb));
    }

    template <typename F>
    requires ( std::invocable<F&, const T&> )
    callback_id listen_with_id(const callback_id id, callback_priority prio, F&& cb) {
        auto ex = boost::asio::get_associated_executor(cb, io_ctx_.executor_);
        return listen_with_id(id, prio, std::forward<F>(cb), std::move(ex));
    }

    template <typename F>
    requires ( std::invocable<F&, const T&> )
    callback_id listen_with_id(const callback_id id, F&& cb) {
        return listen_with_id(id, callback_priority::user, std::forward<F>(cb));
    }

    template <typename Executor, typename F>
    requires ( std::invocable<F&, const T&> && std::is_constructible_v<boost::asio::any_io_executor, std::decay_t<Executor>> && !std::invocable<Executor&, const T&> )
    callback_id listen(callback_priority prio, Executor&& ex, F&& cb) {
        return listen_with_id(reserve_id(), prio, std::forward<Executor>(ex), std::forward<F>(cb));
    }

    template <typename Executor, typename F>
    requires ( std::invocable<F&, const T&> && std::is_constructible_v<boost::asio::any_io_executor, std::decay_t<Executor>> && !std::invocable<Executor&, const T&> )
    callback_id listen(Executor&& ex, F&& cb) {
        return listen_with_id(reserve_id(), callback_priority::user, std::forward<Executor>(ex), std::forward<F>(cb));
    }

    template <typename F, typename Executor>
    requires ( std::invocable<F&, const T&> && std::is_constructible_v<boost::asio::any_io_executor, std::decay_t<Executor>> && !std::invocable<Executor&, const T&> )
    callback_id listen(callback_priority prio, F&& cb, Executor&& ex) {
        return listen_with_id(reserve_id(), prio, std::forward<Executor>(ex), std::forward<F>(cb));
    }

    template <typename F, typename Executor>
    requires ( std::invocable<F&, const T&> && std::is_constructible_v<boost::asio::any_io_executor, std::decay_t<Executor>> && !std::invocable<Executor&, const T&> )
    callback_id listen(F&& cb, Executor&& ex) {
        return listen_with_id(reserve_id(), callback_priority::user, std::forward<Executor>(ex), std::forward<F>(cb));
    }

    template <typename F>
    requires ( std::invocable<F&, const T&> )
    callback_id listen(callback_priority prio, F&& cb) {
        return listen_with_id(reserve_id(), prio, std::forward<F>(cb));
    }

    template <typename F>
    requires ( std::invocable<F&, const T&> )
    callback_id listen(F&& cb) {
        return listen_with_id(reserve_id(), callback_priority::user, std::forward<F>(cb));
    }

    template <typename Executor, typename F>
    requires ( std::invocable<F&, const T&> && std::is_constructible_v<boost::asio::any_io_executor, std::decay_t<Executor>> && !std::invocable<Executor&, const T&> )
    callback_id listen_system(Executor&& ex, F&& cb) {
        return listen(callback_priority::system, std::forward<Executor>(ex), std::forward<F>(cb));
    }

    template <typename F, typename Executor>
    requires ( std::invocable<F&, const T&> && std::is_constructible_v<boost::asio::any_io_executor, std::decay_t<Executor>> && !std::invocable<Executor&, const T&> )
    callback_id listen_system(F&& cb, Executor&& ex) {
        return listen(callback_priority::system, std::forward<F>(cb), std::forward<Executor>(ex));
    }

    template <typename F>
    requires ( std::invocable<F&, const T&> )
    callback_id listen_system(F&& cb) {
        return listen(callback_priority::system, std::forward<F>(cb));
    }

    template <typename Executor, typename F>
    requires ( std::invocable<F&, const T&> && std::is_constructible_v<boost::asio::any_io_executor, std::decay_t<Executor>> && !std::invocable<Executor&, const T&> )
    callback_id operator()(Executor&& ex, F&& cb) {
        return listen(std::forward<Executor>(ex), std::forward<F>(cb));
    }

    template <typename F, typename Executor>
    requires ( std::invocable<F&, const T&> && std::is_constructible_v<boost::asio::any_io_executor, std::decay_t<Executor>> && !std::invocable<Executor&, const T&> )
    callback_id operator()(F&& cb, Executor&& ex) {
        return listen(std::forward<F>(cb), std::forward<Executor>(ex));
    }

    template <typename F>
    requires ( std::invocable<F&, const T&> )
    callback_id operator()(F&& cb) {
        return listen(std::forward<F>(cb));
    }

    bool unregister(const callback_id id) {
        if (!core_) return false;
        return core_->unregister(id);
    }

    void fire(T&& data) {
        if (!core_ || core_->active_count.load(std::memory_order_acquire) == 0) return;
        auto snap = core_->get_snapshot();
        if (!snap || (snap->active_count == 0) || snap->callbacks.empty()) return;

        attach_bot(data, bot_ptr_);

        try {
            if (snap->async_count == 0) {
                io_ctx_.submit([snap = std::move(snap), data = std::move(data)]() mutable {
                    fire_sync_internal(std::move(snap), data);
                });
            } else {
                io_ctx_.co_launch_detached([snap = std::move(snap), data = std::move(data)]() mutable {
                    return fire_async_internal(std::move(snap), std::move(data));
                });
            }
        } 
        #ifdef DISCUSY_LOGGING
        catch (const std::exception& e) {
            log::Logger{}("Callback fire exception: {}", e.what());
        }
        #endif
        catch (...) {
            #ifdef DISCUSY_LOGGING
            log::Logger{}("Callback fire unknown exception");
            #endif
        }
    }

    template <typename W = T>
    void fire_json(std::string&& json) {
        if (!core_ || core_->active_count.load(std::memory_order_acquire) == 0) return;
        auto snap = core_->get_snapshot();
        if (!snap || (snap->active_count == 0) || snap->callbacks.empty()) return;

        try {
            if (snap->async_count == 0) {
                io_ctx_.submit([bot_ptr_ = bot_ptr_, snap = std::move(snap), json = std::move(json)]() mutable {
                    W p{};
                    if (json::parse_json(p, json)) return;
                    attach_bot(p.d, bot_ptr_);
                    fire_sync_internal(std::move(snap), p.d);
                });
            } else {
                io_ctx_.co_launch_detached([bot_ptr_ = bot_ptr_, snap = std::move(snap), json = std::move(json)]() mutable -> coro::awaitable<void> {
                    W p{};
                    if (json::parse_json(p, json)) co_return;
                    attach_bot(p.d, bot_ptr_);
                    co_await fire_async_internal(std::move(snap), std::move(p.d));
                    co_return;
                });
            }
        }
        catch (...) {
            #ifdef DISCUSY_LOGGING
            log::Logger{}("Callback fire_json unknown exception");
            #endif
        }
    }

    // makes a copy of the data, unideal
    // if you want to use it with awaitable operators, pass in boost::asio::use_awaitable
    template <typename Predicate, discusy::asio::ctf<T> CompletionToken = ctx::io_context::dct_t>
    requires ( std::invocable<Predicate&, const T&> && std::is_same_v<std::invoke_result_t<Predicate&, const T&>, bool> )
    auto when_impl(callback_priority prio, Predicate&& pred, CompletionToken&& token = ctx::io_context::dct_t()) {
        return boost::asio::async_initiate<CompletionToken, void(boost::system::error_code, T)>(
            [prio](discusy::asio::chf<T> auto&& handler, std::weak_ptr<Core> weak_core, ctx::io_context::executor_t executor, auto&& p) mutable {
                using handler_t = std::decay_t<decltype(handler)>;

                auto alloc = boost::asio::get_associated_allocator(
                    handler, 
                    boost::asio::recycling_allocator<void>{}
                );

                auto ex = boost::asio::get_associated_executor(handler, executor);
                using executor_type = decltype(ex);

                struct State {
                    handler_t handler;
                    boost::asio::executor_work_guard<executor_type> work;
                    callback_id id{0};

                    std::atomic<bool> completed{false};
                    // 0 = registration pending, 1 = registered, 2 = cancelled/torn down
                    std::atomic<int> reg_state{0};

                    State(handler_t h, const executor_type& e) : handler(std::move(h)), work(boost::asio::make_work_guard(e)) {}

                    void complete(boost::system::error_code ec, T&& data = T{}) {
                        auto slot = boost::asio::get_associated_cancellation_slot(handler);
                        if (slot.is_connected()) slot.clear();
                        work.reset();
                        std::move(handler)(ec, std::move(data));
                    }
                };

                std::shared_ptr<State> state = std::allocate_shared<State>(
                    alloc, 
                    std::forward<decltype(handler)>(handler),
                    ex
                );

                auto slot = boost::asio::get_associated_cancellation_slot(state->handler);

                auto core = weak_core.lock();
                if (!core) {
                    auto ex_imm = boost::asio::get_associated_immediate_executor(state->handler, executor);
                    boost::asio::dispatch(ex_imm,
                        boost::asio::bind_allocator(alloc, [state]() mutable {
                            state->complete(boost::asio::error::make_error_code(boost::asio::error::operation_aborted));
                        })
                    );
                    return;
                }

                const auto id = ++core->cb_idx;
                state->id = id;

                if (slot.is_connected()) {
                    slot.assign([weak_core, weak_state = std::weak_ptr<State>(state), ex, alloc, executor](boost::asio::cancellation_type type) {
                        if (type == boost::asio::cancellation_type::none) return;
                        std::shared_ptr<State> state = weak_state.lock();
                        if (!state) return;
                        
                        if (state->completed.exchange(true, std::memory_order_acq_rel)) return;

                        if (auto core = weak_core.lock()) {
                            if (state->reg_state.exchange(2, std::memory_order_acq_rel) == 1) core->unregister(state->id);
                        }

                        boost::asio::post(executor, 
                            boost::asio::bind_allocator(alloc, [state = std::move(state), alloc, ex]() mutable {
                                boost::asio::dispatch(ex, 
                                    boost::asio::bind_allocator(alloc, [state = std::move(state)]() mutable {
                                        state->complete(boost::asio::error::make_error_code(boost::asio::error::operation_aborted));
                                    })
                                );
                            })
                        );
                    });
                }

                using executor_type = decltype(ex);
                using allocator_type = decltype(alloc);
                using pred_t = std::decay_t<decltype(p)>;

                struct when_callback : callback_interface {
                    std::weak_ptr<Core> weak_core;
                    std::shared_ptr<State> state;
                    executor_type ex;
                    allocator_type alloc;
                    pred_t p;
                    boost::asio::any_io_executor core_ex;

                    when_callback(std::weak_ptr<Core> wc, std::shared_ptr<State> st, executor_type e, allocator_type a, pred_t pred, boost::asio::any_io_executor ce)
                        : weak_core(std::move(wc)), state(std::move(st)), ex(std::move(e)), alloc(std::move(a)), p(std::move(pred)), core_ex(std::move(ce)) {}

                    [[nodiscard]] bool is_async() const noexcept override final { return false; }
                    [[nodiscard]] const boost::asio::any_io_executor& get_executor() const noexcept override final { return core_ex; }

                    bool invoke_sync(const T& data) override final {
                        if (state->completed.load(std::memory_order_acquire)) return true;

                        if (!std::invoke(p, data)) return true;

                        if (state->completed.exchange(true, std::memory_order_acq_rel)) return true;

                        if (auto core = weak_core.lock()) {
                            if (state->reg_state.exchange(2, std::memory_order_acq_rel) == 1) core->unregister(state->id);
                        }

                        boost::asio::dispatch(ex, 
                            boost::asio::bind_allocator(alloc, [state = this->state, data = data]() mutable {
                                state->complete(boost::system::error_code{}, std::move(data));
                            })
                        );
                        return true;
                    }

                    void on_callback_destroyed() override final {
                        if (state->completed.exchange(true, std::memory_order_acq_rel)) return;

                        boost::asio::dispatch(ex, 
                            boost::asio::bind_allocator(alloc, [state = this->state]() mutable {
                                state->complete(boost::asio::error::make_error_code(boost::asio::error::operation_aborted));
                            })
                        );
                    }
                };

                auto cb_ptr = std::allocate_shared<when_callback>(alloc, weak_core, state, ex, alloc, std::forward<decltype(p)>(p), executor);

                {
                std::scoped_lock lock{core->mtx};
                core->callbacks.emplace_back(id, std::move(cb_ptr), prio);
                core->rebuild_snapshot();
                }

                int expected = 0;
                if (!state->reg_state.compare_exchange_strong(expected, 1, std::memory_order_acq_rel)) {
                    core->unregister(id);
                }
            },
            token,
            std::weak_ptr<Core>(core_),
            io_ctx_.executor_,
            std::forward<Predicate>(pred)
        );
    }

    template <typename Predicate, discusy::asio::ctf<T> CompletionToken = ctx::io_context::dct_t>
    requires ( std::invocable<Predicate&, const T&> && std::is_same_v<std::invoke_result_t<Predicate&, const T&>, bool> )
    auto when(Predicate&& pred, CompletionToken&& token = ctx::io_context::dct_t()) {
        return when_impl(callback_priority::user, std::forward<Predicate>(pred), std::forward<CompletionToken>(token));
    }

    template <typename Predicate, discusy::asio::ctf<T> CompletionToken = ctx::io_context::dct_t>
    requires ( std::invocable<Predicate&, const T&> && std::is_same_v<std::invoke_result_t<Predicate&, const T&>, bool> )
    auto when_system(Predicate&& pred, CompletionToken&& token = ctx::io_context::dct_t()) {
        return when_impl(callback_priority::system, std::forward<Predicate>(pred), std::forward<CompletionToken>(token));
    }
};

template <std::integral N>
class CounterCallback {
public:
    CounterCallback(N target, std::move_only_function<void()> callback) noexcept
        : counter_{target}, callback_{std::move(callback)} {}

    void arrive() {
        if (counter_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            std::invoke(callback_);
        }
    }

private:
    std::atomic<N> counter_;
    std::move_only_function<void()> callback_;
};

}