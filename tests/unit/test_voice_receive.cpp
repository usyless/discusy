#include <catch2/catch_test_macros.hpp>

#include <discusy/callback.hpp>
#include <discusy/io_context.hpp>
#include <discusy/json.hpp>
#include <discusy/opcode.hpp>
#include <discusy/api_constants.hpp>

#ifdef DISCUSY_VOICE
#include <discusy/state.hpp>
#include <discusy/voice/voice_connection.hpp>
#endif

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
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
        const std::string_view json_str = R"({"op":8,"d":{"heartbeat_interval":41250,"v":8}})";
        discusy::voice::recieve_payload<discusy::voice::hello> payload{};
        REQUIRE_FALSE(discusy::json::parse_view(payload, json_str));
        CHECK(payload.d.heartbeat_interval == 41250);
        REQUIRE(payload.d.v.has_value());
        CHECK(payload.d.v.value_or(0) == 8);
    }

    SECTION("Incoming Speaking payload") {
        const std::string_view json_str = R"({"op":5,"d":{"user_id":"104694319306248192","ssrc":2,"speaking":1}})";
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
        const discusy::voice::media_sink_wants_payload payload{wants};
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

    SECTION("Decoder edge cases and move semantics") {
        std::array<std::int16_t, 8> tiny_pcm{};
        CHECK(dec.decode_frame(encoded.span(), tiny_pcm) == OPUS_BUFFER_TOO_SMALL);

        discusy::voice::opus_decoder moved_dec{std::move(dec)};
        CHECK(moved_dec.is_valid());
        CHECK_FALSE(dec.is_valid());
        CHECK(dec.decode(encoded.span()).empty());
        CHECK(moved_dec.decode(encoded.span()).size() == total_samples);

        moved_dec.destroy();
        CHECK_FALSE(moved_dec.is_valid());
        CHECK(moved_dec.decode(encoded.span()).empty());
    }
}

TEST_CASE("Voice: connection receive filters are thread-safe", "[voice][receive][threads]") {
    discusy::ctx::io_context io_ctx{2};
    discusy::state st{
        .client{st.ssl_ctx, io_ctx},
        .timers{io_ctx},
        .gateway_callbacks{io_ctx},
        .shards_ready{io_ctx},
        .shard_session_invalidated{io_ctx},
    };

    auto conn = std::make_shared<discusy::voice::connection>(
        io_ctx,
        st,
        discusy::snowflake{1},
        discusy::snowflake{2},
        discusy::snowflake{3},
        std::string{"session"},
        std::string{"token"},
        std::string{"wss://127.0.0.1:443/?v=8"},
        [](discusy::voice::escalation_action) {}
    );

    CHECK_FALSE(conn->is_receiving_any_audio());
    CHECK_FALSE(conn->get_user_for_ssrc(42).has_value());
    CHECK_FALSE(conn->get_ssrc_for_user(discusy::snowflake{100}).has_value());

    std::vector<std::thread> workers;
    workers.reserve(4);
    for (int t = 0; t < 4; ++t) {
        workers.emplace_back([&conn, t]() {
            for (int i = 0; i < 200; ++i) {
                const auto uid = discusy::snowflake{static_cast<std::uint64_t>(1000 + t * 10 + (i % 5))};
                conn->set_receive_any_audio((i % 2) == 0);
                conn->set_receive_user_audio(uid, (i % 3) == 0);
                conn->send_media_sink_wants({{"any", static_cast<std::uint8_t>((i % 2 == 0) ? 100 : 0)}, {"1234", 100}});
                (void)conn->is_receiving_any_audio();
                (void)conn->get_user_for_ssrc(1234);
                (void)conn->get_ssrc_for_user(uid);
                conn->clear_user_audio_filters();
            }
        });
    }

    for (auto& w : workers) {
        w.join();
    }

    conn->enable_all_audio_receive();
    CHECK(conn->is_receiving_any_audio());
    conn->disable_audio_receive();
    CHECK_FALSE(conn->is_receiving_any_audio());

    io_ctx->poll();
    conn.reset();
}

#endif
