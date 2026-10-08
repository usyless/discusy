#pragma once

#include <etf/etf.hpp>
#include <string>
#include <string_view>
#include <usylibpp/strings.hpp>

#include "gateway_events.hpp"
#include "log.hpp" // IWYU pragma: keep

namespace discusy::etf {

namespace detail {
    constexpr void reset_ctx(glz::context& ctx) noexcept {
        ctx.error = glz::error_code::none;
        ctx.custom_error_message = {};
        ctx.depth = 0;
    }
}

inline constexpr glz::etf::etf_opts glz_opts{
    .format = glz::ETF,
    .null_terminated = false,
    .error_on_unknown_keys = false,
};

inline constexpr glz::etf::etf_opts glz_opts_partial_read{
    .format = glz::ETF,
    .null_terminated = false,
    .error_on_unknown_keys = false,
    .partial_read = true,
};

inline constexpr glz::etf::etf_opts glz_opts_not_null_term{
    .format = glz::ETF,
    .null_terminated = false,
    .error_on_unknown_keys = false,
};

// On true return: failure
template <auto opts = glz_opts, typename T, typename F = log::Logger>
requires ( log::IsLogger<F> )
[[nodiscard]] inline bool parse_etf(T& object, std::string& etf_data, [[maybe_unused]] const F& logger = log::Logger{}) noexcept {
    #ifdef DISCUSY_LOGGING
    if (const auto err = glz::read<opts>(object, etf_data)) {
        logger("ETF parsing failed: {}", glz::format_error(err, etf_data));
        return true;
    }
    return false;
    #else
    return static_cast<bool>(glz::read<opts>(object, etf_data));
    #endif
}

template <auto opts = glz_opts, typename T, typename F = log::Logger>
requires ( log::IsLogger<F> )
[[nodiscard]] inline bool parse_etf(T& object, std::string& etf_data, glz::context& ctx, [[maybe_unused]] const F& logger = log::Logger{}) noexcept {
    detail::reset_ctx(ctx);
    #ifdef DISCUSY_LOGGING
    if (const auto err = glz::read<opts>(object, etf_data, ctx)) {
        logger("ETF parsing failed: {}", glz::format_error(err, etf_data));
        return true;
    }
    return false;
    #else
    return static_cast<bool>(glz::read<opts>(object, etf_data, ctx));
    #endif
}

// On true return: failure
template <auto opts = glz_opts_not_null_term, typename T, typename F = log::Logger>
requires ( log::IsLogger<F> )
[[nodiscard]] inline bool parse_etf_view(T& object, std::string_view etf_data, [[maybe_unused]] const F& logger = log::Logger{}) noexcept {
    #ifdef DISCUSY_LOGGING
    if (const auto err = glz::read<opts>(object, etf_data)) {
        logger("ETF parsing failed: {}", glz::format_error(err, etf_data));
        return true;
    }
    return false;
    #else
    return static_cast<bool>(glz::read<opts>(object, etf_data));
    #endif
}

// On true return: failure
template <auto opts = glz_opts_not_null_term, typename T, typename F = log::Logger>
requires ( log::IsLogger<F> )
[[nodiscard]] inline bool parse_etf_view(T& object, std::string_view etf_data, glz::context& ctx, [[maybe_unused]] const F& logger = log::Logger{}) noexcept {
    detail::reset_ctx(ctx);
    #ifdef DISCUSY_LOGGING
    if (const auto err = glz::read<opts>(object, etf_data, ctx)) {
        logger("ETF parsing failed: {}", glz::format_error(err, etf_data));
        return true;
    }
    return false;
    #else
    return static_cast<bool>(glz::read<opts>(object, etf_data, ctx));
    #endif
}

// On true return: failure
template <auto opts = glz_opts, typename T, typename F = log::Logger>
requires ( log::IsLogger<F> )
[[nodiscard]] inline bool write_etf(const T& object, std::string& etf_data, [[maybe_unused]] const F& logger = log::Logger{}) noexcept {
    #ifdef DISCUSY_LOGGING
    if (const auto err = glz::write<opts>(object, etf_data)) {
        logger("ETF serialising failed: {}", glz::format_error(err));
        return true;
    }
    return false;
    #else
    return static_cast<bool>(glz::write<opts>(object, etf_data));
    #endif
}

// On true return: failure
template <auto opts = glz_opts, typename T, typename F = log::Logger>
requires ( log::IsLogger<F> )
[[nodiscard]] inline bool write_etf(const T& object, std::string& etf_data, glz::context& ctx, [[maybe_unused]] const F& logger = log::Logger{}) noexcept {
    detail::reset_ctx(ctx);
    #ifdef DISCUSY_LOGGING
    if (const auto err = glz::write<opts>(object, etf_data, ctx)) {
        logger("ETF serialising failed: {}", glz::format_error(err));
        return true;
    }
    return false;
    #else
    return static_cast<bool>(glz::write<opts>(object, etf_data, ctx));
    #endif
}

// On true return: failure
template <typename F = log::Logger>
requires ( log::IsLogger<F> )
[[nodiscard]] inline bool parse_payload_base(recieve_event::payload_base& object, std::string_view etf_data, [[maybe_unused]] const F& logger = log::Logger{}) noexcept {
    if (etf_data.size() < 6) [[unlikely]] {
        #ifdef DISCUSY_LOGGING
        logger("ETF parsing failed: buffer too short for gateway payload header");
        #endif
        return true;
    }

    if (static_cast<std::uint8_t>(etf_data[0]) != glz::etf::magic_version) [[unlikely]] {
        #ifdef DISCUSY_LOGGING
        logger("ETF parsing failed: invalid magic version");
        #endif
        return true;
    }

    if (static_cast<std::uint8_t>(etf_data[1]) != glz::etf::tag::MAP_EXT) [[unlikely]] {
        #ifdef DISCUSY_LOGGING
        logger("ETF parsing failed: payload root is not a map");
        #endif
        return true;
    }

    const auto arity = glz::etf::detail::read_be<std::uint32_t>(etf_data.data() + 2);
    const auto* it = etf_data.data() + 6;
    const auto* const end = etf_data.data() + etf_data.size();

    if (arity > static_cast<size_t>(end - it) / 2) [[unlikely]] {
        #ifdef DISCUSY_LOGGING
        logger("ETF parsing failed: map arity exceeds payload buffer bounds");
        #endif
        return true;
    }

    glz::context ctx{};
    bool has_op = false;
    bool has_s = false;
    bool has_t = false;

    for (std::uint32_t i = 0; i < arity; ++i) {
        std::string_view key;
        if (!glz::etf::detail::read_key(ctx, it, end, key)) [[unlikely]] {
            #ifdef DISCUSY_LOGGING
            logger("ETF parsing failed: failed to read map key");
            #endif
            return true;
        }

        if (key == "op") {
            glz::parse<glz::EETF>::template op<glz::no_header_on<glz_opts>()>(object.op, ctx, it, end);
            if (static_cast<bool>(ctx.error)) [[unlikely]] {
                #ifdef DISCUSY_LOGGING
                logger("ETF parsing failed: failed to parse \"op\"");
                #endif
                return true;
            }
            has_op = true;
        }
        else if (key == "s") {
            if (glz::etf::detail::is_nil(it, end)) {
                glz::etf::detail::skip_nil(it, end);
                object.s.reset();
            }
            else {
                glz::parse<glz::EETF>::template op<glz::no_header_on<glz_opts>()>(object.s, ctx, it, end);
                if (static_cast<bool>(ctx.error)) [[unlikely]] {
                    #ifdef DISCUSY_LOGGING
                    logger("ETF parsing failed: failed to parse \"s\"");
                    #endif
                    return true;
                }
            }
            has_s = true;
        }
        else if (key == "t") {
            if (glz::etf::detail::is_nil(it, end)) {
                glz::etf::detail::skip_nil(it, end);
                object.t.reset();
            }
            else {
                glz::parse<glz::EETF>::template op<glz::no_header_on<glz_opts>()>(object.t, ctx, it, end);
                if (static_cast<bool>(ctx.error)) [[unlikely]] {
                    #ifdef DISCUSY_LOGGING
                    logger("ETF parsing failed: failed to parse \"t\"");
                    #endif
                    return true;
                }
            }
            has_t = true;
        }
        else if (key == "d") {
            if (has_op && (object.op != Opcode::Dispatch || (has_s && has_t) || i == arity - 1)) {
                return false;
            }
            glz::skip_value<glz::EETF>::template op<glz_opts>(ctx, it, end);
            if (static_cast<bool>(ctx.error)) [[unlikely]] {
                #ifdef DISCUSY_LOGGING
                logger("ETF parsing failed: failed to skip \"d\"");
                #endif
                return true;
            }
        }
        else {
            glz::skip_value<glz::EETF>::template op<glz_opts>(ctx, it, end);
            if (static_cast<bool>(ctx.error)) [[unlikely]] {
                #ifdef DISCUSY_LOGGING
                logger("ETF parsing failed: failed to skip unknown field");
                #endif
                return true;
            }
        }

        if (has_op && (object.op != Opcode::Dispatch || (has_s && has_t))) {
            return false;
        }
    }

    return !has_op;
}

template <auto opts = glz_opts, typename T, typename... Args>
[[nodiscard]] inline bool parse(T& obj, Args&&... args) noexcept {
    return parse_etf<opts>(obj, std::forward<Args>(args)...);
}

template <auto opts = glz_opts_not_null_term, typename T, typename... Args>
[[nodiscard]] inline bool parse_view(T& obj, Args&&... args) noexcept {
    return parse_etf_view<opts>(obj, std::forward<Args>(args)...);
}

template <auto opts = glz_opts, typename T, typename... Args>
[[nodiscard]] inline bool write(const T& obj, Args&&... args) noexcept {
    return write_etf<opts>(obj, std::forward<Args>(args)...);
}

}
