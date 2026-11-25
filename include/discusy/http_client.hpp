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

    [[nodiscard]] inline std::string generate_route_key(const http::method m, std::string_view path) {
        static constexpr std::string_view rest_base{urls::REST_BASE};
        if (!path.starts_with(rest_base)) {
            return {};
        }

        path.remove_prefix(rest_base.size());
        if (!path.empty() && path.front() != '/' && path.front() != '?' && path.front() != '#') {
            return {};
        }

        if (const auto q_or_f = path.find_first_of("?#"); q_or_f != std::string_view::npos) {
            path = path.substr(0, q_or_f);
        }

        if (!path.empty() && path.front() == '/') {
            path.remove_prefix(1);
        }

        boost::container::small_vector<std::string_view, 16> segments;
        ulp::str::split_by_for_each(path, '/', [&](const std::string_view part) {
            if (!part.empty()) {
                segments.emplace_back(part);
            }
        });

        const auto method_name = boost::beast::http::to_string(m);
        std::string route_key;
        route_key.reserve(method_name.size() + 1 + path.size());
        route_key.append(method_name.data(), method_name.size());

        for (std::size_t i = 0; i < segments.size(); ++i) {
            route_key += '/';

            // If the segment is a numeric ID, keep major resource parameters
            if (is_snowflake(segments[i])) {
                bool keep_id = false;
                if (i > 0) {
                    const auto prev = segments[i - 1];
                    if (prev == "guilds" || prev == "channels" || prev == "webhooks") {
                        keep_id = true;
                    }
                }
                // Path structure: /webhooks/{webhook_id}/{webhook_token}
                if (!keep_id && i > 1 && segments[i - 2] == "webhooks") {
                    keep_id = true;
                }

                if (keep_id) {
                    route_key += segments[i];
                } else {
                    route_key += ":id";
                }
            } else {
                route_key += segments[i];
            }
        }

        #ifdef DISCUSY_LOGGING
        log::Logger{}("Route key {} for path {}", route_key, path);
        #endif

        return route_key;
    }

    struct parsed_url {
        bool is_ssl{true};
        bool host_is_name{true};
        std::string host;
        std::string host_header;
        std::string port;
        std::string target;
    };

    [[nodiscard]] inline std::optional<parsed_url> parse_http_url(const boost::urls::url_view_base& uv) {
        const auto scheme_id = uv.scheme_id();
        if (scheme_id != boost::urls::scheme::http && scheme_id != boost::urls::scheme::https) {
            return std::nullopt;
        }

        const bool is_ssl = (scheme_id == boost::urls::scheme::https);
        const auto host_addr = uv.host_address();
        if (host_addr.empty()) {
            return std::nullopt;
        }

        const bool host_is_name = (uv.host_type() == boost::urls::host_type::name);

        std::string port;
        if (uv.has_port() && !uv.port().empty()) {
            if (uv.port_number() == 0) {
                return std::nullopt;
            }
            port.assign(uv.port());
        } else {
            port = is_ssl ? "443" : "80";
        }

        std::string host{host_addr};
        if (host_is_name) {
            ulp::str::to_lowercase_inplace(host);
        }

        std::string host_header{uv.encoded_host()};
        if (host_is_name) {
            ulp::str::to_lowercase_inplace(host_header);
        }
        const std::string_view default_port = is_ssl ? "443" : "80";
        if (port != default_port) {
            host_header += ':';
            host_header += port;
        }

        // RFC 9110 / 9112: request-target is path + optional query (never fragment) and starts with '/'.
        std::string target{uv.encoded_target()};
        if (target.empty() || target.front() != '/') {
            target.insert(target.begin(), '/');
        }

        return parsed_url{
            .is_ssl = is_ssl,
            .host_is_name = host_is_name,
            .host = std::move(host),
            .host_header = std::move(host_header),
            .port = std::move(port),
            .target = std::move(target),
        };
    }

    [[nodiscard]] inline std::optional<parsed_url> parse_http_url(const std::string_view url_str) {
        const auto parsed = boost::urls::parse_uri(url_str);
        if (!parsed) {
            return std::nullopt;
        }
        return parse_http_url(*parsed);
    }

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

    void evict_idle_connections() {
        const auto now = std::chrono::steady_clock::now();

        pooled_conn_list connections_to_close;
        connection_pool_.erase_if([&](auto& item) {
            auto& connections = item.second;
            for (auto it = connections.begin(); it != connections.end();) {
                if (!it->stream || (now - it->last_used) > max_idle_time_) {
                    if (it->stream) connections_to_close.emplace_back(std::move(*it));
                    it = connections.erase(it);
                } else {
                    ++it;
                }
            }
            return connections.empty();
        });

        for (const auto& conn : connections_to_close) {
            http::get_lowest_layer(*conn.stream).close();
        }
    }

    void cancel_eviction_timer() {
        std::shared_ptr<eviction_state> old_state;
        {
            const std::scoped_lock lock{eviction_mtx_};
            old_state = std::move(eviction_state_);
        }
        if (old_state) {
            old_state->active.store(false, std::memory_order_release);
            const auto strand = old_state->strand;
            boost::asio::dispatch(strand, [st = std::move(old_state)] mutable {
                st->timer.cancel();
            });
        }
    }

public:
    http_client_base(boost::asio::ssl::context& ssl_ctx, ctx::io_context& ctx)
        : ssl_ctx_{ssl_ctx}, io_ctx_{ctx} {}

    http_client_base(const http_client_base&) = delete;
    http_client_base& operator=(const http_client_base&) = delete;
    http_client_base(http_client_base&&) = delete;
    http_client_base& operator=(http_client_base&&) = delete;

    ~http_client_base() noexcept {
        try {
            stop();
        } catch (...) { // NOLINT(bugprone-empty-catch)
        }
    }

    std::optional<http::pooled_connection> checkout_connection(const std::string_view host, const std::string_view port, const bool is_ssl) {
        const auto now = std::chrono::steady_clock::now();
        std::optional<http::pooled_connection> conn;
        pooled_conn_list expired;

        connection_pool_.visit(std::tuple{host, port, is_ssl}, [&](auto& entry) {
            auto& connections = entry.second;
            while (!connections.empty()) {
                auto candidate = std::move(connections.back());
                connections.pop_back();
                if (!candidate.stream) continue;
                if ((now - candidate.last_used) > max_idle_time_ ||
                    !http::get_lowest_layer(*candidate.stream).socket().is_open()) {
                    expired.emplace_back(std::move(candidate));
                    continue;
                }
                conn.emplace(std::move(candidate));
                break;
            }
        });

        for (const auto& dead : expired) {
            http::get_lowest_layer(*dead.stream).close();
        }
        return conn;
    }

    bool checkin_connection(const std::string_view host, const std::string_view port, const bool is_ssl, http::pooled_connection& conn) {
        const auto limit = max_pool_size_.load(std::memory_order_relaxed);
        if (limit == 0 || !conn.stream || conn.buffer.size() != 0) return false;

        auto& lowest = http::get_lowest_layer(*conn.stream);
        if (!lowest.socket().is_open()) return false;

        if (conn.buffer.capacity() > POOL_BUFFER_SHRINK_THRESHOLD) {
            conn.buffer = boost::beast::flat_buffer{};
        }

        lowest.expires_never();
        conn.last_used = std::chrono::steady_clock::now();

        bool inserted = false;
        const auto try_push = [&](auto& entry) {
            if (entry.second.size() < limit) {
                entry.second.emplace_back(std::move(conn));
                inserted = true;
            }
        };

        if (connection_pool_.visit(std::tuple{host, port, is_ssl}, try_push) == 0UZ) {
            connection_pool_.try_emplace_and_visit(
                http::pool_endpoint_key{.host = std::string{host}, .port = std::string{port}, .is_ssl = is_ssl},
                try_push,
                try_push
            );
        }

        return inserted;
    }

    bool checkin_connection(const std::string_view host, const std::string_view port, const bool is_ssl, http::pooled_connection&& conn) {
        auto local_conn = std::move(conn);
        return checkin_connection(host, port, is_ssl, local_conn);
    }

    // Call after starting or restarting the io_context.
    void start() {
        cancel_eviction_timer();
        connection_pool_.clear();

        auto st = std::make_shared<eviction_state>(io_ctx_.make_strand());
        st->active.store(true, std::memory_order_release);
        {
            const std::scoped_lock lock{eviction_mtx_};
            eviction_state_ = st;
        }

        const auto strand = st->strand;
        boost::asio::dispatch(strand, [this, st = std::move(st)] mutable {
            if (!st->active.load(std::memory_order_acquire)) return;

            auto& timer = st->timer;
            timer.expires_after(eviction_timeout_);
            timer.async_wait([this, st = std::move(st)](this auto&& self, const boost::system::error_code& ec) -> void {
                if (ec || !st->active.load(std::memory_order_acquire)) return;

                this->evict_idle_connections();
                if (!st->active.load(std::memory_order_acquire)) return;

                st->timer.expires_after(eviction_timeout_);
                st->timer.async_wait(std::forward<decltype(self)>(self));
            });
        });
    }

    void stop() {
        cancel_eviction_timer();
        connection_pool_.clear();
    }

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
            auto base_alloc = boost::asio::get_associated_allocator(h, boost::asio::recycling_allocator<void>{});
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
                    [g = holder_guard{this}, ec, res = std::move(res)]() mutable {
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

    struct request_state {
        using io_alloc_t = boost::asio::recycling_allocator<void>;

        completion_base* completion_{nullptr};
        http_client_callback* self;
        http::method current_method;
        bool is_api{false};
        std::string url;

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
        std::string host_header;
        std::string port;
        std::string target;
        bool is_ssl{true};
        bool host_is_name{true};

        std::unique_ptr<http::stream_t> stream;
        boost::beast::flat_buffer buffer;
        bool is_reused{false};
        bool skip_pool{false};

        ctx::io_context::strand_tcp_resolver_t resolver;
        ctx::io_context::strand_timer_t timer;

        std::optional<boost::beast::http::request<boost::beast::http::string_body>> req;
        std::optional<boost::beast::http::response_parser<boost::beast::http::string_body>> parser;

        std::size_t redirect_count{0};
        http::cookies accumulated_cookies;

        io_alloc_t allocator{};

        request_state(
            completion_base* comp,
            http_client_callback* s,
            const bool api,
            const http::method m,
            std::string u,
            http::parsed_url parsed,
            std::string b,
            headers hdrs,
            http::options o
        )
            : completion_{comp},
              self{s},
              current_method{m},
              is_api{api},
              url{std::move(u)},
              body{std::move(b)},
              req_headers{std::move(hdrs)},
              opts{std::move(o)},
              strand_{s->io_ctx_.make_strand()},
              host{std::move(parsed.host)},
              host_header{std::move(parsed.host_header)},
              port{std::move(parsed.port)},
              target{std::move(parsed.target)},
              is_ssl{parsed.is_ssl},
              host_is_name{parsed.host_is_name},
              resolver{strand_},
              timer{strand_} {}

        ~request_state() noexcept {
            if (completion_ != nullptr) {
                completion_->destroy();
                completion_ = nullptr;
            }
        }

        request_state(const request_state&) = delete;
        request_state& operator=(const request_state&) = delete;
        request_state(request_state&&) = delete;
        request_state& operator=(request_state&&) = delete;

        [[nodiscard]] bool is_cancelled() const noexcept {
            return cancelled_type != boost::asio::cancellation_type::none;
        }

        void handle_cancel(std::shared_ptr<request_state> self_ptr, const boost::asio::cancellation_type type) {
            if (type == boost::asio::cancellation_type::none) return;
            if (completed.load(std::memory_order_acquire)) return;

            // Hold strand_mtx_ across post() so cancel ordering against strand handoff
            // in step_acquire_connection() is strictly FIFO and race-free.
            const std::scoped_lock lock{strand_mtx_};
            boost::asio::post(strand_, [self_ptr = std::move(self_ptr), type] {
                if (self_ptr->completed.load(std::memory_order_acquire)) return;
                self_ptr->cancelled_type = self_ptr->cancelled_type | type;
                self_ptr->child_signal.emit(type);
            });
        }

        void close_stream_immediately() noexcept {
            if (stream) {
                http::get_lowest_layer(*stream).close();
                stream.reset();
            }
        }

        void finish(std::shared_ptr<request_state> self_ptr, const boost::system::error_code ec, http::response res = {}) {
            if (completed.exchange(true, std::memory_order_acq_rel)) return;

            // Ensure stream is closed/destroyed on its own strand before completing on handler's executor.
            close_stream_immediately();

            auto* comp = std::exchange(completion_, nullptr);
            self_ptr.reset();
            if (comp != nullptr) {
                comp->complete_and_destroy(ec, std::move(res));
            }
        }

        void operation_cancelled(std::shared_ptr<request_state> self_ptr) {
            close_stream_immediately();
            finish(std::move(self_ptr), boost::asio::error::operation_aborted);
        }

        void init(std::shared_ptr<request_state> self_ptr) {
            const auto alloc = allocator;
            const auto strand = strand_;
            boost::asio::post(strand, boost::asio::bind_allocator(alloc, [self_ptr = std::move(self_ptr)] mutable {
                auto* ptr = self_ptr.get();
                ptr->start(std::move(self_ptr));
            }));
        }

        void start(std::shared_ptr<request_state> self_ptr) {
            #ifdef DISCUSY_LOGGING
            log::Logger{}("Making request to {} with body {} and headers {}", url, body, req_headers);
            #endif

            if (is_cancelled()) return operation_cancelled(std::move(self_ptr));

            if (is_api) {
                route_key = http::generate_route_key(current_method, url);

                if (!route_key.empty()) {
                    const auto delay = self->rate_limiter_.get_delay(route_key);
                    if (delay.count() > 0) {
                        timer.expires_after(delay);
                        timer.async_wait(
                            boost::asio::bind_cancellation_slot(
                                child_signal.slot(),
                                boost::asio::bind_allocator(
                                    allocator,
                                    [self_ptr = std::move(self_ptr)](const boost::system::error_code& ec) mutable {
                                        auto* ptr = self_ptr.get();
                                        if (ec) {
                                            if (ptr->is_cancelled() || ec == boost::asio::error::operation_aborted) {
                                                return ptr->operation_cancelled(std::move(self_ptr));
                                            }
                                            return ptr->finish(std::move(self_ptr), ec);
                                        }
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
        requires (std::invocable<F&&, std::shared_ptr<request_state>&&>)
        void close_connection_then(std::shared_ptr<request_state> self_ptr, F&& then) {
            if (stream) {
                if (auto* ssl_str_ptr = std::get_if<http::ssl_stream_t>(stream.get())) {
                    const auto shutdown_timeout = std::min(opts.request_timeout, ssl_shutdown_timeout_);
                    boost::beast::get_lowest_layer(*ssl_str_ptr).expires_after(shutdown_timeout);
                    ssl_str_ptr->async_shutdown(
                        boost::asio::bind_cancellation_slot(
                            child_signal.slot(),
                            boost::asio::bind_allocator(
                                allocator,
                                [self_ptr = std::move(self_ptr), then = std::forward<F>(then)](const boost::system::error_code&) mutable {
                                    auto* ptr = self_ptr.get();
                                    ptr->close_stream_immediately();
                                    std::invoke(std::move(then), std::move(self_ptr));
                                }
                            )
                        )
                    );
                    return;
                }

                close_stream_immediately();
            }

            std::invoke(std::forward<F>(then), std::move(self_ptr));
        }

        void step_loop_start(std::shared_ptr<request_state> self_ptr) {
            if (is_cancelled()) return operation_cancelled(std::move(self_ptr));

            close_stream_immediately();
            step_acquire_connection(std::move(self_ptr));
        }

        void step_acquire_connection(std::shared_ptr<request_state> self_ptr) {
            if (is_cancelled()) return operation_cancelled(std::move(self_ptr));

            if (!skip_pool) {
                if (auto conn = self->checkout_connection(host, port, is_ssl)) {
                    #ifdef DISCUSY_LOGGING
                    log::Logger{}("Re-using connection for request: {}", url);
                    #endif

                    buffer = std::move(conn->buffer);
                    is_reused = true;
                    stream = std::move(conn->stream);

                    auto new_strand = http::get_stream_executor(*stream);
                    if (new_strand == strand_) {
                        return step_write_request(std::move(self_ptr));
                    }

                    // Rebind resolver and timer onto the pooled stream's strand so all I/O
                    // and cancellation operations on this request remain serialized on one strand.
                    resolver = ctx::io_context::strand_tcp_resolver_t{new_strand};
                    timer = ctx::io_context::strand_timer_t{new_strand};

                    const auto alloc = allocator;
                    {
                        const std::scoped_lock lock{strand_mtx_};
                        const auto old_strand = std::exchange(strand_, new_strand);

                        // Posted inside strand_mtx_ so any cancel posted before the swap runs
                        // on old_strand prior to this handoff, and any cancel after runs on new_strand.
                        boost::asio::post(old_strand, boost::asio::bind_allocator(alloc, [self_ptr = std::move(self_ptr), alloc, new_strand] mutable {
                            boost::asio::post(new_strand, boost::asio::bind_allocator(alloc, [self_ptr = std::move(self_ptr)] mutable {
                                auto* ptr = self_ptr.get();
                                ptr->step_write_request(std::move(self_ptr));
                            }));
                        }));
                    }
                    return;
                }
            }

            #ifdef DISCUSY_LOGGING
            log::Logger{}("Making new connection for request: {}", url);
            #endif
            is_reused = false;
            skip_pool = false;
            buffer.clear();

            if (is_ssl) {
                stream = std::make_unique<http::stream_t>(std::in_place_type<http::ssl_stream_t>, strand_, self->ssl_ctx_);
                auto& ssl_str = std::get<http::ssl_stream_t>(*stream);

                // RFC 6066 §3: SNI HostName must only be set for DNS hostnames, not IP literals.
                if (host_is_name && !SSL_set_tlsext_host_name(ssl_str.native_handle(), host.c_str())) {
                    const boost::system::error_code ec{static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category()};
                    return finish(std::move(self_ptr), ec ? ec : boost::asio::ssl::error::unspecified_system_error);
                }

                ssl_str.set_verify_callback(boost::asio::ssl::host_name_verification(host));
            } else {
                stream = std::make_unique<http::stream_t>(std::in_place_type<http::plain_stream_t>, strand_);
            }

            step_resolve(std::move(self_ptr));
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
                                auto* ptr = self_ptr.get();
                                if (res_ec) {
                                    if (ptr->is_cancelled()) {
                                        return ptr->operation_cancelled(std::move(self_ptr));
                                    }
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
                            auto* ptr = self_ptr.get();
                            if (conn_ec) {
                                if (ptr->is_cancelled() || conn_ec == boost::asio::error::operation_aborted) {
                                    return ptr->operation_cancelled(std::move(self_ptr));
                                }
                                if (conn_ec == boost::beast::error::timeout) {
                                    return ptr->finish(std::move(self_ptr), boost::asio::error::timed_out);
                                }
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
                            auto* ptr = self_ptr.get();
                            if (hs_ec) {
                                if (ptr->is_cancelled() || hs_ec == boost::asio::error::operation_aborted) {
                                    return ptr->operation_cancelled(std::move(self_ptr));
                                }
                                if (hs_ec == boost::beast::error::timeout) {
                                    return ptr->finish(std::move(self_ptr), boost::asio::error::timed_out);
                                }
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

        [[nodiscard]] static bool is_stale_write_error(const boost::system::error_code& ec) noexcept {
            return ec == boost::beast::http::error::end_of_stream ||
                   ec == boost::asio::error::eof ||
                   ec == boost::asio::error::connection_reset ||
                   ec == boost::asio::error::connection_aborted ||
                   ec == boost::asio::error::broken_pipe ||
                   ec == boost::asio::ssl::error::stream_truncated;
        }

        [[nodiscard]] static bool is_stale_read_error(const boost::system::error_code& ec) noexcept {
            return ec == boost::beast::http::error::end_of_stream ||
                   ec == boost::asio::error::eof ||
                   ec == boost::asio::error::connection_reset ||
                   ec == boost::asio::error::connection_aborted ||
                   ec == boost::asio::ssl::error::stream_truncated;
        }

        void step_write_request(std::shared_ptr<request_state> self_ptr) {
            if (is_cancelled()) return operation_cancelled(std::move(self_ptr));

            if (!req) {
                req.emplace(current_method, target, 11);
                req->set(boost::beast::http::field::host, host_header);
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
                    if (const auto existing = req->find(boost::beast::http::field::cookie); existing != req->end()) {
                        req->set(boost::beast::http::field::cookie, ulp::str::concat_strings(existing->value(), "; ", cookie_header));
                    } else {
                        req->set(boost::beast::http::field::cookie, cookie_header);
                    }
                }

                const bool method_expects_payload =
                    current_method != boost::beast::http::verb::get &&
                    current_method != boost::beast::http::verb::delete_ &&
                    current_method != boost::beast::http::verb::head;
                if (method_expects_payload || (!body.empty() && current_method != boost::beast::http::verb::head)) {
                    req->body() = body;
                    req->prepare_payload();
                }
            }

            auto& lowest = http::get_lowest_layer(*stream);
            lowest.expires_after(opts.request_timeout);

            auto write_handler = [self_ptr = std::move(self_ptr)](const boost::system::error_code& write_ec, std::size_t) mutable {
                auto* ptr = self_ptr.get();
                if (write_ec) {
                    if (ptr->is_cancelled() || write_ec == boost::asio::error::operation_aborted) {
                        return ptr->operation_cancelled(std::move(self_ptr));
                    }
                    if (write_ec == boost::beast::error::timeout) {
                        return ptr->finish(std::move(self_ptr), boost::asio::error::timed_out);
                    }
                    if (ptr->is_reused && is_stale_write_error(write_ec)) {
                        ptr->close_stream_immediately();
                        ptr->skip_pool = true;
                        return ptr->step_acquire_connection(std::move(self_ptr));
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
            if (current_method == boost::beast::http::verb::head) {
                parser->skip(true);
            }

            auto read_handler = [self_ptr = std::move(self_ptr)](const boost::system::error_code& read_ec, std::size_t) mutable {
                auto* ptr = self_ptr.get();
                if (read_ec) {
                    if (ptr->is_cancelled() || read_ec == boost::asio::error::operation_aborted) {
                        return ptr->operation_cancelled(std::move(self_ptr));
                    }
                    if (read_ec == boost::beast::error::timeout) {
                        return ptr->finish(std::move(self_ptr), boost::asio::error::timed_out);
                    }
                    if (ptr->is_reused && !ptr->parser->got_some() && is_stale_read_error(read_ec)) {
                        ptr->close_stream_immediately();
                        ptr->skip_pool = true;
                        return ptr->step_acquire_connection(std::move(self_ptr));
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

        bool try_repool_connection(const bool res_keep_alive) {
            if (!res_keep_alive || !req || !req->keep_alive() || !stream || buffer.size() != 0) {
                return false;
            }
            http::pooled_connection candidate{.stream = std::move(stream), .buffer = std::move(buffer)};
            if (self->checkin_connection(host, port, is_ssl, candidate)) {
                buffer = boost::beast::flat_buffer{};
                return true;
            }
            stream = std::move(candidate.stream);
            buffer = std::move(candidate.buffer);
            return false;
        }

        static void parse_set_cookie_headers(const boost::beast::http::response<boost::beast::http::string_body>& res, http::cookies& out) {
            static constexpr auto trim = [](std::string_view s) noexcept -> std::string_view {
                while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
                while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.remove_suffix(1);
                return s;
            };

            for (const auto& field : res.base()) {
                if (field.name() == boost::beast::http::field::set_cookie) {
                    const std::string_view val{field.value().data(), field.value().size()};
                    const auto semi = val.find(';');
                    const std::string_view pair = (semi != std::string_view::npos) ? val.substr(0, semi) : val;
                    const auto eq = pair.find('=');
                    if (eq != std::string_view::npos) {
                        const std::string_view name = trim(pair.substr(0, eq));
                        const std::string_view value = trim(pair.substr(eq + 1));
                        if (!name.empty()) {
                            out.insert_or_assign(std::string{name}, std::string{value});
                        }
                    }
                }
            }
        }

        void step_handle_response(std::shared_ptr<request_state> self_ptr) {
            if (is_cancelled()) return operation_cancelled(std::move(self_ptr));
            if (!parser) return finish(std::move(self_ptr), boost::asio::error::fault);

            auto res = parser->release();
            const auto status = res.result_int();
            const bool is_redirect = (status == 301 || status == 302 || status == 303 || status == 307 || status == 308);

            if (opts.follow_redirects && is_redirect && (redirect_count < opts.max_redirects)) {
                const auto loc_it = res.find(boost::beast::http::field::location);
                if (loc_it != res.end()) {
                    const auto loc_ref = boost::urls::parse_uri_reference(loc_it->value());
                    const auto cur_parsed = boost::urls::parse_uri(url);
                    if (loc_ref && cur_parsed) {
                        boost::urls::url resolved_url{*cur_parsed};
                        if (resolved_url.resolve(*loc_ref)) {
                            auto next_url = http::parse_http_url(resolved_url);
                            if (!next_url) {
                                return finish(std::move(self_ptr), boost::asio::error::invalid_argument);
                            }

                            if (is_ssl && !next_url->is_ssl && !opts.allow_http_downgrade) {
                                #ifdef DISCUSY_LOGGING
                                log::Logger{}("[http_client] Prevented HTTPS to HTTP downgrade redirect from {} to {}", url, resolved_url.buffer());
                                #endif
                                return finish(std::move(self_ptr), boost::asio::error::no_permission);
                            }

                            const bool origin_changed =
                                (is_ssl != next_url->is_ssl) ||
                                (host != next_url->host) ||
                                (port != next_url->port);

                            if (opts.include_cookies && !origin_changed) {
                                parse_set_cookie_headers(res, accumulated_cookies);
                            }

                            static constexpr auto is_hop_by_hop_header = [](const std::string_view name) noexcept -> bool {
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
                                static constexpr auto is_sensitive_header = [](const std::string_view name) noexcept -> bool {
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

                            if (!try_repool_connection(res.keep_alive())) {
                                close_stream_immediately();
                                buffer.clear();
                            }

                            ++redirect_count;
                            url.assign(resolved_url.buffer());

                            const bool method_changed_to_get =
                                (status == 301 || status == 302 || status == 303) &&
                                current_method != boost::beast::http::verb::get &&
                                current_method != boost::beast::http::verb::head;
                            if (method_changed_to_get) {
                                current_method = boost::beast::http::verb::get;
                                body.clear();

                                static constexpr auto is_payload_header = [](const std::string_view name) noexcept -> bool {
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

                            is_ssl = next_url->is_ssl;
                            host_is_name = next_url->host_is_name;
                            host = std::move(next_url->host);
                            host_header = std::move(next_url->host_header);
                            port = std::move(next_url->port);
                            target = std::move(next_url->target);
                            req.reset();
                            skip_pool = false;

                            return step_acquire_connection(std::move(self_ptr));
                        }
                    }
                }
            }

            const bool repooled = try_repool_connection(res.keep_alive());

            if (is_api) {
                if (!route_key.empty()) {
                    self->rate_limiter_.update(route_key, res);

                    if (status == 429) {
                        #ifdef DISCUSY_LOGGING
                        log::Logger{}("429 Rate limit hit for request: {}", url);
                        #endif

                        const auto delay = self->rate_limiter_.handle_429(res.body());

                        if (!repooled && stream) {
                            close_connection_then(std::move(self_ptr), [delay](std::shared_ptr<request_state> self_ptr) {
                                auto* ptr = self_ptr.get();
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
            log::Logger{}("Successful request to {} with code: {}", url, status);
            #endif

            http::response final_resp{
                .status_code = static_cast<int>(status),
                .body = std::move(res.body()),
                .method = current_method,
            };

            if (opts.include_headers) {
                auto& hdrs = final_resp.headers.emplace();
                for (const auto& field : res.base()) {
                    hdrs.emplace(std::string{field.name_string()}, std::string{field.value()});
                }
            }

            if (opts.include_cookies) {
                auto& cks = final_resp.cookies.emplace(std::move(accumulated_cookies));
                parse_set_cookie_headers(res, cks);
            }

            if (opts.include_redirect_info || opts.follow_redirects) {
                final_resp.redirect_count = redirect_count;
                final_resp.final_url = url;
            }

            if (!repooled && stream) {
                close_connection_then(
                    std::move(self_ptr),
                    [final_resp = std::move(final_resp)](std::shared_ptr<request_state> self_ptr) mutable {
                        auto* ptr = self_ptr.get();
                        ptr->finish(std::move(self_ptr), boost::system::error_code{}, std::move(final_resp));
                    }
                );
                return;
            }

            finish(std::move(self_ptr), boost::system::error_code{}, std::move(final_resp));
        }

        void step_wait_429(std::shared_ptr<request_state> self_ptr, const std::chrono::steady_clock::duration delay) {
            if (is_cancelled()) return operation_cancelled(std::move(self_ptr));

            timer.expires_after(delay);
            timer.async_wait(
                boost::asio::bind_cancellation_slot(
                    child_signal.slot(),
                    boost::asio::bind_allocator(
                        allocator,
                        [self_ptr = std::move(self_ptr)](const boost::system::error_code& ec) mutable {
                            auto* ptr = self_ptr.get();
                            if (ec) {
                                if (ptr->is_cancelled() || ec == boost::asio::error::operation_aborted) {
                                    return ptr->operation_cancelled(std::move(self_ptr));
                                }
                                return ptr->finish(std::move(self_ptr), ec);
                            }
                            ptr->skip_pool = false;
                            ptr->step_loop_start(std::move(self_ptr));
                        }
                    )
                )
            );
        }
    };

    static void start_request_state(
        completion_base* comp,
        boost::asio::cancellation_slot slot,
        http_client_callback* self,
        const bool is_api,
        const http::method m,
        std::string url,
        http::parsed_url parsed,
        std::string body,
        headers req_headers,
        http::options opts
    ) {
        std::shared_ptr<request_state> state;
        try {
            state = std::allocate_shared<request_state>(
                boost::asio::recycling_allocator<void>{},
                comp,
                self, is_api, m, std::move(url), std::move(parsed),
                std::move(body), std::move(req_headers), std::move(opts)
            );
        } catch (...) {
            if (comp != nullptr) comp->destroy();
            throw;
        }

        if (slot.is_connected()) {
            slot.assign([weak_state = std::weak_ptr<request_state>{state}](const boost::asio::cancellation_type type) {
                if (std::shared_ptr<request_state> st = weak_state.lock()) {
                    auto* raw = st.get();
                    raw->handle_cancel(std::move(st), type);
                }
            });
        }

        request_state* raw = state.get();
        raw->init(std::move(state));
    }

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
                auto slot = boost::asio::get_associated_cancellation_slot(comp->handler);
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
