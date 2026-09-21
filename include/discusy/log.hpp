#pragma once

#include <type_traits>

#ifdef DISCUSY_LOGGING
#include <format>
#include <print>
#include <cstdio>
#endif

namespace discusy::log {

template <typename T>
struct SelfLogger {
    #ifdef DISCUSY_LOGGING
    T& self;
    template <typename... Args>
    void operator()(const std::format_string<Args...>& fmt, Args&&... args) const {
        self.log(fmt, std::forward<Args>(args)...);
    }
    #endif
};

struct Logger {
    #ifdef DISCUSY_LOGGING
    template <typename... Args>
    void operator()(const std::format_string<Args...>& fmt, Args&&... args) const {
        std::println(stderr, fmt, std::forward<Args>(args)...);
    }
    #endif
};

template <typename T>
struct is_self_logger : std::false_type {};

template <typename T>
struct is_self_logger<SelfLogger<T>> : std::true_type {};

template <typename F>
concept IsLogger = std::is_same_v<std::decay_t<F>, Logger> || is_self_logger<std::decay_t<F>>::value;

}