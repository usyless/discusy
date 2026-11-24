#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

#include <boost/unordered/concurrent_flat_map.hpp>

#include <usylibpp/strings.hpp>

#include "json.hpp"
#include "log.hpp" // IWYU pragma: keep

namespace discusy {

struct rate_limited {
    // std::string message{};
    double retry_after{-1.0}; // seconds
    bool global{false};
};

class rate_limiter {
    static constexpr double MAX_DELAY_SECONDS = 86400.0; // 24 hours cap to prevent overflow / UB
    static constexpr std::chrono::milliseconds DEFAULT_429_DELAY{1000};
    static constexpr std::chrono::minutes CLEANUP_INTERVAL{5};
    static constexpr std::chrono::minutes BUCKET_EXPIRE_GRACE{5};

    static constexpr json::no_utf8_validation glz_opts_429{
        glz::opts{
            .null_terminated = false,
            .error_on_unknown_keys = false,
            .minified = false,
        },
    };

    struct bucket_state {
        int remaining{1};
        std::chrono::steady_clock::time_point reset_at{};
    };

    struct transparent_string_hash {
        using is_transparent = void;
        [[nodiscard]] std::size_t operator()(std::string_view sv) const noexcept {
            return std::hash<std::string_view>{}(sv);
        }
    };

    // Lock ordering invariant: never acquire route_to_bucket_ while holding a buckets_ segment lock.
    // route_to_bucket_ -> buckets_ is used by both get_delay() and perform_cleanup().
    boost::unordered::concurrent_flat_map<std::string, bucket_state, transparent_string_hash, std::equal_to<>> buckets_;
    boost::unordered::concurrent_flat_map<std::string, std::string, transparent_string_hash, std::equal_to<>> route_to_bucket_;
    std::atomic<std::chrono::steady_clock::time_point> global_reset_at_{};

    std::atomic<std::chrono::steady_clock::time_point> last_cleanup_{std::chrono::steady_clock::now()};

    [[nodiscard]] static std::chrono::milliseconds seconds_to_ms(const double seconds) noexcept {
        if (!std::isfinite(seconds) || seconds <= 0.0) {
            return std::chrono::milliseconds{0};
        }
        const double clamped = std::min(seconds, MAX_DELAY_SECONDS);
        return std::chrono::milliseconds{static_cast<std::int64_t>(std::ceil(clamped * 1000.0))};
    }

    void update_global_reset_at(const std::chrono::steady_clock::time_point candidate) noexcept {
        auto current = global_reset_at_.load(std::memory_order_relaxed);
        while (candidate > current &&
               !global_reset_at_.compare_exchange_weak(current, candidate, std::memory_order_relaxed)) {
        }
    }

    void perform_cleanup(const std::chrono::steady_clock::time_point now) {
        auto last = last_cleanup_.load(std::memory_order_relaxed);
        if (now - last < CLEANUP_INTERVAL) return;
        if (!last_cleanup_.compare_exchange_strong(last, now, std::memory_order_relaxed)) return;

        buckets_.erase_if([now](const auto& pair) {
            return pair.second.reset_at < now && (now - pair.second.reset_at) >= BUCKET_EXPIRE_GRACE;
        });

        route_to_bucket_.erase_if([this](const auto& pair) {
            return !buckets_.contains(pair.second);
        });

        #ifdef DISCUSY_LOGGING
        log::Logger{}("Rate limit maps purged. Routes remaining: {}", route_to_bucket_.size());
        #endif
    }

public:
    [[nodiscard]] std::chrono::milliseconds get_delay(const std::string_view route_key) {
        const auto now = std::chrono::steady_clock::now();
        perform_cleanup(now);

        std::chrono::milliseconds delay{0};

        const auto global_reset = global_reset_at_.load(std::memory_order_relaxed);
        if (global_reset > now) {
            delay = std::chrono::ceil<std::chrono::milliseconds>(global_reset - now);
        }

        route_to_bucket_.cvisit(route_key, [&](const auto& route_pair) {
            buckets_.cvisit(route_pair.second, [&](const auto& bucket_pair) {
                const auto& state = bucket_pair.second;
                if (state.remaining <= 0 && state.reset_at > now) {
                    const auto bucket_delay = std::chrono::ceil<std::chrono::milliseconds>(state.reset_at - now);
                    if (bucket_delay > delay) {
                        delay = bucket_delay;
                    }
                }
            });
        });

        return delay;
    }

    void update(const std::string_view route_key, const auto& headers) {
        const auto bucket_raw = headers["x-ratelimit-bucket"];
        const auto remaining_raw = headers["x-ratelimit-remaining"];
        const auto reset_after_raw = headers["x-ratelimit-reset-after"];

        const std::string_view bucket{bucket_raw.data(), bucket_raw.size()};
        const std::string_view remaining{remaining_raw.data(), remaining_raw.size()};
        const std::string_view reset_after{reset_after_raw.data(), reset_after_raw.size()};

        if (bucket.empty() || remaining.empty() || reset_after.empty()) {
            return;
        }

        const auto remain = ulp::str::to_number<int>(remaining);
        if (!remain) return;
        const auto sec = ulp::str::to_number<double>(reset_after);
        if (!sec || !std::isfinite(*sec)) return;

        const auto now = std::chrono::steady_clock::now();
        perform_cleanup(now);

        const bucket_state new_state{
            .remaining = *remain,
            .reset_at = now + seconds_to_ms(*sec),
        };

        // Update buckets_ before route_to_bucket_ so perform_cleanup() cannot race
        // and prune a newly mapped route before its bucket entry exists.
        if (buckets_.visit(bucket, [&](auto& pair) {
                pair.second = new_state;
            }) == 0UZ) {
            buckets_.insert_or_assign(std::string{bucket}, new_state);
        }

        if (route_to_bucket_.visit(route_key, [&](auto& pair) {
                if (pair.second != bucket) {
                    pair.second.assign(bucket);
                }
            }) == 0UZ) {
            route_to_bucket_.insert_or_assign(std::string{route_key}, std::string{bucket});
        }
    }

    [[nodiscard]] std::chrono::milliseconds handle_429(const std::string_view json_body) {
        rate_limited data{};

        // unsure if minified
        if (json::parse_json_view<glz_opts_429>(data, json_body)) {
            return DEFAULT_429_DELAY;
        }

        const double retry_sec = data.retry_after;
        #ifdef DISCUSY_LOGGING
        log::Logger{}("Rate limited for: {}s", retry_sec);
        #endif

        auto delay = seconds_to_ms(retry_sec);
        if (delay.count() <= 0) {
            delay = DEFAULT_429_DELAY; // just in case
        }

        if (data.global) {
            update_global_reset_at(std::chrono::steady_clock::now() + delay);
        }

        return delay;
    }
};

}