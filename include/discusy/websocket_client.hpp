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
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/circular_buffer.hpp>

#include "io_context.hpp"
#include "log.hpp" // IWYU pragma: keep

namespace discusy::ws {

namespace beast = boost::beast;
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
class websocket_client : public std::enable_shared_from_this<websocket_client> {
public:
    template <typename Sig>
    struct callback_ref;

    template <typename... Args>
    struct callback_ref<void(Args...)> {
        void* ctx{nullptr};
        void (*fn)(void*, Args...){nullptr};

        constexpr void operator()(Args... args) const {
            if (fn != nullptr) {
                fn(ctx, std::forward<Args>(args)...);
            }
        }

        template <auto Method, typename Obj>
        [[nodiscard]] static constexpr callback_ref bind(Obj* obj) noexcept {
            if (obj == nullptr) return {};
            return {
                static_cast<void*>(obj),
                [](void* p, Args... args) {
                    (static_cast<Obj*>(p)->*Method)(std::forward<Args>(args)...);
                },
            };
        }
    };

    using open_callback = callback_ref<void()>;
    using connect_ready_callback = callback_ref<void()>;
    using close_callback = callback_ref<void(std::uint16_t)>;
    using message_callback = callback_ref<void(std::string_view, bool)>;

private:
    static constexpr std::size_t INITIAL_PRIORITY_QUEUE_CAPACITY = 16;
    static constexpr std::size_t INITIAL_RATELIMITED_QUEUE_CAPACITY = 64;
    static constexpr std::size_t READ_BUFFER_SHRINK_THRESHOLD = 256UZ * 1024;
    static constexpr std::size_t WRITE_BUFFER_SHRINK_THRESHOLD = 64UZ * 1024;

    struct connection_state;

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

    open_callback on_open_cb_{};
    close_callback on_close_cb_{};
    message_callback on_message_cb_{};
    connect_ready_callback on_connect_ready_{};

    static void push_queue(boost::circular_buffer<message_t>& q, message_t&& msg, std::size_t initial_cap);
    bool set_url(std::string_view url_str);

public:
    websocket_client(websocket_client&&) = delete;
    websocket_client& operator=(websocket_client&&) = delete;
    websocket_client(const websocket_client&) = delete;
    websocket_client& operator=(const websocket_client&) = delete;

    explicit websocket_client(ctx::io_context::strand_t strand, net::ssl::context& ctx);

    // must run from the strand itself
    ~websocket_client() noexcept;

    [[nodiscard]] std::vector<std::string> extract_pending_payloads();

    void stop();

    // Immediately aborts any active/pending connection without invoking close or connect-ready callbacks.
    void disconnect();

    [[nodiscard]] bool stopped() const noexcept { return stopped_; }

    [[nodiscard]] bool is_open() const noexcept { return open_ && !closed_ && !stopped_; }

    void on_open(const open_callback cb) noexcept { on_open_cb_ = cb; }

    void on_connect_ready(const connect_ready_callback cb) noexcept { on_connect_ready_ = cb; }

    void on_close(const close_callback cb) noexcept { on_close_cb_ = cb; }

    void on_message(const message_callback cb) noexcept { on_message_cb_ = cb; }

    [[nodiscard]] bool connect(std::string_view url);

    void send_ratelimited(std::string&& payload, bool binary = false);
    void send(std::string&& payload, bool binary = false);

    void pause();
    void resume();
    void clear();
    void close();

private:
    void teardown_transport();
    void hard_reset();
    void notify_closed(std::uint16_t code);
    void notify_ready();
    void fail(std::uint16_t code);
    void start_rate_limit_timer();
    void do_resolve();
    void do_connect(std::shared_ptr<connection_state> conn, const tcp::resolver::results_type& results);
    void do_ssl_handshake(std::shared_ptr<connection_state> conn);
    void do_ws_handshake(std::shared_ptr<connection_state> conn);
    void do_read(std::shared_ptr<connection_state> conn);
    void drain();
    void handle_error(std::string_view context, beast::error_code ec);
};

}
