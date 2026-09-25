#pragma once

#include <functional>
#include <shared_mutex>
#include <string>
#include <stop_token>
#include <atomic>
#include <memory>

#include <boost/asio/ssl.hpp>

#include "http_client.hpp"
#include "timers.hpp"
#include "gateway_events.hpp"
#include "callback.hpp"
#include "types.hpp"

namespace discusy {

class bot;

struct gateway_callbacks {
    discusy::ctx::io_context& io_ctx_;
    discusy::bot* bot_ptr_{nullptr};

    explicit gateway_callbacks(discusy::ctx::io_context& ctx) noexcept : io_ctx_{ctx} {}

    void set_bot(bot* b) noexcept {
        bot_ptr_ = b;
#define DISCUSY_X(UPPER, LOWER) on_##LOWER.set_bot(b);
        DISCORD_GATEWAY_EVENTS
#undef DISCUSY_X
    }

#define DISCUSY_X(UPPER, LOWER) Callback<discusy::recieve_event::LOWER> on_##LOWER{io_ctx_};
    DISCORD_GATEWAY_EVENTS
#undef DISCUSY_X
};

struct state {
    boost::asio::ssl::context ssl_ctx{boost::asio::ssl::context::tls_client};

    discusy::bot* bot_ptr{nullptr};
    discusy::http_client client;
    discusy::Timers timers;
    discusy::gateway_callbacks gateway_callbacks;

    Callback<discusy::user::user> shards_ready;

    Callback<std::uint32_t> shard_session_invalidated;

    opt<CounterCallback<std::uint32_t>> shards_ready_counter{std::nullopt};

    std::string gateway_url{};
    std::string application_id{};
    discusy::user::user me{};
    opt<std::once_flag> ready_flag{std::nullopt};
    std::shared_mutex details_mtx{};

    std::atomic<std::shared_ptr<const discusy::send_event::update_presence>> presence{nullptr};

    opt<std::stop_source> sharding_required{std::nullopt};
    opt<std::stop_callback<std::move_only_function<void()>>> sharding_required_cb{std::nullopt};

    opt<std::stop_source> fatal_error{std::nullopt};
    opt<std::stop_callback<std::move_only_function<void()>>> fatal_error_cb{std::nullopt};
};

}