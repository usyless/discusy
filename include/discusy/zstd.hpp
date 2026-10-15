#pragma once

#include <string_view>
#include <stdexcept>
#include <string>
#include <memory>
#include <array>
#include <cstdint>
#include <zstd.h>

#include <usylibpp/strings.hpp>

namespace discusy::zstd {

class stream_decompressor {
public:
    static constexpr std::size_t buffer_size = 64UZ * 1024;
    
    stream_decompressor() // NOLINT(cppcoreguidelines-pro-type-member-init)
        : dctx_(ZSTD_createDCtx(), ZSTD_freeDCtx),
        output_{ .dst = tmp_.data(), .size = tmp_.size(), .pos = 0 } {
        if (!dctx_) throw std::runtime_error("ZSTD_createDCtx failed");
        ptr_ = dctx_.get();
    }

    constexpr ~stream_decompressor() noexcept = default;

    stream_decompressor(const stream_decompressor&) = delete;
    stream_decompressor& operator=(const stream_decompressor&) = delete;

    stream_decompressor(stream_decompressor&&) = delete;
    stream_decompressor& operator=(stream_decompressor&&) = delete;

    bool push(const std::string_view chunk, std::string& out) {
        out.clear();
        if (out.capacity() < chunk.size() * 4) {
            out.reserve(chunk.size() * 4);
        }

        ZSTD_inBuffer input{ .src=chunk.data(), .size=chunk.size(), .pos=0 };

        do {
            output_.pos = 0;

            size_t remaining_hint = ZSTD_decompressStream(ptr_, &output_, &input);

            if (ZSTD_isError(remaining_hint) != 0U) {
                return false;
            }

            if (output_.pos > 0) {
                out.append(reinterpret_cast<const char*>(tmp_.data()), output_.pos);
            }
            
            if (input.pos == input.size && output_.pos < tmp_.size()) {
                break;
            }
        } while (true);

        return true;
    }

    [[nodiscard]] std::string push(const std::string_view chunk) {
        std::string decompressed_payload;
        push(chunk, decompressed_payload);
        return decompressed_payload;
    }

    void reset() noexcept {
        ZSTD_DCtx_reset(ptr_, ZSTD_reset_session_only);
    }

private:
    std::unique_ptr<ZSTD_DCtx, decltype(&ZSTD_freeDCtx)> dctx_;
    ZSTD_DCtx* ptr_;
    std::array<std::uint8_t, buffer_size> tmp_;
    ZSTD_outBuffer output_;
};

}