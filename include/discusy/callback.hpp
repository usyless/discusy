#pragma once

#include <algorithm>
#include <functional>
#include <utility>
#include <concepts>
#include <version>
#include <limits>
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
class Callback {
    struct callback_interface {
        std::atomic<bool> unregistered{false};

        callback_interface() noexcept = default;
        callback_interface(const callback_interface&) = delete;
        callback_interface& operator=(const callback_interface&) = delete;
        callback_interface(callback_interface&&) = delete;
        callback_interface& operator=(callback_interface&&) = delete;

        virtual ~callback_interface() noexcept = default;

        [[nodiscard]] virtual bool is_async() const noexcept = 0;
        [[nodiscard]] virtual const boost::asio::any_io_executor& get_executor() const noexcept = 0;
        virtual bool invoke_sync(const T&) { return true; }
        virtual coro::awaitable<bool> invoke_async(const T&) { co_return true; }
    };

    template <typename F>
    struct sync_callback final : callback_interface {
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
    struct async_callback final : callback_interface {
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

    struct when_interface : std::enable_shared_from_this<when_interface> {
        static constexpr auto INVALID_IDX = std::numeric_limits<std::size_t>::max();

        std::atomic<bool> completed{false};
        std::size_t vector_idx{INVALID_IDX};

        when_interface() noexcept = default;
        virtual ~when_interface() noexcept = default;
        when_interface(const when_interface&) = delete;
        when_interface& operator=(const when_interface&) = delete;
        when_interface(when_interface&&) = delete;
        when_interface& operator=(when_interface&&) = delete;

        virtual void try_match(const T& data) noexcept(noexcept(std::atomic_bool().load()) && noexcept(std::atomic_bool().exchange(true, std::memory_order_acq_rel))) = 0;
        virtual void on_callback_destroyed() noexcept(noexcept(std::atomic_bool().load())) = 0;

        constexpr void set_invalid_idx() noexcept {
            vector_idx = INVALID_IDX;
        }
        
        constexpr bool is_invalid_idx() const noexcept {
            return vector_idx == INVALID_IDX;
        }
    };

    struct entry {
        callback_id id{0};
        std::shared_ptr<callback_interface> func{};
    };

    struct Snapshot {
        boost::container::small_vector<std::shared_ptr<callback_interface>, 2> system_callbacks;
        boost::container::small_vector<std::shared_ptr<callback_interface>, 4> user_callbacks;
        std::size_t async_count{0};

        [[nodiscard]] bool empty() const noexcept {
            return system_callbacks.empty() && user_callbacks.empty();
        }
    };

    struct Core {
        ctx::io_context::executor_t exec;
        boost::container::small_vector<entry, 2> system_callbacks;
        boost::container::small_vector<entry, 4> user_callbacks;
        std::atomic<callback_id> cb_idx{0};
        std::size_t stale_count{0};
        std::atomic<std::shared_ptr<const Snapshot>> snapshot{nullptr};
        std::atomic<std::size_t> active_count{0};
        std::size_t async_count{0};
        std::mutex mtx;

        std::mutex incoming_mtx;
        boost::container::small_vector<std::shared_ptr<when_interface>, 8> incoming_waiters;
        std::atomic<std::size_t> when_waiters_count{0};
        std::mutex when_mtx;
        boost::container::small_vector<std::shared_ptr<when_interface>, 8> active_waiters;

        Core(const Core&) = delete;
        Core& operator=(const Core&) = delete;
        Core(Core&&) = delete;
        Core& operator=(Core&&) = delete;

        explicit Core(auto&& ex) : exec{std::forward<decltype(ex)>(ex)} {}

        ~Core() noexcept = default;

        bool push_when_waiter(std::shared_ptr<when_interface> node) {
            if (node->completed.load(std::memory_order_acquire)) return true;

            {
            std::scoped_lock lock{incoming_mtx};
            if (node->completed.load(std::memory_order_acquire)) return true;
            when_waiters_count.fetch_add(1, std::memory_order_release);
            try {
                incoming_waiters.push_back(std::move(node));
            } catch (...) {
                when_waiters_count.fetch_sub(1, std::memory_order_release);
                return false;
            }
            }
            return true;
        }

        void drain_incoming_when_waiters() {
            std::remove_cvref_t<decltype(incoming_waiters)> incoming;
            {
            std::scoped_lock lock{incoming_mtx};
            if (incoming_waiters.empty()) return;
            incoming = std::move(incoming_waiters);
            incoming_waiters.clear();
            }
            for (auto& node : incoming) {
                active_waiters.push_back(std::move(node));
                active_waiters.back()->vector_idx = active_waiters.size() - 1;
            }
        }

        void dispatch_when_waiters(const T& data) {
            if (when_waiters_count.load(std::memory_order_acquire) == 0) {
                return;
            }

            std::remove_cvref_t<decltype(active_waiters)> active;
            {
            std::scoped_lock lock{when_mtx};
            drain_incoming_when_waiters();
            if (active_waiters.empty()) return;
            active = active_waiters;
            }

            for (auto& node : active) {
                if (node->completed.load(std::memory_order_acquire)) continue;
                node->try_match(data);
            }

            {
            std::scoped_lock lock{when_mtx};
            drain_incoming_when_waiters();
            if (active_waiters.empty()) return;
            std::size_t removed = 0;
            for (std::size_t i = 0; i < active_waiters.size(); ) {
                auto& node = active_waiters[i];
                if (node->completed.load(std::memory_order_acquire)) {
                    node->set_invalid_idx();
                    const auto last_idx = active_waiters.size() - 1;
                    if (i != last_idx) {
                        active_waiters[i] = std::move(active_waiters.back());
                        active_waiters[i]->vector_idx = i;
                    }
                    active_waiters.pop_back();
                    ++removed;
                } else {
                    ++i;
                }
            }
            if (removed > 0) {
                when_waiters_count.fetch_sub(removed, std::memory_order_release);
            }
            }
        }

        // Clean up completed/cancelled waiters when events are not triggering matches
        void clear_completed_waiter(std::shared_ptr<when_interface> node) {
            if (when_waiters_count.load(std::memory_order_acquire) == 0) {
                return;
            }

            {
            std::scoped_lock lock{when_mtx};
            drain_incoming_when_waiters();

            if (node->is_invalid_idx()) return;
            const auto idx = node->vector_idx;
            node->set_invalid_idx();

            if (idx < active_waiters.size() && active_waiters[idx] == node) {
                const auto last_idx = active_waiters.size() - 1;
                if (idx != last_idx) {
                    active_waiters[idx] = std::move(active_waiters.back());
                    active_waiters[idx]->vector_idx = idx;
                }
                active_waiters.pop_back();
                when_waiters_count.fetch_sub(1, std::memory_order_release);
            }
            }
        }

        void abort_all_when_waiters() {
            std::remove_cvref_t<decltype(active_waiters)> pending;
            {
            std::scoped_lock lock{when_mtx};
            drain_incoming_when_waiters();
            for (auto& node : active_waiters) {
                node->set_invalid_idx();
            }
            pending = std::move(active_waiters);
            active_waiters.clear();
            when_waiters_count.store(0, std::memory_order_release);
            }

            for (auto& node : pending) {
                node->on_callback_destroyed();
            }
        }

        void rebuild_snapshot() {
            auto snap = std::make_shared<Snapshot>();
            std::size_t active = 0;
            std::size_t async_c = 0;

            auto compact_and_copy = [&](auto& src, auto& dst) {
                if (stale_count > 0) {
                    auto ret = std::remove_if(src.begin(), src.end(), [](const auto& e) {
                        return !e.func || e.func->unregistered.load(std::memory_order_relaxed);
                    });
                    src.erase(ret, src.end());
                }
                dst.reserve(src.size());
                for (const auto& e : src) {
                    if (e.func && !e.func->unregistered.load(std::memory_order_relaxed)) {
                        dst.emplace_back(e.func);
                        ++active;
                        if (e.func->is_async() || (e.func->get_executor() != exec)) {
                            ++async_c;
                        }
                    }
                }
            };

            compact_and_copy(system_callbacks, snap->system_callbacks);
            compact_and_copy(user_callbacks, snap->user_callbacks);
            stale_count = 0;

            snap->async_count = async_c;
            async_count = async_c;
            active_count.store(active, std::memory_order_release);
            snapshot.store(std::move(snap), std::memory_order_release);
        }

        bool unregister(const callback_id id) {
            if (id == 0) return false;
            
            std::shared_ptr<callback_interface> to_destroy;
            {
            std::scoped_lock lock{mtx};
            auto it_user = std::ranges::find_if(user_callbacks, [id](const auto& e) {
                return e.id == id;
            });
            entry* target_entry = nullptr;
            if (it_user != user_callbacks.end() && it_user->func) {
                target_entry = std::addressof(*it_user);
            } else {
                auto it_sys = std::ranges::find_if(system_callbacks, [id](const auto& e) {
                    return e.id == id;
                });
                if (it_sys != system_callbacks.end() && it_sys->func) {
                    target_entry = std::addressof(*it_sys);
                }
            }
            if (target_entry == nullptr) return false;
            
            if (target_entry->func->is_async() || (target_entry->func->get_executor() != exec)) {
                if (async_count > 0) --async_count;
            }

            target_entry->func->unregistered.store(true, std::memory_order_release);
            to_destroy = std::move(target_entry->func);
            ++stale_count;
            active_count.fetch_sub(1, std::memory_order_release);
            
            rebuild_snapshot();
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
                const auto cont = co_await ctx::io_context::co_launch<ctx::launch::fresh>([&entry, &data]() -> coro::awaitable<bool> {
                    return entry.invoke_async(data);
                }, target_ex, boost::asio::deferred);
                co_return cont;
            }
        } else {
            if (target_ex == current_ex) {
                co_return entry.invoke_sync(data);
            } else {
                const auto cont = co_await ctx::io_context::co_launch<ctx::launch::fresh>([&entry, &data]() -> coro::awaitable<bool> {
                    co_return entry.invoke_sync(data);
                }, target_ex, boost::asio::deferred);
                co_return cont;
            }
        }
    }

    static void fire_sync_internal(std::shared_ptr<Core> core, std::shared_ptr<const Snapshot> snap, const T& data) {
        if (!snap && (!core || core->when_waiters_count.load(std::memory_order_acquire) == 0)) return;

        if (snap) {
            for (const auto& entry_ptr : snap->system_callbacks) {
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
        }

        if (core) {
            core->dispatch_when_waiters(data);
        }

        if (snap) {
            for (const auto& entry_ptr : snap->user_callbacks) {
                if (entry_ptr && !entry_ptr->unregistered.load(std::memory_order_acquire)) {
                    try {
                        if (!invoke_entry_sync(*entry_ptr, data)) {
                            break;
                        }
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
    }

    static coro::awaitable<void> fire_async_internal(std::shared_ptr<Core> core, std::shared_ptr<const Snapshot> snap, T data) {
        if (!snap && (!core || core->when_waiters_count.load(std::memory_order_acquire) == 0)) co_return;

        const auto current_ex = co_await boost::asio::this_coro::executor;

        if (snap) {
            for (const auto& entry_ptr : snap->system_callbacks) {
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
        }

        if (core) {
            core->dispatch_when_waiters(data);
        }

        if (snap) {
            for (const auto& entry_ptr : snap->user_callbacks) {
                if (entry_ptr && !entry_ptr->unregistered.load(std::memory_order_acquire)) {
                    try {
                        if (!(co_await invoke_entry_async(*entry_ptr, data, current_ex))) {
                            break;
                        }
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

        co_return;
    }

public:
    Callback(const Callback&) = delete;
    Callback& operator=(const Callback&) = delete;
    Callback(Callback&&) = delete;
    Callback& operator=(Callback&&) = delete;

    explicit Callback(ctx::io_context& ctx) : core_{std::make_shared<Core>(ctx.executor_)}, io_ctx_{ctx} {}

    void set_bot(bot* b) noexcept {
        bot_ptr_ = b;
    }

    ~Callback() {
        if (!core_) return;

        std::remove_cvref_t<decltype(Core::system_callbacks)> system_old;
        std::remove_cvref_t<decltype(Core::user_callbacks)> user_old;
        {
        std::scoped_lock lock{core_->mtx};
        for (auto& entry : core_->system_callbacks) {
            if (entry.func) entry.func->unregistered.store(true, std::memory_order_release);
        }
        system_old = std::move(core_->system_callbacks);
        core_->system_callbacks.clear();

        for (auto& entry : core_->user_callbacks) {
            if (entry.func) entry.func->unregistered.store(true, std::memory_order_release);
        }
        user_old = std::move(core_->user_callbacks);
        core_->user_callbacks.clear();

        core_->stale_count = 0;
        core_->rebuild_snapshot();
        }

        system_old.clear();
        user_old.clear();

        core_->abort_all_when_waiters();
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
        if (prio == callback_priority::system) {
            core_->system_callbacks.emplace_back(id, std::move(ptr));
        } else {
            core_->user_callbacks.emplace_back(id, std::move(ptr));
        }
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
        if (!core_) return;
        const auto active_c = core_->active_count.load(std::memory_order_acquire);
        if (active_c == 0 && core_->when_waiters_count.load(std::memory_order_acquire) == 0) return;

        auto snap = core_->get_snapshot();
        if ((!snap || snap->empty()) && core_->when_waiters_count.load(std::memory_order_acquire) == 0) return;

        attach_bot(data, bot_ptr_);

        try {
            if (!snap || snap->async_count == 0) {
                io_ctx_.submit([core = core_, snap = std::move(snap), data = std::move(data)]() mutable {
                    fire_sync_internal(std::move(core), std::move(snap), data);
                });
            } else {
                io_ctx_.co_launch_detached([core = core_, snap = std::move(snap), data = std::move(data)]() mutable {
                    return fire_async_internal(std::move(core), std::move(snap), std::move(data));
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
        if (!core_) return;
        const auto active_c = core_->active_count.load(std::memory_order_acquire);
        if (active_c == 0 && core_->when_waiters_count.load(std::memory_order_acquire) == 0) return;

        auto snap = core_->get_snapshot();
        if ((!snap || snap->empty()) && core_->when_waiters_count.load(std::memory_order_acquire) == 0) return;

        try {
            if (!snap || snap->async_count == 0) {
                io_ctx_.submit([core = core_, bot_ptr_ = bot_ptr_, snap = std::move(snap), json = std::move(json)]() mutable {
                    W p{};
                    if (json::parse_json(p, json)) return;
                    attach_bot(p.d, bot_ptr_);
                    fire_sync_internal(std::move(core), std::move(snap), p.d);
                });
            } else {
                io_ctx_.co_launch_detached([core = core_, bot_ptr_ = bot_ptr_, snap = std::move(snap), json = std::move(json)]() mutable -> coro::awaitable<void> {
                    W p{};
                    if (json::parse_json(p, json)) co_return;
                    attach_bot(p.d, bot_ptr_);
                    co_await fire_async_internal(std::move(core), std::move(snap), std::move(p.d));
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

    template <typename Predicate, typename Handler, typename Executor, typename Allocator>
    struct when_node final : when_interface {
        Predicate pred;
        Handler handler;
        boost::asio::executor_work_guard<Executor> work;
        Executor ex;
        ctx::io_context::executor_t io_exec;
        Allocator alloc;

        when_node(auto&& p, auto&& h, auto&& e, auto&& io_e, auto&& a)
            : pred(std::forward<decltype(p)>(p)),
              handler(std::forward<decltype(h)>(h)),
              work(boost::asio::make_work_guard(e)),
              ex(std::forward<decltype(e)>(e)),
              io_exec(std::forward<decltype(io_e)>(io_e)),
              alloc(std::forward<decltype(a)>(a)) {}

        void complete(boost::system::error_code ec, T data = T{}) {
            auto slot = boost::asio::get_associated_cancellation_slot(handler);
            if (slot.is_connected()) slot.clear(); // TODO: is this actually necessary?
            work.reset();
            std::move(handler)(ec, std::move(data));
        }

        void try_match(const T& data) noexcept(noexcept(std::atomic_bool().load()) && noexcept(std::atomic_bool().exchange(true, std::memory_order_acq_rel))) override final {
            if (this->completed.load(std::memory_order_acquire)) return;

            bool matched = false;
            try {
                matched = std::invoke(pred, data);
            }
            #ifdef DISCUSY_LOGGING
            catch (const std::exception& e) {
                log::Logger{}("When predicate exception: {}", e.what());
            }
            #endif
            catch (...) {
                #ifdef DISCUSY_LOGGING
                log::Logger{}("When predicate unknown exception");
                #endif
            }

            if (!matched) return;

            if (this->completed.exchange(true, std::memory_order_acq_rel)) return;

            try {
                boost::asio::post(io_exec,
                    boost::asio::bind_allocator(alloc, [self = this->shared_from_this(), d = data, ex = this->ex, alloc = this->alloc]() mutable {
                        boost::asio::dispatch(ex,
                            boost::asio::bind_allocator(alloc, [self = std::move(self), d = std::move(d)]() mutable {
                                static_cast<when_node*>(self.get())->complete(boost::system::error_code{}, std::move(d));
                            })
                        );
                    })
                );
            } catch (...) {
                #ifdef DISCUSY_LOGGING
                log::Logger{}("When handler post exception");
                #endif
                try {
                    boost::asio::post(io_exec,
                        boost::asio::bind_allocator(alloc, [self = this->shared_from_this(), ex = this->ex, alloc = this->alloc]() mutable {
                            boost::asio::dispatch(ex,
                                boost::asio::bind_allocator(alloc, [self = std::move(self)]() mutable {
                                    static_cast<when_node*>(self.get())->complete(boost::asio::error::make_error_code(boost::asio::error::no_memory));
                                })
                            );
                        })
                    );
                } catch (...) {} // NOLINT(bugprone-empty-catch)
            }
        }

        void on_callback_destroyed() noexcept(noexcept(std::atomic_bool().load())) override final {
            if (this->completed.exchange(true, std::memory_order_acq_rel)) return;

            try {
                boost::asio::post(io_exec,
                    boost::asio::bind_allocator(alloc, [self = this->shared_from_this(), ex = this->ex, alloc = this->alloc]() mutable {
                        boost::asio::dispatch(ex,
                            boost::asio::bind_allocator(alloc, [self = std::move(self)]() mutable {
                                static_cast<when_node*>(self.get())->complete(boost::asio::error::make_error_code(boost::asio::error::operation_aborted));
                            })
                        );
                    })
                );
            } catch (...) {
                #ifdef DISCUSY_LOGGING
                log::Logger{}("When on_callback_destroyed post exception");
                #endif
            }
        }
    };

    // makes a copy of the data, unideal
    // if you want to use it with awaitable operators, pass in boost::asio::use_awaitable
    template <typename Predicate, discusy::asio::ctf<T> CompletionToken = ctx::io_context::dct_t>
    requires ( std::invocable<Predicate&, const T&> && std::is_same_v<std::invoke_result_t<Predicate&, const T&>, bool> )
    auto when(Predicate&& pred, CompletionToken&& token = ctx::io_context::dct_t()) {
        return boost::asio::async_initiate<CompletionToken, void(boost::system::error_code, T)>(
            [](discusy::asio::chf<T> auto&& handler, std::weak_ptr<Core> weak_core, ctx::io_context::executor_t executor, auto&& p) mutable {
                using handler_t = std::decay_t<decltype(handler)>;

                auto alloc = boost::asio::get_associated_allocator(
                    handler, 
                    boost::asio::recycling_allocator<void>{}
                );

                auto ex = boost::asio::get_associated_executor(handler, executor);
                using executor_type = decltype(ex);
                using allocator_type = decltype(alloc);
                using pred_t = std::decay_t<decltype(p)>;

                auto core = weak_core.lock();
                if (!core) {
                    auto ex_imm = boost::asio::get_associated_immediate_executor(handler, executor);
                    boost::asio::dispatch(ex_imm,
                        boost::asio::bind_allocator(alloc, [h = std::forward<decltype(handler)>(handler)]() mutable {
                            std::move(h)(boost::asio::error::make_error_code(boost::asio::error::operation_aborted), T{});
                        })
                    );
                    return;
                }

                using node_t = when_node<pred_t, handler_t, executor_type, allocator_type>;

                auto node = std::allocate_shared<node_t>(
                    alloc,
                    std::forward<decltype(p)>(p),
                    std::forward<decltype(handler)>(handler),
                    ex,
                    executor,
                    alloc
                );

                auto slot = boost::asio::get_associated_cancellation_slot(node->handler);
                if (slot.is_connected()) {
                    slot.assign([node_weak = std::weak_ptr<node_t>{node}, ex, alloc, executor, weak_core](boost::asio::cancellation_type type) noexcept {
                        if (type == boost::asio::cancellation_type::none) return;
                        auto node = node_weak.lock();
                        if (!node) return;

                        if (node->completed.exchange(true, std::memory_order_acq_rel)) return;

                        try {
                            boost::asio::post(executor, 
                                boost::asio::bind_allocator(alloc, [node, alloc, ex, weak_core]() mutable {
                                    if (auto core = weak_core.lock()) {
                                        core->clear_completed_waiter(node);
                                    }

                                    boost::asio::dispatch(ex, 
                                        boost::asio::bind_allocator(alloc, [node = std::move(node)]() mutable {
                                            node->complete(boost::asio::error::make_error_code(boost::asio::error::operation_aborted));
                                        })
                                    );
                                })
                            );
                        } catch (...) {
                            #ifdef DISCUSY_LOGGING
                            log::Logger{}("When cancellation post exception");
                            #endif
                        }
                    });
                }

                if (!core->push_when_waiter(node)) {
                    if (node->completed.exchange(true, std::memory_order_acq_rel)) return;

                    boost::asio::post(executor, 
                        boost::asio::bind_allocator(alloc, [node, alloc, ex]() mutable {
                            boost::asio::dispatch(ex, 
                                boost::asio::bind_allocator(alloc, [node = std::move(node)]() mutable {
                                    node->complete(boost::asio::error::make_error_code(boost::asio::error::no_memory));
                                })
                            );
                        })
                    );
                }
            },
            token,
            std::weak_ptr<Core>(core_),
            io_ctx_.executor_,
            std::forward<Predicate>(pred)
        );
    }
};

template <std::integral N>
class CounterCallback {
public:
    CounterCallback(N target, 
    #if defined(__cpp_lib_move_only_function) && __cpp_lib_move_only_function >= 202110L
        std::move_only_function<void()> 
    #else
        std::function<void()> 
    #endif
        callback) noexcept
        : counter_{target}, callback_{std::move(callback)} {}

    void arrive() {
        if (counter_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            std::invoke(callback_);
        }
    }

private:
    std::atomic<N> counter_;
    #if defined(__cpp_lib_move_only_function) && __cpp_lib_move_only_function >= 202110L
    std::move_only_function<void()> callback_;
    #else
    std::function<void()> callback_;
    #endif
};

}