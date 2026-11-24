#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/cancel_after.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/url.hpp>

#include "io_context.hpp"
#include "log.hpp" // IWYU pragma: keep

namespace discusy::ws {

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using boost::asio::ip::tcp;

inline constexpr std::uint16_t CLOSE_LOCAL = 100;     // our own close completed
inline constexpr std::uint16_t CLOSE_TRANSPORT = 101; // died outside of a websocket close frame

struct message_t {
    std::string payload;
    bool rate_limited{false};
    bool binary{false};

    message_t() = default;
    constexpr ~message_t() noexcept = default;

    message_t(message_t&&) noexcept = default;
    message_t& operator=(message_t&&) noexcept = default;
    // no copies
    message_t(const message_t&) = delete;
    message_t& operator=(const message_t&) = delete;

    explicit message_t(std::string p, const bool rate_limit = false, const bool is_bin = false)
        : payload(std::move(p)), rate_limited(rate_limit), binary(is_bin) {}
};

// all methods must be called from the strand_ itself
template <typename openHandler, typename closeHandler, typename messageHandler, typename connectHandler>
requires (
    std::invocable<std::decay_t<openHandler>&> && std::invocable<std::decay_t<connectHandler>&> &&
    std::invocable<std::decay_t<closeHandler>&, std::uint16_t> && std::invocable<std::decay_t<messageHandler>&, std::string_view, bool>
)
class websocket_client : public std::enable_shared_from_this<websocket_client<openHandler, closeHandler, messageHandler, connectHandler>> {
    using stream_t = websocket::stream<net::ssl::stream<ctx::io_context::strand_tcp_stream_t>>;

    static constexpr std::size_t INITIAL_PRIORITY_QUEUE_CAPACITY = 16;
    static constexpr std::size_t INITIAL_RATELIMITED_QUEUE_CAPACITY = 64;
    static constexpr std::size_t READ_BUFFER_SHRINK_THRESHOLD = 256UZ * 1024;
    static constexpr std::size_t WRITE_BUFFER_SHRINK_THRESHOLD = 64UZ * 1024;

    // Owns the stream and I/O buffers for a single connection attempt so in-flight
    // async operations can never alias across reconnects or outlive their buffers.
    struct connection_state {
        stream_t ws;
        beast::flat_buffer read_buffer{};
        message_t current_msg{};
        bool writing{false};

        connection_state(ctx::io_context::strand_t strand, net::ssl::context& ctx)
            : ws(std::move(strand), ctx) {}
    };

    ctx::io_context::strand_t strand_;
    net::ssl::context& ctx_;
    ctx::io_context::strand_tcp_resolver_t resolver_;

    std::shared_ptr<connection_state> conn_;
    ctx::io_context::strand_timer_t timer_;

    boost::circular_buffer<message_t> priority_pending_{};
    boost::circular_buffer<message_t> ratelimited_pending_{};

    std::uint64_t connection_id_ = 0;
    std::uint64_t timer_gen_ = 0;

    bool timer_active_ = false;
    bool stopped_ = false;
    bool paused_ = false;
    bool open_ = false;
    bool closed_ = true;
    bool close_requested_ = false;
    bool ready_posted_ = false;
    bool host_is_name_ = true;

    std::string host_;
    std::string port_;
    std::string host_header_;
    std::string target_;

    std::decay_t<openHandler> on_open_cb_{};
    std::decay_t<closeHandler> on_close_cb_{};
    std::decay_t<messageHandler> on_message_cb_{};
    std::decay_t<connectHandler> on_connect_ready_{};

    static void push_queue(boost::circular_buffer<message_t>& q, message_t&& msg, const std::size_t initial_cap) {
        if (q.full()) {
            const std::size_t cur = q.capacity();
            const std::size_t max_cap = q.max_size();
            if (cur == 0) {
                q.set_capacity(initial_cap);
            } else {
                const std::size_t next = (cur > max_cap / 2) ? max_cap : (cur * 2);
                if (next == cur) throw std::bad_alloc{};
                q.set_capacity(next);
            }
        }
        q.push_back(std::move(msg));
    }

    bool set_url(const std::string_view url_str) {
        const auto result = boost::urls::parse_uri(url_str);
        if (!result) return false;

        const auto& url = *result;

        if (url.scheme() != "wss") {
            return false;
        }

        const auto host_addr = url.host_address();
        if (host_addr.empty()) {
            return false;
        }

        std::string new_host{host_addr};
        std::string new_port = (url.has_port() && !url.port().empty()) ? std::string{url.port()} : std::string{"443"};

        std::string new_host_header{url.encoded_host()};
        if (new_port != "443") {
            new_host_header += ':';
            new_host_header += new_port;
        }

        std::string new_target{url.encoded_resource()};
        if (new_target.empty() || new_target.front() != '/') {
            new_target.insert(new_target.begin(), '/');
        }

        host_ = std::move(new_host);
        port_ = std::move(new_port);
        host_header_ = std::move(new_host_header);
        target_ = std::move(new_target);
        host_is_name_ = (url.host_type() == boost::urls::host_type::name);

        return true;
    }

public:
    websocket_client(websocket_client&&) = delete;
    websocket_client& operator=(websocket_client&&) = delete;
    websocket_client(const websocket_client&) = delete;
    websocket_client& operator=(const websocket_client&) = delete;

    explicit websocket_client(ctx::io_context::strand_t strand, net::ssl::context& ctx)
        : strand_{std::move(strand)},
          ctx_{ctx},
          resolver_{strand_},
          timer_{strand_} {}

    // must run from the strand itself
    ~websocket_client() noexcept {
        try {
            stopped_ = true;
            hard_reset();
        } catch (...) {
            // destructors must not throw
            stopped_ = true;
        }
    }

    [[nodiscard]] std::vector<std::string> extract_pending_payloads() {
        std::vector<std::string> evacuated;
        evacuated.reserve(ratelimited_pending_.size());
        for (auto& msg : ratelimited_pending_) {
            evacuated.emplace_back(std::move(msg.payload));
        }
        ratelimited_pending_.clear();
        priority_pending_.clear();
        return evacuated;
    }

    void stop() {
        stopped_ = true;
        hard_reset();
    }

    // Immediately aborts any active/pending connection without invoking close or connect-ready callbacks.
    void disconnect() {
        if (stopped_) return;
        hard_reset();
        paused_ = true;
    }

    [[nodiscard]] bool stopped() const noexcept { return stopped_; }

    [[nodiscard]] bool is_open() const noexcept { return open_ && !closed_ && !stopped_; }

    template <typename F>
    requires ( std::invocable<F&> )
    void on_open(F&& cb) { on_open_cb_ = std::forward<F>(cb); }

    template <typename F>
    requires ( std::invocable<F&> )
    void on_connect_ready(F&& cb) { on_connect_ready_ = std::forward<F>(cb); }

    template <typename F>
    requires ( std::invocable<F&, std::uint16_t> )
    void on_close(F&& cb) { on_close_cb_ = std::forward<F>(cb); }

    template <typename F>
    requires ( std::invocable<F&, std::string_view, bool> )
    void on_message(F&& cb) { on_message_cb_ = std::forward<F>(cb); }

    [[nodiscard]] bool connect(const std::string_view url) {
        if (stopped_) return false;

        hard_reset();
        priority_pending_.clear();

        if (!set_url(url)) return false;

        closed_ = false;
        conn_ = std::make_shared<connection_state>(strand_, ctx_);

        do_resolve();
        return true;
    }

    void send_ratelimited(std::string&& payload, const bool binary = false) {
        if (stopped_) return;

        try {
            push_queue(ratelimited_pending_, message_t{std::move(payload), true, binary}, INITIAL_RATELIMITED_QUEUE_CAPACITY);
            drain();
        } catch (const std::bad_alloc& e) {
            #ifdef DISCUSY_LOGGING
            log::Logger{}("[websocket_client] send_ratelimited bad_alloc: {}", e.what());
            #else
            (void)e;
            #endif
        }
    }

    void send(std::string&& payload, const bool binary = false) {
        if (stopped_) return;

        try {
            push_queue(priority_pending_, message_t{std::move(payload), false, binary}, INITIAL_PRIORITY_QUEUE_CAPACITY);
            drain();
        } catch (const std::bad_alloc& e) {
            #ifdef DISCUSY_LOGGING
            log::Logger{}("[websocket_client] send bad_alloc: {}", e.what());
            #else
            (void)e;
            #endif
        }
    }

    void pause() {
        paused_ = true;
    }

    void resume() {
        if (stopped_) return;

        paused_ = false;
        drain();
    }

    void clear() {
        priority_pending_.clear();
        ratelimited_pending_.clear();
    }

    void close() {
        if (stopped_) return;

        close_requested_ = true;

        if (closed_) {
            notify_ready();
            return;
        }

        const bool was_open = open_;
        teardown_transport();
        notify_closed(was_open ? CLOSE_LOCAL : CLOSE_TRANSPORT);
    }

private:
    void teardown_transport() {
        open_ = false;
        paused_ = true;
        ++timer_gen_;
        timer_active_ = false;

        timer_.cancel();
        resolver_.cancel();

        if (conn_) {
            beast::get_lowest_layer(conn_->ws).close();
            conn_.reset();
        }
    }

    void hard_reset() {
        ++connection_id_;

        open_ = false;
        closed_ = true;
        close_requested_ = false;
        ready_posted_ = false;
        ++timer_gen_;
        timer_active_ = false;

        timer_.cancel();
        resolver_.cancel();

        if (conn_) {
            beast::get_lowest_layer(conn_->ws).close();
            conn_.reset();
        }
    }

    void notify_closed(const std::uint16_t code) {
        if (stopped_ || closed_) return;

        closed_ = true;
        open_ = false;

        net::post(strand_, [weak_self = this->weak_from_this(), id = connection_id_, code] {
            const auto s = weak_self.lock();
            if (!s) return;

            auto& self = *s;
            if (self.stopped_ || (self.connection_id_ != id)) return;

            self.on_close_cb_(code);

            if (self.stopped_ || (self.connection_id_ != id)) return;

            if (self.close_requested_) self.notify_ready();
        });
    }

    void notify_ready() {
        if (stopped_ || !closed_ || !close_requested_ || ready_posted_) return;

        ready_posted_ = true;

        net::post(strand_, [weak_self = this->weak_from_this(), id = connection_id_] {
            const auto s = weak_self.lock();
            if (!s) return;

            auto& self = *s;
            if (self.stopped_ || (self.connection_id_ != id)) return;

            self.ready_posted_ = false;
            if (!self.closed_ || !self.close_requested_) return;

            self.close_requested_ = false;
            self.on_connect_ready_();
        });
    }

    void fail(const std::uint16_t code) {
        if (stopped_ || closed_) return;

        teardown_transport();
        notify_closed(code);
    }

    void start_rate_limit_timer() {
        const auto gen = ++timer_gen_;
        timer_active_ = true;
        timer_.expires_after(std::chrono::milliseconds{600});
        timer_.async_wait([weak_self = this->weak_from_this(), id = connection_id_, gen](const beast::error_code ec) {
            if (ec) return;
            const auto s = weak_self.lock();
            if (!s) return;

            auto& self = *s;
            if (self.stopped_ || self.closed_ || (self.connection_id_ != id) || (self.timer_gen_ != gen)) return;

            self.timer_active_ = false;
            self.drain();
        });
    }

    void do_resolve() {
        if (stopped_ || closed_ || !conn_) return;

        if (host_is_name_ && !SSL_set_tlsext_host_name(conn_->ws.next_layer().native_handle(), host_.c_str())) {
            const beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
            handle_error("SSL SNI Setup", ec);
            return;
        }

        boost::system::error_code verify_ec;
        conn_->ws.next_layer().set_verify_callback(net::ssl::host_name_verification(host_), verify_ec);
        if (verify_ec) {
            handle_error("SSL Verify Setup", verify_ec);
            return;
        }

        resolver_.async_resolve(host_, port_,
            boost::asio::cancel_after(std::chrono::seconds{31}, [weak_self = this->weak_from_this(), id = connection_id_](const beast::error_code ec, const tcp::resolver::results_type& results) {
                const auto s = weak_self.lock();
                if (!s) return;

                auto& self = *s;
                if (self.stopped_ || self.closed_ || (self.connection_id_ != id) || !self.conn_) return;

                if (ec) {
                    self.handle_error("Resolve", ec);
                    return;
                }
                self.do_connect(self.conn_, results);
            }));
    }

    void do_connect(std::shared_ptr<connection_state> conn, const tcp::resolver::results_type& results) {
        beast::get_lowest_layer(conn->ws).expires_after(std::chrono::seconds{30});

        beast::get_lowest_layer(conn->ws).async_connect(results,
            [weak_self = this->weak_from_this(), id = connection_id_, conn](const beast::error_code ec, const tcp::resolver::results_type::endpoint_type&) mutable {
                const auto s = weak_self.lock();
                if (!s) return;

                auto& self = *s;
                if (self.stopped_ || self.closed_ || (self.connection_id_ != id) || (self.conn_.get() != conn.get())) return;

                if (ec) {
                    self.handle_error("Connect", ec);
                    return;
                }

                boost::system::error_code opt_ec;
                auto& sock = beast::get_lowest_layer(conn->ws).socket();
                sock.set_option(tcp::no_delay(true), opt_ec);
                sock.set_option(boost::asio::socket_base::keep_alive(true), opt_ec);
                self.do_ssl_handshake(std::move(conn));
            });
    }

    void do_ssl_handshake(std::shared_ptr<connection_state> conn) {
        beast::get_lowest_layer(conn->ws).expires_after(std::chrono::seconds{30});

        conn->ws.next_layer().async_handshake(net::ssl::stream_base::client,
            [weak_self = this->weak_from_this(), id = connection_id_, conn](const beast::error_code ec) mutable {
                const auto s = weak_self.lock();
                if (!s) return;

                auto& self = *s;
                if (self.stopped_ || self.closed_ || (self.connection_id_ != id) || (self.conn_.get() != conn.get())) return;

                if (ec) {
                    self.handle_error("SSL Handshake", ec);
                    return;
                }
                self.do_ws_handshake(std::move(conn));
            });
    }

    void do_ws_handshake(std::shared_ptr<connection_state> conn) {
        beast::get_lowest_layer(conn->ws).expires_never();

        static constexpr websocket::stream_base::timeout options{
            .handshake_timeout = std::chrono::seconds{30},
            .idle_timeout = std::chrono::minutes{5},
            .keep_alive_pings = false,
        };

        conn->ws.set_option(options);
        conn->ws.auto_fragment(false);

        conn->ws.async_handshake(host_header_, target_,
            [weak_self = this->weak_from_this(), id = connection_id_, conn](const beast::error_code ec) mutable {
                const auto s = weak_self.lock();
                if (!s) return;

                auto& self = *s;
                if (self.stopped_ || self.closed_ || (self.connection_id_ != id) || (self.conn_.get() != conn.get())) return;

                if (ec) {
                    self.handle_error("WebSocket Handshake", ec);
                    return;
                }

                self.open_ = true;

                self.on_open_cb_();

                if (self.stopped_ || self.closed_ || !self.open_ || (self.connection_id_ != id) || (self.conn_.get() != conn.get())) return;

                self.do_read(std::move(conn));
                self.drain();
            });
    }

    void do_read(std::shared_ptr<connection_state> conn) {
        if (stopped_ || closed_ || !open_ || !conn || (conn_.get() != conn.get())) return;

        auto& ws = conn->ws;
        auto& read_buf = conn->read_buffer;
        ws.async_read(read_buf, [weak_self = this->weak_from_this(), id = connection_id_, conn = std::move(conn)](const beast::error_code ec, std::size_t) mutable {
            const auto s = weak_self.lock();
            if (!s) return;

            auto& self = *s;
            if (self.stopped_ || self.closed_ || (self.connection_id_ != id) || (self.conn_.get() != conn.get())) return;

            if (ec) {
                if (ec == websocket::error::closed) {
                    const auto code = static_cast<std::uint16_t>(conn->ws.reason().code);
                    self.fail(code != 0U ? code : CLOSE_TRANSPORT);
                } else {
                    self.handle_error("Read Loop", ec);
                }
                return;
            }

            const auto data = conn->read_buffer.data();
            self.on_message_cb_(std::string_view{
                static_cast<char const*>(data.data()),
                data.size(),
            }, conn->ws.got_binary());

            if (self.stopped_ || self.closed_ || !self.open_ || (self.connection_id_ != id) || (self.conn_.get() != conn.get())) return;

            conn->read_buffer.clear();
            if (conn->read_buffer.capacity() > READ_BUFFER_SHRINK_THRESHOLD) {
                conn->read_buffer.shrink_to_fit();
            }
            self.do_read(std::move(conn));
        });
    }

    void drain() {
        if (stopped_ || closed_ || !open_ || !conn_ || conn_->writing) return;

        message_t next_msg;
        if (!priority_pending_.empty()) {
            next_msg = std::move(priority_pending_.front());
            priority_pending_.pop_front();
        } else if (!paused_ && !timer_active_ && !ratelimited_pending_.empty()) {
            next_msg = std::move(ratelimited_pending_.front());
            ratelimited_pending_.pop_front();
        } else {
            return;
        }

        auto conn = conn_;
        conn->writing = true;
        conn->current_msg = std::move(next_msg);

        conn->ws.binary(conn->current_msg.binary);
        auto& ws = conn->ws;
        const auto buf = net::buffer(conn->current_msg.payload);
        ws.async_write(buf,
            [weak_self = this->weak_from_this(), id = connection_id_, conn = std::move(conn)](const beast::error_code ec, std::size_t) {
                const auto s = weak_self.lock();
                if (!s) return;

                auto& self = *s;
                if (self.stopped_ || self.closed_ || (self.connection_id_ != id) || (self.conn_.get() != conn.get())) return;

                conn->writing = false;
                const bool was_rate_limited = conn->current_msg.rate_limited;
                conn->current_msg.payload.clear();
                if (conn->current_msg.payload.capacity() > WRITE_BUFFER_SHRINK_THRESHOLD) {
                    conn->current_msg.payload.shrink_to_fit();
                }

                if (ec) {
                    self.handle_error("Write", ec);
                    return;
                }

                if (was_rate_limited) {
                    self.start_rate_limit_timer();
                }
                self.drain();
            });
    }

    void handle_error([[maybe_unused]] const std::string_view context, [[maybe_unused]] const beast::error_code ec) {
        #ifdef DISCUSY_LOGGING
        log::Logger{}("[Websocket Error] Context: {} | Message: {}", context, ec.message());
        #endif
        fail(CLOSE_TRANSPORT);
    }
};

}
