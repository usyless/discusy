#include <catch2/catch_test_macros.hpp>
#include <discusy/types.hpp>
#include <string_view>
#include <compare>

TEST_CASE("Snowflake: Discord epoch, bit unpacking, and shard routing", "[snowflake]") {
    // A known snowflake: 175928847299117063ULL
    // Timestamp ms = (175928847299117063 >> 22) + 1420070400000 = 41944705796 + 1420070400000 = 1462015105796
    // Worker ID = (175928847299117063 & 0x3E0000) >> 17
    // Process ID = (175928847299117063 & 0x1F000) >> 12
    // Increment = 175928847299117063 & 0xFFF
    constexpr std::uint64_t raw_val = 175928847299117063ULL;
    constexpr discusy::snowflake sf{raw_val};
    constexpr std::uint64_t shifted = (raw_val >> 22);

    SECTION("Bit field deconstruction") {
        CHECK(sf.value == raw_val);
        CHECK(static_cast<std::uint64_t>(sf) == raw_val);
        CHECK(sf.get_timestamp() == 1462015105796ULL);
        CHECK(sf.get_timestamp() == (shifted + discusy::DISCORD_EPOCH));

        auto expected_worker = static_cast<std::uint8_t>((raw_val & 0x3E0000) >> 17);
        auto expected_process = static_cast<std::uint8_t>((raw_val & 0x1F000) >> 12);
        auto expected_inc = static_cast<std::uint16_t>(raw_val & 0xFFF);

        CHECK(sf.get_internal_worker_id() == expected_worker);
        CHECK(sf.get_internal_process_id() == expected_process);
        CHECK(sf.get_increment() == expected_inc);
    }

    SECTION("from_unix_time roundtrip") {
        const std::uint64_t epoch_ms = 1609459200000ULL; // 2021-01-01 00:00:00 UTC
        auto generated = discusy::snowflake::from_unix_time(epoch_ms);

        CHECK(generated.get_timestamp() == epoch_ms);
        CHECK(generated.get_increment() == 0);
    }

    SECTION("Guild shard ID calculation") {
        // (value >> 22) % total_shards
        CHECK(sf.guild_shard_id(1) == 0);
        CHECK(sf.guild_shard_id(10) == (shifted % 10)); // 6
        CHECK(sf.guild_shard_id(16) == (shifted % 16)); // 4
    }

    SECTION("Formatting and mentions") {
        CHECK(sf.str() == "175928847299117063");
        CHECK(sf.mention_user() == "<@175928847299117063>");
        CHECK(sf.mention_channel() == "<#175928847299117063>");
        CHECK(sf.mention_role() == "<@&175928847299117063>");

        constexpr auto const_str = sf.str();
        constexpr auto const_mu = sf.mention_user();
        constexpr auto const_mc = sf.mention_channel();
        constexpr auto const_mr = sf.mention_role();
        static_assert(const_str.view() == "175928847299117063");
        static_assert(const_mu.view() == "<@175928847299117063>");
        static_assert(const_mc.view() == "<#175928847299117063>");
        static_assert(const_mr.view() == "<@&175928847299117063>");

        discusy::snowflake_str stack_s{sf};
        std::string_view sv{stack_s.buf.data(), stack_s.len};
        CHECK(sv == "175928847299117063");
    }

    SECTION("Boolean and comparison semantics") {
        discusy::snowflake empty_sf{};
        CHECK_FALSE(bool(empty_sf));
        CHECK(!empty_sf);

        CHECK(bool(sf));
        CHECK_FALSE(!sf);

        constexpr discusy::snowflake sf2{raw_val};
        CHECK(sf == sf2);
        CHECK_FALSE(sf != sf2);
        CHECK(sf <= sf2);
        CHECK(sf >= sf2);
        CHECK_FALSE(sf < sf2);
        CHECK_FALSE(sf > sf2);

        constexpr discusy::snowflake sf_smaller{raw_val - 1};
        CHECK(sf_smaller < sf);
        CHECK(sf > sf_smaller);
        CHECK(std::is_eq(sf <=> sf2));
        CHECK(std::is_lt(sf_smaller <=> sf));
        CHECK(std::is_gt(sf <=> sf_smaller));
    }
}
