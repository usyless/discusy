#pragma once

#include <glaze/glaze.hpp>
#include <string>
#include <usylibpp/strings.hpp>

#include "gateway_events.hpp"
#include "log.hpp" // IWYU pragma: keep
#include "enum_helpers.hpp"

namespace discusy::json {

namespace detail {
    constexpr void reset_ctx(glz::context& ctx) noexcept {
        ctx.error = glz::error_code::none;
        ctx.custom_error_message = {};
        ctx.depth = 0;
    }
}

inline constexpr glz::opts glz_opts{
    .null_terminated = true,
    .error_on_unknown_keys = false,
    .minified = true,
};

inline constexpr glz::opts glz_opts_partial_read{
    .null_terminated = true,
    .error_on_unknown_keys = false,
    .minified = true,
    .partial_read = true,
};

inline constexpr glz::opts glz_opts_partial_read_not_null_term{
    .null_terminated = false,
    .error_on_unknown_keys = false,
    .minified = true,
    .partial_read = true,
};

inline constexpr glz::opts glz_opts_not_null_term{
    .null_terminated = false,
    .error_on_unknown_keys = false,
    .minified = true,
};

inline constexpr glz::opts glz_opts_non_minified{
    .null_terminated = true,
    .error_on_unknown_keys = false,
    .minified = false,
};

inline constexpr glz::opts glz_opts_non_minified_error_missing{
    .null_terminated = true,
    .error_on_unknown_keys = false,
    .minified = false,
    .error_on_missing_keys = true,
};

// On true return: failure
template <auto opts = glz_opts, typename T, typename F = log::Logger>
requires ( log::IsLogger<F> )
[[nodiscard]] inline bool parse_json(T& object, std::string& json, [[maybe_unused]] const F& logger = log::Logger{}) noexcept {
    #ifdef DISCUSY_LOGGING
    if (const auto err = glz::read<opts>(object, json)) {
        logger("Json parsing failed: {}", glz::format_error(err, json));
        return true;
    }
    return false;
    #else
    return glz::read<opts>(object, json);
    #endif
}

template <auto opts = glz_opts, typename T, typename F = log::Logger>
requires ( log::IsLogger<F> )
[[nodiscard]] inline bool parse_json(T& object, std::string& json, glz::context& ctx, [[maybe_unused]] const F& logger = log::Logger{}) noexcept {
    detail::reset_ctx(ctx);
    #ifdef DISCUSY_LOGGING
    if (const auto err = glz::read<opts>(object, json, ctx)) {
        logger("Json parsing failed: {}", glz::format_error(err, json));
        return true;
    }
    return false;
    #else
    return glz::read<opts>(object, json, ctx);
    #endif
}

// On true return: failure
template <auto opts = glz_opts_not_null_term, typename T, typename F = log::Logger>
requires ( log::IsLogger<F> )
[[nodiscard]] inline bool parse_json_view(T& object, std::string_view json, [[maybe_unused]] const F& logger = log::Logger{}) noexcept {
    #ifdef DISCUSY_LOGGING
    if (const auto err = glz::read<opts>(object, json)) {
        logger("Json parsing failed: {}", glz::format_error(err, json));
        return true;
    }
    return false;
    #else
    return glz::read<opts>(object, json);
    #endif
}

// On true return: failure
template <auto opts = glz_opts_not_null_term, typename T, typename F = log::Logger>
requires ( log::IsLogger<F> )
[[nodiscard]] inline bool parse_json_view(T& object, std::string_view json, glz::context& ctx, [[maybe_unused]] const F& logger = log::Logger{}) noexcept {
    detail::reset_ctx(ctx);
    #ifdef DISCUSY_LOGGING
    if (const auto err = glz::read<opts>(object, json, ctx)) {
        logger("Json parsing failed: {}", glz::format_error(err, json));
        return true;
    }
    return false;
    #else
    return glz::read<opts>(object, json, ctx);
    #endif
}

// On true return: failure
template <auto opts = glz_opts, typename T, typename F = log::Logger>
requires ( log::IsLogger<F> )
[[nodiscard]] inline bool parse_json(T& object, glz::basic_istream_buffer<std::ifstream>& json, [[maybe_unused]] const F& logger = log::Logger{}) noexcept {
    #ifdef DISCUSY_LOGGING
    if (const auto err = glz::read<opts>(object, json)) {
        logger("Json parsing failed: {}", glz::format_error(err));
        return true;
    }
    return false;
    #else
    return glz::read<opts>(object, json);
    #endif
}

// On true return: failure
template <auto opts = glz_opts, typename T, typename F = log::Logger>
requires ( log::IsLogger<F> )
[[nodiscard]] inline bool parse_json(T& object, glz::basic_istream_buffer<std::ifstream>& json, glz::context& ctx, [[maybe_unused]] const F& logger = log::Logger{}) noexcept {
    detail::reset_ctx(ctx);
    #ifdef DISCUSY_LOGGING
    if (const auto err = glz::read<opts>(object, json, ctx)) {
        logger("Json parsing failed: {}", glz::format_error(err));
        return true;
    }
    return false;
    #else
    return glz::read<opts>(object, json, ctx);
    #endif
}

// On true return: failure
template <auto opts = glz::opts{}, typename T, typename F = log::Logger>
requires ( log::IsLogger<F> )
[[nodiscard]] inline bool write_json(const T& object, std::string& json, [[maybe_unused]] const F& logger = log::Logger{}) noexcept {
    #ifdef DISCUSY_LOGGING
    if (const auto err = glz::write<opts>(object, json)) {
        logger("Json serialising failed: {}", glz::format_error(err, json));
        return true;
    }
    return false;
    #else
    return glz::write<opts>(object, json);
    #endif
}

// On true return: failure
template <auto opts = glz::opts{}, typename T, typename F = log::Logger>
requires ( log::IsLogger<F> )
[[nodiscard]] inline bool write_json(const T& object, std::string& json, glz::context& ctx, [[maybe_unused]] const F& logger = log::Logger{}) noexcept {
    detail::reset_ctx(ctx);
    #ifdef DISCUSY_LOGGING
    if (const auto err = glz::write<opts>(object, json, ctx)) {
        logger("Json serialising failed: {}", glz::format_error(err, json));
        return true;
    }
    return false;
    #else
    return glz::write<opts>(object, json, ctx);
    #endif
}

// On true return: failure
template <typename F = log::Logger>
requires ( log::IsLogger<F> )
[[nodiscard]] inline bool parse_payload_base(recieve_event::payload_base& object, std::string_view json, [[maybe_unused]] const F& logger = log::Logger{}) noexcept {
    static constexpr auto t = "{\"t\":";
    if (!json.starts_with(t)) {
        #ifdef DISCUSY_LOGGING
        logger("Json parsing failed: does not start with \"t\" key");
        #endif
        return true;
    }
    json = json.substr(ulp::str::strlen(t));
    static constexpr auto null = "null";
    if (json.starts_with(null)) {
        json = json.substr(ulp::str::strlen(null));
    } else {
        const auto next_apostrophe = json.find('"', 1);
        if (next_apostrophe == std::string_view::npos) {
            #ifdef DISCUSY_LOGGING
            logger("Json parsing failed: no ending apostrophe on \"t\"");
            #endif
            return true;
        }
        if (glz::read<glz_opts_not_null_term>(object.t, json.substr(0, next_apostrophe + 1))) {
            object.t.reset();
        } else if (!object.t) {
            #ifdef DISCUSY_LOGGING
            logger("Json parsing failed: t is nullopt when it shouldn't be");
            #endif
            return true;
        }
        json = json.substr(next_apostrophe + 1);
    }
    static constexpr auto s = ",\"s\":";
    if (!json.starts_with(s)) {
        #ifdef DISCUSY_LOGGING
        logger("Json parsing failed: no s key");
        #endif
        return true;
    }
    json = json.substr(ulp::str::strlen(s));
    if (json.starts_with(null)) {
        json = json.substr(ulp::str::strlen(null));
    } else {
        const auto next_comma = json.find(',');
        if (next_comma == std::string_view::npos) {
            #ifdef DISCUSY_LOGGING
            logger("Json parsing failed: no ending comma on \"s\"");
            #endif
            return true;
        }
        object.s = ulp::str::to_number<std::int64_t>(json.substr(0, next_comma));
        if (!object.s) {
            #ifdef DISCUSY_LOGGING
            logger("Json parsing failed: s is nullopt when it shouldn't be");
            #endif
            return true;
        }
        json = json.substr(next_comma);
    }
    static constexpr auto op = ",\"op\":";
    if (!json.starts_with(op)) {
        #ifdef DISCUSY_LOGGING
        logger("Json parsing failed: no op key");
        #endif
        return true;
    }
    json = json.substr(ulp::str::strlen(op));
    const auto next_comma = json.find(',');
    if (next_comma == std::string_view::npos) {
        #ifdef DISCUSY_LOGGING
        logger("Json parsing failed: no ending comma on \"op\"");
        #endif
        return true;
    }
    const auto op_val = ulp::str::to_number<decltype(+object.op)>(json.substr(0, next_comma));
    if (!op_val) {
        #ifdef DISCUSY_LOGGING
        logger("Json parsing failed: failed to parse value for \"op\"");
        #endif
        return true;
    }
    object.op = static_cast<Opcode>(*op_val);
    json = json.substr(next_comma);
    if (!json.starts_with(",\"d\":")) {
        #ifdef DISCUSY_LOGGING
        logger("Json parsing failed: no \"d\" key");
        #endif
        return true;
    }

    return false;
}

}