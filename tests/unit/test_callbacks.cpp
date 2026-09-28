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

TEST_CASE("Callback: when() basic match and completion", "[callback]") {
    discusy::ctx::io_context ctx{1};
    discusy::Callback<int> cb{ctx};

    bool matched = false;
    int received_value = 0;

    cb.when(
        [](int val) { return val == 42; },
        [&](boost::system::error_code ec, int val) {
            CHECK(!ec);
            matched = true;
            received_value = val;
        }
    );

    // Fire non-matching event first
    cb.fire(10);
    ctx->poll();
    CHECK(!matched);

    ctx->restart();
    // Fire matching event
    cb.fire(42);
    ctx->run();
    CHECK(matched);
    CHECK(received_value == 42);
}

TEST_CASE("Callback: Execution order system -> when -> user", "[callback]") {
    discusy::ctx::io_context ctx{1};
    discusy::Callback<int> cb{ctx};

    std::vector<std::string> order;

    // Register user listener
    cb.listen(discusy::callback_priority::user, [&](int) {
        order.emplace_back("user");
    });

    // Register system listener
    cb.listen(discusy::callback_priority::system, [&](int) {
        order.emplace_back("system");
    });

    // Register when waiter
    cb.when(
        [&](int val) {
            CHECK(!order.empty());
            CHECK(order.front() == "system");
            return val == 100;
        },
        [&](boost::system::error_code ec, int) {
            CHECK(!ec);
            order.emplace_back("when");
        }
    );

    cb.fire(100);
    ctx->run();

    REQUIRE(order.size() == 3);
    CHECK(order[0] == "system");
    CHECK(order[1] == "when");
    CHECK(order[2] == "user");
}

TEST_CASE("Callback: when() aborted on Callback destruction", "[callback]") {
    discusy::ctx::io_context ctx{1};
    bool aborted = false;

    {
        discusy::Callback<int> cb{ctx};
        cb.when(
            [](int) { return true; },
            [&](boost::system::error_code ec, int) {
                if (ec == boost::asio::error::operation_aborted) {
                    aborted = true;
                }
            }
        );
        // cb destroyed here without firing
    }

    ctx->run();
    CHECK(aborted);
}

TEST_CASE("Callback: when() cancellation slot support", "[callback]") {
    discusy::ctx::io_context ctx{1};
    discusy::Callback<int> cb{ctx};

    boost::asio::cancellation_signal sig;
    bool cancelled = false;

    cb.when(
        [](int) { return true; },
        boost::asio::bind_cancellation_slot(
            sig.slot(),
            [&](boost::system::error_code ec, int) {
                if (ec == boost::asio::error::operation_aborted) {
                    cancelled = true;
                }
            }
        )
    );

    // Cancel before any fire
    sig.emit(boost::asio::cancellation_type::terminal);

    ctx->run();
    CHECK(cancelled);
}

TEST_CASE("Callback: when() coroutine awaitable support", "[callback]") {
    discusy::ctx::io_context ctx{1};
    discusy::Callback<int> cb{ctx};

    bool coroutine_completed = false;
    int result = 0;

    ctx.co_launch_detached([&]() -> discusy::coro::awaitable<void> {
        result = co_await cb.when([](int x) { return x == 777; }, boost::asio::use_awaitable);
        coroutine_completed = true;
    });

    ctx.post([&]() {
        cb.fire(1);
        cb.fire(777);
    });

    ctx->run();
    CHECK(coroutine_completed);
    CHECK(result == 777);
}

TEST_CASE("Callback: Concurrent when() and fire() stress test", "[callback]") {
    discusy::ctx::io_context ctx{2};
    discusy::Callback<std::size_t> cb{ctx};

    constexpr std::size_t NUM_WAITERS = 100;
    std::atomic<std::size_t> completed_count{0};

    // Register waiters across multiple threads
    std::vector<std::thread> threads;
    static constexpr std::size_t num_threads = 4;
    threads.reserve(num_threads);
    for (std::size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            for (std::size_t i = 0; i < NUM_WAITERS / num_threads; ++i) {
                cb.when(
                    [val = (t * 100) + i](std::size_t x) { return x == val; },
                    [&](boost::system::error_code ec, std::size_t) {
                        if (!ec) {
                            completed_count.fetch_add(1, std::memory_order_relaxed);
                        }
                    }
                );
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }
    threads.clear();

    // Fire matching events concurrently
    for (std::size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            for (std::size_t i = 0; i < NUM_WAITERS / num_threads; ++i) {
                cb.fire((t * 100) + i);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    ctx->run();
    CHECK(completed_count.load() == NUM_WAITERS);
}

