#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include <opus.h>

#include "internal.hpp"
#include <usylibpp/strings.hpp>

namespace discusy::voice {

/// RAII wrapper around an OpusDecoder for decoding incoming 48kHz stereo Discord voice frames.
class opus_decoder {
public:
    explicit opus_decoder(
        const std::uint32_t sampling_rate = DISCORD_SAMPLING_RATE,
        const std::uint32_t channels = DISCORD_CHANNELS
    ) : channels_{channels} {
        int opus_err = OPUS_OK;
        dec_ = opus_decoder_create(
            static_cast<opus_int32>(sampling_rate),
            static_cast<int>(channels),
            &opus_err
        );

        if (opus_err != OPUS_OK || dec_ == nullptr) {
            throw std::runtime_error(ulp::str::concat_strings("Failed to create opus decoder: ", opus_strerror(opus_err)));
        }
    }

    ~opus_decoder() {
        destroy();
    }

    opus_decoder(opus_decoder&& other) noexcept
        : dec_{std::exchange(other.dec_, nullptr)}, channels_{other.channels_} {}

    opus_decoder& operator=(opus_decoder&& other) noexcept {
        if (this != &other) {
            destroy();
            dec_ = std::exchange(other.dec_, nullptr);
            channels_ = other.channels_;
        }
        return *this;
    }

    opus_decoder(const opus_decoder&) = delete;
    opus_decoder& operator=(const opus_decoder&) = delete;

    void destroy() noexcept {
        if (dec_ != nullptr) {
            opus_decoder_destroy(dec_);
            dec_ = nullptr;
        }
    }

    [[nodiscard]] bool is_valid() const noexcept { return dec_ != nullptr; }
    [[nodiscard]] explicit operator bool() const noexcept { return dec_ != nullptr; }
    [[nodiscard]] OpusDecoder* native_handle() const noexcept { return dec_; }

    bool reset_state() noexcept {
        if (dec_ == nullptr) return false;
        return opus_decoder_ctl(dec_, OPUS_RESET_STATE) == OPUS_OK;
    }

    template <typename... Request>
    bool ctl(Request&&... request) noexcept {
        if (!dec_) return false;
        return opus_decoder_ctl(dec_, std::forward<Request>(request)...) == OPUS_OK;
    }

    /// Decodes a single Opus frame into `pcm_out`, returning decoded samples per channel or a negative Opus error code.
    int decode_frame(
        const std::span<const std::uint8_t> opus_frame,
        const std::span<std::int16_t> pcm_out,
        const bool decode_fec = false
    ) noexcept {
        if (dec_ == nullptr || channels_ == 0) return OPUS_INVALID_STATE;
        if (opus_frame.size() > static_cast<std::size_t>(std::numeric_limits<opus_int32>::max())) return OPUS_BAD_ARG;

        const auto clamped_samples = std::min<std::size_t>(
            pcm_out.size() / channels_,
            static_cast<std::size_t>(std::numeric_limits<int>::max())
        );
        const int max_samples_per_channel = static_cast<int>(clamped_samples);
        if (max_samples_per_channel <= 0) return OPUS_BUFFER_TOO_SMALL;

        return opus_decode(
            dec_,
            opus_frame.empty() ? nullptr : opus_frame.data(),
            static_cast<opus_int32>(opus_frame.size()),
            pcm_out.data(),
            max_samples_per_channel,
            decode_fec ? 1 : 0
        );
    }

    /// Decodes an Opus frame into an owned interleaved PCM vector (`channels_` * samples_per_channel).
    [[nodiscard]] std::vector<std::int16_t> decode(
        const std::span<const std::uint8_t> opus_frame,
        const bool decode_fec = false
    ) {
        if (dec_ == nullptr || channels_ == 0) return {};

        std::vector<std::int16_t> pcm(MAX_OPUS_DECODED_SAMPLES_PER_CHANNEL * channels_);
        const int samples_per_channel = decode_frame(opus_frame, pcm, decode_fec);
        if (samples_per_channel <= 0) {
            return {};
        }

        pcm.resize(static_cast<std::size_t>(samples_per_channel) * channels_);
        return pcm;
    }

private:
    OpusDecoder* dec_{nullptr};
    std::uint32_t channels_{DISCORD_CHANNELS};
};

}
