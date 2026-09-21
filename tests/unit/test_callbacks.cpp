#include <catch2/catch_test_macros.hpp>
#include <discusy/callback.hpp>
#include <discusy/io_context.hpp>
#include <vector>
#include <string>
#include <thread>
#include <atomic>

TEST_CASE("Callback: Short-circuiting execution on return false", "[callback]") {
    discusy::ctx::io_context ctx{1};
    discusy::Callback<int> cb{ctx};

    std::vector<int> executed;

    cb.listen([&](int) -> bool {
        executed.push_back(1);
        return true;
    });

    cb.listen([&](int) -> bool {
        executed.push_back(2);
        return false; // return false aborts subsequent user callbacks
    });

    cb.listen([&](int) -> bool {
        executed.push_back(3);
        return true;
    });

    cb.fire(100);
    ctx->run();

    REQUIRE(executed.size() == 2);
    CHECK(executed[0] == 1);
    CHECK(executed[1] == 2);
}

TEST_CASE("Callback: Priority ordering system runs before user", "[callback]") {
    discusy::ctx::io_context ctx{1};
    discusy::Callback<int> cb{ctx};

    std::vector<std::string> order;

    // Register user first
    cb.listen(discusy::callback_priority::user, [&](int) {
        order.emplace_back("user");
    });

    // Register system second
    cb.listen(discusy::callback_priority::system, [&](int) {
        order.emplace_back("system");
    });

    cb.fire(42);
    ctx->run();

    REQUIRE(order.size() == 2);
    CHECK(order[0] == "system");
    CHECK(order[1] == "user");
}

TEST_CASE("Callback: Unregister listener", "[callback]") {
    discusy::ctx::io_context ctx{1};
    discusy::Callback<int> cb{ctx};

    int call_count = 0;
    auto id = cb.listen([&](int) {
        call_count++;
    });

    CHECK(cb.unregister(id));

    cb.fire(1);
    ctx->run();

    CHECK(call_count == 0);
}

TEST_CASE("CounterCallback: Thread-safe rendezvous trigger", "[callback]") {
    std::atomic<int> completions{0};
    constexpr int TARGET = 10;

    discusy::CounterCallback barrier{TARGET, [&]() {
        completions.fetch_add(1, std::memory_order_relaxed);
    }};

    std::vector<std::thread> workers;
    workers.reserve(TARGET);
    for (int i = 0; i < TARGET; ++i) {
        workers.emplace_back([&barrier]() {
            barrier.arrive();
        });
    }

    for (auto& t : workers) {
        t.join();
    }

    // Must have fired exactly once
    CHECK(completions.load() == 1);

    // Additional calls after target reached must NOT fire again
    barrier.arrive();
    CHECK(completions.load() == 1);
}
