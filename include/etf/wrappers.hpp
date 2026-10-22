#pragma once

#include <glaze/core/custom.hpp>
#include <glaze/core/opts.hpp>
#include <glaze/core/wrappers.hpp>

#include "read.hpp"
#include "write.hpp"

namespace glz
{
   template <is_opts_wrapper T>
   struct from<EETF, T>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&& value, auto&&... args)
      {
         parse<EETF>::op<opt_true<Opts, T::opts_member>>(value.val, args...);
      }
   };

   template <is_opts_wrapper T>
   struct to<EETF, T>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&&... args)
      {
         serialize<EETF>::op<opt_true<Opts, T::opts_member>>(value.val, ctx, args...);
      }

      template <auto Opts>
      GLZ_ALWAYS_INLINE static void no_header(auto&& value, is_context auto&& ctx, auto&&... args)
      {
         serialize<EETF>::no_header<opt_true<Opts, T::opts_member>>(value.val, ctx, args...);
      }
   };
} // namespace glz
