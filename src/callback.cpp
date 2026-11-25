#include <discusy/callback.hpp>

namespace discusy::detail {

namespace {

[[nodiscard]] bool same_executor(
    const boost::asio::any_io_executor& target_ex,
    const boost::asio::any_io_executor& current_ex,
    const boost::asio::any_io_executor& core_exec
) noexcept {
    return target_ex == current_ex || target_ex == core_exec;
}

auto invoke_sync_on_executor(
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

} // namespace

push_waiter_result callback_core::push_when_waiter(std::shared_ptr<when_interface> node) noexcept {
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

boost::container::small_vector<std::shared_ptr<when_interface>, 8> callback_core::drain_incoming_when_waiters() noexcept {
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

void callback_core::dispatch_when_waiters(const void* data) {
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

void callback_core::clear_completed_waiter(const std::shared_ptr<when_interface>& node) noexcept {
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

void callback_core::abort_all_when_waiters() noexcept {
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

std::shared_ptr<const callback_snapshot> callback_core::rebuild_snapshot() {
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

bool callback_core::unregister(const callback_id id) {
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

callback_base::callback_base(ctx::io_context& ctx)
    : core_{std::make_shared<callback_core>(ctx.executor_)}, io_ctx_{ctx} {}

callback_base::~callback_base() noexcept {
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

void callback_base::fire_sync_internal(
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

coro::awaitable<void> callback_base::fire_async_internal(
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

void callback_base::launch_async_erased(
    std::shared_ptr<callback_core> core,
    std::shared_ptr<const callback_snapshot> snap,
    erased_event_data data
) {
    io_ctx_.co_launch_detached([core = std::move(core), snap = std::move(snap), data = std::move(data)]() mutable {
        return fire_async_internal(std::move(core), std::move(snap), std::move(data));
    });
}

void callback_base::fire_payload_erased(
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

void callback_base::fire_registered_payload(std::string_view payload) {
    if (!core_) return;
    const auto active_c = core_->active_count.load(std::memory_order_acquire);
    const auto waiters_c = core_->when_waiters_count.load(std::memory_order_acquire);
    if (active_c == 0 && waiters_c == 0) return;

    const auto sync_fn = core_->payload_sync.load(std::memory_order_acquire);
    const auto parse_fn = core_->payload_parse.load(std::memory_order_acquire);
    if (sync_fn == nullptr || parse_fn == nullptr) return;

    fire_payload_erased(payload, sync_fn, parse_fn);
}

callback_id callback_base::register_callback(
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

} // namespace discusy::detail
