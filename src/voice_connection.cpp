#include <discusy/voice/voice_connection.hpp>

#include <algorithm>
#include <bit>
#include <cstring>
#include <limits>
#include <thread>
#include <utility>

namespace discusy::voice {

connection::connection(
    ctx::io_context& ctx,
    state& state,
    snowflake server_id,
    snowflake channel_id,
    snowflake user_id,
    std::string session_id,
    std::string token,
    std::string endpoint,
    escalation_cb_t escalation_cb
) :
    io_ctx_{ctx},
    strand_{io_ctx_.make_strand()},
    ws_shared_{std::make_shared<ws_client_t>(strand_, state.ssl_ctx)},
    ws_{*ws_shared_},
    on_escalation_{std::move(escalation_cb)},
    hb_timer_{strand_},
    reconnect_timeout_timer_{strand_},
    refresh_watchdog_{strand_},
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
}

void connection::start() {
    if (started_.test_and_set(std::memory_order_acq_rel)) return;

    auto client = std::make_shared<udp_client_t>(
        io_ctx_,
        udp_on_error_cb{this->weak_from_this()},
        udp_dave_encrypt{this->weak_from_this()},
        udp_on_idle_cb{this->weak_from_this()},
        udp_on_receive_cb{this->weak_from_this()}
    );
    client->set_underrun_grace_frames(underrun_grace_frames_.load(std::memory_order_relaxed));
    client->set_max_catchup_frames(max_catchup_frames_.load(std::memory_order_relaxed));
    client->set_not_ready_hold_frames(not_ready_hold_frames_.load(std::memory_order_relaxed));
    udp_client_shared_ = std::move(client);
    udp_client_.store(udp_client_shared_.get(), std::memory_order_release);

    boost::asio::dispatch(strand_, [self = this->shared_from_this()]() {
        if (self->destructed_) return;
        self->reconnect(); // gives us the logic in case of failure to open
    });
}

connection::~connection() noexcept {
    try {
        destruct();
    } catch (...) {
        destructed_ = true;
        closed_.store(true, std::memory_order_release);
    }
}

void connection::move_channel(snowflake new_channel_id) {
    if (closed_.load(std::memory_order_acquire)) return;

    boost::asio::dispatch(strand_, [self = this->shared_from_this(), new_channel_id]() {
        auto& c = *self;
        if (c.destructed_) return;

        const snowflake old_channel_id{c.channel_id_.load()};

        if (new_channel_id && (new_channel_id != old_channel_id)) {
            c.channel_id_.store(new_channel_id.value, std::memory_order_release);
            c.channel_moved_pending_refresh_ = true;

            c.drop_dave_keys();
            c.dave_.create_session();

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

void connection::refresh(std::string session_id, std::string token, std::string endpoint, snowflake channel_id) {
    if (closed_.load(std::memory_order_acquire)) return;

    boost::asio::dispatch(strand_, [self = this->shared_from_this(), session_id = std::move(session_id), token = std::move(token), endpoint = std::move(endpoint), channel_id]() mutable {
        auto& c = *self;
        if (c.destructed_) return;

        const snowflake old_channel_id{c.channel_id_.load()};
        const bool channel_changed = channel_id && (channel_id != old_channel_id);

        if (channel_changed) {
            c.channel_id_.store(channel_id.value, std::memory_order_release);

            c.drop_dave_keys();
            c.dave_.create_session();

            #ifdef DISCUSY_LOGGING
            c.log("Moved to channel {}, recognised users and DAVE keys reset", channel_id.value);
            #endif

            c.on_channel_move.fire(channel_moved{
                .old_channel_id = old_channel_id,
                .new_channel_id = channel_id,
            });
        }

        const bool same_server = c.session_live_ &&
                                 !channel_changed &&
                                 !c.channel_moved_pending_refresh_ &&
                                 (c.endpoint_ == endpoint) &&
                                 (c.token_ == token) &&
                                 (c.session_id_ == session_id);

        c.channel_moved_pending_refresh_ = false;
        c.session_id_ = std::move(session_id);
        c.token_ = std::move(token);

        if (same_server) {
            #ifdef DISCUSY_LOGGING
            c.log("Same voice server, staying connected");
            #endif

            c.cancel_refresh_watchdog();
            c.escalated_ = false;
            return;
        }

        #ifdef DISCUSY_LOGGING
        c.log("Voice server/session changed, rebuilding onto: {}", endpoint);
        #endif

        c.endpoint_ = std::move(endpoint);

        c.begin_fresh_session();
    });
}

void connection::set_speaking_mode(speaking_mode mode) {
    if (closed_.load(std::memory_order_acquire)) return;

    boost::asio::dispatch(strand_, [self = this->shared_from_this(), mode]() {
        if (self->destructed_) return;
        self->speaking_mode_ = mode;
        self->send_speaking();
    });
}

void connection::stop_audio() {
    auto* const udp = udp_client();
    if (closed_.load(std::memory_order_acquire) || (udp == nullptr)) return;
    udp->stop_audio();
}

void connection::pause_audio() {
    auto* const udp = udp_client();
    if (closed_.load(std::memory_order_acquire) || (udp == nullptr)) return;
    udp->pause_audio();
}

void connection::rebuild_in_place() {
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

void connection::resume_audio() {
    auto* const udp = udp_client();
    if (closed_.load(std::memory_order_acquire) || (udp == nullptr)) return;
    udp->resume_audio();
}

double connection::get_secs_remaining() const noexcept {
    const auto* const udp = udp_client();
    if (closed_.load(std::memory_order_acquire) || (udp == nullptr)) return 0.0;
    return udp->get_secs_remaining();
}

std::chrono::milliseconds connection::get_ms_remaining() const noexcept {
    const auto* const udp = udp_client();
    if (closed_.load(std::memory_order_acquire) || (udp == nullptr)) return std::chrono::milliseconds{0};
    return udp->get_ms_remaining();
}

std::uint64_t connection::get_frames_remaining() const noexcept {
    const auto* const udp = udp_client();
    if (closed_.load(std::memory_order_acquire) || (udp == nullptr)) return 0;
    return udp->get_frames_remaining();
}

void connection::set_underrun_grace_frames(const std::uint32_t frames) {
    const auto clamped = std::max<std::uint32_t>(1, frames);
    underrun_grace_frames_.store(clamped, std::memory_order_relaxed);
    if (auto* const udp = udp_client()) udp->set_underrun_grace_frames(clamped);
}

void connection::set_max_catchup_frames(const std::uint32_t frames) {
    const auto clamped = std::max<std::uint32_t>(1, frames);
    max_catchup_frames_.store(clamped, std::memory_order_relaxed);
    if (auto* const udp = udp_client()) udp->set_max_catchup_frames(clamped);
}

void connection::set_not_ready_hold_frames(const std::uint32_t frames) {
    not_ready_hold_frames_.store(frames, std::memory_order_relaxed);
    if (auto* const udp = udp_client()) udp->set_not_ready_hold_frames(frames);
}

audio_stats connection::get_audio_stats() const noexcept {
    audio_stats stats{};
    if (const auto* const udp = udp_client()) {
        stats = udp->get_stats();
    }
    stats.dave_decrypt_drops = stat_dave_decrypt_drops_.load(std::memory_order_relaxed);
    return stats;
}

void connection::reset_audio_stats() noexcept {
    if (auto* const udp = udp_client()) udp->reset_stats();
    stat_dave_decrypt_drops_.store(0, std::memory_order_relaxed);
}

bool connection::set_inband_fec(const bool enabled) {
    if (closed_.load(std::memory_order_acquire)) return false;
    const std::scoped_lock lock{encoder_mutex_};
    return encoder_.set_inband_fec(enabled);
}

bool connection::set_expected_packet_loss(const int percentage) {
    if (closed_.load(std::memory_order_acquire)) return false;
    const std::scoped_lock lock{encoder_mutex_};
    return encoder_.set_expected_packet_loss(percentage);
}

bool connection::set_opus_complexity(const int complexity) {
    if (closed_.load(std::memory_order_acquire)) return false;
    const std::scoped_lock lock{encoder_mutex_};
    return encoder_.set_complexity(complexity);
}

bool connection::set_opus_signal(const int signal) {
    if (closed_.load(std::memory_order_acquire)) return false;
    const std::scoped_lock lock{encoder_mutex_};
    return encoder_.set_signal(signal);
}

bool connection::reset_opus_state() noexcept {
    if (closed_.load(std::memory_order_acquire)) return false;
    const std::scoped_lock lock{encoder_mutex_};
    return encoder_.reset_state();
}

void connection::set_receive_any_audio(const bool enabled) {
    if (closed_.load(std::memory_order_acquire)) return;

    {
    const std::scoped_lock lock{receive_mutex_};
    receive_any_ = enabled;
    sink_wants_dirty_ = true;
    }
    dispatch_media_sink_wants();
}

void connection::set_receive_user_audio(const snowflake user_id, const bool enabled) {
    if (closed_.load(std::memory_order_acquire) || !user_id) return;

    {
    const std::scoped_lock lock{receive_mutex_};
    user_wants_[user_id] = enabled;
    sink_wants_dirty_ = true;
    }
    dispatch_media_sink_wants();
}

void connection::set_received_users(const std::span<const snowflake> user_ids) {
    if (closed_.load(std::memory_order_acquire)) return;

    {
    const std::scoped_lock lock{receive_mutex_};
    receive_any_ = false;
    user_wants_.clear();
    raw_ssrc_wants_.clear();
    for (const auto id : user_ids) {
        if (id) user_wants_[id] = true;
    }
    sink_wants_dirty_ = true;
    }
    dispatch_media_sink_wants();
}

void connection::clear_user_audio_filters() {
    if (closed_.load(std::memory_order_acquire)) return;

    {
    const std::scoped_lock lock{receive_mutex_};
    user_wants_.clear();
    raw_ssrc_wants_.clear();
    sink_wants_dirty_ = true;
    }
    dispatch_media_sink_wants();
}

void connection::enable_all_audio_receive() {
    if (closed_.load(std::memory_order_acquire)) return;

    {
    const std::scoped_lock lock{receive_mutex_};
    receive_any_ = true;
    user_wants_.clear();
    raw_ssrc_wants_.clear();
    sink_wants_dirty_ = true;
    }
    dispatch_media_sink_wants();
}

void connection::send_media_sink_wants(const std::unordered_map<std::string, std::uint8_t>& wants) {
    if (closed_.load(std::memory_order_acquire)) return;

    {
    const std::scoped_lock lock{receive_mutex_};
    receive_any_ = false;
    user_wants_.clear();
    raw_ssrc_wants_.clear();
    for (const auto& [key, val] : wants) {
        const std::uint8_t normalized = (val > 0) ? 100 : 0;
        if (key == "any") {
            receive_any_ = (normalized != 0);
        } else if (const auto parsed = ulp::str::to_number<std::uint32_t>(key)) {
            raw_ssrc_wants_[*parsed] = normalized;
        }
    }
    sink_wants_dirty_ = true;
    }
    dispatch_media_sink_wants();
}

bool connection::is_receiving_any_audio() const noexcept {
    const std::scoped_lock lock{receive_mutex_};
    return receive_any_;
}

opt<snowflake> connection::get_user_for_ssrc(const std::uint32_t ssrc) const noexcept {
    const std::scoped_lock lock{receive_mutex_};
    if (const auto it = ssrc_to_user_.find(ssrc); it != ssrc_to_user_.end()) {
        return it->second;
    }
    return std::nullopt;
}

opt<std::uint32_t> connection::get_ssrc_for_user(const snowflake user_id) const noexcept {
    const std::scoped_lock lock{receive_mutex_};
    if (const auto it = user_to_ssrc_.find(user_id); it != user_to_ssrc_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void connection::send_opus_frame(const std::span<const std::uint8_t> opus_data) {
    auto* const udp = udp_client();
    if (closed_.load(std::memory_order_acquire) || (udp == nullptr)) return;
    udp->send_audio_packet(opus_data);
}

void connection::send_opus_frames(std::vector<discusy::voice::store<discusy::voice::MAX_OPUS_FRAME_BYTES>>&& opus_frames) {
    auto* const udp = udp_client();
    if (closed_.load(std::memory_order_acquire) || (udp == nullptr)) return;
    udp->send_audio_packets(std::move(opus_frames));
}

void connection::send_opus_frames(std::span<const discusy::voice::store<discusy::voice::MAX_OPUS_FRAME_BYTES>> opus_frames) {
    auto* const udp = udp_client();
    if (closed_.load(std::memory_order_acquire) || (udp == nullptr)) return;
    udp->send_audio_packets(opus_frames);
}

void connection::send_pcm(
    std::span<const std::int16_t> pcm_data,
    const std::chrono::milliseconds batch_duration
) {
    if (closed_.load(std::memory_order_acquire) || (udp_client() == nullptr)) return;

    try {
        static constexpr int SAMPLES_PER_FRAME = (DISCORD_SAMPLING_RATE / 1000) * DISCORD_FRAME_SIZE_MS;
        static constexpr std::size_t TOTAL_FRAME_SAMPLES = static_cast<std::size_t>(SAMPLES_PER_FRAME) * DISCORD_CHANNELS;

        const auto total = pcm_data.size();
        if (total == 0) return;

        const auto duration_ms = std::max<std::int64_t>(0, batch_duration.count());
        const std::size_t batch_frames_limit = std::max<std::size_t>(
            1,
            static_cast<std::size_t>(duration_ms) / DISCORD_FRAME_SIZE_MS
        );

        std::array<std::int16_t, TOTAL_FRAME_SAMPLES> padded_pcm{};
        std::vector<discusy::voice::store<discusy::voice::MAX_OPUS_FRAME_BYTES>> batch;

        for (std::size_t pos = 0; pos < total; ) {
            if (closed_.load(std::memory_order_acquire)) return;

            const std::size_t remaining_frames = ((total - pos) + TOTAL_FRAME_SAMPLES - 1) / TOTAL_FRAME_SAMPLES;
            const std::size_t chunk_frames = std::min(batch_frames_limit, remaining_frames);
            batch.clear();
            batch.reserve(chunk_frames);

            {
            const std::scoped_lock lock{encoder_mutex_};
            if (closed_.load(std::memory_order_acquire) || !encoder_) return;
            check_and_apply_bitrate();

            for (std::size_t f = 0; (f < chunk_frames) && (pos < total); ++f, pos += TOTAL_FRAME_SAMPLES) {
                const std::size_t remaining_samples = total - pos;
                const std::int16_t* current_pcm_ptr = pcm_data.data() + pos;

                if (remaining_samples < TOTAL_FRAME_SAMPLES) { // end of data, pad with silence
                    std::ranges::fill(padded_pcm, 0);
                    std::copy_n(current_pcm_ptr, remaining_samples, padded_pcm.begin());
                    current_pcm_ptr = padded_pcm.data();
                }

                discusy::voice::store<discusy::voice::MAX_OPUS_FRAME_BYTES>& opus_out = batch.emplace_back();
                const int bytes_encoded = encoder_.encode_frame(
                    current_pcm_ptr,
                    SAMPLES_PER_FRAME,
                    opus_out.raw_buf.data(),
                    discusy::voice::MAX_OPUS_FRAME_BYTES
                );

                if (bytes_encoded > 0) {
                    opus_out.set_size(static_cast<std::size_t>(bytes_encoded));
                } else {
                    batch.pop_back();
                }
            }
            }

            if (!batch.empty()) {
                send_opus_frames(std::move(batch));
                batch = {};
            }

            if (pos < total) {
                std::this_thread::yield();
            }
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

void connection::send_pcm_bytes(
    std::span<const std::uint8_t> raw_bytes,
    const std::chrono::milliseconds batch_duration
) {
    if (closed_.load(std::memory_order_acquire) || (udp_client() == nullptr)) return;

    const auto byte_count = raw_bytes.size();
    const auto sample_count = byte_count / sizeof(std::int16_t);
    if (sample_count == 0) return;

    try {
        static constexpr int SAMPLES_PER_FRAME = (DISCORD_SAMPLING_RATE / 1000) * DISCORD_FRAME_SIZE_MS;
        static constexpr std::size_t TOTAL_FRAME_SAMPLES = static_cast<std::size_t>(SAMPLES_PER_FRAME) * DISCORD_CHANNELS;

        const auto duration_ms = std::max<std::int64_t>(0, batch_duration.count());
        const std::size_t batch_frames_limit = std::max<std::size_t>(
            1,
            static_cast<std::size_t>(duration_ms) / DISCORD_FRAME_SIZE_MS
        );

        const auto* raw_ptr = raw_bytes.data();
        std::array<std::int16_t, TOTAL_FRAME_SAMPLES> frame_pcm{};
        std::vector<discusy::voice::store<discusy::voice::MAX_OPUS_FRAME_BYTES>> batch;

        for (std::size_t pos = 0; pos < sample_count; ) {
            if (closed_.load(std::memory_order_acquire)) return;

            const std::size_t remaining_frames = ((sample_count - pos) + TOTAL_FRAME_SAMPLES - 1) / TOTAL_FRAME_SAMPLES;
            const std::size_t chunk_frames = std::min(batch_frames_limit, remaining_frames);
            batch.clear();
            batch.reserve(chunk_frames);

            {
            const std::scoped_lock lock{encoder_mutex_};
            if (closed_.load(std::memory_order_acquire) || !encoder_) return;
            check_and_apply_bitrate();

            for (std::size_t f = 0; (f < chunk_frames) && (pos < sample_count); ++f, pos += TOTAL_FRAME_SAMPLES) {
                const std::size_t chunk_samples = std::min(TOTAL_FRAME_SAMPLES, sample_count - pos);
                std::memcpy(frame_pcm.data(), raw_ptr + (pos * sizeof(std::int16_t)), chunk_samples * sizeof(std::int16_t));
                if (chunk_samples < TOTAL_FRAME_SAMPLES) {
                    std::fill(frame_pcm.begin() + static_cast<std::ptrdiff_t>(chunk_samples), frame_pcm.end(), std::int16_t{0});
                }

                discusy::voice::store<discusy::voice::MAX_OPUS_FRAME_BYTES>& opus_out = batch.emplace_back();
                const int bytes_encoded = encoder_.encode_frame(
                    frame_pcm.data(),
                    SAMPLES_PER_FRAME,
                    opus_out.raw_buf.data(),
                    discusy::voice::MAX_OPUS_FRAME_BYTES
                );

                if (bytes_encoded > 0) {
                    opus_out.set_size(static_cast<std::size_t>(bytes_encoded));
                } else {
                    batch.pop_back();
                }
            }
            }

            if (!batch.empty()) {
                send_opus_frames(std::move(batch));
                batch = {};
            }

            if (pos < sample_count) {
                std::this_thread::yield();
            }
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

void connection::destruct() {
    if (destructed_) return;
    destructed_ = true;
    ++session_generation_;

    closed_.store(true, std::memory_order_release);

    if (auto* const udp = udp_client()) udp->close();

    stop_heartbeat();
    ++reconnect_timer_gen_;
    reconnect_timeout_timer_.cancel();
    cancel_refresh_watchdog();
    awaiting_connect_ready_ = false;
    reconnect_scheduled_ = false;

    ws_.stop();
    ws_.pause();

    ws_.on_connect_ready({});
    ws_.on_close({});
    ws_.on_message({});
    ws_.on_open({});

    ws_.clear();

    {
    const std::scoped_lock lock{encoder_mutex_};
    encoder_.destroy();
    }

    pending_transitions_.clear();

    {
    const std::scoped_lock lock{decoder_mutex_};
    opus_decoders_.clear();
    }
    {
    const std::scoped_lock lock{receive_mutex_};
    user_decryptors_.clear();
    ssrc_to_user_.clear();
    user_to_ssrc_.clear();
    }

    if (!io_ctx_->stopped()) {
        on_closed.fire(voice_closed{.guild_id = server_id_});
    }
}

void connection::stop() {
    ++session_generation_;
    reconnecting_ = true;
    awaiting_connect_ready_ = false;
    session_live_ = false;
    ready_received_ = false;
    ready_.store(false, std::memory_order_release);
    ready_signalled_ = false;
    stop_heartbeat();
    ++reconnect_timer_gen_;
    reconnect_timeout_timer_.cancel();
    reconnect_scheduled_ = false;
    ws_.disconnect();
}

void connection::escalate(const escalation_action action) {
    if (destructed_ || escalated_) return;

    escalated_ = true;
    stop();

    if (action == escalation_action::requires_gateway_reconnect) arm_refresh_watchdog();

    std::invoke(on_escalation_, action);
}

void connection::await_gateway_instruction() {
    if (destructed_ || escalated_) return;

    escalated_ = true;
    stop();
    arm_refresh_watchdog();
}

void connection::cancel_refresh_watchdog() {
    ++refresh_watchdog_gen_;
    refresh_watchdog_.cancel();
}

void connection::arm_refresh_watchdog() {
    const auto gen = ++refresh_watchdog_gen_;
    refresh_watchdog_.expires_after(std::chrono::seconds{30});
    refresh_watchdog_.async_wait([weak = this->weak_from_this(), gen](const asio::ec_t ec) {
        if (ec) return;

        const auto self = weak.lock();
        if (!self || self->destructed_ || (self->refresh_watchdog_gen_ != gen)) return;

        #ifdef DISCUSY_LOGGING
        self->log("No refreshed voice session arrived, rebuilding on the endpoint we have");
        #endif

        self->escalated_ = false;
        self->begin_fresh_session(false);
    });
}

void connection::begin_fresh_session(const bool reset_backoff) {
    stop();
    cancel_refresh_watchdog();

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
    session_description_received_ = false;
    channel_moved_pending_refresh_ = false;

    drop_dave_keys();
    dave_.create_session();

    if (auto* const udp = udp_client()) udp->reset_session();

    reconnect();
}

void connection::maybe_signal_ready() {
    if (destructed_ || ready_signalled_ || !session_live_) return;

    ready_signalled_ = true;
    ready_.store(true, std::memory_order_release);
    local_rebuild_attempts_ = 0;

    #ifdef DISCUSY_LOGGING
    log("Ready to transmit audio");
    #endif

    on_ready.fire(voice_ready{.guild_id = server_id_});
}

void connection::handle_ws_open() {
    if (destructed_ || escalated_) return;
    #ifdef DISCUSY_LOGGING
    log("Websocket open");
    #endif
    reconnecting_ = false;
    send_identify();
}

void connection::handle_ws_connect_ready() {
    if (destructed_ || escalated_ || !awaiting_connect_ready_) return;
    awaiting_connect_ready_ = false;

    reconnecting_ = true;
    session_live_ = false;
    ready_received_ = false;
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

void connection::handle_ws_close(const std::uint16_t code) {
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
            escalate(escalation_action::requires_gateway_reconnect);
            return;
        }

        case VoiceCloseCode::Disconnected:
        case VoiceCloseCode::DisconnectedCallTerminated: {
            #ifdef DISCUSY_LOGGING
            log("Disconnected by discord ({}), waiting for the gateway to say why", +code);
            #endif
            await_gateway_instruction();
            return;
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
            escalate(escalation_action::fatal);
            return;
        }

        default: break;
    }

    stop();

    reconnect();
}

void connection::handle_udp_failure(const boost::system::error_code& ec) {
    if (destructed_ || escalated_) return;

    #ifdef DISCUSY_LOGGING
    log("UDP transport failed ({}), rebuilding the voice session", ec.message());
    #else
    (void)ec;
    #endif

    begin_fresh_session(false);
}

void connection::handle_ws_message(const std::string_view msg, bool binary) {
    if (destructed_ || escalated_) return;
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

void connection::setup_gateway_callbacks() {
    ws_.on_open(ws_client_t::open_callback::bind<&connection::handle_ws_open>(this));
    ws_.on_connect_ready(ws_client_t::connect_ready_callback::bind<&connection::handle_ws_connect_ready>(this));
    ws_.on_close(ws_client_t::close_callback::bind<&connection::handle_ws_close>(this));
    ws_.on_message(ws_client_t::message_callback::bind<&connection::handle_ws_message>(this));
}

void connection::reconnect_internal() {
    reconnect_scheduled_ = false;

    if (destructed_ || escalated_) return;

    const auto gen = ++reconnect_timer_gen_;
    reconnect_timeout_timer_.cancel();
    reconnect_timeout_timer_.expires_after(std::chrono::seconds{25});
    reconnect_timeout_timer_.async_wait([weak = this->weak_from_this(), gen](const asio::ec_t ec) {
        if (ec) return;
        const auto self = weak.lock();
        if (!self || self->destructed_ || (self->reconnect_timer_gen_ != gen)) return;

        #ifdef DISCUSY_LOGGING
        self->log("Reconnect timeout expired, attempting to reconnect...");
        #endif

        self->stop();
        self->reconnect();
    });

    awaiting_connect_ready_ = true;
    ws_.close();
}

void connection::reconnect() {
    if (destructed_ || escalated_) return;

    if (reconnect_scheduled_) return;

    reconnecting_ = true;
    ++reconnect_timer_gen_;
    reconnect_timeout_timer_.cancel();

    if (++reconnect_attempts_ > MAX_RECONNECT_ATTEMPTS) {
        #ifdef DISCUSY_LOGGING
        log("Giving up after {} failed voice connection attempts", reconnect_attempts_ - 1);
        #endif
        escalate(escalation_action::fatal);
        return;
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
        const auto gen = ++reconnect_timer_gen_;
        reconnect_timeout_timer_.expires_after(delay);
        reconnect_timeout_timer_.async_wait([weak = this->weak_from_this(), gen](const asio::ec_t ec) {
            if (ec) return;
            const auto self = weak.lock();
            if (!self || self->destructed_ || (self->reconnect_timer_gen_ != gen)) return;

            self->reconnect_internal();
        });
    }
}

void connection::handle_binary_payload(std::string_view data) {
    if (data.size() < 3) return;

    std::uint16_t seq{};
    std::memcpy(&seq, data.data(), sizeof(seq));
    if constexpr (std::endian::native == std::endian::little) {
        seq = std::byteswap(seq);
    }

    if (seq != std::uint16_t{0}) seq_ = seq;

    const auto opcode = static_cast<VoiceOpcode>(static_cast<std::uint8_t>(data[2]));
    const std::string_view payload = data.substr(3);
    const auto* const payload_bytes = reinterpret_cast<const std::uint8_t*>(payload.data());

    switch (opcode) {
        case VoiceOpcode::DaveMlsExternalSender: {
            dave_.session_->SetExternalSender(
                std::vector<std::uint8_t>(payload_bytes, payload_bytes + payload.size())
            );
            break;
        }

        case VoiceOpcode::DaveMlsProposals: {
            const auto commit_welcome = dave_.session_->ProcessProposals(
                std::vector<std::uint8_t>(payload_bytes, payload_bytes + payload.size())
            );

            if (commit_welcome) {
                send_binary_dave_payload(VoiceOpcode::DaveMlsCommitWelcome, *commit_welcome);
            }
            break;
        }

        case VoiceOpcode::DaveMlsAnnounceCommitTransition: {
            if (payload.size() < 2) return;

            discord::dave::TransitionId transition_id{};
            std::memcpy(&transition_id, payload.data(), sizeof(transition_id));
            if constexpr (std::endian::native == std::endian::little) transition_id = std::byteswap(transition_id);

            const auto result = dave_.session_->ProcessCommit(
                std::vector<std::uint8_t>(payload_bytes + 2, payload_bytes + payload.size())
            );

            if (std::holds_alternative<discord::dave::failed_t>(result)) {
                send_invalid_commit_welcome(transition_id);
            } else if (std::holds_alternative<discord::dave::RosterMap>(result)) {
                prepare_dave_transition(transition_id, dave_.session_->GetProtocolVersion());
                maybe_send_ready_for_transition(transition_id);
            }
            break;
        }

        case VoiceOpcode::DaveMlsWelcome: {
            if (payload.size() < 2) return;

            discord::dave::TransitionId transition_id{};
            std::memcpy(&transition_id, payload.data(), sizeof(transition_id));
            if constexpr (std::endian::native == std::endian::little) transition_id = std::byteswap(transition_id);

            const auto result = dave_.session_->ProcessWelcome(
                std::vector<std::uint8_t>(payload_bytes + 2, payload_bytes + payload.size())
            );

            if (!result) {
                send_invalid_commit_welcome(transition_id);
            } else {
                prepare_dave_transition(transition_id, dave_.session_->GetProtocolVersion());
                maybe_send_ready_for_transition(transition_id);
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

void connection::handle_gateway_payload(std::string_view json_payload) {
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
            ++reconnect_timer_gen_;
            reconnect_timeout_timer_.cancel();
            reconnect_scheduled_ = false;

            auto* const udp = udp_client();
            if (udp == nullptr) {
                #ifdef DISCUSY_LOGGING
                log("READY arrived before start(), ignoring");
                #endif
                return;
            }

            udp->reset_session();

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

            const auto session_gen = session_generation_;
            udp->send_ip_discovery(std::move(ready.d.ip), ready.d.port, ssrc_, [weak = this->weak_from_this(), mode, strand = strand_, session_gen](std::string&& my_ip, std::uint16_t my_port) mutable {
                boost::asio::post(strand, [weak = std::move(weak), my_ip = std::move(my_ip), my_port, mode, session_gen] mutable {
                    const auto s = weak.lock();
                    if (!s) return;

                    auto& self = *s;
                    if (self.destructed_ || self.escalated_ || (self.session_generation_ != session_gen)) return;

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
                    if (json::write_json(payload, out, self.json_logger)) {
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
            const std::scoped_lock lock{encryptor_mutex_};
            encryptor_ssrc_ = ssrc_;
            dave_.encryptor_->AssignSsrcToCodec(ssrc_, discord::dave::Codec::Opus);
            }

            ready_received_ = true;
            sync_sink_wants_on_ready();
            break;
        };

        case VoiceOpcode::SessionDescription: {
            recieve_payload<session_description> description{};
            auto* const udp = udp_client();
            if ((udp == nullptr) || json::parse_json_view(description, json_payload, shared_json_ctx_, json_logger)) {
                stop();
                reconnect();
                return;
            }

            const auto max_supported_version = discord::dave::MaxSupportedProtocolVersion();
            if (description.d.dave_protocol_version > max_supported_version) {
                #ifdef DISCUSY_LOGGING
                log("Dave protocol version not supported! Recieved version: {} (max {})", description.d.dave_protocol_version, max_supported_version);
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

            dave_protocol_version_ = description.d.dave_protocol_version;
            latest_prepared_dave_version_ = dave_protocol_version_;
            dave_protocol_version_atomic_.store(dave_protocol_version_, std::memory_order_release);
            session_description_received_ = true;

            if (dave_protocol_version_ > discord::dave::kDisabledVersion) {
                {
                const std::scoped_lock lock{encryptor_mutex_};
                dave_.encryptor_->SetPassthroughMode(false);
                }
                update_all_decryptors_passthrough(false);
                dave_init_session(dave_protocol_version_);
            } else {
                prepare_dave_transition(discord::dave::kInitTransitionId, discord::dave::kDisabledVersion);
            }

            udp->set_secret_key(mode, description.d.secret_key);

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
            ++reconnect_timer_gen_;
            reconnect_timeout_timer_.cancel();
            reconnect_scheduled_ = false;
            reconnect_backoff_time_ = std::chrono::milliseconds{0};
            reconnect_attempts_ = 0;

            ready_received_ = true;
            if (should_sync_sink_wants_on_resume()) {
                send_media_sink_wants_current();
            }

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

        case VoiceOpcode::Speaking: {
            recieve_payload<speaking_recv> sp{};
            if (json::parse_json_view(sp, json_payload, shared_json_ctx_, json_logger)) {
                break;
            }

            if (sp.d.user_id && sp.d.user_id != user_id_ && sp.d.ssrc != 0) {
                bool should_resend_wants = false;
                bool ssrc_reassigned = false;
                opt<std::uint32_t> old_ssrc{};
                {
                const std::scoped_lock lock{receive_mutex_};
                if (const auto ssrc_it = ssrc_to_user_.find(sp.d.ssrc); ssrc_it != ssrc_to_user_.end() && ssrc_it->second != sp.d.user_id) {
                    const snowflake displaced_user = ssrc_it->second;
                    user_to_ssrc_.erase(displaced_user);
                    ssrc_reassigned = true;
                    if (user_wants_.contains(displaced_user)) {
                        should_resend_wants = true;
                    }
                }
                const auto old_it = user_to_ssrc_.find(sp.d.user_id);
                const bool ssrc_changed = (old_it == user_to_ssrc_.end()) || (old_it->second != sp.d.ssrc);
                if (old_it != user_to_ssrc_.end() && old_it->second != sp.d.ssrc) {
                    old_ssrc = old_it->second;
                    ssrc_to_user_.erase(old_it->second);
                }
                ssrc_to_user_[sp.d.ssrc] = sp.d.user_id;
                user_to_ssrc_[sp.d.user_id] = sp.d.ssrc;
                should_resend_wants = should_resend_wants || (ssrc_changed && user_wants_.contains(sp.d.user_id));
                }

                if (old_ssrc || ssrc_reassigned) {
                    const std::scoped_lock lock{decoder_mutex_};
                    if (old_ssrc) opus_decoders_.erase(*old_ssrc);
                    if (ssrc_reassigned) opus_decoders_.erase(sp.d.ssrc);
                }

                ensure_user_decryptor(sp.d.user_id);

                if (should_resend_wants && ready_received_) {
                    send_media_sink_wants_current();
                }
            }

            on_speaking.fire(user_speaking{
                .guild_id = server_id_,
                .user_id = sp.d.user_id,
                .ssrc = sp.d.ssrc,
                .speaking = sp.d.speaking,
            });
            break;
        }

        case VoiceOpcode::MediaSinkWants: {
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

            for (const auto& id : connected.d.user_ids) {
                if (id != user_id_str_) {
                    dave_.session_->AddRecognizedUser(id);
                    if (const auto parsed = ulp::str::to_number<std::uint64_t>(id)) {
                        ensure_user_decryptor(snowflake{*parsed});
                    }
                }
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

            dave_.session_->RemoveRecognizedUser(disconnected.d.user_id);

            if (const auto parsed = ulp::str::to_number<std::uint64_t>(disconnected.d.user_id)) {
                const snowflake uid{*parsed};
                opt<std::uint32_t> removed_ssrc{};
                bool in_user_wants = false;

                {
                const std::scoped_lock lock{receive_mutex_};
                user_decryptors_.erase(uid);
                if (const auto it = user_to_ssrc_.find(uid); it != user_to_ssrc_.end()) {
                    removed_ssrc = it->second;
                    ssrc_to_user_.erase(it->second);
                    user_to_ssrc_.erase(it);
                }
                in_user_wants = user_wants_.contains(uid);
                }

                if (removed_ssrc) {
                    const std::scoped_lock lock{decoder_mutex_};
                    opus_decoders_.erase(*removed_ssrc);
                }

                if (in_user_wants && removed_ssrc && ready_received_) {
                    send_media_sink_wants_current();
                }
            }

            break;
        }

        case VoiceOpcode::DavePrepareTransition: {
            recieve_payload<dave_protocol_prepare_transition> transition{};
            if (json::parse_json_view(transition, json_payload, shared_json_ctx_, json_logger)) {
                stop();
                reconnect();
                return;
            }

            if (transition.d.protocol_version < 0 ||
                std::cmp_greater(transition.d.protocol_version, discord::dave::MaxSupportedProtocolVersion())) {
                #ifdef DISCUSY_LOGGING
                log("Unsupported DAVE protocol version in PrepareTransition: {}", transition.d.protocol_version);
                #endif
                escalate(escalation_action::fatal);
                return;
            }

            if (transition.d.transition_id < 0 ||
                std::cmp_greater(transition.d.transition_id, std::numeric_limits<discord::dave::TransitionId>::max())) {
                break;
            }

            const auto tid = static_cast<discord::dave::TransitionId>(transition.d.transition_id);
            const auto version = static_cast<discord::dave::ProtocolVersion>(transition.d.protocol_version);

            prepare_dave_transition(tid, version);
            maybe_send_ready_for_transition(tid);

            break;
        }

        case VoiceOpcode::DaveExecuteTransition: {
            recieve_payload<dave_protocol_execute_transition> transition{};
            if (json::parse_json_view(transition, json_payload, shared_json_ctx_, json_logger)) {
                stop();
                reconnect();
                return;
            }

            if (transition.d.transition_id < 0 ||
                std::cmp_greater(transition.d.transition_id, std::numeric_limits<discord::dave::TransitionId>::max())) {
                break;
            }

            const auto tid = static_cast<discord::dave::TransitionId>(transition.d.transition_id);
            const auto it = pending_transitions_.find(tid);
            if (it == pending_transitions_.end()) {
                break;
            }

            const auto version = it->second;
            pending_transitions_.erase(it);

            execute_dave_transition_version(version);

            if (tid != discord::dave::kInitTransitionId) {
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

            if (epoch.d.protocol_version < 0 ||
                std::cmp_greater(epoch.d.protocol_version, discord::dave::MaxSupportedProtocolVersion())) {
                #ifdef DISCUSY_LOGGING
                log("Unsupported DAVE protocol version in PrepareEpoch: {}", epoch.d.protocol_version);
                #endif
                escalate(escalation_action::fatal);
                return;
            }

            const auto version = static_cast<discord::dave::ProtocolVersion>(epoch.d.protocol_version);

            if (epoch.d.epoch == 1) {
                if (version > discord::dave::kDisabledVersion) {
                    dave_init_session(version);
                } else {
                    dave_.session_->Reset();
                }
            } else {
                dave_.session_->SetProtocolVersion(version);
            }

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

void connection::dave_init_session(const discord::dave::ProtocolVersion protocol_version) {
    dave_.init_session(protocol_version, channel_id_.load(std::memory_order_acquire), user_id_str_);

    const auto key_package = dave_.session_->GetMarshalledKeyPackage();

    if (key_package.empty()) {
        #ifdef DISCUSY_LOGGING
        log("Fatal: libdave failed to generate an MLS Key Package. Reconnecting.");
        #endif
        stop();
        reconnect();
        return;
    }

    send_binary_dave_payload(VoiceOpcode::DaveMlsKeyPackage, key_package);
}

void connection::send_identify() {
    if (!initialized_.test_and_set()) {
        identify id{
            .server_id{server_id_},
            .user_id{user_id_},
            .session_id{session_id_},
            .token{token_},
            .max_dave_protocol_version = discord::dave::MaxSupportedProtocolVersion(),
        };
        identify_payload payload{id};

        std::string out;
        if (json::write_json(payload, out, json_logger)) {
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
        if (json::write_json(payload, out, json_logger)) {
            escalate(escalation_action::fatal);
            return;
        }

        #ifdef DISCUSY_LOGGING
        log("Sent RESUME: {}", out);
        #endif
        ws_.send(std::move(out));
    }
}

void connection::send_binary_dave_payload(const VoiceOpcode op, const std::span<const uint8_t> data) {
    std::string out;
    out.reserve(1 + data.size());

    out.push_back(static_cast<char>(op));
    out.append(reinterpret_cast<const char*>(data.data()), data.size());

    ws_.send(std::move(out), true);
}

void connection::send_invalid_commit_welcome(const discord::dave::TransitionId transition_id) {
    dave_mls_invalid_commit_welcome invalid_cw{
        .transition_id = transition_id,
    };
    const dave_mls_invalid_commit_welcome_payload payload{invalid_cw};

    std::string out;
    if (json::write_json(payload, out, json_logger)) {
        escalate(escalation_action::fatal);
        return;
    }

    #ifdef DISCUSY_LOGGING
    log("Sent invalid commit/welcome for transition ID: {}", transition_id);
    #endif
    ws_.send(std::move(out));

    const auto session_ver = dave_.session_->GetProtocolVersion();
    const auto recovery_ver = (session_ver != discord::dave::kDisabledVersion) ? session_ver : dave_protocol_version_;
    if (recovery_ver != discord::dave::kDisabledVersion) {
        dave_init_session(recovery_ver);
    }
}

void connection::maybe_send_ready_for_transition(const discord::dave::TransitionId transition_id) {
    if (transition_id == discord::dave::kInitTransitionId) return;

    dave_protocol_ready_for_transition r{
        .transition_id = transition_id,
    };
    const dave_protocol_ready_for_transition_payload payload{r};

    std::string out;
    if (json::write_json(payload, out, json_logger)) {
        escalate(escalation_action::fatal);
        return;
    }

    #ifdef DISCUSY_LOGGING
    log("Sent ready for transition: {}", out);
    #endif
    ws_.send(std::move(out));
}

void connection::prepare_dave_transition(const discord::dave::TransitionId transition_id, const discord::dave::ProtocolVersion protocol_version) {
    latest_prepared_dave_version_ = protocol_version;

    if (protocol_version == discord::dave::kDisabledVersion) {
        update_all_decryptors_passthrough(true);
    } else {
        update_all_decryptors_passthrough(false);
        update_all_decryptors_key_ratchets();
    }

    if (transition_id == discord::dave::kInitTransitionId) {
        execute_dave_transition_version(protocol_version);
    } else {
        pending_transitions_[transition_id] = protocol_version;
    }
}

void connection::execute_dave_transition_version(const discord::dave::ProtocolVersion protocol_version) {
    dave_protocol_version_ = protocol_version;
    dave_protocol_version_atomic_.store(protocol_version, std::memory_order_release);

    if (protocol_version == discord::dave::kDisabledVersion) {
        dave_.session_->Reset();
        update_all_decryptors_passthrough(true);

        {
        const std::scoped_lock lock{encryptor_mutex_};
        dave_.encryptor_->SetKeyRatchet(nullptr);
        dave_.encryptor_->SetPassthroughMode(true);
        }

        if (auto* const udp = udp_client()) udp->kick_drain();
        maybe_signal_ready();
        return;
    }

    auto key_ratchet = dave_.session_->GetKeyRatchet(user_id_str_);
    update_all_decryptors_passthrough(false);
    update_all_decryptors_key_ratchets();

    const bool has_ratchet = (key_ratchet != nullptr);
    {
    const std::scoped_lock lock{encryptor_mutex_};
    dave_.encryptor_->SetPassthroughMode(false);
    if (has_ratchet) {
        dave_.encryptor_->SetKeyRatchet(std::move(key_ratchet));
    }
    }

    if (!has_ratchet) {
        #ifdef DISCUSY_LOGGING
        log("No key ratchet available yet, keeping the current one");
        #endif
        return;
    }

    if (auto* const udp = udp_client()) udp->kick_drain();
    maybe_signal_ready();
}

void connection::drop_dave_keys() {
    pending_transitions_.clear();
    dave_protocol_version_ = discord::dave::kDisabledVersion;
    latest_prepared_dave_version_ = discord::dave::kDisabledVersion;
    dave_protocol_version_atomic_.store(discord::dave::kDisabledVersion, std::memory_order_release);

    {
    const std::scoped_lock lock{encryptor_mutex_};
    encryptor_ssrc_ = 0;
    dave_.encryptor_->SetKeyRatchet(nullptr);
    dave_.encryptor_->SetPassthroughMode(false);
    }

    {
    const std::scoped_lock lock{receive_mutex_};
    user_decryptors_.clear();
    ssrc_to_user_.clear();
    user_to_ssrc_.clear();
    }
    {
    const std::scoped_lock lock{decoder_mutex_};
    opus_decoders_.clear();
    }
}

void connection::ensure_user_decryptor(const snowflake uid) {
    if (!uid) return;

    {
    const std::scoped_lock lock{receive_mutex_};
    if (user_decryptors_.contains(uid)) return;
    }

    const auto dec = std::shared_ptr<discord::dave::IDecryptor>(discord::dave::CreateDecryptor());
    if (!dec) return;

    const bool passthrough = session_description_received_ && (latest_prepared_dave_version_ == discord::dave::kDisabledVersion);
    dec->TransitionToPassthroughMode(passthrough);
    if (latest_prepared_dave_version_ > discord::dave::kDisabledVersion) {
        if (auto kr = dave_.session_->GetKeyRatchet(uid.str().str())) {
            dec->TransitionToKeyRatchet(std::move(kr));
        }
    }

    const std::scoped_lock lock{receive_mutex_};
    user_decryptors_.try_emplace(uid, dec);
}

void connection::update_all_decryptors_key_ratchets() {
    std::vector<std::pair<snowflake, std::shared_ptr<discord::dave::IDecryptor>>> snapshot;
    {
    const std::scoped_lock lock{receive_mutex_};
    snapshot.reserve(user_decryptors_.size());
    for (const auto& [uid, dec] : user_decryptors_) {
        snapshot.emplace_back(uid, dec);
    }
    }

    for (const auto& [uid, dec] : snapshot) {
        if (auto kr = dave_.session_->GetKeyRatchet(uid.str().str())) {
            dec->TransitionToKeyRatchet(std::move(kr));
        }
    }
}

void connection::update_all_decryptors_passthrough(const bool passthrough) {
    std::vector<std::shared_ptr<discord::dave::IDecryptor>> snapshot;
    {
    const std::scoped_lock lock{receive_mutex_};
    snapshot.reserve(user_decryptors_.size());
    for (const auto& [_, dec] : user_decryptors_) {
        snapshot.emplace_back(dec);
    }
    }

    for (const auto& dec : snapshot) {
        dec->TransitionToPassthroughMode(passthrough);
    }
}

bool connection::has_non_default_sink_wants_locked() const noexcept {
    // Differs from Discord's initial server default (`{"any": 100}`).
    return !receive_any_ || !user_wants_.empty() || !raw_ssrc_wants_.empty();
}

bool connection::should_sync_sink_wants_on_resume() const noexcept {
    const std::scoped_lock lock{receive_mutex_};
    return sink_wants_dirty_ || has_non_default_sink_wants_locked();
}

void connection::sync_sink_wants_on_ready() {
    std::unordered_map<std::string, std::uint8_t> wants;
    bool should_send = false;
    {
    const std::scoped_lock lock{receive_mutex_};
    sink_wants_dirty_ = false;
    should_send = has_non_default_sink_wants_locked();
    if (should_send) {
        wants = build_media_sink_wants_locked();
    }
    }
    if (should_send) {
        send_media_sink_wants_payload(wants);
    }
}

void connection::dispatch_media_sink_wants() {
    boost::asio::dispatch(strand_, [self = this->shared_from_this()]() {
        if (self->destructed_ || !self->ready_received_) return;
        self->send_media_sink_wants_current();
    });
}

std::unordered_map<std::string, std::uint8_t> connection::build_media_sink_wants_locked() const {
    std::unordered_map<std::string, std::uint8_t> wants;
    wants.emplace("any", receive_any_ ? std::uint8_t{100} : std::uint8_t{0});
    for (const auto& [uid, enabled] : user_wants_) {
        if (const auto it = user_to_ssrc_.find(uid); it != user_to_ssrc_.end()) {
            wants[std::to_string(it->second)] = enabled ? std::uint8_t{100} : std::uint8_t{0};
        }
    }
    for (const auto& [ssrc, val] : raw_ssrc_wants_) {
        wants[std::to_string(ssrc)] = val;
    }
    return wants;
}

void connection::send_media_sink_wants_current() {
    std::unordered_map<std::string, std::uint8_t> wants;
    {
    const std::scoped_lock lock{receive_mutex_};
    wants = build_media_sink_wants_locked();
    sink_wants_dirty_ = false;
    }
    send_media_sink_wants_payload(wants);
}

void connection::send_media_sink_wants_payload(std::unordered_map<std::string, std::uint8_t>& wants) {
    const media_sink_wants_payload payload{wants};

    std::string out;
    if (json::write_json(payload, out, json_logger)) {
        return;
    }

    #ifdef DISCUSY_LOGGING
    log("Sent MediaSinkWants: {}", out);
    #endif
    ws_.send(std::move(out));
}

void connection::send_speaking(std::optional<speaking_mode> mode_override) {
    if (!session_live_ || (ssrc_ == 0)) return;

    speaking_ s{
        .speaking = mode_override ? *mode_override : speaking_mode_,
        .ssrc = ssrc_,
    };
    speaking_payload payload{s};

    std::string out;
    if (json::write_json(payload, out, json_logger)) {
        escalate(escalation_action::fatal);
        return;
    }

    #ifdef DISCUSY_LOGGING
    log("Sent speaking: {}", out);
    #endif
    ws_.send(std::move(out));
}

bool connection::send_heartbeat() {
    if (!recieved_heartbeat_ack_) {
        stop();
        reconnect();
        return false;
    }

    heartbeat hb{
        .t = static_cast<integer>(rnd_.next() & 0x1FFFFFFFFFFFFFULL),
        .seq_ack = seq_,
    };
    const heartbeat_payload payload{hb};

    last_hb_nonce_ = hb.t;

    std::string out;
    static constexpr glz::opts opts{.skip_null_members = false};
    if (json::write_json<opts>(payload, out, json_logger)) {
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

void connection::start_heartbeat(const std::chrono::milliseconds interval) {
    stop_heartbeat();
    recieved_heartbeat_ack_ = true; // initial state

    const auto gen = ++hb_timer_gen_;
    hb_timer_.expires_after(interval);
    hb_timer_.async_wait([weak = this->weak_from_this(), interval, gen](this auto&& self, const asio::ec_t ec) -> void {
        if (ec) return;
        const auto s = weak.lock();
        if (!s || s->destructed_ || (s->hb_timer_gen_ != gen)) return;

        if (s->send_heartbeat()) {
            const auto now = std::chrono::steady_clock::now();
            auto next = s->hb_timer_.expiry() + interval;
            if (next <= now) next = now + interval;
            s->hb_timer_.expires_at(next);
            s->hb_timer_.async_wait(std::forward<decltype(self)>(self));
        }
    });
}

void connection::stop_heartbeat() {
    ++hb_timer_gen_;
    hb_timer_.cancel();
}

void connection::udp_on_error_cb::operator()(const boost::system::error_code& ec) const {
    auto self = s.lock();
    if (!self) return;

    boost::asio::post(self->strand_, [s = s, ec]() {
        if (auto self = s.lock()) self->handle_udp_failure(ec);
    });
}

void connection::udp_on_idle_cb::operator()(const audio_stop_reason reason) const {
    auto self = s.lock();
    if (!self || self->closed_.load(std::memory_order_acquire) || self->io_ctx_->stopped()) return;

    self->on_audio_stopped.fire(audio_stopped{
        .reason = reason,
        .guild_id = self->server_id_,
    });
}

encrypt_result connection::udp_dave_encrypt::operator()(
    const discusy::voice::store<discusy::voice::MAX_OPUS_FRAME_BYTES>& audio,
    discusy::voice::store<MAX_DAVE_BUFFER_SIZE>& output
) const {
    const auto ptr = s.lock();
    if (!ptr) {
        output.clear();
        return encrypt_result::drop;
    }

    auto& self = *ptr;

    discord::dave::IEncryptor::ResultCode result{};
    size_t bytes_written{0};

    {
    const std::scoped_lock lock{self.encryptor_mutex_};
    if (!self.dave_.encryptor_->HasKeyRatchet() && !self.dave_.encryptor_->IsPassthroughMode()) {
        output.clear();
        return encrypt_result::not_ready;
    }

    if (self.dave_.encryptor_->GetMaxCiphertextByteSize(discord::dave::MediaType::Audio, audio.span().size()) > output.raw_buf.size()) {
        output.clear();
        return encrypt_result::drop;
    }

    result = self.dave_.encryptor_->Encrypt(
        discord::dave::MediaType::Audio,
        self.encryptor_ssrc_,
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
}

void connection::handle_udp_packet(
    const std::uint32_t ssrc,
    const std::uint16_t sequence,
    const std::uint32_t timestamp,
    const std::span<const std::uint8_t> payload
) {
    try {
        handle_udp_packet_impl(ssrc, sequence, timestamp, payload);
    }
    #ifdef DISCUSY_LOGGING
    catch (const std::exception& e) {
        log("handle_udp_packet exception: {}", e.what());
    }
    #endif
    catch (...) {
        #ifdef DISCUSY_LOGGING
        log("handle_udp_packet unknown exception");
        #endif
    }
}

void connection::handle_udp_packet_impl(
    const std::uint32_t ssrc,
    const std::uint16_t sequence,
    const std::uint32_t timestamp,
    const std::span<const std::uint8_t> payload
) {
    if (closed_.load(std::memory_order_acquire)) return;

    const bool want_opus = on_opus_received.has_listeners();
    const bool want_pcm = on_audio_received.has_listeners();
    if (!want_opus && !want_pcm) return;

    snowflake speaker_id{};
    std::shared_ptr<discord::dave::IDecryptor> decryptor;

    {
    const std::scoped_lock lock{receive_mutex_};
    if (closed_.load(std::memory_order_acquire)) return;

    if (const auto it = ssrc_to_user_.find(ssrc); it != ssrc_to_user_.end()) {
        speaker_id = it->second;
    }

    bool accept = receive_any_;
    if (const auto ssrc_it = raw_ssrc_wants_.find(ssrc); ssrc_it != raw_ssrc_wants_.end()) {
        accept = (ssrc_it->second > 0);
    } else if (speaker_id) {
        if (const auto user_it = user_wants_.find(speaker_id); user_it != user_wants_.end()) {
            accept = user_it->second;
        }
    }
    if (!accept) return;

    if (speaker_id) {
        if (const auto dec_it = user_decryptors_.find(speaker_id); dec_it != user_decryptors_.end()) {
            decryptor = dec_it->second;
        }
    }
    }

    const bool dave_disabled = (dave_protocol_version_atomic_.load(std::memory_order_acquire) == 0);
    std::span<const std::uint8_t> opus_span{};
    store<MAX_DAVE_BUFFER_SIZE> stack_buf;
    std::vector<std::uint8_t> heap_buf;

    if (decryptor) {
        const std::size_t max_plain = decryptor->GetMaxPlaintextByteSize(discord::dave::MediaType::Audio, payload.size());
        std::uint8_t* out_ptr = stack_buf.raw_buf.data();
        std::size_t out_cap = store<MAX_DAVE_BUFFER_SIZE>::capacity();

        if (max_plain > out_cap) {
            heap_buf.resize(max_plain);
            out_ptr = heap_buf.data();
            out_cap = heap_buf.size();
        }

        std::size_t bytes_written{0};
        const auto res = decryptor->Decrypt(
            discord::dave::MediaType::Audio,
            discord::dave::MakeArrayView<const std::uint8_t>(payload.data(), payload.size()),
            discord::dave::MakeArrayView(out_ptr, out_cap),
            &bytes_written
        );
        if (res != discord::dave::IDecryptor::Success || bytes_written == 0) {
            stat_dave_decrypt_drops_.fetch_add(1, std::memory_order_relaxed);
            #ifdef DISCUSY_LOGGING
            log("DAVE decryption failed for user {} (ssrc {}): code {}", speaker_id.value, ssrc, static_cast<int>(res));
            #endif
            return;
        }
        opus_span = std::span<const std::uint8_t>{out_ptr, bytes_written};
    } else if (dave_disabled) {
        opus_span = payload;
    } else {
        stat_dave_decrypt_drops_.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    std::vector<std::int16_t> pcm;
    if (want_pcm) {
        const std::scoped_lock lock{decoder_mutex_};
        if (closed_.load(std::memory_order_acquire)) return;
        auto [it, _] = opus_decoders_.try_emplace(ssrc);
        pcm = it->second.decode(opus_span);
    }

    if (want_opus) {
        if (closed_.load(std::memory_order_acquire) || io_ctx_->stopped()) return;
        std::vector<std::uint8_t> opus;
        if (!heap_buf.empty()) {
            heap_buf.resize(opus_span.size());
            opus = std::move(heap_buf);
        } else {
            opus.assign(opus_span.begin(), opus_span.end());
        }

        on_opus_received.fire(received_opus{
            .guild_id = server_id_,
            .user_id = speaker_id,
            .ssrc = ssrc,
            .sequence = sequence,
            .timestamp = timestamp,
            .opus = std::move(opus),
        });
    }

    if (!pcm.empty() && !closed_.load(std::memory_order_acquire) && !io_ctx_->stopped()) {
        on_audio_received.fire(received_audio{
            .guild_id = server_id_,
            .user_id = speaker_id,
            .ssrc = ssrc,
            .sequence = sequence,
            .timestamp = timestamp,
            .pcm = std::move(pcm),
        });
    }
}

void connection::udp_on_receive_cb::operator()(
    const std::uint32_t ssrc,
    const std::uint16_t sequence,
    const std::uint32_t timestamp,
    const std::span<const std::uint8_t> payload
) const {
    if (const auto self = s.lock()) {
        self->handle_udp_packet(ssrc, sequence, timestamp, payload);
    }
}

void connection::check_and_apply_bitrate() noexcept {
    const auto target = target_bitrate_.load(std::memory_order_relaxed);
    if ((target != applied_bitrate_) && encoder_.set_bitrate(target)) {
        applied_bitrate_ = target;
    }
}

bool connection::is_aes256_gcm_supported() noexcept {
    static const bool supported = []() noexcept -> bool {
        if (sodium_init() < 0) {
            return false;
        }
        return crypto_aead_aes256gcm_is_available() != 0;
    }();
    return supported;
}

} // namespace discusy::voice
