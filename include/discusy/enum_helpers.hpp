#pragma once

#include <utility>

namespace discusy {

template <typename T> 
concept ScopedEnum = std::is_scoped_enum_v<T>;

template <ScopedEnum T>
[[nodiscard]] constexpr T operator~(T a) noexcept {
    return static_cast<T>(
        ~std::to_underlying(a)
    );
}

template <ScopedEnum T>
[[nodiscard]] constexpr T operator&(T a, T b) noexcept {
    return static_cast<T>(
        std::to_underlying(a) & std::to_underlying(b)
    );
}

template <ScopedEnum T>
constexpr T& operator&=(T& a, T b) noexcept {
    return (a = a & b);
}

template <ScopedEnum T>
[[nodiscard]] constexpr T operator|(T a, T b) noexcept {
    return static_cast<T>(
        std::to_underlying(a) | std::to_underlying(b)
    );
}

template <ScopedEnum T>
constexpr T& operator|=(T& a, T b) noexcept {
    return (a = a | b);
}

template <ScopedEnum T>
[[nodiscard]] constexpr bool contains_bit(T value, T flag) noexcept {
    return (std::to_underlying(value) & std::to_underlying(flag)) != 0;
}

template <auto F, ScopedEnum T>
[[nodiscard]] constexpr bool contains_bit(T value) noexcept {
    return (std::to_underlying(value) & std::to_underlying(F)) != 0;
}

template <ScopedEnum T>
[[nodiscard]] constexpr auto operator+(T a) noexcept {
    return std::to_underlying(a);
}

}