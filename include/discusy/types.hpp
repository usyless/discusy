#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <chrono>
#include <optional>
#include <bitset>
#include <bit>
#include <array>
#include <span>
#include <format>
#include <initializer_list>
#include <variant>
#include <flat_map>
#include <utility>
#include <type_traits>
#include <charconv>
#include <vector>
#include <unordered_set>
#include <set>

#include <glaze/glaze.hpp>
#include <glaze/base64/base64.hpp>
#include <usylibpp/strings.hpp>

#include "enum_helpers.hpp" // IWYU pragma: keep
#include "permissions.hpp"
#include "log.hpp" // IWYU pragma: keep
#include "macros.hpp" // IWYU pragma: keep

namespace discusy {

class bot;
class http_api;

}

template <>
struct glz::meta<discusy::bot*> {
    static constexpr auto value = glz::skip{};
};

namespace discusy {

namespace http {
    struct case_insensitive_less {
        using is_transparent = void;
        bool operator()(std::string_view a, std::string_view b) const noexcept {
            return std::ranges::lexicographical_compare(
                a, b,
                [](char c1, char c2) {
                    return std::tolower(static_cast<unsigned char>(c1)) < 
                        std::tolower(static_cast<unsigned char>(c2));
                }
            );
        }
    };

    using headers = std::flat_map<std::string, std::string, case_insensitive_less>;
}

struct upload_file_view {
    std::string_view filename{};
    std::string_view data{};
    std::string_view content_type{"application/octet-stream"};
    std::string_view id{"0"};
};

struct upload_file {
    std::string filename{};
    std::string data{};
    std::string content_type{"application/octet-stream"};
    std::string id{"0"};

    [[nodiscard]] constexpr operator upload_file_view() const noexcept {
        return upload_file_view{
            .filename = filename,
            .data = data,
            .content_type = content_type,
            .id = id,
        };
    }
};

struct upload_files_param {
    std::variant<
        std::span<const discusy::upload_file>,
        std::span<const discusy::upload_file_view>
    > files{};

    constexpr upload_files_param() noexcept = default;

    constexpr upload_files_param(std::span<const discusy::upload_file> s) noexcept : files{s} {}
    constexpr upload_files_param(std::span<const discusy::upload_file_view> s) noexcept : files{s} {}

    template <std::size_t N>
    constexpr upload_files_param(const std::array<discusy::upload_file, N>& arr) noexcept : files{std::span<const discusy::upload_file>{arr}} {}

    template <std::size_t N>
    constexpr upload_files_param(const std::array<discusy::upload_file_view, N>& arr) noexcept : files{std::span<const discusy::upload_file_view>{arr}} {}

    template <typename Alloc>
    upload_files_param(const std::vector<discusy::upload_file, Alloc>& vec) noexcept : files{std::span<const discusy::upload_file>{vec}} {}

    template <typename Alloc>
    upload_files_param(const std::vector<discusy::upload_file_view, Alloc>& vec) noexcept : files{std::span<const discusy::upload_file_view>{vec}} {}

    [[nodiscard]] constexpr bool empty() const noexcept {
        return std::visit([](const auto& s) { return s.empty(); }, files);
    }

    [[nodiscard]] constexpr std::size_t size() const noexcept {
        return std::visit([](const auto& s) { return s.size(); }, files);
    }
};

struct command_path {
    std::string_view group{};
    std::string_view sub{};

    [[nodiscard]] constexpr bool is(const std::string_view s) const noexcept { return sub == s; }
    [[nodiscard]] constexpr bool is(const std::string_view g, const std::string_view s) const noexcept {
        return group == g && sub == s;
    }
    [[nodiscard]] constexpr explicit operator bool() const noexcept { return !sub.empty() || !group.empty(); }
};

struct focused_input {
    std::string_view name{};
    std::string_view value{};

    [[nodiscard]] constexpr explicit operator bool() const noexcept { return !name.empty(); }
};

inline constexpr std::uint64_t DISCORD_EPOCH = 1420070400000;

struct snowflake_str;

struct snowflake {
    std::uint64_t value{};

    constexpr bool operator==(const snowflake& other) const noexcept = default;
    constexpr auto operator<=>(const snowflake& other) const noexcept = default;
    [[nodiscard]] constexpr bool operator!() const noexcept {
        return value == 0;
    }
    [[nodiscard]] constexpr explicit operator bool() const noexcept {
        return value != 0;
    }

    constexpr snowflake() noexcept = default;
    constexpr snowflake(const std::uint64_t val) noexcept : value{val} {}

    [[nodiscard]] std::string str() const {
        std::array<char, 32> buf; // NOLINT(cppcoreguidelines-pro-type-member-init)
        auto [ptr, _] = std::to_chars(buf.data(), buf.data() + buf.size(), value);
        return std::string{buf.data(), ptr};
    }

    [[nodiscard]] constexpr snowflake_str stack_str() const noexcept;
    [[nodiscard]] constexpr snowflake_str to_snowflake_str() const noexcept;

    [[nodiscard]] std::string mention_user() const {
        std::array<char, 32> buf; // NOLINT(cppcoreguidelines-pro-type-member-init)
        buf[0] = '<';
        buf[1] = '@';
        auto [ptr, _] = std::to_chars(buf.data() + 2, buf.data() + buf.size() - 1, value);
        *ptr++ = '>';
        return std::string{buf.data(), ptr};
    }

    [[nodiscard]] std::string mention_channel() const {
        std::array<char, 32> buf; // NOLINT(cppcoreguidelines-pro-type-member-init)
        buf[0] = '<';
        buf[1] = '#';
        auto [ptr, _] = std::to_chars(buf.data() + 2, buf.data() + buf.size() - 1, value);
        *ptr++ = '>';
        return std::string{buf.data(), ptr};
    }

    [[nodiscard]] std::string mention_role() const {
        std::array<char, 32> buf; // NOLINT(cppcoreguidelines-pro-type-member-init)
        buf[0] = '<';
        buf[1] = '@';
        buf[2] = '&';
        auto [ptr, _] = std::to_chars(buf.data() + 3, buf.data() + buf.size() - 1, value);
        *ptr++ = '>';
        return std::string{buf.data(), ptr};
    }

    [[nodiscard]] constexpr std::uint64_t get_timestamp() const noexcept {
        return (value >> 22) + DISCORD_EPOCH;
    }

    [[nodiscard]] constexpr std::chrono::time_point<std::chrono::system_clock> get_chrono_timestamp() const noexcept {
        return std::chrono::time_point<std::chrono::system_clock>{
            std::chrono::milliseconds{get_timestamp()}
        };
    }

    [[nodiscard]] constexpr std::uint8_t get_internal_worker_id() const noexcept {
        return static_cast<std::uint8_t>((value & 0x3E0000) >> 17);
    }

    [[nodiscard]] constexpr std::uint8_t get_internal_process_id() const noexcept {
        return static_cast<std::uint8_t>((value & 0x1F000) >> 12);
    }

    // For every ID that is generated on that process, this number is incremented
    [[nodiscard]] constexpr std::uint16_t get_increment() const noexcept {
        return value & 0xFFF;
    }

    // For when the snowflake is a guild id
    [[nodiscard]] constexpr size_t guild_shard_id(const std::uint64_t total_shards) const noexcept {
        return (value >> 22) % total_shards;
    }

    // Timestamp is time since unix epoch in ms
    [[nodiscard]] constexpr static snowflake from_unix_time(const std::uint64_t time_since_epoch_ms) noexcept {
        return (time_since_epoch_ms - DISCORD_EPOCH) << 22;
    }

    [[nodiscard]] constexpr explicit operator std::uint64_t() const noexcept {
        return value;
    }

    struct glaze {
        using T = snowflake;
        // maybe add mimic?
        static constexpr auto value = glz::quoted_num<&T::value>;
    };
};
 
struct snowflake_str {
    std::array<char, 24> buf{};
    std::uint8_t len{};

    constexpr snowflake_str() noexcept = default;

    constexpr explicit snowflake_str(std::uint64_t val) noexcept {
        if consteval {
            if (val == 0) {
                buf[0] = '0';
                buf[1] = '\0';
                len = 1;
                return;
            }
            std::array<char, 24> temp{};
            std::size_t i = 0;
            while (val > 0) {
                temp[i++] = static_cast<char>('0' + (val % 10));
                val /= 10;
            }
            for (std::size_t j = 0; j < i; ++j) {
                buf[j] = temp[i - 1 - j];
            }
            buf[i] = '\0';
            len = static_cast<std::uint8_t>(i);
        } else {
            auto [ptr, _] = std::to_chars(buf.data(), buf.data() + buf.size() - 1, val);
            *ptr = '\0';
            len = static_cast<std::uint8_t>(ptr - buf.data());
        }
    }

    constexpr explicit snowflake_str(snowflake s) noexcept : snowflake_str(s.value) {}

    [[nodiscard]] constexpr std::string_view view() const noexcept {
        return std::string_view{buf.data(), len};
    }

    [[nodiscard]] constexpr operator std::string_view() const noexcept {
        return view();
    }

    [[nodiscard]] std::string str() const {
        return std::string{buf.data(), len};
    }

    [[nodiscard]] constexpr const char* c_str() const noexcept {
        return buf.data();
    }

    [[nodiscard]] constexpr const char* data() const noexcept {
        return buf.data();
    }

    [[nodiscard]] constexpr std::size_t size() const noexcept {
        return len;
    }

    [[nodiscard]] constexpr std::size_t length() const noexcept {
        return len;
    }

    [[nodiscard]] constexpr bool empty() const noexcept {
        return len == 0;
    }

    constexpr bool operator==(const snowflake_str& other) const noexcept {
        return view() == other.view();
    }
    constexpr auto operator<=>(const snowflake_str& other) const noexcept = default;

    constexpr bool operator==(std::string_view sv) const noexcept {
        return view() == sv;
    }
    constexpr auto operator<=>(std::string_view sv) const noexcept {
        return view() <=> sv;
    }

    struct glaze {
        using T = snowflake_str;
        using mimic = std::string_view;
        static constexpr auto value = &T::view;
    };
};

constexpr snowflake_str snowflake::to_snowflake_str() const noexcept {
    return snowflake_str{value};
}

constexpr snowflake_str snowflake::stack_str() const noexcept {
    return snowflake_str{value};
}

enum class flags_type : std::uint8_t {
    number, string,
};

template <typename flags, flags_type type = flags_type::number>
requires (std::is_scoped_enum_v<flags>)
struct flags_t {
    using T = std::underlying_type_t<permissions::permissions>;
    // dont use this directly in case it changes in the future
    T value{};

    constexpr flags_t() noexcept = default;
    constexpr flags_t(const T val) noexcept : value{val} {}
    constexpr flags_t(flags val) noexcept : value{static_cast<T>(val)} {}

    constexpr bool operator==(const flags_t& other) const noexcept = default;
    constexpr auto operator<=>(const flags_t& other) const noexcept = default;

    [[nodiscard]] constexpr bool has_any_flags(const flags permission) const noexcept {
        return value & +permission;
    }

    [[nodiscard]] constexpr bool has_flags(const flags permission) const noexcept {
        return (value & +permission) == +permission;
    }

    constexpr decltype(auto) add_flags(this auto&& self, const flags permission) noexcept {
        self.value |= +permission;
        return std::forward<decltype(self)>(self);
    }

    constexpr decltype(auto) remove_flags(this auto&& self, const flags permission) noexcept {
        self.value &= ~+permission;
        return std::forward<decltype(self)>(self);
    }

    [[nodiscard]] constexpr bool has_flag(const flags permission) const noexcept {
        return has_flags(permission);
    }

    constexpr decltype(auto) add_flag(this auto&& self, const flags permission) noexcept {
        return std::forward<decltype(self)>(self).add_flags(permission);
    }

    constexpr decltype(auto) remove_flag(this auto&& self, const flags permission) noexcept {
        return std::forward<decltype(self)>(self).remove_flags(permission);
    }

    struct glaze {
        using U = flags_t<flags, type>;

        static constexpr auto& get_value() {
            if constexpr (type == flags_type::number) {
                return glz::string_as_number<&U::value>;
            } else {
                return glz::quoted_num<&U::value>;
            }
        }

        static constexpr auto value = get_value();
    };
};

template <std::size_t size, typename flags, flags_type type = flags_type::string>
requires (std::is_scoped_enum_v<flags>)
struct bitset_flags_t {
    std::bitset<size> value{};

    static constexpr std::size_t to_index(flags f) noexcept {
        auto raw = static_cast<std::underlying_type_t<flags>>(f);
        using URaw = std::make_unsigned_t<decltype(raw)>;
        if (raw != 0 && std::has_single_bit(static_cast<URaw>(raw))) {
            return static_cast<std::size_t>(std::countr_zero(static_cast<URaw>(raw)));
        }
        return static_cast<std::size_t>(raw);
    }

    constexpr bitset_flags_t() noexcept = default;
    constexpr bitset_flags_t(const std::bitset<size>& val) noexcept : value{val} {}
    constexpr bitset_flags_t(unsigned long long val) noexcept : value{val} {}
    constexpr bitset_flags_t(flags val) noexcept { add_flag(val); }

    constexpr bool operator==(const bitset_flags_t& other) const noexcept = default;
    constexpr auto operator<=>(const bitset_flags_t& other) const noexcept = default;

    [[nodiscard]] constexpr explicit operator bool() const noexcept {
        return value.any();
    }

    [[nodiscard]] constexpr explicit operator std::bitset<size>() const noexcept {
        return value;
    }

    [[nodiscard]] constexpr bool has_any_flags(const bitset_flags_t permission) const noexcept {
        return (value & permission.value).any();
    }

    [[nodiscard]] constexpr bool has_flags(const bitset_flags_t permission) const noexcept {
        return (value & permission.value) == permission.value;
    }

    [[nodiscard]] constexpr bool has_flag(const flags permission) const noexcept {
        const auto idx = to_index(permission);
        return idx < size && value.test(idx);
    }

    constexpr bitset_flags_t& add_flags(const bitset_flags_t permission) noexcept {
        value |= permission.value;
        return *this;
    }

    constexpr bitset_flags_t& remove_flags(const bitset_flags_t permission) noexcept {
        value &= ~permission.value;
        return *this;
    }

    constexpr bitset_flags_t& add_flag(const flags permission) noexcept {
        const auto idx = to_index(permission);
        if (idx < size) value.set(idx);
        return *this;
    }

    constexpr bitset_flags_t& remove_flag(const flags permission) noexcept {
        const auto idx = to_index(permission);
        if (idx < size) value.reset(idx);
        return *this;
    }

    constexpr bitset_flags_t operator|(const bitset_flags_t& o) const noexcept { return bitset_flags_t{value | o.value}; }
    constexpr bitset_flags_t operator&(const bitset_flags_t& o) const noexcept { return bitset_flags_t{value & o.value}; }
    constexpr bitset_flags_t operator^(const bitset_flags_t& o) const noexcept { return bitset_flags_t{value ^ o.value}; }
    constexpr bitset_flags_t operator~() const noexcept { return bitset_flags_t{~value}; }
    constexpr bitset_flags_t& operator|=(const bitset_flags_t& o) noexcept { value |= o.value; return *this; }
    constexpr bitset_flags_t& operator&=(const bitset_flags_t& o) noexcept { value &= o.value; return *this; }
    constexpr bitset_flags_t& operator^=(const bitset_flags_t& o) noexcept { value ^= o.value; return *this; }

    constexpr bitset_flags_t operator|(flags f) const noexcept { bitset_flags_t r = *this; r.add_flag(f); return r; }
    constexpr bitset_flags_t operator&(flags f) const noexcept { bitset_flags_t r{}; if (has_flag(f)) r.add_flag(f); return r; }
    constexpr bitset_flags_t& operator|=(flags f) noexcept { return add_flag(f); }

    [[nodiscard]] constexpr unsigned long long to_ullong() const {
        return value.to_ullong();
    }

    [[nodiscard]] std::string to_string() const {
        if constexpr (size <= 64) {
            std::array<char, 32> buf; // NOLINT(cppcoreguidelines-pro-type-member-init)
            auto [ptr, _] = std::to_chars(buf.data(), buf.data() + buf.size(), value.to_ullong());
            return std::string{buf.data(), ptr};
        } else {
            if (value.none()) return "0";
            std::bitset<size> temp = value;
            std::string result;
            while (temp.any()) {
                unsigned long remainder = 0;
                std::bitset<size> quotient{};
                for (int i = static_cast<int>(size) - 1; i >= 0; --i) {
                    remainder = (remainder << 1) | temp[i];
                    if (remainder >= 10) {
                        quotient.set(i);
                        remainder -= 10;
                    }
                }
                result.push_back(static_cast<char>('0' + remainder));
                temp = quotient;
            }
            std::ranges::reverse(result);
            return result;
        }
    }

    void from_string(std::string_view sv) noexcept {
        value.reset();
        if (sv.empty()) return;
        if constexpr (size <= 64) {
            unsigned long long val = 0;
            std::from_chars(sv.data(), sv.data() + sv.size(), val);
            value = val;
        } else {
            for (char c : sv) {
                if (c >= '0' && c <= '9') {
                    std::bitset<size> v8 = value << 3;
                    std::bitset<size> v2 = value << 1;
                    while (v2.any()) {
                        std::bitset<size> carry = (v8 & v2) << 1;
                        v8 ^= v2;
                        v2 = carry;
                    }
                    value = v8;
                    std::bitset<size> d(static_cast<unsigned long long>(c - '0'));
                    while (d.any()) {
                        std::bitset<size> carry = (value & d) << 1;
                        value ^= d;
                        d = carry;
                    }
                }
            }
        }
    }
};

using permissions_t = flags_t<permissions::permissions, flags_type::string>;

// for safety
using integer = std::int64_t;

using timer = std::size_t;
using callback_id = std::size_t;

// im unsure if all the timestamps are this
using timestamp = std::chrono::system_clock::time_point;

template <typename... T>
using opt = std::optional<T...>;

template <typename T = void, typename... Args>
[[nodiscard]] constexpr auto make_array(Args&&... args) {
    if constexpr (std::is_void_v<T>) {
        static_assert(sizeof...(Args) > 0, "make_array with no arguments requires an explicit element type: make_array<T>()");
        using value_type = std::common_type_t<std::decay_t<Args>...>;
        return std::array<value_type, sizeof...(Args)>{ static_cast<value_type>(std::forward<Args>(args))... };
    } else {
        return std::array<T, sizeof...(Args)>{ static_cast<T>(std::forward<Args>(args))... };
    }
}

template <typename T = void, typename... Args>
[[nodiscard]] constexpr auto make_vector(Args&&... args) {
    if constexpr (std::is_void_v<T>) {
        static_assert(sizeof...(Args) > 0, "make_vector with no arguments requires an explicit element type: make_vector<T>()");
        using value_type = std::common_type_t<std::decay_t<Args>...>;
        std::vector<value_type> vec;
        vec.reserve(sizeof...(args));
        (vec.emplace_back(std::forward<Args>(args)), ...);
        return vec;
    } else {
        std::vector<T> vec;
        if constexpr (sizeof...(Args) > 0) {
            vec.reserve(sizeof...(args));
            (vec.emplace_back(std::forward<Args>(args)), ...);
        }
        return vec;
    }
}

template <typename T = void, typename... Args>
[[nodiscard]] auto make_unordered_set(Args&&... args) {
    if constexpr (std::is_void_v<T>) {
        static_assert(sizeof...(Args) > 0, "make_unordered_set with no arguments requires an explicit element type: make_unordered_set<T>()");
        using value_type = std::common_type_t<std::decay_t<Args>...>;
        std::unordered_set<value_type> set;
        set.reserve(sizeof...(args));
        (set.emplace(std::forward<Args>(args)), ...);
        return set;
    } else {
        std::unordered_set<T> set;
        if constexpr (sizeof...(Args) > 0) {
            set.reserve(sizeof...(args));
            (set.emplace(std::forward<Args>(args)), ...);
        }
        return set;
    }
}

template <typename T = void, typename... Args>
[[nodiscard]] auto make_set(Args&&... args) {
    if constexpr (std::is_void_v<T>) {
        static_assert(sizeof...(Args) > 0, "make_set with no arguments requires an explicit element type: make_set<T>()");
        using value_type = std::common_type_t<std::decay_t<Args>...>;
        std::set<value_type> set;
        (set.emplace(std::forward<Args>(args)), ...);
        return set;
    } else {
        std::set<T> set;
        (set.emplace(std::forward<Args>(args)), ...);
        return set;
    }
}

template <typename T>
struct explicit_null {
    std::variant<std::nullptr_t, T> val{nullptr};

    constexpr explicit_null() noexcept = default;
    constexpr explicit_null(std::nullopt_t) noexcept : val{nullptr} {}
    constexpr explicit_null(std::nullptr_t) noexcept : val{nullptr} {}

    template <typename U = T>
        requires (!std::is_same_v<std::remove_cvref_t<U>, explicit_null> &&
                  !std::is_same_v<std::remove_cvref_t<U>, std::in_place_t> &&
                  !std::is_same_v<std::remove_cvref_t<U>, std::nullopt_t> &&
                  !std::is_same_v<std::remove_cvref_t<U>, std::nullptr_t> &&
                  std::is_constructible_v<T, U>)
    constexpr explicit_null(U&& v) noexcept(std::is_nothrow_constructible_v<T, U>)
        : val{std::in_place_type<T>, std::forward<U>(v)} {}

    template <typename... Args>
    constexpr explicit explicit_null(std::in_place_t, Args&&... args)
        noexcept(std::is_nothrow_constructible_v<T, Args...>)
        : val{std::in_place_type<T>, std::forward<Args>(args)...} {}

    template <typename U, typename... Args>
    constexpr explicit explicit_null(std::in_place_t, std::initializer_list<U> il, Args&&... args)
        noexcept(std::is_nothrow_constructible_v<T, std::initializer_list<U>&, Args...>)
        : val{std::in_place_type<T>, il, std::forward<Args>(args)...} {}

    template <typename U = T>
        requires (!std::is_same_v<std::remove_cvref_t<U>, explicit_null> &&
                  std::is_constructible_v<T, U>)
    constexpr explicit_null& operator=(U&& v) noexcept(std::is_nothrow_constructible_v<T, U>) {
        val.template emplace<T>(std::forward<U>(v));
        return *this;
    }
    constexpr explicit_null& operator=(std::nullopt_t) noexcept { val = nullptr; return *this; }
    constexpr explicit_null& operator=(std::nullptr_t) noexcept { val = nullptr; return *this; }

    constexpr bool operator==(const explicit_null&) const = default;
    constexpr auto operator<=>(const explicit_null&) const = default;

    template <typename... Args>
    constexpr T& emplace(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) {
        return val.template emplace<T>(std::forward<Args>(args)...);
    }
    template <typename U, typename... Args>
    constexpr T& emplace(std::initializer_list<U> il, Args&&... args) noexcept(
        std::is_nothrow_constructible_v<T, std::initializer_list<U>&, Args...>) {
        return val.template emplace<T>(il, std::forward<Args>(args)...);
    }
    constexpr void reset() noexcept { val = nullptr; }

    [[nodiscard]] constexpr bool has_value() const noexcept { return std::holds_alternative<T>(val); }
    [[nodiscard]] constexpr explicit operator bool() const noexcept { return has_value(); }

    [[nodiscard]] constexpr auto* get_if(this auto&& self) noexcept { return std::get_if<T>(&self.val); }
    [[nodiscard]] constexpr auto* operator->(this auto&& self) noexcept { return self.get_if(); }
    [[nodiscard]] constexpr decltype(auto) operator*(this auto&& self) { return std::get<T>(std::forward<decltype(self)>(self).val); }
    [[nodiscard]] constexpr decltype(auto) value(this auto&& self) { return *std::forward<decltype(self)>(self); }

    template <typename U>
    [[nodiscard]] constexpr T value_or(this auto&& self, U&& default_value) {
        return self.has_value() ? *std::forward<decltype(self)>(self) : static_cast<T>(std::forward<U>(default_value));
    }

    struct glaze {
        static constexpr auto value = &explicit_null::val;
    };
};

enum class image_data_type : std::uint8_t {
    jpeg, png, gif,
};

struct null_bool {
    std::optional<std::nullptr_t> val = std::nullopt;

    constexpr null_bool() noexcept = default;
    constexpr null_bool(bool b) noexcept : val(b ? std::make_optional(nullptr) : std::nullopt) {}

    constexpr null_bool& operator=(bool b) noexcept {
        val = b ? std::make_optional(nullptr) : std::nullopt;
        return *this;
    }

    [[nodiscard]] constexpr operator bool() const noexcept {
        return val.has_value();
    }

    struct glaze {
        static constexpr auto value = &null_bool::val;
    };
};

}

template <>
struct glz::meta<discusy::image_data_type> {
    using enum discusy::image_data_type;
    static constexpr auto value = glz::enumerate(
        "image/jpeg", jpeg, 
        "image/png", png, 
        "image/gif", gif
    );
};

namespace discusy {

struct image_data {
    std::string data{};

    image_data() noexcept = default;

    explicit image_data(const std::string_view bytes, const image_data_type type) : data{ulp::str::concat_strings("data:", glz::get_enum_name(type), ";base64,", glz::write_base64(bytes))} {}

    struct glaze {
        using T = image_data;
        using mimic = std::string;
        static constexpr auto value = &T::data;
    };
};

enum class image_format : std::uint8_t {
    png, jpeg, webp, gif, lottie,
};

namespace cdn {

inline constexpr std::string_view BASE = "https://cdn.discordapp.com/";
inline constexpr std::string_view MEDIA_BASE = "https://media.discordapp.net/";
inline constexpr std::uint64_t STICKER_PACK_BANNER_APPLICATION_ID = 710982414301790216ULL;

[[nodiscard]] constexpr std::string_view extension(const image_format f) noexcept {
    switch (f) {
        case image_format::png:    return ".png";
        case image_format::jpeg:   return ".jpg";
        case image_format::webp:   return ".webp";
        case image_format::gif:    return ".gif";
        case image_format::lottie: return ".json";
    }
    return ".png";
}

inline constexpr std::uint8_t FMT_PNG    = 1ULL << 0;
inline constexpr std::uint8_t FMT_JPEG   = 1ULL << 1;
inline constexpr std::uint8_t FMT_WEBP   = 1ULL << 2;
inline constexpr std::uint8_t FMT_GIF    = 1ULL << 3;
inline constexpr std::uint8_t FMT_LOTTIE = 1ULL << 4;

inline constexpr std::uint8_t FMT_STATIC = FMT_PNG | FMT_JPEG | FMT_WEBP;
inline constexpr std::uint8_t FMT_ANIMATED = FMT_STATIC | FMT_GIF;

[[nodiscard]] constexpr std::uint8_t format_bit(const image_format f) noexcept {
    return static_cast<std::uint8_t>(1ULL << static_cast<std::uint8_t>(f));
}

[[nodiscard]] constexpr bool valid_size(const std::uint32_t size) noexcept {
    return size >= 16 && size <= 4096 && (size & (size - 1)) == 0;
}

struct options {
    // Defaults to gif when the hash is animated and gif is supported, else png.
    opt<image_format> format{};
    // Any power of two between 16 and 4096. Ignored by assets whose size is fixed.
    opt<std::uint32_t> size{};
    bool animated{false};
};

}

enum class cdn_asset : std::uint8_t {
    guild_icon,
    guild_splash,
    guild_discovery_splash,
    guild_banner,
    user_avatar,
    user_banner,
    guild_member_avatar,
    guild_member_banner,
    avatar_decoration,
    application_icon,
    application_cover,
    team_icon,
    role_icon,
    guild_scheduled_event_cover,
    guild_tag_badge,
};

template <cdn_asset A>
struct cdn_traits;

#pragma push_macro("DISCUSY_CDN_TRAITS")
#undef DISCUSY_CDN_TRAITS
#define DISCUSY_CDN_TRAITS(asset, ids, fmts, anim, sized, ...)          \
    template <> struct cdn_traits<cdn_asset::asset> {                   \
        static constexpr std::size_t id_count = ids;                    \
        static constexpr std::uint8_t formats = fmts;                   \
        static constexpr bool animatable = anim;                        \
        static constexpr bool honours_size = sized;                     \
        static constexpr std::array<std::string_view, ids + 1> segments{__VA_ARGS__}; \
    };

DISCUSY_CDN_TRAITS(guild_icon,                  1, cdn::FMT_ANIMATED, true,  true, "icons/", "/")
DISCUSY_CDN_TRAITS(guild_splash,                1, cdn::FMT_STATIC,   false, true, "splashes/", "/")
DISCUSY_CDN_TRAITS(guild_discovery_splash,      1, cdn::FMT_STATIC,   false, true, "discovery-splashes/", "/")
DISCUSY_CDN_TRAITS(guild_banner,                1, cdn::FMT_ANIMATED, true,  true, "banners/", "/")
DISCUSY_CDN_TRAITS(user_avatar,                 1, cdn::FMT_ANIMATED, true,  true, "avatars/", "/")
DISCUSY_CDN_TRAITS(user_banner,                 1, cdn::FMT_ANIMATED, true,  true, "banners/", "/")
DISCUSY_CDN_TRAITS(guild_member_avatar,         2, cdn::FMT_ANIMATED, true,  true, "guilds/", "/users/", "/avatars/")
DISCUSY_CDN_TRAITS(guild_member_banner,         2, cdn::FMT_ANIMATED, true,  true, "guilds/", "/users/", "/banners/")
DISCUSY_CDN_TRAITS(avatar_decoration,           0, cdn::FMT_PNG,      false, true, "avatar-decoration-presets/")
DISCUSY_CDN_TRAITS(application_icon,            1, cdn::FMT_STATIC,   false, true, "app-icons/", "/")
DISCUSY_CDN_TRAITS(application_cover,           1, cdn::FMT_STATIC,   false, true, "app-icons/", "/")
DISCUSY_CDN_TRAITS(team_icon,                   1, cdn::FMT_STATIC,   false, true, "team-icons/", "/")
DISCUSY_CDN_TRAITS(role_icon,                   1, cdn::FMT_STATIC,   false, true, "role-icons/", "/")
DISCUSY_CDN_TRAITS(guild_scheduled_event_cover, 1, cdn::FMT_STATIC,   false, true, "guild-events/", "/")
DISCUSY_CDN_TRAITS(guild_tag_badge,             1, cdn::FMT_STATIC,   false, true, "guild-tag-badges/", "/")

#undef DISCUSY_CDN_TRAITS
#pragma pop_macro("DISCUSY_CDN_TRAITS")

template <cdn_asset A>
struct media_hash {
    using traits = cdn_traits<A>;

    std::string hash{};

    media_hash() noexcept = default;
    media_hash(std::string h) noexcept : hash{std::move(h)} {}

    bool operator==(const media_hash& other) const noexcept = default;

    [[nodiscard]] bool empty() const noexcept { return hash.empty(); }
    [[nodiscard]] explicit operator bool() const noexcept { return !hash.empty(); }

    [[nodiscard]] bool is_animated() const noexcept {
        return traits::animatable && hash.size() > 2 && hash[0] == 'a' && hash[1] == '_';
    }

    [[nodiscard]] static constexpr bool supports(const image_format f) noexcept {
        return (traits::formats & cdn::format_bit(f)) != 0;
    }

    [[nodiscard]] std::string url(const cdn::options opts = {}) const
        requires (traits::id_count == 0)
    {
        return build(nullptr, opts);
    }

    [[nodiscard]] std::string url(const snowflake id, const cdn::options opts = {}) const
        requires (traits::id_count == 1)
    {
        const std::array<snowflake, 1> ids{id};
        return build(ids.data(), opts);
    }

    [[nodiscard]] std::string url(const snowflake first, const snowflake second, const cdn::options opts = {}) const
        requires (traits::id_count == 2)
    {
        const std::array<snowflake, 2> ids{first, second};
        return build(ids.data(), opts);
    }

    struct glaze {
        using T = media_hash;
        using mimic = std::string;
        static constexpr auto value = &T::hash;
    };

private:
    [[nodiscard]] std::string build(const snowflake* const ids, const cdn::options opts) const {
        if (hash.empty()) return {};

        image_format fmt{image_format::png};
        if (opts.format) {
            fmt = *opts.format;
        } else if (opts.animated && is_animated() && supports(image_format::webp)) {
            fmt = image_format::webp;
        } else if (is_animated() && supports(image_format::gif)) {
            fmt = image_format::gif;
        }
        if (!supports(fmt)) fmt = image_format::png;

        std::string out{cdn::BASE};
        for (std::size_t i = 0; i < traits::id_count; ++i) {
            out += traits::segments[i];
            out += ids[i].to_snowflake_str();
        }
        out += traits::segments[traits::id_count];
        out += hash;
        out += cdn::extension(fmt);

        bool first = true;
        const auto add = [&](const std::string_view key, const std::string_view val) {
            out += first ? '?' : '&';
            first = false;
            out += key;
            out += '=';
            out += val;
        };

        if constexpr (traits::honours_size) {
            if (opts.size && cdn::valid_size(*opts.size)) {
                add("size", std::to_string(*opts.size));
            }
        }
        if (opts.animated && fmt == image_format::webp && is_animated()) {
            add("animated", "true");
        }

        return out;
    }
};

using guild_icon_hash                  = media_hash<cdn_asset::guild_icon>;
using guild_splash_hash                = media_hash<cdn_asset::guild_splash>;
using guild_discovery_splash_hash      = media_hash<cdn_asset::guild_discovery_splash>;
using guild_banner_hash                = media_hash<cdn_asset::guild_banner>;
using user_avatar_hash                 = media_hash<cdn_asset::user_avatar>;
using user_banner_hash                 = media_hash<cdn_asset::user_banner>;
using guild_member_avatar_hash         = media_hash<cdn_asset::guild_member_avatar>;
using guild_member_banner_hash         = media_hash<cdn_asset::guild_member_banner>;
using avatar_decoration_hash           = media_hash<cdn_asset::avatar_decoration>;
using application_icon_hash            = media_hash<cdn_asset::application_icon>;
using application_cover_hash           = media_hash<cdn_asset::application_cover>;
using team_icon_hash                   = media_hash<cdn_asset::team_icon>;
using role_icon_hash                   = media_hash<cdn_asset::role_icon>;
using guild_scheduled_event_cover_hash = media_hash<cdn_asset::guild_scheduled_event_cover>;
using guild_tag_badge_hash             = media_hash<cdn_asset::guild_tag_badge>;

namespace cdn {

namespace detail {
    inline void append_size(std::string& out, const opt<std::uint32_t> size) {
        if (size && valid_size(*size)) {
            out += "?size=";
            out += std::to_string(*size);
        }
    }
}

[[nodiscard]] inline std::string custom_emoji(const snowflake emoji_id, const options opts = {}) {
    std::string out{BASE};
    out += "emojis/";
    out += emoji_id.to_snowflake_str();
    out += extension(opts.format.value_or(image_format::webp));
    detail::append_size(out, opts.size);
    return out;
}

[[nodiscard]] inline std::string sticker(const snowflake sticker_id, const image_format format = image_format::png) {
    std::string out{format == image_format::gif ? MEDIA_BASE : BASE};
    out += "stickers/";
    out += sticker_id.to_snowflake_str();
    out += extension(format);
    return out;
}

[[nodiscard]] inline std::string sticker_pack_banner(const snowflake banner_asset_id, const options opts = {}) {
    std::string out{BASE};
    out += "app-assets/";
    out += snowflake{STICKER_PACK_BANNER_APPLICATION_ID}.to_snowflake_str();
    out += "/store/";
    out += banner_asset_id.to_snowflake_str();
    out += extension(opts.format.value_or(image_format::png));
    detail::append_size(out, opts.size);
    return out;
}

[[nodiscard]] inline std::string application_asset(const snowflake application_id, const snowflake asset_id, const options opts = {}) {
    std::string out{BASE};
    out += "app-assets/";
    out += application_id.to_snowflake_str();
    out += '/';
    out += asset_id.to_snowflake_str();
    out += extension(opts.format.value_or(image_format::png));
    detail::append_size(out, opts.size);
    return out;
}

[[nodiscard]] inline std::string store_page_asset(const snowflake application_id, const snowflake asset_id, const options opts = {}) {
    std::string out{BASE};
    out += "app-assets/";
    out += application_id.to_snowflake_str();
    out += "/store/";
    out += asset_id.to_snowflake_str();
    out += extension(opts.format.value_or(image_format::png));
    detail::append_size(out, opts.size);
    return out;
}

[[nodiscard]] inline std::string achievement_icon(
    const snowflake application_id,
    const snowflake achievement_id,
    const std::string_view icon_hash,
    const options opts = {}
) {
    std::string out{BASE};
    out += "app-assets/";
    out += application_id.to_snowflake_str();
    out += "/achievements/";
    out += achievement_id.to_snowflake_str();
    out += "/icons/";
    out += icon_hash;
    out += extension(opts.format.value_or(image_format::png));
    detail::append_size(out, opts.size);
    return out;
}

[[nodiscard]] inline std::string default_user_avatar(const snowflake user_id) {
    std::string out{BASE};
    out += "embed/avatars/";
    out += std::to_string((user_id.value >> 22) % 6);
    out += ".png";
    return out;
}

[[nodiscard]] inline std::string default_user_avatar_legacy(const std::uint16_t discriminator) {
    std::string out{BASE};
    out += "embed/avatars/";
    out += std::to_string(discriminator % 5);
    out += ".png";
    return out;
}

}

namespace detail {

template <typename T>
struct is_optional : std::false_type {};

template <typename T>
struct is_optional<std::optional<T>> : std::true_type {};

template <typename T>
inline constexpr bool is_optional_v = is_optional<std::remove_cvref_t<T>>::value;

template <typename T>
struct is_explicit_null : std::false_type {};

template <typename T>
struct is_explicit_null<explicit_null<T>> : std::true_type {};

template <typename T>
inline constexpr bool is_explicit_null_v = is_explicit_null<std::remove_cvref_t<T>>::value;

template <typename T>
struct explicit_null_value;

template <typename T>
struct explicit_null_value<explicit_null<T>> {
    using type = T;
};

template <typename T>
using explicit_null_value_t = explicit_null_value<std::remove_cvref_t<T>>::type;

template <typename To, typename From>
constexpr To convert_to(From&& f) {
    if constexpr (std::is_constructible_v<To, From>) {
        return To(std::forward<From>(f));
    } else {
        return static_cast<To>(std::forward<From>(f));
    }
}

template <typename Dest, typename Src>
constexpr void smart_assign(Dest& dest, Src&& src) {
    using RawDest = std::remove_cvref_t<Dest>;
    using RawSrc = std::remove_cvref_t<Src>;

    if constexpr (is_optional_v<RawSrc>) {
        if (src.has_value()) {
            if constexpr (is_optional_v<RawDest>) {
                if (!dest.has_value()) dest.emplace();
                smart_assign(*dest, *std::forward<Src>(src));
            } else {
                smart_assign(dest, *std::forward<Src>(src));
            }
        }
    }
    else if constexpr (is_optional_v<RawDest>) {
        using DestInner = RawDest::value_type;

        if constexpr (requires { src.empty(); } && !std::is_same_v<RawDest, RawSrc>) {
            if (!src.empty()) {
                if (!dest.has_value()) dest.emplace();
                smart_assign(*dest, std::forward<Src>(src));
            }
        }
        else if constexpr (std::is_same_v<RawSrc, bool> && std::is_same_v<std::remove_cvref_t<DestInner>, bool>) {
            if (src) {
                dest = true;
            }
        }
        else {
            if (!dest.has_value()) dest.emplace();
            smart_assign(*dest, std::forward<Src>(src));
        }
    }
    else if constexpr (is_explicit_null_v<RawDest>) {
        using Inner = explicit_null_value_t<RawDest>;
        if constexpr (is_explicit_null_v<RawSrc>) {
            if (src.has_value()) {
                dest = convert_to<Inner>(*std::forward<Src>(src));
            } else {
                dest = nullptr;
            }
        } else if constexpr (is_optional_v<RawSrc>) {
            if (src.has_value()) {
                dest = convert_to<Inner>(*std::forward<Src>(src));
            }
        } else if constexpr (requires { src.empty(); }) {
            if (!src.empty()) {
                dest = convert_to<Inner>(std::forward<Src>(src));
            }
        } else {
            dest = convert_to<Inner>(std::forward<Src>(src));
        }
    }
    else if constexpr (requires { RawDest(src.begin(), src.end()); } && !std::is_assignable_v<Dest&, Src>) {
        dest = RawDest(src.begin(), src.end());
    }
    else if constexpr (std::is_assignable_v<Dest&, Src>) {
        dest = std::forward<Src>(src);
    }
    else if constexpr (std::is_constructible_v<RawDest, Src>) {
        dest = convert_to<RawDest>(std::forward<Src>(src));
    }
}

}

}

#pragma push_macro("DISCUSY_HAS_DIRECT_FIELD")
#undef DISCUSY_HAS_DIRECT_FIELD
// NOLINTNEXTLINE(bugprone-macro-parentheses)
#define DISCUSY_HAS_DIRECT_FIELD(Type, Member) requires(Type&& t) { t.Member; }

#pragma push_macro("DISCUSY_HAS_DEREF_FIELD")
#undef DISCUSY_HAS_DEREF_FIELD
// NOLINTNEXTLINE(bugprone-macro-parentheses)
#define DISCUSY_HAS_DEREF_FIELD(Type, Member)  requires(Type&& t) { (*t).Member; }

#pragma push_macro("DISCUSY_HAS_CALL_FIELD")
#undef DISCUSY_HAS_CALL_FIELD
// NOLINTNEXTLINE(bugprone-macro-parentheses)
#define DISCUSY_HAS_CALL_FIELD(Type, Member)   requires(Type&& t) { t().Member; }

#pragma push_macro("DISCUSY_HAS_FIELD")
#undef DISCUSY_HAS_FIELD
#define DISCUSY_HAS_FIELD(Type, Member) \
    (DISCUSY_HAS_DIRECT_FIELD(Type, Member) || \
     DISCUSY_HAS_DEREF_FIELD(Type, Member) || \
     DISCUSY_HAS_CALL_FIELD(Type, Member))

#pragma push_macro("DISCUSY_GET_FIELD")
#undef DISCUSY_GET_FIELD
#define DISCUSY_GET_FIELD(Src, Member) \
    [&]() -> decltype(auto) { \
        using _SrcT = decltype(Src); \
        if constexpr (DISCUSY_HAS_DIRECT_FIELD(_SrcT, Member)) { \
            return std::forward_like<_SrcT>((Src).Member); \
        } else if constexpr (DISCUSY_HAS_DEREF_FIELD(_SrcT, Member)) { \
            return std::forward_like<decltype(*(Src))>((*(Src)).Member); \
        } else if constexpr (DISCUSY_HAS_CALL_FIELD(_SrcT, Member)) { \
            return std::forward_like<decltype((Src)())>(((Src)()).Member); \
        } \
    }()

#pragma push_macro("DISCUSY_TRANSFER_FIELD")
#undef DISCUSY_TRANSFER_FIELD
#define DISCUSY_TRANSFER_FIELD(Dest, Src, Member) \
    do { \
        using _SrcType = decltype(Src); \
        if constexpr (DISCUSY_HAS_FIELD(_SrcType, Member)) { \
            ::discusy::detail::smart_assign((Dest).Member, DISCUSY_GET_FIELD(Src, Member)); \
        } \
    } while (0)

#pragma push_macro("DISCUSY_FORWARD_IF_SAME")
#undef DISCUSY_FORWARD_IF_SAME
#define DISCUSY_FORWARD_IF_SAME(Target, Src) \
    if constexpr (std::is_same_v<std::remove_cvref_t<decltype(Src)>, Target>) { \
        return std::forward<decltype(Src)>(Src); \
    } else {

#pragma push_macro("DISCUSY_END_FROM")
#undef DISCUSY_END_FROM
#define DISCUSY_END_FROM }

template <>
struct std::hash<discusy::snowflake> {
    [[nodiscard]] constexpr std::size_t operator()(const discusy::snowflake& s) const noexcept {
        std::uint64_t x = s.value;
        x ^= x >> 33;
        x *= 0xff51afd7ed558ccdULL;
        x ^= x >> 33;
        return static_cast<std::size_t>(x);
    }
};

template <>
struct std::formatter<discusy::snowflake> : std::formatter<decltype(discusy::snowflake::value)> {
    auto format(const discusy::snowflake& id, std::format_context& ctx) const {
        return std::formatter<uint64_t>::format(id.value, ctx);
    }
};

template <>
struct std::hash<discusy::snowflake_str> {
    [[nodiscard]] std::size_t operator()(const discusy::snowflake_str& s) const noexcept {
        return std::hash<std::string_view>{}(s.view());
    }
};

template <>
struct std::formatter<discusy::snowflake_str> : std::formatter<std::string_view> {
    auto format(const discusy::snowflake_str& s, std::format_context& ctx) const {
        return std::formatter<std::string_view>::format(s.view(), ctx);
    }
};

template <std::size_t size, typename flags, discusy::flags_type type>
struct glz::meta<discusy::bitset_flags_t<size, flags, type>> {
    using T = discusy::bitset_flags_t<size, flags, type>;

    static constexpr auto read_str = [](T& obj, std::string_view input) {
        obj.from_string(input);
    };

    static constexpr auto write_str = [](const T& obj) -> std::string {
        return obj.to_string();
    };

    static constexpr auto read_num = [](T& obj, uint64_t input) {
        obj = input;
    };

    static constexpr auto write_num = [](const T& obj) -> uint64_t {
        if constexpr (size <= 64) {
            return obj.to_ullong();
        } else {
            return 0;
        }
    };

    static constexpr auto get_value() {
        if constexpr (type == discusy::flags_type::number) {
            return glz::custom<read_num, write_num>;
        } else {
            return glz::custom<read_str, write_str>;
        }
    }

    static constexpr auto value = get_value();
};

template <std::size_t size, typename flags, discusy::flags_type type>
struct std::hash<discusy::bitset_flags_t<size, flags, type>> {
    [[nodiscard]] std::size_t operator()(const discusy::bitset_flags_t<size, flags, type>& f) const noexcept {
        return std::hash<std::bitset<size>>{}(f.value);
    }
};

template <std::size_t size, typename flags, discusy::flags_type type>
struct std::formatter<discusy::bitset_flags_t<size, flags, type>> : std::formatter<std::string> {
    auto format(const discusy::bitset_flags_t<size, flags, type>& f, std::format_context& ctx) const {
        return std::formatter<std::string>::format(f.to_string(), ctx);
    }
};
