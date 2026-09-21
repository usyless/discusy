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

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
class stream_decompressor {
public:
    stream_decompressor() : dctx_(ZSTD_createDCtx(), ZSTD_freeDCtx) {
        if (!dctx_) throw std::runtime_error("ZSTD_createDCtx failed");
        ptr_ = dctx_.get();
    }

    constexpr ~stream_decompressor() noexcept = default;

    stream_decompressor(const stream_decompressor&) = delete;
    stream_decompressor& operator=(const stream_decompressor&) = delete;

    [[nodiscard]] std::string push(const std::string_view chunk) {
        ZSTD_inBuffer input{ .src=chunk.data(), .size=chunk.size(), .pos=0 };
        std::string decompressed_payload;
        
        decompressed_payload.reserve(chunk.size() * 4);

        do {
            std::array<std::uint8_t, 64UZ * 1024> tmp; // NOLINT(cppcoreguidelines-pro-type-member-init)
            ZSTD_outBuffer output{ .dst=tmp.data(), .size=tmp.size(), .pos=0 };

            size_t remaining_hint = ZSTD_decompressStream(ptr_, &output, &input);

            if (ZSTD_isError(remaining_hint) != 0U) {
                throw std::runtime_error(ulp::str::concat_strings("ZSTD_decompressStream failed: ", ZSTD_getErrorName(remaining_hint)));
            }

            if (output.pos > 0) {
                decompressed_payload.append(reinterpret_cast<const char*>(tmp.data()), output.pos);
            }
            
            if (input.pos == input.size && output.pos < sizeof(tmp)) {
                break;
            }
        } while (true);

        return decompressed_payload;
    }

    void reset() noexcept {
        ZSTD_DCtx_reset(ptr_, ZSTD_reset_session_only);
    }

private:
    std::unique_ptr<ZSTD_DCtx, decltype(&ZSTD_freeDCtx)> dctx_;
    ZSTD_DCtx* ptr_;
};

}