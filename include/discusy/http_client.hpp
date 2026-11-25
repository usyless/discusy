#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <flat_map>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

#include <boost/asio.hpp>
#include <boost/asio/cancel_after.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/unordered/concurrent_flat_map.hpp>
#include <boost/url.hpp>

#include "rate_limiter.hpp"
#include "urls.hpp"
#include "io_context.hpp"
#include "log.hpp" // IWYU pragma: keep
#include "asio_helpers.hpp"
#include "types.hpp"

namespace discusy {

namespace http {
    using method = boost::beast::http::verb;
    using cookies = std::flat_map<std::string, std::string>;

    struct response {
        int status_code{0};
        std::string body{};
        http::method method{http::method::get};
        std::optional<http::headers> headers{};
        std::optional<http::cookies> cookies{};
        std::optional<std::size_t> redirect_count{};
        std::optional<std::string> final_url{};

        [[nodiscard]] constexpr bool ok() const noexcept {
            return status_code >= 200 && status_code < 300;
        }

        [[nodiscard]] constexpr bool is_informational() const noexcept {
            return status_code >= 100 && status_code < 200;
        }

        [[nodiscard]] constexpr bool is_success() const noexcept {
            return ok();
        }

        [[nodiscard]] constexpr bool is_redirect() const noexcept {
            return status_code >= 300 && status_code < 400;
        }

        [[nodiscard]] constexpr bool is_client_error() const noexcept {
            return status_code >= 400 && status_code < 500;
        }

        [[nodiscard]] constexpr bool is_server_error() const noexcept {
            return status_code >= 500 && status_code < 600;
        }

        [[nodiscard]] constexpr bool is_error() const noexcept {
            return status_code >= 400 && status_code < 600;
        }

        [[nodiscard]] explicit constexpr operator bool() const noexcept {
            return ok();
        }
    };

    [[nodiscard]] constexpr bool is_snowflake(const std::string_view str) noexcept {
        return !str.empty() && std::ranges::all_of(str, [](const char c) noexcept {
            return c >= '0' && c <= '9';
        });
    }

    [[nodiscard]] std::string generate_route_key(http::method m, std::string_view path);

    struct parsed_url {
        bool is_ssl{true};
        bool host_is_name{true};
        std::string host;
        std::string host_header;
        std::string port;
        std::string target;
    };

    [[nodiscard]] std::optional<parsed_url> parse_http_url(const boost::urls::url_view_base& uv);
    [[nodiscard]] std::optional<parsed_url> parse_http_url(std::string_view url_str);

    using plain_stream_t = ctx::io_context::strand_tcp_stream_t;
    using ssl_stream_t = boost::asio::ssl::stream<plain_stream_t>;
    using stream_t = std::variant<plain_stream_t, ssl_stream_t>;

    [[nodiscard]] inline plain_stream_t& get_lowest_layer(stream_t& s) noexcept {
        if (auto* plain = std::get_if<plain_stream_t>(&s)) {
            return *plain;
        }
        return boost::beast::get_lowest_layer(*std::get_if<ssl_stream_t>(&s));
    }

    [[nodiscard]] inline ctx::io_context::strand_t get_stream_executor(stream_t& s) noexcept {
        if (auto* plain = std::get_if<plain_stream_t>(&s)) {
            return plain->get_executor();
        }
        return std::get_if<ssl_stream_t>(&s)->get_executor();
    }

    struct pooled_connection {
        std::unique_ptr<stream_t> stream;
        boost::beast::flat_buffer buffer;
        std::chrono::steady_clock::time_point last_used{};
    };

    struct pool_endpoint_key {
        std::string host;
        std::string port;
        bool is_ssl{true};

        [[nodiscard]] bool operator==(const pool_endpoint_key& other) const noexcept = default;
    };

    struct pool_endpoint_hash {
        using is_transparent = void;

        [[nodiscard]] static std::size_t combine(const std::string_view host, const std::string_view port, const bool is_ssl) noexcept {
            static constexpr auto magic = static_cast<std::size_t>(0x9e3779b97f4a7c15ULL);
            std::size_t seed = std::hash<std::string_view>{}(host);
            seed ^= std::hash<std::string_view>{}(port) + magic + (seed << 6U) + (seed >> 2U);
            seed ^= std::hash<bool>{}(is_ssl) + magic + (seed << 6U) + (seed >> 2U);
            return seed;
        }

        [[nodiscard]] std::size_t operator()(const pool_endpoint_key& k) const noexcept {
            return combine(k.host, k.port, k.is_ssl);
        }

        [[nodiscard]] std::size_t operator()(const std::tuple<std::string_view, std::string_view, bool>& t) const noexcept {
            return combine(std::get<0>(t), std::get<1>(t), std::get<2>(t));
        }
    };

    struct pool_endpoint_equal {
        using is_transparent = void;

        [[nodiscard]] bool operator()(const pool_endpoint_key& a, const pool_endpoint_key& b) const noexcept {
            return a.host == b.host && a.port == b.port && a.is_ssl == b.is_ssl;
        }

        [[nodiscard]] bool operator()(const pool_endpoint_key& a, const std::tuple<std::string_view, std::string_view, bool>& b) const noexcept {
            return a.host == std::get<0>(b) && a.port == std::get<1>(b) && a.is_ssl == std::get<2>(b);
        }

        [[nodiscard]] bool operator()(const std::tuple<std::string_view, std::string_view, bool>& a, const pool_endpoint_key& b) const noexcept {
            return std::get<0>(a) == b.host && std::get<1>(a) == b.port && std::get<2>(a) == b.is_ssl;
        }
    };

    struct options {
        boost::optional<std::uint64_t> body_limit{25 * 1024 * 1024}; // 25 MB
        std::chrono::seconds request_timeout{30};

        bool include_headers{false};
        bool include_cookies{false};
        bool follow_redirects{false};
        std::size_t max_redirects{5};
        bool include_redirect_info{false};
        bool allow_http_downgrade{false};
    };
}

class http_client_base {
public:
    using headers = http::headers;
    using cookies = http::cookies;

    boost::asio::ssl::context& ssl_ctx_;
    ctx::io_context& io_ctx_;
    rate_limiter rate_limiter_;

protected:
    static constexpr std::size_t POOL_BUFFER_SHRINK_THRESHOLD = 64UZ * 1024;
    static constexpr std::chrono::seconds eviction_timeout_{107};
    static constexpr std::chrono::seconds max_idle_time_{117};

    struct eviction_state {
        ctx::io_context::strand_t strand;
        ctx::io_context::strand_timer_t timer;
        std::atomic<bool> active{false};

        explicit eviction_state(ctx::io_context::strand_t s)
            : strand{std::move(s)}, timer{strand} {}
    };

    using pooled_conn_list = boost::container::small_vector<http::pooled_connection, 4>;
    boost::unordered::concurrent_flat_map<http::pool_endpoint_key, pooled_conn_list, http::pool_endpoint_hash, http::pool_endpoint_equal> connection_pool_;
    std::atomic<std::size_t> max_pool_size_{256};

    std::mutex eviction_mtx_;
    std::shared_ptr<eviction_state> eviction_state_;

    void evict_idle_connections();
    void cancel_eviction_timer();

public:
    http_client_base(boost::asio::ssl::context& ssl_ctx, ctx::io_context& ctx);

    http_client_base(const http_client_base&) = delete;
    http_client_base& operator=(const http_client_base&) = delete;
    http_client_base(http_client_base&&) = delete;
    http_client_base& operator=(http_client_base&&) = delete;

    ~http_client_base() noexcept;

    std::optional<http::pooled_connection> checkout_connection(std::string_view host, std::string_view port, bool is_ssl);
    bool checkin_connection(std::string_view host, std::string_view port, bool is_ssl, http::pooled_connection& conn);
    bool checkin_connection(std::string_view host, std::string_view port, bool is_ssl, http::pooled_connection&& conn);

    // Call after starting or restarting the io_context.
    void start();
    void stop();

    void set_max_pool_size(const std::size_t max_size) noexcept {
        max_pool_size_.store(max_size, std::memory_order_relaxed);
    }

    [[nodiscard]] std::size_t get_max_pool_size() const noexcept {
        return max_pool_size_.load(std::memory_order_relaxed);
    }
};

class http_client_callback final : public http_client_base {
private:
    static constexpr std::chrono::seconds ssl_shutdown_timeout_{2};

    struct completion_base {
        completion_base() = default;
        completion_base(const completion_base&) = delete;
        completion_base& operator=(const completion_base&) = delete;
        completion_base(completion_base&&) = delete;
        completion_base& operator=(completion_base&&) = delete;
        virtual ~completion_base() noexcept = default;
        virtual void complete_and_destroy(boost::system::error_code ec, http::response res) = 0;
        virtual void destroy() noexcept = 0;
    };

    template <typename Handler>
    struct completion_holder final : completion_base {
        using alloc_t = std::decay_t<decltype(
            boost::asio::get_associated_allocator(std::declval<const Handler&>(), boost::asio::recycling_allocator<void>{})
        )>;
        using exec_t = std::decay_t<decltype(
            boost::asio::get_associated_executor(std::declval<const Handler&>(), std::declval<ctx::io_context::executor_t>())
        )>;
        using rebind_alloc_t = std::allocator_traits<alloc_t>::template rebind_alloc<completion_holder>;

        Handler handler;
        alloc_t alloc;
        exec_t ex;
        boost::asio::executor_work_guard<exec_t> work_guard;

        completion_holder(Handler&& h, const ctx::io_context::executor_t& default_ex)
            : handler{std::move(h)},
              alloc{boost::asio::get_associated_allocator(handler, boost::asio::recycling_allocator<void>{})},
              ex{boost::asio::get_associated_executor(handler, default_ex)},
              work_guard{boost::asio::make_work_guard(ex)} {}

        static completion_holder* create(Handler&& h, const ctx::io_context::executor_t& default_ex) {
            const auto base_alloc = boost::asio::get_associated_allocator(h, boost::asio::recycling_allocator<void>{});
            rebind_alloc_t ra{base_alloc};
            completion_holder* p = std::allocator_traits<rebind_alloc_t>::allocate(ra, 1);
            try {
                std::allocator_traits<rebind_alloc_t>::construct(ra, p, std::move(h), default_ex);
            } catch (...) {
                std::allocator_traits<rebind_alloc_t>::deallocate(ra, p, 1);
                throw;
            }
            return p;
        }

        void destroy() noexcept override final {
            rebind_alloc_t ra{alloc};
            std::allocator_traits<rebind_alloc_t>::destroy(ra, this);
            std::allocator_traits<rebind_alloc_t>::deallocate(ra, this, 1);
        }

        void complete_and_destroy(const boost::system::error_code ec, http::response res) override final {
            struct holder_guard {
                completion_holder* ptr;
                explicit holder_guard(completion_holder* p) noexcept : ptr{p} {}
                holder_guard(holder_guard&& other) noexcept : ptr{std::exchange(other.ptr, nullptr)} {}
                holder_guard(const holder_guard&) = delete;
                holder_guard& operator=(const holder_guard&) = delete;
                holder_guard& operator=(holder_guard&&) = delete;
                ~holder_guard() noexcept {
                    if (ptr != nullptr) ptr->destroy();
                }
            };

            const auto local_ex = ex;
            const auto local_alloc = alloc;
            boost::asio::dispatch(
                local_ex,
                boost::asio::bind_allocator(
                    local_alloc,
                    [g = holder_guard{this}, ec, res = std::move(res)] mutable {
                        auto* self_h = g.ptr;
                        auto h = std::move(self_h->handler); // NOLINT(misc-const-correctness)
                        auto slot = boost::asio::get_associated_cancellation_slot(h);
                        if (slot.is_connected()) slot.clear();
                        auto wg = std::move(self_h->work_guard);
                        // Release holder allocated with the associated allocator before invoking handler.
                        g.ptr = nullptr;
                        self_h->destroy();
                        wg.reset();
                        std::move(h)(ec, std::move(res));
                    }
                )
            );
        }
    };

    struct request_state;

    static void start_request_state(
        completion_base* comp,
        boost::asio::cancellation_slot slot,
        http_client_callback* self,
        bool is_api,
        http::method m,
        std::string url,
        http::parsed_url parsed,
        std::string body,
        headers req_headers,
        http::options opts
    );

    template <bool Api, discusy::asio::ctf<http::response> CompletionToken>
    auto request_impl_(const http::method m, std::string&& url, std::string&& body, headers&& req_headers, http::options&& opts, CompletionToken&& token) {
        return boost::asio::async_initiate<CompletionToken, void(boost::system::error_code, http::response)>(
            [](discusy::asio::chf<http::response> auto&& handler, http_client_callback* self, const http::method m, auto&& url, auto&& body, auto&& req_headers, auto&& opts) {
                auto parsed = http::parse_http_url(url);
                if (!parsed) {
                    const auto ex_imm = boost::asio::get_associated_immediate_executor(handler, self->io_ctx_.executor_);
                    boost::asio::dispatch(
                        ex_imm,
                        boost::asio::append(
                            std::forward<decltype(handler)>(handler),
                            boost::asio::error::make_error_code(boost::asio::error::invalid_argument),
                            http::response{}
                        )
                    );
                    return;
                }

                using handler_t = std::decay_t<decltype(handler)>;
                auto* comp = completion_holder<handler_t>::create(
                    std::forward<decltype(handler)>(handler),
                    self->io_ctx_.executor_
                );
                const auto slot = boost::asio::get_associated_cancellation_slot(comp->handler);
                start_request_state(
                    comp,
                    slot,
                    self,
                    Api,
                    m,
                    std::forward<decltype(url)>(url),
                    std::move(*parsed),
                    std::forward<decltype(body)>(body),
                    std::forward<decltype(req_headers)>(req_headers),
                    std::forward<decltype(opts)>(opts)
                );
            },
            token,
            this, m, std::move(url), std::move(body), std::move(req_headers), std::move(opts)
        );
    }

public:
    using http_client_base::http_client_base;

    template <discusy::asio::ctf<http::response> CompletionToken = ctx::io_context::dct_t>
    auto request(const http::method m, std::string url, std::string body = {}, headers headers = {}, http::options opts = http::options(), CompletionToken&& token = ctx::io_context::dct_t()) {
        return request_impl_<false>(
            m, std::move(url), std::move(body), std::move(headers), std::move(opts),
            std::forward<CompletionToken>(token)
        );
    }

    template <discusy::asio::ctf<http::response> CompletionToken = ctx::io_context::dct_t>
    auto api_request(const http::method m, std::string url, std::string body = {}, headers headers = {}, http::options opts = http::options(), CompletionToken&& token = ctx::io_context::dct_t()) {
        return request_impl_<true>(
            m, std::move(url), std::move(body), std::move(headers), std::move(opts),
            std::forward<CompletionToken>(token)
        );
    }
};

using http_client = http_client_callback;

}
