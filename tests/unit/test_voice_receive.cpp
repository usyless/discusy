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
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
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

    SECTION("Decoder edge cases, exact sizing, and move semantics") {
        CHECK(pcm_out.capacity() == total_samples);

        const auto plc_pcm = dec.decode(std::span<const std::uint8_t>{});
        CHECK(plc_pcm.size() == total_samples);

        std::array<std::int16_t, 8> tiny_pcm{};
        CHECK(dec.decode_frame(encoded.span(), tiny_pcm) == OPUS_BUFFER_TOO_SMALL);

        discusy::voice::opus_decoder moved_dec{std::move(dec)};
        CHECK(moved_dec.is_valid());
        // NOLINTNEXTLINE(bugprone-use-after-move)
        CHECK_FALSE(dec.is_valid());
        // NOLINTNEXTLINE(bugprone-use-after-move)
        CHECK(dec.decode(encoded.span()).empty());
        CHECK(moved_dec.decode(encoded.span()).size() == total_samples);

        moved_dec.destroy();
        CHECK_FALSE(moved_dec.is_valid());
        CHECK(moved_dec.decode(encoded.span()).empty());
    }

    SECTION("Encoder bounds check and misaligned byte range") {
        std::array<std::int16_t, 16> short_pcm{};
        discusy::voice::store<discusy::voice::MAX_OPUS_FRAME_BYTES> out_short;
        CHECK_FALSE(enc.encode_frame(short_pcm, out_short));

        std::vector<std::uint8_t> unaligned_bytes(1 + (total_samples * sizeof(std::int16_t)), 0);
        std::memcpy(unaligned_bytes.data() + 1, pcm_in.data(), total_samples * sizeof(std::int16_t));
        const std::span<const std::uint8_t> misaligned_span{unaligned_bytes.data() + 1, total_samples * sizeof(std::int16_t)};

        const auto frames = enc.encode(misaligned_span);
        REQUIRE(frames.size() == 1);
        CHECK_FALSE(frames[0].empty());

        discusy::voice::opus_decoder dec2;
        const auto decoded = dec2.decode(frames[0].span());
        CHECK(decoded.size() == total_samples);
    }
}

TEST_CASE("Voice: DAVE session init generates key package", "[voice][dave]") {
    discusy::dave d;
    REQUIRE(d.session_ != nullptr);
    REQUIRE(d.encryptor_ != nullptr);

    d.init_session(discord::dave::MaxSupportedProtocolVersion(), 123456789ULL, "987654321");
    const auto key_pkg = d.session_->GetMarshalledKeyPackage();
    CHECK_FALSE(key_pkg.empty());
}

TEST_CASE("Voice: UDP client RTP receive decrypts extensions, padding, and filters RTCP/non-Opus", "[voice][udp][receive]") {
    REQUIRE(sodium_init() >= 0);

    discusy::ctx::io_context io_ctx{1};
    boost::asio::ip::udp::socket server_sock{io_ctx.io_ctx_, boost::asio::ip::udp::endpoint{boost::asio::ip::make_address_v4("127.0.0.1"), 0}};
    const std::uint16_t server_port = server_sock.local_endpoint().port();

    std::atomic<bool> discovery_done{false};
    std::atomic<std::size_t> received_count{0};
    std::mutex recv_mtx;
    std::uint32_t got_ssrc{0};
    std::uint16_t got_seq{0};
    std::uint32_t got_ts{0};
    std::vector<std::uint8_t> got_payload;

    const auto udp = std::make_shared<discusy::udp::client<
        std::function<void(boost::system::error_code)>,
        std::function<discusy::voice::encrypt_result(const discusy::voice::store<discusy::voice::MAX_OPUS_FRAME_BYTES>&, discusy::voice::store<discusy::voice::MAX_DAVE_BUFFER_SIZE>&)>,
        std::function<void(discusy::voice::audio_stop_reason)>,
        std::function<void(std::uint32_t, std::uint16_t, std::uint32_t, std::span<const std::uint8_t>)>
    >>(
        io_ctx,
        [](boost::system::error_code) {},
        [](const auto& in, auto& out) {
            out.assign(in.span());
            return discusy::voice::encrypt_result::ok;
        },
        [](discusy::voice::audio_stop_reason) {},
        [&](std::uint32_t ssrc, std::uint16_t seq, std::uint32_t ts, std::span<const std::uint8_t> payload) {
            const std::scoped_lock lock{recv_mtx};
            got_ssrc = ssrc;
            got_seq = seq;
            got_ts = ts;
            got_payload.assign(payload.begin(), payload.end());
            received_count.fetch_add(1, std::memory_order_release);
        }
    );

    udp->send_ip_discovery("127.0.0.1", server_port, 0x11223344U, [&](const std::string& ip, std::uint16_t port) {
        if (!ip.empty() && port != 0) {
            discovery_done.store(true, std::memory_order_release);
        }
    });

    std::array<std::uint8_t, 74> disc_req{};
    boost::asio::ip::udp::endpoint client_ep;
    server_sock.receive_from(boost::asio::buffer(disc_req), client_ep);
    REQUIRE(disc_req[0] == 0x00);
    REQUIRE(disc_req[1] == 0x01);

    std::array<std::uint8_t, 74> disc_resp{};
    disc_resp[0] = 0x00;
    disc_resp[1] = 0x02;
    disc_resp[2] = 0x00;
    disc_resp[3] = 0x46;
    std::memcpy(&disc_resp[4], &disc_req[4], 4);
    const std::string ip_str = client_ep.address().to_string();
    std::memcpy(&disc_resp[8], ip_str.data(), std::min<std::size_t>(ip_str.size(), 63));
    const std::uint16_t cport = client_ep.port();
    disc_resp[72] = static_cast<std::uint8_t>((static_cast<unsigned>(cport) >> 8U) & 0xFFU);
    disc_resp[73] = static_cast<std::uint8_t>(static_cast<unsigned>(cport) & 0xFFU);
    server_sock.send_to(boost::asio::buffer(disc_resp), client_ep);

    for (int i = 0; i < 200 && !discovery_done.load(std::memory_order_acquire); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds{5});
    }
    REQUIRE(discovery_done.load(std::memory_order_acquire));

    std::array<std::uint8_t, 32> key{};
    for (std::size_t i = 0; i < key.size(); ++i) key[i] = static_cast<std::uint8_t>(i + 1);
    udp->set_secret_key(discusy::udp::encryption_mode::aead_xchacha20_poly1305_rtpsize, key);
    std::this_thread::sleep_for(std::chrono::milliseconds{20});

    // Send RTCP 200 (SR), 205 (RTPFB), 206 (PSFB) and non-Opus RTP (PT 101) — all must be ignored without decrypt drops.
    for (const std::uint8_t pt : {std::uint8_t{200}, std::uint8_t{205}, std::uint8_t{206}, std::uint8_t{101}}) {
        std::array<std::uint8_t, 48> pkt{};
        pkt[0] = 0x80;
        pkt[1] = pt;
        server_sock.send_to(boost::asio::buffer(pkt), client_ep);
    }

    // Build an RTP v2 packet with P=1, X=1, CC=1, PT=0x78 (Opus), 1-word 0xBEDE extension, and 4 bytes of padding.
    const std::vector<std::uint8_t> expected_opus{0xF8, 0xFF, 0xFE, 0x11, 0x22, 0x33};
    std::vector<std::uint8_t> header(12 + 4 + 4, 0);
    header[0] = 0x80U | 0x20U | 0x10U | 0x01U; // V=2, P=1, X=1, CC=1
    header[1] = 0x78U;
    const std::uint16_t seq_net = discusy::udp::to_network_order(std::uint16_t{42});
    const std::uint32_t ts_net = discusy::udp::to_network_order(std::uint32_t{9600});
    const std::uint32_t ssrc_net = discusy::udp::to_network_order(std::uint32_t{0xAABBCCDDU});
    std::memcpy(&header[2], &seq_net, 2);
    std::memcpy(&header[4], &ts_net, 4);
    std::memcpy(&header[8], &ssrc_net, 4);
    // Extension preamble at offset 16: 0xBEDE, length = 1 (4 bytes)
    header[16] = 0xBE;
    header[17] = 0xDE;
    header[18] = 0x00;
    header[19] = 0x01;

    std::vector<std::uint8_t> plaintext;
    plaintext.insert(plaintext.end(), {0x10, 0xAA, 0x00, 0x00}); // 4-byte encrypted extension body
    plaintext.insert(plaintext.end(), expected_opus.begin(), expected_opus.end());
    plaintext.insert(plaintext.end(), {0x00, 0x00, 0x00, 0x04}); // 4 bytes RTP padding

    std::array<std::uint8_t, crypto_aead_xchacha20poly1305_ietf_NPUBBYTES> nonce{};
    const std::uint32_t nonce_be = discusy::udp::to_network_order(std::uint32_t{7});
    std::memcpy(nonce.data(), &nonce_be, 4);

    std::vector<std::uint8_t> packet(header.size() + plaintext.size() + 16 + 4);
    std::memcpy(packet.data(), header.data(), header.size());
    unsigned long long ct_len = 0;
    REQUIRE(crypto_aead_xchacha20poly1305_ietf_encrypt(
        packet.data() + header.size(), &ct_len,
        plaintext.data(), plaintext.size(),
        header.data(), header.size(),
        nullptr, nonce.data(), key.data()
    ) == 0);
    std::memcpy(packet.data() + header.size() + ct_len, &nonce_be, 4);
    packet.resize(header.size() + static_cast<std::size_t>(ct_len) + 4);

    server_sock.send_to(boost::asio::buffer(packet), client_ep);

    for (int i = 0; i < 200 && received_count.load(std::memory_order_acquire) == 0; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds{5});
    }

    REQUIRE(received_count.load(std::memory_order_acquire) == 1);
    {
        const std::scoped_lock lock{recv_mtx};
        CHECK(got_ssrc == 0xAABBCCDDU);
        CHECK(got_seq == 42);
        CHECK(got_ts == 9600);
        CHECK(got_payload == expected_opus);
    }

    const auto stats = udp->get_stats();
    CHECK(stats.packets_received == 1);
    CHECK(stats.receive_decrypt_drops == 0);

    udp->close();
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
        workers.emplace_back([&conn, t] {
            for (int i = 0; i < 200; ++i) {
                const auto uid = discusy::snowflake{static_cast<std::uint64_t>(1000 + (t * 10) + (i % 5))};
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

    conn->set_underrun_grace_frames(3);
    conn->set_max_catchup_frames(2);
    conn->set_not_ready_hold_frames(10);
    CHECK(conn->set_inband_fec(true));
    CHECK(conn->set_expected_packet_loss(5));
    CHECK(conn->set_opus_complexity(5));
    CHECK(conn->set_opus_signal(OPUS_SIGNAL_VOICE));

    // Negative batch_duration and misaligned bytes must not throw or crash
    const std::array<std::uint8_t, 5> tiny_bytes{0, 1, 2, 3, 4};
    conn->send_pcm(std::span<const std::uint8_t>{tiny_bytes.data() + 1, 4}, std::chrono::milliseconds{-50});

    io_ctx->poll();
    conn.reset();
}

#endif
