#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <span>
#include <stdexcept>

#include <opus.h>

namespace discusy::voice {

enum class escalation_action : std::uint8_t {
    requires_gateway_reconnect,
    fatal,
};

enum class encrypt_result : std::uint8_t {
    ok,        // encrypted, send it
    drop,      // unrecoverable for this frame, skip it
    not_ready,  // keep it queued, we cannot encrypt yet (no DAVE key ratchet)
};

enum class audio_stop_reason : std::uint8_t {
    paused,   // pause_audio() took effect, the queue is intact
    stopped,  // stop_audio() took effect, the queue was discarded
    finished,  // everything queued has been transmitted
};

inline constexpr std::size_t MAX_OPUS_BITRATE = 384000;

inline constexpr std::size_t DEFAULT_OPUS_BITRATE = 64000;

// TODO: make these more realistic
inline constexpr opus_int32 MAX_OPUS_FRAME_BYTES = 1275;
inline constexpr std::size_t MAX_DAVE_BUFFER_SIZE = 1500;
inline constexpr std::size_t MAX_FINAL_BUFFER_SIZE = 1750;


inline constexpr std::size_t DISCORD_SAMPLING_RATE = 48000;
inline constexpr std::size_t DISCORD_CHANNELS = 2;
inline constexpr std::size_t DISCORD_FRAME_SIZE_MS = 20;

inline constexpr std::size_t TIMESTAMP_INCREMENT = (DISCORD_SAMPLING_RATE / 1000) * DISCORD_FRAME_SIZE_MS;

inline constexpr auto DISCORD_FRAME_DURATION = std::chrono::milliseconds{DISCORD_FRAME_SIZE_MS};

inline constexpr std::uint32_t DEFAULT_UNDERRUN_GRACE_FRAMES = 5;

inline constexpr std::uint32_t DEFAULT_MAX_CATCHUP_FRAMES = 3;

inline constexpr std::uint32_t DEFAULT_NOT_READY_HOLD_FRAMES = 50;

struct audio_stats {
    std::uint64_t frames_sent{};        // RTP packets that made it to the socket
    std::uint64_t silence_frames_sent{};// of which were generated silence
    std::uint64_t underrun_events{};    // times the queue ran dry *mid-stream*
    std::uint64_t underrun_frames{};    // silence frames those underruns cost
    std::uint64_t clock_resyncs{};      // times pacing fell far enough behind to skip
    std::uint64_t frames_skipped{};     // media time skipped by those resyncs
    std::uint64_t send_errors{};        // sendto() failures
    std::uint64_t would_block_drops{};  // frames dropped, socket send buffer full
    std::uint64_t encrypt_drops{};      // frames dropped, encryption failed
    std::uint64_t not_ready_stalls{};   // ticks spent holding for a DAVE key ratchet
    std::uint64_t no_key_drops{};       // frames dropped after the hold expired, still no key
    std::int64_t  worst_lateness_us{};  // worst observed pacing lateness
};

static constexpr std::array<std::uint8_t, 3> opus_silence = {0xF8, 0xFF, 0xFE};

template <std::size_t Capacity>
struct store {
    std::array<std::uint8_t, Capacity> raw_buf{};
    std::size_t size_{0};

    constexpr store() noexcept = default;

    constexpr store(std::span<const std::uint8_t> input_data) {
        assign(input_data);
    }

    constexpr void assign(std::span<const std::uint8_t> input_data) {
        if (input_data.size() > Capacity) {
            throw std::length_error("discusy::voice::store: input exceeds capacity");
        }
        std::copy(input_data.begin(), input_data.end(), raw_buf.begin());
        size_ = input_data.size();
    }

    constexpr void set_size(std::size_t new_size) noexcept {
        size_ = std::min(new_size, Capacity);
    }

    constexpr void clear() noexcept {
        size_ = 0;
    }

    [[nodiscard]] constexpr std::span<std::uint8_t> span() noexcept {
        return std::span<std::uint8_t>{raw_buf.data(), size_};
    }

    [[nodiscard]] constexpr std::span<const std::uint8_t> span() const noexcept {
        return std::span<const std::uint8_t>{raw_buf.data(), size_};
    }

    constexpr operator std::span<std::uint8_t>() noexcept {
        return span();
    }

    constexpr operator std::span<const std::uint8_t>() const noexcept {
        return span();
    }

    [[nodiscard]] constexpr std::uint8_t* data() noexcept {
        return raw_buf.data();
    }

    [[nodiscard]] constexpr const std::uint8_t* data() const noexcept {
        return raw_buf.data();
    }

    [[nodiscard]] constexpr std::size_t size() const noexcept {
        return size_;
    }

    [[nodiscard]] static constexpr std::size_t capacity() noexcept {
        return Capacity;
    }

    [[nodiscard]] constexpr bool empty() const noexcept {
        return size_ == 0;
    }
};

}
