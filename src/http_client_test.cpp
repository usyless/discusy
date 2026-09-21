#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <memory>
#include <memory_resource>
#include <optional>
#include <print>
#include <string_view>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/core/flat_static_buffer.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/span_body.hpp>

#include <discusy/discusy.hpp>

#ifdef DISCUSY_USE_MIMALLOC
#include <mimalloc.h>
#endif

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

using executor_t = net::io_context::executor_type;
using socket_t = boost::asio::basic_stream_socket<boost::asio::ip::tcp, executor_t>;
using acceptor_t = boost::asio::basic_socket_acceptor<boost::asio::ip::tcp, executor_t>;

using pmr_alloc = std::pmr::polymorphic_allocator<char>;
using pmr_fields = http::basic_fields<pmr_alloc>;
using pmr_string_body = http::basic_string_body<char, std::char_traits<char>, pmr_alloc>;
using request_t = http::request<pmr_string_body, pmr_fields>;
using response_t = http::response<http::span_body<const char>>;

static constexpr std::string_view kResponseBody = R"({"status":"ok","message":"benchmark"})";

class mock_session {
    socket_t socket_;
    beast::flat_static_buffer<8192> buffer_;

    alignas(std::max_align_t) std::array<std::byte, 4096> memory_arena_;
    std::optional<std::pmr::monotonic_buffer_resource> pool_;
    
    std::optional<request_t> req_;
    response_t resp_;

public:
    explicit mock_session(socket_t socket)
        : socket_(std::move(socket)) {
            boost::system::error_code ec;
            socket_.set_option(tcp::no_delay(true), ec);
        }

    void run(std::shared_ptr<mock_session>&& self_ptr) {
        do_read(std::move(self_ptr));
    }

private:
    void do_read(std::shared_ptr<mock_session>&& self_ptr) {
        req_.reset();
        pool_.emplace(memory_arena_.data(), memory_arena_.size(), std::pmr::null_memory_resource());
        req_.emplace(
            std::piecewise_construct,
            std::make_tuple(pmr_alloc{&*pool_}),
            std::make_tuple(pmr_alloc{&*pool_})
        );

        http::async_read(
            socket_,
            buffer_,
            *req_,
            [self_ptr = std::move(self_ptr)](beast::error_code ec, std::size_t /*bytes*/) mutable -> void {
                if (ec == http::error::end_of_stream || ec == net::error::eof || ec) {
                    return self_ptr->do_close();
                }

                auto ptr = self_ptr.get();
                ptr->do_write(std::move(self_ptr));
            }
        );
    }

    void do_write(std::shared_ptr<mock_session>&& self_ptr) {
        resp_ = response_t{http::status::ok, req_->version()};
        resp_.set(http::field::server, "discusy-bench-mock");
        resp_.set(http::field::content_type, "application/json");
        resp_.keep_alive(req_->keep_alive());

        resp_.body() = boost::span<const char>(kResponseBody.data(), kResponseBody.size());
        resp_.prepare_payload();

        http::async_write(
            socket_,
            resp_,
            [self_ptr = std::move(self_ptr)](beast::error_code ec, std::size_t /*bytes*/) mutable -> void {
                if (ec || !self_ptr->req_->keep_alive()) {
                    return self_ptr->do_close();
                }

                auto ptr = self_ptr.get();
                ptr->do_read(std::move(self_ptr));
            }
        );
    }

    void do_close() {
        beast::error_code ignored;
        socket_.shutdown(tcp::socket::shutdown_both, ignored);
    }
};

void handle_mock_session(socket_t socket) {
    auto session = std::allocate_shared<mock_session>(
        boost::asio::recycling_allocator<void>{},
        std::move(socket)
    );
    auto ptr = session.get();
    ptr->run(std::move(session));
}

class mock_http_server {
    static constexpr auto concurrency = 8;
    net::io_context ioc_{concurrency};
    acceptor_t acceptor_;
    std::vector<std::thread> threads_;

    void do_accept() {
        acceptor_.async_accept([this](boost::system::error_code ec, socket_t socket) {
            if (!ec) {
                handle_mock_session(std::move(socket));
            }
            if (acceptor_.is_open()) {
                do_accept();
            }
        });
    }

public:
    explicit mock_http_server(unsigned short port)
        : acceptor_(ioc_, tcp::endpoint{net::ip::make_address("127.0.0.1"), port}) {
        do_accept();
        for (int i = 0; i < concurrency; ++i) {
            threads_.emplace_back([this] { ioc_.run(); });
        }
    }

    void stop() {
        boost::system::error_code ec;
        acceptor_.close(ec);
        ioc_.stop();
        for (auto& thread_ : threads_) {
            if (thread_.joinable()) {
                thread_.join();
            }
        }
    }

    ~mock_http_server() {
        stop();
    }
};

struct BenchmarkResult {
    std::string name;
    std::size_t total_reqs{0};
    double total_sec{0.0};
    double throughput{0.0};
    double p50_us{0.0};
    double p95_us{0.0};
    double p99_us{0.0};
};

template <typename ClientType>
class BenchmarkRunner {
    std::string name_;
    discusy::ctx::io_context& io_ctx_;
    ClientType& client_;
    std::string url_;
    std::size_t total_reqs_;
    std::size_t concurrency_;

    std::vector<double> latencies_us_;
    std::atomic<std::size_t> sent_count_{0};
    std::atomic<std::size_t> completed_count_{0};
    std::chrono::steady_clock::time_point start_time_;
    std::function<void(BenchmarkResult)> on_finish_;

public:
    BenchmarkRunner(std::string name, discusy::ctx::io_context& ctx, ClientType& client, std::string url, std::size_t total, std::size_t conc, std::function<void(BenchmarkResult)> on_finish)
        : name_(std::move(name)), io_ctx_(ctx), client_(client), url_(std::move(url)), total_reqs_(total), concurrency_(conc), latencies_us_(total), on_finish_(std::move(on_finish)) {}

    void start() {
        std::println("\n==============================================");
        std::println("Starting benchmark for [{}]: {} total requests, concurrency level = {}", name_, total_reqs_, concurrency_);
        start_time_ = std::chrono::steady_clock::now();
        for (std::size_t i = 0; i < concurrency_ && i < total_reqs_; ++i) {
            launch_one();
        }
    }

private:
    void launch_one() {
        auto req_id = sent_count_.fetch_add(1);
        if (req_id >= total_reqs_) return;

        auto t0 = std::chrono::steady_clock::now();

        client_.request(
            boost::beast::http::verb::get,
            url_,
            "",
            {},
            {},
            [this, t0](boost::system::error_code ec, discusy::http::response) {
                if (ec) {
                    std::println("[{}] Request failed with error: {}", name_, ec.message());
                }

                auto done = completed_count_.fetch_add(1);
                latencies_us_[done] = std::chrono::duration<double, std::micro>(
                    std::chrono::steady_clock::now() - t0
                ).count();

                if (done % 50000 == 0 && done > 0) {
                    std::println("[{}] Progress: {}/{}", name_, done, total_reqs_);
                }

                if ((done + 1) == total_reqs_) {
                    auto end_time = std::chrono::steady_clock::now();
                    double total_sec = std::chrono::duration<double>(end_time - start_time_).count();

                    std::ranges::sort(latencies_us_);
                    double p50 = latencies_us_[latencies_us_.size() * 50 / 100];
                    double p95 = latencies_us_[latencies_us_.size() * 95 / 100];
                    double p99 = latencies_us_[latencies_us_.size() * 99 / 100];

                    BenchmarkResult result{
                        .name = name_,
                        .total_reqs = total_reqs_,
                        .total_sec = total_sec,
                        .throughput = total_reqs_ / total_sec,
                        .p50_us = p50,
                        .p95_us = p95,
                        .p99_us = p99
                    };

                    std::println("----------------------------------------------");
                    std::println("[{}] Completed: {} requests in {:.3f}s", name_, total_reqs_, total_sec);
                    std::println("[{}] Throughput: {:.2f} req/sec", name_, result.throughput);
                    std::println("[{}] Latency (p50): {:.2f} us", name_, p50);
                    std::println("[{}] Latency (p95): {:.2f} us", name_, p95);
                    std::println("[{}] Latency (p99): {:.2f} us", name_, p99);
                    std::println("----------------------------------------------");

                    if (on_finish_) {
                        on_finish_(result);
                    }
                    return;
                }

                launch_one();
            }
        );
    }
};

int main() {
    mock_http_server server{8080};
    std::string test_url = "http://127.0.0.1:8080/benchmark";
    constexpr std::size_t total_requests = 250000;
    constexpr std::size_t concurrency = 100;

    std::vector<BenchmarkResult> results;

    // 1. Benchmark Callback HTTP Client
    {
        discusy::ctx::io_context io_ctx{8};
        boost::asio::ssl::context ssl_ctx{boost::asio::ssl::context::tls_client};
        discusy::http_client_callback client{ssl_ctx, io_ctx};
        client.set_max_pool_size(500);

        std::optional<BenchmarkRunner<discusy::http_client_callback>> runner;
        runner.emplace("Callback Client", io_ctx, client, test_url, total_requests, concurrency, [&](BenchmarkResult res) {
            results.push_back(res);
            io_ctx->stop();
        });
        runner->start();

        std::vector<std::thread> threads;
        for (int i = 0; i < 8; ++i) {
            threads.emplace_back([&] { io_ctx->run(); });
        }
        for (auto& t : threads) {
            if (t.joinable()) t.join();
        }
    }

    // 2. Benchmark Coro (co_composed) HTTP Client
    {
        discusy::ctx::io_context io_ctx{8};
        boost::asio::ssl::context ssl_ctx{boost::asio::ssl::context::tls_client};
        discusy::http_client_coro client{ssl_ctx, io_ctx};
        client.set_max_pool_size(500);

        std::optional<BenchmarkRunner<discusy::http_client_coro>> runner;
        runner.emplace("co_composed Coro Client", io_ctx, client, test_url, total_requests, concurrency, [&](BenchmarkResult res) {
            results.push_back(res);
            io_ctx->stop();
        });
        runner->start();

        std::vector<std::thread> threads;
        for (int i = 0; i < 8; ++i) {
            threads.emplace_back([&] { io_ctx->run(); });
        }
        for (auto& t : threads) {
            if (t.joinable()) t.join();
        }
    }

    {
        discusy::ctx::io_context io_ctx{8};
        boost::asio::ssl::context ssl_ctx{boost::asio::ssl::context::tls_client};
        discusy::http_client_callback client{ssl_ctx, io_ctx};
        client.set_max_pool_size(500);

        std::optional<BenchmarkRunner<discusy::http_client_callback>> runner;
        runner.emplace("Callback Client", io_ctx, client, test_url, total_requests, concurrency, [&](BenchmarkResult res) {
            results.push_back(res);
            io_ctx->stop();
        });
        runner->start();

        std::vector<std::thread> threads;
        for (int i = 0; i < 8; ++i) {
            threads.emplace_back([&] { io_ctx->run(); });
        }
        for (auto& t : threads) {
            if (t.joinable()) t.join();
        }
    }

    {
        discusy::ctx::io_context io_ctx{8};
        boost::asio::ssl::context ssl_ctx{boost::asio::ssl::context::tls_client};
        discusy::http_client_coro client{ssl_ctx, io_ctx};
        client.set_max_pool_size(500);

        std::optional<BenchmarkRunner<discusy::http_client_coro>> runner;
        runner.emplace("co_composed Coro Client", io_ctx, client, test_url, total_requests, concurrency, [&](BenchmarkResult res) {
            results.push_back(res);
            io_ctx->stop();
        });
        runner->start();

        std::vector<std::thread> threads;
        for (int i = 0; i < 8; ++i) {
            threads.emplace_back([&] { io_ctx->run(); });
        }
        for (auto& t : threads) {
            if (t.joinable()) t.join();
        }
    }

    server.stop();

    // Summary Comparison Table
    std::println("\n=========================================================================");
    std::println("                     BENCHMARK COMPARISON SUMMARY                        ");
    std::println("=========================================================================");
    std::println("{:<25} | {:<12} | {:<16} | {:<10} | {:<10} | {:<10}", 
                 "Implementation", "Time (s)", "Throughput (r/s)", "p50 (us)", "p95 (us)", "p99 (us)");
    std::println("-------------------------------------------------------------------------");
    for (const auto& r : results) {
        std::println("{:<25} | {:<12.3f} | {:<16.2f} | {:<10.2f} | {:<10.2f} | {:<10.2f}",
                     r.name, r.total_sec, r.throughput, r.p50_us, r.p95_us, r.p99_us);
    }
    std::println("=========================================================================");

    #ifdef DISCUSY_USE_MIMALLOC
    mi_stats_print(nullptr);
    #endif

    return 0;
}