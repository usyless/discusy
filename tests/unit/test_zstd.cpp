#include <catch2/catch_test_macros.hpp>
#include <discusy/zstd.hpp>
#include <zstd.h>
#include <string>
#include <string_view>
#include <vector>

TEST_CASE("Zstd: Stream decompressor handles compressed chunks and reset", "[zstd]") {
    discusy::zstd::stream_decompressor decompressor;

    const std::string_view original = R"({"t":"MESSAGE_CREATE","s":1,"op":0,"d":{"content":"hello test message"}})";

    // Compress using standard zstd API
    std::vector<char> compressed_buf(ZSTD_compressBound(original.size()));
    size_t compressed_size = ZSTD_compress(
        compressed_buf.data(), compressed_buf.size(),
        original.data(), original.size(),
        1 // compression level
    );
    REQUIRE_FALSE(ZSTD_isError(compressed_size));
    compressed_buf.resize(compressed_size);

    std::string_view chunk{compressed_buf.data(), compressed_size};

    SECTION("Decompress whole payload in single push") {
        std::string decompressed = decompressor.push(chunk);
        CHECK(decompressed == original);
    }

    SECTION("Stream reset allows reusing decompressor") {
        std::string first = decompressor.push(chunk);
        CHECK(first == original);

        decompressor.reset();

        std::string second = decompressor.push(chunk);
        CHECK(second == original);
    }
}
