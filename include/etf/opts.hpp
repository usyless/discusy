#pragma once

#include <cstdint>
#include <glaze/core/opts.hpp>

namespace glz
{
   namespace etf
   {
      struct etf_opts
      {
         uint32_t format = EETF;
         uint32_t internal{};
         bool null_terminated = false;
         bool error_on_unknown_keys = false;
         bool error_on_missing_keys = false;
         bool skip_null_members = true;
         bool partial_read = false;
         bool quoted_num = false;
         bool string_as_number = false;
         bool raw_string = false;
         bool append_arrays = false;

         [[nodiscard]] constexpr bool operator==(const etf_opts&) const noexcept = default;
      };
   } // namespace etf

   // ETF Options Presets
   inline constexpr etf::etf_opts etf_opts{
      .format = EETF,
      .null_terminated = false,
      .error_on_unknown_keys = false,
   };

   inline constexpr etf::etf_opts etf_opts_partial_read{
      .format = EETF,
      .null_terminated = false,
      .error_on_unknown_keys = false,
      .partial_read = true,
   };

   inline constexpr etf::etf_opts etf_opts_not_null_term{
      .format = EETF,
      .null_terminated = false,
      .error_on_unknown_keys = false,
   };

   inline constexpr etf::etf_opts etf_opts_append_arrays{
      .format = EETF,
      .null_terminated = false,
      .error_on_unknown_keys = false,
      .append_arrays = true,
   };

   inline constexpr etf::etf_opts etf_opts_error_on_missing_keys{
      .format = EETF,
      .null_terminated = false,
      .error_on_unknown_keys = false,
      .error_on_missing_keys = true,
   };
} // namespace glz
