#include <catch2/catch_test_macros.hpp>
#include <discusy/rate_limiter.hpp>

#include <chrono>
#include <string>
#include <string_view>
#include <unordered_map>

struct mock_headers {
    std::unordered_map<std::string, std::string> entries;

    mock_headers() = default;
    mock_headers(std::initializer_list<std::pair<const std::string, std::string>> init)
        : entries(init) {}

    std::string_view operator[](std::string_view key) const {
        const auto it = entries.find(std::string(key));
        if (it != entries.end()) {
            return it->second;
        }
        return {};
    }
};

TEST_CASE("RateLimiter: Route-to-bucket binding and delay calculation", "[rate_limiter]") {
    discusy::rate_limiter rl;
    const std::string route_a = "POST /channels/123/messages";
    const std::string route_b = "DELETE /channels/123/messages/456";

    // 1. Initially no delay
    CHECK(rl.get_delay(route_a).count() == 0);

    // 2. Update with remaining = 0 and reset_after = 0.5s
    const mock_headers headers_a{
        {"x-ratelimit-bucket", "bucket_channel_123"},
        {"x-ratelimit-remaining", "0"},
        {"x-ratelimit-reset-after", "0.5"},
    };
    rl.update(route_a, headers_a);

    // 3. Route A should now have a positive delay (<= 600ms)
    const auto delay_a = rl.get_delay(route_a);
    CHECK(delay_a.count() > 0);
    CHECK(delay_a.count() <= 600);

    // 4. Route B is not associated with this bucket yet, so no delay
    CHECK(rl.get_delay(route_b).count() == 0);

    // 5. Associating Route B with the same bucket causes it to share the delay
    const mock_headers headers_b{
        {"x-ratelimit-bucket", "bucket_channel_123"},
        {"x-ratelimit-remaining", "0"},
        {"x-ratelimit-reset-after", "0.4"},
    };
    rl.update(route_b, headers_b);
    CHECK(rl.get_delay(route_b).count() > 0);

    // 6. When quota is replenished (remaining > 0), delay is cleared
    const mock_headers headers_replenished{
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
        const std::string payload = R"({"retry_after": 0.4, "global": true})";
        const auto delay = rl.handle_429(payload);

        CHECK(delay.count() == 400);
        CHECK(rl.get_delay(route_1).count() > 0);
        CHECK(rl.get_delay(route_2).count() > 0);
    }

    SECTION("Malformed 429 JSON falls back to default 1000ms delay") {
        const std::string invalid_json = "not json";
        const auto delay = rl.handle_429(invalid_json);
        CHECK(delay.count() == 1000);
    }

    SECTION("Negative, zero, or non-finite retry_after falls back safely") {
        CHECK(rl.handle_429(R"({"retry_after": -5.0, "global": false})").count() == 1000);
        CHECK(rl.handle_429(R"({"retry_after": 0, "global": false})").count() == 1000);
        CHECK(rl.handle_429(R"({"retry_after": 1e30, "global": false})").count() == 86400000);
    }

    SECTION("Shorter global 429 does not overwrite a longer active global 429") {
        const auto long_delay = rl.handle_429(R"({"retry_after": 1.0, "global": true})");
        CHECK(long_delay.count() == 1000);

        const auto short_delay = rl.handle_429(R"({"retry_after": 0.1, "global": true})");
        CHECK(short_delay.count() == 100);

        const auto effective = rl.get_delay(route_1);
        CHECK(effective.count() > 500);
    }
}

TEST_CASE("RateLimiter: Sub-millisecond ceiling and invalid reset_after headers", "[rate_limiter]") {
    discusy::rate_limiter rl;
    const std::string route = "POST/channels/42/messages";

    SECTION("Sub-millisecond reset_after rounds up to at least 1ms") {
        const mock_headers headers{
            {"x-ratelimit-bucket", "bucket_sub_ms"},
            {"x-ratelimit-remaining", "0"},
            {"x-ratelimit-reset-after", "0.05"},
        };
        rl.update(route, headers);
        const auto delay = rl.get_delay(route);
        CHECK(delay.count() >= 1);
        CHECK(delay.count() <= 100);
    }

    SECTION("Huge or negative reset_after does not overflow or trigger UB") {
        const mock_headers huge_headers{
            {"x-ratelimit-bucket", "bucket_huge"},
            {"x-ratelimit-remaining", "0"},
            {"x-ratelimit-reset-after", "1e30"},
        };
        rl.update(route, huge_headers);
        const auto delay = rl.get_delay(route);
        CHECK(delay.count() > 0);
        CHECK(delay.count() <= 86400000);

        const mock_headers neg_headers{
            {"x-ratelimit-bucket", "bucket_huge"},
            {"x-ratelimit-remaining", "0"},
            {"x-ratelimit-reset-after", "-10.5"},
        };
        rl.update(route, neg_headers);
        CHECK(rl.get_delay(route).count() == 0);
    }

    SECTION("get_delay returns max of global and bucket delay") {
        const mock_headers headers{
            {"x-ratelimit-bucket", "bucket_long"},
            {"x-ratelimit-remaining", "0"},
            {"x-ratelimit-reset-after", "1.0"},
        };
        rl.update(route, headers);
        (void)rl.handle_429(R"({"retry_after": 0.1, "global": true})");

        const auto delay = rl.get_delay(route);
        CHECK(delay.count() > 500);
    }
}
