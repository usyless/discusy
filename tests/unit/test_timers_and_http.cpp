#include <catch2/catch_test_macros.hpp>

#include <discusy/http_client.hpp>
#include <discusy/timers.hpp>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <utility>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

namespace {

namespace net = boost::asio;
namespace beast = boost::beast;
namespace bhttp = beast::http;
using net::ip::tcp;

struct local_http_server {
    net::io_context ioc;
    tcp::acceptor acceptor;
    std::jthread thread;
    std::atomic<int> accepted_connections{0};
    std::atomic<int> requests_handled{0};
    std::atomic<bool> stall_responses{false};
    std::atomic<bool> close_after_first_keepalive{false};

    local_http_server()
        : acceptor{ioc, tcp::endpoint{net::ip::make_address("127.0.0.1"), 0}} {}

    local_http_server(const local_http_server&) = delete;
    local_http_server& operator=(const local_http_server&) = delete;
    local_http_server(local_http_server&&) = delete;
    local_http_server& operator=(local_http_server&&) = delete;

    std::uint16_t port() const {
        return acceptor.local_endpoint().port();
    }

    void start() {
        do_accept();
        thread = std::jthread([this] { ioc.run(); });
    }

    void stop() {
        net::post(ioc, [this] {
            beast::error_code ec;
            acceptor.close(ec);
        });
        ioc.stop();
        if (thread.joinable()) {
            thread.join();
        }
    }

    ~local_http_server() noexcept {
        try {
            stop();
        } catch (...) { // NOLINT(bugprone-empty-catch)
        }
    }

private:
    void do_accept() {
        acceptor.async_accept([this](const beast::error_code ec, tcp::socket sock) {
            if (ec) return;
            accepted_connections.fetch_add(1, std::memory_order_relaxed);
            const auto s = std::make_shared<tcp::socket>(std::move(sock));
            const auto buf = std::make_shared<beast::flat_buffer>();
            handle_session(s, buf);
            do_accept();
        });
    }

    void handle_session(const std::shared_ptr<tcp::socket>& sock, const std::shared_ptr<beast::flat_buffer>& buf) {
        const auto req = std::make_shared<bhttp::request<bhttp::string_body>>();
        bhttp::async_read(*sock, *buf, *req, [this, sock, buf, req](const beast::error_code ec, std::size_t) {
            if (ec) return;
            requests_handled.fetch_add(1, std::memory_order_relaxed);

            if (stall_responses.load(std::memory_order_relaxed)) {
                const auto hold = std::make_shared<net::steady_timer>(ioc, std::chrono::seconds{5});
                hold->async_wait([sock, hold](const beast::error_code&) {});
                return;
            }

            const std::string target{req->target()};
            const auto res = std::make_shared<bhttp::response<bhttp::string_body>>(bhttp::status::ok, req->version());
            res->keep_alive(req->keep_alive());

            if (target == "/head") {
                res->set(bhttp::field::content_length, "12345");
                // Intentionally no body for HEAD response.
            } else if (target == "/redirect-303") {
                res->result(bhttp::status::see_other);
                res->set(bhttp::field::location, "/echo-method");
                res->prepare_payload();
            } else if (target == "/echo-method") {
                res->body() = std::string{bhttp::to_string(req->method())};
                res->prepare_payload();
            } else if (target == "/close") {
                res->keep_alive(false);
                res->body() = "closing";
                res->prepare_payload();
            } else {
                res->body() = "ok:" + target;
                res->prepare_payload();
            }

            const bool keep_open = res->keep_alive();
            const bool close_stale = close_after_first_keepalive.exchange(false, std::memory_order_relaxed);

            bhttp::async_write(*sock, *res, [this, sock, buf, res, keep_open, close_stale](const beast::error_code wec, std::size_t) {
                if (wec) return;
                if (close_stale) {
                    beast::error_code ignored;
                    sock->shutdown(tcp::socket::shutdown_both, ignored);
                    sock->close(ignored);
                    return;
                }
                if (keep_open) {
                    handle_session(sock, buf);
                } else {
                    beast::error_code ignored;
                    sock->shutdown(tcp::socket::shutdown_send, ignored);
                }
            });
        });
    }
};

} // namespace

TEST_CASE("HTTP: generate_route_key handles valid, query, and non-REST URLs safely", "[http_client]") {
    using discusy::http::generate_route_key;
    using discusy::http::method;

    CHECK(generate_route_key(method::get, "https://discord.com/api/v10/channels/12345/messages/67890")
          == "GET/channels/12345/messages/:id");

    // Query strings and fragments must be stripped so route buckets don't explode.
    CHECK(generate_route_key(method::get, "https://discord.com/api/v10/channels/12345/messages?limit=50&before=999")
          == "GET/channels/12345/messages");
    CHECK(generate_route_key(method::get, "https://discord.com/api/v10/channels/12345/messages/67890?around=1#frag")
          == "GET/channels/12345/messages/:id");

    // Guilds and webhooks keep major params (including webhook token).
    CHECK(generate_route_key(method::patch, "https://discord.com/api/v10/guilds/111/members/222")
          == "PATCH/guilds/111/members/:id");
    CHECK(generate_route_key(method::patch, "https://discord.com/api/v10/webhooks/111/222/messages/333")
          == "PATCH/webhooks/111/222/messages/:id");

    // Short / non-Discord URLs must return empty string instead of throwing out_of_range.
    CHECK(generate_route_key(method::get, "https://example.com").empty());
    CHECK(generate_route_key(method::get, "short").empty());
    CHECK(generate_route_key(method::get, "https://discord.com/api/v100/channels/1").empty());
}

TEST_CASE("HTTP: parse_http_url validates schemes, ports, IPv6, and targets", "[http_client]") {
    using discusy::http::parse_http_url;

    {
        const auto parsed = parse_http_url("HTTPS://Example.COM/path?a=1#ignored");
        REQUIRE(parsed.has_value());
        if (parsed) {
            CHECK(parsed->is_ssl);
            CHECK(parsed->host_is_name);
            CHECK(parsed->host == "example.com");
            CHECK(parsed->host_header == "example.com");
            CHECK(parsed->port == "443");
            CHECK(parsed->target == "/path?a=1");
        }
    }

    {
        const auto parsed = parse_http_url("http://localhost:8080?x=1#frag");
        REQUIRE(parsed.has_value());
        if (parsed) {
            CHECK(!parsed->is_ssl);
            CHECK(parsed->host == "localhost");
            CHECK(parsed->host_header == "localhost:8080");
            CHECK(parsed->port == "8080");
            CHECK(parsed->target == "/?x=1");
        }
    }

    {
        const auto parsed = parse_http_url("http://[::1]:8080/test");
        REQUIRE(parsed.has_value());
        if (parsed) {
            CHECK(!parsed->is_ssl);
            CHECK(!parsed->host_is_name);
            CHECK(parsed->host == "::1");
            CHECK(parsed->host_header == "[::1]:8080");
            CHECK(parsed->port == "8080");
            CHECK(parsed->target == "/test");
        }
    }

    {
        const auto parsed = parse_http_url("https://127.0.0.1:8443");
        REQUIRE(parsed.has_value());
        if (parsed) {
            CHECK(parsed->is_ssl);
            CHECK(!parsed->host_is_name);
            CHECK(parsed->host == "127.0.0.1");
            CHECK(parsed->host_header == "127.0.0.1:8443");
            CHECK(parsed->target == "/");
        }
    }

    CHECK(!parse_http_url("ftp://example.com/file").has_value());
    CHECK(!parse_http_url("https:///only-path").has_value());
    CHECK(!parse_http_url("http://example.com:0/").has_value());
    CHECK(!parse_http_url("not a url").has_value());
}

TEST_CASE("HTTP client: keep-alive reuse, HEAD, redirects, stale retry, cancel, and timeout", "[http_client]") {
    local_http_server server;
    server.start();
    const std::string base = "http://127.0.0.1:" + std::to_string(server.port());

    discusy::ctx::io_context io_ctx;
    net::ssl::context ssl_ctx{net::ssl::context::tls_client};
    discusy::http_client client{ssl_ctx, io_ctx};
    client.start();

    const auto run_sync = [&](auto&& initiate) {
        io_ctx->restart();
        boost::system::error_code out_ec;
        discusy::http::response out_resp;
        initiate([&](const boost::system::error_code ec, discusy::http::response resp) {
            out_ec = ec;
            out_resp = std::move(resp);
            io_ctx->stop();
        });
        io_ctx->run();
        return std::pair{out_ec, std::move(out_resp)};
    };

    SECTION("Invalid URL fails immediately with invalid_argument") {
        auto [ec, resp] = run_sync([&](auto&& cb) {
            client.request(discusy::http::method::get, "ftp://127.0.0.1/bad", {}, {}, {}, std::forward<decltype(cb)>(cb));
        });
        CHECK(ec == net::error::invalid_argument);
    }

    SECTION("Keep-alive connection is pooled and reused") {
        auto [ec1, r1] = run_sync([&](auto&& cb) {
            client.request(discusy::http::method::get, base + "/one", {}, {}, {}, std::forward<decltype(cb)>(cb));
        });
        REQUIRE(!ec1);
        CHECK(r1.status_code == 200);
        CHECK(r1.body == "ok:/one");

        auto [ec2, r2] = run_sync([&](auto&& cb) {
            client.request(discusy::http::method::get, base + "/two", {}, {}, {}, std::forward<decltype(cb)>(cb));
        });
        REQUIRE(!ec2);
        CHECK(r2.status_code == 200);
        CHECK(r2.body == "ok:/two");
        CHECK(server.accepted_connections.load() == 1);
    }

    SECTION("max_pool_size == 0 disables pooling completely") {
        client.set_max_pool_size(0);
        auto [ec1, r1] = run_sync([&](auto&& cb) {
            client.request(discusy::http::method::get, base + "/a", {}, {}, {}, std::forward<decltype(cb)>(cb));
        });
        REQUIRE(!ec1);
        auto [ec2, r2] = run_sync([&](auto&& cb) {
            client.request(discusy::http::method::get, base + "/b", {}, {}, {}, std::forward<decltype(cb)>(cb));
        });
        REQUIRE(!ec2);
        CHECK(server.accepted_connections.load() == 2);
    }

    SECTION("HEAD request with Content-Length completes immediately without hanging") {
        discusy::http::options opts;
        opts.request_timeout = std::chrono::seconds{2};
        auto [ec, resp] = run_sync([&](auto&& cb) {
            client.request(discusy::http::method::head, base + "/head", {}, {}, opts, std::forward<decltype(cb)>(cb));
        });
        REQUIRE(!ec);
        CHECK(resp.status_code == 200);
        CHECK(resp.body.empty());
    }

    SECTION("303 redirect converts POST to GET and preserves HEAD") {
        discusy::http::options opts;
        opts.follow_redirects = true;

        auto [ec_post, r_post] = run_sync([&](auto&& cb) {
            client.request(discusy::http::method::post, base + "/redirect-303", "payload", {}, opts, std::forward<decltype(cb)>(cb));
        });
        REQUIRE(!ec_post);
        CHECK(r_post.status_code == 200);
        CHECK(r_post.method == discusy::http::method::get);
        CHECK(r_post.body == "GET");

        auto [ec_head, r_head] = run_sync([&](auto&& cb) {
            client.request(discusy::http::method::head, base + "/redirect-303", {}, {}, opts, std::forward<decltype(cb)>(cb));
        });
        REQUIRE(!ec_head);
        CHECK(r_head.status_code == 200);
        CHECK(r_head.method == discusy::http::method::head);
    }

    SECTION("Stale pooled connection recovers transparently on a fresh connection") {
        server.close_after_first_keepalive.store(true);
        auto [ec1, r1] = run_sync([&](auto&& cb) {
            client.request(discusy::http::method::get, base + "/first", {}, {}, {}, std::forward<decltype(cb)>(cb));
        });
        REQUIRE(!ec1);

        // Give the server's FIN a brief moment to arrive so the pooled socket is half-closed.
        std::this_thread::sleep_for(std::chrono::milliseconds{30});

        auto [ec2, r2] = run_sync([&](auto&& cb) {
            client.request(discusy::http::method::get, base + "/after-stale", {}, {}, {}, std::forward<decltype(cb)>(cb));
        });
        REQUIRE(!ec2);
        CHECK(r2.status_code == 200);
        CHECK(r2.body == "ok:/after-stale");
        CHECK(server.accepted_connections.load() == 2);
    }

    SECTION("User cancellation returns operation_aborted, not timed_out") {
        server.stall_responses.store(true);
        net::cancellation_signal sig;
        net::steady_timer cancel_timer{io_ctx.executor_, std::chrono::milliseconds{40}};
        cancel_timer.async_wait([&](const boost::system::error_code&) {
            sig.emit(net::cancellation_type::terminal);
        });

        discusy::http::options opts;
        opts.request_timeout = std::chrono::seconds{5};

        auto [ec, resp] = run_sync([&](auto&& cb) {
            client.request(
                discusy::http::method::get,
                base + "/stall",
                {},
                {},
                opts,
                net::bind_cancellation_slot(sig.slot(), std::forward<decltype(cb)>(cb))
            );
        });
        CHECK(ec == net::error::operation_aborted);
    }

    SECTION("Request timeout returns timed_out") {
        server.stall_responses.store(true);
        discusy::http::options opts;
        opts.request_timeout = std::chrono::seconds{1};

        auto [ec, resp] = run_sync([&](auto&& cb) {
            client.request(discusy::http::method::get, base + "/stall", {}, {}, opts, std::forward<decltype(cb)>(cb));
        });
        CHECK(ec == net::error::timed_out);
    }

    client.stop();
}

TEST_CASE("Timers: stop_interval works from inside callbacks and suspended coroutines", "[timers]") {
    discusy::ctx::io_context io_ctx;

    SECTION("Callback interval stopping itself does not reschedule") {
        discusy::Timers timers{io_ctx};
        std::atomic<int> ticks{0};

        timers.start_interval([&](const discusy::timer id) {
            if (ticks.fetch_add(1) + 1 >= 2) {
                CHECK(timers.stop_interval(id));
            }
        }, std::chrono::milliseconds{10});

        net::steady_timer watchdog{io_ctx.executor_, std::chrono::milliseconds{120}};
        watchdog.async_wait([&](const boost::system::error_code&) { io_ctx->stop(); });
        io_ctx->run();

        CHECK(ticks.load() == 2);
    }

    SECTION("Coroutine interval stopped while suspended does not continue") {
        discusy::Timers timers{io_ctx};
        std::atomic<int> ticks{0};

        const auto id = timers.start_interval([&](discusy::timer) -> discusy::coro::awaitable<void> {
            ticks.fetch_add(1);
            co_await io_ctx.sleep(std::chrono::milliseconds{40}, discusy::token::t_deferred);
        }, std::chrono::milliseconds{10});

        net::steady_timer stopper{io_ctx.executor_, std::chrono::milliseconds{25}};
        stopper.async_wait([&](const boost::system::error_code&) {
            CHECK(timers.stop_interval(id));
        });

        net::steady_timer watchdog{io_ctx.executor_, std::chrono::milliseconds{140}};
        watchdog.async_wait([&](const boost::system::error_code&) { io_ctx->stop(); });
        io_ctx->run();

        CHECK(ticks.load() == 1);
    }

    SECTION("Destroying Timers while io_context runs causes no UAF") {
        std::atomic<int> fired{0};
        auto guard = net::make_work_guard(io_ctx.executor_);
        std::jthread worker([&] { io_ctx->run(); });

        {
            discusy::Timers timers{io_ctx};
            for (int i = 0; i < 16; ++i) {
                timers.start_timer([&](discusy::timer) { fired.fetch_add(1); }, std::chrono::milliseconds{20});
                timers.start_interval([&](discusy::timer) { fired.fetch_add(1); }, std::chrono::milliseconds{15});
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{5});
        } // Timers destroyed here while worker thread is actively running io_ctx

        std::this_thread::sleep_for(std::chrono::milliseconds{40});
        guard.reset();
        worker.join();
        CHECK(fired.load() >= 0);
    }
}
