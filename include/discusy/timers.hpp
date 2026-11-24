#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include <boost/asio.hpp>
#include <boost/unordered/concurrent_flat_map.hpp>

#include "types.hpp"
#include "io_context.hpp"
#include "asio_helpers.hpp"
#include "log.hpp" // IWYU pragma: keep

namespace discusy {

class Timers {
public:
    explicit Timers(ctx::io_context& ctx) : io_ctx_{ctx}, state_{std::make_shared<timers_state>()} {}
    Timers(const Timers& other) = delete;
    Timers& operator=(const Timers& other) = delete;
    Timers(Timers&& other) = delete;
    Timers& operator=(Timers&& other) = delete;

    /**
    * Starts an interval that schedules and executes a function on every tick.
    * Returns a tracking ID that can be stopped normally via stop_interval(id).
    */
    template <typename F, typename Rep, typename Period>
    requires (std::invocable<F&, timer>)
    std::uint64_t start_interval(F&& f, const std::chrono::duration<Rep, Period> interval) {
        const auto id = next_id();
        const auto first_deadline = std::chrono::steady_clock::now() + interval;

        auto entry = std::make_shared<timer_entry>(io_ctx_.make_strand());
        state_->timers.emplace(id, entry);

        if constexpr (coro::IsAwaitable<std::invoke_result_t<F&, discusy::timer>>) {
            ctx::io_context::co_launch_detached<ctx::launch::fresh>(
                [id, entry, interval, first_deadline, f = std::forward<F>(f)] mutable -> coro::awaitable<void> {
                    if (entry->cancelled.load(std::memory_order_acquire)) co_return;

                    entry->timer.expires_at(first_deadline);
                    while (!entry->cancelled.load(std::memory_order_acquire)) {
                        auto [ec] = co_await entry->timer.async_wait(boost::asio::as_tuple(boost::asio::deferred));
                        if (ec || entry->cancelled.load(std::memory_order_acquire)) co_return;

                        try {
                            co_await std::invoke(f, id);
                        }
                        #ifdef DISCUSY_LOGGING
                        catch (const std::exception& e) {
                            log::Logger{}("start_interval coroutine exception: {}", e.what());
                        }
                        #endif
                        catch (...) { // NOLINT(bugprone-empty-catch)
                            #ifdef DISCUSY_LOGGING
                            log::Logger{}("start_interval coroutine unknown exception");
                            #endif
                        }

                        if (entry->cancelled.load(std::memory_order_acquire)) co_return;

                        const auto now = std::chrono::steady_clock::now();
                        const auto next = entry->timer.expiry() + interval;
                        entry->timer.expires_at(next > now ? next : now + interval);
                    }
                },
                entry->strand
            );
        } else {
            const auto strand = entry->strand;
            boost::asio::dispatch(
                strand,
                [interval, first_deadline, f = std::forward<F>(f), id, entry = std::move(entry)] mutable -> void {
                    if (entry->cancelled.load(std::memory_order_acquire)) return;

                    auto& t = entry->timer;
                    t.expires_at(first_deadline);
                    t.async_wait([interval, f = std::move(f), id, entry = std::move(entry)](this auto&& self, asio::ec_t ec) -> void {
                        if (ec || entry->cancelled.load(std::memory_order_acquire)) return;

                        try {
                            std::invoke(f, id);
                        }
                        #ifdef DISCUSY_LOGGING
                        catch (const std::exception& e) {
                            log::Logger{}("start_interval callback exception: {}", e.what());
                        }
                        #endif
                        catch (...) { // NOLINT(bugprone-empty-catch)
                            #ifdef DISCUSY_LOGGING
                            log::Logger{}("start_interval callback unknown exception");
                            #endif
                        }

                        if (entry->cancelled.load(std::memory_order_acquire)) return;

                        const auto now = std::chrono::steady_clock::now();
                        const auto next = entry->timer.expiry() + interval;
                        entry->timer.expires_at(next > now ? next : now + interval);
                        entry->timer.async_wait(std::forward<decltype(self)>(self));
                    });
                }
            );
        }

        return id;
    }

    /**
    * Starts a single-shot timer that schedules and executes a function when it fires.
    * Returns a tracking ID that can be stopped normally via stop_timer(id).
    */
    template <typename F, typename Rep, typename Period>
    requires (std::invocable<F&, timer>)
    std::uint64_t start_timer(F&& f, const std::chrono::duration<Rep, Period> time) {
        const auto id = next_id();
        const auto deadline = std::chrono::steady_clock::now() + time;

        auto entry = std::make_shared<timer_entry>(io_ctx_.make_strand());
        state_->timers.emplace(id, entry);

        const auto strand = entry->strand;
        std::weak_ptr<timers_state> weak_state{state_};
        boost::asio::dispatch(
            strand,
            [deadline, f = std::forward<F>(f), id, entry = std::move(entry), weak_state = std::move(weak_state)] mutable -> void {
                if (entry->cancelled.load(std::memory_order_acquire)) return;

                auto& t = entry->timer;
                t.expires_at(deadline);
                t.async_wait([f = std::move(f), id, entry = std::move(entry), weak_state = std::move(weak_state)](asio::ec_t ec) mutable -> void {
                    if (ec || entry->cancelled.load(std::memory_order_acquire)) return;

                    if (const auto st = weak_state.lock()) {
                        st->timers.erase(id);
                    }

                    try {
                        if constexpr (coro::IsAwaitable<std::invoke_result_t<F&, discusy::timer>>) {
                            ctx::io_context::co_launch_detached<ctx::launch::inline_if_on_executor>(
                                [f = std::move(f), id] mutable -> coro::awaitable<void> {
                                    try {
                                        co_await std::invoke(f, id);
                                    }
                                    #ifdef DISCUSY_LOGGING
                                    catch (const std::exception& e) {
                                        log::Logger{}("start_timer callback exception: {}", e.what());
                                    }
                                    #endif
                                    catch (...) { // NOLINT(bugprone-empty-catch)
                                        #ifdef DISCUSY_LOGGING
                                        log::Logger{}("start_timer callback unknown exception");
                                        #endif
                                    }
                                },
                                entry->strand
                            );
                        } else {
                            std::invoke(f, id);
                        }
                    }
                    #ifdef DISCUSY_LOGGING
                    catch (const std::exception& e) {
                        log::Logger{}("start_timer callback exception: {}", e.what());
                    }
                    #endif
                    catch (...) { // NOLINT(bugprone-empty-catch)
                        #ifdef DISCUSY_LOGGING
                        log::Logger{}("start_timer callback unknown exception");
                        #endif
                    }
                });
            }
        );

        return id;
    }

    bool stop_interval(const timer id) {
        if (id == 0) return false;

        std::shared_ptr<timer_entry> entry;
        const bool erased = state_->timers.erase_if(id, [&entry](auto& val) {
            entry = std::move(val.second);
            return true;
        }) != 0UZ;

        if (!erased || !entry) return false;

        entry->cancelled.store(true, std::memory_order_release);
        const auto strand = entry->strand;
        boost::asio::dispatch(strand, [entry = std::move(entry)] mutable {
            entry->timer.cancel();
        });
        return true;
    }

    bool stop_timer(const timer id) {
        return stop_interval(id);
    }

    void clear() {
        std::vector<std::shared_ptr<timer_entry>> dying;
        dying.reserve(state_->timers.size());
        state_->timers.erase_if([&dying](auto& val) {
            dying.emplace_back(std::move(val.second));
            return true;
        });

        for (auto& entry : dying) {
            if (!entry) continue;
            entry->cancelled.store(true, std::memory_order_release);
            const auto strand = entry->strand;
            boost::asio::dispatch(strand, [entry = std::move(entry)] mutable {
                entry->timer.cancel();
            });
        }
    }

    ~Timers() noexcept {
        try {
            clear();
        } catch (...) { // NOLINT(bugprone-empty-catch)
        }
    }

private:
    struct timer_entry {
        ctx::io_context::strand_t strand;
        ctx::io_context::strand_timer_t timer;
        std::atomic<bool> cancelled{false};

        explicit timer_entry(ctx::io_context::strand_t s)
            : strand{std::move(s)}, timer{strand} {}
    };

    struct timers_state {
        boost::unordered::concurrent_flat_map<timer, std::shared_ptr<timer_entry>> timers;
    };

    [[nodiscard]] std::uint64_t next_id() noexcept {
        return timers_idx_.fetch_add(1, std::memory_order_relaxed) + 1;
    }

    ctx::io_context& io_ctx_;
    std::atomic<timer> timers_idx_{0};
    std::shared_ptr<timers_state> state_;
};

}
