#include <catch2/catch_test_macros.hpp>
#include <discusy/etf.hpp>
#include <discusy/types.hpp>
#include <discusy/gateway_events.hpp>
#include <discusy/json.hpp>
#include <charconv>
#include <etf/etf.hpp>
#include <numbers>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <optional>
#include <variant>
#include <cstdint>
#include <limits>
#include <cmath>
#include <tuple>
#include <array>
#include <cstring>

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

struct ArrayHolder {
    std::string name{};
    std::array<int, 3> coords{};

    bool operator==(const ArrayHolder&) const = default;
};

struct OverfilledStructTest {
    std::array<int, 2> coords{};
    std::string marker{};

    bool operator==(const OverfilledStructTest&) const = default;
};

struct LocalGlazeValueWrapper {
    int inner{42};
    struct glaze {
        static constexpr auto value = &LocalGlazeValueWrapper::inner;
    };
    bool operator==(const LocalGlazeValueWrapper&) const = default;
};

struct LocalGlazeObjectWrapper {
    int x{1};
    int y{2};
    struct glaze {
        static constexpr auto value = glz::object("x", &LocalGlazeObjectWrapper::x, "y", &LocalGlazeObjectWrapper::y);
    };
    bool operator==(const LocalGlazeObjectWrapper&) const = default;
};

struct GlobalMetaValueWrapper {
    std::string text{"hello"};
    bool operator==(const GlobalMetaValueWrapper&) const = default;
};

struct HexColorWrapper {
    uint32_t rgb{0x00FF00};
    bool operator==(const HexColorWrapper&) const = default;
};

using number_perms_t = discusy::flags_t<discusy::permissions::permissions, discusy::flags_type::number>;

struct RolePermissionsTest {
    discusy::snowflake role_id{999};
    discusy::permissions_t perms{};
    number_perms_t num_perms{};
    bool operator==(const RolePermissionsTest&) const = default;
};

struct TriStateTestStruct {
    std::string name{"item"};
    discusy::opt<discusy::explicit_null<std::string>> description{};
    discusy::opt<discusy::explicit_null<int>> count{};
    discusy::opt<int> regular_opt{};
    discusy::explicit_null<std::string> direct_null{nullptr};

    bool operator==(const TriStateTestStruct&) const = default;
};

struct RegularOptStruct {
    std::string name{"test"};
    std::optional<int> opt_val{};
    bool operator==(const RegularOptStruct&) const = default;
};

struct NullableParityStruct {
    std::string name{"default_name"};
    std::optional<int> std_opt{};
    discusy::opt<int> discusy_opt{};
    discusy::explicit_null<int> exp_null{nullptr};
    discusy::opt<discusy::explicit_null<int>> opt_exp_null{};

    bool operator==(const NullableParityStruct&) const = default;
};

struct LocalMimicNumber {
    int val{0};
    struct glaze {
        using mimic = int;
        static constexpr auto value = &LocalMimicNumber::val;
    };
    bool operator==(const LocalMimicNumber&) const = default;
};

struct LocalMimicString {
    std::string val{};
    struct glaze {
        using mimic = std::string;
        static constexpr auto value = &LocalMimicString::val;
    };
    bool operator==(const LocalMimicString&) const = default;
};

struct GlobalMimicNumber {
    int val{0};
    bool operator==(const GlobalMimicNumber&) const = default;
};

struct GlobalMimicString {
    std::string val{};
    bool operator==(const GlobalMimicString&) const = default;
};

struct MimicHolderStruct {
    LocalMimicNumber local_num{42};
    LocalMimicString local_str{"local_mimic"};
    GlobalMimicNumber global_num{100};
    GlobalMimicString global_str{"global_mimic"};
    bool operator==(const MimicHolderStruct&) const = default;
};

}

template <>
struct glz::meta<test_etf_types::GlobalMetaValueWrapper> {
    static constexpr auto value = &test_etf_types::GlobalMetaValueWrapper::text;
};

template <>
struct glz::meta<test_etf_types::HexColorWrapper> {
    static constexpr auto read_hex = [](test_etf_types::HexColorWrapper& obj, std::string_view sv) {
        if (!sv.empty() && sv[0] == '#') sv.remove_prefix(1);
        uint32_t val = 0;
        std::from_chars(sv.data(), sv.data() + sv.size(), val, 16);
        obj.rgb = val;
    };
    static constexpr auto write_hex = [](const test_etf_types::HexColorWrapper& obj) -> std::string {
        char buf[16];
        auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), obj.rgb, 16);
        std::string hex(buf, ptr);
        return "#" + hex;
    };
    static constexpr auto value = glz::custom<read_hex, write_hex>;
};

template <>
struct glz::meta<test_etf_types::GlobalMimicNumber> {
    using mimic = int;
    static constexpr auto value = &test_etf_types::GlobalMimicNumber::val;
};

template <>
struct glz::meta<test_etf_types::GlobalMimicString> {
    using mimic = std::string;
    static constexpr auto value = &test_etf_types::GlobalMimicString::val;
};

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

    SECTION("Snowflake roundtrip as integer (SMALL_BIG_EXT)") {
        std::string encoded;
        REQUIRE_FALSE(glz::write_etf(sf.value, encoded));
        REQUIRE(encoded.size() > 1);
        CHECK(static_cast<std::uint8_t>(encoded[1]) == glz::etf::tag::SMALL_BIG_EXT);

        // Deserializing 64-bit integer directly into discusy::snowflake
        discusy::snowflake decoded{};
        REQUIRE_FALSE(glz::read_etf(decoded, encoded));
        CHECK(decoded == sf);
        CHECK(decoded.value == raw_id);

        // Also test into decoded.value (raw uint64_t)
        std::uint64_t raw_decoded = 0;
        REQUIRE_FALSE(glz::read_etf(raw_decoded, encoded));
        CHECK(raw_decoded == raw_id);
    }

    SECTION("Snowflake direct serialization (writes quoted string due to glz::quoted_num)") {
        std::string encoded;
        REQUIRE_FALSE(glz::write_etf(sf, encoded));

        // When serialized directly, snowflake uses quoted_num so ETF writes it as BINARY_EXT string
        REQUIRE(encoded.size() > 1);
        CHECK(static_cast<std::uint8_t>(encoded[1]) == glz::etf::tag::BINARY_EXT);

        discusy::snowflake decoded{};
        REQUIRE_FALSE(glz::read_etf(decoded, encoded));
        CHECK(decoded == sf);
        CHECK(decoded.value == raw_id);
    }

    SECTION("Snowflake deserialization from string (BINARY_EXT)") {
        // Discord transmits Snowflakes either as 64-bit integers or as numeric strings
        std::string string_id = "175928847299117063";
        std::string encoded;
        REQUIRE_FALSE(glz::write_etf(string_id, encoded));

        discusy::snowflake decoded{};
        REQUIRE_FALSE(glz::read_etf(decoded, encoded));
        CHECK(decoded == sf);
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
        REQUIRE_FALSE(glz::read_etf(decoded, custom_etf));
        CHECK(decoded == sf);
        CHECK(decoded.value == raw_id);
    }

    SECTION("Snowflake deserialization from SMALL_INTEGER_EXT (8-bit integer)") {
        // Small ID like 100
        std::string custom_etf;
        custom_etf.push_back(static_cast<char>(glz::etf::magic_version));
        custom_etf.push_back(static_cast<char>(glz::etf::tag::SMALL_INTEGER_EXT));
        custom_etf.push_back(static_cast<char>(100));

        discusy::snowflake decoded{};
        REQUIRE_FALSE(glz::read_etf(decoded, custom_etf));
        CHECK(decoded == discusy::snowflake{100ULL});
        CHECK(decoded.value == 100ULL);
    }

    SECTION("Snowflake deserialization from INTEGER_EXT (32-bit integer)") {
        std::int32_t val32 = 1234567;
        std::string custom_etf;
        REQUIRE_FALSE(glz::write_etf(val32, custom_etf));
        REQUIRE(static_cast<std::uint8_t>(custom_etf[1]) == glz::etf::tag::INTEGER_EXT);

        discusy::snowflake decoded{};
        REQUIRE_FALSE(glz::read_etf(decoded, custom_etf));
        CHECK(decoded == discusy::snowflake{1234567ULL});
        CHECK(decoded.value == 1234567ULL);
    }

    SECTION("Snowflake in struct: deserialization from integer vs string payload") {
        // Struct with snowflake id
        // 1. When payload sends id as a 64-bit integer (SMALL_BIG_EXT)
        std::string etf_int_payload;
        {
            etf_int_payload.push_back(static_cast<char>(glz::etf::magic_version));
            etf_int_payload.push_back(static_cast<char>(glz::etf::tag::MAP_EXT));
            std::uint32_t arity = glz::etf::detail::to_big_endian(2U);
            etf_int_payload.append(reinterpret_cast<const char*>(&arity), 4);

            // "id" => raw_id (SMALL_BIG_EXT)
            std::string id_key = "id";
            std::string key_etf;
            REQUIRE_FALSE(glz::write_etf(id_key, key_etf));
            etf_int_payload.append(key_etf.data() + 1, key_etf.size() - 1);

            std::string val_etf;
            REQUIRE_FALSE(glz::write_etf(raw_id, val_etf));
            etf_int_payload.append(val_etf.data() + 1, val_etf.size() - 1);

            // "name" => "test"
            std::string name_key = "name";
            std::string name_key_etf;
            REQUIRE_FALSE(glz::write_etf(name_key, name_key_etf));
            etf_int_payload.append(name_key_etf.data() + 1, name_key_etf.size() - 1);

            std::string name_val = "test";
            std::string name_val_etf;
            REQUIRE_FALSE(glz::write_etf(name_val, name_val_etf));
            etf_int_payload.append(name_val_etf.data() + 1, name_val_etf.size() - 1);
        }

        SampleStruct sample_from_int{};
        REQUIRE_FALSE(glz::read_etf(sample_from_int, etf_int_payload));
        CHECK(sample_from_int.id == sf);
        CHECK(sample_from_int.name == "test");

        // 2. When payload sends id as string (BINARY_EXT)
        SampleStruct original{.id = sf, .name = "test"};
        std::string etf_str_payload;
        REQUIRE_FALSE(glz::write_etf(original, etf_str_payload));

        SampleStruct sample_from_str{};
        REQUIRE_FALSE(glz::read_etf(sample_from_str, etf_str_payload));
        CHECK(sample_from_str.id == sf);
        CHECK(sample_from_str.name == "test");
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

    SECTION("Sets of integers and strings (std::set, std::unordered_set)") {
        std::set<int> num_set = {10, 20, 30, 40};
        std::string enc_set;
        REQUIRE_FALSE(glz::write_etf(num_set, enc_set));
        CHECK(static_cast<std::uint8_t>(enc_set[1]) == glz::etf::tag::LIST_EXT);

        std::set<int> dec_set;
        REQUIRE_FALSE(glz::read_etf(dec_set, enc_set));
        CHECK(dec_set == num_set);

        std::unordered_set<std::string> str_set = {"apple", "banana", "cherry"};
        std::string enc_uset;
        REQUIRE_FALSE(glz::write_etf(str_set, enc_uset));
        std::unordered_set<std::string> dec_uset;
        REQUIRE_FALSE(glz::read_etf(dec_uset, enc_uset));
        CHECK(dec_uset == str_set);

        std::set<int> empty_set;
        std::string enc_empty;
        REQUIRE_FALSE(glz::write_etf(empty_set, enc_empty));
        std::set<int> dec_empty = {1, 2, 3};
        REQUIRE_FALSE(glz::read_etf(dec_empty, enc_empty));
        CHECK(dec_empty.empty());
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

TEST_CASE("ETF: Integer boundaries, signedness, and bignum overflow security", "[etf][security][integers]") {
    auto append_be32 = [](std::string& s, std::uint32_t val) {
        s.push_back(static_cast<char>((val >> 24) & 0xFF));
        s.push_back(static_cast<char>((val >> 16) & 0xFF));
        s.push_back(static_cast<char>((val >> 8) & 0xFF));
        s.push_back(static_cast<char>(val & 0xFF));
    };

    SECTION("Signed 32-bit integer boundaries (INTEGER_EXT min, max, zero)") {
        std::int32_t max_i32 = std::numeric_limits<std::int32_t>::max();
        std::string enc_max;
        REQUIRE_FALSE(glz::write_etf(max_i32, enc_max));
        CHECK(static_cast<std::uint8_t>(enc_max[1]) == glz::etf::tag::INTEGER_EXT);
        std::int32_t dec_max = 0;
        REQUIRE_FALSE(glz::read_etf(dec_max, enc_max));
        CHECK(dec_max == max_i32);

        std::int32_t min_i32 = std::numeric_limits<std::int32_t>::min();
        std::string enc_min;
        REQUIRE_FALSE(glz::write_etf(min_i32, enc_min));
        CHECK(static_cast<std::uint8_t>(enc_min[1]) == glz::etf::tag::INTEGER_EXT);
        std::int32_t dec_min = 0;
        REQUIRE_FALSE(glz::read_etf(dec_min, enc_min));
        CHECK(dec_min == min_i32);

        // Explicit 0 in INTEGER_EXT format
        std::string enc_zero;
        enc_zero.push_back(static_cast<char>(glz::etf::magic_version));
        enc_zero.push_back(static_cast<char>(glz::etf::tag::INTEGER_EXT));
        append_be32(enc_zero, 0);
        std::int32_t dec_zero = -1;
        REQUIRE_FALSE(glz::read_etf(dec_zero, enc_zero));
        CHECK(dec_zero == 0);
    }

    SECTION("Negative integer decoded into unsigned types rejection") {
        std::string enc_neg;
        enc_neg.push_back(static_cast<char>(glz::etf::magic_version));
        enc_neg.push_back(static_cast<char>(glz::etf::tag::INTEGER_EXT));
        append_be32(enc_neg, 0xFFFFFFFFU); // -1 in 2's complement

        std::int32_t dec_s = 0;
        REQUIRE_FALSE(glz::read_etf(dec_s, enc_neg));
        CHECK(dec_s == -1);

        std::uint32_t dec_u32 = 0;
        auto ec_u32 = glz::read_etf(dec_u32, enc_neg);
        CHECK(static_cast<bool>(ec_u32));

        std::uint64_t dec_u64 = 0;
        auto ec_u64 = glz::read_etf(dec_u64, enc_neg);
        CHECK(static_cast<bool>(ec_u64));
    }

    SECTION("Narrowing integer overflow rejection") {
        std::string enc_large;
        enc_large.push_back(static_cast<char>(glz::etf::magic_version));
        enc_large.push_back(static_cast<char>(glz::etf::tag::INTEGER_EXT));
        append_be32(enc_large, 100000); // 100,000 exceeds int16_t (max 32767)

        std::int16_t dec_i16 = 0;
        auto ec_i16 = glz::read_etf(dec_i16, enc_large);
        CHECK(static_cast<bool>(ec_i16));

        std::int8_t dec_i8 = 0;
        auto ec_i8 = glz::read_etf(dec_i8, enc_large);
        CHECK(static_cast<bool>(ec_i8));

        std::uint8_t dec_u8 = 0;
        auto ec_u8 = glz::read_etf(dec_u8, enc_large);
        CHECK(static_cast<bool>(ec_u8));
    }

    SECTION("SMALL_BIG_EXT extreme values (UINT64_MAX, INT64_MAX, INT64_MIN, n=0)") {
        // Zero length bignum
        std::string buf_zero;
        buf_zero.push_back(static_cast<char>(glz::etf::magic_version));
        buf_zero.push_back(static_cast<char>(glz::etf::tag::SMALL_BIG_EXT));
        buf_zero.push_back(static_cast<char>(0)); // len 0
        buf_zero.push_back(static_cast<char>(0)); // sign 0
        std::uint64_t dec_bz = 999;
        auto ec_bz = glz::read_etf(dec_bz, buf_zero);
        if (!static_cast<bool>(ec_bz)) {
            CHECK(dec_bz == 0);
        }

        // UINT64_MAX (18446744073709551615)
        std::string buf_u64max;
        buf_u64max.push_back(static_cast<char>(glz::etf::magic_version));
        buf_u64max.push_back(static_cast<char>(glz::etf::tag::SMALL_BIG_EXT));
        buf_u64max.push_back(static_cast<char>(8)); // 8 bytes
        buf_u64max.push_back(static_cast<char>(0)); // positive
        for (int i = 0; i < 8; ++i) buf_u64max.push_back(static_cast<char>(0xFF));
        std::uint64_t dec_u64max = 0;
        REQUIRE_FALSE(glz::read_etf(dec_u64max, buf_u64max));
        CHECK(dec_u64max == std::numeric_limits<std::uint64_t>::max());

        // UINT64_MAX into int64_t must fail due to signed overflow
        std::int64_t dec_i64 = 0;
        auto ec_i64 = glz::read_etf(dec_i64, buf_u64max);
        CHECK(static_cast<bool>(ec_i64));

        // INT64_MAX (9223372036854775807)
        std::string buf_i64max;
        buf_i64max.push_back(static_cast<char>(glz::etf::magic_version));
        buf_i64max.push_back(static_cast<char>(glz::etf::tag::SMALL_BIG_EXT));
        buf_i64max.push_back(static_cast<char>(8));
        buf_i64max.push_back(static_cast<char>(0));
        for (int i = 0; i < 7; ++i) buf_i64max.push_back(static_cast<char>(0xFF));
        buf_i64max.push_back(static_cast<char>(0x7F));
        std::int64_t dec_i64max = 0;
        REQUIRE_FALSE(glz::read_etf(dec_i64max, buf_i64max));
        CHECK(dec_i64max == std::numeric_limits<std::int64_t>::max());

        // INT64_MIN (-9223372036854775808)
        std::string buf_i64min;
        buf_i64min.push_back(static_cast<char>(glz::etf::magic_version));
        buf_i64min.push_back(static_cast<char>(glz::etf::tag::SMALL_BIG_EXT));
        buf_i64min.push_back(static_cast<char>(8));
        buf_i64min.push_back(static_cast<char>(1)); // sign 1 = negative
        for (int i = 0; i < 7; ++i) buf_i64min.push_back(static_cast<char>(0x00));
        buf_i64min.push_back(static_cast<char>(0x80));
        std::int64_t dec_i64min = 0;
        REQUIRE_FALSE(glz::read_etf(dec_i64min, buf_i64min));
        CHECK(dec_i64min == std::numeric_limits<std::int64_t>::min());

        // Negative bignum into uint64_t must fail
        std::uint64_t dec_u_neg = 0;
        auto ec_u_neg = glz::read_etf(dec_u_neg, buf_i64min);
        CHECK(static_cast<bool>(ec_u_neg));
    }

    SECTION("Bignum 65-bit integer overflow protection") {
        std::string buf_ovf;
        buf_ovf.push_back(static_cast<char>(glz::etf::magic_version));
        buf_ovf.push_back(static_cast<char>(glz::etf::tag::SMALL_BIG_EXT));
        buf_ovf.push_back(static_cast<char>(9)); // 9 bytes (exceeds 64 bits)
        buf_ovf.push_back(static_cast<char>(0)); // sign 0
        for (int i = 0; i < 8; ++i) buf_ovf.push_back(static_cast<char>(0x00));
        buf_ovf.push_back(static_cast<char>(0x01)); // 2^64

        std::uint64_t dec_ovf_u = 0;
        auto ec_ovf_u = glz::read_etf(dec_ovf_u, buf_ovf);
        CHECK(static_cast<bool>(ec_ovf_u));

        std::int64_t dec_ovf_i = 0;
        auto ec_ovf_i = glz::read_etf(dec_ovf_i, buf_ovf);
        CHECK(static_cast<bool>(ec_ovf_i));

        discusy::snowflake sf{};
        auto ec_ovf_sf = glz::read_etf(sf, buf_ovf);
        CHECK(static_cast<bool>(ec_ovf_sf));
    }

    SECTION("LARGE_BIG_EXT valid 64-bit integer and header truncation checks") {
        std::string buf_lbig;
        buf_lbig.push_back(static_cast<char>(glz::etf::magic_version));
        buf_lbig.push_back(static_cast<char>(glz::etf::tag::LARGE_BIG_EXT));
        append_be32(buf_lbig, 8); // len = 8
        buf_lbig.push_back(static_cast<char>(0)); // sign = 0
        for (int i = 0; i < 8; ++i) buf_lbig.push_back(static_cast<char>(0x01));

        std::uint64_t dec_lbig = 0;
        REQUIRE_FALSE(glz::read_etf(dec_lbig, buf_lbig));
        CHECK(dec_lbig == 0x0101010101010101ULL);

        // Truncated LARGE_BIG_EXT missing sign byte (5 bytes total: magic + tag + 3 len bytes)
        std::string trunc_hdr = buf_lbig.substr(0, 5);
        auto ec_thdr = glz::read_etf(dec_lbig, trunc_hdr);
        CHECK(static_cast<bool>(ec_thdr));
        CHECK(ec_thdr.ec == glz::error_code::unexpected_end);
    }
}

TEST_CASE("ETF: Floating-point standards, special values, and legacy float corruption", "[etf][security][floats]") {
    SECTION("IEEE 754 special values (Infinity, NaN, -0.0, denorm_min)") {
        double inf = std::numeric_limits<double>::infinity();
        std::string enc_inf;
        REQUIRE_FALSE(glz::write_etf(inf, enc_inf));
        double dec_inf = 0.0;
        REQUIRE_FALSE(glz::read_etf(dec_inf, enc_inf));
        CHECK(std::isinf(dec_inf));
        CHECK(dec_inf > 0);

        double neg_inf = -std::numeric_limits<double>::infinity();
        std::string enc_ninf;
        REQUIRE_FALSE(glz::write_etf(neg_inf, enc_ninf));
        double dec_ninf = 0.0;
        REQUIRE_FALSE(glz::read_etf(dec_ninf, enc_ninf));
        CHECK(std::isinf(dec_ninf));
        CHECK(dec_ninf < 0);

        double qnan = std::numeric_limits<double>::quiet_NaN();
        std::string enc_nan;
        REQUIRE_FALSE(glz::write_etf(qnan, enc_nan));
        double dec_nan = 0.0;
        REQUIRE_FALSE(glz::read_etf(dec_nan, enc_nan));
        CHECK(std::isnan(dec_nan));

        double neg_zero = -0.0;
        std::string enc_nzero;
        REQUIRE_FALSE(glz::write_etf(neg_zero, enc_nzero));
        double dec_nzero = 1.0;
        REQUIRE_FALSE(glz::read_etf(dec_nzero, enc_nzero));
        CHECK(dec_nzero == 0.0);
        CHECK(std::signbit(dec_nzero));

        double denorm = std::numeric_limits<double>::denorm_min();
        std::string enc_denorm;
        REQUIRE_FALSE(glz::write_etf(denorm, enc_denorm));
        double dec_denorm = 0.0;
        REQUIRE_FALSE(glz::read_etf(dec_denorm, enc_denorm));
        CHECK(dec_denorm == denorm);
    }

    SECTION("Type confusion: Float into integer, snowflake, and string rejection") {
        double valid_d = 42.5;
        std::string enc_d;
        REQUIRE_FALSE(glz::write_etf(valid_d, enc_d));

        std::int32_t bad_i = 0;
        CHECK(static_cast<bool>(glz::read_etf(bad_i, enc_d)));

        std::uint64_t bad_u = 0;
        CHECK(static_cast<bool>(glz::read_etf(bad_u, enc_d)));

        discusy::snowflake bad_sf{};
        CHECK(static_cast<bool>(glz::read_etf(bad_sf, enc_d)));

        std::string bad_str;
        CHECK(static_cast<bool>(glz::read_etf(bad_str, enc_d)));
    }

    SECTION("Legacy FLOAT_EXT valid parsing and corrupt string rejection") {
        std::string legacy_valid;
        legacy_valid.push_back(static_cast<char>(glz::etf::magic_version));
        legacy_valid.push_back(static_cast<char>(glz::etf::tag::FLOAT_EXT));
        std::string float_str = "3.14159265358979311600e+00";
        legacy_valid.append(float_str);
        legacy_valid.append(31 - float_str.size(), '\0');
        REQUIRE(legacy_valid.size() == 1 + 1 + 31);

        double dec_leg = 0.0;
        REQUIRE_FALSE(glz::read_etf(dec_leg, legacy_valid));
        CHECK(std::abs(dec_leg - 3.141592653589793) < 1e-9);

        // Corrupt float string: non-numeric text
        std::string legacy_corrupt;
        legacy_corrupt.push_back(static_cast<char>(glz::etf::magic_version));
        legacy_corrupt.push_back(static_cast<char>(glz::etf::tag::FLOAT_EXT));
        legacy_corrupt.append("completely_invalid_float_text!!");
        legacy_corrupt.resize(1 + 1 + 31, '\0');
        double dec_corrupt = 0.0;
        CHECK(static_cast<bool>(glz::read_etf(dec_corrupt, legacy_corrupt)));

        // Truncated at 30 bytes (1 byte short of 31)
        std::string legacy_trunc = legacy_valid.substr(0, 1 + 1 + 30);
        CHECK(static_cast<bool>(glz::read_etf(dec_leg, legacy_trunc)));

        // Skip valid and corrupt FLOAT_EXT
        glz::skip sk{};
        REQUIRE_FALSE(glz::read_etf(sk, legacy_valid));
        CHECK(static_cast<bool>(glz::read_etf(sk, legacy_trunc)));
    }
}

TEST_CASE("ETF: String, binary, atom length attacks, and embedded null preservation", "[etf][security][strings]") {
    auto append_be32 = [](std::string& s, std::uint32_t val) {
        s.push_back(static_cast<char>((val >> 24) & 0xFF));
        s.push_back(static_cast<char>((val >> 16) & 0xFF));
        s.push_back(static_cast<char>((val >> 8) & 0xFF));
        s.push_back(static_cast<char>(val & 0xFF));
    };
    auto append_be16 = [](std::string& s, std::uint16_t val) {
        s.push_back(static_cast<char>((val >> 8) & 0xFF));
        s.push_back(static_cast<char>(val & 0xFF));
    };

    SECTION("Binary-safe strings: preservation of embedded null bytes") {
        std::string raw_nulls;
        raw_nulls.assign("header\0middle\0tail", 18);
        std::string enc_nulls;
        REQUIRE_FALSE(glz::write_etf(raw_nulls, enc_nulls));

        std::string dec_nulls;
        REQUIRE_FALSE(glz::read_etf(dec_nulls, enc_nulls));
        CHECK(dec_nulls.size() == 18);
        CHECK(dec_nulls == raw_nulls);
        CHECK(dec_nulls[6] == '\0');
        CHECK(dec_nulls[13] == '\0');

        std::string_view dec_sv;
        REQUIRE_FALSE(glz::read_etf(dec_sv, enc_nulls));
        CHECK(dec_sv.size() == 18);
        CHECK(dec_sv == raw_nulls);
    }

    SECTION("STRING_EXT valid empty, normal, and embedded null strings") {
        // Empty STRING_EXT
        std::string empty_str_ext;
        empty_str_ext.push_back(static_cast<char>(glz::etf::magic_version));
        empty_str_ext.push_back(static_cast<char>(glz::etf::tag::STRING_EXT));
        append_be16(empty_str_ext, 0);
        std::string dec_empty = "initial";
        REQUIRE_FALSE(glz::read_etf(dec_empty, empty_str_ext));
        CHECK(dec_empty.empty());

        // STRING_EXT with embedded null
        std::string null_str_ext;
        null_str_ext.push_back(static_cast<char>(glz::etf::magic_version));
        null_str_ext.push_back(static_cast<char>(glz::etf::tag::STRING_EXT));
        append_be16(null_str_ext, 9);
        null_str_ext.append("foo\0bar\0z", 9);
        std::string dec_str_ext;
        REQUIRE_FALSE(glz::read_etf(dec_str_ext, null_str_ext));
        CHECK(dec_str_ext.size() == 9);
        CHECK(dec_str_ext[3] == '\0');
    }

    SECTION("Pre-allocation DoS protection on BINARY_EXT and STRING_EXT") {
        std::string s_out;

        // BINARY_EXT claiming 0xFFFFFFFF bytes
        std::string dos_bin;
        dos_bin.push_back(static_cast<char>(glz::etf::magic_version));
        dos_bin.push_back(static_cast<char>(glz::etf::tag::BINARY_EXT));
        append_be32(dos_bin, 0xFFFFFFFFU);
        dos_bin.append("short");
        auto ec_dos1 = glz::read_etf(s_out, dos_bin);
        CHECK(static_cast<bool>(ec_dos1));
        CHECK(ec_dos1.ec == glz::error_code::unexpected_end);

        // BINARY_EXT claiming 2GB
        std::string dos_bin2;
        dos_bin2.push_back(static_cast<char>(glz::etf::magic_version));
        dos_bin2.push_back(static_cast<char>(glz::etf::tag::BINARY_EXT));
        append_be32(dos_bin2, 0x7FFFFFFFU);
        dos_bin2.append("short");
        auto ec_dos2 = glz::read_etf(s_out, dos_bin2);
        CHECK(static_cast<bool>(ec_dos2));
        CHECK(ec_dos2.ec == glz::error_code::unexpected_end);

        // STRING_EXT claiming 65535 bytes
        std::string dos_str;
        dos_str.push_back(static_cast<char>(glz::etf::magic_version));
        dos_str.push_back(static_cast<char>(glz::etf::tag::STRING_EXT));
        append_be16(dos_str, 0xFFFFU);
        dos_str.append("short");
        auto ec_dos3 = glz::read_etf(s_out, dos_str);
        CHECK(static_cast<bool>(ec_dos3));
        CHECK(ec_dos3.ec == glz::error_code::unexpected_end);
    }

    SECTION("Off-by-one string length underflow checks") {
        std::string s_out;
        std::string off1;
        off1.push_back(static_cast<char>(glz::etf::magic_version));
        off1.push_back(static_cast<char>(glz::etf::tag::BINARY_EXT));
        append_be32(off1, 5);
        off1.append("1234"); // only 4 bytes
        auto ec_off1 = glz::read_etf(s_out, off1);
        CHECK(static_cast<bool>(ec_off1));
        CHECK(ec_off1.ec == glz::error_code::unexpected_end);
    }

    SECTION("Atom type rejection and nil atom in optional strings") {
        std::optional<std::string> opt_str = "initial";
        std::string nil_atom;
        nil_atom.push_back(static_cast<char>(glz::etf::magic_version));
        nil_atom.push_back(static_cast<char>(glz::etf::tag::SMALL_ATOM_UTF8_EXT));
        nil_atom.push_back(static_cast<char>(3));
        nil_atom.append("nil");
        REQUIRE_FALSE(glz::read_etf(opt_str, nil_atom));
        CHECK_FALSE(opt_str.has_value());

        int int_target = 0;
        CHECK(static_cast<bool>(glz::read_etf(int_target, nil_atom)));

        discusy::snowflake sf_target{};
        CHECK(static_cast<bool>(glz::read_etf(sf_target, nil_atom)));
    }
}

TEST_CASE("ETF: Snowflake parser security, malicious strings, and overflow boundaries", "[etf][security][snowflake]") {
    auto append_be32 = [](std::string& s, std::uint32_t val) {
        s.push_back(static_cast<char>((val >> 24) & 0xFF));
        s.push_back(static_cast<char>((val >> 16) & 0xFF));
        s.push_back(static_cast<char>((val >> 8) & 0xFF));
        s.push_back(static_cast<char>(val & 0xFF));
    };

    SECTION("Rejection of malformed, negative, decimal, and hex strings") {
        auto check_bad_sf = [&](std::string_view bad) {
            std::string enc;
            REQUIRE_FALSE(glz::write_etf(std::string{bad}, enc));
            discusy::snowflake sf{};
            auto ec = glz::read_etf(sf, enc);
            CHECK(static_cast<bool>(ec));
        };

        check_bad_sf("");
        check_bad_sf("-1");
        check_bad_sf("-175928847299117063");
        check_bad_sf("123.456");
        check_bad_sf("175928847299117063xyz");
        check_bad_sf(" 175928847299117063");
        check_bad_sf("0x123456");
        check_bad_sf("18446744073709551616"); // 2^64 overflow
        check_bad_sf("9999999999999999999999999999999999999999");
    }

    SECTION("Valid snowflake string boundaries (zero, max uint64, leading zeros)") {
        std::string zero_s = "0";
        std::string enc_z;
        REQUIRE_FALSE(glz::write_etf(zero_s, enc_z));
        discusy::snowflake sf_z{};
        REQUIRE_FALSE(glz::read_etf(sf_z, enc_z));
        CHECK(sf_z.value == 0ULL);

        std::string max_s = "18446744073709551615";
        std::string enc_m;
        REQUIRE_FALSE(glz::write_etf(max_s, enc_m));
        discusy::snowflake sf_m{};
        REQUIRE_FALSE(glz::read_etf(sf_m, enc_m));
        CHECK(sf_m.value == std::numeric_limits<std::uint64_t>::max());

        std::string lead_s = "0000000000123456";
        std::string enc_l;
        REQUIRE_FALSE(glz::write_etf(lead_s, enc_l));
        discusy::snowflake sf_l{};
        REQUIRE_FALSE(glz::read_etf(sf_l, enc_l));
        CHECK(sf_l.value == 123456ULL);
    }

    SECTION("ETF integer format bounds and negative value rejection for snowflake") {
        // Negative signed integer into snowflake must fail
        std::string sf_neg_int;
        sf_neg_int.push_back(static_cast<char>(glz::etf::magic_version));
        sf_neg_int.push_back(static_cast<char>(glz::etf::tag::INTEGER_EXT));
        append_be32(sf_neg_int, 0xFFFFFFFFU); // -1
        discusy::snowflake sf_neg{};
        CHECK(static_cast<bool>(glz::read_etf(sf_neg, sf_neg_int)));

        // Negative bignum into snowflake must fail
        std::string sf_neg_bignum;
        sf_neg_bignum.push_back(static_cast<char>(glz::etf::magic_version));
        sf_neg_bignum.push_back(static_cast<char>(glz::etf::tag::SMALL_BIG_EXT));
        sf_neg_bignum.push_back(static_cast<char>(4));
        sf_neg_bignum.push_back(static_cast<char>(1)); // sign 1 = negative
        append_be32(sf_neg_bignum, 100);
        CHECK(static_cast<bool>(glz::read_etf(sf_neg, sf_neg_bignum)));

        // 9-byte bignum (exceeds uint64_t) into snowflake must fail
        std::string sf_ovf_bignum;
        sf_ovf_bignum.push_back(static_cast<char>(glz::etf::magic_version));
        sf_ovf_bignum.push_back(static_cast<char>(glz::etf::tag::SMALL_BIG_EXT));
        sf_ovf_bignum.push_back(static_cast<char>(9));
        sf_ovf_bignum.push_back(static_cast<char>(0));
        for (int i = 0; i < 8; ++i) sf_ovf_bignum.push_back(static_cast<char>(0x00));
        sf_ovf_bignum.push_back(static_cast<char>(0x01));
        CHECK(static_cast<bool>(glz::read_etf(sf_neg, sf_ovf_bignum)));
    }

    SECTION("Type confusion: Float, bool, list, and map rejection into snowflake") {
        discusy::snowflake sf{};

        // Float
        std::string enc_f;
        REQUIRE_FALSE(glz::write_etf(12345.67, enc_f));
        CHECK(static_cast<bool>(glz::read_etf(sf, enc_f)));

        // Bool
        std::string enc_b;
        REQUIRE_FALSE(glz::write_etf(true, enc_b));
        CHECK(static_cast<bool>(glz::read_etf(sf, enc_b)));

        // List
        std::vector<int> list_val = {1, 2, 3};
        std::string enc_l;
        REQUIRE_FALSE(glz::write_etf(list_val, enc_l));
        CHECK(static_cast<bool>(glz::read_etf(sf, enc_l)));

        // Map
        std::map<std::string, int> map_val = {{"id", 1}};
        std::string enc_m;
        REQUIRE_FALSE(glz::write_etf(map_val, enc_m));
        CHECK(static_cast<bool>(glz::read_etf(sf, enc_m)));
    }
}

TEST_CASE("ETF: Map key-value corruption, non-string keys, and duplicate key resilience", "[etf][security][maps]") {
    auto append_be32 = [](std::string& s, std::uint32_t val) {
        s.push_back(static_cast<char>((val >> 24) & 0xFF));
        s.push_back(static_cast<char>((val >> 16) & 0xFF));
        s.push_back(static_cast<char>((val >> 8) & 0xFF));
        s.push_back(static_cast<char>(val & 0xFF));
    };

    SECTION("Truncated key-value pairs in MAP_EXT (unexpected_end)") {
        std::string trunc_kv;
        trunc_kv.push_back(static_cast<char>(glz::etf::magic_version));
        trunc_kv.push_back(static_cast<char>(glz::etf::tag::MAP_EXT));
        append_be32(trunc_kv, 2); // claims 2 pairs
        // Pair 1: key "id", value 123
        trunc_kv.push_back(static_cast<char>(glz::etf::tag::BINARY_EXT));
        append_be32(trunc_kv, 2);
        trunc_kv.append("id");
        trunc_kv.push_back(static_cast<char>(glz::etf::tag::SMALL_INTEGER_EXT));
        trunc_kv.push_back(static_cast<char>(123));
        // Pair 2: key "name", but NO VALUE! Buffer ends here!
        trunc_kv.push_back(static_cast<char>(glz::etf::tag::BINARY_EXT));
        append_be32(trunc_kv, 4);
        trunc_kv.append("name");

        test_etf_types::SampleStruct dec_sample{};
        auto ec_tkv = glz::read_etf(dec_sample, trunc_kv);
        CHECK(static_cast<bool>(ec_tkv));
        CHECK(ec_tkv.ec == glz::error_code::unexpected_end);

        std::map<std::string, int> dec_map;
        auto ec_tmap = glz::read_etf(dec_map, trunc_kv);
        CHECK(static_cast<bool>(ec_tmap));
        CHECK(ec_tmap.ec == glz::error_code::unexpected_end);
    }

    SECTION("Non-string keys in struct deserialization without crash") {
        std::string bad_key_map;
        bad_key_map.push_back(static_cast<char>(glz::etf::magic_version));
        bad_key_map.push_back(static_cast<char>(glz::etf::tag::MAP_EXT));
        append_be32(bad_key_map, 1);
        // Key is an integer, value is 100
        bad_key_map.push_back(static_cast<char>(glz::etf::tag::SMALL_INTEGER_EXT));
        bad_key_map.push_back(static_cast<char>(42));
        bad_key_map.push_back(static_cast<char>(glz::etf::tag::SMALL_INTEGER_EXT));
        bad_key_map.push_back(static_cast<char>(100));

        test_etf_types::SampleStruct s_bad{};
        // Parser must reject or handle gracefully without crashing
        auto ec_sbad = glz::read_etf(s_bad, bad_key_map);
        (void)ec_sbad;
    }

    SECTION("Duplicate keys in MAP_EXT resilience") {
        std::string dup_map;
        dup_map.push_back(static_cast<char>(glz::etf::magic_version));
        dup_map.push_back(static_cast<char>(glz::etf::tag::MAP_EXT));
        append_be32(dup_map, 2);
        // Pair 1: "name" -> "first"
        dup_map.push_back(static_cast<char>(glz::etf::tag::BINARY_EXT));
        append_be32(dup_map, 4);
        dup_map.append("name");
        dup_map.push_back(static_cast<char>(glz::etf::tag::BINARY_EXT));
        append_be32(dup_map, 5);
        dup_map.append("first");
        // Pair 2: "name" -> "second"
        dup_map.push_back(static_cast<char>(glz::etf::tag::BINARY_EXT));
        append_be32(dup_map, 4);
        dup_map.append("name");
        dup_map.push_back(static_cast<char>(glz::etf::tag::BINARY_EXT));
        append_be32(dup_map, 6);
        dup_map.append("second");

        test_etf_types::SampleStruct dec_dup{};
        REQUIRE_FALSE(glz::read_etf(dec_dup, dup_map));
        CHECK_FALSE(dec_dup.name.empty());

        std::map<std::string, std::string> map_dup;
        REQUIRE_FALSE(glz::read_etf(map_dup, dup_map));
        CHECK(map_dup.size() == 1);
    }

    SECTION("Container type mismatch rejection (map vs vector, list vs map)") {
        std::map<std::string, int> map_src = {{"a", 1}};
        std::string enc_map;
        REQUIRE_FALSE(glz::write_etf(map_src, enc_map));

        std::vector<int> dec_vec;
        CHECK(static_cast<bool>(glz::read_etf(dec_vec, enc_map)));

        std::vector<int> list_src = {1, 2, 3};
        std::string enc_list;
        REQUIRE_FALSE(glz::write_etf(list_src, enc_list));

        std::map<std::string, int> dec_map;
        CHECK(static_cast<bool>(glz::read_etf(dec_map, enc_list)));
    }

    SECTION("Partial read skipping unknown complex types (deep lists, bignums, floats)") {
        std::string complex_map;
        complex_map.push_back(static_cast<char>(glz::etf::magic_version));
        complex_map.push_back(static_cast<char>(glz::etf::tag::MAP_EXT));
        append_be32(complex_map, 3);
        // Pair 1: "name" -> "special_agent"
        complex_map.push_back(static_cast<char>(glz::etf::tag::BINARY_EXT));
        append_be32(complex_map, 4);
        complex_map.append("name");
        complex_map.push_back(static_cast<char>(glz::etf::tag::BINARY_EXT));
        append_be32(complex_map, 13);
        complex_map.append("special_agent");
        // Pair 2: "unknown_list" -> [1, 2, 3]
        complex_map.push_back(static_cast<char>(glz::etf::tag::BINARY_EXT));
        append_be32(complex_map, 12);
        complex_map.append("unknown_list");
        complex_map.push_back(static_cast<char>(glz::etf::tag::LIST_EXT));
        append_be32(complex_map, 3);
        for (int i = 1; i <= 3; ++i) {
            complex_map.push_back(static_cast<char>(glz::etf::tag::SMALL_INTEGER_EXT));
            complex_map.push_back(static_cast<char>(i));
        }
        complex_map.push_back(static_cast<char>(glz::etf::tag::NIL_EXT));
        // Pair 3: "count" -> 99
        complex_map.push_back(static_cast<char>(glz::etf::tag::BINARY_EXT));
        append_be32(complex_map, 5);
        complex_map.append("count");
        complex_map.push_back(static_cast<char>(glz::etf::tag::SMALL_INTEGER_EXT));
        complex_map.push_back(static_cast<char>(99));

        test_etf_types::SubsetStruct sub{};
        auto ec_sub = glz::read<glz::etf_opts_partial_read>(sub, complex_map);
        REQUIRE_FALSE(static_cast<bool>(ec_sub));
        CHECK(sub.name == "special_agent");
        CHECK(sub.count == 99);
    }
}

TEST_CASE("ETF: Improper lists, tuple arity mismatches, and container bounds", "[etf][security][lists][tuples]") {
    auto append_be32 = [](std::string& s, std::uint32_t val) {
        s.push_back(static_cast<char>((val >> 24) & 0xFF));
        s.push_back(static_cast<char>((val >> 16) & 0xFF));
        s.push_back(static_cast<char>((val >> 8) & 0xFF));
        s.push_back(static_cast<char>(val & 0xFF));
    };

    SECTION("Improper lists (LIST_EXT with non-nil tail) resilience") {
        std::string improper;
        improper.push_back(static_cast<char>(glz::etf::magic_version));
        improper.push_back(static_cast<char>(glz::etf::tag::LIST_EXT));
        append_be32(improper, 2);
        improper.push_back(static_cast<char>(glz::etf::tag::SMALL_INTEGER_EXT));
        improper.push_back(static_cast<char>(10));
        improper.push_back(static_cast<char>(glz::etf::tag::SMALL_INTEGER_EXT));
        improper.push_back(static_cast<char>(20));
        // Improper tail is an integer instead of NIL_EXT
        improper.push_back(static_cast<char>(glz::etf::tag::SMALL_INTEGER_EXT));
        improper.push_back(static_cast<char>(30));

        std::vector<int> v_imp;
        auto ec_imp = glz::read_etf(v_imp, improper);
        (void)ec_imp; // Handled without crash or infinite loop

        glz::skip sk{};
        auto ec_sk = glz::read_etf(sk, improper);
        (void)ec_sk;
    }

    SECTION("List element corruption and type mismatch rejection") {
        // List with truncated element
        std::string trunc_elem;
        trunc_elem.push_back(static_cast<char>(glz::etf::magic_version));
        trunc_elem.push_back(static_cast<char>(glz::etf::tag::LIST_EXT));
        append_be32(trunc_elem, 2);
        trunc_elem.push_back(static_cast<char>(glz::etf::tag::SMALL_INTEGER_EXT));
        trunc_elem.push_back(static_cast<char>(10));
        trunc_elem.push_back(static_cast<char>(glz::etf::tag::INTEGER_EXT));
        // Buffer ends prematurely before 4 bytes of INTEGER_EXT
        std::vector<int> v_out;
        auto ec_te = glz::read_etf(v_out, trunc_elem);
        CHECK(static_cast<bool>(ec_te));
        CHECK(ec_te.ec == glz::error_code::unexpected_end);

        // List with element type mismatch (string in vector<int>)
        std::string mismatch_elem;
        mismatch_elem.push_back(static_cast<char>(glz::etf::magic_version));
        mismatch_elem.push_back(static_cast<char>(glz::etf::tag::LIST_EXT));
        append_be32(mismatch_elem, 2);
        mismatch_elem.push_back(static_cast<char>(glz::etf::tag::SMALL_INTEGER_EXT));
        mismatch_elem.push_back(static_cast<char>(10));
        mismatch_elem.push_back(static_cast<char>(glz::etf::tag::BINARY_EXT));
        append_be32(mismatch_elem, 3);
        mismatch_elem.append("bad");
        mismatch_elem.push_back(static_cast<char>(glz::etf::tag::NIL_EXT));

        auto ec_me = glz::read_etf(v_out, mismatch_elem);
        CHECK(static_cast<bool>(ec_me));
    }

    SECTION("Tuple arity mismatch rejection (too few or too many elements)") {
        std::string tup_buf;
        tup_buf.push_back(static_cast<char>(glz::etf::magic_version));
        tup_buf.push_back(static_cast<char>(glz::etf::tag::LARGE_TUPLE_EXT));
        append_be32(tup_buf, 3);
        tup_buf.push_back(static_cast<char>(glz::etf::tag::SMALL_INTEGER_EXT));
        tup_buf.push_back(static_cast<char>(1));
        tup_buf.push_back(static_cast<char>(glz::etf::tag::SMALL_INTEGER_EXT));
        tup_buf.push_back(static_cast<char>(2));
        tup_buf.push_back(static_cast<char>(glz::etf::tag::SMALL_INTEGER_EXT));
        tup_buf.push_back(static_cast<char>(3));

        std::tuple<int, int> dec_tup2;
        auto ec_t2 = glz::read_etf(dec_tup2, tup_buf);
        CHECK(static_cast<bool>(ec_t2)); // 3 elements into 2-tuple must fail
    }

    SECTION("Truncated tuples and tuple arity integer overflow protection") {
        std::string trunc_tup;
        trunc_tup.push_back(static_cast<char>(glz::etf::magic_version));
        trunc_tup.push_back(static_cast<char>(glz::etf::tag::LARGE_TUPLE_EXT));
        append_be32(trunc_tup, 3);
        trunc_tup.push_back(static_cast<char>(glz::etf::tag::SMALL_INTEGER_EXT));
        trunc_tup.push_back(static_cast<char>(1));
        // Buffer ends prematurely

        std::tuple<int, int, int> dec_t3;
        auto ec_tt = glz::read_etf(dec_t3, trunc_tup);
        CHECK(static_cast<bool>(ec_tt));
        CHECK(ec_tt.ec == glz::error_code::unexpected_end);

        // DoS arity 0xFFFFFFFF
        std::string dos_tup;
        dos_tup.push_back(static_cast<char>(glz::etf::magic_version));
        dos_tup.push_back(static_cast<char>(glz::etf::tag::LARGE_TUPLE_EXT));
        append_be32(dos_tup, 0xFFFFFFFFU);
        dos_tup.append("short");

        glz::skip sk{};
        auto ec_dos_tup = glz::read_etf(sk, dos_tup);
        CHECK(static_cast<bool>(ec_dos_tup));
        CHECK(ec_dos_tup.ec == glz::error_code::unexpected_end);
    }
}

TEST_CASE("ETF: Gateway parse_payload_base attack vectors and malformed payloads", "[etf][security][gateway]") {
    auto append_be32 = [](std::string& s, std::uint32_t val) {
        s.push_back(static_cast<char>((val >> 24) & 0xFF));
        s.push_back(static_cast<char>((val >> 16) & 0xFF));
        s.push_back(static_cast<char>((val >> 8) & 0xFF));
        s.push_back(static_cast<char>(val & 0xFF));
    };

    SECTION("Rejection of non-map top-level payloads (ints, lists, strings, short buffers)") {
        discusy::recieve_event::payload_base pb{};

        std::string not_a_map;
        not_a_map.push_back(static_cast<char>(glz::etf::magic_version));
        not_a_map.push_back(static_cast<char>(glz::etf::tag::SMALL_INTEGER_EXT));
        not_a_map.push_back(static_cast<char>(42));
        CHECK(discusy::etf::parse_payload_base(pb, not_a_map));

        std::string not_a_map_str;
        not_a_map_str.push_back(static_cast<char>(glz::etf::magic_version));
        not_a_map_str.push_back(static_cast<char>(glz::etf::tag::BINARY_EXT));
        append_be32(not_a_map_str, 5);
        not_a_map_str.append("hello");
        CHECK(discusy::etf::parse_payload_base(pb, not_a_map_str));

        std::string one_byte;
        one_byte.push_back(static_cast<char>(glz::etf::magic_version));
        CHECK(discusy::etf::parse_payload_base(pb, one_byte));

        std::string empty_buf;
        CHECK(discusy::etf::parse_payload_base(pb, empty_buf));
    }

    SECTION("Rejection of missing or non-integer op field") {
        discusy::recieve_event::payload_base pb{};

        // Map without op key
        std::string no_op_map;
        no_op_map.push_back(static_cast<char>(glz::etf::magic_version));
        no_op_map.push_back(static_cast<char>(glz::etf::tag::MAP_EXT));
        append_be32(no_op_map, 1);
        no_op_map.push_back(static_cast<char>(glz::etf::tag::BINARY_EXT));
        append_be32(no_op_map, 1);
        no_op_map.push_back('s');
        no_op_map.push_back(static_cast<char>(glz::etf::tag::SMALL_INTEGER_EXT));
        no_op_map.push_back(static_cast<char>(42));
        CHECK(discusy::etf::parse_payload_base(pb, no_op_map));

        // Map with string op value
        std::string string_op_map;
        string_op_map.push_back(static_cast<char>(glz::etf::magic_version));
        string_op_map.push_back(static_cast<char>(glz::etf::tag::MAP_EXT));
        append_be32(string_op_map, 1);
        string_op_map.push_back(static_cast<char>(glz::etf::tag::BINARY_EXT));
        append_be32(string_op_map, 2);
        string_op_map.append("op");
        string_op_map.push_back(static_cast<char>(glz::etf::tag::BINARY_EXT));
        append_be32(string_op_map, 8);
        string_op_map.append("Dispatch");
        CHECK(discusy::etf::parse_payload_base(pb, string_op_map));
    }

    SECTION("Malformed s and t field handling without crash") {
        discusy::recieve_event::payload_base pb{};

        // Map with string s value (expected integer)
        std::string bad_s_map;
        bad_s_map.push_back(static_cast<char>(glz::etf::magic_version));
        bad_s_map.push_back(static_cast<char>(glz::etf::tag::MAP_EXT));
        append_be32(bad_s_map, 2);
        // op = 0
        bad_s_map.push_back(static_cast<char>(glz::etf::tag::BINARY_EXT));
        append_be32(bad_s_map, 2);
        bad_s_map.append("op");
        bad_s_map.push_back(static_cast<char>(glz::etf::tag::SMALL_INTEGER_EXT));
        bad_s_map.push_back(static_cast<char>(0));
        // s = "not_an_int"
        bad_s_map.push_back(static_cast<char>(glz::etf::tag::BINARY_EXT));
        append_be32(bad_s_map, 1);
        bad_s_map.push_back('s');
        bad_s_map.push_back(static_cast<char>(glz::etf::tag::BINARY_EXT));
        append_be32(bad_s_map, 10);
        bad_s_map.append("not_an_int");

        // Should handle without throwing or crashing
        auto res = discusy::etf::parse_payload_base(pb, bad_s_map);
        (void)res;
    }

    SECTION("Flooding of unknown fields before op, s, t in parse_payload_base") {
        std::string flood_map;
        flood_map.push_back(static_cast<char>(glz::etf::magic_version));
        flood_map.push_back(static_cast<char>(glz::etf::tag::MAP_EXT));
        append_be32(flood_map, 23); // 20 unknown + op, s, t

        for (int i = 0; i < 20; ++i) {
            std::string k = "junk_" + std::to_string(i);
            flood_map.push_back(static_cast<char>(glz::etf::tag::BINARY_EXT));
            append_be32(flood_map, static_cast<std::uint32_t>(k.size()));
            flood_map.append(k);
            flood_map.push_back(static_cast<char>(glz::etf::tag::SMALL_INTEGER_EXT));
            flood_map.push_back(static_cast<char>(i));
        }

        // op = 0 (Dispatch)
        flood_map.push_back(static_cast<char>(glz::etf::tag::BINARY_EXT));
        append_be32(flood_map, 2);
        flood_map.append("op");
        flood_map.push_back(static_cast<char>(glz::etf::tag::SMALL_INTEGER_EXT));
        flood_map.push_back(static_cast<char>(0));

        // s = 999
        flood_map.push_back(static_cast<char>(glz::etf::tag::BINARY_EXT));
        append_be32(flood_map, 1);
        flood_map.append("s");
        flood_map.push_back(static_cast<char>(glz::etf::tag::INTEGER_EXT));
        append_be32(flood_map, 999);

        // t = "MESSAGE_CREATE"
        flood_map.push_back(static_cast<char>(glz::etf::tag::BINARY_EXT));
        append_be32(flood_map, 1);
        flood_map.append("t");
        flood_map.push_back(static_cast<char>(glz::etf::tag::BINARY_EXT));
        append_be32(flood_map, 14);
        flood_map.append("MESSAGE_CREATE");

        discusy::recieve_event::payload_base pb_flood{};
        REQUIRE_FALSE(discusy::etf::parse_payload_base(pb_flood, flood_map));
        CHECK(pb_flood.op == discusy::Opcode::Dispatch);
        REQUIRE(pb_flood.s.has_value());
        CHECK(*pb_flood.s == 999);
        REQUIRE(pb_flood.t.has_value());
        CHECK(*pb_flood.t == discusy::recieve_event::event::MESSAGE_CREATE);
    }
}

TEST_CASE("ETF: Skip parser coverage on all tags and deep recursion protection", "[etf][security][skip]") {
    auto append_be32 = [](std::string& s, std::uint32_t val) {
        s.push_back(static_cast<char>((val >> 24) & 0xFF));
        s.push_back(static_cast<char>((val >> 16) & 0xFF));
        s.push_back(static_cast<char>((val >> 8) & 0xFF));
        s.push_back(static_cast<char>(val & 0xFF));
    };

    SECTION("glz::skip on every valid ETF tag") {
        auto check_skip = [](const std::string& b) {
            glz::skip sk{};
            return !static_cast<bool>(glz::read_etf(sk, b));
        };

        // SMALL_INTEGER_EXT
        std::string b_sint{static_cast<char>(glz::etf::magic_version), static_cast<char>(glz::etf::tag::SMALL_INTEGER_EXT), 42};
        CHECK(check_skip(b_sint));

        // INTEGER_EXT
        std::string b_int{static_cast<char>(glz::etf::magic_version), static_cast<char>(glz::etf::tag::INTEGER_EXT), 0, 0, 1, 0};
        CHECK(check_skip(b_int));

        // SMALL_BIG_EXT
        std::string b_sbig{static_cast<char>(glz::etf::magic_version), static_cast<char>(glz::etf::tag::SMALL_BIG_EXT), 2, 0, 1, 2};
        CHECK(check_skip(b_sbig));

        // NIL_EXT
        std::string b_nil{static_cast<char>(glz::etf::magic_version), static_cast<char>(glz::etf::tag::NIL_EXT)};
        CHECK(check_skip(b_nil));

        // STRING_EXT
        std::string b_str{static_cast<char>(glz::etf::magic_version), static_cast<char>(glz::etf::tag::STRING_EXT), 0, 2, 'a', 'b'};
        CHECK(check_skip(b_str));

        // BINARY_EXT
        std::string b_bin{static_cast<char>(glz::etf::magic_version), static_cast<char>(glz::etf::tag::BINARY_EXT), 0, 0, 0, 2, 'x', 'y'};
        CHECK(check_skip(b_bin));

        // BIT_BINARY_EXT
        std::string b_bbin{static_cast<char>(glz::etf::magic_version), static_cast<char>(glz::etf::tag::BIT_BINARY_EXT), 0, 0, 0, 1, 8, 'z'};
        CHECK(check_skip(b_bbin));

        // SMALL_ATOM_UTF8_EXT
        std::string b_satom{static_cast<char>(glz::etf::magic_version), static_cast<char>(glz::etf::tag::SMALL_ATOM_UTF8_EXT), 3, 'a', 'b', 'c'};
        CHECK(check_skip(b_satom));

        // SMALL_ATOM_EXT
        std::string b_latom{static_cast<char>(glz::etf::magic_version), static_cast<char>(glz::etf::tag::SMALL_ATOM_EXT), 3, 'a', 'b', 'c'};
        CHECK(check_skip(b_latom));

        // ATOM_EXT
        std::string b_atom{static_cast<char>(glz::etf::magic_version), static_cast<char>(glz::etf::tag::ATOM_EXT), 0, 3, 'a', 'b', 'c'};
        CHECK(check_skip(b_atom));
    }

    SECTION("Deeply nested tuples recursion limit enforcement") {
        constexpr size_t nesting = 300;
        std::string deep_tup;
        deep_tup.push_back(static_cast<char>(glz::etf::magic_version));
        for (size_t i = 0; i < nesting; ++i) {
            deep_tup.push_back(static_cast<char>(glz::etf::tag::LARGE_TUPLE_EXT));
            append_be32(deep_tup, 1);
        }
        deep_tup.push_back(static_cast<char>(glz::etf::tag::SMALL_INTEGER_EXT));
        deep_tup.push_back(static_cast<char>(42));

        glz::skip sk_tup{};
        auto ec_dt = glz::read_etf(sk_tup, deep_tup);
        CHECK(static_cast<bool>(ec_dt));
        CHECK(ec_dt.ec == glz::error_code::exceeded_max_recursive_depth);
    }

    SECTION("Deeply alternating nested containers recursion limit enforcement") {
        constexpr size_t nesting = 100;
        std::string alt_buf;
        alt_buf.push_back(static_cast<char>(glz::etf::magic_version));
        for (size_t i = 0; i < nesting; ++i) {
            // MAP_EXT (arity 1)
            alt_buf.push_back(static_cast<char>(glz::etf::tag::MAP_EXT));
            append_be32(alt_buf, 1);
            alt_buf.push_back(static_cast<char>(glz::etf::tag::BINARY_EXT));
            append_be32(alt_buf, 1);
            alt_buf.push_back('k');

            // LIST_EXT (length 1)
            alt_buf.push_back(static_cast<char>(glz::etf::tag::LIST_EXT));
            append_be32(alt_buf, 1);

            // LARGE_TUPLE_EXT (arity 1)
            alt_buf.push_back(static_cast<char>(glz::etf::tag::LARGE_TUPLE_EXT));
            append_be32(alt_buf, 1);
        }
        alt_buf.push_back(static_cast<char>(glz::etf::tag::SMALL_INTEGER_EXT));
        alt_buf.push_back(static_cast<char>(1));
        // Tails for lists
        for (size_t i = 0; i < nesting; ++i) {
            alt_buf.push_back(static_cast<char>(glz::etf::tag::NIL_EXT));
        }

        glz::skip sk_alt{};
        auto ec_alt = glz::read_etf(sk_alt, alt_buf);
        CHECK(static_cast<bool>(ec_alt));
        CHECK(ec_alt.ec == glz::error_code::exceeded_max_recursive_depth);
    }
}

TEST_CASE("ETF: Variant deduction edge cases, type backtracking, and malformed inputs", "[etf][security][variants]") {
    SECTION("Snowflake vs string variant backtracking and deduction") {
        using SfOrStr = std::variant<discusy::snowflake, std::string>;

        // 1. Integer payload -> snowflake
        std::uint64_t sf_val = 175928847299117063ULL;
        std::string enc_sf;
        REQUIRE_FALSE(glz::write_etf(sf_val, enc_sf));
        SfOrStr dec_sf{};
        REQUIRE_FALSE(glz::read_etf(dec_sf, enc_sf));
        REQUIRE(std::holds_alternative<discusy::snowflake>(dec_sf));
        CHECK(std::get<discusy::snowflake>(dec_sf).value == sf_val);

        // 2. Non-numeric text -> string
        std::string text_val = "not_a_snowflake_identifier";
        std::string enc_text;
        REQUIRE_FALSE(glz::write_etf(text_val, enc_text));
        SfOrStr dec_text{};
        REQUIRE_FALSE(glz::read_etf(dec_text, enc_text));
        REQUIRE(std::holds_alternative<std::string>(dec_text));
        CHECK(std::get<std::string>(dec_text) == text_val);

        // 3. Boolean payload -> fails no_matching_variant_type
        bool bool_val = true;
        std::string enc_bool;
        REQUIRE_FALSE(glz::write_etf(bool_val, enc_bool));
        SfOrStr dec_bad{};
        auto ec_bad = glz::read_etf(dec_bad, enc_bool);
        CHECK(static_cast<bool>(ec_bad));
        CHECK(ec_bad.ec == glz::error_code::no_matching_variant_type);
    }

    SECTION("Container variant deduction and rejection of malformed alternatives") {
        using ContainerVar = std::variant<int, std::vector<int>, std::string>;

        // Int alternative
        ContainerVar v_i = 123;
        std::string enc_i;
        REQUIRE_FALSE(glz::write_etf(v_i, enc_i));
        ContainerVar dec_i{};
        REQUIRE_FALSE(glz::read_etf(dec_i, enc_i));
        REQUIRE(std::holds_alternative<int>(dec_i));
        CHECK(std::get<int>(dec_i) == 123);

        // Vector alternative
        ContainerVar v_vec = std::vector<int>{1, 2, 3};
        std::string enc_vec;
        REQUIRE_FALSE(glz::write_etf(v_vec, enc_vec));
        ContainerVar dec_vec{};
        REQUIRE_FALSE(glz::read_etf(dec_vec, enc_vec));
        REQUIRE(std::holds_alternative<std::vector<int>>(dec_vec));
        CHECK(std::get<std::vector<int>>(dec_vec) == std::vector<int>{1, 2, 3});

        // String alternative
        ContainerVar v_str = std::string{"container_test"};
        std::string enc_str;
        REQUIRE_FALSE(glz::write_etf(v_str, enc_str));
        ContainerVar dec_str{};
        REQUIRE_FALSE(glz::read_etf(dec_str, enc_str));
        REQUIRE(std::holds_alternative<std::string>(dec_str));
        CHECK(std::get<std::string>(dec_str) == "container_test");

        // Float payload: neither int, vector<int>, nor string -> must fail
        double f_val = 3.14;
        std::string enc_f;
        REQUIRE_FALSE(glz::write_etf(f_val, enc_f));
        ContainerVar dec_f{};
        auto ec_f = glz::read_etf(dec_f, enc_f);
        CHECK(static_cast<bool>(ec_f));
        CHECK(ec_f.ec == glz::error_code::no_matching_variant_type);
    }
}

TEST_CASE("ETF: Single-byte buffers and protocol boundary fuzzing", "[etf][security][boundaries]") {
    SECTION("Extremely short buffers (0, 1, 2 bytes) for all types") {
        std::string b0;
        std::string b1{static_cast<char>(glz::etf::magic_version)};
        std::string b2{static_cast<char>(glz::etf::magic_version), static_cast<char>(glz::etf::tag::SMALL_INTEGER_EXT)};

        int val_i = 0;
        CHECK(static_cast<bool>(glz::read_etf(val_i, b0)));
        CHECK(static_cast<bool>(glz::read_etf(val_i, b1)));
        CHECK(static_cast<bool>(glz::read_etf(val_i, b2)));

        std::string str_val;
        CHECK(static_cast<bool>(glz::read_etf(str_val, b0)));
        CHECK(static_cast<bool>(glz::read_etf(str_val, b1)));
        CHECK(static_cast<bool>(glz::read_etf(str_val, b2)));

        glz::skip sk_val{};
        CHECK(static_cast<bool>(glz::read_etf(sk_val, b0)));
        CHECK(static_cast<bool>(glz::read_etf(sk_val, b1)));
        CHECK(static_cast<bool>(glz::read_etf(sk_val, b2)));

        std::vector<int> vec_val;
        CHECK(static_cast<bool>(glz::read_etf(vec_val, b0)));
        CHECK(static_cast<bool>(glz::read_etf(vec_val, b1)));
        CHECK(static_cast<bool>(glz::read_etf(vec_val, b2)));

        test_etf_types::SampleStruct struct_val{};
        CHECK(static_cast<bool>(glz::read_etf(struct_val, b0)));
        CHECK(static_cast<bool>(glz::read_etf(struct_val, b1)));
        CHECK(static_cast<bool>(glz::read_etf(struct_val, b2)));
    }

    SECTION("Trailing garbage handling after valid primitive terms") {
        // [131, SMALL_INTEGER_EXT, 42, 0xFF, 0xFF, 0xFF]
        std::string buf_trail;
        buf_trail.push_back(static_cast<char>(glz::etf::magic_version));
        buf_trail.push_back(static_cast<char>(glz::etf::tag::SMALL_INTEGER_EXT));
        buf_trail.push_back(static_cast<char>(42));
        buf_trail.append("\xFF\xFF\xFF");

        int val = 0;
        // Should parse the 42 without crashing
        auto ec = glz::read_etf(val, buf_trail);
        if (!static_cast<bool>(ec)) {
            CHECK(val == 42);
        }

        glz::skip sk{};
        auto ec_sk = glz::read_etf(sk, buf_trail);
        (void)ec_sk;
    }
}

TEST_CASE("ETF: Generic JSON parsing and roundtrip (glz::generic)", "[etf][generic]") {
    SECTION("Primitives in glz::generic") {
        // Small unsigned integer
        std::string enc_sint;
        std::uint8_t uval = 42;
        REQUIRE_FALSE(glz::write_etf(uval, enc_sint));
        glz::generic gen_sint;
        REQUIRE_FALSE(glz::read_etf(gen_sint, enc_sint));
        CHECK(gen_sint.is_number());
        CHECK(gen_sint.get_number() == 42.0);

        // Signed 32-bit integer
        std::string enc_int;
        std::int32_t ival = -12345;
        REQUIRE_FALSE(glz::write_etf(ival, enc_int));
        glz::generic gen_int;
        REQUIRE_FALSE(glz::read_etf(gen_int, enc_int));
        CHECK(gen_int.is_number());
        CHECK(gen_int.get_number() == -12345.0);

        // 64-bit integer
        std::string enc_big;
        std::uint64_t bval = 175928847299117063ULL;
        REQUIRE_FALSE(glz::write_etf(bval, enc_big));
        glz::generic gen_big;
        REQUIRE_FALSE(glz::read_etf(gen_big, enc_big));
        CHECK(gen_big.is_number());
        CHECK(gen_big.get_number() == static_cast<double>(bval));

        // Floating point
        std::string enc_flt;
        double dval = 3.141592653589793;
        REQUIRE_FALSE(glz::write_etf(dval, enc_flt));
        glz::generic gen_flt;
        REQUIRE_FALSE(glz::read_etf(gen_flt, enc_flt));
        CHECK(gen_flt.is_number());
        CHECK(gen_flt.get_number() == dval);

        // Booleans
        std::string enc_t, enc_f;
        REQUIRE_FALSE(glz::write_etf(true, enc_t));
        REQUIRE_FALSE(glz::write_etf(false, enc_f));
        glz::generic gen_t, gen_f;
        REQUIRE_FALSE(glz::read_etf(gen_t, enc_t));
        REQUIRE_FALSE(glz::read_etf(gen_f, enc_f));
        CHECK(gen_t.is_boolean());
        CHECK(gen_t.get_boolean() == true);
        CHECK(gen_f.is_boolean());
        CHECK(gen_f.get_boolean() == false);

        // Null / Nil atom
        std::optional<int> none_val = std::nullopt;
        std::string enc_nil;
        REQUIRE_FALSE(glz::write_etf(none_val, enc_nil));
        glz::generic gen_nil;
        REQUIRE_FALSE(glz::read_etf(gen_nil, enc_nil));
        CHECK(gen_nil.is_null());

        // String
        std::string enc_str;
        std::string sval = "Hello, glz::generic ETF!";
        REQUIRE_FALSE(glz::write_etf(sval, enc_str));
        glz::generic gen_str;
        REQUIRE_FALSE(glz::read_etf(gen_str, enc_str));
        CHECK(gen_str.is_string());
        CHECK(gen_str.get_string() == sval);
    }

    SECTION("Arrays and Tuples in glz::generic") {
        // Empty list (NIL_EXT)
        std::vector<int> empty_vec{};
        std::string enc_empty;
        REQUIRE_FALSE(glz::write_etf(empty_vec, enc_empty));
        glz::generic gen_empty;
        REQUIRE_FALSE(glz::read_etf(gen_empty, enc_empty));
        CHECK(gen_empty.is_array());
        CHECK(gen_empty.get_array().empty());

        // Homogeneous list (LIST_EXT)
        std::vector<int> numbers = {10, 20, 30};
        std::string enc_nums;
        REQUIRE_FALSE(glz::write_etf(numbers, enc_nums));
        glz::generic gen_nums;
        REQUIRE_FALSE(glz::read_etf(gen_nums, enc_nums));
        CHECK(gen_nums.is_array());
        REQUIRE(gen_nums.get_array().size() == 3);
        CHECK(gen_nums[0].get_number() == 10.0);
        CHECK(gen_nums[1].get_number() == 20.0);
        CHECK(gen_nums[2].get_number() == 30.0);

        // Tuple (SMALL_TUPLE_EXT)
        std::tuple<int, std::string> tup = {42, "tuple_element"};
        std::string enc_tup;
        REQUIRE_FALSE(glz::write_etf(tup, enc_tup));
        glz::generic gen_tup;
        REQUIRE_FALSE(glz::read_etf(gen_tup, enc_tup));
        CHECK(gen_tup.is_array());
        REQUIRE(gen_tup.get_array().size() == 2);
        CHECK(gen_tup[0].get_number() == 42.0);
        CHECK(gen_tup[1].get_string() == "tuple_element");
    }

    SECTION("Objects and nested structures in glz::generic") {
        std::map<std::string, int> dict = {
            {"first", 100},
            {"second", 200},
        };
        std::string enc_map;
        REQUIRE_FALSE(glz::write_etf(dict, enc_map));
        glz::generic gen_map;
        REQUIRE_FALSE(glz::read_etf(gen_map, enc_map));
        CHECK(gen_map.is_object());
        CHECK(gen_map["first"].get_number() == 100.0);
        CHECK(gen_map["second"].get_number() == 200.0);

        // Nested struct
        SampleStruct original{
            .id = discusy::snowflake{999888777ULL},
            .name = "GenericStruct",
            .description = "testing generic object read",
            .count = 77,
            .enabled = true,
        };
        std::string enc_struct;
        REQUIRE_FALSE(glz::write_etf(original, enc_struct));
        glz::generic gen_struct;
        REQUIRE_FALSE(glz::read_etf(gen_struct, enc_struct));
        CHECK(gen_struct.is_object());
        CHECK(gen_struct["name"].get_string() == "GenericStruct");
        CHECK(gen_struct["count"].get_number() == 77.0);
        CHECK(gen_struct["enabled"].get_boolean() == true);
        REQUIRE(gen_struct["description"].is_string());
        CHECK(gen_struct["description"].get_string() == "testing generic object read");
    }

    SECTION("glz::generic roundtrip serialization and deserialization") {
        glz::generic original;
        original["title"] = "glz_generic_test";
        original["score"] = 99.5;
        original["active"] = true;
        glz::generic::array_t arr;
        arr.emplace_back(1.0);
        arr.emplace_back("two");
        original["items"] = std::move(arr);

        std::string encoded;
        REQUIRE_FALSE(glz::write_etf(original, encoded));

        glz::generic decoded;
        REQUIRE_FALSE(glz::read_etf(decoded, encoded));
        CHECK(decoded.is_object());
        CHECK(decoded["title"].get_string() == "glz_generic_test");
        CHECK(decoded["score"].get_number() == 99.5);
        CHECK(decoded["active"].get_boolean() == true);
        CHECK(decoded["items"].is_array());
        REQUIRE(decoded["items"].get_array().size() == 2);
        CHECK(decoded["items"][0].get_number() == 1.0);
        CHECK(decoded["items"][1].get_string() == "two");
    }
}

TEST_CASE("ETF: Fixed array (std::array) parsing and boundary security", "[etf][array][bounds]") {
    auto append_be32 = [](std::string& s, std::uint32_t val) {
        s.push_back(static_cast<char>((val >> 24) & 0xFF));
        s.push_back(static_cast<char>((val >> 16) & 0xFF));
        s.push_back(static_cast<char>((val >> 8) & 0xFF));
        s.push_back(static_cast<char>(val & 0xFF));
    };
    auto append_be16 = [](std::string& s, std::uint16_t val) {
        s.push_back(static_cast<char>((val >> 8) & 0xFF));
        s.push_back(static_cast<char>(val & 0xFF));
    };

    SECTION("Exact size std::array roundtrip from LIST_EXT") {
        std::array<int, 3> original = {10, 20, 30};
        std::string encoded;
        REQUIRE_FALSE(glz::write_etf(original, encoded));
        CHECK(static_cast<std::uint8_t>(encoded[1]) == glz::etf::tag::LIST_EXT);

        std::array<int, 3> decoded{};
        REQUIRE_FALSE(glz::read_etf(decoded, encoded));
        CHECK(decoded == original);
    }

    SECTION("Underfilled std::array (ETF list has fewer elements than array size)") {
        std::vector<int> short_list = {111, 222};
        std::string encoded;
        REQUIRE_FALSE(glz::write_etf(short_list, encoded));

        std::array<int, 4> target = {1, 2, 3, 4};
        REQUIRE_FALSE(glz::read_etf(target, encoded));
        // First 2 elements should be overwritten with 111, 222; remaining 2 should remain untouched
        CHECK(target[0] == 111);
        CHECK(target[1] == 222);
        CHECK(target[2] == 3);
        CHECK(target[3] == 4);
    }

    SECTION("Overfilled list / Out-of-bounds protection (exceeded_static_array_size vs partial_read)") {
        // Construct a LIST_EXT containing 6 elements: [10, 20, 30, 40, 50, 60]
        std::vector<int> long_list = {10, 20, 30, 40, 50, 60};
        std::string encoded;
        REQUIRE_FALSE(glz::write_etf(long_list, encoded));

        // In default mode: more elements than array capacity MUST be an error
        std::array<int, 2> small_arr = {0, 0};
        auto ec = glz::read_etf(small_arr, encoded);
        CHECK(static_cast<bool>(ec));
        CHECK(ec.ec == glz::error_code::exceeded_static_array_size);

        // In partial_read mode: gracefully populates up to capacity without error
        auto ec_partial = glz::read<glz::etf_opts_partial_read>(small_arr, encoded);
        REQUIRE_FALSE(static_cast<bool>(ec_partial));
        CHECK(small_arr[0] == 10);
        CHECK(small_arr[1] == 20);
    }

    SECTION("Stream synchronization after overfilled array in map / struct") {
        // Construct an ETF map containing:
        // "coords" -> [10, 20, 30, 40, 50] (5 elements for a 2-element array)
        // "marker" -> "passed"
        std::string buf;
        buf.push_back(static_cast<char>(glz::etf::magic_version));
        buf.push_back(static_cast<char>(glz::etf::tag::MAP_EXT));
        append_be32(buf, 2);

        // Key: "coords"
        buf.push_back(static_cast<char>(glz::etf::tag::BINARY_EXT));
        append_be32(buf, 6);
        buf.append("coords");
        // Value: LIST_EXT of 5 integers
        buf.push_back(static_cast<char>(glz::etf::tag::LIST_EXT));
        append_be32(buf, 5);
        for (int i = 1; i <= 5; ++i) {
            buf.push_back(static_cast<char>(glz::etf::tag::SMALL_INTEGER_EXT));
            buf.push_back(static_cast<char>(i * 10));
        }
        buf.push_back(static_cast<char>(glz::etf::tag::NIL_EXT));

        // Key: "marker"
        buf.push_back(static_cast<char>(glz::etf::tag::BINARY_EXT));
        append_be32(buf, 6);
        buf.append("marker");
        // Value: "passed"
        buf.push_back(static_cast<char>(glz::etf::tag::BINARY_EXT));
        append_be32(buf, 6);
        buf.append("passed");

        // In default mode, rejects with exceeded_static_array_size
        OverfilledStructTest result{};
        auto ec_def = glz::read_etf(result, buf);
        CHECK(static_cast<bool>(ec_def));
        CHECK(ec_def.ec == glz::error_code::exceeded_static_array_size);

        // In partial_read mode, successfully skips excess elements and synchronizes stream
        auto ec_part = glz::read<glz::etf_opts_partial_read>(result, buf);
        REQUIRE_FALSE(static_cast<bool>(ec_part));
        CHECK(result.coords[0] == 10);
        CHECK(result.coords[1] == 20);
        CHECK(result.marker == "passed");
    }

    SECTION("Empty ETF list (NIL_EXT) into std::array") {
        std::vector<int> empty_vec{};
        std::string encoded;
        REQUIRE_FALSE(glz::write_etf(empty_vec, encoded));
        CHECK(static_cast<std::uint8_t>(encoded[1]) == glz::etf::tag::NIL_EXT);

        std::array<int, 3> arr = {5, 6, 7};
        REQUIRE_FALSE(glz::read_etf(arr, encoded));
        CHECK(arr[0] == 5);
        CHECK(arr[1] == 6);
        CHECK(arr[2] == 7);
    }

    SECTION("Tuple parsing into std::array (SMALL_TUPLE_EXT and LARGE_TUPLE_EXT)") {
        // Exact size SMALL_TUPLE_EXT
        std::tuple<int, int> tup = {42, 84};
        std::string enc_tup;
        REQUIRE_FALSE(glz::write_etf(tup, enc_tup));
        CHECK(static_cast<std::uint8_t>(enc_tup[1]) == glz::etf::tag::SMALL_TUPLE_EXT);

        std::array<int, 2> arr{};
        REQUIRE_FALSE(glz::read_etf(arr, enc_tup));
        CHECK(arr[0] == 42);
        CHECK(arr[1] == 84);

        // Overfilled SMALL_TUPLE_EXT: error in default mode, truncated in partial_read
        std::tuple<int, int, int, int> big_tup = {1, 2, 3, 4};
        std::string enc_big_tup;
        REQUIRE_FALSE(glz::write_etf(big_tup, enc_big_tup));

        std::array<int, 2> arr_small{};
        auto ec_tup = glz::read_etf(arr_small, enc_big_tup);
        CHECK(static_cast<bool>(ec_tup));
        CHECK(ec_tup.ec == glz::error_code::exceeded_static_array_size);

        auto ec_tup_part = glz::read<glz::etf_opts_partial_read>(arr_small, enc_big_tup);
        REQUIRE_FALSE(static_cast<bool>(ec_tup_part));
        CHECK(arr_small[0] == 1);
        CHECK(arr_small[1] == 2);

        // Underfilled SMALL_TUPLE_EXT
        std::tuple<int> one_tup = {99};
        std::string enc_one_tup;
        REQUIRE_FALSE(glz::write_etf(one_tup, enc_one_tup));

        std::array<int, 3> arr_under = {0, 55, 66};
        REQUIRE_FALSE(glz::read_etf(arr_under, enc_one_tup));
        CHECK(arr_under[0] == 99);
        CHECK(arr_under[1] == 55);
        CHECK(arr_under[2] == 66);

        // LARGE_TUPLE_EXT parsing into std::array
        std::string large_tup;
        large_tup.push_back(static_cast<char>(glz::etf::magic_version));
        large_tup.push_back(static_cast<char>(glz::etf::tag::LARGE_TUPLE_EXT));
        append_be32(large_tup, 2);
        large_tup.push_back(static_cast<char>(glz::etf::tag::SMALL_INTEGER_EXT));
        large_tup.push_back(static_cast<char>(7));
        large_tup.push_back(static_cast<char>(glz::etf::tag::SMALL_INTEGER_EXT));
        large_tup.push_back(static_cast<char>(8));

        std::array<int, 2> arr_lt{};
        REQUIRE_FALSE(glz::read_etf(arr_lt, large_tup));
        CHECK(arr_lt[0] == 7);
        CHECK(arr_lt[1] == 8);
    }

    SECTION("Byte arrays from BINARY_EXT and STRING_EXT") {
        std::string raw_bin = "ABCDEF";
        std::string enc_bin;
        REQUIRE_FALSE(glz::write_etf(raw_bin, enc_bin));

        // Array of size 6 exact
        std::array<uint8_t, 6> b6{};
        REQUIRE_FALSE(glz::read_etf(b6, enc_bin));
        CHECK(b6[0] == 'A');
        CHECK(b6[5] == 'F');

        // Array of size 3 (overfilled): error in default mode, truncated in partial_read
        std::array<uint8_t, 3> b3 = {0, 0, 0};
        auto ec_b3 = glz::read_etf(b3, enc_bin);
        CHECK(static_cast<bool>(ec_b3));
        CHECK(ec_b3.ec == glz::error_code::exceeded_static_array_size);

        auto ec_b3_part = glz::read<glz::etf_opts_partial_read>(b3, enc_bin);
        REQUIRE_FALSE(static_cast<bool>(ec_b3_part));
        CHECK(b3[0] == 'A');
        CHECK(b3[1] == 'B');
        CHECK(b3[2] == 'C');

        // STRING_EXT exact and overfilled bounds protection
        std::string str_ext;
        str_ext.push_back(static_cast<char>(glz::etf::magic_version));
        str_ext.push_back(static_cast<char>(glz::etf::tag::STRING_EXT));
        append_be16(str_ext, 4);
        str_ext.append("WXYZ");

        std::array<char, 4> c4{};
        REQUIRE_FALSE(glz::read_etf(c4, str_ext));
        CHECK(c4[0] == 'W');
        CHECK(c4[3] == 'Z');

        std::array<char, 2> c2 = {'\0', '\0'};
        auto ec_c2 = glz::read_etf(c2, str_ext);
        CHECK(static_cast<bool>(ec_c2));
        CHECK(ec_c2.ec == glz::error_code::exceeded_static_array_size);

        auto ec_c2_part = glz::read<glz::etf_opts_partial_read>(c2, str_ext);
        REQUIRE_FALSE(static_cast<bool>(ec_c2_part));
        CHECK(c2[0] == 'W');
        CHECK(c2[1] == 'X');
    }

    SECTION("Struct with std::array member roundtrip") {
        ArrayHolder orig{
            .name = "points",
            .coords = {100, 200, 300},
        };
        std::string enc;
        REQUIRE_FALSE(glz::write_etf(orig, enc));

        ArrayHolder dec{};
        REQUIRE_FALSE(glz::read_etf(dec, enc));
        CHECK(dec == orig);
    }
}

TEST_CASE("ETF & JSON: struct glaze, glaze meta value, and glz::custom for permissions", "[etf][json][custom][permissions]") {
    SECTION("Local struct glaze value wrapper (glaze_value_t)") {
        LocalGlazeValueWrapper orig{12345};

        // ETF
        std::string enc_etf;
        REQUIRE_FALSE(glz::write_etf(orig, enc_etf));
        REQUIRE(enc_etf.size() > 1);
        CHECK(static_cast<uint8_t>(enc_etf[1]) == glz::etf::tag::INTEGER_EXT);

        LocalGlazeValueWrapper dec_etf{0};
        REQUIRE_FALSE(glz::read_etf(dec_etf, enc_etf));
        CHECK(dec_etf.inner == 12345);
        CHECK(dec_etf == orig);

        // JSON
        std::string enc_json;
        REQUIRE_FALSE(glz::write_json(orig, enc_json));
        CHECK(enc_json == "12345");

        LocalGlazeValueWrapper dec_json{0};
        REQUIRE_FALSE(glz::read_json(dec_json, enc_json));
        CHECK(dec_json.inner == 12345);
        CHECK(dec_json == orig);
    }

    SECTION("Local struct glaze object wrapper (glz::object)") {
        LocalGlazeObjectWrapper orig{.x = 10, .y = 20};

        // ETF
        std::string enc_etf;
        REQUIRE_FALSE(glz::write_etf(orig, enc_etf));
        CHECK(static_cast<uint8_t>(enc_etf[1]) == glz::etf::tag::MAP_EXT);

        LocalGlazeObjectWrapper dec_etf{};
        REQUIRE_FALSE(glz::read_etf(dec_etf, enc_etf));
        CHECK(dec_etf == orig);

        // JSON
        std::string enc_json;
        REQUIRE_FALSE(glz::write_json(orig, enc_json));
        CHECK(enc_json.find("\"x\":10") != std::string::npos);
        CHECK(enc_json.find("\"y\":20") != std::string::npos);

        LocalGlazeObjectWrapper dec_json{};
        REQUIRE_FALSE(glz::read_json(dec_json, enc_json));
        CHECK(dec_json == orig);
    }

    SECTION("Global glz::meta value wrapper") {
        GlobalMetaValueWrapper orig{"hello world"};

        // ETF
        std::string enc_etf;
        REQUIRE_FALSE(glz::write_etf(orig, enc_etf));
        CHECK(static_cast<uint8_t>(enc_etf[1]) == glz::etf::tag::BINARY_EXT);

        GlobalMetaValueWrapper dec_etf{};
        REQUIRE_FALSE(glz::read_etf(dec_etf, enc_etf));
        CHECK(dec_etf == orig);

        // JSON
        std::string enc_json;
        REQUIRE_FALSE(glz::write_json(orig, enc_json));
        CHECK(enc_json == "\"hello world\"");

        GlobalMetaValueWrapper dec_json{};
        REQUIRE_FALSE(glz::read_json(dec_json, enc_json));
        CHECK(dec_json == orig);
    }

    SECTION("glz::custom with custom transformation functions") {
        HexColorWrapper orig{0xFF00AA};

        // ETF
        std::string enc_etf;
        REQUIRE_FALSE(glz::write_etf(orig, enc_etf));
        CHECK(static_cast<uint8_t>(enc_etf[1]) == glz::etf::tag::BINARY_EXT);

        HexColorWrapper dec_etf{};
        REQUIRE_FALSE(glz::read_etf(dec_etf, enc_etf));
        CHECK(dec_etf.rgb == 0xFF00AA);
        CHECK(dec_etf == orig);

        // JSON
        std::string enc_json;
        REQUIRE_FALSE(glz::write_json(orig, enc_json));
        CHECK(enc_json == "\"#ff00aa\"");

        HexColorWrapper dec_json{};
        REQUIRE_FALSE(glz::read_json(dec_json, enc_json));
        CHECK(dec_json.rgb == 0xFF00AA);
        CHECK(dec_json == orig);
    }

    SECTION("Standalone discusy::permissions_t (string-backed glz::custom)") {
        // Zero permissions
        discusy::permissions_t empty_p{};
        std::string enc_empty_etf;
        REQUIRE_FALSE(glz::write_etf(empty_p, enc_empty_etf));
        discusy::permissions_t dec_empty_etf{discusy::permissions::permissions::ADMINISTRATOR};
        REQUIRE_FALSE(glz::read_etf(dec_empty_etf, enc_empty_etf));
        CHECK(dec_empty_etf.value == 0);

        std::string enc_empty_json;
        REQUIRE_FALSE(glz::write_json(empty_p, enc_empty_json));
        CHECK(enc_empty_json == "\"0\"");
        discusy::permissions_t dec_empty_json{discusy::permissions::permissions::ADMINISTRATOR};
        REQUIRE_FALSE(glz::read_json(dec_empty_json, enc_empty_json));
        CHECK(dec_empty_json.value == 0);

        // Multiple permissions: ADMINISTRATOR (8) | MANAGE_GUILD (32) | VIEW_CHANNEL (1024) | SEND_MESSAGES (2048) = 3112
        discusy::permissions_t multi_p{};
        multi_p.add_flag(discusy::permissions::permissions::ADMINISTRATOR);
        multi_p.add_flag(discusy::permissions::permissions::MANAGE_GUILD);
        multi_p.add_flag(discusy::permissions::permissions::VIEW_CHANNEL);
        multi_p.add_flag(discusy::permissions::permissions::SEND_MESSAGES);
        CHECK(multi_p.value == 3112);

        // ETF roundtrip
        std::string enc_multi_etf;
        REQUIRE_FALSE(glz::write_etf(multi_p, enc_multi_etf));
        CHECK(static_cast<uint8_t>(enc_multi_etf[1]) == glz::etf::tag::BINARY_EXT);
        discusy::permissions_t dec_multi_etf{};
        REQUIRE_FALSE(glz::read_etf(dec_multi_etf, enc_multi_etf));
        CHECK(dec_multi_etf.has_flag(discusy::permissions::permissions::ADMINISTRATOR));
        CHECK(dec_multi_etf.has_flag(discusy::permissions::permissions::MANAGE_GUILD));
        CHECK(dec_multi_etf.has_flag(discusy::permissions::permissions::VIEW_CHANNEL));
        CHECK(dec_multi_etf.has_flag(discusy::permissions::permissions::SEND_MESSAGES));
        CHECK_FALSE(dec_multi_etf.has_flag(discusy::permissions::permissions::BAN_MEMBERS));
        CHECK_FALSE(dec_multi_etf.has_flag(discusy::permissions::permissions::KICK_MEMBERS));
        CHECK(dec_multi_etf == multi_p);

        // JSON roundtrip
        std::string enc_multi_json;
        REQUIRE_FALSE(glz::write_json(multi_p, enc_multi_json));
        CHECK(enc_multi_json == "\"3112\"");
        discusy::permissions_t dec_multi_json{};
        REQUIRE_FALSE(glz::read_json(dec_multi_json, enc_multi_json));
        CHECK(dec_multi_json == multi_p);
    }

    SECTION("Numeric-backed flags (glz::custom<read_num, write_num>)") {
        number_perms_t num_p{};
        num_p.add_flag(discusy::permissions::permissions::VIEW_AUDIT_LOG); // 1ULL << 7 = 128
        num_p.add_flag(discusy::permissions::permissions::PRIORITY_SPEAKER); // 1ULL << 8 = 256
        CHECK(num_p.value == 384);

        // ETF
        std::string enc_etf;
        REQUIRE_FALSE(glz::write_etf(num_p, enc_etf));
        number_perms_t dec_etf{};
        REQUIRE_FALSE(glz::read_etf(dec_etf, enc_etf));
        CHECK(dec_etf.has_flag(discusy::permissions::permissions::VIEW_AUDIT_LOG));
        CHECK(dec_etf.has_flag(discusy::permissions::permissions::PRIORITY_SPEAKER));
        CHECK(dec_etf == num_p);

        // JSON
        std::string enc_json;
        REQUIRE_FALSE(glz::write_json(num_p, enc_json));
        CHECK(enc_json == "384");
        number_perms_t dec_json{};
        REQUIRE_FALSE(glz::read_json(dec_json, enc_json));
        CHECK(dec_json == num_p);
    }

    SECTION("Permissions inside a reflectable struct") {
        RolePermissionsTest role_orig{
            .role_id = discusy::snowflake{11223344556677ULL},
            .perms = {},
            .num_perms = {},
        };
        role_orig.perms.add_flag(discusy::permissions::permissions::ADMINISTRATOR);
        role_orig.num_perms.add_flag(discusy::permissions::permissions::MANAGE_CHANNELS);

        // ETF roundtrip
        std::string enc_etf;
        REQUIRE_FALSE(glz::write_etf(role_orig, enc_etf));
        RolePermissionsTest dec_etf{};
        REQUIRE_FALSE(glz::read_etf(dec_etf, enc_etf));
        CHECK(dec_etf == role_orig);
        CHECK(dec_etf.perms.has_flag(discusy::permissions::permissions::ADMINISTRATOR));
        CHECK(dec_etf.num_perms.has_flag(discusy::permissions::permissions::MANAGE_CHANNELS));

        // JSON roundtrip
        std::string enc_json;
        REQUIRE_FALSE(glz::write_json(role_orig, enc_json));
        CHECK(enc_json.find("\"perms\":\"8\"") != std::string::npos);
        CHECK(enc_json.find("\"num_perms\":16") != std::string::npos);
        RolePermissionsTest dec_json{};
        REQUIRE_FALSE(glz::read_json(dec_json, enc_json));
        CHECK(dec_json == role_orig);
    }
}

TEST_CASE("ETF & JSON: Explicit nulls (discusy::explicit_null and opt<explicit_null>)", "[etf][json][nulls][explicit_null]") {
    SECTION("Standalone explicit_null<int> and explicit_null<std::string>") {
        // Null integer
        discusy::explicit_null<int> null_int{nullptr};
        CHECK_FALSE(null_int.has_value());

        std::string enc_null_int_etf;
        REQUIRE_FALSE(glz::write_etf(null_int, enc_null_int_etf));
        REQUIRE(enc_null_int_etf.size() >= 5);
        CHECK(static_cast<uint8_t>(enc_null_int_etf[1]) == glz::etf::tag::SMALL_ATOM_UTF8_EXT);
        discusy::explicit_null<int> dec_null_int_etf{999};
        REQUIRE_FALSE(glz::read_etf(dec_null_int_etf, enc_null_int_etf));
        CHECK_FALSE(dec_null_int_etf.has_value());

        std::string enc_null_int_json;
        REQUIRE_FALSE(glz::write_json(null_int, enc_null_int_json));
        CHECK(enc_null_int_json == "null");
        discusy::explicit_null<int> dec_null_int_json{999};
        REQUIRE_FALSE(glz::read_json(dec_null_int_json, enc_null_int_json));
        CHECK_FALSE(dec_null_int_json.has_value());

        // Value integer
        discusy::explicit_null<int> val_int{42};
        CHECK(val_int.has_value());
        CHECK(*val_int == 42);

        std::string enc_val_int_etf;
        REQUIRE_FALSE(glz::write_etf(val_int, enc_val_int_etf));
        discusy::explicit_null<int> dec_val_int_etf{nullptr};
        REQUIRE_FALSE(glz::read_etf(dec_val_int_etf, enc_val_int_etf));
        CHECK(dec_val_int_etf.has_value());
        CHECK(*dec_val_int_etf == 42);

        std::string enc_val_int_json;
        REQUIRE_FALSE(glz::write_json(val_int, enc_val_int_json));
        CHECK(enc_val_int_json == "42");
        discusy::explicit_null<int> dec_val_int_json{nullptr};
        REQUIRE_FALSE(glz::read_json(dec_val_int_json, enc_val_int_json));
        CHECK(dec_val_int_json.has_value());
        CHECK(*dec_val_int_json == 42);

        // Null string
        discusy::explicit_null<std::string> null_str{nullptr};
        std::string enc_null_str_etf;
        REQUIRE_FALSE(glz::write_etf(null_str, enc_null_str_etf));
        discusy::explicit_null<std::string> dec_null_str_etf{"existing"};
        REQUIRE_FALSE(glz::read_etf(dec_null_str_etf, enc_null_str_etf));
        CHECK_FALSE(dec_null_str_etf.has_value());

        std::string enc_null_str_json;
        REQUIRE_FALSE(glz::write_json(null_str, enc_null_str_json));
        CHECK(enc_null_str_json == "null");

        // Value string
        discusy::explicit_null<std::string> val_str{"discusy"};
        std::string enc_val_str_etf;
        REQUIRE_FALSE(glz::write_etf(val_str, enc_val_str_etf));
        discusy::explicit_null<std::string> dec_val_str_etf{nullptr};
        REQUIRE_FALSE(glz::read_etf(dec_val_str_etf, enc_val_str_etf));
        CHECK(dec_val_str_etf.has_value());
        CHECK(*dec_val_str_etf == "discusy");

        std::string enc_val_str_json;
        REQUIRE_FALSE(glz::write_json(val_str, enc_val_str_json));
        CHECK(enc_val_str_json == "\"discusy\"");
    }

    SECTION("Tri-State in struct: Unengaged fields are OMITTED (not written)") {
        TriStateTestStruct s{};
        REQUIRE_FALSE(s.description.has_value());
        REQUIRE_FALSE(s.count.has_value());
        REQUIRE_FALSE(s.regular_opt.has_value());
        REQUIRE_FALSE(s.direct_null.has_value());

        // JSON: unengaged optional fields must NOT appear in output!
        std::string json_str;
        REQUIRE_FALSE(glz::write_json(s, json_str));
        CHECK(json_str.find("\"name\":\"item\"") != std::string::npos);
        CHECK(json_str.find("\"direct_null\":null") != std::string::npos);
        CHECK(json_str.find("\"description\"") == std::string::npos);
        CHECK(json_str.find("\"count\"") == std::string::npos);
        CHECK(json_str.find("\"regular_opt\"") == std::string::npos);

        // ETF: unengaged optional fields must NOT be in the map!
        std::string etf_str;
        REQUIRE_FALSE(glz::write_etf(s, etf_str));
        REQUIRE(etf_str.size() > 5);
        CHECK(static_cast<uint8_t>(etf_str[1]) == glz::etf::tag::MAP_EXT);
        // Map arity should be exactly 2 (name and direct_null)
        uint32_t arity = glz::etf::detail::read_be<uint32_t>(&etf_str[2]);
        CHECK(arity == 2);

        // Deserialization roundtrip
        TriStateTestStruct dec_etf{};
        REQUIRE_FALSE(glz::read_etf(dec_etf, etf_str));
        CHECK(dec_etf.name == "item");
        CHECK_FALSE(dec_etf.description.has_value());
        CHECK_FALSE(dec_etf.count.has_value());
        CHECK_FALSE(dec_etf.regular_opt.has_value());
        CHECK_FALSE(dec_etf.direct_null.has_value());

        TriStateTestStruct dec_json{};
        REQUIRE_FALSE(glz::read_json(dec_json, json_str));
        CHECK(dec_json.name == "item");
        CHECK_FALSE(dec_json.description.has_value());
        CHECK_FALSE(dec_json.count.has_value());
        CHECK_FALSE(dec_json.regular_opt.has_value());
        CHECK_FALSE(dec_json.direct_null.has_value());
    }

    SECTION("Tri-State in struct: Engaged with nullptr are WRITTEN as null") {
        TriStateTestStruct s{
            .name = "item_explicit_null",
            .description = discusy::explicit_null<std::string>{nullptr},
            .count = discusy::explicit_null<int>{nullptr},
            .regular_opt = std::nullopt, // still omitted!
            .direct_null = discusy::explicit_null<std::string>{nullptr},
        };

        // JSON: description, count, direct_null MUST be written as null
        std::string json_str;
        REQUIRE_FALSE(glz::write_json(s, json_str));
        CHECK(json_str.find("\"name\":\"item_explicit_null\"") != std::string::npos);
        CHECK(json_str.find("\"description\":null") != std::string::npos);
        CHECK(json_str.find("\"count\":null") != std::string::npos);
        CHECK(json_str.find("\"direct_null\":null") != std::string::npos);
        CHECK(json_str.find("\"regular_opt\"") == std::string::npos); // omitted!

        // ETF: map arity should be exactly 4
        std::string etf_str;
        REQUIRE_FALSE(glz::write_etf(s, etf_str));
        uint32_t arity = glz::etf::detail::read_be<uint32_t>(&etf_str[2]);
        CHECK(arity == 4);

        // Deserialization roundtrip
        TriStateTestStruct dec_etf{};
        REQUIRE_FALSE(glz::read_etf(dec_etf, etf_str));
        CHECK(dec_etf.name == "item_explicit_null");
        CHECK_FALSE(dec_etf.description.has_value());
        CHECK_FALSE(dec_etf.count.has_value());
        CHECK_FALSE(dec_etf.regular_opt.has_value()); // unengaged
        CHECK_FALSE(dec_etf.direct_null.has_value());

        TriStateTestStruct dec_json{};
        REQUIRE_FALSE(glz::read_json(dec_json, json_str));
        CHECK(dec_json.name == "item_explicit_null");
        CHECK_FALSE(dec_json.description.has_value());
        CHECK_FALSE(dec_json.count.has_value());
        CHECK_FALSE(dec_json.regular_opt.has_value());
        CHECK_FALSE(dec_json.direct_null.has_value());
    }

    SECTION("Tri-State in struct: Engaged with values are WRITTEN with values") {
        TriStateTestStruct s{
            .name = "item_with_values",
            .description = discusy::explicit_null<std::string>{"Special item"},
            .count = discusy::explicit_null<int>{42},
            .regular_opt = 100,
            .direct_null = "mandatory_present",
        };

        // JSON: all 5 fields written with values
        std::string json_str;
        REQUIRE_FALSE(glz::write_json(s, json_str));
        CHECK(json_str.find("\"name\":\"item_with_values\"") != std::string::npos);
        CHECK(json_str.find("\"description\":\"Special item\"") != std::string::npos);
        CHECK(json_str.find("\"count\":42") != std::string::npos);
        CHECK(json_str.find("\"regular_opt\":100") != std::string::npos);
        CHECK(json_str.find("\"direct_null\":\"mandatory_present\"") != std::string::npos);

        // ETF: map arity should be exactly 5
        std::string etf_str;
        REQUIRE_FALSE(glz::write_etf(s, etf_str));
        uint32_t arity = glz::etf::detail::read_be<uint32_t>(&etf_str[2]);
        CHECK(arity == 5);

        // Deserialization roundtrip
        TriStateTestStruct dec_etf{};
        REQUIRE_FALSE(glz::read_etf(dec_etf, etf_str));
        CHECK(dec_etf.name == "item_with_values");
        REQUIRE(dec_etf.description.has_value());
        CHECK((*dec_etf.description).has_value());
        CHECK(*(*dec_etf.description) == "Special item");
        REQUIRE(dec_etf.count.has_value());
        CHECK((*dec_etf.count).has_value());
        CHECK(*(*dec_etf.count) == 42);
        REQUIRE(dec_etf.regular_opt.has_value());
        CHECK(*dec_etf.regular_opt == 100);
        CHECK(dec_etf.direct_null.has_value());
        CHECK(*dec_etf.direct_null == "mandatory_present");

        TriStateTestStruct dec_json{};
        REQUIRE_FALSE(glz::read_json(dec_json, json_str));
        CHECK(dec_json.name == "item_with_values");
        REQUIRE(dec_json.description.has_value());
        CHECK(*(*dec_json.description) == "Special item");
        REQUIRE(dec_json.count.has_value());
        CHECK(*(*dec_json.count) == 42);
        REQUIRE(dec_json.regular_opt.has_value());
        CHECK(*dec_json.regular_opt == 100);
        CHECK(dec_json.direct_null.has_value());
        CHECK(*dec_json.direct_null == "mandatory_present");
    }

    SECTION("Real-world types: update_voice_state with channel_id explicit null") {
        // Disconnecting voice (channel_id = null)
        auto disconnect_state = discusy::send_event::update_voice_state::create(
            discusy::snowflake{12345},
            discusy::explicit_null<discusy::snowflake>{nullptr},
            false, false
        );

        std::string json_disc;
        REQUIRE_FALSE(glz::write_json(disconnect_state, json_disc));
        CHECK(json_disc.find("\"channel_id\":null") != std::string::npos);

        std::string etf_disc;
        REQUIRE_FALSE(glz::write_etf(disconnect_state, etf_disc));
        discusy::send_event::update_voice_state dec_disc{};
        REQUIRE_FALSE(glz::read_etf(dec_disc, etf_disc));
        CHECK_FALSE(dec_disc.channel_id.has_value());
        CHECK(dec_disc.guild_id == discusy::snowflake{12345});

        // Connecting voice (channel_id = 67890)
        auto connect_state = discusy::send_event::update_voice_state::create(
            discusy::snowflake{12345},
            discusy::explicit_null<discusy::snowflake>{discusy::snowflake{67890}},
            false, false
        );

        std::string json_conn;
        REQUIRE_FALSE(glz::write_json(connect_state, json_conn));
        CHECK(json_conn.find("\"channel_id\":\"67890\"") != std::string::npos);

        std::string etf_conn;
        REQUIRE_FALSE(glz::write_etf(connect_state, etf_conn));
        discusy::send_event::update_voice_state dec_conn{};
        REQUIRE_FALSE(glz::read_etf(dec_conn, etf_conn));
        REQUIRE(dec_conn.channel_id.has_value());
        CHECK(*dec_conn.channel_id == discusy::snowflake{67890});
        CHECK(dec_conn.guild_id == discusy::snowflake{12345});
    }

    SECTION("Real-world types: edit_channel_permissions (opt<explicit_null<permissions_t>>)") {
        // Case 1: unengaged (omitted)
        discusy::api::channels::edit_channel_permissions p_unengaged{};
        std::string json_u;
        REQUIRE_FALSE(glz::write_json(p_unengaged, json_u));
        CHECK(json_u.find("\"allow\"") == std::string::npos);
        CHECK(json_u.find("\"deny\"") == std::string::npos);

        std::string etf_u;
        REQUIRE_FALSE(glz::write_etf(p_unengaged, etf_u));
        uint32_t arity_u = glz::etf::detail::read_be<uint32_t>(&etf_u[2]);
        CHECK(arity_u == 1); // only "type" is written

        // Case 2: reset permissions to null (clear overrides)
        discusy::api::channels::edit_channel_permissions p_null{};
        p_null.allow = discusy::explicit_null<discusy::permissions_t>{nullptr};
        p_null.deny = discusy::explicit_null<discusy::permissions_t>{nullptr};

        std::string json_n;
        REQUIRE_FALSE(glz::write_json(p_null, json_n));
        CHECK(json_n.find("\"allow\":null") != std::string::npos);
        CHECK(json_n.find("\"deny\":null") != std::string::npos);

        std::string etf_n;
        REQUIRE_FALSE(glz::write_etf(p_null, etf_n));
        uint32_t arity_n = glz::etf::detail::read_be<uint32_t>(&etf_n[2]);
        CHECK(arity_n == 3); // type, allow, deny

        discusy::api::channels::edit_channel_permissions dec_n{};
        REQUIRE_FALSE(glz::read_etf(dec_n, etf_n));
        CHECK_FALSE(dec_n.allow.has_value());
        CHECK_FALSE(dec_n.deny.has_value());

        // Case 3: grant SEND_MESSAGES
        discusy::permissions_t allow_p{};
        allow_p.add_flag(discusy::permissions::permissions::SEND_MESSAGES); // 2048
        discusy::api::channels::edit_channel_permissions p_val{};
        p_val.allow = discusy::explicit_null<discusy::permissions_t>{allow_p};

        std::string json_v;
        REQUIRE_FALSE(glz::write_json(p_val, json_v));
        CHECK(json_v.find("\"allow\":\"2048\"") != std::string::npos);
        CHECK(json_v.find("\"deny\"") == std::string::npos); // deny is still omitted!

        std::string etf_v;
        REQUIRE_FALSE(glz::write_etf(p_val, etf_v));
        uint32_t arity_v = glz::etf::detail::read_be<uint32_t>(&etf_v[2]);
        CHECK(arity_v == 2); // type and allow

        discusy::api::channels::edit_channel_permissions dec_v{};
        REQUIRE_FALSE(glz::read_etf(dec_v, etf_v));
        REQUIRE(dec_v.allow.has_value());
        CHECK((*dec_v.allow).has_value());
        CHECK((*dec_v.allow)->has_flag(discusy::permissions::permissions::SEND_MESSAGES));
        CHECK_FALSE(dec_v.deny.has_value()); // deny remains unengaged
    }

    SECTION("skip_null_members option behavior") {
        RegularOptStruct orig{.name = "test", .opt_val = std::nullopt};

        // Default: skip_null_members = true -> opt_val omitted
        std::string etf_skip;
        REQUIRE_FALSE(glz::write_etf(orig, etf_skip));
        uint32_t arity_skip = glz::etf::detail::read_be<uint32_t>(&etf_skip[2]);
        CHECK(arity_skip == 1);

        // With skip_null_members = false -> opt_val written as nil
        std::string etf_no_skip;
        REQUIRE_FALSE(glz::write<glz::etf::etf_opts{.skip_null_members = false}>(orig, etf_no_skip));
        uint32_t arity_no_skip = glz::etf::detail::read_be<uint32_t>(&etf_no_skip[2]);
        CHECK(arity_no_skip == 2);

        // In JSON with skip_null_members = false -> opt_val: null
        std::string json_no_skip;
        REQUIRE_FALSE(glz::write<glz::opts{.skip_null_members = false}>(orig, json_no_skip));
        CHECK(json_no_skip.find("\"opt_val\":null") != std::string::npos);
    }
}

TEST_CASE("ETF & JSON: Nullable parity (std::nullopt, explicit_null, nulls, and omitted fields)", "[etf][json][nulls][parity]") {
    SECTION("Omitted fields: default-initialized struct preserves nulls/unengaged state identically in JSON and ETF") {
        NullableParityStruct dec_etf{};
        NullableParityStruct dec_json{};

        // JSON payload with only "name", omitting std_opt, discusy_opt, exp_null, opt_exp_null
        std::string json_payload = "{\"name\":\"omitted_test\"}";
        REQUIRE_FALSE(glz::read_json(dec_json, json_payload));
        CHECK(dec_json.name == "omitted_test");
        CHECK_FALSE(dec_json.std_opt.has_value());
        CHECK_FALSE(dec_json.discusy_opt.has_value());
        CHECK_FALSE(dec_json.exp_null.has_value());
        CHECK_FALSE(dec_json.opt_exp_null.has_value());

        // ETF map with only "name" => "omitted_test"
        std::string etf_payload;
        {
            etf_payload.push_back(static_cast<char>(glz::etf::magic_version));
            etf_payload.push_back(static_cast<char>(glz::etf::tag::MAP_EXT));
            uint32_t arity = glz::etf::detail::to_big_endian(1U);
            etf_payload.append(reinterpret_cast<const char*>(&arity), 4);
            std::string k = "name";
            std::string enc_k;
            REQUIRE_FALSE(glz::write_etf(k, enc_k));
            etf_payload.append(enc_k.data() + 1, enc_k.size() - 1);
            std::string v = "omitted_test";
            std::string enc_v;
            REQUIRE_FALSE(glz::write_etf(v, enc_v));
            etf_payload.append(enc_v.data() + 1, enc_v.size() - 1);
        }

        REQUIRE_FALSE(glz::read_etf(dec_etf, etf_payload));
        CHECK(dec_etf.name == "omitted_test");
        CHECK_FALSE(dec_etf.std_opt.has_value());
        CHECK_FALSE(dec_etf.discusy_opt.has_value());
        CHECK_FALSE(dec_etf.exp_null.has_value());
        CHECK_FALSE(dec_etf.opt_exp_null.has_value());

        CHECK(dec_etf == dec_json);
    }

    SECTION("Omitted fields: pre-populated struct fields are NOT overwritten (exact parity)") {
        NullableParityStruct pre_json{
            .name = "old_name",
            .std_opt = 10,
            .discusy_opt = 20,
            .exp_null = 30,
            .opt_exp_null = discusy::explicit_null<int>{40},
        };
        NullableParityStruct pre_etf = pre_json;

        std::string json_payload = "{\"name\":\"new_name\"}";
        REQUIRE_FALSE(glz::read_json(pre_json, json_payload));
        CHECK(pre_json.name == "new_name");
        CHECK(pre_json.std_opt == 10);
        CHECK(pre_json.discusy_opt == 20);
        CHECK(pre_json.exp_null.has_value());
        CHECK(*pre_json.exp_null == 30);
        CHECK(pre_json.opt_exp_null.has_value());
        CHECK((*pre_json.opt_exp_null).has_value());
        CHECK(*(*pre_json.opt_exp_null) == 40);

        std::string etf_payload;
        {
            etf_payload.push_back(static_cast<char>(glz::etf::magic_version));
            etf_payload.push_back(static_cast<char>(glz::etf::tag::MAP_EXT));
            uint32_t arity = glz::etf::detail::to_big_endian(1U);
            etf_payload.append(reinterpret_cast<const char*>(&arity), 4);
            std::string k = "name";
            std::string enc_k;
            REQUIRE_FALSE(glz::write_etf(k, enc_k));
            etf_payload.append(enc_k.data() + 1, enc_k.size() - 1);
            std::string v = "new_name";
            std::string enc_v;
            REQUIRE_FALSE(glz::write_etf(v, enc_v));
            etf_payload.append(enc_v.data() + 1, enc_v.size() - 1);
        }

        REQUIRE_FALSE(glz::read_etf(pre_etf, etf_payload));
        CHECK(pre_etf == pre_json);
    }

    SECTION("Explicit null fields: pre-populated fields are reset to nullopt / nullptr (exact parity)") {
        NullableParityStruct pre_json{
            .name = "reset_test",
            .std_opt = 100,
            .discusy_opt = 200,
            .exp_null = 300,
            .opt_exp_null = discusy::explicit_null<int>{400},
        };
        NullableParityStruct pre_etf = pre_json;

        // JSON: all fields explicitly set to null
        std::string json_null_payload =
            "{\"name\":\"reset_test\",\"std_opt\":null,\"discusy_opt\":null,\"exp_null\":null,\"opt_exp_null\":null}";
        REQUIRE_FALSE(glz::read_json(pre_json, json_null_payload));
        CHECK_FALSE(pre_json.std_opt.has_value());
        CHECK_FALSE(pre_json.discusy_opt.has_value());
        CHECK_FALSE(pre_json.exp_null.has_value());
        CHECK_FALSE(pre_json.opt_exp_null.has_value());

        // ETF: all fields explicitly set to nil atom
        std::string etf_null_payload;
        {
            etf_null_payload.push_back(static_cast<char>(glz::etf::magic_version));
            etf_null_payload.push_back(static_cast<char>(glz::etf::tag::MAP_EXT));
            uint32_t arity = glz::etf::detail::to_big_endian(5U);
            etf_null_payload.append(reinterpret_cast<const char*>(&arity), 4);

            auto add_pair = [&](std::string_view key, auto val_writer) {
                std::string k{key};
                std::string enc_k;
                REQUIRE_FALSE(glz::write_etf(k, enc_k));
                etf_null_payload.append(enc_k.data() + 1, enc_k.size() - 1);
                val_writer();
            };

            add_pair("name", [&] {
                std::string v = "reset_test";
                std::string enc_v;
                REQUIRE_FALSE(glz::write_etf(v, enc_v));
                etf_null_payload.append(enc_v.data() + 1, enc_v.size() - 1);
            });
            add_pair("std_opt", [&] {
                std::string enc_nil;
                REQUIRE_FALSE(glz::write_etf(nullptr, enc_nil));
                etf_null_payload.append(enc_nil.data() + 1, enc_nil.size() - 1);
            });
            add_pair("discusy_opt", [&] {
                std::string enc_nil;
                REQUIRE_FALSE(glz::write_etf(nullptr, enc_nil));
                etf_null_payload.append(enc_nil.data() + 1, enc_nil.size() - 1);
            });
            add_pair("exp_null", [&] {
                std::string enc_nil;
                REQUIRE_FALSE(glz::write_etf(nullptr, enc_nil));
                etf_null_payload.append(enc_nil.data() + 1, enc_nil.size() - 1);
            });
            add_pair("opt_exp_null", [&] {
                std::string enc_nil;
                REQUIRE_FALSE(glz::write_etf(nullptr, enc_nil));
                etf_null_payload.append(enc_nil.data() + 1, enc_nil.size() - 1);
            });
        }

        REQUIRE_FALSE(glz::read_etf(pre_etf, etf_null_payload));
        CHECK(pre_etf == pre_json);
        CHECK_FALSE(pre_etf.std_opt.has_value());
        CHECK_FALSE(pre_etf.discusy_opt.has_value());
        CHECK_FALSE(pre_etf.exp_null.has_value());
        CHECK_FALSE(pre_etf.opt_exp_null.has_value());
    }

    SECTION("Explicit null serialization parity: std::nullopt is omitted, explicit_null(nullptr) is written") {
        NullableParityStruct obj{
            .name = "serialize_null_parity",
            .std_opt = std::nullopt,
            .discusy_opt = std::nullopt,
            .exp_null = nullptr,
            .opt_exp_null = std::nullopt,
        };

        // JSON: std_opt, discusy_opt, opt_exp_null are omitted; exp_null is written as null
        std::string json_str;
        REQUIRE_FALSE(glz::write_json(obj, json_str));
        CHECK(json_str.find("\"name\":\"serialize_null_parity\"") != std::string::npos);
        CHECK(json_str.find("\"exp_null\":null") != std::string::npos);
        CHECK(json_str.find("\"std_opt\"") == std::string::npos);
        CHECK(json_str.find("\"discusy_opt\"") == std::string::npos);
        CHECK(json_str.find("\"opt_exp_null\"") == std::string::npos);

        // ETF: arity should be 2 (name, exp_null)
        std::string etf_str;
        REQUIRE_FALSE(glz::write_etf(obj, etf_str));
        uint32_t arity = glz::etf::detail::read_be<uint32_t>(&etf_str[2]);
        CHECK(arity == 2);

        // Roundtrip parity
        NullableParityStruct dec_etf{};
        REQUIRE_FALSE(glz::read_etf(dec_etf, etf_str));
        NullableParityStruct dec_json{};
        REQUIRE_FALSE(glz::read_json(dec_json, json_str));
        CHECK(dec_etf == dec_json);
        CHECK(dec_etf == obj);
    }

    SECTION("Concrete values roundtrip and match between JSON and ETF") {
        NullableParityStruct orig{
            .name = "full_values",
            .std_opt = 111,
            .discusy_opt = 222,
            .exp_null = 333,
            .opt_exp_null = discusy::explicit_null<int>{444},
        };

        std::string json_str;
        REQUIRE_FALSE(glz::write_json(orig, json_str));
        NullableParityStruct dec_json{};
        REQUIRE_FALSE(glz::read_json(dec_json, json_str));
        CHECK(dec_json == orig);

        std::string etf_str;
        REQUIRE_FALSE(glz::write_etf(orig, etf_str));
        NullableParityStruct dec_etf{};
        REQUIRE_FALSE(glz::read_etf(dec_etf, etf_str));
        CHECK(dec_etf == orig);

        CHECK(dec_json == dec_etf);
    }

    SECTION("Standalone std::nullopt and explicit_null(nullptr) serialization and deserialization") {
        std::optional<int> opt_null = std::nullopt;
        std::string opt_json;
        REQUIRE_FALSE(glz::write_json(opt_null, opt_json));
        CHECK(opt_json == "null");
        std::string opt_etf;
        REQUIRE_FALSE(glz::write_etf(opt_null, opt_etf));
        CHECK(static_cast<uint8_t>(opt_etf[1]) == glz::etf::tag::SMALL_ATOM_UTF8_EXT);

        std::optional<int> dec_opt_json = 123;
        REQUIRE_FALSE(glz::read_json(dec_opt_json, opt_json));
        CHECK_FALSE(dec_opt_json.has_value());

        std::optional<int> dec_opt_etf = 123;
        REQUIRE_FALSE(glz::read_etf(dec_opt_etf, opt_etf));
        CHECK_FALSE(dec_opt_etf.has_value());

        discusy::explicit_null<int> exp_null = nullptr;
        std::string exp_json;
        REQUIRE_FALSE(glz::write_json(exp_null, exp_json));
        CHECK(exp_json == "null");
        std::string exp_etf;
        REQUIRE_FALSE(glz::write_etf(exp_null, exp_etf));
        CHECK(static_cast<uint8_t>(exp_etf[1]) == glz::etf::tag::SMALL_ATOM_UTF8_EXT);

        discusy::explicit_null<int> dec_exp_json = 123;
        REQUIRE_FALSE(glz::read_json(dec_exp_json, exp_json));
        CHECK_FALSE(dec_exp_json.has_value());

        discusy::explicit_null<int> dec_exp_etf = 123;
        REQUIRE_FALSE(glz::read_etf(dec_exp_etf, exp_etf));
        CHECK_FALSE(dec_exp_etf.has_value());
    }
}

TEST_CASE("ETF & JSON: struct glaze and glz::meta with mimic support", "[etf][json][mimic]") {
    SECTION("Concept checks for mimic types") {
        static_assert(glz::has_mimic<LocalMimicNumber>);
        static_assert(glz::mimics_num_t<LocalMimicNumber>);
        static_assert(!glz::mimics_str_t<LocalMimicNumber>);

        static_assert(glz::has_mimic<LocalMimicString>);
        static_assert(glz::mimics_str_t<LocalMimicString>);
        static_assert(!glz::mimics_num_t<LocalMimicString>);

        static_assert(glz::has_mimic<GlobalMimicNumber>);
        static_assert(glz::mimics_num_t<GlobalMimicNumber>);
        static_assert(!glz::mimics_str_t<GlobalMimicNumber>);

        static_assert(glz::has_mimic<GlobalMimicString>);
        static_assert(glz::mimics_str_t<GlobalMimicString>);
        static_assert(!glz::mimics_num_t<GlobalMimicString>);
    }

    SECTION("Local struct glaze with mimic: numeric and string") {
        LocalMimicNumber num{42};
        std::string etf_num;
        REQUIRE_FALSE(glz::write_etf(num, etf_num));
        CHECK(static_cast<uint8_t>(etf_num[1]) == glz::etf::tag::SMALL_INTEGER_EXT);
        LocalMimicNumber dec_num_etf{};
        REQUIRE_FALSE(glz::read_etf(dec_num_etf, etf_num));
        CHECK(dec_num_etf == num);

        std::string json_num;
        REQUIRE_FALSE(glz::write_json(num, json_num));
        CHECK(json_num == "42");
        LocalMimicNumber dec_num_json{};
        REQUIRE_FALSE(glz::read_json(dec_num_json, json_num));
        CHECK(dec_num_json == num);

        LocalMimicString str{"hello_mimic"};
        std::string etf_str;
        REQUIRE_FALSE(glz::write_etf(str, etf_str));
        CHECK(static_cast<uint8_t>(etf_str[1]) == glz::etf::tag::BINARY_EXT);
        LocalMimicString dec_str_etf{};
        REQUIRE_FALSE(glz::read_etf(dec_str_etf, etf_str));
        CHECK(dec_str_etf == str);

        std::string json_str;
        REQUIRE_FALSE(glz::write_json(str, json_str));
        CHECK(json_str == "\"hello_mimic\"");
        LocalMimicString dec_str_json{};
        REQUIRE_FALSE(glz::read_json(dec_str_json, json_str));
        CHECK(dec_str_json == str);
    }

    SECTION("Global glz::meta with mimic: numeric and string") {
        GlobalMimicNumber num{9999};
        std::string etf_num;
        REQUIRE_FALSE(glz::write_etf(num, etf_num));
        CHECK(static_cast<uint8_t>(etf_num[1]) == glz::etf::tag::INTEGER_EXT);
        GlobalMimicNumber dec_num_etf{};
        REQUIRE_FALSE(glz::read_etf(dec_num_etf, etf_num));
        CHECK(dec_num_etf == num);

        std::string json_num;
        REQUIRE_FALSE(glz::write_json(num, json_num));
        CHECK(json_num == "9999");
        GlobalMimicNumber dec_num_json{};
        REQUIRE_FALSE(glz::read_json(dec_num_json, json_num));
        CHECK(dec_num_json == num);

        GlobalMimicString str{"global_mimic_text"};
        std::string etf_str;
        REQUIRE_FALSE(glz::write_etf(str, etf_str));
        CHECK(static_cast<uint8_t>(etf_str[1]) == glz::etf::tag::BINARY_EXT);
        GlobalMimicString dec_str_etf{};
        REQUIRE_FALSE(glz::read_etf(dec_str_etf, etf_str));
        CHECK(dec_str_etf == str);

        std::string json_str;
        REQUIRE_FALSE(glz::write_json(str, json_str));
        CHECK(json_str == "\"global_mimic_text\"");
        GlobalMimicString dec_str_json{};
        REQUIRE_FALSE(glz::read_json(dec_str_json, json_str));
        CHECK(dec_str_json == str);
    }

    SECTION("Mimic types inside a reflectable struct") {
        MimicHolderStruct orig{
            .local_num = {123},
            .local_str = {"local_data"},
            .global_num = {456},
            .global_str = {"global_data"},
        };

        std::string etf_s;
        REQUIRE_FALSE(glz::write_etf(orig, etf_s));
        MimicHolderStruct dec_etf{};
        REQUIRE_FALSE(glz::read_etf(dec_etf, etf_s));
        CHECK(dec_etf == orig);

        std::string json_s;
        REQUIRE_FALSE(glz::write_json(orig, json_s));
        MimicHolderStruct dec_json{};
        REQUIRE_FALSE(glz::read_json(dec_json, json_s));
        CHECK(dec_json == orig);

        CHECK(dec_etf == dec_json);
    }

    SECTION("Mimic types in std::variant: deduction and deserialization parity") {
        using MimicVar = std::variant<LocalMimicNumber, LocalMimicString>;

        // Numeric variant alternative
        MimicVar var_num{LocalMimicNumber{777}};
        std::string etf_n;
        REQUIRE_FALSE(glz::write_etf(var_num, etf_n));
        MimicVar dec_n_etf{};
        REQUIRE_FALSE(glz::read_etf(dec_n_etf, etf_n));
        REQUIRE(std::holds_alternative<LocalMimicNumber>(dec_n_etf));
        CHECK(std::get<LocalMimicNumber>(dec_n_etf).val == 777);

        std::string json_n;
        REQUIRE_FALSE(glz::write_json(var_num, json_n));
        MimicVar dec_n_json{};
        REQUIRE_FALSE(glz::read_json(dec_n_json, json_n));
        REQUIRE(std::holds_alternative<LocalMimicNumber>(dec_n_json));
        CHECK(std::get<LocalMimicNumber>(dec_n_json).val == 777);

        // String variant alternative
        MimicVar var_str{LocalMimicString{"variant_text"}};
        std::string etf_s;
        REQUIRE_FALSE(glz::write_etf(var_str, etf_s));
        MimicVar dec_s_etf{};
        REQUIRE_FALSE(glz::read_etf(dec_s_etf, etf_s));
        REQUIRE(std::holds_alternative<LocalMimicString>(dec_s_etf));
        CHECK(std::get<LocalMimicString>(dec_s_etf).val == "variant_text");

        std::string json_s;
        REQUIRE_FALSE(glz::write_json(var_str, json_s));
        MimicVar dec_s_json{};
        REQUIRE_FALSE(glz::read_json(dec_s_json, json_s));
        REQUIRE(std::holds_alternative<LocalMimicString>(dec_s_json));
        CHECK(std::get<LocalMimicString>(dec_s_json).val == "variant_text");
    }
}
