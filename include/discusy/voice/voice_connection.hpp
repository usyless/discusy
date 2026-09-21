#pragma once

#include <algorithm>
#include <atomic>
#include <concepts>
#include <mutex>
#include <optional>
#include <ranges>
#include <array>
#include <thread>
#include <vector>
#include <set>

#include <sodium.h>
#include <opus.h>

#include "../types.hpp"
#include "../websocket_client.hpp"
#include "../opcode.hpp"
#include "../state.hpp"
#include "../random.hpp"
#include "../udp_client.hpp"
#include "dave.hpp"
#include "internal.hpp"

namespace discusy::voice {

namespace detail {
    template <typename T>
    struct is_span_helper : std::false_type {};

    template <typename ElementType, std::size_t Extent>
    struct is_span_helper<std::span<ElementType, Extent>> : std::true_type {};

    template <typename T>
    inline constexpr bool is_span_v = is_span_helper<std::remove_cvref_t<T>>::value;
}

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
    Microphone = 1ULL << 0, // Normal transmission of voice audio
    Soundshare = 1ULL << 1, // Transmission of context audio for video, no speaking indicator
    Priority = 1ULL << 2, // Priority speaker, lowering audio of other speakers
};

struct speaking_ {
    speaking_mode speaking{};
    std::uint8_t delay{0};
    std::uint32_t ssrc{};
};

using speaking_payload = send_payload_base<VoiceOpcode::Speaking, speaking_>;

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

template <typename escalationCB>
requires ( std::invocable<std::decay_t<escalationCB>, escalation_action> )
// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
class connection : public std::enable_shared_from_this<connection<escalationCB>> {
public:
    template <typename F>
    requires ( std::invocable<F&, escalation_action> )
    connection(ctx::io_context& ctx, state& state, snowflake server_id, snowflake channel_id, snowflake user_id, std::string session_id, std::string token, std::string endpoint, F&& escalation_cb) : 
        io_ctx_{ctx},
        strand_{io_ctx_.make_strand()},
        ws_shared_{std::make_shared<ws_client_t>(strand_, state.ssl_ctx)},
        ws_{*ws_shared_},
        on_escalation_{std::forward<F>(escalation_cb)},
        hb_timer_{strand_},
        reconnect_timeout_timer_{strand_},
        refresh_watchdog_{strand_},
        recognised_users_{user_id.str()}, // initialise with bot itself as user
        server_id_{server_id},
        channel_id_{channel_id.value},
        user_id_{user_id},
        user_id_str_{user_id_.str()},
        session_id_{std::move(session_id)},
        token_{std::move(token)},
        endpoint_{std::move(endpoint)}
    {
        setup_gateway_callbacks();
        ws_.pause();

        int opus_err = OPUS_OK;
        opus_encoder_ = opus_encoder_create(DISCORD_SAMPLING_RATE, DISCORD_CHANNELS, OPUS_APPLICATION_AUDIO, &opus_err);

        if (opus_err != OPUS_OK) {
            #ifdef DISCUSY_LOGGING
            log("Failed to create opus encoder: {}", opus_strerror(opus_err));
            #endif
            throw std::runtime_error("Failed to create opus encoder!");
        }

        opus_err = opus_encoder_ctl(opus_encoder_, OPUS_SET_BITRATE(DEFAULT_OPUS_BITRATE));
        if (opus_err != OPUS_OK) {
            #ifdef DISCUSY_LOGGING
            log("Failed to set opus bitrate: {}", opus_strerror(opus_err));
            #endif
        }
    }

    void start() {
        udp_client_shared_ = std::make_shared<udp_client_t>(
            io_ctx_,
            udp_on_error_cb{this->weak_from_this()},
            udp_dave_encrypt{this->weak_from_this()},
            udp_on_idle_cb{this->weak_from_this()}
        );
        udp_client_ = udp_client_shared_.get();

        boost::asio::dispatch(strand_, [self = this->shared_from_this()]() {
            if (self->destructed_) return;
            self->reconnect(); // gives us the logic in case of failure to open
        });
    }

    // must be called from its own strand, which is the websockets strand
    ~connection() {
        destruct();
    }

    // TODO: unsure if valid
    void move_channel(snowflake new_channel_id) {
        if (closed_.load(std::memory_order_acquire)) return;

        boost::asio::dispatch(strand_, [self = this->shared_from_this(), new_channel_id]() {
            auto& c = *self;
            if (c.destructed_) return;

            const snowflake old_channel_id{c.channel_id_.load()};

            if (new_channel_id && (new_channel_id != old_channel_id)) {
                c.channel_id_.store(new_channel_id.value);

                c.recognised_users_.clear();
                c.recognised_users_.emplace(c.user_id_str_);
                c.drop_dave_keys();

                #ifdef DISCUSY_LOGGING
                c.log("Moved to channel {}, recognised users and DAVE keys reset", new_channel_id.value);
                #endif

                c.on_channel_move.fire(channel_moved{
                    .old_channel_id = old_channel_id,
                    .new_channel_id = new_channel_id,
                });
            }
        });
    }

    void refresh(std::string session_id, std::string token, std::string endpoint, snowflake channel_id) {
        if (closed_.load(std::memory_order_acquire)) return;

        boost::asio::dispatch(strand_, [self = this->shared_from_this(), session_id = std::move(session_id), token = std::move(token), endpoint = std::move(endpoint), channel_id]() mutable {
            auto& c = *self;
            if (c.destructed_) return;

            const snowflake old_channel_id{c.channel_id_.load()};

            if (channel_id && (channel_id != old_channel_id)) {
                c.channel_id_.store(channel_id.value);

                c.recognised_users_.clear();
                c.recognised_users_.emplace(c.user_id_str_);
                c.drop_dave_keys();

                #ifdef DISCUSY_LOGGING
                c.log("Moved to channel {}, recognised users and DAVE keys reset", channel_id.value);
                #endif

                c.on_channel_move.fire(channel_moved{ // TODO: check if this needs to be anywhere else
                    .old_channel_id = old_channel_id,
                    .new_channel_id = channel_id,
                });
            }

            const bool same_server = c.session_live_ && (c.endpoint_ == endpoint);

            c.session_id_ = std::move(session_id);
            c.token_ = std::move(token);

            if (same_server) {
                #ifdef DISCUSY_LOGGING
                c.log("Same voice server, staying connected");
                #endif

                c.refresh_watchdog_.cancel();
                c.escalated_ = false;
                return;
            }

            #ifdef DISCUSY_LOGGING
            c.log("Voice server changed, rebuilding onto: {}", endpoint);
            #endif

            c.endpoint_ = std::move(endpoint);

            c.begin_fresh_session();
        });
    }

    [[nodiscard]] bool is_active() const noexcept {
        return !closed_.load(std::memory_order_acquire);
    }

    [[nodiscard]] bool is_ready_to_send() const noexcept {
        return ready_.load(std::memory_order_acquire) && !closed_.load(std::memory_order_acquire);
    }

    void set_speaking_mode(speaking_mode mode) {
        if (closed_.load(std::memory_order_acquire)) return;

        boost::asio::dispatch(strand_, [self = this->shared_from_this(), mode]() {
            if (self->destructed_) return;
            self->speaking_mode_ = mode;
            self->send_speaking();
        });
    }

    void stop_audio() {
        if (closed_.load(std::memory_order_acquire) || (udp_client_ == nullptr)) return;
        udp_client_->stop_audio();
    }

    void pause_audio() {
        if (closed_.load(std::memory_order_acquire) || (udp_client_ == nullptr)) return;
        udp_client_->pause_audio();
    }

    void rebuild_in_place() {
        if (closed_.load(std::memory_order_acquire)) return;

        boost::asio::dispatch(strand_, [self = this->shared_from_this()]() {
            if (self->destructed_) return;

            #ifdef DISCUSY_LOGGING
            self->log("Rebuilding voice session in place");
            #endif

            self->escalated_ = false;
            self->begin_fresh_session(false);
        });
    }

    void resume_audio() {
        if (closed_.load(std::memory_order_acquire) || (udp_client_ == nullptr)) return;
        udp_client_->resume_audio();
    }

    [[nodiscard]] double get_secs_remaining() const noexcept {
        if (closed_.load(std::memory_order_acquire) || (udp_client_ == nullptr)) return 0.0;
        return udp_client_->get_secs_remaining();
    }

    [[nodiscard]] std::chrono::milliseconds get_ms_remaining() const noexcept {
        if (closed_.load(std::memory_order_acquire) || (udp_client_ == nullptr)) return std::chrono::milliseconds{0};
        return udp_client_->get_ms_remaining();
    }

    [[nodiscard]] std::uint64_t get_frames_remaining() const noexcept {
        if (closed_.load(std::memory_order_acquire) || (udp_client_ == nullptr)) return 0;
        return udp_client_->get_frames_remaining();
    }

    [[nodiscard]] bool has_audio_queued() const noexcept {
        return get_frames_remaining() != 0;
    }

    void set_underrun_grace_frames(const std::uint32_t frames) {
        if (udp_client_) udp_client_->set_underrun_grace_frames(frames);
    }

    void set_max_catchup_frames(const std::uint32_t frames) {
        if (udp_client_) udp_client_->set_max_catchup_frames(frames);
    }

    void set_not_ready_hold_frames(const std::uint32_t frames) {
        if (udp_client_) udp_client_->set_not_ready_hold_frames(frames);
    }

    [[nodiscard]] audio_stats get_audio_stats() const noexcept {
        if (udp_client_ == nullptr) return audio_stats{};
        return udp_client_->get_stats();
    }

    void reset_audio_stats() noexcept {
        if (udp_client_) udp_client_->reset_stats();
    }

    bool set_inband_fec(const bool enabled) {
        return opus_ctl("inband fec", OPUS_SET_INBAND_FEC(enabled ? 1 : 0));
    }

    bool set_expected_packet_loss(const int percentage) {
        return opus_ctl("packet loss percentage", OPUS_SET_PACKET_LOSS_PERC(std::clamp(percentage, 0, 100)));
    }

    bool set_opus_complexity(const int complexity) {
        return opus_ctl("complexity", OPUS_SET_COMPLEXITY(std::clamp(complexity, 0, 10)));
    }

    // OPUS_SIGNAL_MUSIC / OPUS_SIGNAL_VOICE / OPUS_AUTO
    bool set_opus_signal(const int signal) {
        return opus_ctl("signal", OPUS_SET_SIGNAL(signal));
    }

    [[nodiscard]] snowflake get_guild_id() const noexcept { return server_id_; }
    [[nodiscard]] snowflake get_server_id() const noexcept { return server_id_; }

    [[nodiscard]] snowflake get_channel_id() const noexcept {
        return snowflake{channel_id_.load(std::memory_order_acquire)};
    }

    template <typename... Request>
    bool opus_ctl([[maybe_unused]] const std::string_view what, Request&&... request) {
        if (closed_.load(std::memory_order_acquire)) return false;

        int opus_err = 0;
        {
        std::scoped_lock lock{opus_mutex_};
        if (!opus_encoder_) return false;
        opus_err = opus_encoder_ctl(opus_encoder_, std::forward<Request>(request)...);
        }

        if (opus_err != OPUS_OK) {
            #ifdef DISCUSY_LOGGING
            log("Failed to set opus {}: {}", what, opus_strerror(opus_err));
            #endif
            return false;
        }
        return true;
    }

    bool set_bitrate(const std::uint32_t bitrate) {
        if (closed_.load(std::memory_order_acquire)) return false;

        int opus_err = 0;
        {
        std::scoped_lock lock{opus_mutex_};
        if (opus_encoder_ == nullptr) return false;
        opus_err = opus_encoder_ctl(opus_encoder_, OPUS_SET_BITRATE(bitrate));
        }

        if (opus_err != OPUS_OK) {
            #ifdef DISCUSY_LOGGING
            log("Failed to set opus bitrate: {}", opus_strerror(opus_err));
            #endif
            return false;
        }
        return true;
    }

    // make sure the frame isnt bigger than voice::MAX_OPUS_FRAME_BYTES bytes
    void send_opus_frame(const std::span<const std::uint8_t> opus_data) {
        if (closed_.load(std::memory_order_acquire) || (udp_client_ == nullptr)) return;
        udp_client_->send_audio_packet(opus_data);
    }

    void send_opus_frames(std::vector<discusy::voice::store<discusy::voice::MAX_OPUS_FRAME_BYTES>>&& opus_frames) {
        if (closed_.load(std::memory_order_acquire) || (udp_client_ == nullptr)) return;
        udp_client_->send_audio_packets(std::move(opus_frames));
    }

    void send_opus_frames(std::span<const discusy::voice::store<discusy::voice::MAX_OPUS_FRAME_BYTES>> opus_frames) {
        if (closed_.load(std::memory_order_acquire) || (udp_client_ == nullptr)) return;
        udp_client_->send_audio_packets(opus_frames);
    }

    // if you send data that doesnt exactly align to the 20ms 48000hz stereo frames silence will be appended
    void send_pcm(
        std::span<const std::int16_t> pcm_data,
        const std::chrono::milliseconds batch_duration = std::chrono::seconds{10}
    ) {
        if (closed_.load(std::memory_order_acquire) || (udp_client_ == nullptr)) return;

        try {
            std::unique_lock lock{opus_mutex_};
            if (opus_encoder_ == nullptr) return;

            static constexpr int SAMPLES_PER_FRAME = (DISCORD_SAMPLING_RATE / 1000) * DISCORD_FRAME_SIZE_MS;
            static constexpr int TOTAL_FRAME_SAMPLES = SAMPLES_PER_FRAME * DISCORD_CHANNELS;

            const auto total = pcm_data.size();
            if (total == 0) return;

            const size_t batch_frames_limit = std::max<size_t>(
                1, 
                static_cast<size_t>(batch_duration.count() / DISCORD_FRAME_SIZE_MS)
            );

            std::vector<discusy::voice::store<discusy::voice::MAX_OPUS_FRAME_BYTES>> batch;
            batch.reserve(batch_frames_limit);

            std::array<std::int16_t, TOTAL_FRAME_SAMPLES> padded_pcm{};

            for (size_t pos = 0; pos < total; pos += TOTAL_FRAME_SAMPLES) {
                if (closed_.load(std::memory_order_acquire) || (opus_encoder_ == nullptr)) return;

                const size_t remaining_samples = total - pos;
                const std::int16_t* current_pcm_ptr = pcm_data.data() + pos;

                if (remaining_samples < TOTAL_FRAME_SAMPLES) { // end of data, pad with silence
                    std::ranges::fill(padded_pcm, 0);
                    // TODO: check correct
                    std::copy_n(current_pcm_ptr, remaining_samples, padded_pcm.begin());
                    current_pcm_ptr = padded_pcm.data();
                }

                discusy::voice::store<discusy::voice::MAX_OPUS_FRAME_BYTES>& opus_out = batch.emplace_back();
                const int bytes_encoded = opus_encode(
                    opus_encoder_, 
                    current_pcm_ptr, 
                    SAMPLES_PER_FRAME, 
                    opus_out.raw_buf.data(), 
                    discusy::voice::MAX_OPUS_FRAME_BYTES
                );

                if (bytes_encoded > 0) {
                    opus_out.set_size(static_cast<size_t>(bytes_encoded));
                } else {
                    batch.pop_back();
                }

                if (batch.size() >= batch_frames_limit) {
                    lock.unlock();
                    send_opus_frames(std::move(batch));
                    batch = {};
                    batch.reserve(batch_frames_limit);

                    std::this_thread::yield();

                    lock.lock();
                    if (closed_.load(std::memory_order_acquire) || (opus_encoder_ == nullptr)) return;
                }
            }

            if (!batch.empty()) {
                lock.unlock();
                send_opus_frames(std::move(batch));
            }
        }
        #ifdef DISCUSY_LOGGING
        catch (const std::exception& e) {
            log("send_pcm exception: {}", e.what());
        }
        #endif
        catch (...) {
            #ifdef DISCUSY_LOGGING
            log("send_pcm unknown exception");
            #endif
        }
    }

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
        const auto byte_count = std::ranges::size(raw_bytes);
        const auto sample_count = byte_count / sizeof(std::int16_t);
        if (sample_count == 0) return;

        const auto* raw_ptr = reinterpret_cast<const char*>(std::ranges::data(raw_bytes));

        if (reinterpret_cast<std::uintptr_t>(raw_ptr) % alignof(std::int16_t) == 0) {
            send_pcm(
                std::span<const std::int16_t>{reinterpret_cast<const std::int16_t*>(raw_ptr), sample_count},
                batch_duration
            );
        } else {
            #ifdef DISCUSY_LOGGING
            log::Logger{}("Misaligned data passed into send_pcm! Copying into an aligned vector - this is inefficient");
            #endif
            std::vector<std::int16_t> aligned(sample_count);
            std::memcpy(aligned.data(), raw_ptr, sample_count * sizeof(std::int16_t));
            send_pcm(std::span<const std::int16_t>{aligned.data(), aligned.size()}, batch_duration);
        }
    }

    // Needs to be an ownable container, not a view or span over data
    template <typename Container>
    requires (
        !std::is_lvalue_reference_v<Container> &&
        !std::ranges::view<std::remove_cvref_t<Container>> &&
        !std::ranges::borrowed_range<std::remove_cvref_t<Container>> &&
        std::ranges::input_range<std::remove_cvref_t<Container>> &&
        (std::convertible_to<std::ranges::range_value_t<std::remove_cvref_t<Container>>, std::int16_t> ||
         sizeof(std::ranges::range_value_t<std::remove_cvref_t<Container>>) == 1)
    )
    void send_pcm_async(
        Container&& pcm_data,
        const std::chrono::milliseconds batch_duration = std::chrono::seconds{10}
    ) {
        if (closed_.load(std::memory_order_acquire) || !udp_client_) return;

        using RawContainer = std::remove_cvref_t<Container>;

        auto launch_task = [self = this->shared_from_this(), batch_duration]<typename ContiguousContainer>(ContiguousContainer&& owned_buffer) {
            struct task_state {
                std::shared_ptr<connection> self;
                std::remove_cvref_t<ContiguousContainer> data;
                std::size_t offset{0};
                std::chrono::milliseconds batch_duration{};

                void operator()() {
                    if (self->closed_.load(std::memory_order_acquire) || !self->udp_client_) return;

                    try {
                        using ElemType = std::remove_cvref_t<std::ranges::range_value_t<std::remove_cvref_t<ContiguousContainer>>>;
                        static constexpr size_t SAMPLES_PER_FRAME = (DISCORD_SAMPLING_RATE / 1000) * DISCORD_FRAME_SIZE_MS;
                        static constexpr size_t TOTAL_FRAME_SAMPLES = SAMPLES_PER_FRAME * DISCORD_CHANNELS;
                        static constexpr size_t ELEMENTS_PER_FRAME = (sizeof(ElemType) == 1) 
                            ? (TOTAL_FRAME_SAMPLES * sizeof(std::int16_t)) 
                            : TOTAL_FRAME_SAMPLES;

                        const auto total_elements = std::ranges::size(data);
                        const size_t batch_frames_limit = std::max<size_t>(
                            1, 
                            static_cast<size_t>(batch_duration.count() / DISCORD_FRAME_SIZE_MS)
                        );
                        const size_t batch_elements_limit = batch_frames_limit * ELEMENTS_PER_FRAME;

                        const size_t remaining = total_elements - offset;
                        const size_t chunk_elements = std::min(remaining, batch_elements_limit);

                        if (chunk_elements == 0) return;

                        if constexpr (sizeof(ElemType) == 1) {
                            const auto* raw_ptr = reinterpret_cast<const char*>(std::ranges::data(data)) + offset;
                            self->send_pcm(std::span<const char>{raw_ptr, chunk_elements}, batch_duration);
                        } else {
                            const auto* raw_ptr = std::ranges::data(data) + offset;
                            self->send_pcm(std::span<const std::int16_t>{raw_ptr, chunk_elements}, batch_duration);
                        }

                        offset += chunk_elements;

                        if (offset < total_elements && !self->closed_.load(std::memory_order_acquire)) {
                            auto ex = self->io_ctx_.executor_;
                            boost::asio::post(ex, std::move(*this));
                        }
                    }
                    #ifdef DISCUSY_LOGGING
                    catch (const std::exception& e) {
                        self->log("send_pcm_async exception: {}", e.what());
                    }
                    #endif
                    catch (...) {
                        #ifdef DISCUSY_LOGGING
                        self->log("send_pcm_async unknown exception");
                        #endif
                    }
                }
            };

            auto ex = self->io_ctx_.executor_;
            boost::asio::post(ex, task_state{
                .self{std::move(self)},
                .data{std::forward<ContiguousContainer>(owned_buffer)},
                .offset = 0,
                .batch_duration = batch_duration,
            });
        };

        if constexpr (std::ranges::contiguous_range<RawContainer> && 
                      std::is_same_v<std::remove_cvref_t<std::ranges::range_value_t<RawContainer>>, std::int16_t>) {
            launch_task(std::forward<Container>(pcm_data));
        } else if constexpr (std::ranges::contiguous_range<RawContainer> && 
                             sizeof(std::ranges::range_value_t<RawContainer>) == 1) {
            const auto* raw_ptr = reinterpret_cast<const char*>(std::ranges::data(pcm_data));
            if (reinterpret_cast<std::uintptr_t>(raw_ptr) % alignof(std::int16_t) == 0) {
                launch_task(std::forward<Container>(pcm_data));
            } else {
                #ifdef DISCUSY_LOGGING
                log::Logger{}("Misaligned data passed into send_pcm_async! Copying into an aligned vector upfront - this is inefficient");
                #endif
                const auto byte_count = std::ranges::size(pcm_data);
                const auto sample_count = byte_count / sizeof(std::int16_t);
                std::vector<std::int16_t> aligned(sample_count);
                std::memcpy(aligned.data(), raw_ptr, sample_count * sizeof(std::int16_t));
                launch_task(std::move(aligned));
            }
        } else if constexpr (sizeof(std::ranges::range_value_t<RawContainer>) == 1) {
            std::string bytes_vec;
            if constexpr (std::ranges::sized_range<RawContainer>) {
                bytes_vec.reserve(std::ranges::size(pcm_data));
            }
            for (auto&& b : pcm_data) {
                bytes_vec.push_back(static_cast<char>(b));
            }
            launch_task(std::move(bytes_vec));
        } else {
            std::vector<std::int16_t> vec;
            if constexpr (std::ranges::sized_range<RawContainer>) {
                vec.reserve(std::ranges::size(pcm_data));
            }
            for (auto&& sample : pcm_data) {
                vec.push_back(static_cast<std::int16_t>(sample));
            }
            launch_task(std::move(vec));
        }
    }

private:
    // must be called from its own strand, which is the websockets strand
    void destruct() {
        if (destructed_) return;
        destructed_ = true;

        closed_.store(true, std::memory_order_release);

        if (udp_client_) udp_client_->close();

        stop_heartbeat();
        reconnect_timeout_timer_.cancel();
        refresh_watchdog_.cancel();

        ws_.stop();
        ws_.pause();

        ws_.on_connect_ready(connect_cb{nullptr});
        ws_.on_close(close_cb{nullptr});
        ws_.on_message(message_cb{nullptr});
        ws_.on_open(open_cb{nullptr});

        ws_.close();
        ws_.clear();

        {
        std::scoped_lock lock{opus_mutex_};
        if (opus_encoder_ != nullptr) {
            opus_encoder_destroy(opus_encoder_);
            opus_encoder_ = nullptr;
        }
        }

        on_closed.fire(voice_closed{.guild_id = server_id_});
    }

public:
    template <typename botT>
    friend class client;
private:
    #ifdef DISCUSY_LOGGING
    template <typename ...Args>
    void log(const std::format_string<Args...>& s, Args&&... args) {
        log::Logger{}("{}{}", ulp::str::concat_strings("[discusy voice conn ", ulp::str::to_string_view(channel_id_.load(std::memory_order_relaxed)), "] "), std::format(s, std::forward<Args>(args)...));
    }
    #endif

    void stop() {
        reconnecting_ = true;
        session_live_ = false;
        ready_.store(false, std::memory_order_release);
        ready_signalled_ = false;
        stop_heartbeat();
        ws_.pause();
        reconnect_timeout_timer_.cancel();
    }

    void escalate(const escalation_action action) {
        if (destructed_ || escalated_) return;

        escalated_ = true;
        stop();

        if (action == escalation_action::requires_gateway_reconnect) arm_refresh_watchdog();

        std::invoke(on_escalation_, action);
    }

    void await_gateway_instruction() {
        if (destructed_ || escalated_) return;

        escalated_ = true;
        stop();
        arm_refresh_watchdog();
    }

    void arm_refresh_watchdog() {
        refresh_watchdog_.expires_after(std::chrono::seconds{30});
        refresh_watchdog_.async_wait([weak = this->weak_from_this()](asio::ec_t ec) {
            if (ec) return;

            auto self = weak.lock();
            if (!self || self->destructed_) return;

            #ifdef DISCUSY_LOGGING
            self->log("No refreshed voice session arrived, rebuilding on the endpoint we have");
            #endif

            self->escalated_ = false;
            self->begin_fresh_session(false);
        });
    }

    void begin_fresh_session(const bool reset_backoff = true) {
        stop();
        refresh_watchdog_.cancel();

        escalated_ = false;
        awaiting_connect_ready_ = false;
        reconnect_scheduled_ = false;

        if (reset_backoff) {
            reconnect_attempts_ = 0;
            reconnect_backoff_time_ = std::chrono::milliseconds{0};
        }

        initialized_.clear();
        seq_ = -1;
        ssrc_ = 0;
        last_hb_nonce_ = -1;

        if (udp_client_) udp_client_->reset_session();

        reconnect();
    }

    void maybe_signal_ready() {
        if (destructed_ || ready_signalled_ || !session_live_) return;

        ready_signalled_ = true;
        ready_.store(true, std::memory_order_release);
        local_rebuild_attempts_ = 0;

        #ifdef DISCUSY_LOGGING
        log("Ready to transmit audio");
        #endif

        on_ready.fire(voice_ready{.guild_id = server_id_});
    }

    void handle_ws_open() {
        #ifdef DISCUSY_LOGGING
        log("Websocket open");
        #endif
        reconnecting_ = false;
        send_identify();
    }

    void handle_ws_connect_ready() {
        awaiting_connect_ready_ = false;

        if (destructed_ || escalated_) return;

        reconnecting_ = true;
        session_live_ = false;
        ready_.store(false, std::memory_order_release);
        ready_signalled_ = false;
        stop_heartbeat();
        ws_.pause();

        #ifdef DISCUSY_LOGGING
        log("Re/connecting to: {}", endpoint_);
        #endif

        if (!ws_.connect(endpoint_)) {
            #ifdef DISCUSY_LOGGING
            log("Failed to start a connection to {}, backing off", endpoint_);
            #endif
            reconnect();
        }
    }

    void handle_ws_close(const std::uint16_t code) {
        #ifdef DISCUSY_LOGGING
        log("Websocket close: {}", code);
        #endif

        session_live_ = false;
        stop_heartbeat();

        if (destructed_ || escalated_) return;

        if (awaiting_connect_ready_) {
            #ifdef DISCUSY_LOGGING
            log("Close was ours, waiting for connect ready");
            #endif
            return;
        }

        switch (static_cast<VoiceCloseCode>(code)) {
            // Recoverable on the same voice server: reconnect and resume.
            case VoiceCloseCode::SessionTimeout:
            case VoiceCloseCode::VoiceServerCrashed: {
                #ifdef DISCUSY_LOGGING
                log("Attempting to reconnect!");
                #endif
                break;
            }

            case VoiceCloseCode::SessionNoLongerValid:
            case VoiceCloseCode::AlreadyAuthenticated: {
                if (local_rebuild_attempts_ < MAX_LOCAL_REBUILDS) {
                    ++local_rebuild_attempts_;

                    #ifdef DISCUSY_LOGGING
                    log("Session rejected ({}), re-identifying on the same endpoint (attempt {})",
                        +code, local_rebuild_attempts_);
                    #endif

                    begin_fresh_session(false);
                    return;
                }

                #ifdef DISCUSY_LOGGING
                log("Voice session no longer valid ({}), asking for a new one", +code);
                #endif
                return escalate(escalation_action::requires_gateway_reconnect);
            }

            case VoiceCloseCode::Disconnected:
            case VoiceCloseCode::DisconnectedCallTerminated: {
                #ifdef DISCUSY_LOGGING
                log("Disconnected by discord ({}), waiting for the gateway to say why", +code);
                #endif
                return await_gateway_instruction();
            }

            case VoiceCloseCode::AuthenticationFailed:
            case VoiceCloseCode::FailedToDecodePayload:
            case VoiceCloseCode::NotAuthenticated:
            case VoiceCloseCode::ServerNotFound:
            case VoiceCloseCode::UnknownProtocol:
            case VoiceCloseCode::UnknownEncryptionMode:
            case VoiceCloseCode::E2eeDaveProtocolRequired:
            case VoiceCloseCode::BadRequest:
            case VoiceCloseCode::DisconnectedRateLimited: {
                #ifdef DISCUSY_LOGGING
                log("Fatal close code {}, shutting down voice.", +code);
                #endif
                return escalate(escalation_action::fatal);
            }

            default: break;
        }

        stop();

        reconnect();
    }

    void handle_udp_failure(const boost::system::error_code& ec) {
        if (destructed_ || escalated_) return;

        #ifdef DISCUSY_LOGGING
        log("UDP transport failed ({}), rebuilding the voice session", ec.message());
        #else
        (void)ec;
        #endif

        begin_fresh_session(false);
    }

    void handle_ws_message(const std::string_view msg, bool binary) {
        try {
            if (binary) {
                handle_binary_payload(msg);
            } else {
                handle_gateway_payload(msg);
            }
        }
        #ifdef DISCUSY_LOGGING
        catch (const std::exception& e) {
            log("on_message exception: {}", e.what());
        }
        #endif
        catch (...) {
            #ifdef DISCUSY_LOGGING
            log("on_message unknown exception");
            #endif
        }
    }

    void setup_gateway_callbacks() {
        ws_.on_open(open_cb{this});
        ws_.on_connect_ready(connect_cb{this});
        ws_.on_close(close_cb{this});
        ws_.on_message(message_cb{this});
    }

    void reconnect_internal() {
        reconnect_scheduled_ = false;

        if (destructed_ || escalated_) return;

        reconnect_timeout_timer_.cancel();
        reconnect_timeout_timer_.expires_after(std::chrono::seconds{25});
        reconnect_timeout_timer_.async_wait([weak = this->weak_from_this()](asio::ec_t ec) {
            if (ec) return;
            auto self = weak.lock();
            if (!self || self->destructed_) return;

            #ifdef DISCUSY_LOGGING
            self->log("Reconnect timeout expired, attempting to reconnect...");
            #endif

            self->stop();
            self->reconnect();
        });

        awaiting_connect_ready_ = true;
        ws_.close();
    }

    void reconnect() {
        if (destructed_ || escalated_) return;

        if (reconnect_scheduled_) return;

        reconnecting_ = true;
        reconnect_timeout_timer_.cancel();

        if (++reconnect_attempts_ > MAX_RECONNECT_ATTEMPTS) {
            #ifdef DISCUSY_LOGGING
            log("Giving up after {} failed voice connection attempts", reconnect_attempts_ - 1);
            #endif
            return escalate(escalation_action::fatal);
        }

        const auto delay = reconnect_backoff_time_;

        if (reconnect_backoff_time_.count() == 0) {
            reconnect_backoff_time_ = std::chrono::milliseconds{1000};
        } else {
            reconnect_backoff_time_ *= 2;
            reconnect_backoff_time_ = std::chrono::milliseconds{std::min<decltype(reconnect_backoff_time_.count())>(20000, reconnect_backoff_time_.count())};
        }

        reconnect_scheduled_ = true;

        if (delay.count() == 0) {
            #ifdef DISCUSY_LOGGING
            log("Re/connecting immediately");
            #endif
            reconnect_internal();
        } else {
            #ifdef DISCUSY_LOGGING
            log("Re/connecting after: {}", delay);
            #endif
            reconnect_timeout_timer_.expires_after(delay);
            reconnect_timeout_timer_.async_wait([weak = this->weak_from_this()](asio::ec_t ec) {
                if (ec) return;
                auto self = weak.lock();
                if (!self || self->destructed_) return;

                self->reconnect_internal();
            });
        }
    }

    void handle_binary_payload(std::string_view data) {
        if (data.size() < 3) return;

        std::uint16_t seq{};
        std::memcpy(&seq, data.data(), sizeof(seq));
        if constexpr (std::endian::native == std::endian::little) {
            seq = std::byteswap(seq);
        }

        if (seq != std::uint16_t{0}) seq_ = seq;

        const auto opcode = static_cast<VoiceOpcode>(data[2]);
        std::string_view payload = data.substr(3);

        switch (opcode) {
            case VoiceOpcode::DaveMlsExternalSender: {
                dave_.cached_external_sender_.assign(payload.begin(), payload.end());
                dave_.session_->SetExternalSender(dave_.cached_external_sender_);
                break;
            }

            case VoiceOpcode::DaveMlsProposals: {
                const auto commit_welcome = dave_.session_->ProcessProposals(
                    std::vector<uint8_t>(payload.begin(), payload.end()), recognised_users_
                );
                
                if (commit_welcome) {
                    send_binary_dave_payload<VoiceOpcode::DaveMlsCommitWelcome>(*commit_welcome);
                }
                break;
            }

            case VoiceOpcode::DaveMlsAnnounceCommitTransition: {
                if (payload.size() < 2) return;
                
                std::uint16_t transition_id{};
                std::memcpy(&transition_id, payload.data(), sizeof(transition_id));
                if constexpr (std::endian::native == std::endian::little) transition_id = std::byteswap(transition_id);
                
                transition_id_ = transition_id;

                const auto result = dave_.session_->ProcessCommit(
                    std::vector<uint8_t>(payload.begin() + 2, payload.end())
                );
                
                if (std::holds_alternative<discord::dave::failed_t>(result)) {
                    send_invalid_commit_welcome();
                } else if (std::holds_alternative<discord::dave::RosterMap>(result)) {
                    send_ready_for_transition();
                }
                break;
            }

            case VoiceOpcode::DaveMlsWelcome: {
                if (payload.size() < 2) return;
                
                std::uint16_t transition_id{};
                std::memcpy(&transition_id, payload.data(), sizeof(transition_id));
                if constexpr (std::endian::native == std::endian::little) transition_id = std::byteswap(transition_id);
                
                transition_id_ = transition_id;

                const auto result = dave_.session_->ProcessWelcome(
                    std::vector<uint8_t>(payload.begin() + 2, payload.end()), recognised_users_
                );
                
                if (!result) {
                    send_invalid_commit_welcome();
                } else {
                    send_ready_for_transition();
                }
                break;
            }
            
            default: {
                #ifdef DISCUSY_LOGGING
                log("Unknown binary opcode recieved: {}", +opcode);
                #endif
                break;
            }
        }
    }

    void handle_gateway_payload(std::string_view json_payload) {
        #ifdef DISCUSY_LOGGING
        log("Handle gateway payload: {}", json_payload);
        #endif
        if (json_payload.empty()) return;

        recieve_payload_base e{};
        if (json::parse_json_view<json::glz_opts_partial_read_not_null_term>(e, json_payload, shared_json_ctx_, json_logger)) return;

        if (e.seq) seq_ = *e.seq;

        switch (e.op) {
            case VoiceOpcode::Ready: {
                recieve_payload<ready> ready{};
                if (json::parse_json_view(ready, json_payload, shared_json_ctx_, json_logger)) {
                    stop();
                    reconnect();
                    return;
                }

                ws_.resume();
                reconnect_timeout_timer_.cancel();

                if (udp_client_ == nullptr) {
                    #ifdef DISCUSY_LOGGING
                    log("READY arrived before start(), ignoring");
                    #endif
                    return;
                }

                udp_client_->reset_session();

                std::string_view mode;

                if (is_aes256_gcm_supported() && std::ranges::contains(ready.d.modes, "aead_aes256_gcm_rtpsize")) {
                    mode = "aead_aes256_gcm_rtpsize";
                } else if (std::ranges::contains(ready.d.modes, "aead_xchacha20_poly1305_rtpsize")) {
                    mode = "aead_xchacha20_poly1305_rtpsize";
                } else {
                    #ifdef DISCUSY_LOGGING
                    log("No supported encryption mode found! Available modes: {}", ready.d.modes);
                    #endif
                    escalate(escalation_action::fatal);
                    return;
                }

                ssrc_ = ready.d.ssrc;

                udp_client_->send_ip_discovery(std::move(ready.d.ip), ready.d.port, ssrc_, [weak = this->weak_from_this(), mode, strand = strand_](std::string&& my_ip, std::uint16_t my_port) mutable {
                    boost::asio::post(strand, [weak = std::move(weak), my_ip = std::move(my_ip), my_port, mode]() mutable {
                        auto s = weak.lock();
                        if (!s) return;

                        auto& self = *s;
                        if (self.destructed_) return;

                        if (my_ip.empty() || my_port == 0) {
                            #ifdef DISCUSY_LOGGING
                            self.log("IP discovery gave us nothing usable, rebuilding the session");
                            #endif
                            self.begin_fresh_session(false);
                            return;
                        }

                        self.reconnect_attempts_ = 0;
                        self.reconnect_backoff_time_ = std::chrono::milliseconds{0};

                        select_protocol select{
                            .data{
                                .address{std::move(my_ip)},
                                .port = my_port,
                                .mode{mode},
                            },
                        };
                        select_protocol_payload payload{select};

                        std::string out;
                        if (json::write_json(payload, out, self.shared_json_ctx_, self.json_logger)) {
                            self.escalate(escalation_action::fatal);
                            return;
                        }

                        #ifdef DISCUSY_LOGGING
                        self.log("Sent SELECT_PAYLOAD: {}", out);
                        #endif
                        self.ws_.send(std::move(out));
                    });
                });

                {
                std::scoped_lock lock{encryptor_mutex_};
                dave_.encryptor_->AssignSsrcToCodec(ssrc_, discord::dave::Codec::Opus);
                }
                break;
            };

            case VoiceOpcode::SessionDescription: {
                recieve_payload<session_description> description{};
                if ((udp_client_ == nullptr) || json::parse_json_view(description, json_payload, shared_json_ctx_, json_logger)) {
                    stop();
                    reconnect();
                    return;
                }

                dave_protocol_version_ = description.d.dave_protocol_version;

                if (dave_protocol_version_ > DAVE_PROTOCOL_VERSION) {
                    #ifdef DISCUSY_LOGGING
                    log("Dave protocol version not supported! Recieved version: {}", description.d.dave_protocol_version);
                    #endif
                    escalate(escalation_action::fatal);
                    return;
                }

                udp::encryption_mode mode{};
                if (is_aes256_gcm_supported() && (description.d.mode == "aead_aes256_gcm_rtpsize")) {
                    mode = udp::encryption_mode::aead_aes256_gcm_rtpsize;
                } else if (description.d.mode == "aead_xchacha20_poly1305_rtpsize") {
                    mode = udp::encryption_mode::aead_xchacha20_poly1305_rtpsize;
                } else {
                    #ifdef DISCUSY_LOGGING
                    log("No supported encryption mode found! Recieved mode: {}", description.d.mode);
                    #endif
                    escalate(escalation_action::fatal);
                    return;
                }

                dave_init_session();

                udp_client_->set_secret_key(mode, description.d.secret_key);

                session_live_ = true;
                reconnect_attempts_ = 0;
                reconnect_backoff_time_ = std::chrono::milliseconds{0};

                send_speaking(speaking_mode::None);
                send_speaking();

                maybe_signal_ready();

                break;
            };

            case VoiceOpcode::Resumed: {
                ws_.resume();
                reconnect_timeout_timer_.cancel();
                reconnect_backoff_time_ = std::chrono::milliseconds{0};
                reconnect_attempts_ = 0;

                session_live_ = true;
                maybe_signal_ready();
                break;
            };

            case VoiceOpcode::Hello: {
                recieve_payload<hello> hello{};
                if (json::parse_json_view(hello, json_payload, shared_json_ctx_, json_logger) || hello.d.heartbeat_interval <= 0) {
                    stop();
                    reconnect();
                    return;
                }

                #ifdef DISCUSY_LOGGING
                log("HELLO interval={}",  hello.d.heartbeat_interval);
                #endif
                start_heartbeat(std::chrono::milliseconds{hello.d.heartbeat_interval});
                break;
            }

            case VoiceOpcode::HeartbeatAck: {
                recieve_payload<heartbeat_ack_bare> bare{};
                if (!json::parse_json_view(bare, json_payload, shared_json_ctx_, json_logger)) {
                    if (bare.d.t == last_hb_nonce_) recieved_heartbeat_ack_ = true;
                    break;
                }

                recieve_payload<heartbeat_ack_quoted> quoted{};
                if (!json::parse_json_view(quoted, json_payload, shared_json_ctx_, json_logger)) {
                    if (quoted.d.t == last_hb_nonce_) recieved_heartbeat_ack_ = true;
                    break;
                }

                #ifdef DISCUSY_LOGGING
                log("Could not read heartbeat ack nonce, accepting the ack anyway");
                #endif
                recieved_heartbeat_ack_ = true;
                break;
            }

            // MLS stuff
            case VoiceOpcode::ClientsConnect: {
                recieve_payload<clients_connect> connected{};
                if (json::parse_json_view(connected, json_payload, shared_json_ctx_, json_logger)) {
                    stop();
                    reconnect();
                    return;
                }

                for (auto& id : connected.d.user_ids) {
                    recognised_users_.emplace(std::move(id));
                }

                send_speaking(speaking_mode::None); // TODO: should only send here if no audio truly remains
                send_speaking();

                break;
            }

            case VoiceOpcode::ClientDisconnect: {
                recieve_payload<client_disconnect> disconnected{};
                if (json::parse_json_view(disconnected, json_payload, shared_json_ctx_, json_logger)) {
                    stop();
                    reconnect();
                    return;
                }

                recognised_users_.erase(disconnected.d.user_id);

                break;
            }

            case VoiceOpcode::DavePrepareTransition: {
                recieve_payload<dave_protocol_prepare_transition> transition{};
                if (json::parse_json_view(transition, json_payload, shared_json_ctx_, json_logger)) {
                    stop();
                    reconnect();
                    return;
                }

                transition_id_ = transition.d.transition_id;

                pending_passthrough_ = (transition.d.protocol_version == 0);

                send_ready_for_transition();

                break;
            }

            case VoiceOpcode::DaveExecuteTransition: {
                recieve_payload<dave_protocol_execute_transition> transition{};
                if (json::parse_json_view(transition, json_payload, shared_json_ctx_, json_logger)) {
                    stop();
                    reconnect();
                    return;
                }

                transition_id_ = transition.d.transition_id;

                apply_pending_passthrough();

                if (transition_id_ != 0) {
                    apply_key_ratchet();

                    send_speaking(speaking_mode::None); // TODO: should only send here if no audio truly remains
                    send_speaking();
                }
                break;
            }

            case VoiceOpcode::DavePrepareEpoch: {
                recieve_payload<dave_protocol_prepare_epoch> epoch{};
                if (json::parse_json_view(epoch, json_payload, shared_json_ctx_, json_logger)) {
                    stop();
                    reconnect();
                    return;
                }

                pending_passthrough_ = (epoch.d.protocol_version == 0);

                if (epoch.d.epoch == 1) {
                    dave_.create_session();
                    dave_init_session();
                }

                #ifdef DISCUSY_LOGGING
                if (epoch.d.epoch != 1) {
                    log("Prepare epoch with epoch {} (protocol version change on a retained group) -- unhandled", epoch.d.epoch);
                }
                #endif

                break;
            }

            default: {
                #ifdef DISCUSY_LOGGING
                log("Unknown opcode recieved: {}", +e.op);
                #endif
                break;
            }
        }
    }

    void dave_init_session() {
        dave_.init_session(dave_protocol_version_, channel_id_.load(), user_id_str_);

        const auto key_package = dave_.session_->GetMarshalledKeyPackage();
        
        if (key_package.empty()) {
            #ifdef DISCUSY_LOGGING
            log("Fatal: libdave failed to generate an MLS Key Package. Reconnecting.");
            #endif
            stop();
            reconnect();
            return;
        }

        send_binary_dave_payload<VoiceOpcode::DaveMlsKeyPackage>(key_package);
    }

    void send_identify() {
        if (!initialized_.test_and_set()) {
            identify id{
                .server_id{server_id_},
                .user_id{user_id_},
                .session_id{session_id_},
                .token{token_},
            };
            identify_payload payload{id};

            std::string out;
            if (json::write_json(payload, out, shared_json_ctx_, json_logger)) {
                escalate(escalation_action::fatal);
                return;
            }

            #ifdef DISCUSY_LOGGING
            log("Sent IDENTIFY: {}", out);
            #endif
            ws_.send(std::move(out));
        } else { // send resume
            resume r{
                .server_id{server_id_},
                .session_id{session_id_},
                .token{token_},
                .seq_ack = seq_,
            };
            resume_payload payload{r};

            std::string out;
            if (json::write_json(payload, out, shared_json_ctx_, json_logger)) {
                escalate(escalation_action::fatal);
                return;
            }

            #ifdef DISCUSY_LOGGING
            log("Sent RESUME: {}", out);
            #endif
            ws_.send(std::move(out));
        }
    }

    template <VoiceOpcode op>
    void send_binary_dave_payload(const std::span<const uint8_t> data) {
        std::string out;
        out.reserve(1 + data.size());

        out.push_back(static_cast<char>(op));
        out.append(reinterpret_cast<const char*>(data.data()), data.size());

        ws_.send(std::move(out), true);
    }

    void send_invalid_commit_welcome() {
        dave_mls_invalid_commit_welcome invalid_cw{
            .transition_id = transition_id_,
        };
        dave_mls_invalid_commit_welcome_payload payload{invalid_cw};

        std::string out;
        if (json::write_json(payload, out, shared_json_ctx_, json_logger)) {
            escalate(escalation_action::fatal);
            return;
        }

        #ifdef DISCUSY_LOGGING
        log("Sent invalid commit/welcome for transition ID: {}", transition_id_);
        #endif
        ws_.send(std::move(out));

        dave_.create_session();
        dave_init_session();
    }

    void send_ready_for_transition() {
        dave_protocol_ready_for_transition r{
            .transition_id = transition_id_,
        };
        dave_protocol_ready_for_transition_payload payload{r};

        std::string out;
        if (json::write_json(payload, out, shared_json_ctx_, json_logger)) {
            escalate(escalation_action::fatal);
            return;
        }

        #ifdef DISCUSY_LOGGING
        log("Sent ready for transition: {}", out);
        #endif
        ws_.send(std::move(out));

        if (transition_id_ == 0) {
            apply_pending_passthrough();
            apply_key_ratchet();
        }
    }

    bool apply_key_ratchet() {
        auto key_ratchet = dave_.session_->GetKeyRatchet(user_id_str_);

        if (!key_ratchet) {
            #ifdef DISCUSY_LOGGING
            log("No key ratchet available yet, keeping the current one");
            #endif
            return false;
        }

        {
        std::scoped_lock lock{encryptor_mutex_};
        dave_.encryptor_->SetKeyRatchet(std::move(key_ratchet));
        }

        if (udp_client_) udp_client_->kick_drain();
        maybe_signal_ready();
        return true;
    }

    void apply_pending_passthrough() {
        if (!pending_passthrough_) return;

        const bool passthrough = *pending_passthrough_;
        pending_passthrough_.reset();

        {
        std::scoped_lock lock{encryptor_mutex_};
        dave_.encryptor_->SetPassthroughMode(passthrough);
        }

        if (udp_client_) udp_client_->kick_drain();
        maybe_signal_ready();
    }

    void drop_dave_keys() {
        pending_passthrough_.reset();
        transition_id_ = 0;

        std::scoped_lock lock{encryptor_mutex_};
        dave_.encryptor_->SetKeyRatchet(nullptr);
        dave_.encryptor_->SetPassthroughMode(false);
    }

    void send_speaking(std::optional<speaking_mode> mode_override = std::nullopt) {
        speaking_ s{
            .speaking = mode_override ? *mode_override : speaking_mode_,
            .ssrc = ssrc_,
        };
        speaking_payload payload{s};

        std::string out;
        if (json::write_json(payload, out, shared_json_ctx_, json_logger)) {
            escalate(escalation_action::fatal);
            return;
        }

        #ifdef DISCUSY_LOGGING
        log("Sent speaking: {}", out);
        #endif
        ws_.send(std::move(out));

    }

    bool send_heartbeat() {
        if (!recieved_heartbeat_ack_) {
            stop();
            reconnect();
            return false;
        }

        heartbeat hb{
            .t = static_cast<integer>(rnd_.next() & 0x1FFFFFFFFFFFFFLL),
            .seq_ack = seq_,
        };
        heartbeat_payload payload{hb};

        last_hb_nonce_ = hb.t;

        std::string out;
        static constexpr glz::opts opts{.skip_null_members = false};
        if (json::write_json<opts>(payload, out, shared_json_ctx_, json_logger)) {
            escalate(escalation_action::fatal);
            return false;
        }

        #ifdef DISCUSY_LOGGING
        log("Sent heartbeat: {}", out);
        #endif
        recieved_heartbeat_ack_ = false;
        ws_.send(std::move(out));
        return true;
    }

    void start_heartbeat(const std::chrono::milliseconds interval) {
        recieved_heartbeat_ack_ = true; // initial state

        hb_timer_.expires_after(interval);
        hb_timer_.async_wait([i = this, interval](this auto&& self, asio::ec_t ec) -> void {
            if (ec) return;

            if (i->send_heartbeat()) {
                i->hb_timer_.expires_at(i->hb_timer_.expiry() + interval);
                i->hb_timer_.async_wait(self);
            }
        });
    }

    void stop_heartbeat() {
        hb_timer_.cancel();
    }

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
private:
    struct open_cb    { connection* s; void operator()() const { if(s) s->handle_ws_open(); } };
    struct connect_cb { connection* s; void operator()() const { if(s) s->handle_ws_connect_ready(); } };
    struct close_cb   { connection* s; void operator()(std::uint16_t c) const { if(s) s->handle_ws_close(c); } };
    struct message_cb { connection* s; void operator()(std::string_view m, bool b) const { if(s) s->handle_ws_message(m, b); } };

    using ws_client_t = ws::websocket_client<open_cb, close_cb, message_cb, connect_cb>;

    std::shared_ptr<ws_client_t> ws_shared_;
    ws_client_t& ws_;

    struct udp_on_error_cb { std::weak_ptr<connection> s; void operator()(const boost::system::error_code& ec) const {
        auto self = s.lock();
        if (!self) return;

        boost::asio::post(self->strand_, [s = s, ec]() {
            if (auto self = s.lock()) self->handle_udp_failure(ec);
        });
    } };

    struct udp_on_idle_cb { std::weak_ptr<connection> s; void operator()(const audio_stop_reason reason) const {
        auto self = s.lock();
        if (!self || self->closed_.load(std::memory_order_acquire)) return;

        self->on_audio_stopped.fire(audio_stopped{
            .reason = reason,
            .guild_id = self->server_id_,
        });
    } };

    struct udp_dave_encrypt { std::weak_ptr<connection> s; encrypt_result operator()(const discusy::voice::store<discusy::voice::MAX_OPUS_FRAME_BYTES>& audio, discusy::voice::store<MAX_DAVE_BUFFER_SIZE>& output) const {
        auto ptr = s.lock();
        if (!ptr) {
            output.clear();
            return encrypt_result::drop;
        }

        auto& self = *ptr;

        discord::dave::IEncryptor::ResultCode result{};
        size_t bytes_written{0};

        {
        std::scoped_lock lock{self.encryptor_mutex_};
        if (!self.dave_.encryptor_->HasKeyRatchet() && !self.dave_.encryptor_->IsPassthroughMode()) {
            output.clear();
            return encrypt_result::not_ready;
        }

        result = self.dave_.encryptor_->Encrypt(
            discord::dave::MediaType::Audio,
            self.ssrc_,
            discord::dave::MakeArrayView<const std::uint8_t>(audio.span().data(), audio.span().size()),
            discord::dave::MakeArrayView(output.raw_buf.data(), output.raw_buf.size()),
            &bytes_written
        );
        }

        if (result != discord::dave::IEncryptor::Success) {
            #ifdef DISCUSY_LOGGING
            self.log("DAVE encryption failed with code: {}", static_cast<int>(result));
            #endif
            output.clear();
            return encrypt_result::drop;
        }

        output.set_size(bytes_written);
        return encrypt_result::ok;
    } };

    using udp_client_t = udp::client<udp_on_error_cb, udp_dave_encrypt, udp_on_idle_cb>;

    std::shared_ptr<udp_client_t> udp_client_shared_;
    udp_client_t* udp_client_ = nullptr;

    using escalation_cb_t = std::decay_t<escalationCB>;

    escalation_cb_t on_escalation_;

    std::int64_t seq_{-1};
    std::int64_t transition_id_{};
    std::optional<bool> pending_passthrough_{};

    dave dave_;
    std::mutex encryptor_mutex_;

    std::mutex opus_mutex_;
    OpusEncoder* opus_encoder_{nullptr};
    
    std::uint32_t ssrc_{0};

    ctx::io_context::strand_timer_t hb_timer_;

    ctx::io_context::strand_timer_t reconnect_timeout_timer_;

    ctx::io_context::strand_timer_t refresh_watchdog_;

    std::chrono::milliseconds reconnect_backoff_time_{0};

    discord::dave::ProtocolVersion dave_protocol_version_{};

    std::int64_t last_hb_nonce_{-1};

    std::set<std::string> recognised_users_;

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
    bool awaiting_connect_ready_{false};
    bool reconnect_scheduled_{false};
    bool escalated_{false};

    speaking_mode speaking_mode_{speaking_mode::Microphone};

    static constexpr int MAX_LOCAL_REBUILDS = 2;
    int local_rebuild_attempts_{0};

    std::uint32_t reconnect_attempts_{0};

    static constexpr std::uint32_t MAX_RECONNECT_ATTEMPTS = 10;

    [[nodiscard]] static bool is_aes256_gcm_supported() noexcept {
        static const bool supported = []() noexcept -> bool {
            if (sodium_init() < 0) {
                return false;
            }
            return crypto_aead_aes256gcm_is_available() != 0;
        }();
        return supported;
    }

    glz::context shared_json_ctx_{};
};

}