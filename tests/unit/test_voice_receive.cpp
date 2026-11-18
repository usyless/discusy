#include <catch2/catch_test_macros.hpp>

#include <discusy/callback.hpp>
#include <discusy/io_context.hpp>
#include <discusy/json.hpp>
#include <discusy/opcode.hpp>
#include <discusy/api_constants.hpp>

#ifdef DISCUSY_VOICE
#include <discusy/voice/voice_connection.hpp>
#endif

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

TEST_CASE("Voice: Callback::has_listeners tracks registration", "[voice][callback]") {
    discusy::ctx::io_context ctx{1};
    discusy::Callback<int> cb{ctx};

    CHECK_FALSE(cb.has_listeners());
    const auto id = cb([](const int&) {});
    CHECK(cb.has_listeners());
    CHECK(cb.unregister(id));
    CHECK_FALSE(cb.has_listeners());
}

TEST_CASE("Constants: Lobby limits match Discord docs", "[constants][lobby]") {
    CHECK(discusy::constants::lobby::IDLE_TIMEOUT_SECONDS_MIN == 5);
    CHECK(discusy::constants::lobby::IDLE_TIMEOUT_SECONDS_MAX == 604800);
    CHECK(discusy::constants::lobby::MEMBERS_MAX == 1000);
    CHECK(discusy::constants::lobby::MEMBERS_PER_REQUEST_MAX == 25);
}

#ifdef DISCUSY_VOICE

TEST_CASE("Voice: Hello, Speaking, and MediaSinkWants payload serialization", "[voice][json]") {
    SECTION("Hello payload with v field") {
        std::string json_str = R"({"op":8,"d":{"heartbeat_interval":41250,"v":8}})";
        discusy::voice::recieve_payload<discusy::voice::hello> payload{};
        REQUIRE_FALSE(discusy::json::parse_view(payload, json_str));
        CHECK(payload.d.heartbeat_interval == 41250);
        REQUIRE(payload.d.v.has_value());
        CHECK(*payload.d.v == 8);
    }

    SECTION("Incoming Speaking payload") {
        std::string json_str = R"({"op":5,"d":{"user_id":"104694319306248192","ssrc":2,"speaking":1}})";
        discusy::voice::recieve_payload<discusy::voice::speaking_recv> payload{};
        REQUIRE_FALSE(discusy::json::parse_view(payload, json_str));
        CHECK(payload.d.user_id == discusy::snowflake{104694319306248192ULL});
        CHECK(payload.d.ssrc == 2);
        CHECK(payload.d.speaking == discusy::voice::speaking_mode::Microphone);
    }

    SECTION("Outgoing MediaSinkWants payload") {
        std::unordered_map<std::string, std::uint8_t> wants{
            {"any", 0},
            {"2", 100},
        };
        discusy::voice::media_sink_wants_payload payload{wants};
        std::string out;
        REQUIRE_FALSE(discusy::json::write(payload, out));
        CHECK(out.contains("\"op\":15"));
        CHECK(out.contains("\"any\":0"));
        CHECK(out.contains("\"2\":100"));
    }
}

TEST_CASE("Voice: Opus encoder and decoder roundtrip", "[voice][opus]") {
    discusy::voice::opus_encoder enc;
    discusy::voice::opus_decoder dec;

    REQUIRE(enc.is_valid());
    REQUIRE(dec.is_valid());

    constexpr std::size_t samples_per_channel = (discusy::voice::DISCORD_SAMPLING_RATE / 1000) * discusy::voice::DISCORD_FRAME_SIZE_MS;
    constexpr std::size_t total_samples = samples_per_channel * discusy::voice::DISCORD_CHANNELS;
    std::vector<std::int16_t> pcm_in(total_samples, 1200);

    discusy::voice::store<discusy::voice::MAX_OPUS_FRAME_BYTES> encoded;
    REQUIRE(enc.encode_frame(pcm_in, encoded));
    REQUIRE_FALSE(encoded.empty());

    const auto pcm_out = dec.decode(encoded.span());
    CHECK(pcm_out.size() == total_samples);
}

#endif
