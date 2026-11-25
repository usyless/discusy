#pragma once

#include <algorithm>
#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <version>

#include <boost/container/small_vector.hpp>

#include "coro.hpp"
#include "io_context.hpp"
#include "types.hpp"
#include "log.hpp" // IWYU pragma: keep
#ifdef DISCUSY_USE_ETF
#include "etf.hpp"
#else
#include "json.hpp"
#endif
#include "asio_helpers.hpp"
#include "atomic_shared_ptr.hpp"

namespace discusy {

#ifdef DISCUSY_USE_ETF
namespace gateway_protocol = etf;
#else
namespace gateway_protocol = json;
#endif

enum class callback_priority : std::uint8_t {
    user = 0,
    system = 1,
};

class bot;

template <typename Obj>
GLZ_ALWAYS_INLINE void attach_bot(Obj& obj, bot* b) noexcept {
    if constexpr (requires { obj.set_bot_void(b); }) {
        obj.set_bot_void(b);
    }
}

namespace detail {

#ifdef DISCUSY_LOGGING
template <typename... Args>
inline void callback_log_noexcept(const std::format_string<Args...>& fmt, Args&&... args) noexcept {
    try {
        log::Logger{}(fmt, std::forward<Args>(args)...);
    } catch (...) {} // NOLINT(bugprone-empty-catch)
}
#endif

struct callback_interface {
    boost::asio::any_io_executor ex;
    std::atomic<bool> unregistered{false};
    bool is_async_{false};

    explicit callback_interface(boost::asio::any_io_executor exec, bool is_async = false) noexcept
        : ex{std::move(exec)}, is_async_{is_async} {}
    callback_interface() noexcept = default;
    callback_interface(const callback_interface&) = delete;
    callback_interface& operator=(const callback_interface&) = delete;
    callback_interface(callback_interface&&) = delete;
    callback_interface& operator=(callback_interface&&) = delete;

    virtual ~callback_interface() noexcept = default;

    [[nodiscard]] constexpr bool is_async() const noexcept { return is_async_; }
    [[nodiscard]] const boost::asio::any_io_executor& get_executor() const noexcept { return ex; }
    virtual bool invoke_sync(const void*) { return true; }
    virtual coro::awaitable<bool> invoke_async(const void*) { co_return true; }
};

struct when_interface : std::enable_shared_from_this<when_interface> {
    static constexpr auto INVALID_IDX = std::numeric_limits<std::size_t>::max();

    // Serializes predicate invocations on this node and ensures any in-flight
    // predicate finishes before cancellation/destruction resumes the caller.
    std::mutex match_mtx;
    std::atomic<bool> completed{false};
    std::atomic<bool> complete_entered{false};
    std::size_t vector_idx{INVALID_IDX};

    when_interface() noexcept = default;
    virtual ~when_interface() noexcept = default;
    when_interface(const when_interface&) = delete;
    when_interface& operator=(const when_interface&) = delete;
    when_interface(when_interface&&) = delete;
    when_interface& operator=(when_interface&&) = delete;

    // Returns true if this waiter is now completed (matched, cancelled, or aborted).
    virtual bool try_match(const void* data) noexcept = 0;
    virtual void on_callback_destroyed(const ctx::io_context::executor_t& io_exec) noexcept = 0;

    constexpr void set_invalid_idx() noexcept {
        vector_idx = INVALID_IDX;
    }

    [[nodiscard]] constexpr bool is_invalid_idx() const noexcept {
        return vector_idx == INVALID_IDX;
    }
};

enum class push_waiter_result : std::uint8_t {
    pushed,
    already_completed,
    destroyed,
    no_memory,
};

struct callback_entry {
    callback_id id{0};
    std::shared_ptr<callback_interface> func{};
};

struct callback_snapshot {
    boost::container::small_vector<std::shared_ptr<callback_interface>, 2> system_callbacks;
    boost::container::small_vector<std::shared_ptr<callback_interface>, 4> user_callbacks;
    std::size_t async_count{0};

    [[nodiscard]] bool empty() const noexcept {
        return system_callbacks.empty() && user_callbacks.empty();
    }
};

struct callback_core {
    ctx::io_context::executor_t exec;
    boost::asio::any_io_executor any_exec;
    boost::container::small_vector<callback_entry, 2> system_callbacks;
    boost::container::small_vector<callback_entry, 4> user_callbacks;
    std::atomic<callback_id> cb_idx{0};
    atomic_shared_ptr<const callback_snapshot> snapshot{nullptr};
    std::atomic<std::size_t> active_count{0};
    std::mutex mtx;

    std::mutex incoming_mtx;
    boost::container::small_vector<std::shared_ptr<when_interface>, 8> incoming_waiters;
    bool waiters_destroyed{false};
    std::atomic<std::size_t> when_waiters_count{0};
    std::mutex when_mtx;
    boost::container::small_vector<std::shared_ptr<when_interface>, 8> active_waiters;

    callback_core(const callback_core&) = delete;
    callback_core& operator=(const callback_core&) = delete;
    callback_core(callback_core&&) = delete;
    callback_core& operator=(callback_core&&) = delete;

    explicit callback_core(const ctx::io_context::executor_t& ex)
        : exec{ex}, any_exec{ex} {}

    ~callback_core() noexcept = default;

    push_waiter_result push_when_waiter(std::shared_ptr<when_interface> node) noexcept {
        if (node->completed.load(std::memory_order_acquire)) {
            return push_waiter_result::already_completed;
        }

        {
            std::scoped_lock lock{incoming_mtx};
            if (waiters_destroyed) {
                return push_waiter_result::destroyed;
            }
            if (node->completed.load(std::memory_order_acquire)) {
                return push_waiter_result::already_completed;
            }
            try {
                incoming_waiters.emplace_back(std::move(node));
            } catch (...) {
                return push_waiter_result::no_memory;
            }
            when_waiters_count.fetch_add(1, std::memory_order_release);
        }
        return push_waiter_result::pushed;
    }

    // Must be called with when_mtx held. Returns any waiters that could not be
    // moved into active_waiters due to OOM so the caller can abort them outside locks.
    boost::container::small_vector<std::shared_ptr<when_interface>, 8> drain_incoming_when_waiters() noexcept {
        boost::container::small_vector<std::shared_ptr<when_interface>, 8> incoming;
        {
            std::scoped_lock lock{incoming_mtx};
            if (incoming_waiters.empty()) return {};
            incoming = std::move(incoming_waiters);
            incoming_waiters.clear();
        }

        try {
            active_waiters.reserve(active_waiters.size() + incoming.size());
        } catch (...) {
            when_waiters_count.fetch_sub(incoming.size(), std::memory_order_release);
            return incoming;
        }

        for (auto& node : incoming) {
            auto& waiter = active_waiters.emplace_back(std::move(node));
            waiter->vector_idx = active_waiters.size() - 1;
        }
        return {};
    }

    void dispatch_when_waiters(const void* data) {
        if (when_waiters_count.load(std::memory_order_acquire) == 0) {
            return;
        }

        boost::container::small_vector<std::shared_ptr<when_interface>, 8> active;
        boost::container::small_vector<std::shared_ptr<when_interface>, 8> failed_drain;
        {
            std::scoped_lock lock{when_mtx};
            failed_drain = drain_incoming_when_waiters();
            if (!active_waiters.empty()) {
                active = active_waiters;
            }
        }

        for (auto& node : failed_drain) {
            node->on_callback_destroyed(exec);
        }
        failed_drain.clear();

        if (active.empty()) return;

        bool any_completed = false;
        for (auto& node : active) {
            if (node->completed.load(std::memory_order_acquire)) {
                any_completed = true;
                continue;
            }
            if (node->try_match(data)) {
                any_completed = true;
            }
        }

        if (!any_completed) return;

        boost::container::small_vector<std::shared_ptr<when_interface>, 8> removed_nodes;
        {
            std::scoped_lock lock{when_mtx};
            if (active_waiters.empty()) return;
            std::size_t removed = 0;
            for (std::size_t i = 0; i < active_waiters.size(); ) {
                auto& node = active_waiters[i];
                if (node->completed.load(std::memory_order_acquire)) {
                    node->set_invalid_idx();
                    try {
                        removed_nodes.emplace_back(std::move(node));
                    } catch (...) {
                        // Keep alive via `active` if present; otherwise falls back to immediate release.
                    }
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

    // Clean up completed/cancelled waiters when events are not triggering matches.
    // Zero-allocation and noexcept.
    void clear_completed_waiter(const std::shared_ptr<when_interface>& node) noexcept {
        if (when_waiters_count.load(std::memory_order_acquire) == 0) {
            return;
        }

        std::scoped_lock lock{when_mtx};
        if (!node->is_invalid_idx()) {
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
            return;
        }

        // Not in active_waiters; it may still be queued in incoming_waiters.
        std::scoped_lock in_lock{incoming_mtx};
        for (std::size_t i = 0; i < incoming_waiters.size(); ++i) {
            if (incoming_waiters[i] == node) {
                const auto last_idx = incoming_waiters.size() - 1;
                if (i != last_idx) {
                    incoming_waiters[i] = std::move(incoming_waiters.back());
                }
                incoming_waiters.pop_back();
                when_waiters_count.fetch_sub(1, std::memory_order_release);
                return;
            }
        }
    }

    void abort_all_when_waiters() noexcept {
        std::unique_lock when_lock{when_mtx};
        std::unique_lock in_lock{incoming_mtx};

        waiters_destroyed = true;

        for (auto& node : active_waiters) {
            node->set_invalid_idx();
        }

        auto pending_active = std::move(active_waiters);
        active_waiters.clear();
        auto pending_incoming = std::move(incoming_waiters);
        incoming_waiters.clear();
        when_waiters_count.store(0, std::memory_order_release);

        in_lock.unlock();
        when_lock.unlock();

        for (auto& node : pending_active) {
            node->on_callback_destroyed(exec);
        }
        for (auto& node : pending_incoming) {
            node->on_callback_destroyed(exec);
        }
    }

    // Must be called with mtx held. Returns the replaced snapshot so the caller
    // destroys it outside mtx.
    [[nodiscard]] std::shared_ptr<const callback_snapshot> rebuild_snapshot() {
        auto snap = std::make_shared<callback_snapshot>();
        std::size_t active = 0;
        std::size_t async_c = 0;

        auto copy_entries = [&](const auto& src, auto& dst) {
            dst.reserve(src.size());
            for (const auto& e : src) {
                if (e.func && !e.func->unregistered.load(std::memory_order_relaxed)) {
                    dst.emplace_back(e.func);
                    ++active;
                    if (e.func->is_async() || (e.func->get_executor() != any_exec)) {
                        ++async_c;
                    }
                }
            }
        };

        copy_entries(system_callbacks, snap->system_callbacks);
        copy_entries(user_callbacks, snap->user_callbacks);

        snap->async_count = async_c;
        // Publish snapshot BEFORE active_count so readers seeing active_count > 0
        // via acquire load are guaranteed to observe the new snapshot.
        auto old_snap = snapshot.exchange(std::move(snap), std::memory_order_acq_rel);
        active_count.store(active, std::memory_order_release);
        return old_snap;
    }

    bool unregister(const callback_id id) {
        if (id == 0) return false;

        std::shared_ptr<callback_interface> to_destroy;
        std::shared_ptr<const callback_snapshot> old_snap;
        {
            std::scoped_lock lock{mtx};
            auto it_user = std::ranges::find_if(user_callbacks, [id](const auto& e) {
                return e.id == id;
            });
            if (it_user != user_callbacks.end() && it_user->func) {
                it_user->func->unregistered.store(true, std::memory_order_release);
                to_destroy = std::move(it_user->func);
                user_callbacks.erase(it_user);
            } else {
                auto it_sys = std::ranges::find_if(system_callbacks, [id](const auto& e) {
                    return e.id == id;
                });
                if (it_sys == system_callbacks.end() || !it_sys->func) {
                    return false;
                }
                it_sys->func->unregistered.store(true, std::memory_order_release);
                to_destroy = std::move(it_sys->func);
                system_callbacks.erase(it_sys);
            }

            try {
                old_snap = rebuild_snapshot();
            } catch (...) {
                // Snapshot remains valid (unregistered flag skips the removed callback).
                active_count.store(system_callbacks.size() + user_callbacks.size(), std::memory_order_release);
                throw;
            }
        }
        return true;
    }

    [[nodiscard]] std::shared_ptr<const callback_snapshot> get_snapshot() const noexcept {
        return snapshot.load(std::memory_order_acquire);
    }
};

struct erased_event_data {
    void* owner{nullptr};
    const void* data{nullptr};
    void (*destroy)(void*) noexcept {nullptr};

    erased_event_data() noexcept = default;
    erased_event_data(void* o, const void* d, void (*fn)(void*) noexcept) noexcept
        : owner{o}, data{d}, destroy{fn} {}
    erased_event_data(const erased_event_data&) = delete;
    erased_event_data& operator=(const erased_event_data&) = delete;
    erased_event_data(erased_event_data&& other) noexcept
        : owner{std::exchange(other.owner, nullptr)},
          data{std::exchange(other.data, nullptr)},
          destroy{std::exchange(other.destroy, nullptr)} {}
    erased_event_data& operator=(erased_event_data&& other) noexcept {
        if (this != &other) {
            reset();
            owner = std::exchange(other.owner, nullptr);
            data = std::exchange(other.data, nullptr);
            destroy = std::exchange(other.destroy, nullptr);
        }
        return *this;
    }
    ~erased_event_data() noexcept { reset(); }

    void reset() noexcept {
        if (owner != nullptr && destroy != nullptr) {
            destroy(owner);
        }
        owner = nullptr;
        data = nullptr;
        destroy = nullptr;
    }

    [[nodiscard]] explicit operator bool() const noexcept { return data != nullptr; }
};

class callback_base {
protected:
    using payload_sync_fn = void (*)(std::shared_ptr<callback_core>, std::shared_ptr<const callback_snapshot>, bot*, std::string&);
    using payload_parse_fn = erased_event_data (*)(bot*, std::string&);

    std::shared_ptr<callback_core> core_;
    ctx::io_context& io_ctx_;
    std::atomic<bot*> bot_ptr_{nullptr};

    [[nodiscard]] static bool same_executor(
        const boost::asio::any_io_executor& target_ex,
        const boost::asio::any_io_executor& current_ex,
        const boost::asio::any_io_executor& core_exec
    ) noexcept {
        return target_ex == current_ex || target_ex == core_exec;
    }

    static auto invoke_sync_on_executor(
        callback_interface& entry,
        const void* data,
        const boost::asio::any_io_executor& target_ex
    ) {
        auto token = boost::asio::deferred;
        return boost::asio::async_initiate<boost::asio::deferred_t, void(std::exception_ptr, bool)>(
            [](auto&& handler, callback_interface* ep, const void* dp, boost::asio::any_io_executor target) mutable {
                auto alloc = boost::asio::get_associated_allocator(
                    handler,
                    boost::asio::recycling_allocator<void>{}
                );
                auto handler_ex = boost::asio::get_associated_executor(handler, target);
                auto work = boost::asio::make_work_guard(handler_ex);

                boost::asio::post(
                    target,
                    boost::asio::bind_allocator(
                        alloc,
                        [h = std::forward<decltype(handler)>(handler),
                         handler_ex = std::move(handler_ex),
                         work = std::move(work),
                         alloc,
                         ep,
                         dp]() mutable {
                            std::exception_ptr eptr;
                            bool res = true;
                            try {
                                res = ep->invoke_sync(dp);
                            } catch (...) {
                                eptr = std::current_exception();
                            }
                            boost::asio::post(
                                handler_ex,
                                boost::asio::bind_allocator(
                                    alloc,
                                    [h = std::move(h), work = std::move(work), eptr, res]() mutable {
                                        work.reset();
                                        std::move(h)(eptr, res);
                                    }
                                )
                            );
                        }
                    )
                );
            },
            token,
            std::addressof(entry),
            data,
            target_ex
        );
    }

    static void fire_sync_internal(
        std::shared_ptr<callback_core> core,
        std::shared_ptr<const callback_snapshot> snap,
        const void* data
    ) {
        if (!snap && (!core || core->when_waiters_count.load(std::memory_order_acquire) == 0)) return;

        if (snap) {
            for (const auto& entry_ptr : snap->system_callbacks) {
                if (entry_ptr && !entry_ptr->unregistered.load(std::memory_order_acquire)) {
                    try {
                        entry_ptr->invoke_sync(data);
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
                        if (!entry_ptr->invoke_sync(data)) {
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

    static coro::awaitable<void> fire_async_internal(
        std::shared_ptr<callback_core> core,
        std::shared_ptr<const callback_snapshot> snap,
        erased_event_data data_box
    ) {
        if (!snap && (!core || core->when_waiters_count.load(std::memory_order_acquire) == 0)) co_return;

        const void* const data = data_box.data;
        const auto current_ex = co_await boost::asio::this_coro::executor;
        const auto& core_exec = core ? core->any_exec : current_ex;

        if (snap) {
            for (const auto& entry_ptr : snap->system_callbacks) {
                if (entry_ptr && !entry_ptr->unregistered.load(std::memory_order_acquire)) {
                    try {
                        const auto& target_ex = entry_ptr->get_executor();
                        if (same_executor(target_ex, current_ex, core_exec)) {
                            if (entry_ptr->is_async()) {
                                co_await entry_ptr->invoke_async(data);
                            } else {
                                entry_ptr->invoke_sync(data);
                            }
                        } else if (entry_ptr->is_async()) {
                            co_await ctx::io_context::co_launch<ctx::launch::fresh>([entry_ptr, data]() -> coro::awaitable<bool> {
                                return entry_ptr->invoke_async(data);
                            }, target_ex, boost::asio::deferred);
                        } else {
                            co_await invoke_sync_on_executor(*entry_ptr, data, target_ex);
                        }
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
                        bool cont = true;
                        const auto& target_ex = entry_ptr->get_executor();
                        if (same_executor(target_ex, current_ex, core_exec)) {
                            if (entry_ptr->is_async()) {
                                cont = co_await entry_ptr->invoke_async(data);
                            } else {
                                cont = entry_ptr->invoke_sync(data);
                            }
                        } else if (entry_ptr->is_async()) {
                            cont = co_await ctx::io_context::co_launch<ctx::launch::fresh>([entry_ptr, data]() -> coro::awaitable<bool> {
                                return entry_ptr->invoke_async(data);
                            }, target_ex, boost::asio::deferred);
                        } else {
                            cont = co_await invoke_sync_on_executor(*entry_ptr, data, target_ex);
                        }
                        if (!cont) {
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

    void launch_async_erased(
        std::shared_ptr<callback_core> core,
        std::shared_ptr<const callback_snapshot> snap,
        erased_event_data data
    ) {
        io_ctx_.co_launch_detached([core = std::move(core), snap = std::move(snap), data = std::move(data)]() mutable {
            return fire_async_internal(std::move(core), std::move(snap), std::move(data));
        });
    }

    void fire_payload_erased(
        std::string_view payload,
        const payload_sync_fn sync_fn,
        const payload_parse_fn parse_fn
    ) {
        if (!core_) return;
        const auto active_c = core_->active_count.load(std::memory_order_acquire);
        const auto waiters_c = core_->when_waiters_count.load(std::memory_order_acquire);
        if (active_c == 0 && waiters_c == 0) return;

        auto snap = active_c != 0 ? core_->get_snapshot() : nullptr;
        if ((!snap || snap->empty()) && core_->when_waiters_count.load(std::memory_order_acquire) == 0) return;

        auto* const bot_ptr = bot_ptr_.load(std::memory_order_relaxed);

        try {
            if (!snap || snap->async_count == 0) {
                io_ctx_.submit([core = core_, bot_ptr, snap = std::move(snap), payload = std::string(payload), sync_fn]() mutable {
                    sync_fn(std::move(core), std::move(snap), bot_ptr, payload);
                });
            } else {
                io_ctx_.co_launch_detached([core = core_, bot_ptr, snap = std::move(snap), payload = std::string(payload), parse_fn]() mutable -> coro::awaitable<void> {
                    auto data = parse_fn(bot_ptr, payload);
                    if (!data) co_return;
                    co_await fire_async_internal(std::move(core), std::move(snap), std::move(data));
                    co_return;
                });
            }
        }
        #ifdef DISCUSY_LOGGING
        catch (const std::exception& e) {
            log::Logger{}("Callback fire_payload exception: {}", e.what());
        }
        #endif
        catch (...) {
            #ifdef DISCUSY_LOGGING
            log::Logger{}("Callback fire_payload unknown exception");
            #endif
        }
    }

    callback_id register_callback(
        const callback_id id,
        const callback_priority prio,
        std::shared_ptr<callback_interface> ptr
    ) {
        std::shared_ptr<const callback_snapshot> old_snap;
        {
            std::scoped_lock lock{core_->mtx};
            if (prio == callback_priority::system) {
                core_->system_callbacks.emplace_back(id, std::move(ptr));
                try {
                    old_snap = core_->rebuild_snapshot();
                } catch (...) {
                    core_->system_callbacks.pop_back();
                    throw;
                }
            } else {
                core_->user_callbacks.emplace_back(id, std::move(ptr));
                try {
                    old_snap = core_->rebuild_snapshot();
                } catch (...) {
                    core_->user_callbacks.pop_back();
                    throw;
                }
            }
        }
        return id;
    }

public:
    callback_base(const callback_base&) = delete;
    callback_base& operator=(const callback_base&) = delete;
    callback_base(callback_base&&) = delete;
    callback_base& operator=(callback_base&&) = delete;

    explicit callback_base(ctx::io_context& ctx)
        : core_{std::make_shared<callback_core>(ctx.executor_)}, io_ctx_{ctx} {}

    void set_bot(bot* b) noexcept {
        bot_ptr_.store(b, std::memory_order_relaxed);
    }

    ~callback_base() noexcept {
        if (!core_) return;

        std::shared_ptr<const callback_snapshot> old_snap;
        std::unique_lock lock{core_->mtx};
        for (auto& e : core_->system_callbacks) {
            if (e.func) e.func->unregistered.store(true, std::memory_order_release);
        }
        auto system_old = std::move(core_->system_callbacks);
        core_->system_callbacks.clear();

        for (auto& e : core_->user_callbacks) {
            if (e.func) e.func->unregistered.store(true, std::memory_order_release);
        }
        auto user_old = std::move(core_->user_callbacks);
        core_->user_callbacks.clear();

        old_snap = core_->snapshot.exchange(nullptr, std::memory_order_acq_rel);
        core_->active_count.store(0, std::memory_order_release);
        lock.unlock();

        old_snap.reset();
        system_old.clear();
        user_old.clear();

        core_->abort_all_when_waiters();
    }

    [[nodiscard]] callback_id reserve_id() noexcept {
        return ++core_->cb_idx;
    }

    bool unregister(const callback_id id) {
        if (!core_) return false;
        return core_->unregister(id);
    }

    [[nodiscard]] bool has_listeners() const noexcept {
        if (!core_) return false;
        return core_->active_count.load(std::memory_order_acquire) != 0 ||
               core_->when_waiters_count.load(std::memory_order_acquire) != 0;
    }
};

} // namespace detail

// should callback unregister and register be dispatch or post?
// if im currently running callbacks and i unregister one should i unregister it immediately
// or unregister it after theyve all run?
//
// there will also be some mutex contention here but just dont register or unregister too much and its fine
template <typename T>
class Callback : private detail::callback_base {
    template <typename F>
    struct sync_callback final : detail::callback_interface {
        F func;

        template <typename FF>
        explicit sync_callback(FF&& f, boost::asio::any_io_executor executor) noexcept(std::is_nothrow_constructible_v<F, FF&&>)
            : detail::callback_interface{std::move(executor), false}, func(std::forward<FF>(f)) {}

        bool invoke_sync(const void* data_ptr) override final {
            const auto& data = *static_cast<const T*>(data_ptr);
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
    struct async_callback final : detail::callback_interface {
        F func;

        template <typename FF>
        explicit async_callback(FF&& f, boost::asio::any_io_executor executor) noexcept(std::is_nothrow_constructible_v<F, FF&&>)
            : detail::callback_interface{std::move(executor), true}, func(std::forward<FF>(f)) {}

        coro::awaitable<bool> invoke_async(const void* data_ptr) override final {
            const auto& data = *static_cast<const T*>(data_ptr);
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

    template <typename W>
    #if defined(_MSC_VER)
    __declspec(noinline)
    #elif defined(__GNUC__) || defined(__clang__)
    __attribute__((noinline))
    #endif
    static bool parse_and_attach_payload(W& p, bot* bot_ptr, std::string& payload) {
        if (gateway_protocol::parse(p, payload)) return false;
        attach_bot(p.d, bot_ptr);
        return true;
    }

    template <typename W>
    static void dispatch_payload_sync(
        std::shared_ptr<detail::callback_core> core,
        std::shared_ptr<const detail::callback_snapshot> snap,
        bot* bot_ptr,
        std::string& payload
    ) {
        W p{};
        if (!parse_and_attach_payload(p, bot_ptr, payload)) return;
        detail::callback_base::fire_sync_internal(std::move(core), std::move(snap), std::addressof(p.d));
    }

    template <typename W>
    static detail::erased_event_data parse_payload_boxed(bot* bot_ptr, std::string& payload) {
        auto holder = std::make_unique<W>();
        if (!parse_and_attach_payload(*holder, bot_ptr, payload)) return {};
        W* raw = holder.release();
        return detail::erased_event_data{
            raw,
            std::addressof(raw->d),
            [](void* ptr) noexcept { delete static_cast<W*>(ptr); }
        };
    }

    template <typename Predicate, typename Handler, typename Executor>
    struct when_node final : detail::when_interface {
        Predicate pred;
        Handler handler;
        boost::asio::executor_work_guard<Executor> work;
        Executor ex;

        when_node(auto&& p, auto&& h, auto&& e)
            : pred(std::forward<decltype(p)>(p)),
              handler(std::forward<decltype(h)>(h)),
              work(boost::asio::make_work_guard(e)),
              ex(std::forward<decltype(e)>(e)) {}

        static void complete(std::shared_ptr<detail::when_interface> self_ptr, boost::system::error_code ec, T data = T{}) {
            auto* self = static_cast<when_node*>(self_ptr.get());
            self->complete_entered.store(true, std::memory_order_release);
            auto h = std::move(self->handler);
            auto slot = boost::asio::get_associated_cancellation_slot(h);
            if (slot.is_connected()) slot.clear();
            self->work.reset();
            self_ptr.reset();
            std::move(h)(ec, std::move(data));
        }

        bool try_match(const void* data_ptr) noexcept override final {
            if (this->completed.load(std::memory_order_acquire)) return true;

            const auto& data = *static_cast<const T*>(data_ptr);
            bool matched = false;
            {
                std::scoped_lock lock{this->match_mtx};
                if (this->completed.load(std::memory_order_acquire)) return true;

                try {
                    matched = std::invoke(pred, data);
                }
                #ifdef DISCUSY_LOGGING
                catch (const std::exception& e) {
                    detail::callback_log_noexcept("When predicate exception: {}", e.what());
                }
                #endif
                catch (...) {
                    #ifdef DISCUSY_LOGGING
                    detail::callback_log_noexcept("When predicate unknown exception");
                    #endif
                }

                if (!matched) return false;

                if (this->completed.exchange(true, std::memory_order_acq_rel)) return true;
            }

            auto alloc = boost::asio::get_associated_allocator(
                handler,
                boost::asio::recycling_allocator<void>{}
            );

            std::optional<T> copied_data;
            try {
                copied_data.emplace(data);
            } catch (...) {
                #ifdef DISCUSY_LOGGING
                detail::callback_log_noexcept("When data copy exception");
                #endif
                try {
                    boost::asio::dispatch(ex,
                        boost::asio::bind_allocator(alloc, [self = this->shared_from_this()]() mutable {
                            when_node::complete(std::move(self), boost::asio::error::make_error_code(boost::asio::error::no_memory));
                        })
                    );
                } catch (...) {
                    #ifdef DISCUSY_LOGGING
                    detail::callback_log_noexcept("When handler no_memory dispatch exception");
                    #endif
                }
                return true;
            }

            try {
                boost::asio::dispatch(ex,
                    boost::asio::bind_allocator(alloc, [self = this->shared_from_this(), d = std::move(*copied_data)]() mutable {
                        when_node::complete(std::move(self), boost::system::error_code{}, std::move(d));
                    })
                );
            } catch (...) {
                if (!this->complete_entered.load(std::memory_order_acquire)) {
                    #ifdef DISCUSY_LOGGING
                    detail::callback_log_noexcept("When handler dispatch exception");
                    #endif
                    try {
                        boost::asio::dispatch(ex,
                            boost::asio::bind_allocator(alloc, [self = this->shared_from_this()]() mutable {
                                when_node::complete(std::move(self), boost::asio::error::make_error_code(boost::asio::error::no_memory));
                            })
                        );
                    } catch (...) {} // NOLINT(bugprone-empty-catch)
                }
                #ifdef DISCUSY_LOGGING
                else {
                    detail::callback_log_noexcept("When completion handler threw exception");
                }
                #endif
            }
            return true;
        }

        void on_callback_destroyed(const ctx::io_context::executor_t& io_exec) noexcept override final {
            if (this->completed.exchange(true, std::memory_order_acq_rel)) return;

            auto alloc = boost::asio::get_associated_allocator(
                handler,
                boost::asio::recycling_allocator<void>{}
            );

            try {
                boost::asio::post(io_exec,
                    boost::asio::bind_allocator(alloc, [self = this->shared_from_this(), ex = this->ex, alloc]() mutable {
                        boost::asio::dispatch(ex,
                            boost::asio::bind_allocator(alloc, [self = std::move(self)]() mutable {
                                {
                                    std::scoped_lock lock{self->match_mtx};
                                }
                                when_node::complete(std::move(self), boost::asio::error::make_error_code(boost::asio::error::operation_aborted));
                            })
                        );
                    })
                );
            } catch (...) {
                #ifdef DISCUSY_LOGGING
                detail::callback_log_noexcept("When on_callback_destroyed post exception");
                #endif
            }
        }
    };

    struct initiate_when {
        using executor_type = ctx::io_context::executor_t;

        ctx::io_context::executor_t executor;

        // left out on purpose so users are explicit
        // [[nodiscard]] executor_type get_executor() const noexcept {
        //     return executor;
        // }

        template <discusy::asio::chf<T> Handler, typename Pred>
        void operator()(Handler&& handler, std::weak_ptr<detail::callback_core> weak_core, Pred&& p) const {
            using handler_t = std::decay_t<Handler>;

            auto alloc = boost::asio::get_associated_allocator(
                handler,
                boost::asio::recycling_allocator<void>{}
            );

            auto ex = boost::asio::get_associated_executor(handler, executor);
            using executor_type_h = decltype(ex);
            using pred_t = std::decay_t<Pred>;

            auto core = weak_core.lock();
            if (!core) {
                auto ex_imm = boost::asio::get_associated_immediate_executor(handler, executor);
                boost::asio::dispatch(
                    ex_imm,
                    boost::asio::append(std::forward<decltype(handler)>(handler), boost::asio::error::make_error_code(boost::asio::error::operation_aborted), T{})
                );
                return;
            }

            using node_t = when_node<pred_t, handler_t, executor_type_h>;

            auto node = std::allocate_shared<node_t>(
                alloc,
                std::forward<Pred>(p),
                std::forward<Handler>(handler),
                ex
            );

            auto slot = boost::asio::get_associated_cancellation_slot(node->handler);
            if (slot.is_connected()) {
                slot.assign([node_weak = std::weak_ptr<node_t>{node}, ex, alloc, weak_core](boost::asio::cancellation_type type) noexcept {
                    if (type == boost::asio::cancellation_type::none) return;
                    auto n = node_weak.lock();
                    if (!n) return;

                    if (n->completed.exchange(true, std::memory_order_acq_rel)) return;

                    try {
                        boost::asio::post(ex,
                            boost::asio::bind_allocator(alloc, [n = std::move(n), weak_core]() mutable {
                                {
                                    std::scoped_lock lock{n->match_mtx};
                                }
                                if (auto c = weak_core.lock()) {
                                    c->clear_completed_waiter(n);
                                }
                                node_t::complete(std::move(n), boost::asio::error::make_error_code(boost::asio::error::operation_aborted));
                            })
                        );
                    } catch (...) {
                        #ifdef DISCUSY_LOGGING
                        detail::callback_log_noexcept("When cancellation post exception");
                        #endif
                    }
                });
            }

            const auto push_res = core->push_when_waiter(node);
            if (push_res == detail::push_waiter_result::destroyed || push_res == detail::push_waiter_result::no_memory) {
                if (node->completed.exchange(true, std::memory_order_acq_rel)) return;

                auto h = std::move(node->handler);
                auto cancel_slot = boost::asio::get_associated_cancellation_slot(h);
                if (cancel_slot.is_connected()) cancel_slot.clear();
                auto ex_imm = boost::asio::get_associated_immediate_executor(h, executor);
                node->work.reset();
                node.reset();

                const auto ec = (push_res == detail::push_waiter_result::destroyed)
                    ? boost::asio::error::make_error_code(boost::asio::error::operation_aborted)
                    : boost::asio::error::make_error_code(boost::asio::error::no_memory);

                boost::asio::dispatch(
                    ex_imm,
                    boost::asio::append(std::move(h), ec, T{})
                );
            }
        }
    };

public:
    Callback(const Callback&) = delete;
    Callback& operator=(const Callback&) = delete;
    Callback(Callback&&) = delete;
    Callback& operator=(Callback&&) = delete;

    explicit Callback(ctx::io_context& ctx) : detail::callback_base{ctx} {}
    ~Callback() noexcept = default;

    using detail::callback_base::set_bot;
    using detail::callback_base::reserve_id;
    using detail::callback_base::unregister;
    using detail::callback_base::has_listeners;

    template <typename F>
    requires ( std::invocable<std::decay_t<F>&, const T&> )
    callback_id listen_with_id(const callback_id id, callback_priority prio, F&& cb, boost::asio::any_io_executor ex) {
        if (id == 0 || !core_) return 0;

        if constexpr (requires { static_cast<bool>(cb == nullptr); }) {
            if (cb == nullptr) return 0;
        }

        if (!ex) {
            ex = io_ctx_.executor_;
        }

        std::shared_ptr<detail::callback_interface> ptr;

        using DecayedF = std::decay_t<F>;
        using Result = std::invoke_result_t<DecayedF&, const T&>;
        if constexpr (!coro::IsAwaitable<Result>) {
            ptr = std::make_shared<sync_callback<DecayedF>>(std::forward<F>(cb), std::move(ex));
        } else {
            ptr = std::make_shared<async_callback<DecayedF>>(std::forward<F>(cb), std::move(ex));
        }

        return register_callback(id, prio, std::move(ptr));
    }

    template <typename F>
    requires ( std::invocable<std::decay_t<F>&, const T&> )
    callback_id listen_with_id(const callback_id id, F&& cb, boost::asio::any_io_executor ex) {
        return listen_with_id(id, callback_priority::user, std::forward<F>(cb), std::move(ex));
    }

    template <typename F, typename Executor>
    requires ( std::invocable<std::decay_t<F>&, const T&> && std::is_constructible_v<boost::asio::any_io_executor, std::decay_t<Executor>> && !std::is_same_v<std::decay_t<Executor>, boost::asio::any_io_executor> && !std::invocable<std::decay_t<Executor>&, const T&> )
    callback_id listen_with_id(const callback_id id, callback_priority prio, F&& cb, Executor&& ex) {
        return listen_with_id(id, prio, std::forward<F>(cb), boost::asio::any_io_executor(std::forward<Executor>(ex)));
    }

    template <typename F, typename Executor>
    requires ( std::invocable<std::decay_t<F>&, const T&> && std::is_constructible_v<boost::asio::any_io_executor, std::decay_t<Executor>> && !std::is_same_v<std::decay_t<Executor>, boost::asio::any_io_executor> && !std::invocable<std::decay_t<Executor>&, const T&> )
    callback_id listen_with_id(const callback_id id, F&& cb, Executor&& ex) {
        return listen_with_id(id, callback_priority::user, std::forward<F>(cb), std::forward<Executor>(ex));
    }

    template <typename Executor, typename F>
    requires ( std::invocable<std::decay_t<F>&, const T&> && std::is_constructible_v<boost::asio::any_io_executor, std::decay_t<Executor>> && !std::invocable<std::decay_t<Executor>&, const T&> )
    callback_id listen_with_id(const callback_id id, callback_priority prio, Executor&& ex, F&& cb) {
        return listen_with_id(id, prio, std::forward<F>(cb), boost::asio::any_io_executor(std::forward<Executor>(ex)));
    }

    template <typename Executor, typename F>
    requires ( std::invocable<std::decay_t<F>&, const T&> && std::is_constructible_v<boost::asio::any_io_executor, std::decay_t<Executor>> && !std::invocable<std::decay_t<Executor>&, const T&> )
    callback_id listen_with_id(const callback_id id, Executor&& ex, F&& cb) {
        return listen_with_id(id, callback_priority::user, std::forward<Executor>(ex), std::forward<F>(cb));
    }

    template <typename F>
    requires ( std::invocable<std::decay_t<F>&, const T&> )
    callback_id listen_with_id(const callback_id id, callback_priority prio, F&& cb) {
        auto ex = boost::asio::get_associated_executor(cb, io_ctx_.executor_);
        return listen_with_id(id, prio, std::forward<F>(cb), std::move(ex));
    }

    template <typename F>
    requires ( std::invocable<std::decay_t<F>&, const T&> )
    callback_id listen_with_id(const callback_id id, F&& cb) {
        return listen_with_id(id, callback_priority::user, std::forward<F>(cb));
    }

    template <typename Executor, typename F>
    requires ( std::invocable<std::decay_t<F>&, const T&> && std::is_constructible_v<boost::asio::any_io_executor, std::decay_t<Executor>> && !std::invocable<std::decay_t<Executor>&, const T&> )
    callback_id listen(callback_priority prio, Executor&& ex, F&& cb) {
        return listen_with_id(reserve_id(), prio, std::forward<Executor>(ex), std::forward<F>(cb));
    }

    template <typename Executor, typename F>
    requires ( std::invocable<std::decay_t<F>&, const T&> && std::is_constructible_v<boost::asio::any_io_executor, std::decay_t<Executor>> && !std::invocable<std::decay_t<Executor>&, const T&> )
    callback_id listen(Executor&& ex, F&& cb) {
        return listen_with_id(reserve_id(), callback_priority::user, std::forward<Executor>(ex), std::forward<F>(cb));
    }

    template <typename F, typename Executor>
    requires ( std::invocable<std::decay_t<F>&, const T&> && std::is_constructible_v<boost::asio::any_io_executor, std::decay_t<Executor>> && !std::invocable<std::decay_t<Executor>&, const T&> )
    callback_id listen(callback_priority prio, F&& cb, Executor&& ex) {
        return listen_with_id(reserve_id(), prio, std::forward<Executor>(ex), std::forward<F>(cb));
    }

    template <typename F, typename Executor>
    requires ( std::invocable<std::decay_t<F>&, const T&> && std::is_constructible_v<boost::asio::any_io_executor, std::decay_t<Executor>> && !std::invocable<std::decay_t<Executor>&, const T&> )
    callback_id listen(F&& cb, Executor&& ex) {
        return listen_with_id(reserve_id(), callback_priority::user, std::forward<Executor>(ex), std::forward<F>(cb));
    }

    template <typename F>
    requires ( std::invocable<std::decay_t<F>&, const T&> )
    callback_id listen(callback_priority prio, F&& cb) {
        return listen_with_id(reserve_id(), prio, std::forward<F>(cb));
    }

    template <typename F>
    requires ( std::invocable<std::decay_t<F>&, const T&> )
    callback_id listen(F&& cb) {
        return listen_with_id(reserve_id(), callback_priority::user, std::forward<F>(cb));
    }

    template <typename Executor, typename F>
    requires ( std::invocable<std::decay_t<F>&, const T&> && std::is_constructible_v<boost::asio::any_io_executor, std::decay_t<Executor>> && !std::invocable<std::decay_t<Executor>&, const T&> )
    callback_id listen_system(Executor&& ex, F&& cb) {
        return listen(callback_priority::system, std::forward<Executor>(ex), std::forward<F>(cb));
    }

    template <typename F, typename Executor>
    requires ( std::invocable<std::decay_t<F>&, const T&> && std::is_constructible_v<boost::asio::any_io_executor, std::decay_t<Executor>> && !std::invocable<std::decay_t<Executor>&, const T&> )
    callback_id listen_system(F&& cb, Executor&& ex) {
        return listen(callback_priority::system, std::forward<F>(cb), std::forward<Executor>(ex));
    }

    template <typename F>
    requires ( std::invocable<std::decay_t<F>&, const T&> )
    callback_id listen_system(F&& cb) {
        return listen(callback_priority::system, std::forward<F>(cb));
    }

    template <typename Executor, typename F>
    requires ( std::invocable<std::decay_t<F>&, const T&> && std::is_constructible_v<boost::asio::any_io_executor, std::decay_t<Executor>> && !std::invocable<std::decay_t<Executor>&, const T&> )
    callback_id operator()(Executor&& ex, F&& cb) {
        return listen(std::forward<Executor>(ex), std::forward<F>(cb));
    }

    template <typename F, typename Executor>
    requires ( std::invocable<std::decay_t<F>&, const T&> && std::is_constructible_v<boost::asio::any_io_executor, std::decay_t<Executor>> && !std::invocable<std::decay_t<Executor>&, const T&> )
    callback_id operator()(F&& cb, Executor&& ex) {
        return listen(std::forward<F>(cb), std::forward<Executor>(ex));
    }

    template <typename F>
    requires ( std::invocable<std::decay_t<F>&, const T&> )
    callback_id operator()(F&& cb) {
        return listen(std::forward<F>(cb));
    }

    void fire(T&& data) {
        if (!core_) return;
        const auto active_c = core_->active_count.load(std::memory_order_acquire);
        const auto waiters_c = core_->when_waiters_count.load(std::memory_order_acquire);
        if (active_c == 0 && waiters_c == 0) return;

        auto snap = active_c != 0 ? core_->get_snapshot() : nullptr;
        if ((!snap || snap->empty()) && core_->when_waiters_count.load(std::memory_order_acquire) == 0) return;

        attach_bot(data, bot_ptr_.load(std::memory_order_relaxed));

        try {
            if (!snap || snap->async_count == 0) {
                io_ctx_.submit([core = core_, snap = std::move(snap), data = std::move(data)]() mutable {
                    detail::callback_base::fire_sync_internal(std::move(core), std::move(snap), std::addressof(data));
                });
            } else {
                auto holder = std::make_unique<T>(std::move(data));
                T* raw = holder.release();
                launch_async_erased(
                    core_,
                    std::move(snap),
                    detail::erased_event_data{
                        raw,
                        raw,
                        [](void* ptr) noexcept { delete static_cast<T*>(ptr); }
                    }
                );
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
    void fire_payload(std::string_view payload) {
        fire_payload_erased(payload, &dispatch_payload_sync<W>, &parse_payload_boxed<W>);
    }

    template <typename W = T>
    void fire_json(std::string_view json) {
        fire_payload<W>(json);
    }

    // makes a copy of the data, unideal
    // if you want to use it with awaitable operators, pass in boost::asio::use_awaitable
    template <typename Predicate, discusy::asio::ctf<T> CompletionToken = ctx::io_context::dct_t>
    requires ( std::invocable<std::decay_t<Predicate>&, const T&> && std::is_same_v<std::invoke_result_t<std::decay_t<Predicate>&, const T&>, bool> )
    auto when(Predicate&& pred, CompletionToken&& token = ctx::io_context::dct_t()) {
        return boost::asio::async_initiate<CompletionToken, void(boost::system::error_code, T)>(
            initiate_when{io_ctx_.executor_},
            token,
            std::weak_ptr<detail::callback_core>(core_),
            std::forward<Predicate>(pred)
        );
    }
};

template <std::integral N>
requires (!std::same_as<N, bool>)
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

    CounterCallback(const CounterCallback&) = delete;
    CounterCallback& operator=(const CounterCallback&) = delete;
    CounterCallback(CounterCallback&&) = delete;
    CounterCallback& operator=(CounterCallback&&) = delete;

    void arrive() {
        N cur = counter_.load(std::memory_order_acquire);
        while (cur > 0) {
            if (counter_.compare_exchange_weak(cur, static_cast<N>(cur - 1),
                                               std::memory_order_acq_rel,
                                               std::memory_order_acquire)) {
                if (cur == 1 && callback_) {
                    auto cb = std::move(callback_);
                    std::invoke(cb);
                }
                return;
            }
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
