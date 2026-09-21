#pragma once

#include <concepts> // IWYU pragma: keep

#pragma push_macro("DISCUSY_NO_UNIQUE_ADDRESS")
#undef DISCUSY_NO_UNIQUE_ADDRESS
#if defined(__has_cpp_attribute)
#  if defined(_MSC_VER) && __has_cpp_attribute(msvc::no_unique_address)
#    define DISCUSY_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#  elif __has_cpp_attribute(no_unique_address)
#    define DISCUSY_NO_UNIQUE_ADDRESS [[no_unique_address]]
#  elif __has_cpp_attribute(msvc::no_unique_address)
#    define DISCUSY_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#  else
#    define DISCUSY_NO_UNIQUE_ADDRESS
#  endif
#else
#  define DISCUSY_NO_UNIQUE_ADDRESS
#endif

#pragma push_macro("DISCUSY_VARIADIC_SETTER")
#undef DISCUSY_VARIADIC_SETTER
#define DISCUSY_VARIADIC_SETTER(NAME, ...) \
    template <typename... Args_> \
        requires (sizeof...(Args_) > 0 && (std::convertible_to<Args_, __VA_ARGS__> && ...)) \
    decltype(auto) NAME(this auto&& self, Args_&&... args_) { \
        return std::forward<decltype(self)>(self).NAME( \
            ::discusy::make_vector<__VA_ARGS__>(std::forward<Args_>(args_)...) \
        ); \
    }

#pragma push_macro("DISCUSY_VARIADIC_SETTER_SET")
#undef DISCUSY_VARIADIC_SETTER_SET
#define DISCUSY_VARIADIC_SETTER_SET(NAME, ...) \
    template <typename... Args_> \
        requires (sizeof...(Args_) > 0 && (std::convertible_to<Args_, __VA_ARGS__> && ...)) \
    decltype(auto) NAME(this auto&& self, Args_&&... args_) { \
        return std::forward<decltype(self)>(self).NAME( \
            ::discusy::make_unordered_set<__VA_ARGS__>(std::forward<Args_>(args_)...) \
        ); \
    }
