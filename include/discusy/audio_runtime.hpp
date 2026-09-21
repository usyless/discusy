#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include <boost/asio.hpp>

#ifdef __linux__
#include <sys/prctl.h>
#endif

#include "log.hpp" // IWYU pragma: keep

namespace discusy::ctx {

namespace detail {

[[nodiscard]] inline std::atomic<unsigned>& requested_audio_threads() noexcept {
    static std::atomic<unsigned> n{0};
    return n;
}

[[nodiscard]] inline unsigned default_audio_threads() noexcept {
    const auto hw = std::thread::hardware_concurrency();
    if (hw == 0) return 1;
    return std::clamp(hw / 4U, 1U, 4U);
}

}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
class audio_runtime {
public:
    using strand_t = boost::asio::strand<boost::asio::io_context::executor_type>;

    [[nodiscard]] static audio_runtime& instance() {
        static audio_runtime rt;
        return rt;
    }

    static void configure(const unsigned threads) noexcept {
        detail::requested_audio_threads().store(std::max(1U, threads), std::memory_order_relaxed);
    }

    [[nodiscard]] auto make_strand() {
        const auto i = next_.fetch_add(1, std::memory_order_relaxed) % nodes_.size();
        return boost::asio::make_strand(nodes_[i]->io_ctx);
    }

    [[nodiscard]] std::size_t thread_count() const noexcept { return nodes_.size(); }

    audio_runtime(const audio_runtime&) = delete;
    audio_runtime& operator=(const audio_runtime&) = delete;

private:
    struct node {
        boost::asio::io_context io_ctx{1};
        boost::asio::executor_work_guard<boost::asio::io_context::executor_type> guard{boost::asio::make_work_guard(io_ctx)};
        std::thread thread;
    };

    audio_runtime() {
        const auto requested = detail::requested_audio_threads().load(std::memory_order_relaxed);
        const auto count = (requested != static_cast<std::remove_cvref_t<decltype(requested)>>(0)) ? requested : detail::default_audio_threads();

        nodes_.reserve(count);
        for (unsigned i = 0; i < count; ++i) {
            auto& n = *nodes_.emplace_back(std::make_unique<node>());
            n.thread = std::thread{[&n]() {
                #ifdef __linux__
                ::prctl(PR_SET_TIMERSLACK, 1UL, 0, 0, 0);
                #endif
                while (true) {
                    try {
                        n.io_ctx.run();
                        break;
                    }
                    #ifdef DISCUSY_LOGGING
                    catch (const std::exception& e) {
                        log::Logger{}("[discusy audio runtime] worker error: {}", e.what());
                        if (n.io_ctx.stopped()) break;
                        std::this_thread::sleep_for(std::chrono::milliseconds{50});
                        n.io_ctx.restart();
                    }
                    #endif
                    catch (...) {
                        #ifdef DISCUSY_LOGGING
                        log::Logger{}("[discusy audio runtime] unknown worker error");
                        #endif
                        if (n.io_ctx.stopped()) break;
                        std::this_thread::sleep_for(std::chrono::milliseconds{50});
                        n.io_ctx.restart();
                    }

                    if (n.io_ctx.stopped()) break;
                }
            }};
        }

        #ifdef DISCUSY_LOGGING
        log::Logger{}("[discusy audio runtime] started with {} pacing thread(s)", count);
        #endif
    }

    ~audio_runtime() {
        for (auto& n : nodes_) {
            n->guard.reset();
            n->io_ctx.stop();
        }
        for (auto& n : nodes_) {
            if (n->thread.joinable()) n->thread.join();
        }
    }

    std::vector<std::unique_ptr<node>> nodes_;
    std::atomic<std::size_t> next_{0};
};

}
