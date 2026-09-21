#include <catch2/catch_test_macros.hpp>
#include <discusy/rate_limiter.hpp>

#include <chrono>
#include <string>
#include <unordered_map>

TEST_CASE("RateLimiter: Route-to-bucket binding and delay calculation", "[rate_limiter]") {
    discusy::rate_limiter rl;
    const std::string route_a = "POST /channels/123/messages";
    const std::string route_b = "DELETE /channels/123/messages/456";

    // 1. Initially no delay
    CHECK(rl.get_delay(route_a).count() == 0);

    // 2. Update with remaining = 0 and reset_after = 0.5s
    std::unordered_map<std::string, std::string> headers_a{
        {"x-ratelimit-bucket", "bucket_channel_123"},
        {"x-ratelimit-remaining", "0"},
        {"x-ratelimit-reset-after", "0.5"},
    };
    rl.update(route_a, headers_a);

    // 3. Route A should now have a positive delay (<= 600ms)
    auto delay_a = rl.get_delay(route_a);
    CHECK(delay_a.count() > 0);
    CHECK(delay_a.count() <= 600);

    // 4. Route B is not associated with this bucket yet, so no delay
    CHECK(rl.get_delay(route_b).count() == 0);

    // 5. Associating Route B with the same bucket causes it to share the delay
    std::unordered_map<std::string, std::string> headers_b{
        {"x-ratelimit-bucket", "bucket_channel_123"},
        {"x-ratelimit-remaining", "0"},
        {"x-ratelimit-reset-after", "0.4"},
    };
    rl.update(route_b, headers_b);
    CHECK(rl.get_delay(route_b).count() > 0);

    // 6. When quota is replenished (remaining > 0), delay is cleared
    std::unordered_map<std::string, std::string> headers_replenished{
        {"x-ratelimit-bucket", "bucket_channel_123"},
        {"x-ratelimit-remaining", "5"},
        {"x-ratelimit-reset-after", "0.3"},
    };
    rl.update(route_a, headers_replenished);
    CHECK(rl.get_delay(route_a).count() == 0);
    CHECK(rl.get_delay(route_b).count() == 0);
}

TEST_CASE("RateLimiter: 429 response handling", "[rate_limiter]") {
    discusy::rate_limiter rl;
    const std::string route_1 = "POST /channels/999/messages";
    const std::string route_2 = "GET /guilds/888";

    SECTION("Global 429 delays all routes") {
        std::string payload = R"({"retry_after": 0.4, "global": true})";
        auto delay = rl.handle_429(payload);

        CHECK(delay.count() == 400);
        CHECK(rl.get_delay(route_1).count() > 0);
        CHECK(rl.get_delay(route_2).count() > 0);
    }

    SECTION("Malformed 429 JSON falls back to default 1000ms delay") {
        std::string invalid_json = "not json";
        auto delay = rl.handle_429(invalid_json);
        CHECK(delay.count() == 1000);
    }
}
