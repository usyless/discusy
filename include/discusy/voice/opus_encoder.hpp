#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include <opus.h>

#include "internal.hpp"
#include <usylibpp/strings.hpp>

namespace discusy::voice {

class opus_encoder {
public:
    explicit opus_encoder(
        const std::uint32_t sampling_rate = DISCORD_SAMPLING_RATE,
        const std::uint32_t channels = DISCORD_CHANNELS,
        const int application = OPUS_APPLICATION_AUDIO,
        const std::uint32_t bitrate = DEFAULT_OPUS_BITRATE
    ) {
        int opus_err = OPUS_OK;
        enc_ = opus_encoder_create(
            static_cast<opus_int32>(sampling_rate),
            static_cast<int>(channels),
            application,
            &opus_err
        );

        if (opus_err != OPUS_OK || enc_ == nullptr) {
            throw std::runtime_error(ulp::str::concat_strings("Failed to create opus encoder: ", opus_strerror(opus_err)));
        }

        if (bitrate > 0) {
            set_bitrate(bitrate);
        }
    }

    ~opus_encoder() {
        destroy();
    }

    opus_encoder(opus_encoder&& other) noexcept : enc_{std::exchange(other.enc_, nullptr)} {}

    opus_encoder& operator=(opus_encoder&& other) noexcept {
        if (this != &other) {
            destroy();
            enc_ = std::exchange(other.enc_, nullptr);
        }
        return *this;
    }

    opus_encoder(const opus_encoder&) = delete;
    opus_encoder& operator=(const opus_encoder&) = delete;

    void destroy() noexcept {
        if (enc_ != nullptr) {
            opus_encoder_destroy(enc_);
            enc_ = nullptr;
        }
    }

    [[nodiscard]] bool is_valid() const noexcept { return enc_ != nullptr; }
    [[nodiscard]] explicit operator bool() const noexcept { return enc_ != nullptr; }
    [[nodiscard]] OpusEncoder* native_handle() const noexcept { return enc_; }

    bool reset_state() noexcept {
        if (enc_ == nullptr) return false;
        return opus_encoder_ctl(enc_, OPUS_RESET_STATE) == OPUS_OK;
    }

    bool set_bitrate(const std::uint32_t bitrate) noexcept {
        if (enc_ == nullptr) return false;
        return opus_encoder_ctl(enc_, OPUS_SET_BITRATE(bitrate)) == OPUS_OK;
    }

    bool set_inband_fec(const bool enabled) noexcept {
        if (enc_ == nullptr) return false;
        return opus_encoder_ctl(enc_, OPUS_SET_INBAND_FEC(enabled ? 1 : 0)) == OPUS_OK;
    }

    bool set_expected_packet_loss(const int percentage) noexcept {
        if (enc_ == nullptr) return false;
        return opus_encoder_ctl(enc_, OPUS_SET_PACKET_LOSS_PERC(std::clamp(percentage, 0, 100))) == OPUS_OK;
    }

    bool set_complexity(const int complexity) noexcept {
        if (enc_ == nullptr) return false;
        return opus_encoder_ctl(enc_, OPUS_SET_COMPLEXITY(std::clamp(complexity, 0, 10))) == OPUS_OK;
    }

    // OPUS_SIGNAL_MUSIC / OPUS_SIGNAL_VOICE / OPUS_AUTO
    bool set_signal(const int signal) noexcept {
        if (enc_ == nullptr) return false;
        return opus_encoder_ctl(enc_, OPUS_SET_SIGNAL(signal)) == OPUS_OK;
    }

    template <typename... Request>
    bool ctl(Request&&... request) noexcept {
        if (!enc_) return false;
        return opus_encoder_ctl(enc_, std::forward<Request>(request)...) == OPUS_OK;
    }

    int encode_frame(
        const std::int16_t* pcm,
        const int frame_size_per_channel,
        std::uint8_t* out_data,
        const opus_int32 max_data_bytes
    ) noexcept {
        if (enc_ == nullptr) return -1;
        return opus_encode(enc_, pcm, frame_size_per_channel, out_data, max_data_bytes);
    }

    bool encode_frame(
        const std::span<const std::int16_t> pcm,
        store<MAX_OPUS_FRAME_BYTES>& out
    ) noexcept {
        if (enc_ == nullptr) return false;
        static constexpr int SAMPLES_PER_FRAME = (DISCORD_SAMPLING_RATE / 1000) * DISCORD_FRAME_SIZE_MS;
        const int bytes = opus_encode(
            enc_,
            pcm.data(),
            SAMPLES_PER_FRAME,
            out.raw_buf.data(),
            MAX_OPUS_FRAME_BYTES
        );
        if (bytes > 0) {
            out.set_size(static_cast<size_t>(bytes));
            return true;
        }
        return false;
    }

    std::vector<store<MAX_OPUS_FRAME_BYTES>> encode(
        const std::span<const std::int16_t> pcm_data
    ) {
        std::vector<store<MAX_OPUS_FRAME_BYTES>> frames;
        if ((enc_ == nullptr) || pcm_data.empty()) return frames;

        static constexpr int SAMPLES_PER_FRAME = (DISCORD_SAMPLING_RATE / 1000) * DISCORD_FRAME_SIZE_MS;
        static constexpr int TOTAL_FRAME_SAMPLES = SAMPLES_PER_FRAME * DISCORD_CHANNELS;

        const auto total = pcm_data.size();
        const size_t num_frames = (total + TOTAL_FRAME_SAMPLES - 1) / TOTAL_FRAME_SAMPLES;
        frames.reserve(num_frames);

        std::array<std::int16_t, TOTAL_FRAME_SAMPLES> padded_pcm{};

        for (size_t pos = 0; pos < total; pos += TOTAL_FRAME_SAMPLES) {
            const size_t remaining_samples = total - pos;
            const std::int16_t* current_pcm_ptr = pcm_data.data() + pos;

            if (remaining_samples < TOTAL_FRAME_SAMPLES) {
                std::ranges::fill(padded_pcm, 0);
                std::copy_n(current_pcm_ptr, remaining_samples, padded_pcm.begin());
                current_pcm_ptr = padded_pcm.data();
            }

            store<MAX_OPUS_FRAME_BYTES>& opus_out = frames.emplace_back();
            const int bytes_encoded = opus_encode(
                enc_,
                current_pcm_ptr,
                SAMPLES_PER_FRAME,
                opus_out.raw_buf.data(),
                MAX_OPUS_FRAME_BYTES
            );

            if (bytes_encoded > 0) {
                opus_out.set_size(static_cast<size_t>(bytes_encoded));
            } else {
                frames.pop_back();
            }
        }
        return frames;
    }

    template <typename Range>
    requires (
        !detail::is_span_v<Range> &&
        std::ranges::contiguous_range<Range> &&
        std::ranges::sized_range<Range> &&
        std::is_same_v<std::remove_cvref_t<std::ranges::range_value_t<Range>>, std::int16_t>
    )
    std::vector<store<MAX_OPUS_FRAME_BYTES>> encode(const Range& pcm_data) {
        if constexpr (std::is_convertible_v<const Range&, std::span<const std::int16_t>>) {
            return encode(std::span<const std::int16_t>{pcm_data});
        } else {
            return encode(std::span<const std::int16_t>{std::ranges::data(pcm_data), std::ranges::size(pcm_data)});
        }
    }

    template <typename Range>
    requires (
        std::ranges::contiguous_range<Range> &&
        std::ranges::sized_range<Range> &&
        (sizeof(std::ranges::range_value_t<Range>) == 1)
    )
    std::vector<store<MAX_OPUS_FRAME_BYTES>> encode(const Range& raw_bytes) {
        const auto byte_count = std::ranges::size(raw_bytes);
        const auto sample_count = byte_count / sizeof(std::int16_t);
        if (sample_count == 0) return {};

        const auto* raw_ptr = reinterpret_cast<const char*>(std::ranges::data(raw_bytes));
        if (reinterpret_cast<std::uintptr_t>(raw_ptr) % alignof(std::int16_t) == 0) {
            return encode(std::span<const std::int16_t>{
                reinterpret_cast<const std::int16_t*>(raw_ptr),
                sample_count,
            });
        }

        std::vector<std::int16_t> aligned(sample_count);
        std::memcpy(aligned.data(), raw_ptr, sample_count * sizeof(std::int16_t));
        return encode(std::span<const std::int16_t>{aligned.data(), aligned.size()});
    }

private:
    OpusEncoder* enc_{nullptr};
};

}
