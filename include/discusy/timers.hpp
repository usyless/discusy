#pragma once

#include <memory>
#include <optional>
#include <vector>

#include <boost/asio.hpp>
#include <boost/unordered/concurrent_flat_map.hpp>

#include "types.hpp"
#include "io_context.hpp"
#include "asio_helpers.hpp"

namespace discusy {

class Timers {
public:
    explicit Timers(ctx::io_context& ctx) noexcept : io_ctx_{ctx} {}
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

        t tmr{io_ctx_.make_strand()};
        auto timer = tmr.timer_;
        auto strand = tmr.strand_;

        timers_.emplace(id, std::move(tmr));

        if constexpr (coro::IsAwaitable<std::invoke_result_t<F&, discusy::timer>>) {
            ctx::io_context::co_launch_detached<ctx::launch::fresh>(
                [id, timer, interval, f = std::forward<F>(f)]() -> coro::awaitable<void> {
                    timer->expires_after(interval);
                    while (true) {
                        auto [ec] = co_await timer->async_wait(boost::asio::as_tuple(boost::asio::deferred));
                        if (ec) co_return;

                        timer->expires_at(timer->expiry() + interval);

                        try {
                            co_await std::invoke(f, id);
                        }
                        #ifdef DISCUSY_LOGGING
                        catch (const std::exception& e) {
                            log::Logger{}("start_interval coroutine exception: {}", e.what());
                        }
                        catch (...) {
                            log::Logger{}("start_interval coroutine unknown exception");
                        }
                        #endif
                    }
                },
                strand
            );
        } else {
            timer->expires_after(interval);

            timer->async_wait([interval, f = std::forward<F>(f), id, timer](this auto& self, asio::ec_t ec) -> void {
                if (ec) return;

                try {
                    std::invoke(f, id);
                }
                #ifdef DISCUSY_LOGGING
                catch (const std::exception& e) {
                    log::Logger{}("start_interval callback exception: {}", e.what());
                }
                catch (...) {
                    log::Logger{}("start_interval callback unknown exception");
                }
                #endif

                auto t = timer;
                t->expires_at(t->expiry() + interval);
                t->async_wait(std::move(self));
            });
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

        t tmr{io_ctx_.make_strand()};
        std::shared_ptr<ctx::io_context::strand_timer_t> timer{tmr.timer_};
        timer->expires_after(time);

        timers_.emplace(id, std::move(tmr));
        
        timer->async_wait([this, f = std::forward<F>(f), id, timer](asio::ec_t ec) mutable -> void {
            if (ec) return;
            timers_.erase(id);

            try {
                if constexpr (coro::IsAwaitable<std::invoke_result_t<F&, discusy::timer>>) {
                    io_ctx_.co_launch_detached<ctx::launch::inline_if_on_executor>(
                        std::invoke(std::move(f), id),
                        timer->get_executor()
                    );
                } else {
                    std::invoke(std::move(f), id);
                }
            }
            #ifdef DISCUSY_LOGGING
            catch (const std::exception& e) {
                log::Logger{}("start_timer callback exception: {}", e.what());
            }
            catch (...) {
                log::Logger{}("start_timer callback unknown exception");
            }
            #endif
        });
        return id;
    }

    bool stop_interval(const timer id) {
        if (id == 0) return false;

        std::optional<t> entry;
        const bool erased = timers_.erase_if(id, [&entry](auto& val) {
            entry.emplace(std::move(val.second));
            return true;
        }) != 0UZ;

        if (!erased || !entry) return false;

        if (entry->timer_) {
            boost::asio::dispatch(entry->strand_, [timer = std::move(entry->timer_)]() mutable {
                timer->cancel();
            });
        }
        return true;
    }

    bool stop_timer(const timer id) {
        return stop_interval(id);
    }

    void clear() {
        std::vector<t> dying;
        dying.reserve(timers_.size());
        timers_.erase_if([&dying](auto& val) {
            dying.emplace_back(std::move(val.second));
            return true;
        });

        for (auto& entry : dying) {
            auto strand = entry.strand_;
            boost::asio::dispatch(strand, [timer = std::move(entry.timer_)]() mutable {
                if (timer) timer->cancel();
            });
        }
    }

    ~Timers() {
        clear();
    }
private:
    [[nodiscard]] std::uint64_t next_id() noexcept {
        return ++timers_idx_;
    }

    ctx::io_context& io_ctx_;
    std::atomic<timer> timers_idx_{0};

    struct t {
        std::shared_ptr<ctx::io_context::strand_timer_t> timer_;
        ctx::io_context::strand_t strand_;

        t(const ctx::io_context::strand_t& strand) : timer_{std::make_shared<ctx::io_context::strand_timer_t>(strand)}, strand_{strand} {}
    };
    boost::unordered::concurrent_flat_map<timer, t> timers_;
};

}