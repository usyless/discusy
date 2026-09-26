#pragma once

#include <chrono>
#include <atomic>
#include <string>
#include <string_view>

#include <boost/unordered/concurrent_flat_map.hpp>

#include <usylibpp/strings.hpp>

#include "json.hpp"
#include "log.hpp" // IWYU pragma: keep

namespace discusy {

struct rate_limited {
    // std::string message{};
    float retry_after{}; // seconds
    bool global{};
};

class rate_limiter {
    struct bucket_state {
        int remaining = 1;
        std::chrono::steady_clock::time_point reset_at{};
    };

    struct transparent_string_hash {
        using is_transparent = void;
        [[nodiscard]] size_t operator()(std::string_view sv) const noexcept { return std::hash<std::string_view>{}(sv); }
    };

    boost::unordered::concurrent_flat_map<std::string, bucket_state, transparent_string_hash, std::equal_to<>> buckets_;
    boost::unordered::concurrent_flat_map<std::string, std::string, transparent_string_hash, std::equal_to<>> route_to_bucket_;
    std::atomic<std::chrono::steady_clock::time_point> global_reset_at_{};

    std::atomic<std::chrono::steady_clock::time_point> last_cleanup_{std::chrono::steady_clock::now()};

    void perform_cleanup(std::chrono::steady_clock::time_point now) {
        auto last = last_cleanup_.load(std::memory_order_relaxed);
        if (now - last < std::chrono::minutes{5}) return;
        if (!last_cleanup_.compare_exchange_strong(last, now, std::memory_order_relaxed)) return;

        buckets_.erase_if([now](const auto& pair) {
            return pair.second.reset_at < now;
        });

        route_to_bucket_.erase_if([this](const auto& pair) {
            return !buckets_.contains(pair.second);
        });
        
        #ifdef DISCUSY_LOGGING
        log::Logger{}("Rate limit maps purged. Routes remaining: {}", route_to_bucket_.size());
        #endif
    }

public:
    [[nodiscard]] std::chrono::milliseconds get_delay(const std::string& route_key) {
        const auto now = std::chrono::steady_clock::now();
        perform_cleanup(now);

        const auto global_reset = global_reset_at_.load(std::memory_order_relaxed);
        if (global_reset > now) {
            return std::chrono::duration_cast<std::chrono::milliseconds>(global_reset - now);
        }

        std::string bucket;
        bool has_bucket = false;
        route_to_bucket_.cvisit(route_key, [&](const auto& pair) {
            bucket = pair.second;
            has_bucket = true;
        });

        if (has_bucket) {
            std::chrono::milliseconds delay{0};
            buckets_.cvisit(bucket, [&](const auto& pair) {
                const auto& state = pair.second;
                if (state.remaining <= 0 && state.reset_at > now) {
                    delay = std::chrono::duration_cast<std::chrono::milliseconds>(state.reset_at - now);
                }
            });
            return delay;
        }

        return std::chrono::milliseconds{0};
    }

    void update(const std::string& route_key, const auto& headers) {
        auto bucket = headers["x-ratelimit-bucket"];
        auto remaining = headers["x-ratelimit-remaining"];
        auto reset_after = headers["x-ratelimit-reset-after"];

        if (!bucket.empty() && !remaining.empty() && !reset_after.empty()) {
            const auto remain = ulp::str::to_number<int>(remaining);
            if (!remain) return;
            const auto sec = ulp::str::to_number<double>(reset_after);
            if (!sec) return;

            const auto now = std::chrono::steady_clock::now();
            perform_cleanup(now);

            route_to_bucket_.insert_or_assign(route_key, std::string(bucket));
            buckets_.insert_or_assign(std::string(bucket), bucket_state{
                .remaining = *remain,
                .reset_at = now + std::chrono::milliseconds(static_cast<int>(*sec * 1000)),
            });
        }
    }

    std::chrono::milliseconds handle_429(std::string& json) {
        rate_limited data{};

        // unsure if minified
        if (json::parse_json<json::glz_opts_non_minified>(data, json)) {
            return std::chrono::milliseconds{1000};
        }

        auto retry_sec = data.retry_after;
        #ifdef DISCUSY_LOGGING
        log::Logger{}("Rate limited for: {}s", retry_sec);
        #endif

        auto delay = std::chrono::milliseconds{static_cast<int>(retry_sec * 1000)};
        if (delay.count() == 0) delay = std::chrono::milliseconds{1000}; // just in case
        
        if (data.global) {
            global_reset_at_.store(std::chrono::steady_clock::now() + delay, std::memory_order_relaxed);
        }

        return delay;
    }
};

}