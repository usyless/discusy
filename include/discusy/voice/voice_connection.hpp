#pragma once

#include <algorithm>
#include <atomic>
#include <concepts>
#include <functional>
#include <limits>
#include <mutex>
#include <optional>
#include <ranges>
#include <array>
#include <thread>
#include <unordered_map>
#include <vector>
#include <version>

#include <sodium.h>
#include <opus.h>

#include "../types.hpp"
#include "../websocket_client.hpp"
#include "../opcode.hpp"
#include "../state.hpp"
#include "../random.hpp"
#include "udp_client.hpp"
#include "dave.hpp"
#include "internal.hpp"
#include "opus_encoder.hpp"
#include "opus_decoder.hpp"

namespace discusy::voice {

// recieve structs
struct recieve_payload_base {
    VoiceOpcode op{};
    opt<integer> seq{};
};

template <typename T>
struct recieve_payload {
    T d;
};

struct ready {
    std::uint32_t ssrc{};
    std::string ip{};
    std::uint16_t port{};
    std::vector<std::string_view> modes{}; // view used here as no unicode in these and buffer stays alive
    // heartbeat interval ignored here
};

struct hello {
    integer heartbeat_interval{};
    opt<integer> v{};
};

struct heartbeat_ack_quoted {
    integer t;

    struct glaze {
        using T = heartbeat_ack_quoted;
        static constexpr auto value = glz::object(
            "t", glz::quoted_num<&T::t>
        );
    };
};

struct heartbeat_ack_bare {
    integer t;
};

// update with DAVE updates
inline constexpr discord::dave::ProtocolVersion DAVE_PROTOCOL_VERSION = 1;

struct session_description {
    std::string_view mode{};
    std::array<uint8_t, 32> secret_key{};
    discord::dave::ProtocolVersion dave_protocol_version{};
};

// https://daveprotocol.com/#voice-gateway-opcodes
struct clients_connect {
    std::vector<std::string> user_ids{};
};

struct client_disconnect {
    std::string user_id{};
};

struct dave_protocol_prepare_transition {
    std::int64_t protocol_version{};
    std::int64_t transition_id{};
};

struct dave_protocol_execute_transition {
    std::int64_t transition_id{};
};

struct dave_protocol_prepare_epoch   {
    std::int64_t protocol_version{};
    std::int64_t epoch{};
};





// send structs
template <VoiceOpcode code, typename T>
struct send_payload_base {
    VoiceOpcode op = code;
    T& d;

    send_payload_base(T& d_) : d{d_} {}

    struct glaze {
        using U = send_payload_base<code, T>;
        
        static constexpr auto value = glz::object(
            "op", &U::op,
            "d", [](auto&& self) -> auto& { return self.d; }
        );
    };
};

struct identify {
    snowflake server_id{};
    snowflake user_id{};
    std::string session_id{};
    std::string token{};
    discord::dave::ProtocolVersion max_dave_protocol_version{DAVE_PROTOCOL_VERSION};
};

using identify_payload = send_payload_base<VoiceOpcode::Identify, identify>;

struct resume {
    snowflake server_id{};
    std::string session_id{};
    std::string token{};
    integer seq_ack{};
};

using resume_payload = send_payload_base<VoiceOpcode::Resume, resume>;

struct heartbeat {
    integer t{};
    integer seq_ack{};
};

using heartbeat_payload = send_payload_base<VoiceOpcode::Heartbeat, heartbeat>;

struct select_protocol {
    std::string_view protocol{"udp"}; // this is always udp
    struct data {
        std::string address{};
        std::uint16_t port{};
        std::string_view mode{};
    } data{};
};

using select_protocol_payload = send_payload_base<VoiceOpcode::SelectProtocol, select_protocol>;

struct dave_protocol_ready_for_transition  {
    std::int64_t transition_id{};
};

using dave_protocol_ready_for_transition_payload = send_payload_base<VoiceOpcode::DaveTransitionReady, dave_protocol_ready_for_transition>;

struct dave_mls_invalid_commit_welcome {
    std::int64_t transition_id{};
};

using dave_mls_invalid_commit_welcome_payload = send_payload_base<VoiceOpcode::DaveMlsInvalidCommitWelcome, dave_mls_invalid_commit_welcome>;

enum class speaking_mode : std::uint8_t {
    None = 0,
    Microphone = 1U << 0U, // Normal transmission of voice audio
    Soundshare = 1U << 1U, // Transmission of context audio for video, no speaking indicator
    Priority = 1U << 2U, // Priority speaker, lowering audio of other speakers
};

struct speaking_ {
    speaking_mode speaking{};
    std::uint8_t delay{0};
    std::uint32_t ssrc{};
};

using speaking_payload = send_payload_base<VoiceOpcode::Speaking, speaking_>;

struct speaking_recv {
    snowflake user_id{};
    std::uint32_t ssrc{};
    speaking_mode speaking{};
};

using media_sink_wants_payload = send_payload_base<VoiceOpcode::MediaSinkWants, std::unordered_map<std::string, std::uint8_t>>;

struct user_speaking {
    snowflake guild_id{};
    snowflake user_id{};
    std::uint32_t ssrc{};
    speaking_mode speaking{};
};

struct received_opus {
    snowflake guild_id{};
    snowflake user_id{}; // 0 if the user's SSRC has not been announced via Speaking yet
    std::uint32_t ssrc{};
    std::uint16_t sequence{};
    std::uint32_t timestamp{};
    std::vector<std::uint8_t> opus{};
};

struct received_audio {
    snowflake guild_id{};
    snowflake user_id{}; // 0 if the user's SSRC has not been announced via Speaking yet
    std::uint32_t ssrc{};
    std::uint16_t sequence{};
    std::uint32_t timestamp{};
    std::vector<std::int16_t> pcm{}; // 48kHz stereo signed 16-bit PCM
};

struct audio_stopped {
    audio_stop_reason reason{};
    snowflake guild_id{};
};

struct voice_ready {
    snowflake guild_id{};
};

struct voice_closed {
    snowflake guild_id{};
};

struct channel_moved {
    snowflake old_channel_id;
    snowflake new_channel_id;
};

class connection : public std::enable_shared_from_this<connection> {
public:
    using opus_encoder = discusy::voice::opus_encoder;
    using opus_decoder = discusy::voice::opus_decoder;

    // Escalation cb is tiny anyway, fits in SBO
    #if defined(__cpp_lib_move_only_function) && __cpp_lib_move_only_function >= 202110L
    using escalation_cb_t = std::move_only_function<void(escalation_action)>;
    #else
    using escalation_cb_t = std::function<void(escalation_action)>;
    #endif

    connection(connection&&) = delete;
    connection& operator=(connection&&) = delete;
    connection(const connection&) = delete;
    connection& operator=(const connection&) = delete;

    connection(ctx::io_context& ctx, state& state, snowflake server_id, snowflake channel_id, snowflake user_id, std::string session_id, std::string token, std::string endpoint, escalation_cb_t escalation_cb);

    template <typename F>
    requires ( std::invocable<F&, escalation_action> && !std::same_as<std::remove_cvref_t<F>, escalation_cb_t> )
    connection(ctx::io_context& ctx, state& state, snowflake server_id, snowflake channel_id, snowflake user_id, std::string session_id, std::string token, std::string endpoint, F&& escalation_cb)
        : connection(ctx, state, server_id, channel_id, user_id, std::move(session_id), std::move(token), std::move(endpoint), escalation_cb_t{std::forward<F>(escalation_cb)}) {}

    void start();

    // must be called from its own strand, which is the websockets strand
    ~connection() noexcept;

    void move_channel(snowflake new_channel_id);

    void refresh(std::string session_id, std::string token, std::string endpoint, snowflake channel_id);

    [[nodiscard]] bool is_active() const noexcept {
        return !closed_.load(std::memory_order_acquire);
    }

    [[nodiscard]] bool is_ready_to_send() const noexcept {
        return ready_.load(std::memory_order_acquire) && !closed_.load(std::memory_order_acquire);
    }

    void set_speaking_mode(speaking_mode mode);

    void stop_audio();

    void pause_audio();

    void rebuild_in_place();

    void resume_audio();

    [[nodiscard]] double get_secs_remaining() const noexcept;

    [[nodiscard]] std::chrono::milliseconds get_ms_remaining() const noexcept;

    [[nodiscard]] std::uint64_t get_frames_remaining() const noexcept;

    [[nodiscard]] bool has_audio_queued() const noexcept {
        return get_frames_remaining() != 0;
    }

    void set_underrun_grace_frames(std::uint32_t frames);

    void set_max_catchup_frames(std::uint32_t frames);

    void set_not_ready_hold_frames(std::uint32_t frames);

    [[nodiscard]] audio_stats get_audio_stats() const noexcept;

    void reset_audio_stats() noexcept;

    bool set_inband_fec(bool enabled);

    bool set_expected_packet_loss(int percentage);

    bool set_opus_complexity(int complexity);

    // OPUS_SIGNAL_MUSIC / OPUS_SIGNAL_VOICE / OPUS_AUTO
    bool set_opus_signal(int signal);

    bool reset_opus_state() noexcept;

    bool reset_encoder_state() noexcept {
        return reset_opus_state();
    }

    /// Direct encoder accessor. Not synchronized with `send_pcm`/`send_pcm_async` or connection teardown.
    [[nodiscard]] opus_encoder& encoder() noexcept { return encoder_; }
    [[nodiscard]] const opus_encoder& encoder() const noexcept { return encoder_; }

    [[nodiscard]] snowflake get_guild_id() const noexcept { return server_id_; }
    [[nodiscard]] snowflake get_server_id() const noexcept { return server_id_; }

    [[nodiscard]] snowflake get_channel_id() const noexcept {
        return snowflake{channel_id_.load(std::memory_order_acquire)};
    }

    template <typename... Request>
    bool opus_ctl([[maybe_unused]] const std::string_view what, Request&&... request) {
        if (closed_.load(std::memory_order_acquire)) return false;

        bool ok = false;
        {
        const std::scoped_lock lock{encoder_mutex_};
        ok = encoder_.ctl(std::forward<Request>(request)...);
        }
        #ifdef DISCUSY_LOGGING
        if (!ok) {
            log("Failed to set opus {}", what);
        }
        #endif
        return ok;
    }

    // May be called concurrently while encoding is happening
    bool set_bitrate(const std::uint32_t bitrate) noexcept {
        if (closed_.load(std::memory_order_acquire)) return false;
        target_bitrate_.store(bitrate, std::memory_order_relaxed);
        return true;
    }

    [[nodiscard]] std::uint32_t get_bitrate() const noexcept {
        return target_bitrate_.load(std::memory_order_relaxed);
    }

    /// Controls whether incoming audio from users without an explicit override is received (`any: 100` vs `any: 0`).
    void set_receive_any_audio(bool enabled);

    /// Controls whether incoming audio from `user_id` is received (`100` vs `0`), overriding `set_receive_any_audio`.
    void set_receive_user_audio(snowflake user_id, bool enabled);

    /// Restricts incoming audio exclusively to `user_ids` (sets `any` to `0` and enables only the given users).
    /// Passing an empty span disables all incoming audio (`any: 0`).
    void set_received_users(std::span<const snowflake> user_ids);

    void set_received_users(std::initializer_list<snowflake> user_ids) {
        set_received_users(std::span<const snowflake>{user_ids.begin(), user_ids.size()});
    }

    /// Clears all per-user and per-SSRC audio overrides, falling back to `is_receiving_any_audio()`.
    void clear_user_audio_filters();

    /// Stops receiving audio from all users (`any: 0`, clearing per-user overrides).
    void disable_audio_receive() {
        set_received_users(std::span<const snowflake>{});
    }

    /// Receives audio from all users (`any: 100`, clearing per-user overrides).
    void enable_all_audio_receive();

    /// Sends a custom Opcode 15 Media Sink Wants payload directly (keys are `"any"` or SSRC strings; `0` = disabled, `100` = enabled).
    void send_media_sink_wants(const std::unordered_map<std::string, std::uint8_t>& wants);

    [[nodiscard]] bool is_receiving_any_audio() const noexcept;

    [[nodiscard]] opt<snowflake> get_user_for_ssrc(std::uint32_t ssrc) const noexcept;

    [[nodiscard]] opt<std::uint32_t> get_ssrc_for_user(snowflake user_id) const noexcept;

    // make sure the frame isnt bigger than voice::MAX_OPUS_FRAME_BYTES bytes
    void send_opus_frame(std::span<const std::uint8_t> opus_data);

    void send_opus_frames(std::vector<discusy::voice::store<discusy::voice::MAX_OPUS_FRAME_BYTES>>&& opus_frames);

    void send_opus_frames(std::span<const discusy::voice::store<discusy::voice::MAX_OPUS_FRAME_BYTES>> opus_frames);

    // if you send data that doesnt exactly align to the 20ms 48000hz stereo frames silence will be appended
    void send_pcm(
        std::span<const std::int16_t> pcm_data,
        std::chrono::milliseconds batch_duration = std::chrono::seconds{10}
    );

    void send_pcm_bytes(
        std::span<const std::uint8_t> raw_bytes,
        std::chrono::milliseconds batch_duration = std::chrono::seconds{10}
    );

    template <typename Range>
    requires (
        !detail::is_span_v<Range> &&
        std::ranges::contiguous_range<Range> &&
        std::ranges::sized_range<Range> &&
        std::is_same_v<std::remove_cvref_t<std::ranges::range_value_t<Range>>, std::int16_t>
    )
    void send_pcm(
        const Range& pcm_data,
        const std::chrono::milliseconds batch_duration = std::chrono::seconds{10}
    ) {
        if constexpr (std::is_convertible_v<const Range&, std::span<const std::int16_t>>) {
            send_pcm(std::span<const std::int16_t>{pcm_data}, batch_duration);
        } else {
            send_pcm(std::span<const std::int16_t>{std::ranges::data(pcm_data), std::ranges::size(pcm_data)}, batch_duration);
        }
    }

    template <typename Range>
    requires (
        std::ranges::contiguous_range<Range> &&
        std::ranges::sized_range<Range> &&
        (sizeof(std::ranges::range_value_t<Range>) == 1)
    )
    void send_pcm(
        const Range& raw_bytes,
        const std::chrono::milliseconds batch_duration = std::chrono::seconds{10}
    ) {
        send_pcm_bytes(
            std::span<const std::uint8_t>{
                reinterpret_cast<const std::uint8_t*>(std::ranges::data(raw_bytes)),
                std::ranges::size(raw_bytes)
            },
            batch_duration
        );
    }

    // Needs to be an ownable container, not a view or span over data
    // Does not block for too long by batching the audio it queues
    template <
        typename Container,
        discusy::asio::ctf<> CompletionToken = ctx::io_context::dct_t
    >
    requires (
        !std::is_lvalue_reference_v<Container> &&
        !std::ranges::view<std::remove_cvref_t<Container>> &&
        !std::ranges::borrowed_range<std::remove_cvref_t<Container>> &&
        std::ranges::input_range<std::remove_cvref_t<Container>> &&
        (std::convertible_to<std::ranges::range_value_t<std::remove_cvref_t<Container>>, std::int16_t> ||
         sizeof(std::ranges::range_value_t<std::remove_cvref_t<Container>>) == 1)
    )
    auto send_pcm_async(
        Container&& pcm_data,
        const std::chrono::milliseconds batch_duration,
        CompletionToken&& token = ctx::io_context::dct_t{}
    ) {
        return boost::asio::async_initiate<CompletionToken, void(boost::system::error_code)>(
            [](discusy::asio::chf<> auto&& handler, connection* self, const std::chrono::milliseconds batch_duration, auto&& owned_buffer) mutable {
                if (self->closed_.load(std::memory_order_acquire) || (self->udp_client() == nullptr)) {
                    auto ex_imm = boost::asio::get_associated_immediate_executor(handler, self->io_ctx_.executor_);
                    boost::asio::dispatch(
                        ex_imm,
                        boost::asio::append(
                            std::forward<decltype(handler)>(handler),
                            boost::asio::error::make_error_code(boost::asio::error::operation_aborted)
                        )
                    );
                    return;
                }

                using OwnedContainer = decltype(owned_buffer);
                using RawContainer = std::remove_cvref_t<OwnedContainer>;

                using handler_t = decltype(handler);

                auto launch_task = [self = self->shared_from_this(), batch_duration, handler = std::forward<handler_t>(handler)]<typename ContiguousContainer>(ContiguousContainer&& owned_buffer) mutable {
                    struct task_state {
                        std::remove_cvref_t<handler_t> handler;
                        std::shared_ptr<connection> self;
                        std::remove_cvref_t<ContiguousContainer> data;
                        ctx::io_context::strand_t executor;
                        std::size_t offset{0};
                        std::chrono::milliseconds batch_duration{};
                        decltype(
                            boost::asio::make_work_guard(boost::asio::get_associated_executor(handler, std::declval<ctx::io_context::executor_t>()))
                        ) work_guard_;
                        boost::asio::cancellation_type cancelled_type{boost::asio::cancellation_type::none};

                        void operator()(std::shared_ptr<task_state> ptr) {
                            boost::system::error_code ec{};
                            if (self->closed_.load(std::memory_order_acquire) || (self->udp_client() == nullptr) || (cancelled_type != boost::asio::cancellation_type::none)) {
                                ec = boost::asio::error::operation_aborted;
                            }

                            if (!ec) {
                                try {
                                    using ElemType = std::remove_cvref_t<std::ranges::range_value_t<std::remove_cvref_t<ContiguousContainer>>>;
                                    static constexpr size_t SAMPLES_PER_FRAME = (DISCORD_SAMPLING_RATE / 1000) * DISCORD_FRAME_SIZE_MS;
                                    static constexpr size_t TOTAL_FRAME_SAMPLES = SAMPLES_PER_FRAME * DISCORD_CHANNELS;
                                    static constexpr size_t ELEMENTS_PER_FRAME = (sizeof(ElemType) == 1) 
                                        ? (TOTAL_FRAME_SAMPLES * sizeof(std::int16_t)) 
                                        : TOTAL_FRAME_SAMPLES;

                                    const auto total_elements = std::ranges::size(data);
                                    const auto duration_ms = std::max<std::int64_t>(0, batch_duration.count());
                                    const size_t batch_frames_limit = std::max<size_t>(
                                        1,
                                        static_cast<size_t>(duration_ms) / DISCORD_FRAME_SIZE_MS
                                    );
                                    const size_t batch_elements_limit = batch_frames_limit * ELEMENTS_PER_FRAME;

                                    const size_t remaining = total_elements - offset;
                                    const size_t chunk_elements = std::min(remaining, batch_elements_limit);

                                    if (chunk_elements > 0) {
                                        if constexpr (sizeof(ElemType) == 1) {
                                            const auto* raw_ptr = reinterpret_cast<const char*>(std::ranges::data(data)) + offset;
                                            self->send_pcm(std::span<const char>{raw_ptr, chunk_elements}, batch_duration);
                                        } else {
                                            const auto* raw_ptr = std::ranges::data(data) + offset;
                                            self->send_pcm(std::span<const std::int16_t>{raw_ptr, chunk_elements}, batch_duration);
                                        }

                                        offset += chunk_elements;
                                    }

                                    if (offset < total_elements && !self->closed_.load(std::memory_order_acquire) && (cancelled_type == boost::asio::cancellation_type::none)) {
                                        auto ex = executor;
                                        auto alloc = boost::asio::get_associated_allocator(
                                            handler,
                                            boost::asio::recycling_allocator<void>{}
                                        );
                                        // ptr is not moved in case post fails
                                        boost::asio::post(std::move(ex), boost::asio::bind_allocator(std::move(alloc), [ptr]() mutable {
                                            auto* c = ptr.get();
                                            c->operator()(std::move(ptr));
                                        }));
                                        return;
                                    }

                                    if (offset < total_elements) {
                                        ec = boost::asio::error::operation_aborted;
                                    }
                                }
                                #ifdef DISCUSY_LOGGING
                                catch (const std::exception& e) {
                                    self->log("send_pcm_async exception: {}", e.what());
                                    ec = boost::asio::error::fault;
                                }
                                #endif
                                catch (...) {
                                    #ifdef DISCUSY_LOGGING
                                    self->log("send_pcm_async unknown exception");
                                    #endif
                                    ec = boost::asio::error::fault;
                                }
                            }

                            auto ex = boost::asio::get_associated_executor(handler, self->io_ctx_.executor_);
                            auto alloc = boost::asio::get_associated_allocator(
                                handler,
                                boost::asio::recycling_allocator<void>{}
                            );
                            auto h = std::move(this->handler);
                            auto work_guard = std::move(this->work_guard_);
                            ptr.reset();

                            boost::asio::dispatch(ex, boost::asio::bind_allocator(alloc, [handler = std::move(h), work_guard = std::move(work_guard), ec]() mutable {
                                auto slot = boost::asio::get_associated_cancellation_slot(handler);
                                if (slot.is_connected()) slot.clear();
                                work_guard.reset();
                                std::move(handler)(ec);
                            }));
                        }
                    };

                    auto alloc = boost::asio::get_associated_allocator(
                        handler,
                        boost::asio::recycling_allocator<void>{}
                    );

                    auto ex = self->io_ctx_.make_strand();
                    auto ex_user = boost::asio::get_associated_executor(handler, self->io_ctx_.executor_);

                    auto state = std::allocate_shared<task_state>(alloc, 
                        std::move(handler), 
                        std::move(self), 
                        std::forward<ContiguousContainer>(owned_buffer),
                        ex,
                        0,
                        batch_duration,
                        boost::asio::make_work_guard(std::move(ex_user))
                    );

                    auto slot = boost::asio::get_associated_cancellation_slot(state->handler);
                    if (slot.is_connected()) {
                        slot.assign([weak_state = std::weak_ptr<task_state>{state}, raw = state.get(), alloc](boost::asio::cancellation_type type) {
                            if (type == boost::asio::cancellation_type::none) return;

                            if (std::shared_ptr<task_state> st = weak_state.lock()) {
                                boost::asio::post(raw->executor, boost::asio::bind_allocator(alloc, [state = std::move(st), type]() {
                                    state->cancelled_type = type;
                                }));
                            }
                        });
                    }

                    boost::asio::post(ex, boost::asio::bind_allocator(alloc, [state = std::move(state)]() mutable {
                        auto* c = state.get();
                        c->operator()(std::move(state));
                    }));
                };

                if constexpr (std::ranges::contiguous_range<RawContainer> &&
                            (std::is_same_v<std::remove_cvref_t<std::ranges::range_value_t<RawContainer>>, std::int16_t> ||
                             sizeof(std::ranges::range_value_t<RawContainer>) == 1)) {
                    std::move(launch_task)(std::forward<OwnedContainer>(owned_buffer));
                } else if constexpr (sizeof(std::ranges::range_value_t<RawContainer>) == 1) {
                    std::string bytes_vec;
                    if constexpr (std::ranges::sized_range<RawContainer>) {
                        bytes_vec.reserve(std::ranges::size(owned_buffer));
                    }
                    for (auto&& b : owned_buffer) {
                        bytes_vec.push_back(static_cast<char>(b));
                    }
                    std::move(launch_task)(std::move(bytes_vec));
                } else {
                    std::vector<std::int16_t> vec;
                    if constexpr (std::ranges::sized_range<RawContainer>) {
                        vec.reserve(std::ranges::size(owned_buffer));
                    }
                    for (auto&& sample : owned_buffer) {
                        vec.push_back(static_cast<std::int16_t>(sample));
                    }
                    std::move(launch_task)(std::move(vec));
                }
            }, 
            token, this, std::move(batch_duration), std::forward<Container>(pcm_data)
        );
    }

    template <typename Container, discusy::asio::ctf<> CompletionToken = ctx::io_context::dct_t>
    auto send_pcm_async(Container&& pcm_data, CompletionToken&& token = ctx::io_context::dct_t{}) {
        return send_pcm_async(std::forward<Container>(pcm_data), std::chrono::seconds{10}, std::forward<CompletionToken>(token));
    }

private:
    // must be called from its own strand, which is the websockets strand
    void destruct();

public:
    template <typename botT>
    friend class client;
private:
    #ifdef DISCUSY_LOGGING
    template <typename ...Args>
    void log(const std::format_string<Args...>& s, Args&&... args) {
        log::Logger{}("{}{}", ulp::str::concat_strings("[discusy voice conn ", discusy::number_str{channel_id_.load(std::memory_order_relaxed)}, "] "), std::format(s, std::forward<Args>(args)...));
    }
    #endif

    void stop();

    void escalate(escalation_action action);

    void await_gateway_instruction();

    void cancel_refresh_watchdog();

    void arm_refresh_watchdog();

    void begin_fresh_session(bool reset_backoff = true);

    void maybe_signal_ready();

    void handle_ws_open();

    void handle_ws_connect_ready();

    void handle_ws_close(std::uint16_t code);

    void handle_udp_failure(const boost::system::error_code& ec);

    void handle_ws_message(std::string_view msg, bool binary);

    void setup_gateway_callbacks();

    void reconnect_internal();

    void reconnect();

    void handle_binary_payload(std::string_view data);

    void handle_gateway_payload(std::string_view json_payload);

    void dave_init_session(discord::dave::ProtocolVersion protocol_version);

    void send_identify();

    void send_binary_dave_payload(VoiceOpcode op, std::span<const uint8_t> data);

    void send_invalid_commit_welcome(discord::dave::TransitionId transition_id);

    void maybe_send_ready_for_transition(discord::dave::TransitionId transition_id);

    void prepare_dave_transition(discord::dave::TransitionId transition_id, discord::dave::ProtocolVersion protocol_version);

    void execute_dave_transition_version(discord::dave::ProtocolVersion protocol_version);

    void drop_dave_keys();

    void ensure_user_decryptor(snowflake uid);

    void update_all_decryptors_key_ratchets();

    void update_all_decryptors_passthrough(bool passthrough);

    [[nodiscard]] bool has_non_default_sink_wants_locked() const noexcept;

    [[nodiscard]] bool should_sync_sink_wants_on_resume() const noexcept;

    void sync_sink_wants_on_ready();

    void dispatch_media_sink_wants();

    [[nodiscard]] std::unordered_map<std::string, std::uint8_t> build_media_sink_wants_locked() const;

    void send_media_sink_wants_current();

    void send_media_sink_wants_payload(std::unordered_map<std::string, std::uint8_t>& wants);

    void send_speaking(std::optional<speaking_mode> mode_override = std::nullopt);

    bool send_heartbeat();

    void start_heartbeat(std::chrono::milliseconds interval);

    void stop_heartbeat();

    #ifdef DISCUSY_LOGGING
    friend log::SelfLogger<connection>;
    log::SelfLogger<connection> json_logger{*this};
    #else
    log::Logger json_logger{};
    #endif

    rnd::Random64 rnd_{1};
    ctx::io_context& io_ctx_;
public:
    const ctx::io_context::strand_t strand_;

    Callback<audio_stopped> on_audio_stopped{io_ctx_};
    Callback<voice_ready> on_ready{io_ctx_};
    Callback<voice_closed> on_closed{io_ctx_};
    Callback<channel_moved> on_channel_move{io_ctx_};
    Callback<user_speaking> on_speaking{io_ctx_};
    /// Note: incoming audio is disabled by default (`MediaSinkWants` `{"any": 0}`). Enable via `join_options::receive_audio`, `set_receive_any_audio(true)`, or `set_received_users(...)`.
    Callback<received_opus> on_opus_received{io_ctx_};
    /// Note: incoming audio is disabled by default (`MediaSinkWants` `{"any": 0}`). Enable via `join_options::receive_audio`, `set_receive_any_audio(true)`, or `set_received_users(...)`.
    Callback<received_audio> on_audio_received{io_ctx_};
private:
    using ws_client_t = ws::websocket_client;

    std::shared_ptr<ws_client_t> ws_shared_;
    ws_client_t& ws_;

    struct udp_on_error_cb { std::weak_ptr<connection> s; void operator()(const boost::system::error_code& ec) const; };

    struct udp_on_idle_cb { std::weak_ptr<connection> s; void operator()(audio_stop_reason reason) const; };

    struct udp_dave_encrypt { std::weak_ptr<connection> s; encrypt_result operator()(const discusy::voice::store<discusy::voice::MAX_OPUS_FRAME_BYTES>& audio, discusy::voice::store<MAX_DAVE_BUFFER_SIZE>& output) const; };

    void handle_udp_packet(std::uint32_t ssrc, std::uint16_t sequence, std::uint32_t timestamp, std::span<const std::uint8_t> payload);

    void handle_udp_packet_impl(std::uint32_t ssrc, std::uint16_t sequence, std::uint32_t timestamp, std::span<const std::uint8_t> payload);

    struct udp_on_receive_cb { std::weak_ptr<connection> s; void operator()(std::uint32_t ssrc, std::uint16_t sequence, std::uint32_t timestamp, std::span<const std::uint8_t> payload) const; };

    using udp_client_t = udp::client<udp_on_error_cb, udp_dave_encrypt, udp_on_idle_cb, udp_on_receive_cb>;

    std::shared_ptr<udp_client_t> udp_client_shared_;
    std::atomic<udp_client_t*> udp_client_{nullptr};

    [[nodiscard]] udp_client_t* udp_client() const noexcept {
        return udp_client_.load(std::memory_order_acquire);
    }

    escalation_cb_t on_escalation_;

    std::int64_t seq_{-1};
    std::unordered_map<discord::dave::TransitionId, discord::dave::ProtocolVersion> pending_transitions_{};

    dave dave_;
    std::mutex encryptor_mutex_;
    std::uint32_t encryptor_ssrc_{0};

    mutable std::mutex receive_mutex_;
    bool receive_any_{false};
    bool sink_wants_dirty_{false};
    std::unordered_map<snowflake, bool> user_wants_;
    std::unordered_map<std::uint32_t, std::uint8_t> raw_ssrc_wants_;
    std::unordered_map<std::uint32_t, snowflake> ssrc_to_user_;
    std::unordered_map<snowflake, std::uint32_t> user_to_ssrc_;
    std::unordered_map<snowflake, std::shared_ptr<discord::dave::IDecryptor>> user_decryptors_;

    std::mutex decoder_mutex_;
    std::unordered_map<std::uint32_t, opus_decoder> opus_decoders_;

    std::mutex encoder_mutex_;
    opus_encoder encoder_{};
    std::atomic<std::uint32_t> target_bitrate_{DEFAULT_OPUS_BITRATE};
    std::uint32_t applied_bitrate_{DEFAULT_OPUS_BITRATE};

    std::atomic<std::uint32_t> underrun_grace_frames_{DEFAULT_UNDERRUN_GRACE_FRAMES};
    std::atomic<std::uint32_t> max_catchup_frames_{DEFAULT_MAX_CATCHUP_FRAMES};
    std::atomic<std::uint32_t> not_ready_hold_frames_{DEFAULT_NOT_READY_HOLD_FRAMES};
    std::atomic<std::uint64_t> stat_dave_decrypt_drops_{0};
    std::atomic_flag started_ = ATOMIC_FLAG_INIT;

    void check_and_apply_bitrate() noexcept;
    
    std::uint32_t ssrc_{0};

    ctx::io_context::strand_timer_t hb_timer_;
    std::uint64_t hb_timer_gen_{0};

    ctx::io_context::strand_timer_t reconnect_timeout_timer_;
    std::uint64_t reconnect_timer_gen_{0};

    ctx::io_context::strand_timer_t refresh_watchdog_;
    std::uint64_t refresh_watchdog_gen_{0};

    std::uint64_t session_generation_{0};

    std::chrono::milliseconds reconnect_backoff_time_{0};

    discord::dave::ProtocolVersion dave_protocol_version_{};
    discord::dave::ProtocolVersion latest_prepared_dave_version_{};
    std::atomic<discord::dave::ProtocolVersion> dave_protocol_version_atomic_{0};

    std::int64_t last_hb_nonce_{-1};

    std::atomic_flag initialized_ = ATOMIC_FLAG_INIT;

    std::atomic_bool closed_{false};
    std::atomic_bool ready_{false};
    bool ready_signalled_{false};

    const snowflake server_id_;
    std::atomic<decltype(snowflake::value)> channel_id_;
    const snowflake user_id_;
    const std::string user_id_str_;
    std::string session_id_;
    std::string token_;
    std::string endpoint_;

    bool destructed_{false};
    bool reconnecting_{false};
    bool recieved_heartbeat_ack_{false};

    bool session_live_{false};
    bool session_description_received_{false};
    bool ready_received_{false};
    bool awaiting_connect_ready_{false};
    bool reconnect_scheduled_{false};
    bool escalated_{false};
    bool channel_moved_pending_refresh_{false};

    speaking_mode speaking_mode_{speaking_mode::Microphone};

    static constexpr int MAX_LOCAL_REBUILDS = 2;
    int local_rebuild_attempts_{0};

    std::uint32_t reconnect_attempts_{0};

    static constexpr std::uint32_t MAX_RECONNECT_ATTEMPTS = 10;

    [[nodiscard]] static bool is_aes256_gcm_supported() noexcept;

    glz::context shared_json_ctx_{};
};

}