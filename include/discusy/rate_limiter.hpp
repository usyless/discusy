#pragma once

#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

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

    std::shared_mutex mtx_;
    std::unordered_map<std::string, bucket_state, transparent_string_hash, std::equal_to<>> buckets_;
    std::unordered_map<std::string, std::string, transparent_string_hash, std::equal_to<>> route_to_bucket_;
    std::chrono::steady_clock::time_point global_reset_at_{};

    std::chrono::steady_clock::time_point last_cleanup_{std::chrono::steady_clock::now()};

    // must hold mutex
    void perform_cleanup(std::chrono::steady_clock::time_point now) {
        if (now - last_cleanup_ < std::chrono::minutes{5}) return;
        last_cleanup_ = now;

        std::erase_if(buckets_, [now](const auto& pair) {
            return pair.second.reset_at < now;
        });

        std::erase_if(route_to_bucket_, [this](const auto& pair) {
            return !buckets_.contains(pair.second);
        });
        
        #ifdef DISCUSY_LOGGING
        log::Logger{}("Rate limit maps purged. Routes remaining: {}", route_to_bucket_.size());
        #endif
    }

public:
    [[nodiscard]] std::chrono::milliseconds get_delay(const std::string& route_key) {
        const auto now = std::chrono::steady_clock::now();
        

        {
        std::shared_lock lock{mtx_};
        if (now - last_cleanup_ > std::chrono::minutes{5}) {
            lock.unlock();
            {
            std::scoped_lock lock_exclusive{mtx_};
            perform_cleanup(now);
            }
            lock.lock();
        }

        if (global_reset_at_ > now) {
            return std::chrono::duration_cast<std::chrono::milliseconds>(global_reset_at_ - now);
        }

        if (auto b_it = route_to_bucket_.find(route_key); b_it != route_to_bucket_.end()) {
            if (auto s_it = buckets_.find(b_it->second); s_it != buckets_.end()) {
                auto& state = s_it->second;
                if (state.remaining <= 0 && state.reset_at > now) {
                    return std::chrono::duration_cast<std::chrono::milliseconds>(state.reset_at - now);
                }
            }
        }
        }
        return std::chrono::milliseconds{0};
    }

    void update(const std::string& route_key, auto&& headers) {
        auto bucket = headers["x-ratelimit-bucket"];
        auto remaining = headers["x-ratelimit-remaining"];
        auto reset_after = headers["x-ratelimit-reset-after"];

        if (!bucket.empty() && !remaining.empty() && !reset_after.empty()) {
            const auto remain = ulp::str::to_number<int>(remaining);
            if (!remain) return;
            const auto sec = ulp::str::to_number<double>(reset_after);
            if (!sec) return;

            {
            std::scoped_lock lock{mtx_};

            perform_cleanup(std::chrono::steady_clock::now());

            route_to_bucket_[route_key] = bucket;
            auto& state = buckets_[bucket];
            state.remaining = *remain;
            
            state.reset_at = std::chrono::steady_clock::now() + std::chrono::milliseconds(static_cast<int>(*sec * 1000));
            }
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
            std::scoped_lock lock{mtx_};
            global_reset_at_ = std::chrono::steady_clock::now() + delay;
        }

        return delay;
    }
};

}