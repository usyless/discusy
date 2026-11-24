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
    ) : sampling_rate_{sampling_rate}, channels_{channels} {
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

    opus_encoder(opus_encoder&& other) noexcept
        : enc_{std::exchange(other.enc_, nullptr)}, sampling_rate_{other.sampling_rate_}, channels_{other.channels_} {}

    opus_encoder& operator=(opus_encoder&& other) noexcept {
        if (this != &other) {
            destroy();
            enc_ = std::exchange(other.enc_, nullptr);
            sampling_rate_ = other.sampling_rate_;
            channels_ = other.channels_;
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
        if (enc_ == nullptr || channels_ == 0 || sampling_rate_ < 1000U) return false;
        const std::size_t samples_per_frame = (static_cast<std::size_t>(sampling_rate_) / 1000U) * DISCORD_FRAME_SIZE_MS;
        const std::size_t total_frame_samples = samples_per_frame * channels_;
        if (samples_per_frame == 0 || pcm.size() < total_frame_samples) return false;

        const int bytes = opus_encode(
            enc_,
            pcm.data(),
            static_cast<int>(samples_per_frame),
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
        if ((enc_ == nullptr) || pcm_data.empty() || channels_ == 0 || sampling_rate_ < 1000U) return frames;

        const std::size_t samples_per_frame = (static_cast<std::size_t>(sampling_rate_) / 1000U) * DISCORD_FRAME_SIZE_MS;
        const std::size_t total_frame_samples = samples_per_frame * channels_;
        if (total_frame_samples == 0) return frames;

        const auto total = pcm_data.size();
        const size_t num_frames = (total + total_frame_samples - 1) / total_frame_samples;
        frames.reserve(num_frames);

        std::vector<std::int16_t> padded_pcm;

        for (size_t pos = 0; pos < total; pos += total_frame_samples) {
            const size_t remaining_samples = total - pos;
            const std::int16_t* current_pcm_ptr = pcm_data.data() + pos;

            if (remaining_samples < total_frame_samples) {
                padded_pcm.assign(total_frame_samples, 0);
                std::copy_n(current_pcm_ptr, remaining_samples, padded_pcm.begin());
                current_pcm_ptr = padded_pcm.data();
            }

            store<MAX_OPUS_FRAME_BYTES>& opus_out = frames.emplace_back();
            const int bytes_encoded = opus_encode(
                enc_,
                current_pcm_ptr,
                static_cast<int>(samples_per_frame),
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
        std::vector<store<MAX_OPUS_FRAME_BYTES>> frames;
        if ((enc_ == nullptr) || channels_ == 0 || sampling_rate_ < 1000U) return frames;

        const auto byte_count = std::ranges::size(raw_bytes);
        const auto sample_count = byte_count / sizeof(std::int16_t);
        if (sample_count == 0) return frames;

        const std::size_t samples_per_frame = (static_cast<std::size_t>(sampling_rate_) / 1000U) * DISCORD_FRAME_SIZE_MS;
        const std::size_t total_frame_samples = samples_per_frame * channels_;
        if (total_frame_samples == 0) return frames;

        const auto* raw_ptr = reinterpret_cast<const std::uint8_t*>(std::ranges::data(raw_bytes));
        const std::size_t num_frames = (sample_count + total_frame_samples - 1) / total_frame_samples;
        frames.reserve(num_frames);

        std::vector<std::int16_t> frame_pcm(total_frame_samples);
        for (std::size_t pos = 0; pos < sample_count; pos += total_frame_samples) {
            const std::size_t chunk_samples = std::min(total_frame_samples, sample_count - pos);
            std::memcpy(frame_pcm.data(), raw_ptr + (pos * sizeof(std::int16_t)), chunk_samples * sizeof(std::int16_t));
            if (chunk_samples < total_frame_samples) {
                std::fill(frame_pcm.begin() + static_cast<std::ptrdiff_t>(chunk_samples), frame_pcm.end(), std::int16_t{0});
            }

            store<MAX_OPUS_FRAME_BYTES>& opus_out = frames.emplace_back();
            const int bytes_encoded = opus_encode(
                enc_,
                frame_pcm.data(),
                static_cast<int>(samples_per_frame),
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

private:
    OpusEncoder* enc_{nullptr};
    std::uint32_t sampling_rate_{DISCORD_SAMPLING_RATE};
    std::uint32_t channels_{DISCORD_CHANNELS};
};

}
