#pragma once

#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/cancel_after.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/url.hpp>

#include "io_context.hpp"
#include "log.hpp" // IWYU pragma: keep

namespace discusy::ws {

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using boost::asio::ip::tcp;

inline constexpr std::uint16_t CLOSE_LOCAL = 100;     // our own graceful close completed
inline constexpr std::uint16_t CLOSE_TRANSPORT = 101; // died outside of a websocket close frame

// all methods must be called from the strand_ itself
template <typename openHandler, typename closeHandler, typename messageHandler, typename connectHandler>
requires (
    std::invocable<std::decay_t<openHandler>&> && std::invocable<std::decay_t<connectHandler>&> &&
    std::invocable<std::decay_t<closeHandler>&, std::uint16_t> && std::invocable<std::decay_t<messageHandler>&, std::string_view, bool>
)
// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
class websocket_client : public std::enable_shared_from_this<websocket_client<openHandler, closeHandler, messageHandler, connectHandler>> {
    struct message_t {
        std::string payload;
        bool rate_limited{false};
        bool binary{false};
    };

    using stream_t = websocket::stream<net::ssl::stream<ctx::io_context::strand_tcp_stream_t>>;

    ctx::io_context::strand_t strand_;
    net::ssl::context& ctx_;
    ctx::io_context::strand_tcp_resolver_t resolver_;

    std::shared_ptr<stream_t> ws_;
    ctx::io_context::strand_timer_t timer_;

    std::deque<message_t> pending_;
    message_t current_msg_;

    std::uint64_t connection_id_ = 0;

    bool timer_active_ = false;
    bool is_writing_ = false;
    bool stopped_ = false;
    bool paused_ = false;
    bool open_ = false;
    bool closing_ = false;
    bool closed_ = true;
    bool close_requested_ = false;
    bool ready_posted_ = false;

    beast::flat_buffer read_buffer_;

    std::string host_;
    std::string port_;
    std::string target_;

    std::decay_t<openHandler> on_open_cb_;
    std::decay_t<closeHandler> on_close_cb_;
    std::decay_t<messageHandler> on_message_cb_;
    std::decay_t<connectHandler> on_connect_ready_;

    bool set_url(std::string_view url_str) {
        auto result = boost::urls::parse_uri(url_str);
        if (!result) return false;

        auto const& url = *result;

        if (url.scheme() != "wss") {
            return false;
        }

        host_ = url.host();

        port_ = url.has_port() ? url.port() : "443";

        target_ = url.encoded_resource();
        if (target_.empty() || target_.front() != '/') target_ = "/" + target_;

        return true;
    }

public:
    explicit websocket_client(ctx::io_context::strand_t strand, net::ssl::context& ctx)
        : strand_{std::move(strand)},
          ctx_{ctx},
          resolver_{strand_},
          timer_{strand_} {}

    // must run from the strand itself
    ~websocket_client() {
        stopped_ = true;
        hard_reset();
    }

    [[nodiscard]] std::vector<std::string> extract_pending_payloads() {
        std::vector<std::string> evacuated;
        evacuated.reserve(pending_.size());
        for (auto& msg : pending_) {
            if (!msg.rate_limited) continue; // skip identifying related payloads

            evacuated.emplace_back(std::move(msg.payload));
        }
        pending_.clear();
        return evacuated;
    }

    void stop() {
        stopped_ = true;
        hard_reset();
    }

    [[nodiscard]] bool stopped() const noexcept { return stopped_; }

    [[nodiscard]] bool is_open() const noexcept { return open_ && !closing_; }

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

    [[nodiscard]] bool connect(std::string_view url) {
        if (stopped_) return false;

        hard_reset();

        closing_ = false;
        closed_ = false;
        close_requested_ = false;
        ready_posted_ = false;
        std::erase_if(pending_, [](const auto& msg) { return !msg.rate_limited; });
        read_buffer_.consume(read_buffer_.size());

        if (!set_url(url)) return false;

        ws_ = std::make_shared<stream_t>(strand_, ctx_);

        do_resolve();
        return true;
    }

    void send_ratelimited(std::string&& payload, bool binary = false) {
        if (stopped_) return;

        try {
            pending_.emplace_back(std::move(payload), true, binary);
            drain();
        } catch (const std::bad_alloc&) {
            #ifdef DISCUSY_LOGGING
            log::Logger{}("[websocket_client] send_ratelimited bad_alloc");
            #endif
        }
    }

    void send(std::string&& payload, bool binary = false) {
        if (stopped_) return;

        try {
            pending_.emplace_front(std::move(payload), false, binary);

            if (timer_active_) {
                timer_.cancel();
                timer_active_ = false;
            }

            drain();
        } catch (const std::bad_alloc&) {
            #ifdef DISCUSY_LOGGING
            log::Logger{}("[websocket_client] send bad_alloc");
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
        pending_.clear();
    }

    void close() {
        if (stopped_) return;

        close_requested_ = true;

        if (closed_) {
            notify_ready();
            return;
        }

        if (closing_) {
            if (ws_) beast::get_lowest_layer(*ws_).close();
            return;
        }

        closing_ = true;
        pause();
        timer_.cancel();
        timer_active_ = false;

        if (ws_ && open_) {
            if (!is_writing_) do_close();
        } else {
            notify_closed(CLOSE_TRANSPORT);
        }
    }

private:
    void hard_reset() {
        ++connection_id_;

        open_ = false;
        is_writing_ = false;
        timer_active_ = false;

        timer_.cancel();
        resolver_.cancel();

        if (ws_) {
            beast::get_lowest_layer(*ws_).close();
            ws_.reset();
        }
    }

    void notify_closed(const std::uint16_t code) {
        if (stopped_ || closed_) return;

        closed_ = true;
        open_ = false;
        closing_ = true;

        net::post(strand_, [s = this->shared_from_this(), id = connection_id_, code]() {
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

        net::post(strand_, [s = this->shared_from_this(), id = connection_id_]() {
            auto& self = *s;
            self.ready_posted_ = false;
            if (self.stopped_ || (self.connection_id_ != id)) return;

            self.close_requested_ = false;
            self.on_connect_ready_();
        });
    }

    void fail(const std::uint16_t code) {
        if (stopped_ || closed_) return;

        closing_ = true;
        paused_ = true;
        timer_.cancel();
        timer_active_ = false;

        if (ws_) beast::get_lowest_layer(*ws_).close();
        open_ = false;

        notify_closed(code);
    }

    void do_close() {
        if (stopped_ || closed_) return;
        if (!ws_) return notify_closed(CLOSE_TRANSPORT);

        beast::get_lowest_layer(*ws_).close();
        open_ = false;
        notify_closed(CLOSE_LOCAL);
    }

    void do_resolve() {
        if (!SSL_set_tlsext_host_name(ws_->next_layer().native_handle(), host_.c_str())) {
            beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
            return handle_error("SSL SNI Setup", ec);
        }

        ws_->next_layer().set_verify_callback(net::ssl::host_name_verification(host_));

        resolver_.async_resolve(host_, port_,
            boost::asio::cancel_after(std::chrono::seconds{31}, [weak_self = this->weak_from_this(), id = connection_id_](beast::error_code ec, const tcp::resolver::results_type& results) {
                if (auto s = weak_self.lock()) {
                    auto& self = *s;
                    if (self.stopped_ || (self.connection_id_ != id)) return;

                    if (ec) return self.handle_error("Resolve", ec);
                    self.do_connect(results);
                }
            }));
    }

    void do_connect(const tcp::resolver::results_type& results) {
        beast::get_lowest_layer(*ws_).expires_after(std::chrono::seconds{30});

        beast::get_lowest_layer(*ws_).async_connect(results,
            [weak_self = this->weak_from_this(), id = connection_id_, stream = ws_](beast::error_code ec, const tcp::resolver::results_type::endpoint_type&) {
                if (auto s = weak_self.lock()) {
                    auto& self = *s;
                    if (self.stopped_ || (self.connection_id_ != id)) return;

                    if (ec) return self.handle_error("Connect", ec);
                    boost::system::error_code opt_ec;
                    auto& sock = beast::get_lowest_layer(*self.ws_).socket();
                    sock.set_option(tcp::no_delay(true), opt_ec);
                    sock.set_option(boost::asio::socket_base::keep_alive(true), opt_ec);
                    self.do_ssl_handshake();
                }
            });
    }

    void do_ssl_handshake() {
        beast::get_lowest_layer(*ws_).expires_after(std::chrono::seconds{30});

        ws_->next_layer().async_handshake(net::ssl::stream_base::client,
            [weak_self = this->weak_from_this(), id = connection_id_, stream = ws_](beast::error_code ec) {
                if (auto s = weak_self.lock()) {
                    auto& self = *s;
                    if (self.stopped_ || (self.connection_id_ != id)) return;

                    if (ec) return self.handle_error("SSL Handshake", ec);
                    self.do_ws_handshake();
                }
            });
    }

    void do_ws_handshake() {
        beast::get_lowest_layer(*ws_).expires_never();

        static constexpr websocket::stream_base::timeout options{
            .handshake_timeout = std::chrono::seconds{30},
            .idle_timeout = std::chrono::minutes{5},
            .keep_alive_pings = false,
        };

        ws_->set_option(options);

        std::string host_header = host_;
        if (port_ != "443") {
            host_header += ':';
            host_header += port_;
        }

        ws_->async_handshake(host_header, target_,
            [weak_self = this->weak_from_this(), id = connection_id_, stream = ws_](beast::error_code ec) {
                if (auto s = weak_self.lock()) {
                    auto& self = *s;
                    if (self.stopped_ || (self.connection_id_ != id)) return;

                    if (ec) return self.handle_error("WebSocket Handshake", ec);

                    self.open_ = true;

                    self.on_open_cb_();

                    if (self.stopped_ || (self.connection_id_ != id)) return;

                    self.do_read();
                    self.drain();
                }
            });
    }

    void do_read() {
        if (stopped_ || !ws_ || !open_) return;

        ws_->async_read(read_buffer_, [weak_self = this->weak_from_this(), id = connection_id_, stream = ws_](beast::error_code ec, std::size_t) {
            if (auto s = weak_self.lock()) {
                auto& self = *s;

                if (self.stopped_ || (self.connection_id_ != id)) return; // we have moved on

                if (ec) {
                    if (ec == websocket::error::closed) {
                        const auto reason = self.ws_ ? self.ws_->reason().code : websocket::close_code::none;
                        const auto code = static_cast<std::uint16_t>(reason);

                        self.paused_ = true;
                        self.timer_.cancel();
                        self.timer_active_ = false;
                        self.open_ = false;
                        self.notify_closed(code ? code : CLOSE_TRANSPORT);
                    } else if (ec == net::error::operation_aborted) {
                        // cancelled by us, whoever cancelled it owns the teardown
                    } else {
                        self.handle_error("Read Loop", ec);
                    }
                    return;
                }

                auto mutable_seq = self.read_buffer_.data();
                self.on_message_cb_(std::string_view{
                    static_cast<char const*>(mutable_seq.data()),
                    mutable_seq.size(),
                }, self.ws_->got_binary());

                if (self.stopped_ || (self.connection_id_ != id)) return; // handler tore us down

                self.read_buffer_.consume(self.read_buffer_.size());
                self.do_read();
            }
        });
    }

    void drain() {
        if (stopped_ || closing_ || !open_ || !ws_) return;
        if (is_writing_ || timer_active_ || pending_.empty()) return;

        if (paused_ && pending_.front().rate_limited) {
            return;
        }

        is_writing_ = true;
        current_msg_ = std::move(pending_.front());
        pending_.pop_front();

        ws_->binary(current_msg_.binary);
        ws_->async_write(net::buffer(current_msg_.payload),
            [s = this->shared_from_this(), id = connection_id_, stream = ws_](beast::error_code ec, std::size_t) {
                auto& self = *s;
                if (self.stopped_ || (self.connection_id_ != id)) return;

                self.is_writing_ = false;

                if (self.closing_) return self.do_close();

                if (ec) return self.handle_error("Write", ec);

                if (self.current_msg_.rate_limited) {
                    self.timer_active_ = true;
                    self.timer_.expires_after(std::chrono::milliseconds{600});
                    self.timer_.async_wait([weak_self = self.weak_from_this(), id](beast::error_code ec) {
                        if (auto s = weak_self.lock()) {
                            auto& self = *s;
                            if (self.stopped_ || (self.connection_id_ != id)) return;

                            self.timer_active_ = false;
                            if (!ec) self.drain();
                        }
                    });
                } else {
                    self.drain();
                }
            });
    }

    void handle_error([[maybe_unused]] const std::string_view context, [[maybe_unused]] beast::error_code ec) {
        #ifdef DISCUSY_LOGGING
        log::Logger{}("[Websocket Error] Context: {} | Message: {}", context, ec.message());
        #endif
        fail(CLOSE_TRANSPORT);
    }
};

}
