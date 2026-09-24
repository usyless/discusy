#pragma once

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <string>
#include <tuple>
#include <variant>

#include <boost/asio.hpp>
#include <boost/asio/co_composed.hpp>
#include <boost/asio/cancel_after.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/url.hpp>
#include <boost/unordered/concurrent_flat_map.hpp>
#include <boost/container/small_vector.hpp>

#include <glaze/glaze.hpp>

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
        int status_code;
        std::string body;
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

    [[nodiscard]] inline bool is_snowflake(const std::string_view str) noexcept {
        return !str.empty() && std::ranges::all_of(str, [](unsigned char c) { 
            return std::isdigit(c);
        });
    }

    [[nodiscard]] inline std::string generate_route_key(http::method m, std::string_view path) {
        path = path.substr(ulp::str::strlen(urls::REST_BASE) + 1);
        boost::container::small_vector<std::string_view, 16> segments;
        ulp::str::split_by_for_each(path, '/', [&](std::string_view part) {
            segments.emplace_back(part);
        });

        const auto method_name = boost::beast::http::to_string(m);
        std::string route_key;
        route_key.reserve(method_name.size() + 1 + path.size());
        route_key.append(method_name);
        
        for (size_t i = 0; i < segments.size(); ++i) {
            route_key += '/';
            
            // If the segment is a numeric ID, we need to check if it's a top-level resource
            if (is_snowflake(segments[i])) {
                if (i > 0) {
                    const auto prev = segments[i - 1];
                    if (prev == "guilds" || prev == "channels" || prev == "webhooks") {
                        goto keep_id;
                    }
                }
                // Path structure: /webhooks/{webhook_id}/{webhook_token}
                if (i > 1 && segments[i - 2] == "webhooks") {
                    goto keep_id;
                }

                route_key += ":id";
                continue;
                keep_id:
                route_key += segments[i];
            } else {
                route_key += segments[i];
            }
        }

        #ifdef DISCUSY_LOGGING
        log::Logger{}("Route key {} for path {}", route_key, path);
        #endif

        return route_key;
    }

    using plain_stream_t = ctx::io_context::strand_tcp_stream_t;
    using ssl_stream_t = boost::asio::ssl::stream<plain_stream_t>;
    using stream_t = std::variant<plain_stream_t, ssl_stream_t>;

    [[nodiscard]] inline plain_stream_t& get_lowest_layer(stream_t& s) noexcept {
        return std::visit([](auto& stream) -> plain_stream_t& {
            return boost::beast::get_lowest_layer(stream);
        }, s);
    }

    [[nodiscard]] inline ctx::io_context::strand_t get_stream_executor(stream_t& s) noexcept {
        return std::visit([](auto& stream) {
            return stream.get_executor();
        }, s);
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

        [[nodiscard]] std::size_t operator()(const pool_endpoint_key& k) const noexcept {
            std::size_t h1 = std::hash<std::string_view>{}(k.host);
            std::size_t h2 = std::hash<std::string_view>{}(k.port);
            std::size_t h3 = std::hash<bool>{}(k.is_ssl);
            return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2)) ^ (h3 + 0x9e3779b9 + (h2 << 6) + (h2 >> 2));
        }

        [[nodiscard]] std::size_t operator()(const std::tuple<std::string_view, std::string_view, bool>& t) const noexcept {
            std::size_t h1 = std::hash<std::string_view>{}(std::get<0>(t));
            std::size_t h2 = std::hash<std::string_view>{}(std::get<1>(t));
            std::size_t h3 = std::hash<bool>{}(std::get<2>(t));
            return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2)) ^ (h3 + 0x9e3779b9 + (h2 << 6) + (h2 >> 2));
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
    using pooled_conn_list = boost::container::small_vector<http::pooled_connection, 4>;
    boost::unordered::concurrent_flat_map<http::pool_endpoint_key, pooled_conn_list, http::pool_endpoint_hash, http::pool_endpoint_equal> connection_pool_;
    std::atomic<std::size_t> max_pool_size_{256};

    static constexpr std::chrono::seconds eviction_timeout_{107};
    ctx::io_context::executor_timer_t eviction_timer_;

    void evict_idle_connections() {
        static constexpr auto max_idle_time = std::chrono::seconds{117};
        const auto now = std::chrono::steady_clock::now();

        pooled_conn_list connections_to_close;
        connection_pool_.erase_if([&](auto& item) {
            auto& connections = item.second;
            for (auto it = connections.begin(); it != connections.end(); ) {
                if ((now - it->last_used) > max_idle_time) {
                    if (it->stream) connections_to_close.emplace_back(std::move(*it));
                    it = connections.erase(it);
                } else {
                    ++it;
                }
            }
            return connections.empty();
        });

        for (auto& conn : connections_to_close) {
            boost::beast::error_code ignored_ec;
            http::get_lowest_layer(*conn.stream).socket().close(ignored_ec);
        }
    }

public:
    std::optional<http::pooled_connection> checkout_connection(const std::string_view host, const std::string_view port, bool is_ssl) {
        std::optional<http::pooled_connection> conn;
        connection_pool_.visit(std::make_tuple(host, port, is_ssl), [&conn](auto& entry) {
            auto& connections = entry.second;
            if (!connections.empty()) {
                conn.emplace(std::move(connections.back()));
                connections.pop_back();
            }
        });
        return conn;
    }

    bool checkin_connection(const std::string_view host, const std::string_view port, bool is_ssl, http::pooled_connection conn) {
        if (!conn.stream) return false;
        auto& lowest = http::get_lowest_layer(*conn.stream);
        if (!lowest.socket().is_open()) return false;
        
        lowest.expires_never();
        conn.last_used = std::chrono::steady_clock::now();
        const auto limit = max_pool_size_.load(std::memory_order_relaxed);

        bool inserted = false;
        bool visited = connection_pool_.visit(std::tuple{host, port, is_ssl}, [&](auto& entry) {
            if (entry.second.size() < limit) {
                entry.second.emplace_back(std::move(conn));
                inserted = true;
            }
        }) != 0UZ;

        if (!visited) {
            pooled_conn_list vec;
            vec.emplace_back(std::move(conn));
            inserted = connection_pool_.emplace(http::pool_endpoint_key{.host = std::string(host), .port = std::string(port), .is_ssl = is_ssl}, std::move(vec));
        }
        return inserted;
    }

    http_client_base(boost::asio::ssl::context& ssl_ctx, ctx::io_context& ctx) 
        : ssl_ctx_{ssl_ctx}, io_ctx_{ctx}, eviction_timer_{io_ctx_.executor_} {}

    // call after stopping and re-starting io context
    void start() {
        connection_pool_.clear();

        eviction_timer_.expires_after(eviction_timeout_);
        eviction_timer_.async_wait([i = this](this auto&& self, boost::system::error_code ec) -> void {
            if (ec) return;

            i->evict_idle_connections();
            i->eviction_timer_.expires_after(discusy::http_client_base::eviction_timeout_);
            i->eviction_timer_.async_wait(self);
        });
    }

    void stop() {
        // eviction timer not cancelled as this is only called when the io context is stopped, so cancelled anyway
        connection_pool_.clear();
    }

    void set_max_pool_size(std::size_t max_size) noexcept {
        max_pool_size_.store(max_size, std::memory_order_relaxed);
    }

    [[nodiscard]] std::size_t get_max_pool_size() const noexcept {
        return max_pool_size_.load(std::memory_order_relaxed);
    }
};

class http_client_callback : public http_client_base {
private:
    template <bool Api, typename Handler>
    struct request_state {
        Handler handler;
        http_client_callback* self;
        http::method current_method;
        std::string url;
        boost::urls::url_view url_view;
        
        std::string body;
        headers req_headers;
        http::options opts;
        std::atomic<bool> completed{false};

        boost::asio::cancellation_type cancelled_type{boost::asio::cancellation_type::none};
        boost::asio::cancellation_signal child_signal;

        ctx::io_context::strand_t strand_;
        std::mutex strand_mtx_;

        std::string route_key;
        std::string host;
        std::string port;
        std::string target;
        bool is_ssl{true};

        std::unique_ptr<http::stream_t> stream;
        boost::beast::flat_buffer buffer;
        bool is_reused{false};

        ctx::io_context::strand_tcp_resolver_t resolver;
        ctx::io_context::strand_timer_t timer;

        std::optional<boost::beast::http::request<boost::beast::http::string_body>> req;
        std::optional<boost::beast::http::response_parser<boost::beast::http::string_body>> parser;

        std::size_t redirect_count{0};
        http::cookies accumulated_cookies;

        decltype(
            boost::asio::get_associated_allocator(handler, boost::asio::recycling_allocator<void>{})
        ) allocator;

        decltype(
            boost::asio::make_work_guard(boost::asio::get_associated_executor(handler, std::declval<ctx::io_context::executor_t>()))
        ) work_guard_;

        request_state(auto&& h, http_client_callback* s, http::method m, auto&& u, auto&& b, auto&& hdrs, auto&& o)
            : handler{std::forward<decltype(h)>(h)},
              self{s},
              current_method{m},
              strand_{s->io_ctx_.make_strand()},
              url{std::forward<decltype(u)>(u)},
              body{std::forward<decltype(b)>(b)},
              req_headers{std::forward<decltype(hdrs)>(hdrs)},
              opts{std::forward<decltype(o)>(o)},
              resolver{strand_},
              timer{strand_},
              allocator{boost::asio::get_associated_allocator(handler, boost::asio::recycling_allocator<void>{})},
              work_guard_{boost::asio::make_work_guard(boost::asio::get_associated_executor(handler, s->io_ctx_.executor_))} {}
        
        bool is_cancelled() const noexcept {
            return cancelled_type != boost::asio::cancellation_type::none;
        }

        void handle_cancel(std::shared_ptr<request_state> self_ptr, boost::asio::cancellation_type type) {
            if (type == boost::asio::cancellation_type::none) return;

            boost::asio::post(get_strand(), [self_ptr = std::move(self_ptr), type]() {
                self_ptr->cancelled_type = type;
                self_ptr->child_signal.emit(type);
            });
        }

        ctx::io_context::strand_t get_strand() {
            std::scoped_lock lock{strand_mtx_};
            return strand_;
        }

        auto get_executor() {
            return boost::asio::get_associated_executor(handler, self->io_ctx_.executor_);
        }

        auto get_immediate_executor() {
            return boost::asio::get_associated_immediate_executor(handler, self->io_ctx_.executor_);
        }

        void complete(boost::system::error_code ec, http::response&& res = {}) {
            auto slot = boost::asio::get_associated_cancellation_slot(handler);
            if (slot.is_connected()) slot.clear();
            work_guard_.reset();
            std::move(handler)(ec, std::move(res));
        }

        void finish(std::shared_ptr<request_state> self_ptr, boost::system::error_code ec, http::response res = {}) {
            if (completed.exchange(true, std::memory_order_acq_rel)) return;

            auto ex = get_executor();

            boost::asio::dispatch(ex,
                boost::asio::bind_allocator(allocator, [self_ptr = std::move(self_ptr), ec, res = std::move(res)]() mutable {
                    self_ptr->complete(ec, std::move(res));
                })
            );
        }

        void operation_cancelled(std::shared_ptr<request_state> self_ptr) {
            if (stream) {
                boost::beast::error_code ignored_ec;
                http::get_lowest_layer(*stream).socket().close(ignored_ec);
                stream.reset();
            }
            finish(std::move(self_ptr), boost::asio::error::operation_aborted);
        }

        void early_finish(std::shared_ptr<request_state> self_ptr, boost::system::error_code ec, http::response res = {}) {
            if (completed.exchange(true, std::memory_order_acq_rel)) return;

            auto ex_imm = get_immediate_executor();

            boost::asio::dispatch(ex_imm,
                boost::asio::bind_allocator(allocator, [self_ptr = std::move(self_ptr), ec, res = std::move(res)]() mutable {
                    self_ptr->complete(ec, std::move(res));
                })
            );
        }

        void init(std::shared_ptr<request_state> self_ptr) {
            auto url_result = boost::urls::parse_uri(url);
            if (!url_result) {
                early_finish(std::move(self_ptr), boost::asio::error::invalid_argument);
                return;
            }

            url_view = *url_result;

            is_ssl = (url_view.scheme() == "https");
            host.assign(url_view.host());
            port.assign(url_view.has_port() ? std::string(url_view.port()) : (is_ssl ? "443" : "80"));
            target.assign(url_view.encoded_resource());
            if (target.empty()) target.assign("/");

            boost::asio::post(strand_, boost::asio::bind_allocator(allocator, [self_ptr = std::move(self_ptr)]() mutable {
                auto ptr = self_ptr.get();
                ptr->start(std::move(self_ptr));
            }));
        }

        void start(std::shared_ptr<request_state> self_ptr) {
            #ifdef DISCUSY_LOGGING
            log::Logger{}("Making request to {} with body {} and headers {}", url, body, req_headers);
            #endif

            if (is_cancelled()) return operation_cancelled(std::move(self_ptr));

            if constexpr (Api) {
                route_key = http::generate_route_key(current_method, url);

                if (!route_key.empty()) {
                    auto delay = self->rate_limiter_.get_delay(route_key);
                    if (delay.count() > 0) {
                        timer.expires_after(delay);
                        timer.async_wait(
                            boost::asio::bind_cancellation_slot(
                                child_signal.slot(),
                                boost::asio::bind_allocator(
                                    allocator,
                                    [self_ptr = std::move(self_ptr)](const boost::system::error_code& ec) mutable {
                                        auto ptr = self_ptr.get();
                                        if (ec) return ptr->finish(std::move(self_ptr), ec);
                                        ptr->step_loop_start(std::move(self_ptr));
                                    }
                                )
                            )
                        );
                        return;
                    }
                }
            }

            step_loop_start(std::move(self_ptr));
        }

        template <typename F>
        requires ( std::invocable<F&&, std::shared_ptr<request_state>&&> )
        void close_connection_then(std::shared_ptr<request_state> self_ptr, F&& then) {
            if (stream) {
                if (auto* ssl_str_ptr = std::get_if<http::ssl_stream_t>(stream.get()); ssl_str_ptr && is_ssl) {
                    boost::beast::get_lowest_layer(*ssl_str_ptr).expires_after(opts.request_timeout);
                    ssl_str_ptr->async_shutdown(
                        boost::asio::bind_cancellation_slot(
                            child_signal.slot(),
                            boost::asio::bind_allocator(
                                allocator,
                                [self_ptr = std::move(self_ptr), then = std::forward<F>(then)](const boost::system::error_code& ec) mutable {
                                    auto ptr = self_ptr.get();
                                    if (ec && ec != boost::asio::ssl::error::stream_truncated) {
                                        boost::beast::error_code ignored_ec;
                                        http::get_lowest_layer(*ptr->stream).socket().close(ignored_ec);
                                    }
                                    ptr->stream.reset();
                                    std::invoke(std::move(then), std::move(self_ptr));
                                }
                            )
                        )
                    );
                    return;
                }

                boost::beast::error_code ignored_ec;
                http::get_lowest_layer(*stream).socket().close(ignored_ec);
                stream.reset();
                std::invoke(std::forward<F>(then), std::move(self_ptr));
                return;
            }

            std::invoke(std::forward<F>(then), std::move(self_ptr));
        }

        void step_loop_start(std::shared_ptr<request_state> self_ptr) {
            if (is_cancelled()) return operation_cancelled(std::move(self_ptr));

            close_connection_then(std::move(self_ptr), [](std::shared_ptr<request_state> self_ptr) {
                auto ptr = self_ptr.get();
                ptr->step_acquire_connection(std::move(self_ptr));
            });
        }

        void step_acquire_connection(std::shared_ptr<request_state> self_ptr) {
            if (is_cancelled()) return operation_cancelled(std::move(self_ptr));

            if (auto conn = self->checkout_connection(host, port, is_ssl)) {
                #ifdef DISCUSY_LOGGING
                log::Logger{}("Re-using connection for request: {}", url);
                #endif
                std::optional<ctx::io_context::strand_t> old_strand;
                auto alloc = allocator;

                buffer = std::move(conn->buffer);
                is_reused = true;
                stream = std::move(conn->stream);
                {
                std::scoped_lock lock{strand_mtx_};
                old_strand.emplace(strand_);
                strand_ = http::get_stream_executor(*stream);

                // this is done to prevent cancellation slot race conditions
                // it is on purpose inside the mutex
                boost::asio::post(*old_strand, boost::asio::bind_allocator(alloc, [self_ptr = std::move(self_ptr), alloc, strand = strand_]() mutable {
                    boost::asio::dispatch(strand, boost::asio::bind_allocator(alloc, [self_ptr = std::move(self_ptr)]() mutable {
                        auto ptr = self_ptr.get();
                        ptr->step_write_request(std::move(self_ptr));
                    }));
                }));
                }
            } else {
                #ifdef DISCUSY_LOGGING
                log::Logger{}("Making new connection for request: {}", url);
                #endif
                is_reused = false;
                buffer.consume(buffer.size());

                if (is_ssl) {
                    stream = std::make_unique<http::stream_t>(std::in_place_type<http::ssl_stream_t>, strand_, self->ssl_ctx_);
                    auto& ssl_str = std::get<http::ssl_stream_t>(*stream);

                    if (!SSL_set_tlsext_host_name(ssl_str.native_handle(), host.c_str())) {
                        boost::system::error_code ec{static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category()};
                        return finish(std::move(self_ptr), ec ? ec : boost::asio::ssl::error::unspecified_system_error);
                    }

                    ssl_str.set_verify_callback(boost::asio::ssl::host_name_verification(host));
                } else {
                    stream = std::make_unique<http::stream_t>(std::in_place_type<http::plain_stream_t>, strand_);
                }

                step_resolve(std::move(self_ptr));
            }
        }

        void step_resolve(std::shared_ptr<request_state> self_ptr) {
            if (is_cancelled()) return operation_cancelled(std::move(self_ptr));

            resolver.async_resolve(
                host,
                port,
                boost::asio::cancel_after(
                    opts.request_timeout,
                    boost::asio::bind_cancellation_slot(
                        child_signal.slot(),
                        boost::asio::bind_allocator(
                            allocator,
                            [self_ptr = std::move(self_ptr)](const boost::system::error_code& res_ec, const boost::asio::ip::tcp::resolver::results_type& results) mutable {
                                auto ptr = self_ptr.get();
                                if (res_ec) {
                                    if (res_ec == boost::asio::error::operation_aborted) {
                                        return ptr->finish(std::move(self_ptr), boost::asio::error::timed_out);
                                    }
                                    #ifdef DISCUSY_LOGGING
                                    log::Logger{}("[http_client] resolve error for {}: {} ({})", ptr->host, res_ec.message(), res_ec.value());
                                    #endif
                                    return ptr->finish(std::move(self_ptr), res_ec);
                                }
                                ptr->step_connect(std::move(self_ptr), results);
                            }
                        )
                    )
                )
            );
        }

        void step_connect(std::shared_ptr<request_state> self_ptr, const boost::asio::ip::tcp::resolver::results_type& results) {
            if (is_cancelled()) return operation_cancelled(std::move(self_ptr));

            auto& lowest = http::get_lowest_layer(*stream);
            lowest.expires_after(opts.request_timeout);

            lowest.async_connect(
                results,
                boost::asio::bind_cancellation_slot(
                    child_signal.slot(),
                    boost::asio::bind_allocator(
                        allocator,
                        [self_ptr = std::move(self_ptr)](const boost::system::error_code& conn_ec, const boost::asio::ip::tcp::resolver::results_type::endpoint_type&) mutable {
                            auto ptr = self_ptr.get();
                            if (conn_ec == boost::asio::error::operation_aborted) {
                                return ptr->finish(std::move(self_ptr), boost::asio::error::timed_out);
                            }
                            if (conn_ec) {
                                #ifdef DISCUSY_LOGGING
                                log::Logger{}("[http_client] connect error to {}:{}: {} ({})", ptr->host, ptr->port, conn_ec.message(), conn_ec.value());
                                #endif
                                return ptr->finish(std::move(self_ptr), conn_ec);
                            }
                            boost::system::error_code opt_ec;
                            auto& sock = http::get_lowest_layer(*ptr->stream).socket();
                            sock.set_option(boost::asio::ip::tcp::no_delay(true), opt_ec);
                            sock.set_option(boost::asio::socket_base::keep_alive(true), opt_ec);
                            if (ptr->is_ssl) {
                                ptr->step_handshake(std::move(self_ptr));
                            } else {
                                ptr->step_write_request(std::move(self_ptr));
                            }
                        }
                    )
                )
            );
        }

        void step_handshake(std::shared_ptr<request_state> self_ptr) {
            if (is_cancelled()) return operation_cancelled(std::move(self_ptr));

            auto& lowest = http::get_lowest_layer(*stream);
            lowest.expires_after(opts.request_timeout);

            auto& ssl_str = std::get<http::ssl_stream_t>(*stream);
            ssl_str.async_handshake(
                boost::asio::ssl::stream_base::client,
                boost::asio::bind_cancellation_slot(
                    child_signal.slot(),
                    boost::asio::bind_allocator(
                        allocator,
                        [self_ptr = std::move(self_ptr)](const boost::system::error_code& hs_ec) mutable {
                            auto ptr = self_ptr.get();
                            if (hs_ec == boost::asio::error::operation_aborted) {
                                return ptr->finish(std::move(self_ptr), boost::asio::error::timed_out);
                            }
                            if (hs_ec) {
                                #ifdef DISCUSY_LOGGING
                                log::Logger{}("[http_client] handshake error with {}: {} ({})", ptr->host, hs_ec.message(), hs_ec.value());
                                #endif
                                return ptr->finish(std::move(self_ptr), hs_ec);
                            }
                            ptr->step_write_request(std::move(self_ptr));
                        }
                    )
                )
            );
        }

        void step_write_request(std::shared_ptr<request_state> self_ptr) {
            if (is_cancelled()) return operation_cancelled(std::move(self_ptr));

            req.emplace(current_method, target, 11);
            req->set(boost::beast::http::field::host, host);
            req->set(boost::beast::http::field::user_agent, "discusy/1.0");
            req->keep_alive(true);

            for (const auto& [k, v] : req_headers) {
                req->set(k, v);
            }

            if (!accumulated_cookies.empty()) {
                std::string cookie_header;
                for (const auto& [k, v] : accumulated_cookies) {
                    if (!cookie_header.empty()) cookie_header += "; ";
                    cookie_header += k;
                    cookie_header += '=';
                    cookie_header += v;
                }
                if (auto existing = req->find(boost::beast::http::field::cookie); existing != req->end()) {
                    req->set(boost::beast::http::field::cookie, ulp::str::concat_strings(existing->value(), "; ", cookie_header));
                } else {
                    req->set(boost::beast::http::field::cookie, cookie_header);
                }
            }

            if (current_method != boost::beast::http::verb::get && current_method != boost::beast::http::verb::delete_ && current_method != boost::beast::http::verb::head) {
                req->body() = body;
                req->prepare_payload();
            }

            auto& lowest = http::get_lowest_layer(*stream);
            lowest.expires_after(opts.request_timeout);

            auto write_handler = [self_ptr = std::move(self_ptr)](const boost::system::error_code& write_ec, std::size_t) mutable {
                auto ptr = self_ptr.get();
                if (write_ec) {
                    if (ptr->is_reused && (write_ec == boost::beast::http::error::end_of_stream || 
                                                write_ec == boost::asio::error::connection_reset ||
                                                write_ec == boost::asio::error::broken_pipe)) {
                        return ptr->step_loop_start(std::move(self_ptr));
                    }
                    if (write_ec == boost::asio::error::operation_aborted) {
                        return ptr->finish(std::move(self_ptr), boost::asio::error::timed_out);
                    }
                    return ptr->finish(std::move(self_ptr), write_ec);
                }
                ptr->step_read_response(std::move(self_ptr));
            };

            std::visit([&](auto& s) {
                boost::beast::http::async_write(
                    s,
                    *req,
                    boost::asio::bind_cancellation_slot(
                        child_signal.slot(),
                        boost::asio::bind_allocator(allocator, std::move(write_handler))
                    )
                );
            }, *stream);
        }

        void step_read_response(std::shared_ptr<request_state> self_ptr) {
            if (is_cancelled()) return operation_cancelled(std::move(self_ptr));

            auto& lowest = http::get_lowest_layer(*stream);
            lowest.expires_after(opts.request_timeout);

            parser.emplace();
            parser->body_limit(opts.body_limit);

            auto read_handler = [self_ptr = std::move(self_ptr)](const boost::system::error_code& read_ec, std::size_t) mutable {
                auto ptr = self_ptr.get();
                if (read_ec) {
                    if (ptr->is_reused && (read_ec == boost::beast::http::error::end_of_stream || 
                                                read_ec == boost::asio::error::connection_reset ||
                                                read_ec == boost::asio::ssl::error::stream_truncated)) {
                        return ptr->step_loop_start(std::move(self_ptr));
                    }
                    if (read_ec == boost::asio::error::operation_aborted) {
                        return ptr->finish(std::move(self_ptr), boost::asio::error::timed_out);
                    }
                    return ptr->finish(std::move(self_ptr), read_ec);
                }
                ptr->step_handle_response(std::move(self_ptr));
            };

            std::visit([&](auto& s) {
                boost::beast::http::async_read(
                    s,
                    buffer,
                    *parser,
                    boost::asio::bind_cancellation_slot(
                        child_signal.slot(),
                        boost::asio::bind_allocator(allocator, std::move(read_handler))
                    )
                );
            }, *stream);
        }

        void step_handle_response(std::shared_ptr<request_state> self_ptr) {
            if (is_cancelled()) return operation_cancelled(std::move(self_ptr));

            auto res = parser->release();
            const auto status = res.result_int();
            const bool is_redirect = (status == 301 || status == 302 || status == 303 || status == 307 || status == 308);

            if (opts.follow_redirects && is_redirect && (redirect_count < opts.max_redirects)) {
                auto loc_it = res.find(boost::beast::http::field::location);
                if (loc_it != res.end()) {
                    auto loc_ref = boost::urls::parse_uri_reference(loc_it->value());
                    if (loc_ref) {
                        boost::urls::url b_url = url_view;
                        auto resolved = b_url.resolve(*loc_ref);
                        if (resolved) {
                            boost::urls::url boost_url = std::move(b_url);
                            const bool new_is_ssl = (boost_url.scheme() == "https");

                            if (is_ssl && !new_is_ssl && !opts.allow_http_downgrade) {
                                #ifdef DISCUSY_LOGGING
                                log::Logger{}("[http_client] Prevented HTTPS to HTTP downgrade redirect from {} to {}", url, boost_url.buffer());
                                #endif
                                return finish(std::move(self_ptr), boost::asio::error::no_permission);
                            }

                            const auto new_host = boost_url.host();
                            const auto new_port = boost_url.has_port() ? boost_url.port() : (new_is_ssl ? "443" : "80");
                            const bool origin_changed = (is_ssl != new_is_ssl) || (host != new_host) || (port != new_port);

                            if (opts.include_cookies) {
                                for (const auto& field : res.base()) {
                                    if (field.name() == boost::beast::http::field::set_cookie || boost::beast::iequals(field.name_string(), "set-cookie")) {
                                        std::string_view val = field.value();
                                        auto semi = val.find(';');
                                        std::string_view pair = (semi != std::string_view::npos) ? val.substr(0, semi) : val;
                                        auto eq = pair.find('=');
                                        if (eq != std::string_view::npos) {
                                            static constexpr auto trim = [](std::string_view s) -> std::string_view {
                                                while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
                                                while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.remove_suffix(1);
                                                return s;
                                            };
                                            std::string_view name = trim(pair.substr(0, eq));
                                            std::string_view value = trim(pair.substr(eq + 1));
                                            if (!name.empty()) {
                                                accumulated_cookies.insert_or_assign(std::string(name), std::string(value));
                                            }
                                        }
                                    }
                                }
                            }

                            static constexpr auto is_hop_by_hop_header = [](std::string_view name) -> bool {
                                return boost::beast::iequals(name, "connection") ||
                                       boost::beast::iequals(name, "keep-alive") ||
                                       boost::beast::iequals(name, "proxy-authenticate") ||
                                       boost::beast::iequals(name, "proxy-authorization") ||
                                       boost::beast::iequals(name, "te") ||
                                       boost::beast::iequals(name, "trailer") ||
                                       boost::beast::iequals(name, "transfer-encoding") ||
                                       boost::beast::iequals(name, "upgrade") ||
                                       boost::beast::iequals(name, "host");
                            };
                            std::erase_if(req_headers, [](const auto& item) {
                                return is_hop_by_hop_header(item.first);
                            });

                            if (origin_changed) {
                                static constexpr auto is_sensitive_header = [](std::string_view name) -> bool {
                                    return boost::beast::iequals(name, "authorization") ||
                                           boost::beast::iequals(name, "cookie") ||
                                           boost::beast::iequals(name, "cookie2") ||
                                           boost::beast::iequals(name, "proxy-authorization") ||
                                           boost::beast::iequals(name, "www-authenticate") ||
                                           boost::beast::iequals(name, "x-api-key") ||
                                           boost::beast::iequals(name, "api-key") ||
                                           boost::beast::iequals(name, "x-auth-token") ||
                                           boost::beast::iequals(name, "auth-token") ||
                                           boost::beast::iequals(name, "token") ||
                                           boost::beast::iequals(name, "x-user-id") ||
                                           boost::beast::iequals(name, "x-csrf-token") ||
                                           boost::beast::iequals(name, "x-xsrf-token");
                                };

                                std::erase_if(req_headers, [](const auto& item) {
                                    return is_sensitive_header(item.first);
                                });
                                accumulated_cookies.clear();
                            }

                            if (res.keep_alive()) self->checkin_connection(host, port, is_ssl, {std::move(stream), std::move(buffer)});
                            buffer.consume(buffer.size());

                            ++redirect_count;
                            url.assign(boost_url.buffer());

                            const bool method_changed_to_get = (status == 303 || ((status == 301 || status == 302) && current_method != boost::beast::http::verb::get && current_method != boost::beast::http::verb::head));
                            if (method_changed_to_get) {
                                current_method = boost::beast::http::verb::get;
                                body.clear();

                                static constexpr auto is_payload_header = [](std::string_view name) -> bool {
                                    return boost::beast::iequals(name, "content-length") ||
                                           boost::beast::iequals(name, "content-type") ||
                                           boost::beast::iequals(name, "content-encoding") ||
                                           boost::beast::iequals(name, "content-language") ||
                                           boost::beast::iequals(name, "transfer-encoding");
                                };
                                std::erase_if(req_headers, [](const auto& item) {
                                    return is_payload_header(item.first);
                                });
                            }

                            auto url_result = boost::urls::parse_uri(url);
                            if (!url_result) {
                                return finish(std::move(self_ptr), boost::asio::error::invalid_argument);
                            }
                            url_view = *url_result;

                            is_ssl = new_is_ssl;
                            host.assign(url_view.host());
                            port.assign(url_view.has_port() ? url_view.port() : (is_ssl ? "443" : "80"));
                            target.assign(url_view.encoded_resource());
                            if (target.empty()) target.assign("/");

                            return step_loop_start(std::move(self_ptr));
                        }
                    }
                }
            }

            const bool repooled = res.keep_alive() && self->checkin_connection(host, port, is_ssl, {std::move(stream), std::move(buffer)});

            if constexpr (Api) {
                if (!route_key.empty()) {
                    self->rate_limiter_.update(route_key, res);

                    if (res.result_int() == 429) {
                        #ifdef DISCUSY_LOGGING
                        log::Logger{}("429 Rate limit hit for request: {}", url);
                        #endif

                        auto delay = self->rate_limiter_.handle_429(res.body());

                        if (!repooled && stream) {
                            close_connection_then(std::move(self_ptr), [delay](std::shared_ptr<request_state> self_ptr) {
                                auto ptr = self_ptr.get();
                                ptr->step_wait_429(std::move(self_ptr), delay);
                            });
                            return;
                        }

                        step_wait_429(std::move(self_ptr), delay);
                        return;
                    }
                }
                #ifdef DISCUSY_LOGGING
                log::Logger{}("Request to {} with body:\n{}", url, res.body());
                #endif
            }

            #ifdef DISCUSY_LOGGING
            log::Logger{}("Successful request to {} with code: {}", url, res.result_int());
            #endif

            http::response final_resp{
                .status_code = static_cast<int>(res.result_int()),
                .body = std::move(res.body()),
                .method = current_method,
            };

            if (opts.include_headers) {
                auto& hdrs = final_resp.headers.emplace();
                for (const auto& field : res.base()) {
                    hdrs.emplace(std::string(field.name_string()), std::string(field.value()));
                }
            }

            if (opts.include_cookies) {
                auto& cks = final_resp.cookies.emplace(std::move(accumulated_cookies));
                for (const auto& field : res.base()) {
                    if (field.name() == boost::beast::http::field::set_cookie || boost::beast::iequals(field.name_string(), "set-cookie")) {
                        std::string_view val = field.value();
                        auto semi = val.find(';');
                        std::string_view pair = (semi != std::string_view::npos) ? val.substr(0, semi) : val;
                        auto eq = pair.find('=');
                        if (eq != std::string_view::npos) {
                            static constexpr auto trim = [](std::string_view s) -> std::string_view {
                                while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
                                while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.remove_suffix(1);
                                return s;
                            };
                            std::string_view name = trim(pair.substr(0, eq));
                            std::string_view value = trim(pair.substr(eq + 1));
                            if (!name.empty()) {
                                cks.insert_or_assign(std::string(name), std::string(value));
                            }
                        }
                    }
                }
            }

            if (opts.include_redirect_info || opts.follow_redirects) {
                final_resp.redirect_count = redirect_count;
                final_resp.final_url = url;
            }

            if (!repooled && stream) {
                close_connection_then(self_ptr, [](std::shared_ptr<request_state>) {});
            }

            finish(std::move(self_ptr), boost::system::error_code{}, std::move(final_resp));
        }

        template <typename Rep, typename Period>
        void step_wait_429(std::shared_ptr<request_state> self_ptr, std::chrono::duration<Rep, Period> delay) {
            if (is_cancelled()) return operation_cancelled(std::move(self_ptr));

            timer.expires_after(delay);
            timer.async_wait(
                boost::asio::bind_cancellation_slot(
                    child_signal.slot(),
                    boost::asio::bind_allocator(
                        allocator,
                        [self_ptr = std::move(self_ptr)](const boost::system::error_code& ec) mutable {
                            auto ptr = self_ptr.get();
                            if (ec) return ptr->finish(std::move(self_ptr), boost::asio::error::timed_out);
                            ptr->step_loop_start(std::move(self_ptr));
                        }
                    )
                )
            );
        }
    };

    template <bool Api, discusy::asio::ctf<http::response> CompletionToken>
    auto request_impl_(http::method m, std::string&& url, std::string&& body, headers&& req_headers, http::options&& opts, CompletionToken&& token) {
        return boost::asio::async_initiate<CompletionToken, void(boost::system::error_code, http::response)>(
            [](discusy::asio::chf<http::response> auto&& handler, http_client_callback* self, http::method m, auto&& url, auto&& body, auto&& req_headers, auto&& opts) {
                using handler_t = std::decay_t<decltype(handler)>;
                using state_type = request_state<Api, handler_t>;

                auto alloc = boost::asio::get_associated_allocator(
                    handler,
                    boost::asio::recycling_allocator<void>{}
                );

                std::shared_ptr<state_type> state = std::allocate_shared<state_type>(
                    alloc,
                    std::forward<decltype(handler)>(handler),
                    self, m, std::forward<decltype(url)>(url), std::forward<decltype(body)>(body), std::forward<decltype(req_headers)>(req_headers), std::forward<decltype(opts)>(opts)
                );

                auto slot = boost::asio::get_associated_cancellation_slot(state->handler);

                if (slot.is_connected()) {
                    slot.assign([weak_state = std::weak_ptr<state_type>{state}, raw = state.get()](boost::asio::cancellation_type type) {
                        if (std::shared_ptr<state_type> st = weak_state.lock()) raw->handle_cancel(std::move(st), type);
                    });
                }

                state_type* raw = state.get();
                raw->init(std::move(state));
            },
            token,
            this, m, std::move(url), std::move(body), std::move(req_headers), std::move(opts)
        );
    }

public:
    using http_client_base::http_client_base;

    template <discusy::asio::ctf<http::response> CompletionToken = ctx::io_context::dct_t>
    auto request(http::method m, std::string url, std::string body = {}, headers headers = {}, http::options opts = {}, CompletionToken&& token = ctx::io_context::dct_t()) {
        return request_impl_<false>(
            m, std::move(url), std::move(body), std::move(headers), std::move(opts), 
            std::forward<CompletionToken>(token)
        );
    }

    template <discusy::asio::ctf<http::response> CompletionToken = ctx::io_context::dct_t>
    auto api_request(http::method m, std::string url, std::string body = {}, headers headers = {}, http::options opts = {}, CompletionToken&& token = ctx::io_context::dct_t()) {
        return request_impl_<true>(
            m, std::move(url), std::move(body), std::move(headers), std::move(opts), 
            std::forward<CompletionToken>(token)
        );
    }
};

// TODO: this client isn't used anwyay, but it runs on the system executor by default? it should probably recurse coroutines to switch strands
class http_client_coro : public http_client_base {
private:
    template <bool Api, discusy::asio::ctf<http::response> CompletionToken>
    auto request_impl_(http::method m, std::string url, std::string body, headers req_headers, http::options opts, CompletionToken&& token) {
        return boost::asio::async_initiate<CompletionToken, void(boost::system::error_code, http::response)>(
            boost::asio::co_composed<void(boost::system::error_code, http::response)>(
                [](auto state, http_client_coro* self, http::method current_method, std::string url, std::string body, headers req_headers, http::options opts) -> void {
                    try {
                        ctx::io_context::strand_t strand = self->io_ctx_.make_strand();
                        auto alloc = boost::asio::get_associated_allocator(
                            state.handler(),
                            boost::asio::recycling_allocator<void>{}
                        );

                        auto make_token = [&]() {
                            return boost::asio::bind_executor(
                                strand, 
                                boost::asio::bind_allocator(
                                    alloc,
                                    boost::asio::as_tuple(boost::asio::deferred)
                                )
                            );
                        };

                        #ifdef DISCUSY_LOGGING
                        log::Logger{}("Making request to {} with body {} and headers {}", url, body, req_headers);
                        #endif

                        [[maybe_unused]] std::string route_key;
                        if constexpr (Api) {
                            route_key = http::generate_route_key(current_method, url);

                            if (!route_key.empty()) {
                                auto delay = self->rate_limiter_.get_delay(route_key);
                                if (delay.count() > 0) {
                                    ctx::io_context::strand_timer_t timer(strand, delay);
                                    auto [ec] = co_await timer.async_wait(make_token());
                                    if (ec) co_return {ec, http::response{}};
                                }
                            }
                        }

                        auto url_result = boost::urls::parse_uri(url);
                        if (!url_result) co_return {boost::asio::error::invalid_argument, http::response{}};

                        boost::urls::url_view url_view = *url_result;
                        bool is_ssl = (url_view.scheme() == "https");
                        std::string host(url_view.host());
                        std::string port(url_view.has_port() ? std::string(url_view.port()) : (is_ssl ? "443" : "80"));
                        std::string target(url_view.encoded_resource());
                        if (target.empty()) target = "/";

                        std::size_t redirect_count = 0;
                        http::cookies accumulated_cookies;

                        std::unique_ptr<http::stream_t> stream;

                        while (true) {
                            boost::beast::flat_buffer buffer;
                            bool is_reused = false;

                            if (stream) {
                                if (auto* ssl_str_ptr = std::get_if<http::ssl_stream_t>(stream.get()); ssl_str_ptr && is_ssl) {
                                    boost::beast::get_lowest_layer(*ssl_str_ptr).expires_after(opts.request_timeout);
                                    const auto [ec] = co_await ssl_str_ptr->async_shutdown(make_token());
                                    if (ec && ec != boost::asio::ssl::error::stream_truncated) {
                                        boost::beast::error_code ignored_ec;
                                        boost::beast::get_lowest_layer(*ssl_str_ptr).socket().close(ignored_ec);
                                    }
                                } else {
                                    boost::beast::error_code ignored_ec;
                                    http::get_lowest_layer(*stream).socket().close(ignored_ec);
                                }
                                stream.reset();
                            }

                            if (auto conn = self->checkout_connection(host, port, is_ssl)) {
                                #ifdef DISCUSY_LOGGING
                                log::Logger{}("Re-using connection for request: {}", url);
                                #endif
                                stream = std::move(conn->stream);
                                buffer = std::move(conn->buffer);
                                is_reused = true;

                                auto stream_strand = http::get_stream_executor(*stream);
                                if (strand != stream_strand) {
                                    strand = stream_strand;
                                    co_await boost::asio::post(
                                        boost::asio::bind_executor(
                                            strand,
                                            boost::asio::bind_allocator(alloc, boost::asio::deferred)
                                        )
                                    );
                                }
                            } else {
                                #ifdef DISCUSY_LOGGING
                                log::Logger{}("Making new connection for request: {}", url);
                                #endif
                                is_reused = false;
                                buffer.consume(buffer.size());

                                if (is_ssl) {
                                    stream = std::make_unique<http::stream_t>(std::in_place_type<http::ssl_stream_t>, strand, self->ssl_ctx_);
                                    auto& ssl_str = std::get<http::ssl_stream_t>(*stream);

                                    if (!SSL_set_tlsext_host_name(ssl_str.native_handle(), host.c_str())) {
                                        boost::system::error_code ec{static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category()};
                                        co_return {ec ? ec : boost::asio::ssl::error::unspecified_system_error, http::response{}};
                                    }

                                    ssl_str.set_verify_callback(boost::asio::ssl::host_name_verification(host));
                                } else {
                                    stream = std::make_unique<http::stream_t>(std::in_place_type<http::plain_stream_t>, strand);
                                }

                                ctx::io_context::strand_tcp_resolver_t resolver(strand);

                                auto [res_ec, results] = co_await resolver.async_resolve(
                                    host, 
                                    port, 
                                    boost::asio::cancel_after(opts.request_timeout, make_token())
                                );
                                if (res_ec) {
                                    if (res_ec == boost::asio::error::operation_aborted) {
                                        co_return {boost::asio::error::timed_out, http::response{}};
                                    }
                                    #ifdef DISCUSY_LOGGING
                                    log::Logger{}("[http_client] resolve error for {}: {} ({})", host, res_ec.message(), res_ec.value());
                                    #endif
                                    co_return {res_ec, http::response{}};
                                }

                                auto& lowest = http::get_lowest_layer(*stream);
                                lowest.expires_after(opts.request_timeout);

                                auto [conn_ec, _] = co_await lowest.async_connect(results, make_token());
                                if (conn_ec == boost::asio::error::operation_aborted) {
                                    co_return {boost::asio::error::timed_out, http::response{}};
                                }
                                if (conn_ec) {
                                    #ifdef DISCUSY_LOGGING
                                    log::Logger{}("[http_client] connect error to {}:{}: {} ({})", host, port, conn_ec.message(), conn_ec.value());
                                    #endif
                                    co_return {conn_ec, http::response{}};
                                }

                                boost::system::error_code opt_ec;
                                auto& sock = lowest.socket();
                                sock.set_option(boost::asio::ip::tcp::no_delay(true), opt_ec);
                                sock.set_option(boost::asio::socket_base::keep_alive(true), opt_ec);

                                if (is_ssl) {
                                    lowest.expires_after(opts.request_timeout);
                                    auto& ssl_str = std::get<http::ssl_stream_t>(*stream);
                                    auto [hs_ec] = co_await ssl_str.async_handshake(boost::asio::ssl::stream_base::client, make_token());
                                    if (hs_ec == boost::asio::error::operation_aborted) {
                                        co_return {boost::asio::error::timed_out, http::response{}};
                                    }
                                    if (hs_ec) {
                                        #ifdef DISCUSY_LOGGING
                                        log::Logger{}("[http_client] handshake error with {}: {} ({})", host, hs_ec.message(), hs_ec.value());
                                        #endif
                                        co_return {hs_ec, http::response{}};
                                    }
                                }
                            }

                            boost::beast::http::request<boost::beast::http::string_body> req{current_method, target, 11};
                            req.set(boost::beast::http::field::host, host);
                            req.set(boost::beast::http::field::user_agent, "discusy/1.0");
                            req.keep_alive(true);

                            for (const auto& [k, v] : req_headers) {
                                req.set(k, v);
                            }

                            if (!accumulated_cookies.empty()) {
                                std::string cookie_header;
                                for (const auto& [k, v] : accumulated_cookies) {
                                    if (!cookie_header.empty()) cookie_header += "; ";
                                    cookie_header += k;
                                    cookie_header += '=';
                                    cookie_header += v;
                                }
                                if (auto existing = req.find(boost::beast::http::field::cookie); existing != req.end()) {
                                    req.set(boost::beast::http::field::cookie, ulp::str::concat_strings(existing->value(), "; ", cookie_header));
                                } else {
                                    req.set(boost::beast::http::field::cookie, cookie_header);
                                }
                            }

                            if (current_method != boost::beast::http::verb::get && current_method != boost::beast::http::verb::delete_ && current_method != boost::beast::http::verb::head) {
                                req.body() = body;
                                req.prepare_payload();
                            }

                            auto& lowest = http::get_lowest_layer(*stream);
                            lowest.expires_after(opts.request_timeout);

                            auto [write_ec, bytes_written] = is_ssl
                                ? co_await boost::beast::http::async_write(std::get<http::ssl_stream_t>(*stream), req, make_token())
                                : co_await boost::beast::http::async_write(std::get<http::plain_stream_t>(*stream), req, make_token());

                            if (write_ec) {
                                if (is_reused && (write_ec == boost::beast::http::error::end_of_stream || 
                                                write_ec == boost::asio::error::connection_reset ||
                                                write_ec == boost::asio::error::broken_pipe)) {
                                    continue;
                                }
                                if (write_ec == boost::asio::error::operation_aborted) {
                                    co_return {boost::asio::error::timed_out, http::response{}};
                                }
                                co_return {write_ec, http::response{}};
                            }

                            lowest.expires_after(opts.request_timeout);

                            boost::beast::http::response_parser<boost::beast::http::string_body> parser;
                            parser.body_limit(opts.body_limit);
                            auto [read_ec, bytes_read] = is_ssl
                                ? co_await boost::beast::http::async_read(std::get<http::ssl_stream_t>(*stream), buffer, parser, make_token())
                                : co_await boost::beast::http::async_read(std::get<http::plain_stream_t>(*stream), buffer, parser, make_token());

                            if (read_ec) {
                                if (is_reused && (read_ec == boost::beast::http::error::end_of_stream || 
                                                read_ec == boost::asio::error::connection_reset ||
                                                read_ec == boost::asio::ssl::error::stream_truncated)) {
                                    continue;
                                }
                                if (read_ec == boost::asio::error::operation_aborted) {
                                    co_return {boost::asio::error::timed_out, http::response{}};
                                }
                                co_return {read_ec, http::response{}};
                            }

                            auto res = parser.release();
                            const auto status = res.result_int();
                            const bool is_redirect = (status == 301 || status == 302 || status == 303 || status == 307 || status == 308);

                            if (opts.follow_redirects && is_redirect && (redirect_count < opts.max_redirects)) {
                                auto loc_it = res.find(boost::beast::http::field::location);
                                if (loc_it != res.end()) {
                                    auto loc_ref = boost::urls::parse_uri_reference(loc_it->value());
                                    if (loc_ref) {
                                        boost::urls::url b_url = url_view;
                                        auto resolved = b_url.resolve(*loc_ref);
                                        if (resolved) {
                                            boost::urls::url boost_url = std::move(b_url);
                                            const bool new_is_ssl = (boost_url.scheme() == "https");

                                            if (is_ssl && !new_is_ssl && !opts.allow_http_downgrade) {
                                                #ifdef DISCUSY_LOGGING
                                                log::Logger{}("[http_client] Prevented HTTPS to HTTP downgrade redirect from {} to {}", url, boost_url.buffer());
                                                #endif
                                                co_return {boost::asio::error::no_permission, http::response{}};
                                            }

                                            const auto new_host = boost_url.host();
                                            const auto new_port = boost_url.has_port() ? boost_url.port() : (new_is_ssl ? "443" : "80");
                                            const bool origin_changed = (is_ssl != new_is_ssl) || (host != new_host) || (port != new_port);

                                            if (opts.include_cookies) {
                                                for (const auto& field : res.base()) {
                                                    if (field.name() == boost::beast::http::field::set_cookie || boost::beast::iequals(field.name_string(), "set-cookie")) {
                                                        std::string_view val = field.value();
                                                        auto semi = val.find(';');
                                                        std::string_view pair = (semi != std::string_view::npos) ? val.substr(0, semi) : val;
                                                        auto eq = pair.find('=');
                                                        if (eq != std::string_view::npos) {
                                                            static constexpr auto trim = [](std::string_view s) -> std::string_view {
                                                                while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
                                                                while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.remove_suffix(1);
                                                                return s;
                                                            };
                                                            std::string_view name = trim(pair.substr(0, eq));
                                                            std::string_view value = trim(pair.substr(eq + 1));
                                                            if (!name.empty()) {
                                                                accumulated_cookies.insert_or_assign(std::string(name), std::string(value));
                                                            }
                                                        }
                                                    }
                                                }
                                            }

                                            static constexpr auto is_hop_by_hop_header = [](std::string_view name) -> bool {
                                                return boost::beast::iequals(name, "connection") ||
                                                       boost::beast::iequals(name, "keep-alive") ||
                                                       boost::beast::iequals(name, "proxy-authenticate") ||
                                                       boost::beast::iequals(name, "proxy-authorization") ||
                                                       boost::beast::iequals(name, "te") ||
                                                       boost::beast::iequals(name, "trailer") ||
                                                       boost::beast::iequals(name, "transfer-encoding") ||
                                                       boost::beast::iequals(name, "upgrade") ||
                                                       boost::beast::iequals(name, "host");
                                            };
                                            std::erase_if(req_headers, [](const auto& item) {
                                                return is_hop_by_hop_header(item.first);
                                            });

                                            if (origin_changed) {
                                                static constexpr auto is_sensitive_header = [](std::string_view name) -> bool {
                                                    return boost::beast::iequals(name, "authorization") ||
                                                           boost::beast::iequals(name, "cookie") ||
                                                           boost::beast::iequals(name, "cookie2") ||
                                                           boost::beast::iequals(name, "proxy-authorization") ||
                                                           boost::beast::iequals(name, "www-authenticate") ||
                                                           boost::beast::iequals(name, "x-api-key") ||
                                                           boost::beast::iequals(name, "api-key") ||
                                                           boost::beast::iequals(name, "x-auth-token") ||
                                                           boost::beast::iequals(name, "auth-token") ||
                                                           boost::beast::iequals(name, "token") ||
                                                           boost::beast::iequals(name, "x-user-id") ||
                                                           boost::beast::iequals(name, "x-csrf-token") ||
                                                           boost::beast::iequals(name, "x-xsrf-token");
                                                };

                                                std::erase_if(req_headers, [](const auto& item) {
                                                    return is_sensitive_header(item.first);
                                                });
                                                accumulated_cookies.clear();
                                            }

                                            if (res.keep_alive()) self->checkin_connection(host, port, is_ssl, {std::move(stream), std::move(buffer)});
                                            buffer.consume(buffer.size());

                                            ++redirect_count;
                                            url.assign(boost_url.buffer());

                                            const bool method_changed_to_get = (status == 303 || ((status == 301 || status == 302) && current_method != boost::beast::http::verb::get && current_method != boost::beast::http::verb::head));
                                            if (method_changed_to_get) {
                                                current_method = boost::beast::http::verb::get;
                                                body.clear();

                                                static constexpr auto is_payload_header = [](std::string_view name) -> bool {
                                                    return boost::beast::iequals(name, "content-length") ||
                                                           boost::beast::iequals(name, "content-type") ||
                                                           boost::beast::iequals(name, "content-encoding") ||
                                                           boost::beast::iequals(name, "content-language") ||
                                                           boost::beast::iequals(name, "transfer-encoding");
                                                };
                                                std::erase_if(req_headers, [](const auto& item) {
                                                    return is_payload_header(item.first);
                                                });
                                            }

                                            auto url_res = boost::urls::parse_uri(url);
                                            if (!url_res) {
                                                co_return {boost::asio::error::invalid_argument, http::response{}};
                                            }
                                            url_view = *url_res;

                                            is_ssl = new_is_ssl;
                                            host.assign(url_view.host());
                                            port.assign(url_view.has_port() ? url_view.port() : (is_ssl ? "443" : "80"));
                                            target.assign(url_view.encoded_resource());
                                            if (target.empty()) target.assign("/");

                                            continue;
                                        }
                                    }
                                }
                            }

                            const bool repooled = res.keep_alive() && self->checkin_connection(host, port, is_ssl, {std::move(stream), std::move(buffer)});

                            if constexpr (Api) {
                                if (!route_key.empty()) {
                                    self->rate_limiter_.update(route_key, res);

                                    if (res.result_int() == 429) {
                                        #ifdef DISCUSY_LOGGING
                                        log::Logger{}("429 Rate limit hit for request: {}", url);
                                        #endif

                                        auto delay = self->rate_limiter_.handle_429(res.body());

                                        if (!repooled && stream) {
                                            if (auto* ssl_str_ptr = std::get_if<http::ssl_stream_t>(stream.get()); ssl_str_ptr && is_ssl) {
                                                boost::beast::get_lowest_layer(*ssl_str_ptr).expires_after(opts.request_timeout);
                                                const auto [ec] = co_await ssl_str_ptr->async_shutdown(make_token());
                                                if (ec && ec != boost::asio::ssl::error::stream_truncated) {
                                                    boost::beast::error_code ignored_ec;
                                                    boost::beast::get_lowest_layer(*ssl_str_ptr).socket().close(ignored_ec);
                                                }
                                            } else {
                                                boost::beast::error_code ignored_ec;
                                                http::get_lowest_layer(*stream).socket().close(ignored_ec);
                                            }
                                            stream.reset();
                                        }

                                        ctx::io_context::strand_timer_t timer(strand, delay);
                                        auto [ec] = co_await timer.async_wait(make_token());
                                        if (ec) {
                                            co_return {boost::asio::error::timed_out, http::response{}};
                                        }
                                        continue;
                                    }
                                }
                                #ifdef DISCUSY_LOGGING
                                log::Logger{}("Request to {} with body:\n{}", url, res.body());
                                #endif
                            }

                            #ifdef DISCUSY_LOGGING
                            log::Logger{}("Successful request to {} with code: {}", url, res.result_int());
                            #endif

                            http::response final_resp{
                                .status_code = static_cast<int>(res.result_int()),
                                .body = std::move(res.body()),
                                .method = current_method,
                            };

                            if (opts.include_headers) {
                                auto& hdrs = final_resp.headers.emplace();
                                for (const auto& field : res.base()) {
                                    hdrs.emplace(std::string(field.name_string()), std::string(field.value()));
                                }
                            }

                            if (opts.include_cookies) {
                                auto& cks = final_resp.cookies.emplace(std::move(accumulated_cookies));
                                for (const auto& field : res.base()) {
                                    if (field.name() == boost::beast::http::field::set_cookie || boost::beast::iequals(field.name_string(), "set-cookie")) {
                                        std::string_view val = field.value();
                                        auto semi = val.find(';');
                                        std::string_view pair = (semi != std::string_view::npos) ? val.substr(0, semi) : val;
                                        auto eq = pair.find('=');
                                        if (eq != std::string_view::npos) {
                                            static constexpr auto trim = [](std::string_view s) -> std::string_view {
                                                while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
                                                while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.remove_suffix(1);
                                                return s;
                                            };
                                            std::string_view name = trim(pair.substr(0, eq));
                                            std::string_view value = trim(pair.substr(eq + 1));
                                            if (!name.empty()) {
                                                cks.insert_or_assign(std::string(name), std::string(value));
                                            }
                                        }
                                    }
                                }
                            }

                            if (opts.include_redirect_info || opts.follow_redirects) {
                                final_resp.redirect_count = redirect_count;
                                final_resp.final_url = url;
                            }

                            if (!repooled && stream) {
                                if (auto* ssl_str_ptr = std::get_if<http::ssl_stream_t>(stream.get()); ssl_str_ptr && is_ssl) {
                                    boost::beast::get_lowest_layer(*ssl_str_ptr).expires_after(opts.request_timeout);
                                    const auto [ec] = co_await ssl_str_ptr->async_shutdown(make_token());
                                    if (ec && ec != boost::asio::ssl::error::stream_truncated) {
                                        boost::beast::error_code ignored_ec;
                                        boost::beast::get_lowest_layer(*ssl_str_ptr).socket().close(ignored_ec);
                                    }
                                } else {
                                    boost::beast::error_code ignored_ec;
                                    http::get_lowest_layer(*stream).socket().close(ignored_ec);
                                }
                                stream.reset();
                            }

                            co_return {
                                boost::system::error_code{},
                                std::move(final_resp),
                            };
                        }
                    }
                    catch (const boost::system::system_error& e) {
                        #ifdef DISCUSY_LOGGING
                        log::Logger{}("[http_client] system error: {}", e.what());
                        #endif
                        co_return {e.code(), http::response{}};
                    }
                    catch (const std::bad_alloc& e) {
                        #ifdef DISCUSY_LOGGING
                        log::Logger{}("[http_client] bad_alloc: {}", e.what());
                        #endif
                        co_return {boost::asio::error::no_memory, http::response{}};
                    }
                    #ifdef DISCUSY_LOGGING
                    catch (const std::exception& e) {
                        log::Logger{}("[http_client] error: {}", e.what());
                    }
                    #endif
                    catch (...) {
                        #ifdef DISCUSY_LOGGING
                        log::Logger{}("[http_client] unknown error");
                        #endif
                    }

                    co_return {boost::asio::error::fault, http::response{}};
                }
            ),
            token,
            this, m, std::move(url), std::move(body), std::move(req_headers), std::move(opts)
        );
    }

public:
    using http_client_base::http_client_base;

    template <discusy::asio::ctf<http::response> CompletionToken = ctx::io_context::dct_t>
    auto request(http::method m, std::string url, std::string body = {}, headers headers = {}, http::options opts = {}, CompletionToken&& token = ctx::io_context::dct_t()) {
        return request_impl_<false>(
            m, std::move(url), std::move(body), std::move(headers), std::move(opts), 
            std::forward<CompletionToken>(token)
        );
    }

    template <discusy::asio::ctf<http::response> CompletionToken = ctx::io_context::dct_t>
    auto api_request(http::method m, std::string url, std::string body = {}, headers headers = {}, http::options opts = {}, CompletionToken&& token = ctx::io_context::dct_t()) {
        return request_impl_<true>(
            m, std::move(url), std::move(body), std::move(headers), std::move(opts), 
            std::forward<CompletionToken>(token)
        );
    }
};

#ifdef DISCUSY_USE_CORO_HTTP_CLIENT
using http_client = http_client_coro;
#else
using http_client = http_client_callback;
#endif

}
