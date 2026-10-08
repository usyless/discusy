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
   };

   template <class T, size_t MaxLen>
   struct from<EETF, max_length_t<T, MaxLen>>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&& wrapper, is_context auto&& ctx, auto&& it, auto end)
      {
         from<EETF, T>::template op<Opts>(wrapper.val, ctx, it, end);
      }
   };

   template <class T, size_t MaxLen>
   struct to<EETF, max_length_t<T, MaxLen>>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&& wrapper, is_context auto&& ctx, auto&&... args)
      {
         to<EETF, T>::template op<Opts>(wrapper.val, ctx, args...);
      }
   };
} // namespace glz
