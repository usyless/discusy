#include <catch2/catch_test_macros.hpp>
#include <discusy/etf.hpp>
#include <discusy/types.hpp>
#include <discusy/gateway_events.hpp>
#include <etf/etf.hpp>
#include <numbers>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <unordered_map>
#include <optional>
#include <variant>
#include <cstdint>

namespace test_etf_types {

struct SampleStruct {
    discusy::snowflake id{};
    std::string name{};
    std::optional<std::string> description{};
    std::uint32_t count{0};
    bool enabled{false};

    bool operator==(const SampleStruct&) const = default;
};

struct NestedStruct {
    std::string title{};
    SampleStruct sample{};
    std::vector<int> numbers{};

    bool operator==(const NestedStruct&) const = default;
};

struct SubsetStruct {
    std::string name{};
    std::uint32_t count{0};
};

struct GatewayBaseEvent {
    discusy::Opcode op{discusy::Opcode::Dispatch};
    std::optional<std::int64_t> s{1234};
    std::optional<discusy::recieve_event::event> t{discusy::recieve_event::event::MESSAGE_CREATE};
    std::string d{"dummy_event_data"};
};

template <typename T>
struct FullGatewayEvent {
    discusy::Opcode op{discusy::Opcode::Dispatch};
    std::optional<std::int64_t> s{42};
    std::optional<discusy::recieve_event::event> t{discusy::recieve_event::event::INTERACTION_CREATE};
    T d{};
};

template <typename T>
struct ErlangSortedFullGatewayEvent {
    T d{};
    discusy::Opcode op{discusy::Opcode::Dispatch};
    std::optional<std::int64_t> s{42};
    std::optional<discusy::recieve_event::event> t{discusy::recieve_event::event::INTERACTION_CREATE};
};

struct FullPayload {
    discusy::snowflake id{123456789012345678ULL};
    std::string name{"test_name"};
    std::uint32_t count{42};
    std::string extra_data{"extra data that should be skipped by partial read"};
    std::vector<int> extra_list{1, 2, 3, 4, 5};
};

struct ErlangSortedGatewayEvent {
    std::string d{"massive_data_first"};
    discusy::Opcode op{discusy::Opcode::Dispatch};
    std::optional<std::int64_t> s{777};
    std::optional<discusy::recieve_event::event> t{discusy::recieve_event::event::CHANNEL_CREATE};
};

struct HelloGatewayEvent {
    discusy::Opcode op{discusy::Opcode::Hello};
    std::string d{"hello_payload"};
};

struct VariantHolder {
    std::string label{};
    std::variant<int, double, std::string, bool> data{};

    bool operator==(const VariantHolder&) const = default;
};

struct StructA {
    discusy::snowflake id{};
    std::string name{};
};
struct StructB {
    discusy::snowflake id{};
    std::string custom_id{};
    int component_type{0};
};

}

using namespace test_etf_types;

TEST_CASE("ETF: Basic types serialization and deserialization", "[etf]") {
    SECTION("Booleans") {
        std::string encoded_true;
        std::string encoded_false;
        REQUIRE_FALSE(glz::write_etf(true, encoded_true));
        REQUIRE_FALSE(glz::write_etf(false, encoded_false));

        // Version byte (131) + SMALL_ATOM_UTF8_EXT (119) + length + atom name
        REQUIRE(encoded_true.size() > 2);
        CHECK(static_cast<std::uint8_t>(encoded_true[0]) == glz::etf::magic_version);
        CHECK(static_cast<std::uint8_t>(encoded_true[1]) == glz::etf::tag::SMALL_ATOM_UTF8_EXT);

        bool val_true = false;
        bool val_false = true;
        REQUIRE_FALSE(glz::read_etf(val_true, encoded_true));
        REQUIRE_FALSE(glz::read_etf(val_false, encoded_false));
        CHECK(val_true == true);
        CHECK(val_false == false);
    }

    SECTION("Small unsigned integers (0 - 255)") {
        std::uint8_t val = 42;
        std::string encoded;
        REQUIRE_FALSE(glz::write_etf(val, encoded));

        REQUIRE(encoded.size() == 3); // 131 + SMALL_INTEGER_EXT (97) + 42
        CHECK(static_cast<std::uint8_t>(encoded[0]) == glz::etf::magic_version);
        CHECK(static_cast<std::uint8_t>(encoded[1]) == glz::etf::tag::SMALL_INTEGER_EXT);
        CHECK(static_cast<std::uint8_t>(encoded[2]) == 42);

        std::uint8_t decoded = 0;
        REQUIRE_FALSE(glz::read_etf(decoded, encoded));
        CHECK(decoded == 42);
    }

    SECTION("Signed 32-bit integers") {
        std::int32_t positive = 100000;
        std::int32_t negative = -54321;
        std::string enc_pos;
        std::string enc_neg;
        REQUIRE_FALSE(glz::write_etf(positive, enc_pos));
        REQUIRE_FALSE(glz::write_etf(negative, enc_neg));

        CHECK(static_cast<std::uint8_t>(enc_pos[1]) == glz::etf::tag::INTEGER_EXT);
        CHECK(static_cast<std::uint8_t>(enc_neg[1]) == glz::etf::tag::INTEGER_EXT);

        std::int32_t dec_pos = 0;
        std::int32_t dec_neg = 0;
        REQUIRE_FALSE(glz::read_etf(dec_pos, enc_pos));
        REQUIRE_FALSE(glz::read_etf(dec_neg, enc_neg));
        CHECK(dec_pos == positive);
        CHECK(dec_neg == negative);
    }

    SECTION("64-bit integers (SMALL_BIG_EXT)") {
        std::uint64_t large_uint = 175928847299117063ULL;
        std::string encoded;
        REQUIRE_FALSE(glz::write_etf(large_uint, encoded));

        // 131 + SMALL_BIG_EXT (110) + len(8) + sign(0) + 8 little-endian bytes
        REQUIRE(encoded.size() == 1 + 1 + 1 + 1 + 8);
        CHECK(static_cast<std::uint8_t>(encoded[0]) == glz::etf::magic_version);
        CHECK(static_cast<std::uint8_t>(encoded[1]) == glz::etf::tag::SMALL_BIG_EXT);
        CHECK(static_cast<std::uint8_t>(encoded[2]) == 8); // 8 bytes
        CHECK(static_cast<std::uint8_t>(encoded[3]) == 0); // positive sign

        std::uint64_t decoded = 0;
        REQUIRE_FALSE(glz::read_etf(decoded, encoded));
        CHECK(decoded == large_uint);
    }

    SECTION("Floating point numbers") {
        double d = std::numbers::pi;
        std::string encoded;
        REQUIRE_FALSE(glz::write_etf(d, encoded));

        CHECK(static_cast<std::uint8_t>(encoded[0]) == glz::etf::magic_version);
        CHECK(static_cast<std::uint8_t>(encoded[1]) == glz::etf::tag::NEW_FLOAT_EXT);

        double decoded = 0.0;
        REQUIRE_FALSE(glz::read_etf(decoded, encoded));
        CHECK(decoded == d);
    }

    SECTION("Strings (BINARY_EXT)") {
        std::string str = "Hello, Discord ETF!";
        std::string encoded;
        REQUIRE_FALSE(glz::write_etf(str, encoded));

        // 131 + BINARY_EXT (109) + 4-byte length + string data
        CHECK(static_cast<std::uint8_t>(encoded[0]) == glz::etf::magic_version);
        CHECK(static_cast<std::uint8_t>(encoded[1]) == glz::etf::tag::BINARY_EXT);

        std::string decoded;
        REQUIRE_FALSE(glz::read_etf(decoded, encoded));
        CHECK(decoded == str);

        // Deserializing into std::string_view
        std::string_view sv_decoded;
        REQUIRE_FALSE(glz::read_etf(sv_decoded, encoded));
        CHECK(sv_decoded == str);
    }

    SECTION("Empty strings") {
        std::string empty;
        std::string encoded;
        REQUIRE_FALSE(glz::write_etf(empty, encoded));

        std::string decoded = "not_empty";
        REQUIRE_FALSE(glz::read_etf(decoded, encoded));
        CHECK(decoded.empty());
    }

    SECTION("Nullable types & Optionals") {
        std::optional<int> with_val = 123;
        std::optional<int> without_val = std::nullopt;

        std::string enc_val;
        std::string enc_none;
        REQUIRE_FALSE(glz::write_etf(with_val, enc_val));
        REQUIRE_FALSE(glz::write_etf(without_val, enc_none));

        // nullopt should serialize as atom nil
        CHECK(static_cast<std::uint8_t>(enc_none[1]) == glz::etf::tag::SMALL_ATOM_UTF8_EXT);
        CHECK(enc_none.contains("nil"));

        std::optional<int> dec_val;
        std::optional<int> dec_none = 999;
        REQUIRE_FALSE(glz::read_etf(dec_val, enc_val));
        REQUIRE_FALSE(glz::read_etf(dec_none, enc_none));

        REQUIRE(dec_val.has_value());
        CHECK(*dec_val == 123);
        CHECK_FALSE(dec_none.has_value());
    }
}

TEST_CASE("ETF: Snowflake handling (integer and string transmissions)", "[etf][snowflake]") {
    constexpr std::uint64_t raw_id = 175928847299117063ULL;
    const discusy::snowflake sf{raw_id};

    SECTION("Snowflake roundtrip as integer") {
        std::string encoded;
        REQUIRE_FALSE(glz::write_etf(sf.value, encoded));

        discusy::snowflake decoded{};
        REQUIRE_FALSE(glz::read_etf(decoded.value, encoded));
        CHECK(decoded == sf);
        CHECK(decoded.value == raw_id);
    }

    SECTION("Snowflake deserialization from string (BINARY_EXT)") {
        // Discord transmits Snowflakes either as 64-bit integers or as numeric strings
        std::string string_id = "175928847299117063";
        std::string encoded;
        REQUIRE_FALSE(glz::write_etf(string_id, encoded));

        discusy::snowflake decoded{};
        REQUIRE_FALSE(glz::read_etf(decoded.value, encoded));
        CHECK(decoded.value == raw_id);
    }

    SECTION("Snowflake deserialization from STRING_EXT") {
        // Manually construct STRING_EXT payload
        std::string raw_str = "175928847299117063";
        std::string custom_etf;
        custom_etf.push_back(static_cast<char>(glz::etf::magic_version));
        custom_etf.push_back(static_cast<char>(glz::etf::tag::STRING_EXT));
        std::uint16_t len = static_cast<std::uint16_t>(raw_str.size());
        std::uint16_t be_len = glz::etf::detail::to_big_endian(len);
        custom_etf.append(reinterpret_cast<const char*>(&be_len), 2);
        custom_etf.append(raw_str);

        discusy::snowflake decoded{};
        REQUIRE_FALSE(glz::read_etf(decoded.value, custom_etf));
        CHECK(decoded.value == raw_id);
    }

    SECTION("Snowflake deserialization from SMALL_INTEGER_EXT") {
        // Small ID like 100
        std::string custom_etf;
        custom_etf.push_back(static_cast<char>(glz::etf::magic_version));
        custom_etf.push_back(static_cast<char>(glz::etf::tag::SMALL_INTEGER_EXT));
        custom_etf.push_back(static_cast<char>(100));

        discusy::snowflake decoded{};
        REQUIRE_FALSE(glz::read_etf(decoded.value, custom_etf));
        CHECK(decoded.value == 100ULL);
    }
}

TEST_CASE("ETF: Containers, Vectors, Maps, and Tuples", "[etf][containers]") {
    SECTION("Vectors of integers") {
        std::vector<int> numbers = {10, 20, 30, 40, 50};
        std::string encoded;
        REQUIRE_FALSE(glz::write_etf(numbers, encoded));

        CHECK(static_cast<std::uint8_t>(encoded[1]) == glz::etf::tag::LIST_EXT);

        std::vector<int> decoded;
        REQUIRE_FALSE(glz::read_etf(decoded, encoded));
        CHECK(decoded == numbers);
    }

    SECTION("Empty vectors (NIL_EXT)") {
        std::vector<int> empty_vec;
        std::string encoded;
        REQUIRE_FALSE(glz::write_etf(empty_vec, encoded));

        // In Erlang ETF, empty list is NIL_EXT (106)
        REQUIRE(encoded.size() == 2);
        CHECK(static_cast<std::uint8_t>(encoded[1]) == glz::etf::tag::NIL_EXT);

        std::vector<int> decoded = {1, 2, 3};
        REQUIRE_FALSE(glz::read_etf(decoded, encoded));
        CHECK(decoded.empty());
    }

    SECTION("Maps of string to int") {
        std::map<std::string, int> dict = {
            {"apple", 5},
            {"banana", 12},
            {"cherry", 30},
        };
        std::string encoded;
        REQUIRE_FALSE(glz::write_etf(dict, encoded));

        CHECK(static_cast<std::uint8_t>(encoded[1]) == glz::etf::tag::MAP_EXT);

        std::map<std::string, int> decoded;
        REQUIRE_FALSE(glz::read_etf(decoded, encoded));
        CHECK(decoded == dict);
    }

    SECTION("Tuples and Pairs") {
        std::tuple<int, std::string, bool> tup = {42, "answer", true};
        std::string encoded;
        REQUIRE_FALSE(glz::write_etf(tup, encoded));

        std::tuple<int, std::string, bool> decoded;
        REQUIRE_FALSE(glz::read_etf(decoded, encoded));
        CHECK(decoded == tup);
    }

    SECTION("Raw JSON and Text views (guild_members_chunk not_found)") {
        std::vector<int> numbers = {10, 20, 30};
        std::string encoded;
        REQUIRE_FALSE(glz::write_etf(numbers, encoded));

        std::vector<glz::raw_json_view> views;
        REQUIRE_FALSE(glz::read_etf(views, encoded));
        REQUIRE(views.size() == 3);
        CHECK(views[0].str.size() == 2); // SMALL_INTEGER_EXT (97) + 10
        CHECK(static_cast<std::uint8_t>(views[0].str[0]) == glz::etf::tag::SMALL_INTEGER_EXT);
        CHECK(static_cast<std::uint8_t>(views[0].str[1]) == 10);
    }
}

TEST_CASE("ETF: Compile-time hash map object serialization and string keys enforcement", "[etf][object]") {
    SampleStruct original{
        .id = discusy::snowflake{987654321012345678ULL},
        .name = "TestObject",
        .description = "A thorough description",
        .count = 42,
        .enabled = true,
    };

    SECTION("Object serialization and deserialization roundtrip") {
        std::string encoded;
        REQUIRE_FALSE(glz::write_etf(original, encoded));

        // 1. First byte must be magic version 131
        REQUIRE(encoded.size() > 5);
        CHECK(static_cast<std::uint8_t>(encoded[0]) == glz::etf::magic_version);

        // 2. Second byte must be MAP_EXT (116)
        CHECK(static_cast<std::uint8_t>(encoded[1]) == glz::etf::tag::MAP_EXT);

        // 3. Arity check: 5 pairs
        auto arity = glz::etf::detail::read_be<std::uint32_t>(&encoded[2]);
        CHECK(arity == 5);

        // 4. CRITICAL: Keys must NOT be atoms! Discord 4002 error requires STRING KEYS ONLY (BINARY_EXT 109)
        // Check that ATOM_EXT (100), SMALL_ATOM_EXT (115), ATOM_UTF8_EXT (118), SMALL_ATOM_UTF8_EXT (119) are not used for keys
        // Scan for keys:
        CHECK(encoded.contains("name"));
        CHECK(encoded.contains("count"));
        CHECK(encoded.contains("enabled"));

        SampleStruct decoded{};
        REQUIRE_FALSE(glz::read_etf(decoded, encoded));
        CHECK(decoded == original);
    }

    SECTION("Null member skipping with single-pass arity back-patching") {
        SampleStruct without_desc{
            .id = discusy::snowflake{12345ULL},
            .name = "NoDesc",
            .description = std::nullopt, // Should be omitted when skip_null_members = true
            .count = 10,
            .enabled = false,
        };

        std::string encoded;
        REQUIRE_FALSE(glz::write_etf(without_desc, encoded));

        // Arity should be back-patched to exactly 4!
        auto arity = glz::etf::detail::read_be<std::uint32_t>(&encoded[2]);
        CHECK(arity == 4);

        // "description" should not appear in the encoded output
        CHECK_FALSE(encoded.contains("description"));

        SampleStruct decoded{};
        REQUIRE_FALSE(glz::read_etf(decoded, encoded));
        CHECK(decoded == without_desc);
        CHECK_FALSE(decoded.description.has_value());
    }

    SECTION("Skipping unknown keys in input without error") {
        // Construct an ETF map that includes an unknown extra key
        std::string encoded;
        REQUIRE_FALSE(glz::write_etf(original, encoded));

        // Manually prepend/append an unknown key to an ETF map
        // Or decode into a struct that only has a subset of fields
        SubsetStruct subset{};
        REQUIRE_FALSE(glz::read_etf(subset, encoded));
        CHECK(subset.name == original.name);
        CHECK(subset.count == original.count);
    }

    SECTION("Nested structures") {
        NestedStruct nested{
            .title = "Nested Document",
            .sample = original,
            .numbers = {1, 2, 3, 4, 5},
        };

        std::string encoded;
        REQUIRE_FALSE(glz::write_etf(nested, encoded));

        NestedStruct decoded{};
        REQUIRE_FALSE(glz::read_etf(decoded, encoded));
        CHECK(decoded == nested);
    }
}

TEST_CASE("ETF: Discord Gateway Events & Payloads", "[etf][gateway]") {
    SECTION("Hello payload") {
        discusy::recieve_event::hello hello_data{
            .heartbeat_interval = 41250,
        };
        discusy::recieve_event::payload<discusy::recieve_event::hello> payload{
            .d = hello_data,
        };

        std::string encoded;
        REQUIRE_FALSE(discusy::etf::write_etf(payload, encoded));

        discusy::recieve_event::payload<discusy::recieve_event::hello> decoded{};
        REQUIRE_FALSE(discusy::etf::parse_etf(decoded, encoded));
        CHECK(decoded.d.heartbeat_interval == 41250);
    }

    SECTION("Identify payload") {
        discusy::send_event::identify id_data{
            .token = "my_super_secret_bot_token",
            .shard{{0, 8}},
            .intents = static_cast<discusy::intent>(3276799),
        };
        discusy::send_event::identify_payload payload{id_data};

        std::string encoded;
        REQUIRE_FALSE(discusy::etf::write_etf(payload, encoded));

        // Ensure token key is serialized as string key
        CHECK(encoded.contains("token"));
        CHECK(encoded.contains("intents"));

        discusy::send_event::identify_payload decoded{id_data};
        REQUIRE_FALSE(discusy::etf::parse_etf(decoded, encoded));
        CHECK(decoded.d.token == "my_super_secret_bot_token");
        CHECK(decoded.d.intents == static_cast<discusy::intent>(3276799));
        CHECK(decoded.d.shard.has_value());
        CHECK((*decoded.d.shard)[0] == 0);
        CHECK((*decoded.d.shard)[1] == 8);
    }

    SECTION("Resume payload") {
        discusy::send_event::resume resume_data{
            .token = "resume_bot_token",
            .session_id = "abc123session",
            .seq = 450,
        };
        discusy::send_event::resume_payload payload{resume_data};

        std::string encoded;
        REQUIRE_FALSE(discusy::etf::write_etf(payload, encoded));

        discusy::send_event::resume_payload decoded{resume_data};
        REQUIRE_FALSE(discusy::etf::parse_etf(decoded, encoded));
        CHECK(decoded.d.token == "resume_bot_token");
        CHECK(decoded.d.session_id == "abc123session");
        CHECK(decoded.d.seq == 450);
    }

    SECTION("Heartbeat payload") {
        discusy::send_event::heartbeat hb{42};
        discusy::send_event::heartbeat_payload payload{hb};

        std::string encoded;
        REQUIRE_FALSE(discusy::etf::write_etf(payload, encoded));

        discusy::send_event::heartbeat_payload decoded{hb};
        REQUIRE_FALSE(discusy::etf::parse_etf(decoded, encoded));
        CHECK(decoded.d.has_value());
        CHECK(*decoded.d == 42);
    }

    SECTION("Payload base extraction") {
        // Gateway dispatch event base: op, s, t
        GatewayBaseEvent evt{};
        std::string encoded;
        REQUIRE_FALSE(glz::write_etf(evt, encoded));

        discusy::recieve_event::payload_base base_extracted{};
        REQUIRE_FALSE(discusy::etf::parse_payload_base(base_extracted, encoded));

        CHECK(base_extracted.op == discusy::Opcode::Dispatch);
        REQUIRE(base_extracted.s.has_value());
        CHECK(*base_extracted.s == 1234);
        REQUIRE(base_extracted.t.has_value());
        CHECK(*base_extracted.t == discusy::recieve_event::event::MESSAGE_CREATE);
    }
}

TEST_CASE("ETF: Error Handling and Malformed Input", "[etf][errors]") {
    SECTION("Magic version mismatch") {
        std::string malformed;
        malformed.push_back(static_cast<char>(132)); // wrong version (not 131)
        malformed.push_back(static_cast<char>(glz::etf::tag::SMALL_INTEGER_EXT));
        malformed.push_back(static_cast<char>(1));

        int val = 0;
        auto ec = glz::read_etf(val, malformed);
        CHECK(static_cast<bool>(ec));
        CHECK(ec.ec == glz::error_code::version_mismatch);
    }

    SECTION("Empty input buffer") {
        std::string empty;
        int val = 0;
        auto ec = glz::read_etf(val, empty);
        CHECK(static_cast<bool>(ec));
    }

    SECTION("Truncated binary payload") {
        std::string truncated;
        truncated.push_back(static_cast<char>(glz::etf::magic_version));
        truncated.push_back(static_cast<char>(glz::etf::tag::BINARY_EXT));
        truncated.push_back(static_cast<char>(0));
        truncated.push_back(static_cast<char>(0));
        truncated.push_back(static_cast<char>(0));
        truncated.push_back(static_cast<char>(10)); // claims 10 bytes follow, but buffer ends here

        std::string str;
        auto ec = glz::read_etf(str, truncated);
        CHECK(static_cast<bool>(ec));
        CHECK(ec.ec == glz::error_code::unexpected_end);
    }
}

TEST_CASE("ETF: Security and Robustness Edge Cases", "[etf][security][errors]") {
    auto append_be32 = [](std::string& s, uint32_t val) {
        s.push_back(static_cast<char>((val >> 24) & 0xFF));
        s.push_back(static_cast<char>((val >> 16) & 0xFF));
        s.push_back(static_cast<char>((val >> 8) & 0xFF));
        s.push_back(static_cast<char>(val & 0xFF));
    };
    auto append_be16 = [](std::string& s, uint16_t val) {
        s.push_back(static_cast<char>((val >> 8) & 0xFF));
        s.push_back(static_cast<char>(val & 0xFF));
    };

    SECTION("LARGE_BIG_EXT integer overflow protection (n = 0xFFFFFFFF)") {
        // Attack payload: n = 0xFFFFFFFF (4294967295) with only 8 total bytes
        std::string buf;
        buf.push_back(static_cast<char>(glz::etf::magic_version));
        buf.push_back(static_cast<char>(glz::etf::tag::LARGE_BIG_EXT));
        append_be32(buf, 0xFFFFFFFFU);
        buf.push_back(static_cast<char>(0)); // sign
        buf.push_back(static_cast<char>(1)); // 1 digit byte

        uint64_t num = 0;
        auto ec_read = glz::read_etf(num, buf);
        CHECK(static_cast<bool>(ec_read));
        CHECK(ec_read.ec == glz::error_code::unexpected_end);

        glz::skip s{};
        auto ec_skip = glz::read_etf(s, buf);
        CHECK(static_cast<bool>(ec_skip));
        CHECK(ec_skip.ec == glz::error_code::unexpected_end);

        // Sub-case: n = 0xFFFFFFFE with truncated buffer
        std::string buf2;
        buf2.push_back(static_cast<char>(glz::etf::magic_version));
        buf2.push_back(static_cast<char>(glz::etf::tag::LARGE_BIG_EXT));
        append_be32(buf2, 0xFFFFFFFEU);
        auto ec_read2 = glz::read_etf(num, buf2);
        CHECK(static_cast<bool>(ec_read2));
        CHECK(ec_read2.ec == glz::error_code::unexpected_end);

        // Sub-case: n = 10, but only 3 digits provided
        std::string buf3;
        buf3.push_back(static_cast<char>(glz::etf::magic_version));
        buf3.push_back(static_cast<char>(glz::etf::tag::LARGE_BIG_EXT));
        append_be32(buf3, 10);
        buf3.push_back(static_cast<char>(0)); // sign
        buf3.push_back(static_cast<char>(1));
        buf3.push_back(static_cast<char>(2));
        buf3.push_back(static_cast<char>(3));
        auto ec_read3 = glz::read_etf(num, buf3);
        CHECK(static_cast<bool>(ec_read3));
        CHECK(ec_read3.ec == glz::error_code::unexpected_end);
    }

    SECTION("Container size pre-allocation sanity checks (LIST_EXT)") {
        // Attack payload: claims 2,000,000,000 elements in a 7-byte buffer
        std::string buf;
        buf.push_back(static_cast<char>(glz::etf::magic_version));
        buf.push_back(static_cast<char>(glz::etf::tag::LIST_EXT));
        append_be32(buf, 2000000000U);
        buf.push_back(static_cast<char>(glz::etf::tag::NIL_EXT));

        std::vector<int> vec;
        auto ec = glz::read_etf(vec, buf);
        CHECK(static_cast<bool>(ec));
        CHECK(ec.ec == glz::error_code::unexpected_end);
        CHECK(vec.empty());

        // parse_etf wrapper must return failure without throwing std::bad_alloc
        bool failed = discusy::etf::parse_etf(vec, buf);
        CHECK(failed);
        CHECK(vec.empty());

        // skip_value must also reject the bogus length
        glz::skip s{};
        auto ec_skip = glz::read_etf(s, buf);
        CHECK(static_cast<bool>(ec_skip));
        CHECK(ec_skip.ec == glz::error_code::unexpected_end);

        // Sub-case: LIST_EXT truncated before tail term (len = 1, 1 element, no NIL_EXT)
        std::string buf_no_tail;
        buf_no_tail.push_back(static_cast<char>(glz::etf::magic_version));
        buf_no_tail.push_back(static_cast<char>(glz::etf::tag::LIST_EXT));
        append_be32(buf_no_tail, 1);
        buf_no_tail.push_back(static_cast<char>(glz::etf::tag::SMALL_INTEGER_EXT));
        buf_no_tail.push_back(static_cast<char>(42));
        auto ec_no_tail = glz::read_etf(vec, buf_no_tail);
        CHECK(static_cast<bool>(ec_no_tail));
        CHECK(ec_no_tail.ec == glz::error_code::unexpected_end);
    }

    SECTION("Container size pre-allocation sanity checks (LARGE_TUPLE_EXT)") {
        // Attack payload: claims 1,000,000 elements in a 6-byte buffer
        std::string buf;
        buf.push_back(static_cast<char>(glz::etf::magic_version));
        buf.push_back(static_cast<char>(glz::etf::tag::LARGE_TUPLE_EXT));
        append_be32(buf, 1000000U);

        std::vector<int> vec;
        auto ec = glz::read_etf(vec, buf);
        CHECK(static_cast<bool>(ec));
        CHECK(ec.ec == glz::error_code::unexpected_end);
        CHECK(vec.empty());

        glz::skip s{};
        auto ec_skip = glz::read_etf(s, buf);
        CHECK(static_cast<bool>(ec_skip));
        CHECK(ec_skip.ec == glz::error_code::unexpected_end);
    }

    SECTION("Map arity bounds check against payload size (MAP_EXT)") {
        // Attack payload: claims 1,000,000,000 key-value pairs in a 6-byte buffer
        std::string buf;
        buf.push_back(static_cast<char>(glz::etf::magic_version));
        buf.push_back(static_cast<char>(glz::etf::tag::MAP_EXT));
        append_be32(buf, 1000000000U);

        std::map<std::string, int> m;
        auto ec_map = glz::read_etf(m, buf);
        CHECK(static_cast<bool>(ec_map));
        CHECK(ec_map.ec == glz::error_code::unexpected_end);
        CHECK(m.empty());

        test_etf_types::SampleStruct sample{};
        auto ec_struct = glz::read_etf(sample, buf);
        CHECK(static_cast<bool>(ec_struct));
        CHECK(ec_struct.ec == glz::error_code::unexpected_end);

        // parse_payload_base must reject map arity exceeding buffer
        discusy::recieve_event::payload_base base_event{};
        CHECK(discusy::etf::parse_payload_base(base_event, buf));

        glz::skip s{};
        auto ec_skip = glz::read_etf(s, buf);
        CHECK(static_cast<bool>(ec_skip));
        CHECK(ec_skip.ec == glz::error_code::unexpected_end);
    }

    SECTION("Maximum recursive depth enforcement (exceeded_max_recursive_depth)") {
        // Construct deeply nested LIST_EXT (300 levels > max_recursive_depth_limit 256)
        constexpr size_t nesting = 300;
        std::string deep_list;
        deep_list.push_back(static_cast<char>(glz::etf::magic_version));
        for (size_t i = 0; i < nesting; ++i) {
            deep_list.push_back(static_cast<char>(glz::etf::tag::LIST_EXT));
            append_be32(deep_list, 1);
        }
        deep_list.push_back(static_cast<char>(glz::etf::tag::SMALL_INTEGER_EXT));
        deep_list.push_back(static_cast<char>(1));
        for (size_t i = 0; i < nesting; ++i) {
            deep_list.push_back(static_cast<char>(glz::etf::tag::NIL_EXT));
        }

        glz::generic json_val{};
        auto ec_list = glz::read_etf(json_val, deep_list);
        CHECK(static_cast<bool>(ec_list));
        CHECK(ec_list.ec == glz::error_code::exceeded_max_recursive_depth);

        glz::skip s{};
        auto ec_skip = glz::read_etf(s, deep_list);
        CHECK(static_cast<bool>(ec_skip));
        CHECK(ec_skip.ec == glz::error_code::exceeded_max_recursive_depth);

        // Construct deeply nested MAP_EXT (300 levels)
        std::string deep_map;
        deep_map.push_back(static_cast<char>(glz::etf::magic_version));
        for (size_t i = 0; i < nesting; ++i) {
            deep_map.push_back(static_cast<char>(glz::etf::tag::MAP_EXT));
            append_be32(deep_map, 1);
            deep_map.push_back(static_cast<char>(glz::etf::tag::BINARY_EXT));
            append_be32(deep_map, 1);
            deep_map.push_back('k');
        }
        deep_map.push_back(static_cast<char>(glz::etf::tag::SMALL_INTEGER_EXT));
        deep_map.push_back(static_cast<char>(99));

        auto ec_map = glz::read_etf(json_val, deep_map);
        CHECK(static_cast<bool>(ec_map));
        CHECK(ec_map.ec == glz::error_code::exceeded_max_recursive_depth);
    }

    SECTION("Truncated primitive data types") {
        // NEW_FLOAT_EXT (< 8 bytes)
        std::string trunc_nfloat;
        trunc_nfloat.push_back(static_cast<char>(glz::etf::magic_version));
        trunc_nfloat.push_back(static_cast<char>(glz::etf::tag::NEW_FLOAT_EXT));
        trunc_nfloat.push_back(static_cast<char>(0));
        double d = 0.0;
        CHECK(static_cast<bool>(glz::read_etf(d, trunc_nfloat)));

        // FLOAT_EXT (< 31 bytes)
        std::string trunc_float;
        trunc_float.push_back(static_cast<char>(glz::etf::magic_version));
        trunc_float.push_back(static_cast<char>(glz::etf::tag::FLOAT_EXT));
        trunc_float.append("3.14");
        CHECK(static_cast<bool>(glz::read_etf(d, trunc_float)));

        // INTEGER_EXT (< 4 bytes)
        std::string trunc_int;
        trunc_int.push_back(static_cast<char>(glz::etf::magic_version));
        trunc_int.push_back(static_cast<char>(glz::etf::tag::INTEGER_EXT));
        trunc_int.push_back(static_cast<char>(1));
        int val_i = 0;
        CHECK(static_cast<bool>(glz::read_etf(val_i, trunc_int)));

        // SMALL_BIG_EXT (claims 8 bytes, only 1 provided)
        std::string trunc_sbig;
        trunc_sbig.push_back(static_cast<char>(glz::etf::magic_version));
        trunc_sbig.push_back(static_cast<char>(glz::etf::tag::SMALL_BIG_EXT));
        trunc_sbig.push_back(static_cast<char>(8)); // len 8
        trunc_sbig.push_back(static_cast<char>(0)); // sign
        trunc_sbig.push_back(static_cast<char>(1)); // only 1 digit
        uint64_t val_u = 0;
        CHECK(static_cast<bool>(glz::read_etf(val_u, trunc_sbig)));

        // STRING_EXT (claims 20 bytes, only 2 provided)
        std::string trunc_str;
        trunc_str.push_back(static_cast<char>(glz::etf::magic_version));
        trunc_str.push_back(static_cast<char>(glz::etf::tag::STRING_EXT));
        append_be16(trunc_str, 20);
        trunc_str.append("ab");
        std::string s;
        CHECK(static_cast<bool>(glz::read_etf(s, trunc_str)));

        // ATOM_EXT (claims 15 bytes, only 1 provided)
        std::string trunc_atom;
        trunc_atom.push_back(static_cast<char>(glz::etf::magic_version));
        trunc_atom.push_back(static_cast<char>(glz::etf::tag::ATOM_EXT));
        append_be16(trunc_atom, 15);
        trunc_atom.push_back('x');
        CHECK(static_cast<bool>(glz::read_etf(s, trunc_atom)));

        // SMALL_ATOM_EXT (claims 25 bytes, only 1 provided)
        std::string trunc_satom;
        trunc_satom.push_back(static_cast<char>(glz::etf::magic_version));
        trunc_satom.push_back(static_cast<char>(glz::etf::tag::SMALL_ATOM_EXT));
        trunc_satom.push_back(static_cast<char>(25));
        trunc_satom.push_back('x');
        CHECK(static_cast<bool>(glz::read_etf(s, trunc_satom)));

        // BIT_BINARY_EXT (< 5 bytes header)
        std::string trunc_bitbin;
        trunc_bitbin.push_back(static_cast<char>(glz::etf::magic_version));
        trunc_bitbin.push_back(static_cast<char>(glz::etf::tag::BIT_BINARY_EXT));
        append_be32(trunc_bitbin, 10); // missing bits byte
        glz::skip sk{};
        CHECK(static_cast<bool>(glz::read_etf(sk, trunc_bitbin)));
    }

    SECTION("Type mismatch and invalid value handling") {
        // Non-numeric string into number
        std::string non_num;
        non_num.push_back(static_cast<char>(glz::etf::magic_version));
        non_num.push_back(static_cast<char>(glz::etf::tag::BINARY_EXT));
        append_be32(non_num, 6);
        non_num.append("foobar");
        int num = 0;
        auto ec_num = glz::read_etf(num, non_num);
        CHECK(static_cast<bool>(ec_num));
        CHECK(ec_num.ec == glz::error_code::parse_number_failure);

        // Alphanumeric string into snowflake
        std::string bad_sf;
        bad_sf.push_back(static_cast<char>(glz::etf::magic_version));
        bad_sf.push_back(static_cast<char>(glz::etf::tag::BINARY_EXT));
        append_be32(bad_sf, 7);
        bad_sf.append("1234abc");
        discusy::snowflake sf{};
        auto ec_sf = glz::read_etf(sf, bad_sf);
        CHECK(static_cast<bool>(ec_sf));
        CHECK(ec_sf.ec == glz::error_code::parse_number_failure);

        // Non-boolean atom into bool
        std::string bad_bool;
        bad_bool.push_back(static_cast<char>(glz::etf::magic_version));
        bad_bool.push_back(static_cast<char>(glz::etf::tag::SMALL_ATOM_EXT));
        bad_bool.push_back(static_cast<char>(5));
        bad_bool.append("maybe");
        bool b = false;
        auto ec_bool = glz::read_etf(b, bad_bool);
        CHECK(static_cast<bool>(ec_bool));
        CHECK(ec_bool.ec == glz::error_code::syntax_error);

        // Unknown tag byte (250)
        std::string unknown_tag;
        unknown_tag.push_back(static_cast<char>(glz::etf::magic_version));
        unknown_tag.push_back(static_cast<char>(250));
        unknown_tag.push_back(static_cast<char>(0));
        int val = 0;
        CHECK(static_cast<bool>(glz::read_etf(val, unknown_tag)));
        glz::skip sk{};
        CHECK(static_cast<bool>(glz::read_etf(sk, unknown_tag)));
    }

    SECTION("Valid minimal and empty edge cases") {
        // Valid empty LIST_EXT (len = 0 with NIL_EXT tail)
        std::string empty_list;
        empty_list.push_back(static_cast<char>(glz::etf::magic_version));
        empty_list.push_back(static_cast<char>(glz::etf::tag::LIST_EXT));
        append_be32(empty_list, 0);
        empty_list.push_back(static_cast<char>(glz::etf::tag::NIL_EXT));

        std::vector<int> vec{1, 2, 3};
        REQUIRE_FALSE(glz::read_etf(vec, empty_list));
        CHECK(vec.empty());

        // Valid NIL_EXT directly into std::vector
        std::string nil_list;
        nil_list.push_back(static_cast<char>(glz::etf::magic_version));
        nil_list.push_back(static_cast<char>(glz::etf::tag::NIL_EXT));

        std::vector<int> vec2{1, 2};
        REQUIRE_FALSE(glz::read_etf(vec2, nil_list));
        CHECK(vec2.empty());

        // Valid MAP_EXT with arity 0
        std::string empty_map;
        empty_map.push_back(static_cast<char>(glz::etf::magic_version));
        empty_map.push_back(static_cast<char>(glz::etf::tag::MAP_EXT));
        append_be32(empty_map, 0);

        std::map<std::string, int> m{{"key", 123}};
        REQUIRE_FALSE(glz::read_etf(m, empty_map));
        CHECK(m.empty());

        test_etf_types::SampleStruct s{};
        s.name = "initial";
        REQUIRE_FALSE(glz::read_etf(s, empty_map));

        // Valid empty BINARY_EXT string
        std::string empty_bin;
        empty_bin.push_back(static_cast<char>(glz::etf::magic_version));
        empty_bin.push_back(static_cast<char>(glz::etf::tag::BINARY_EXT));
        append_be32(empty_bin, 0);

        std::string str{"not_empty"};
        REQUIRE_FALSE(glz::read_etf(str, empty_bin));
        CHECK(str.empty());
    }
}

TEST_CASE("ETF: Gateway payload base quick route and skipping payload data", "[etf][gateway]") {
    SECTION("Payload base extraction skips trailing corrupt or massive d without error") {
        GatewayBaseEvent evt{};
        evt.op = discusy::Opcode::Dispatch;
        evt.s = 42;
        evt.t = discusy::recieve_event::event::MESSAGE_CREATE;
        evt.d = "arbitrary payload";

        std::string encoded;
        REQUIRE_FALSE(glz::write_etf(evt, encoded));

        discusy::recieve_event::payload_base base_extracted{};
        REQUIRE_FALSE(discusy::etf::parse_payload_base(base_extracted, encoded));

        CHECK(base_extracted.op == discusy::Opcode::Dispatch);
        REQUIRE(base_extracted.s.has_value());
        CHECK(*base_extracted.s == 42);
        REQUIRE(base_extracted.t.has_value());
        CHECK(*base_extracted.t == discusy::recieve_event::event::MESSAGE_CREATE);
    }

    SECTION("Payload base extraction works when d is placed first (Erlang sorted order)") {
        ErlangSortedGatewayEvent evt{};
        std::string encoded;
        REQUIRE_FALSE(glz::write_etf(evt, encoded));

        discusy::recieve_event::payload_base base_extracted{};
        REQUIRE_FALSE(discusy::etf::parse_payload_base(base_extracted, encoded));

        CHECK(base_extracted.op == discusy::Opcode::Dispatch);
        REQUIRE(base_extracted.s.has_value());
        CHECK(*base_extracted.s == 777);
        REQUIRE(base_extracted.t.has_value());
        CHECK(*base_extracted.t == discusy::recieve_event::event::CHANNEL_CREATE);
    }

    SECTION("Non-dispatch gateway payload exits immediately upon reading op") {
        HelloGatewayEvent evt{};
        std::string encoded;
        REQUIRE_FALSE(glz::write_etf(evt, encoded));

        discusy::recieve_event::payload_base base_extracted{};
        REQUIRE_FALSE(discusy::etf::parse_payload_base(base_extracted, encoded));

        CHECK(base_extracted.op == discusy::Opcode::Hello);
        CHECK_FALSE(base_extracted.s.has_value());
        CHECK_FALSE(base_extracted.t.has_value());
    }

    SECTION("Partial read on reflectable struct terminates early") {
        FullPayload full{};
        std::string encoded_full;
        REQUIRE_FALSE(glz::write_etf(full, encoded_full));

        SubsetStruct subset{};
        auto ec = glz::read<glz::etf_opts_partial_read>(subset, encoded_full);
        REQUIRE_FALSE(static_cast<bool>(ec));
        CHECK(subset.name == "test_name");
        CHECK(subset.count == 42);
    }
}

TEST_CASE("ETF: Variant serialization and deserialization", "[etf][variants]") {
    SECTION("Primitive variant (std::variant<int, std::string>)") {
        using Var = std::variant<int, std::string>;

        Var v_int = 42;
        std::string encoded_int;
        REQUIRE_FALSE(glz::write_etf(v_int, encoded_int));

        Var decoded_int{};
        REQUIRE_FALSE(glz::read_etf(decoded_int, encoded_int));
        REQUIRE(std::holds_alternative<int>(decoded_int));
        CHECK(std::get<int>(decoded_int) == 42);

        Var v_str = std::string{"hello world"};
        std::string encoded_str;
        REQUIRE_FALSE(glz::write_etf(v_str, encoded_str));

        Var decoded_str{};
        REQUIRE_FALSE(glz::read_etf(decoded_str, encoded_str));
        REQUIRE(std::holds_alternative<std::string>(decoded_str));
        CHECK(std::get<std::string>(decoded_str) == "hello world");

        Var v_alphanum = std::string{"123hello"};
        std::string encoded_alphanum;
        REQUIRE_FALSE(glz::write_etf(v_alphanum, encoded_alphanum));

        Var decoded_alphanum{};
        REQUIRE_FALSE(glz::read_etf(decoded_alphanum, encoded_alphanum));
        REQUIRE(std::holds_alternative<std::string>(decoded_alphanum));
        CHECK(std::get<std::string>(decoded_alphanum) == "123hello");
    }

    SECTION("Numeric variant (std::variant<int, double>)") {
        using Var = std::variant<int, double>;

        Var v_int = 100;
        std::string encoded_int;
        REQUIRE_FALSE(glz::write_etf(v_int, encoded_int));

        Var decoded_int{};
        REQUIRE_FALSE(glz::read_etf(decoded_int, encoded_int));
        REQUIRE(std::holds_alternative<int>(decoded_int));
        CHECK(std::get<int>(decoded_int) == 100);

        Var v_dbl = 3.14159;
        std::string encoded_dbl;
        REQUIRE_FALSE(glz::write_etf(v_dbl, encoded_dbl));

        Var decoded_dbl{};
        REQUIRE_FALSE(glz::read_etf(decoded_dbl, encoded_dbl));
        REQUIRE(std::holds_alternative<double>(decoded_dbl));
        CHECK(std::get<double>(decoded_dbl) == 3.14159);
    }

    SECTION("Boolean and integer variant (std::variant<bool, int>)") {
        using Var = std::variant<bool, int>;

        Var v_true = true;
        std::string enc_true;
        REQUIRE_FALSE(glz::write_etf(v_true, enc_true));

        Var dec_true{};
        REQUIRE_FALSE(glz::read_etf(dec_true, enc_true));
        REQUIRE(std::holds_alternative<bool>(dec_true));
        CHECK(std::get<bool>(dec_true) == true);

        Var v_false = false;
        std::string enc_false;
        REQUIRE_FALSE(glz::write_etf(v_false, enc_false));

        Var dec_false{};
        REQUIRE_FALSE(glz::read_etf(dec_false, enc_false));
        REQUIRE(std::holds_alternative<bool>(dec_false));
        CHECK(std::get<bool>(dec_false) == false);

        Var v_int = 42;
        std::string enc_int;
        REQUIRE_FALSE(glz::write_etf(v_int, enc_int));

        Var dec_int{};
        REQUIRE_FALSE(glz::read_etf(dec_int, enc_int));
        REQUIRE(std::holds_alternative<int>(dec_int));
        CHECK(std::get<int>(dec_int) == 42);

        Var v_zero = 0;
        std::string enc_zero;
        REQUIRE_FALSE(glz::write_etf(v_zero, enc_zero));

        Var dec_zero{};
        REQUIRE_FALSE(glz::read_etf(dec_zero, enc_zero));
        REQUIRE(std::holds_alternative<int>(dec_zero));
        CHECK(std::get<int>(dec_zero) == 0);
    }

    SECTION("Variant with std::monostate") {
        using Var = std::variant<std::monostate, bool, int, std::string>;

        Var v_nil = std::monostate{};
        std::string enc_nil;
        REQUIRE_FALSE(glz::write_etf(v_nil, enc_nil));

        Var dec_nil{};
        REQUIRE_FALSE(glz::read_etf(dec_nil, enc_nil));
        CHECK(std::holds_alternative<std::monostate>(dec_nil));

        Var v_b = true;
        std::string enc_b;
        REQUIRE_FALSE(glz::write_etf(v_b, enc_b));

        Var dec_b{};
        REQUIRE_FALSE(glz::read_etf(dec_b, enc_b));
        REQUIRE(std::holds_alternative<bool>(dec_b));
        CHECK(std::get<bool>(dec_b) == true);

        Var v_i = 12345;
        std::string enc_i;
        REQUIRE_FALSE(glz::write_etf(v_i, enc_i));

        Var dec_i{};
        REQUIRE_FALSE(glz::read_etf(dec_i, enc_i));
        REQUIRE(std::holds_alternative<int>(dec_i));
        CHECK(std::get<int>(dec_i) == 12345);

        Var v_s = std::string{"hello ETF"};
        std::string enc_s;
        REQUIRE_FALSE(glz::write_etf(v_s, enc_s));

        Var dec_s{};
        REQUIRE_FALSE(glz::read_etf(dec_s, enc_s));
        REQUIRE(std::holds_alternative<std::string>(dec_s));
        CHECK(std::get<std::string>(dec_s) == "hello ETF");
    }

    SECTION("Variant with struct and primitive (std::variant<int, SampleStruct>)") {
        using Var = std::variant<int, SampleStruct>;

        Var v_int = 999;
        std::string enc_int;
        REQUIRE_FALSE(glz::write_etf(v_int, enc_int));

        Var dec_int{};
        REQUIRE_FALSE(glz::read_etf(dec_int, enc_int));
        REQUIRE(std::holds_alternative<int>(dec_int));
        CHECK(std::get<int>(dec_int) == 999);

        SampleStruct sample{};
        sample.id = 1234567890ULL;
        sample.name = "sample_name";
        sample.description = "sample_desc";
        sample.count = 7;
        sample.enabled = true;

        Var v_struct = sample;
        std::string enc_struct;
        REQUIRE_FALSE(glz::write_etf(v_struct, enc_struct));

        Var dec_struct{};
        REQUIRE_FALSE(glz::read_etf(dec_struct, enc_struct));
        REQUIRE(std::holds_alternative<SampleStruct>(dec_struct));
        CHECK(std::get<SampleStruct>(dec_struct) == sample);
    }

    SECTION("Variant member inside struct") {
        VariantHolder holder1{.label = "holder_int", .data = 123};
        std::string enc1;
        REQUIRE_FALSE(glz::write_etf(holder1, enc1));

        VariantHolder dec1{};
        REQUIRE_FALSE(glz::read_etf(dec1, enc1));
        CHECK(dec1.label == "holder_int");
        REQUIRE(std::holds_alternative<int>(dec1.data));
        CHECK(std::get<int>(dec1.data) == 123);

        VariantHolder holder2{.label = "holder_str", .data = std::string{"nested_value"}};
        std::string enc2;
        REQUIRE_FALSE(glz::write_etf(holder2, enc2));

        VariantHolder dec2{};
        REQUIRE_FALSE(glz::read_etf(dec2, enc2));
        CHECK(dec2.label == "holder_str");
        REQUIRE(std::holds_alternative<std::string>(dec2.data));
        CHECK(std::get<std::string>(dec2.data) == "nested_value");

        VariantHolder holder3{.label = "holder_bool", .data = true};
        std::string enc3;
        REQUIRE_FALSE(glz::write_etf(holder3, enc3));

        VariantHolder dec3{};
        REQUIRE_FALSE(glz::read_etf(dec3, enc3));
        CHECK(dec3.label == "holder_bool");
        REQUIRE(std::holds_alternative<bool>(dec3.data));
        CHECK(std::get<bool>(dec3.data) == true);
    }

    SECTION("Nested variants") {
        using Inner = std::variant<std::string, bool>;
        using Outer = std::variant<int, Inner>;

        Outer o1 = 10;
        std::string enc1;
        REQUIRE_FALSE(glz::write_etf(o1, enc1));

        Outer dec1{};
        REQUIRE_FALSE(glz::read_etf(dec1, enc1));
        REQUIRE(std::holds_alternative<int>(dec1));
        CHECK(std::get<int>(dec1) == 10);

        Outer o2 = Inner{std::string{"nested_text"}};
        std::string enc2;
        REQUIRE_FALSE(glz::write_etf(o2, enc2));

        Outer dec2{};
        REQUIRE_FALSE(glz::read_etf(dec2, enc2));
        REQUIRE(std::holds_alternative<Inner>(dec2));
        REQUIRE(std::holds_alternative<std::string>(std::get<Inner>(dec2)));
        CHECK(std::get<std::string>(std::get<Inner>(dec2)) == "nested_text");

        Outer o3 = Inner{false};
        std::string enc3;
        REQUIRE_FALSE(glz::write_etf(o3, enc3));

        Outer dec3{};
        REQUIRE_FALSE(glz::read_etf(dec3, enc3));
        REQUIRE(std::holds_alternative<Inner>(dec3));
        REQUIRE(std::holds_alternative<bool>(std::get<Inner>(dec3)));
        CHECK(std::get<bool>(std::get<Inner>(dec3)) == false);
    }

    SECTION("Vector of heterogeneous variants") {
        using Var = std::variant<int, std::string, bool>;
        std::vector<Var> list{
            42,
            std::string{"apple"},
            true,
            100,
            false,
            std::string{"banana"},
        };

        std::string enc;
        REQUIRE_FALSE(glz::write_etf(list, enc));

        std::vector<Var> dec{};
        REQUIRE_FALSE(glz::read_etf(dec, enc));
        REQUIRE(dec.size() == 6);

        REQUIRE(std::holds_alternative<int>(dec[0]));
        CHECK(std::get<int>(dec[0]) == 42);

        REQUIRE(std::holds_alternative<std::string>(dec[1]));
        CHECK(std::get<std::string>(dec[1]) == "apple");

        REQUIRE(std::holds_alternative<bool>(dec[2]));
        CHECK(std::get<bool>(dec[2]) == true);

        REQUIRE(std::holds_alternative<int>(dec[3]));
        CHECK(std::get<int>(dec[3]) == 100);

        REQUIRE(std::holds_alternative<bool>(dec[4]));
        CHECK(std::get<bool>(dec[4]) == false);

        REQUIRE(std::holds_alternative<std::string>(dec[5]));
        CHECK(std::get<std::string>(dec[5]) == "banana");
    }

    SECTION("Structural variant deduction between object types") {
        using TestVar = std::variant<glz::skip, test_etf_types::StructA, test_etf_types::StructB>;

        test_etf_types::StructB b{.id = discusy::snowflake{123}, .custom_id = "btn_test", .component_type = 2};
        std::string enc_b;
        REQUIRE_FALSE(glz::write_etf(b, enc_b));

        TestVar dec_b{};
        REQUIRE_FALSE(glz::read_etf(dec_b, enc_b));
        REQUIRE(std::holds_alternative<test_etf_types::StructB>(dec_b));
        CHECK(std::get<test_etf_types::StructB>(dec_b).custom_id == "btn_test");
        CHECK(std::get<test_etf_types::StructB>(dec_b).component_type == 2);

        StructA a{.id = discusy::snowflake{456}, .name = "command_test"};
        std::string enc_a;
        REQUIRE_FALSE(glz::write_etf(a, enc_a));

        TestVar dec_a{};
        REQUIRE_FALSE(glz::read_etf(dec_a, enc_a));
        REQUIRE(std::holds_alternative<StructA>(dec_a));
        CHECK(std::get<StructA>(dec_a).name == "command_test");
    }

    SECTION("Error on variant with no matching type") {
        using Var = std::variant<int, double>;

        // Encode a string which cannot match int or double
        std::string str_val = "not a number";
        std::string enc;
        REQUIRE_FALSE(glz::write_etf(str_val, enc));

        Var dec{};
        auto ec = glz::read_etf(dec, enc);
        REQUIRE(static_cast<bool>(ec));
        CHECK(ec.ec == glz::error_code::no_matching_variant_type);
    }
}

TEST_CASE("ETF: Deep Discord Interaction gateway event payload", "[etf][gateway][interaction]") {
    // 1. Build a deep and complete Discord interaction payload
    discusy::interaction::interaction inter{};
    inter.id = discusy::snowflake{112233445566778899ULL};
    inter.application_id = discusy::snowflake{998877665544332211ULL};
    inter.type = discusy::interaction::interaction_type::APPLICATION_COMMAND;
    inter.token = "deep_interaction_secure_token_abcdef1234567890";
    inter.version = 1;
    inter.guild_id = discusy::snowflake{123123123123123123ULL};
    inter.channel_id = discusy::snowflake{456456456456456456ULL};
    inter.locale = "en-US";
    inter.guild_locale = "en-GB";
    inter.context = discusy::interaction::interaction_context_type::GUILD;
    inter.attachment_size_limit = 26214400;
    inter.add_app_permission(discusy::permissions::permissions::SEND_MESSAGES);
    inter.add_app_permission(discusy::permissions::permissions::VIEW_CHANNEL);

    // User details
    discusy::user::user u{};
    u.id = discusy::snowflake{789789789789789789ULL};
    u.username = "bot_tester";
    u.discriminator = "0001";
    u.global_name = "Bot Tester";
    u.bot = false;
    inter.user = u;

    // Guild member details
    discusy::guild::guild_member m{};
    m.user = u;
    m.nick = "Commander Tester";
    m.roles = {discusy::snowflake{101010101010101010ULL}, discusy::snowflake{202020202020202020ULL}};
    m.deaf = false;
    m.mute = false;
    inter.member = m;

    // Entitlements
    discusy::entitlement::entitlement ent{};
    ent.id = discusy::snowflake{303030303030303030ULL};
    ent.sku_id = discusy::snowflake{404040404040404040ULL};
    ent.application_id = inter.application_id;
    ent.type = discusy::entitlement::entitlement_type::APPLICATION_SUBSCRIPTION;
    ent.deleted = false;
    inter.entitlements.push_back(ent);

    // Authorizing integration owners
    inter.add_authorizing_integration_owner(discusy::application::application_integration_type::GUILD_INSTALL, *inter.guild_id);

    // Deep Command Data with 3-level option hierarchy
    discusy::interaction::application_command_data cmd_data{};
    cmd_data.id = discusy::snowflake{505050505050505050ULL};
    cmd_data.name = "admin";
    cmd_data.type = discusy::application_commands::application_command_type::CHAT_INPUT;
    cmd_data.guild_id = inter.guild_id;

    // Leaf Options (Level 3):
    discusy::interaction::application_command_interaction_data_option opt_host{};
    opt_host.name = "host";
    opt_host.type = discusy::application_commands::application_command_option_type::STRING;
    opt_host.value = std::string{"gateway.internal.net"};

    discusy::interaction::application_command_interaction_data_option opt_port{};
    opt_port.name = "port";
    opt_port.type = discusy::application_commands::application_command_option_type::INTEGER;
    opt_port.value = discusy::integer{8443};

    discusy::interaction::application_command_interaction_data_option opt_ratio{};
    opt_ratio.name = "sample_ratio";
    opt_ratio.type = discusy::application_commands::application_command_option_type::NUMBER;
    opt_ratio.value = 0.875;

    discusy::interaction::application_command_interaction_data_option opt_enabled{};
    opt_enabled.name = "ssl_enabled";
    opt_enabled.type = discusy::application_commands::application_command_option_type::BOOLEAN;
    opt_enabled.value = true;
    opt_enabled.focused = false;

    // Subcommand (Level 2):
    discusy::interaction::application_command_interaction_data_option sub_cmd{};
    sub_cmd.name = "configure";
    sub_cmd.type = discusy::application_commands::application_command_option_type::SUB_COMMAND;
    sub_cmd.options = std::vector<discusy::interaction::application_command_interaction_data_option>{
        opt_host, opt_port, opt_ratio, opt_enabled,
    };

    // Subcommand Group (Level 1):
    discusy::interaction::application_command_interaction_data_option sub_group{};
    sub_group.name = "system";
    sub_group.type = discusy::application_commands::application_command_option_type::SUB_COMMAND_GROUP;
    sub_group.options = std::vector<discusy::interaction::application_command_interaction_data_option>{sub_cmd};

    cmd_data.options = std::vector<discusy::interaction::application_command_interaction_data_option>{sub_group};

    // Resolved payload data
    discusy::interaction::resolved res{};
    res.users.emplace();
    (*res.users)[u.id.value] = u;

    res.members.emplace();
    (*res.members)[u.id.value] = m;

    res.roles.emplace();
    discusy::permissions::role r{};
    r.id = discusy::snowflake{101010101010101010ULL};
    r.name = "AdminRole";
    r.color = 0xFF0000;
    r.hoist = true;
    r.permissions.add_flag(discusy::permissions::permissions::ADMINISTRATOR);
    (*res.roles)[r.id.value] = r;

    res.channels.emplace();
    discusy::channel::channel ch{};
    ch.id = *inter.channel_id;
    ch.type = discusy::channel::channel_type::GUILD_TEXT;
    ch.name = "bot-commands";
    ch.guild_id = inter.guild_id;
    (*res.channels)[ch.id.value] = ch;

    cmd_data.resolved = res;
    inter.data = cmd_data;

    SECTION("Standard gateway dispatch order (op, s, t, d)") {
        test_etf_types::FullGatewayEvent<discusy::recieve_event::interaction_create> full_event{};
        full_event.op = discusy::Opcode::Dispatch;
        full_event.s = 98765;
        full_event.t = discusy::recieve_event::event::INTERACTION_CREATE;
        full_event.d = inter;

        std::string encoded_etf;
        REQUIRE_FALSE(glz::write_etf(full_event, encoded_etf));
        REQUIRE_FALSE(encoded_etf.empty());

        // Fast payload base extraction (skip massive d)
        discusy::recieve_event::payload_base base_event{};
        REQUIRE_FALSE(discusy::etf::parse_payload_base(base_event, encoded_etf));
        CHECK(base_event.op == discusy::Opcode::Dispatch);
        REQUIRE(base_event.s.has_value());
        CHECK(*base_event.s == 98765);
        REQUIRE(base_event.t.has_value());
        CHECK(*base_event.t == discusy::recieve_event::event::INTERACTION_CREATE);

        // Discusy shard dispatch pathway
        discusy::recieve_event::payload<discusy::recieve_event::interaction_create> dispatch_payload{};
        REQUIRE_FALSE(discusy::etf::parse_etf(dispatch_payload, encoded_etf));
        CHECK(dispatch_payload.d.id == inter.id);
        CHECK(dispatch_payload.d.token == inter.token);

        // Full gateway event read
        test_etf_types::FullGatewayEvent<discusy::recieve_event::interaction_create> decoded_event{};
        REQUIRE_FALSE(glz::read_etf(decoded_event, encoded_etf));

        CHECK(decoded_event.op == discusy::Opcode::Dispatch);
        REQUIRE(decoded_event.s.has_value());
        CHECK(*decoded_event.s == 98765);
        REQUIRE(decoded_event.t.has_value());
        CHECK(*decoded_event.t == discusy::recieve_event::event::INTERACTION_CREATE);

        const auto& dec_inter = decoded_event.d;
        CHECK(dec_inter.id == discusy::snowflake{112233445566778899ULL});
        CHECK(dec_inter.application_id == discusy::snowflake{998877665544332211ULL});
        CHECK(dec_inter.type == discusy::interaction::interaction_type::APPLICATION_COMMAND);
        CHECK(dec_inter.token == "deep_interaction_secure_token_abcdef1234567890");
        CHECK(dec_inter.version == 1);
        REQUIRE(dec_inter.guild_id.has_value());
        CHECK(*dec_inter.guild_id == discusy::snowflake{123123123123123123ULL});
        REQUIRE(dec_inter.channel_id.has_value());
        CHECK(*dec_inter.channel_id == discusy::snowflake{456456456456456456ULL});
        REQUIRE(dec_inter.locale.has_value());
        CHECK(*dec_inter.locale == "en-US");
        REQUIRE(dec_inter.guild_locale.has_value());
        CHECK(*dec_inter.guild_locale == "en-GB");
        REQUIRE(dec_inter.context.has_value());
        CHECK(*dec_inter.context == discusy::interaction::interaction_context_type::GUILD);
        CHECK(dec_inter.attachment_size_limit == 26214400);
        CHECK(dec_inter.app_permissions.has_flag(discusy::permissions::permissions::SEND_MESSAGES));
        CHECK(dec_inter.app_permissions.has_flag(discusy::permissions::permissions::VIEW_CHANNEL));
        CHECK_FALSE(dec_inter.app_permissions.has_flag(discusy::permissions::permissions::BAN_MEMBERS));

        // User
        REQUIRE(dec_inter.user.has_value());
        CHECK(dec_inter.user->id == discusy::snowflake{789789789789789789ULL});
        CHECK(dec_inter.user->username == "bot_tester");
        CHECK(dec_inter.user->discriminator == "0001");
        REQUIRE(dec_inter.user->global_name.has_value());
        CHECK(*dec_inter.user->global_name == "Bot Tester");
        REQUIRE(dec_inter.user->bot.has_value());
        CHECK(*dec_inter.user->bot == false);

        // Member
        REQUIRE(dec_inter.member.has_value());
        REQUIRE(dec_inter.member->nick.has_value());
        CHECK(*dec_inter.member->nick == "Commander Tester");
        REQUIRE(dec_inter.member->roles.size() == 2);
        CHECK(dec_inter.member->roles[0] == discusy::snowflake{101010101010101010ULL});
        CHECK(dec_inter.member->roles[1] == discusy::snowflake{202020202020202020ULL});
        REQUIRE(dec_inter.member->deaf.has_value());
        CHECK(*dec_inter.member->deaf == false);
        REQUIRE(dec_inter.member->mute.has_value());
        CHECK(*dec_inter.member->mute == false);
        REQUIRE(dec_inter.member->user.has_value());
        CHECK(dec_inter.member->user->username == "bot_tester");

        // Entitlements
        REQUIRE(dec_inter.entitlements.size() == 1);
        CHECK(dec_inter.entitlements[0].id == discusy::snowflake{303030303030303030ULL});
        CHECK(dec_inter.entitlements[0].sku_id == discusy::snowflake{404040404040404040ULL});
        CHECK(dec_inter.entitlements[0].application_id == dec_inter.application_id);
        CHECK(dec_inter.entitlements[0].type == discusy::entitlement::entitlement_type::APPLICATION_SUBSCRIPTION);
        CHECK(dec_inter.entitlements[0].deleted == false);

        // Integration owners
        REQUIRE(dec_inter.authorizing_integration_owners.size() == 1);
        auto it_owner = dec_inter.authorizing_integration_owners.find(discusy::application::application_integration_type::GUILD_INSTALL);
        REQUIRE(it_owner != dec_inter.authorizing_integration_owners.end());
        CHECK(it_owner->second == discusy::snowflake{123123123123123123ULL});

        // Command Data
        REQUIRE(dec_inter.data.has_value());
        REQUIRE(std::holds_alternative<discusy::interaction::application_command_data>(*dec_inter.data));
        const auto& dec_cmd = std::get<discusy::interaction::application_command_data>(*dec_inter.data);
        CHECK(dec_cmd.id == discusy::snowflake{505050505050505050ULL});
        CHECK(dec_cmd.name == "admin");
        CHECK(dec_cmd.type == discusy::application_commands::application_command_type::CHAT_INPUT);
        REQUIRE(dec_cmd.guild_id.has_value());
        CHECK(*dec_cmd.guild_id == discusy::snowflake{123123123123123123ULL});

        // Hierarchy Level 1 (SUB_COMMAND_GROUP)
        REQUIRE(dec_cmd.options.has_value());
        REQUIRE(dec_cmd.options->size() == 1);
        const auto& dec_group = (*dec_cmd.options)[0];
        CHECK(dec_group.name == "system");
        CHECK(dec_group.type == discusy::application_commands::application_command_option_type::SUB_COMMAND_GROUP);

        // Hierarchy Level 2 (SUB_COMMAND)
        REQUIRE(dec_group.options.has_value());
        REQUIRE(dec_group.options->size() == 1);
        const auto& dec_sub = (*dec_group.options)[0];
        CHECK(dec_sub.name == "configure");
        CHECK(dec_sub.type == discusy::application_commands::application_command_option_type::SUB_COMMAND);

        // Hierarchy Level 3 (Leaf options)
        REQUIRE(dec_sub.options.has_value());
        REQUIRE(dec_sub.options->size() == 4);

        // String option
        const auto& dec_host = (*dec_sub.options)[0];
        CHECK(dec_host.name == "host");
        CHECK(dec_host.type == discusy::application_commands::application_command_option_type::STRING);
        REQUIRE(dec_host.value.has_value());
        REQUIRE(std::holds_alternative<std::string>(*dec_host.value));
        CHECK(std::get<std::string>(*dec_host.value) == "gateway.internal.net");
        REQUIRE(dec_host.value_as<std::string>().has_value());
        CHECK(*dec_host.value_as<std::string>() == "gateway.internal.net");

        // Integer option
        const auto& dec_port = (*dec_sub.options)[1];
        CHECK(dec_port.name == "port");
        CHECK(dec_port.type == discusy::application_commands::application_command_option_type::INTEGER);
        REQUIRE(dec_port.value.has_value());
        REQUIRE(std::holds_alternative<discusy::integer>(*dec_port.value));
        CHECK(std::get<discusy::integer>(*dec_port.value) == 8443);
        REQUIRE(dec_port.value_as<discusy::integer>().has_value());
        CHECK(*dec_port.value_as<discusy::integer>() == 8443);

        // Number option
        const auto& dec_ratio = (*dec_sub.options)[2];
        CHECK(dec_ratio.name == "sample_ratio");
        CHECK(dec_ratio.type == discusy::application_commands::application_command_option_type::NUMBER);
        REQUIRE(dec_ratio.value.has_value());
        REQUIRE(std::holds_alternative<double>(*dec_ratio.value));
        CHECK(std::get<double>(*dec_ratio.value) == 0.875);
        REQUIRE(dec_ratio.value_as<double>().has_value());
        CHECK(*dec_ratio.value_as<double>() == 0.875);

        // Boolean option
        const auto& dec_enabled = (*dec_sub.options)[3];
        CHECK(dec_enabled.name == "ssl_enabled");
        CHECK(dec_enabled.type == discusy::application_commands::application_command_option_type::BOOLEAN);
        REQUIRE(dec_enabled.value.has_value());
        REQUIRE(std::holds_alternative<bool>(*dec_enabled.value));
        CHECK(std::get<bool>(*dec_enabled.value) == true);
        REQUIRE(dec_enabled.value_as<bool>().has_value());
        CHECK(*dec_enabled.value_as<bool>() == true);
        REQUIRE(dec_enabled.focused.has_value());
        CHECK(*dec_enabled.focused == false);

        // Resolved payload data
        REQUIRE(dec_cmd.resolved.has_value());
        const auto& dec_res = *dec_cmd.resolved;

        REQUIRE(dec_res.users.has_value());
        auto it_u = dec_res.users->find(u.id.value);
        REQUIRE(it_u != dec_res.users->end());
        CHECK(it_u->second.username == "bot_tester");
        CHECK(it_u->second.discriminator == "0001");

        REQUIRE(dec_res.members.has_value());
        auto it_m = dec_res.members->find(u.id.value);
        REQUIRE(it_m != dec_res.members->end());
        REQUIRE(it_m->second.nick.has_value());
        CHECK(*it_m->second.nick == "Commander Tester");

        REQUIRE(dec_res.roles.has_value());
        auto it_r = dec_res.roles->find(r.id.value);
        REQUIRE(it_r != dec_res.roles->end());
        CHECK(it_r->second.name == "AdminRole");
        CHECK(it_r->second.color == 0xFF0000);
        CHECK(it_r->second.hoist == true);
        CHECK(it_r->second.permissions.has_flag(discusy::permissions::permissions::ADMINISTRATOR));

        REQUIRE(dec_res.channels.has_value());
        auto it_ch = dec_res.channels->find(ch.id.value);
        REQUIRE(it_ch != dec_res.channels->end());
        CHECK(it_ch->second.name == "bot-commands");
        CHECK(it_ch->second.type == discusy::channel::channel_type::GUILD_TEXT);
    }

    SECTION("Erlang sorted gateway dispatch order (d field first)") {
        test_etf_types::ErlangSortedFullGatewayEvent<discusy::recieve_event::interaction_create> sorted_event{};
        sorted_event.d = inter;
        sorted_event.op = discusy::Opcode::Dispatch;
        sorted_event.s = 98766;
        sorted_event.t = discusy::recieve_event::event::INTERACTION_CREATE;

        std::string encoded_etf;
        REQUIRE_FALSE(glz::write_etf(sorted_event, encoded_etf));
        REQUIRE_FALSE(encoded_etf.empty());

        // Fast payload base extraction when d comes first: verifies huge d is skipped and op/s/t extracted
        discusy::recieve_event::payload_base base_event{};
        REQUIRE_FALSE(discusy::etf::parse_payload_base(base_event, encoded_etf));
        CHECK(base_event.op == discusy::Opcode::Dispatch);
        REQUIRE(base_event.s.has_value());
        CHECK(*base_event.s == 98766);
        REQUIRE(base_event.t.has_value());
        CHECK(*base_event.t == discusy::recieve_event::event::INTERACTION_CREATE);

        // Full gateway event read
        test_etf_types::ErlangSortedFullGatewayEvent<discusy::recieve_event::interaction_create> decoded_sorted{};
        REQUIRE_FALSE(glz::read_etf(decoded_sorted, encoded_etf));

        CHECK(decoded_sorted.op == discusy::Opcode::Dispatch);
        REQUIRE(decoded_sorted.s.has_value());
        CHECK(*decoded_sorted.s == 98766);
        REQUIRE(decoded_sorted.t.has_value());
        CHECK(*decoded_sorted.t == discusy::recieve_event::event::INTERACTION_CREATE);

        CHECK(decoded_sorted.d.id == discusy::snowflake{112233445566778899ULL});
        CHECK(decoded_sorted.d.token == "deep_interaction_secure_token_abcdef1234567890");
        REQUIRE(decoded_sorted.d.data.has_value());
        REQUIRE(std::holds_alternative<discusy::interaction::application_command_data>(*decoded_sorted.d.data));
        const auto& dec_cmd = std::get<discusy::interaction::application_command_data>(*decoded_sorted.d.data);
        CHECK(dec_cmd.name == "admin");
        REQUIRE(dec_cmd.options.has_value());
        REQUIRE(dec_cmd.options->size() == 1);
        CHECK((*dec_cmd.options)[0].name == "system");
    }
}

TEST_CASE("ETF: Component and Modal Interaction payloads and when-handler matching", "[etf][gateway][interaction][component]") {
    SECTION("Message Component button click interaction") {
        discusy::interaction::interaction inter{};
        inter.id = discusy::snowflake{1122334455667788ULL};
        inter.application_id = discusy::snowflake{9988776655443322ULL};
        inter.type = discusy::interaction::interaction_type::MESSAGE_COMPONENT;
        inter.token = "btn_click_secure_token_12345";
        inter.version = 1;
        inter.data = discusy::interaction::message_component_data::create(
            "btn_primary42",
            discusy::components::component_type::Button
        );

        test_etf_types::FullGatewayEvent<discusy::recieve_event::interaction_create> gateway_event{};
        gateway_event.op = discusy::Opcode::Dispatch;
        gateway_event.s = 1001;
        gateway_event.t = discusy::recieve_event::event::INTERACTION_CREATE;
        gateway_event.d = inter;

        std::string encoded_etf;
        REQUIRE_FALSE(glz::write_etf(gateway_event, encoded_etf));
        REQUIRE_FALSE(encoded_etf.empty());

        // Fast payload base check
        discusy::recieve_event::payload_base base_event{};
        REQUIRE_FALSE(discusy::etf::parse_payload_base(base_event, encoded_etf));
        CHECK(base_event.op == discusy::Opcode::Dispatch);
        REQUIRE(base_event.t.has_value());
        CHECK(*base_event.t == discusy::recieve_event::event::INTERACTION_CREATE);

        // Discusy shard dispatch payload parsing
        discusy::recieve_event::payload<discusy::recieve_event::interaction_create> dispatch_payload{};
        REQUIRE_FALSE(discusy::etf::parse_etf(dispatch_payload, encoded_etf));

        const auto& e = dispatch_payload.d;
        CHECK(e.type == discusy::interaction::interaction_type::MESSAGE_COMPONENT);

        REQUIRE(e.data.has_value());
        REQUIRE(std::holds_alternative<discusy::interaction::message_component_data>(*e.data));

        const auto& comp_data = std::get<discusy::interaction::message_component_data>(*e.data);
        CHECK(comp_data.custom_id == "btn_primary42");
        CHECK(comp_data.component_type == discusy::components::component_type::Button);

        // Verification of when(...) predicate compatibility
        const std::string_view custom_id{comp_data.custom_id};
        CHECK(custom_id == "btn_primary42");
        CHECK(custom_id.starts_with("btn_"));
        CHECK(custom_id.ends_with("42"));
    }

    SECTION("Modal Submit interaction") {
        discusy::interaction::interaction inter{};
        inter.id = discusy::snowflake{2233445566778899ULL};
        inter.application_id = discusy::snowflake{9988776655443322ULL};
        inter.type = discusy::interaction::interaction_type::MODAL_SUBMIT;
        inter.token = "modal_submit_secure_token_67890";
        inter.version = 1;
        inter.data = discusy::interaction::modal_component_data::create(
            "feedback_modal_99",
            {}
        );

        test_etf_types::FullGatewayEvent<discusy::recieve_event::interaction_create> gateway_event{};
        gateway_event.op = discusy::Opcode::Dispatch;
        gateway_event.s = 1002;
        gateway_event.t = discusy::recieve_event::event::INTERACTION_CREATE;
        gateway_event.d = inter;

        std::string encoded_etf;
        REQUIRE_FALSE(glz::write_etf(gateway_event, encoded_etf));
        REQUIRE_FALSE(encoded_etf.empty());

        discusy::recieve_event::payload<discusy::recieve_event::interaction_create> dispatch_payload{};
        REQUIRE_FALSE(discusy::etf::parse_etf(dispatch_payload, encoded_etf));

        const auto& e = dispatch_payload.d;
        CHECK(e.type == discusy::interaction::interaction_type::MODAL_SUBMIT);

        REQUIRE(e.data.has_value());
        REQUIRE(std::holds_alternative<discusy::interaction::modal_component_data>(*e.data));

        const auto& modal_data = std::get<discusy::interaction::modal_component_data>(*e.data);
        CHECK(modal_data.custom_id == "feedback_modal_99");
        const std::string_view modal_id{modal_data.custom_id};
        CHECK(modal_id == "feedback_modal_99");
    }
}

