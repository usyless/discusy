#pragma once

#include <glaze/glaze.hpp>
#include <string>

namespace discusy {

namespace detail {
    constexpr void reset_ctx(glz::context& ctx) noexcept {
        ctx.error = glz::error_code::none;
        ctx.custom_error_message = {};
        ctx.depth = 0;
    }

    struct scratch_storage {
        std::string buffer{};
        glz::context ctx{};
    };

    inline thread_local scratch_storage tls_scratch{}; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
}

using detail::scratch_storage;
using detail::tls_scratch;

}
