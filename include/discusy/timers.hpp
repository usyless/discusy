#pragma once

#include <memory>

#include <boost/asio.hpp>

#include "types.hpp"
#include "io_context.hpp"
#include "asio_helpers.hpp"

namespace discusy {

// unsure of whether to use boost::asio::post or boost::asio::dispatch here
// validate use of this captures, since it lives with the bot object, the io context will be stopped if anything clearing this?
// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
class Timers {
public:
    explicit Timers(ctx::io_context& ctx) noexcept : io_ctx_{ctx} {}

    /**
    * Starts an interval that schedules and executes a function on every tick.
    * Returns a tracking ID that can be stopped normally via stop_interval(id).
    */
    template <typename F, typename Rep, typename Period>
    requires (std::invocable<F&, timer>)
    std::uint64_t start_interval(F&& f, const std::chrono::duration<Rep, Period> interval) {
        const auto id = next_id();

        t tmr{io_ctx_.make_strand()};
        std::shared_ptr<ctx::io_context::strand_timer_t> timer{tmr.timer_};
        timer->expires_after(interval);

        {
        std::scoped_lock lock{timers_mtx_};
        timers_.emplace(id, std::move(tmr));
        }

        timer->async_wait([i = this, interval, func = std::make_shared<std::decay_t<F>>(std::forward<F>(f)), id, timer](this auto&& self, asio::ec_t ec) -> void {
            if (ec) {
                std::scoped_lock lock{i->timers_mtx_};
                i->timers_.erase(id);
                return;
            }

            i->io_ctx_.handle_callback_coro_normal(*func, id);

            timer->expires_at(timer->expiry() + interval);
            timer->async_wait(self);
        });
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

        {
        std::scoped_lock lock{timers_mtx_};
        timers_.emplace(id, std::move(tmr));
        }
        
        timer->async_wait([this, f = std::forward<F>(f), id, timer](asio::ec_t ec) mutable -> void {
            if (!ec) io_ctx_.handle_callback_coro_normal(std::move(f), id);

            {
            std::scoped_lock lock{timers_mtx_};
            timers_.erase(id);
            }
        });
        return id;
    }

    bool stop_interval(const timer id) {
        if (id == 0) return false;

        std::unordered_map<timer, t>::node_type handle;

        {
        std::scoped_lock lock{timers_mtx_};
        auto it = timers_.find(id);
        if (it == timers_.end()) return false;

        handle = timers_.extract(it);
        }

        auto strand = handle.mapped().strand_;
        boost::asio::dispatch(strand, [handle = std::move(handle)]() mutable {
            handle.mapped().timer_->cancel();
        });
        return true;
    }

    bool stop_timer(const timer id) {
        return stop_interval(id);
    }

    void clear() {
        std::unordered_map<timer, t> dying;
        {
        std::scoped_lock lock{timers_mtx_};
        dying = std::move(timers_);
        timers_.clear();
        }

        for (auto& [_, entry] : dying) {
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

    std::mutex timers_mtx_;

    struct t {
        std::shared_ptr<ctx::io_context::strand_timer_t> timer_;
        const ctx::io_context::strand_t strand_;

        t(const ctx::io_context::strand_t& strand) : timer_{std::make_shared<ctx::io_context::strand_timer_t>(strand)}, strand_{strand} {}
    };
    std::unordered_map<timer, t> timers_;
};

}