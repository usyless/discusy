#pragma once

#include <cstdint>
#include <glaze/core/opts.hpp>
#include "tags.hpp"

namespace glz
{
   namespace etf
   {
      struct etf_opts
      {
         uint32_t format = ETF;
         uint32_t internal{};
         bool null_terminated = false;
         bool error_on_unknown_keys = false;
         bool error_on_missing_keys = false;
         bool skip_null_members = true;
         bool partial_read = false;
         bool quoted_num = false;
         bool string_as_number = false;
         bool raw_string = false;

         [[nodiscard]] constexpr bool operator==(const etf_opts&) const noexcept = default;
      };
   } // namespace etf

   // ETF Options Presets
   inline constexpr etf::etf_opts etf_opts{
      .format = ETF,
      .null_terminated = false,
      .error_on_unknown_keys = false,
   };

   inline constexpr etf::etf_opts etf_opts_partial_read{
      .format = ETF,
      .null_terminated = false,
      .error_on_unknown_keys = false,
      .partial_read = true,
   };

   inline constexpr etf::etf_opts etf_opts_not_null_term{
      .format = ETF,
      .null_terminated = false,
      .error_on_unknown_keys = false,
   };
} // namespace glz
