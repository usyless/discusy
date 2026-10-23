#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <variant>
#include <optional>
#include <memory>
#include <type_traits>
#include <chrono>
#include <vector>
#include <bit>

#include <glaze/core/context.hpp>
#include <glaze/core/opts.hpp>
#include <glaze/core/read.hpp>
#include <glaze/core/reflect.hpp>
#include <glaze/core/chrono.hpp>
#include <glaze/core/buffer_traits.hpp>
#include <glaze/util/compare.hpp>
#include <glaze/util/for_each.hpp>
#include <glaze/util/variant.hpp>
#include <glaze/util/bit_array.hpp>
#include <glaze/json/generic_fwd.hpp>

#include "tags.hpp"
#include "opts.hpp"
#include "skip.hpp"

namespace glz
{
   // Specialize decode_hash_with_size for EETF to delegate to JSON
   template <class T, auto HashInfo, hash_type Type>
   struct decode_hash_with_size<EETF, T, HashInfo, Type> : decode_hash_with_size<JSON, T, HashInfo, Type>
   {};

   // Fast-path unfilled resize for std::vector with trivially copyable elements.
   // On libc++, __resize_default_init(n) skips zero-initialization.
   // Custom vector types with resize_uninitialized(n) or resize_and_overwrite are also supported.
   // On standard MSVC STL and GCC libstdc++, std::vector lacks an uninitialized resize API in C++23;
   // attempting to manipulate private members (_M_finish / _Mylast) breaks iterator debugging and triggers
   // AddressSanitizer poison traps (_ASAN_VECTOR_EXTEND_GUARD), so b.resize(n) is the conforming fallback.
   // Deserialization speed across all compilers is recovered through unrolled wire decoding in decode_etf_elements.
   template <class Item, class Alloc>
      requires(std::is_trivially_copyable_v<Item>)
   GLZ_ALWAYS_INLINE void resize_unfilled(std::vector<Item, Alloc>& b, const size_t n)
   {
      if constexpr (requires { b.__resize_default_init(n); }) {
         b.__resize_default_init(n);
      }
      else if constexpr (requires { b.resize_uninitialized(n); }) {
         b.resize_uninitialized(n);
      }
      else if constexpr (requires { b.resize_and_overwrite(n, [](auto*, size_t written) noexcept { return written; }); }) {
         b.resize_and_overwrite(n, [](auto*, size_t written) noexcept { return written; });
      }
      else {
         b.resize(n);
      }
   }

   namespace etf::detail
   {
      template <size_t Base, size_t Rem, class Lambda>
      GLZ_ALWAYS_INLINE constexpr void switch_chunk_16(Lambda&& lambda, size_t offset)
      {
         switch (offset) {
         case 0: if constexpr (0 < Rem) lambda.template operator()<Base + 0>(); break;
         case 1: if constexpr (1 < Rem) lambda.template operator()<Base + 1>(); break;
         case 2: if constexpr (2 < Rem) lambda.template operator()<Base + 2>(); break;
         case 3: if constexpr (3 < Rem) lambda.template operator()<Base + 3>(); break;
         case 4: if constexpr (4 < Rem) lambda.template operator()<Base + 4>(); break;
         case 5: if constexpr (5 < Rem) lambda.template operator()<Base + 5>(); break;
         case 6: if constexpr (6 < Rem) lambda.template operator()<Base + 6>(); break;
         case 7: if constexpr (7 < Rem) lambda.template operator()<Base + 7>(); break;
         case 8: if constexpr (8 < Rem) lambda.template operator()<Base + 8>(); break;
         case 9: if constexpr (9 < Rem) lambda.template operator()<Base + 9>(); break;
         case 10: if constexpr (10 < Rem) lambda.template operator()<Base + 10>(); break;
         case 11: if constexpr (11 < Rem) lambda.template operator()<Base + 11>(); break;
         case 12: if constexpr (12 < Rem) lambda.template operator()<Base + 12>(); break;
         case 13: if constexpr (13 < Rem) lambda.template operator()<Base + 13>(); break;
         case 14: if constexpr (14 < Rem) lambda.template operator()<Base + 14>(); break;
         case 15: if constexpr (15 < Rem) lambda.template operator()<Base + 15>(); break;
         default: break;
         }
      }

      template <size_t N, class Lambda>
      GLZ_ALWAYS_INLINE constexpr void direct_visit(Lambda&& lambda, size_t index)
      {
         if constexpr (N == 0) {
            (void)lambda;
            (void)index;
         }
         else if constexpr (N == 1) {
            (void)index;
            lambda.template operator()<0>();
         }
         else if constexpr (N <= 16) {
            switch_chunk_16<0, N>(std::forward<Lambda>(lambda), index);
         }
         else if constexpr (N <= 32) {
            if (index < 16) {
               switch_chunk_16<0, 16>(std::forward<Lambda>(lambda), index);
            }
            else {
               switch_chunk_16<16, N - 16>(std::forward<Lambda>(lambda), index - 16);
            }
         }
         else if constexpr (N <= 48) {
            if (index < 16) {
               switch_chunk_16<0, 16>(std::forward<Lambda>(lambda), index);
            }
            else if (index < 32) {
               switch_chunk_16<16, 16>(std::forward<Lambda>(lambda), index - 16);
            }
            else {
               switch_chunk_16<32, N - 32>(std::forward<Lambda>(lambda), index - 32);
            }
         }
         else if constexpr (N <= 64) {
            if (index < 32) {
               if (index < 16) { switch_chunk_16<0, 16>(std::forward<Lambda>(lambda), index); }
               else { switch_chunk_16<16, 16>(std::forward<Lambda>(lambda), index - 16); }
            }
            else {
               if (index < 48) { switch_chunk_16<32, 16>(std::forward<Lambda>(lambda), index - 32); }
               else { switch_chunk_16<48, N - 48>(std::forward<Lambda>(lambda), index - 48); }
            }
         }
         else if constexpr (N <= 96) {
            if (index < 48) {
               if (index < 16) { switch_chunk_16<0, 16>(std::forward<Lambda>(lambda), index); }
               else if (index < 32) { switch_chunk_16<16, 16>(std::forward<Lambda>(lambda), index - 16); }
               else { switch_chunk_16<32, 16>(std::forward<Lambda>(lambda), index - 32); }
            }
            else {
               if (index < 64) { switch_chunk_16<48, 16>(std::forward<Lambda>(lambda), index - 48); }
               else if (index < 80) { switch_chunk_16<64, 16>(std::forward<Lambda>(lambda), index - 64); }
               else { switch_chunk_16<80, N - 80>(std::forward<Lambda>(lambda), index - 80); }
            }
         }
         else {
            glz::visit<N>(std::forward<Lambda>(lambda), index);
         }
      }
   }

   template <>
   struct parse<EETF>
   {
      template <auto Opts, class T, is_context Ctx, class It0, class It1>
      GLZ_ALWAYS_INLINE static void op(T&& value, Ctx&& ctx, It0&& it, It1 end) noexcept
      {
         if constexpr (!check_no_header(Opts)) {
            if (it >= end) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            const auto etf_version = static_cast<uint8_t>(*it++);
            if (etf_version != etf::magic_version) [[unlikely]] {
               ctx.error = error_code::version_mismatch;
               return;
            }
            parse<EETF>::template op<no_header_on<Opts>()>(std::forward<T>(value), std::forward<Ctx>(ctx),
                                                           std::forward<It0>(it), end);
         }
         else {
            if constexpr (const_value_v<T>) {
               if constexpr (check_error_on_const_read(Opts)) {
                  ctx.error = error_code::attempt_const_read;
               }
               else {
                  skip_value<EETF>::op<Opts>(std::forward<Ctx>(ctx), std::forward<It0>(it), end);
               }
            }
            else {
               using V = std::remove_cvref_t<T>;
               from<EETF, V>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx),
                                                std::forward<It0>(it), end);
            }
         }
      }
   };

   // Null
   template <always_null_t T>
   struct from<EETF, T> final
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&&, is_context auto& ctx, auto& it, auto end) noexcept
      {
         if (it >= end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }
         if (static_cast<uint8_t>(*it) == etf::tag::NIL_EXT) {
            ++it;
            return;
         }
         if (etf::detail::read_nil(it, end)) {
            return;
         }
         ctx.error = error_code::syntax_error;
      }
   };

   // Skip type
   template <>
   struct from<EETF, skip> final
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&&, is_context auto&& ctx, auto&&... args) noexcept
      {
         skip_value<EETF>::op<Opts>(ctx, args...);
      }
   };

   // Booleans
   template <boolean_like T>
      requires(!custom_read<T>)
   struct from<EETF, T> final
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto& ctx, auto& it, auto end) noexcept
      {
         bool b = false;
         if (!etf::detail::read_bool(ctx, it, end, b)) [[unlikely]] return;
         value = b;
      }
   };

   // Numbers (integers, floats, bignums, and string-quoted numbers e.g. Snowflakes)
   template <num_t T>
      requires(!custom_read<T>)
   struct from<EETF, T> final
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto& ctx, auto& it, auto end) noexcept
      {
         if (!etf::detail::read_number(ctx, it, end, value)) [[unlikely]] return;
      }
   };

   // Strings
   template <str_t T>
      requires(!custom_read<T>)
   struct from<EETF, T> final
   {
      template <auto Opts>
      static void op(auto& value, is_context auto& ctx, auto& it, auto end) noexcept
      {
         std::string_view sv;
         if (!etf::detail::read_str(ctx, it, end, sv)) [[unlikely]] {
            return;
         }
         if constexpr (check_max_string_length(Opts) > 0) {
            if (sv.size() > check_max_string_length(Opts)) [[unlikely]] {
               ctx.error = error_code::invalid_length;
               return;
            }
         }
         if constexpr (has_runtime_max_string_length<std::decay_t<decltype(ctx)>>) {
            if (ctx.max_string_length > 0) [[unlikely]] {
               if (sv.size() > ctx.max_string_length) [[unlikely]] {
                  ctx.error = error_code::invalid_length;
                  return;
               }
            }
         }
         if constexpr (string_view_t<T>) {
            value = sv;
         }
         else if constexpr (array_char_t<T>) {
            if constexpr (not check_partial_read(Opts)) {
               if (sv.size() > value.size()) [[unlikely]] {
                  ctx.error = error_code::exceeded_static_array_size;
                  return;
               }
            }
            const size_t copy_len = (sv.size() < value.size()) ? sv.size() : value.size();
            if (copy_len > 0) {
               std::memcpy(value.data(), sv.data(), copy_len); // NOLINT(bugprone-suspicious-stringview-data-usage)
            }
            if (copy_len < value.size()) {
               std::memset(value.data() + copy_len, 0, value.size() - copy_len);
            }
         }
         else if constexpr (char_array_t<T>) {
            if constexpr (not check_partial_read(Opts)) {
               if (sv.size() > sizeof(value)) [[unlikely]] {
                  ctx.error = error_code::exceeded_static_array_size;
                  return;
               }
            }
            const size_t copy_len = (sv.size() < sizeof(value)) ? sv.size() : sizeof(value);
            if (copy_len > 0) {
               std::memcpy(value, sv.data(), copy_len); // NOLINT(bugprone-suspicious-stringview-data-usage)
            }
            if (copy_len < sizeof(value)) {
               std::memset(value + copy_len, 0, sizeof(value) - copy_len);
            }
         }
         else {
            value.assign(sv.data(), sv.size());
         }
      }
   };

   // Enums (reflected names or underlying integers)
   template <class T>
      requires(std::is_enum_v<T> && !custom_read<T>)
   struct from<EETF, T> final
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto& value, is_context auto& ctx, auto& it, auto end) noexcept
      {
         if (it >= end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }
         const auto tag = static_cast<uint8_t>(*it);
         if (tag == etf::tag::SMALL_INTEGER_EXT) [[likely]] {
            if (end - it < 2) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            const auto val = static_cast<uint8_t>(*(it + 1));
            using underlying = std::underlying_type_t<std::decay_t<T>>;
            if constexpr (std::is_signed_v<underlying> && sizeof(underlying) == 1) {
               if (val > static_cast<uint64_t>((std::numeric_limits<underlying>::max)())) [[unlikely]] {
                  ctx.error = error_code::parse_number_failure;
                  return;
               }
            }
            value = static_cast<std::decay_t<T>>(val);
            it += 2;
            return;
         }
         if (tag == etf::tag::INTEGER_EXT) [[likely]] {
            if (end - it < 5) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            const int32_t val = etf::detail::read_be<int32_t>(it + 1);
            using underlying = std::underlying_type_t<std::decay_t<T>>;
            if constexpr (std::is_unsigned_v<underlying>) {
               if (val < 0) [[unlikely]] {
                  ctx.error = error_code::parse_number_failure;
                  return;
               }
               if constexpr (sizeof(underlying) < 4) {
                  if (static_cast<uint64_t>(val) > static_cast<uint64_t>((std::numeric_limits<underlying>::max)())) [[unlikely]] {
                     ctx.error = error_code::parse_number_failure;
                     return;
                  }
               }
            }
            else {
               if constexpr (sizeof(underlying) < 4) {
                  if (val < static_cast<int64_t>((std::numeric_limits<underlying>::min)()) ||
                      val > static_cast<int64_t>((std::numeric_limits<underlying>::max)())) [[unlikely]] {
                     ctx.error = error_code::parse_number_failure;
                     return;
                  }
               }
            }
            value = static_cast<std::decay_t<T>>(val);
            it += 5;
            return;
         }
         if (tag == etf::tag::SMALL_BIG_EXT || tag == etf::tag::LARGE_BIG_EXT) {
            using underlying = std::underlying_type_t<std::decay_t<T>>;
            using U = std::conditional_t<std::same_as<underlying, bool>, uint8_t, underlying>;
            U u{};
            if (!etf::detail::read_number(ctx, it, end, u)) return;
            value = static_cast<std::decay_t<T>>(u);
            return;
         }

         std::string_view sv;
         if (etf::detail::read_atom_or_str(ctx, it, end, sv)) {
            if constexpr (glaze_enum_t<T> || (meta_keys<T> && std::is_enum_v<T>)) {
               static constexpr auto N = reflect<T>::size;
               if constexpr (N > 0) {
                  static constexpr auto HashInfo = hash_info<T>;
                  const auto index = decode_hash_with_size<EETF, T, HashInfo, HashInfo.type>::op(
                     sv.data(), sv.data() + sv.size(), sv.size());
                  if (index < N) {
                     bool matched = false;
                     etf::detail::direct_visit<N>([&]<size_t I>() {
                        static constexpr auto TargetKey = get<I>(reflect<T>::keys);
                        if (TargetKey.size() == sv.size() && compare<TargetKey.size()>(TargetKey.data(), sv.data())) {
                           value = static_cast<std::decay_t<T>>(get<I>(reflect<T>::values));
                           matched = true;
                        }
                     }, index);
                     if (matched) return;
                  }
               }
            }

            using underlying = std::underlying_type_t<std::decay_t<T>>;
            underlying u{};
            if (etf::detail::parse_number_from_string(sv, u)) {
               value = static_cast<std::decay_t<T>>(u);
               return;
            }
            ctx.error = error_code::unexpected_enum;
            return;
         }
         ctx.error = error_code::syntax_error;
      }
   };

   // Nullable types (std::optional, std::unique_ptr, std::shared_ptr, nullable_value_t)
   template <class T>
      requires(nullable_like<T> || nullable_value_t<T>)
   struct from<EETF, T> final
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto& value, is_context auto& ctx, auto& it, auto end)
      {
         if (it >= end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         if (etf::detail::read_nil(it, end)) {
            if constexpr (is_specialization_v<T, std::optional>) {
               value = std::nullopt;
            }
            else if constexpr (is_specialization_v<T, std::unique_ptr> || is_specialization_v<T, std::shared_ptr>) {
               value = nullptr;
            }
            else if constexpr (requires { value.reset(); }) {
               value.reset();
            }
            else {
               value = {};
            }
            return;
         }

         const bool has_val = [&] {
            if constexpr (nullable_like<std::decay_t<T>>) {
               return bool(value);
            }
            else {
               return value.has_value();
            }
         }();

         if (!has_val) {
            if constexpr (is_specialization_v<T, std::optional>) {
               value.emplace();
            }
            else if constexpr (requires { value.emplace(); }) {
               value.emplace();
            }
            else if constexpr (is_specialization_v<T, std::unique_ptr>) {
               value = std::make_unique<typename T::element_type>();
            }
            else if constexpr (is_specialization_v<T, std::shared_ptr>) {
               value = std::make_shared<typename T::element_type>();
            }
            else if constexpr (constructible<T>) {
               value = meta_construct_v<T>();
            }
            else {
               ctx.error = error_code::invalid_nullable_read;
               return;
            }
         }
         decltype(auto) val = [&]() -> decltype(auto) {
            if constexpr (nullable_like<std::decay_t<T>>) {
               return *value;
            }
            else {
               return value.value();
            }
         }();
         parse<EETF>::op<Opts>(val, ctx, it, end);
      }
   };

   namespace etf::detail
   {
      template <class T>
      struct is_numeric_like {
         using U = std::decay_t<T>;
         static constexpr bool value =
            (std::integral<U> && !std::same_as<U, bool> && !wide_char_t<U>) || std::floating_point<U>;
      };

      template <class T>
         requires(is_opts_wrapper<std::decay_t<T>>)
      struct is_numeric_like<T> {
         using member_t = std::decay_t<T>::value_type;
         static constexpr bool value = is_numeric_like<member_t>::value;
      };

      template <class T>
         requires(glaze_value_t<std::decay_t<T>> && !custom_read<std::decay_t<T>> && !is_opts_wrapper<std::decay_t<T>>)
      struct is_numeric_like<T> {
         using member_t = std::decay_t<decltype(get_member(std::declval<T&>(), meta_wrapper_v<std::decay_t<T>>))>;
         static constexpr bool value = is_numeric_like<member_t>::value;
      };

      template <class T>
      inline constexpr bool is_numeric_like_v = is_numeric_like<T>::value;

      template <class T>
      GLZ_ALWAYS_INLINE decltype(auto) get_numeric_ref(T& val) noexcept
      {
         using D = std::decay_t<T>;
         if constexpr (is_opts_wrapper<D>) {
            return get_numeric_ref(val.val);
         }
         else if constexpr (glaze_value_t<D> && !custom_read<D>) {
            decltype(auto) m = get_member(val, meta_wrapper_v<D>);
            return get_numeric_ref(m);
         }
         else {
            return val;
         }
      }

      template <auto Opts, class V>
      GLZ_ALWAYS_INLINE void decode_etf_elements(V* data, size_t count, is_context auto& ctx, auto& it, auto end)
      {
         if constexpr (is_numeric_like_v<V>) {
            using RawNum = std::decay_t<decltype(get_numeric_ref(std::declval<V&>()))>;
            size_t i = 0;

            // 4-way unrolled loop
            for (; i + 3 < count; i += 4) {
               // Fast path for 64-bit unsigned integers / snowflakes (SMALL_BIG_EXT with n=8, sign=0)
               if constexpr (sizeof(RawNum) == 8 && std::is_integral_v<RawNum> && std::is_unsigned_v<RawNum>) {
                  if (static_cast<size_t>(end - it) >= 44 &&
                      static_cast<uint8_t>(it[0]) == etf::tag::SMALL_BIG_EXT && static_cast<uint8_t>(it[1]) == 8 && static_cast<uint8_t>(it[2]) == 0 &&
                      static_cast<uint8_t>(it[11]) == etf::tag::SMALL_BIG_EXT && static_cast<uint8_t>(it[12]) == 8 && static_cast<uint8_t>(it[13]) == 0 &&
                      static_cast<uint8_t>(it[22]) == etf::tag::SMALL_BIG_EXT && static_cast<uint8_t>(it[23]) == 8 && static_cast<uint8_t>(it[24]) == 0 &&
                      static_cast<uint8_t>(it[33]) == etf::tag::SMALL_BIG_EXT && static_cast<uint8_t>(it[34]) == 8 && static_cast<uint8_t>(it[35]) == 0) [[likely]]
                  {
                     uint64_t v0; // NOLINT(cppcoreguidelines-init-variables)
                     uint64_t v1; // NOLINT(cppcoreguidelines-init-variables)
                     uint64_t v2; // NOLINT(cppcoreguidelines-init-variables)
                     uint64_t v3; // NOLINT(cppcoreguidelines-init-variables)
                     std::memcpy(&v0, it + 3, 8);
                     std::memcpy(&v1, it + 14, 8);
                     std::memcpy(&v2, it + 25, 8);
                     std::memcpy(&v3, it + 36, 8);
                     if constexpr (std::endian::native == std::endian::big) {
                        v0 = std::byteswap(v0);
                        v1 = std::byteswap(v1);
                        v2 = std::byteswap(v2);
                        v3 = std::byteswap(v3);
                     }
                     get_numeric_ref(data[i]) = static_cast<RawNum>(v0);
                     get_numeric_ref(data[i + 1]) = static_cast<RawNum>(v1);
                     get_numeric_ref(data[i + 2]) = static_cast<RawNum>(v2);
                     get_numeric_ref(data[i + 3]) = static_cast<RawNum>(v3);
                     it += 44;
                     continue;
                  }
               }

               // Fast path for 32-bit / 64-bit integers (INTEGER_EXT)
               if constexpr (sizeof(RawNum) >= 4 && std::is_integral_v<RawNum>) {
                  if (static_cast<size_t>(end - it) >= 20 &&
                      static_cast<uint8_t>(it[0]) == etf::tag::INTEGER_EXT &&
                      static_cast<uint8_t>(it[5]) == etf::tag::INTEGER_EXT &&
                      static_cast<uint8_t>(it[10]) == etf::tag::INTEGER_EXT &&
                      static_cast<uint8_t>(it[15]) == etf::tag::INTEGER_EXT) [[likely]]
                  {
                     const int32_t v0 = etf::detail::read_be<int32_t>(it + 1);
                     const int32_t v1 = etf::detail::read_be<int32_t>(it + 6);
                     const int32_t v2 = etf::detail::read_be<int32_t>(it + 11);
                     const int32_t v3 = etf::detail::read_be<int32_t>(it + 16);
                     if constexpr (std::is_unsigned_v<RawNum>) {
                        if (v0 >= 0 && v1 >= 0 && v2 >= 0 && v3 >= 0) [[likely]] {
                           get_numeric_ref(data[i]) = static_cast<RawNum>(v0);
                           get_numeric_ref(data[i + 1]) = static_cast<RawNum>(v1);
                           get_numeric_ref(data[i + 2]) = static_cast<RawNum>(v2);
                           get_numeric_ref(data[i + 3]) = static_cast<RawNum>(v3);
                           it += 20;
                           continue;
                        }
                     }
                     else {
                        get_numeric_ref(data[i]) = static_cast<RawNum>(v0);
                        get_numeric_ref(data[i + 1]) = static_cast<RawNum>(v1);
                        get_numeric_ref(data[i + 2]) = static_cast<RawNum>(v2);
                        get_numeric_ref(data[i + 3]) = static_cast<RawNum>(v3);
                        it += 20;
                        continue;
                     }
                  }
               }

               // Fast path for 8-bit / 16-bit / 32-bit / 64-bit integers (SMALL_INTEGER_EXT)
               if constexpr (std::is_integral_v<RawNum>) {
                  if (static_cast<size_t>(end - it) >= 8 &&
                      static_cast<uint8_t>(it[0]) == etf::tag::SMALL_INTEGER_EXT &&
                      static_cast<uint8_t>(it[2]) == etf::tag::SMALL_INTEGER_EXT &&
                      static_cast<uint8_t>(it[4]) == etf::tag::SMALL_INTEGER_EXT &&
                      static_cast<uint8_t>(it[6]) == etf::tag::SMALL_INTEGER_EXT) [[likely]]
                  {
                     const auto v0 = static_cast<uint8_t>(it[1]);
                     const auto v1 = static_cast<uint8_t>(it[3]);
                     const auto v2 = static_cast<uint8_t>(it[5]);
                     const auto v3 = static_cast<uint8_t>(it[7]);
                     if constexpr (std::is_signed_v<RawNum> && sizeof(RawNum) == 1) {
                        if (v0 <= 127 && v1 <= 127 && v2 <= 127 && v3 <= 127) [[likely]] {
                           get_numeric_ref(data[i]) = static_cast<RawNum>(v0);
                           get_numeric_ref(data[i + 1]) = static_cast<RawNum>(v1);
                           get_numeric_ref(data[i + 2]) = static_cast<RawNum>(v2);
                           get_numeric_ref(data[i + 3]) = static_cast<RawNum>(v3);
                           it += 8;
                           continue;
                        }
                     }
                     else {
                        get_numeric_ref(data[i]) = static_cast<RawNum>(v0);
                        get_numeric_ref(data[i + 1]) = static_cast<RawNum>(v1);
                        get_numeric_ref(data[i + 2]) = static_cast<RawNum>(v2);
                        get_numeric_ref(data[i + 3]) = static_cast<RawNum>(v3);
                        it += 8;
                        continue;
                     }
                  }
               }

               // Fast path for 64-bit IEEE-754 doubles (NEW_FLOAT_EXT)
               if constexpr (std::floating_point<RawNum> && sizeof(RawNum) == 8) {
                  if (static_cast<size_t>(end - it) >= 36 &&
                      static_cast<uint8_t>(it[0]) == etf::tag::NEW_FLOAT_EXT &&
                      static_cast<uint8_t>(it[9]) == etf::tag::NEW_FLOAT_EXT &&
                      static_cast<uint8_t>(it[18]) == etf::tag::NEW_FLOAT_EXT &&
                      static_cast<uint8_t>(it[27]) == etf::tag::NEW_FLOAT_EXT) [[likely]]
                  {
                     const uint64_t b0 = read_be<uint64_t>(it + 1);
                     const uint64_t b1 = read_be<uint64_t>(it + 10);
                     const uint64_t b2 = read_be<uint64_t>(it + 19);
                     const uint64_t b3 = read_be<uint64_t>(it + 28);
                     double d0; // NOLINT(cppcoreguidelines-init-variables)
                     double d1; // NOLINT(cppcoreguidelines-init-variables)
                     double d2; // NOLINT(cppcoreguidelines-init-variables)
                     double d3; // NOLINT(cppcoreguidelines-init-variables)
                     std::memcpy(&d0, &b0, 8);
                     std::memcpy(&d1, &b1, 8);
                     std::memcpy(&d2, &b2, 8);
                     std::memcpy(&d3, &b3, 8);
                     get_numeric_ref(data[i]) = static_cast<RawNum>(d0);
                     get_numeric_ref(data[i + 1]) = static_cast<RawNum>(d1);
                     get_numeric_ref(data[i + 2]) = static_cast<RawNum>(d2);
                     get_numeric_ref(data[i + 3]) = static_cast<RawNum>(d3);
                     it += 36;
                     continue;
                  }
               }

               // Fallback 4-way scalar parse
               parse<EETF>::op<Opts>(data[i], ctx, it, end);
               if (bool(ctx.error)) [[unlikely]] return;
               parse<EETF>::op<Opts>(data[i + 1], ctx, it, end);
               if (bool(ctx.error)) [[unlikely]] return;
               parse<EETF>::op<Opts>(data[i + 2], ctx, it, end);
               if (bool(ctx.error)) [[unlikely]] return;
               parse<EETF>::op<Opts>(data[i + 3], ctx, it, end);
               if (bool(ctx.error)) [[unlikely]] return;
            }

            // Remainder loop
            for (; i < count; ++i) {
               if constexpr (sizeof(RawNum) == 8 && std::is_integral_v<RawNum> && std::is_unsigned_v<RawNum>) {
                  if (static_cast<size_t>(end - it) >= 11 &&
                      static_cast<uint8_t>(it[0]) == etf::tag::SMALL_BIG_EXT &&
                      static_cast<uint8_t>(it[1]) == 8 &&
                      static_cast<uint8_t>(it[2]) == 0) [[likely]]
                  {
                     uint64_t v; // NOLINT(cppcoreguidelines-init-variables)
                     std::memcpy(&v, it + 3, 8);
                     if constexpr (std::endian::native == std::endian::big) {
                        v = std::byteswap(v);
                     }
                     get_numeric_ref(data[i]) = static_cast<RawNum>(v);
                     it += 11;
                     continue;
                  }
               }
               if constexpr (sizeof(RawNum) >= 4 && std::is_integral_v<RawNum>) {
                  if (static_cast<size_t>(end - it) >= 5 &&
                      static_cast<uint8_t>(it[0]) == etf::tag::INTEGER_EXT) [[likely]]
                  {
                     const int32_t v = etf::detail::read_be<int32_t>(it + 1);
                     if constexpr (std::is_unsigned_v<RawNum>) {
                        if (v >= 0) [[likely]] {
                           get_numeric_ref(data[i]) = static_cast<RawNum>(v);
                           it += 5;
                           continue;
                        }
                     }
                     else {
                        get_numeric_ref(data[i]) = static_cast<RawNum>(v);
                        it += 5;
                        continue;
                     }
                  }
               }
               if constexpr (std::is_integral_v<RawNum>) {
                  if (static_cast<size_t>(end - it) >= 2 &&
                      static_cast<uint8_t>(it[0]) == etf::tag::SMALL_INTEGER_EXT) [[likely]]
                  {
                     const auto v = static_cast<uint8_t>(it[1]);
                     if constexpr (std::is_signed_v<RawNum> && sizeof(RawNum) == 1) {
                        if (v <= 127) [[likely]] {
                           get_numeric_ref(data[i]) = static_cast<RawNum>(v);
                           it += 2;
                           continue;
                        }
                     }
                     else {
                        get_numeric_ref(data[i]) = static_cast<RawNum>(v);
                        it += 2;
                        continue;
                     }
                  }
               }
               if constexpr (std::floating_point<RawNum> && sizeof(RawNum) == 8) {
                  if (static_cast<size_t>(end - it) >= 9 &&
                      static_cast<uint8_t>(it[0]) == etf::tag::NEW_FLOAT_EXT) [[likely]]
                  {
                     const uint64_t b = read_be<uint64_t>(it + 1);
                     double d; // NOLINT(cppcoreguidelines-init-variables)
                     std::memcpy(&d, &b, 8);
                     get_numeric_ref(data[i]) = static_cast<RawNum>(d);
                     it += 9;
                     continue;
                  }
               }

               parse<EETF>::op<Opts>(data[i], ctx, it, end);
               if (bool(ctx.error)) [[unlikely]] return;
            }
         }
         else {
            // Non-numeric elements: 4-way unrolled loop
            size_t i = 0;
            for (; i + 3 < count; i += 4) {
               parse<EETF>::op<Opts>(data[i], ctx, it, end);
               if (bool(ctx.error)) [[unlikely]] return;
               parse<EETF>::op<Opts>(data[i + 1], ctx, it, end);
               if (bool(ctx.error)) [[unlikely]] return;
               parse<EETF>::op<Opts>(data[i + 2], ctx, it, end);
               if (bool(ctx.error)) [[unlikely]] return;
               parse<EETF>::op<Opts>(data[i + 3], ctx, it, end);
               if (bool(ctx.error)) [[unlikely]] return;
            }
            for (; i < count; ++i) {
               parse<EETF>::op<Opts>(data[i], ctx, it, end);
               if (bool(ctx.error)) [[unlikely]] return;
            }
         }
      }
   }

   // Arrays (std::vector, std::deque, std::span, etc.)
   template <readable_array_t T>
   struct from<EETF, T> final
   {
      template <auto Opts>
      static void op(auto& value, is_context auto& ctx, auto& it, auto end)
      {
         using V = range_value_t<std::remove_cvref_t<T>>;
         constexpr bool set_like = !resizable<T> && !emplace_backable<T> && emplaceable<T>;
         constexpr bool growable = resizable<T> || emplace_backable<T> || set_like;

         if (it >= end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         const auto tag = static_cast<uint8_t>(*it++);
         if (tag == etf::tag::NIL_EXT) {
            if constexpr (growable) {
               if constexpr (not check_append_arrays(Opts)) {
                  value.clear();
               }
            }
            return;
         }

         if (tag == etf::tag::LIST_EXT) {
            if (end - it < 4) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            const uint32_t len = etf::detail::read_be<uint32_t>(it);
            it += 4;

            if (end - it < 1 || static_cast<size_t>(end - it - 1) < len) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }

            if constexpr (check_max_array_size(Opts) > 0) {
               if (len > check_max_array_size(Opts)) [[unlikely]] {
                  ctx.error = error_code::invalid_length;
                  return;
               }
            }
            if constexpr (has_runtime_max_array_size<std::decay_t<decltype(ctx)>>) {
               if (ctx.max_array_size > 0) [[unlikely]] {
                  if (len > ctx.max_array_size) [[unlikely]] {
                     ctx.error = error_code::invalid_length;
                     return;
                  }
               }
            }

            if constexpr (resizable<T>) {
               if constexpr (check_append_arrays(Opts)) {
                  const auto orig_size = value.size();
                  if (exceeds_capacity(value, orig_size + len, ctx)) [[unlikely]] return;
                  if constexpr (sizeof(V) > 64) {
                     if (len > 4096) {
                        value.resize(orig_size + 4096);
                        for (size_t i = 0; i < len; ++i) {
                           if (orig_size + i >= value.size()) {
                              value.resize(std::min<size_t>(orig_size + len, value.size() + 4096));
                           }
                           parse<EETF>::op<Opts>(value[orig_size + i], ctx, it, end);
                           if (bool(ctx.error)) [[unlikely]] return;
                        }
                     }
                     else {
                        if constexpr (std::is_trivially_copyable_v<V>) {
                           resize_unfilled(value, orig_size + len);
                        }
                        else {
                           value.resize(orig_size + len);
                        }
                        if constexpr (requires { value.data(); }) {
                           etf::detail::decode_etf_elements<Opts>(value.data() + orig_size, len, ctx, it, end);
                           if (bool(ctx.error)) [[unlikely]] return;
                        }
                        else {
                           for (size_t i = orig_size; i < orig_size + len; ++i) {
                              parse<EETF>::op<Opts>(value[i], ctx, it, end);
                              if (bool(ctx.error)) [[unlikely]] return;
                           }
                        }
                     }
                  }
                  else {
                     if constexpr (std::is_trivially_copyable_v<V>) {
                        resize_unfilled(value, orig_size + len);
                     }
                     else {
                        value.resize(orig_size + len);
                     }
                     if constexpr (requires { value.data(); }) {
                        etf::detail::decode_etf_elements<Opts>(value.data() + orig_size, len, ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]] return;
                     }
                     else {
                        for (size_t i = orig_size; i < orig_size + len; ++i) {
                           parse<EETF>::op<Opts>(value[i], ctx, it, end);
                           if (bool(ctx.error)) [[unlikely]] return;
                        }
                     }
                  }
               }
               else {
                  if (exceeds_capacity(value, len, ctx)) [[unlikely]] return;
                  if constexpr (sizeof(V) > 64) {
                     if (len > 4096) {
                        value.clear();
                        value.resize(4096);
                        for (size_t i = 0; i < len; ++i) {
                           if (i >= value.size()) {
                              value.resize(std::min<size_t>(len, value.size() + 4096));
                           }
                           parse<EETF>::op<Opts>(value[i], ctx, it, end);
                           if (bool(ctx.error)) [[unlikely]] return;
                        }
                     }
                     else {
                        if constexpr (std::is_trivially_copyable_v<V>) {
                           resize_unfilled(value, len);
                        }
                        else {
                           value.resize(len);
                        }
                        if constexpr (requires { value.data(); }) {
                           etf::detail::decode_etf_elements<Opts>(value.data(), len, ctx, it, end);
                           if (bool(ctx.error)) [[unlikely]] return;
                        }
                        else {
                           for (auto&& elem : value) {
                              parse<EETF>::op<Opts>(elem, ctx, it, end);
                              if (bool(ctx.error)) [[unlikely]] return;
                           }
                        }
                     }
                  }
                  else {
                     if constexpr (std::is_trivially_copyable_v<V>) {
                        resize_unfilled(value, len);
                     }
                     else {
                        value.resize(len);
                     }
                     if constexpr (requires { value.data(); }) {
                        etf::detail::decode_etf_elements<Opts>(value.data(), len, ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]] return;
                     }
                     else {
                        for (auto&& elem : value) {
                           parse<EETF>::op<Opts>(elem, ctx, it, end);
                           if (bool(ctx.error)) [[unlikely]] return;
                        }
                     }
                  }
               }
            }
            else if constexpr (emplace_backable<T>) {
               if constexpr (check_append_arrays(Opts)) {
                  if (exceeds_capacity(value, value.size() + len, ctx)) [[unlikely]] return;
                  if constexpr (has_reserve<std::remove_cvref_t<T>>) {
                     value.reserve(std::min<size_t>(value.size() + len, value.size() + 4096));
                  }
                  for (size_t i = 0; i < len; ++i) {
                     if constexpr (has_reserve<std::remove_cvref_t<T>>) {
                        if (i > 0 && (i % 4096 == 0)) {
                           value.reserve(std::min<size_t>(value.size() + len - i, value.size() + 4096));
                        }
                     }
                     auto& elem = value.emplace_back();
                     parse<EETF>::op<Opts>(elem, ctx, it, end);
                     if (parse_failed(ctx.error)) [[unlikely]] {
                        if constexpr (requires { value.pop_back(); }) {
                           value.pop_back();
                        }
                        return;
                     }
                     if (bool(ctx.error)) [[unlikely]] return;
                  }
               }
               else {
                  if (exceeds_capacity(value, len, ctx)) [[unlikely]] return;
                  value.clear();
                  if constexpr (has_reserve<std::remove_cvref_t<T>>) {
                     value.reserve(std::min<size_t>(len, 4096));
                  }
                  for (size_t i = 0; i < len; ++i) {
                     if constexpr (has_reserve<std::remove_cvref_t<T>>) {
                        if (i > 0 && (i % 4096 == 0)) {
                           value.reserve(std::min<size_t>(len, i + 4096));
                        }
                     }
                     auto& elem = value.emplace_back();
                     parse<EETF>::op<Opts>(elem, ctx, it, end);
                     if (parse_failed(ctx.error)) [[unlikely]] {
                        if constexpr (requires { value.pop_back(); }) {
                           value.pop_back();
                        }
                        return;
                     }
                     if (bool(ctx.error)) [[unlikely]] return;
                  }
               }
            }
            else if constexpr (set_like) {
               if constexpr (check_append_arrays(Opts)) {
                  if (exceeds_capacity(value, value.size() + len, ctx)) [[unlikely]] return;
               }
               else {
                  if (exceeds_capacity(value, len, ctx)) [[unlikely]] return;
                  value.clear();
               }
               if constexpr (has_reserve<std::remove_cvref_t<T>>) {
                  value.reserve(std::min<size_t>(len, 4096));
               }
               for (size_t i = 0; i < len; ++i) {
                  if constexpr (has_reserve<std::remove_cvref_t<T>>) {
                     if (i > 0 && (i % 4096 == 0)) {
                        value.reserve(std::min<size_t>(len, i + 4096));
                     }
                  }
                  V elem{};
                  parse<EETF>::op<Opts>(elem, ctx, it, end);
                  if (parse_failed(ctx.error)) [[unlikely]] return;
                  value.emplace(std::move(elem));
                  if (bool(ctx.error)) [[unlikely]] return;
               }
            }
            else {
               if constexpr (not check_partial_read(Opts)) {
                  if (len > value.size()) [[unlikely]] {
                     ctx.error = error_code::exceeded_static_array_size;
                     return;
                  }
               }
               const size_t parse_len = (len < value.size()) ? len : value.size();
               if constexpr (requires { value.data(); }) {
                  etf::detail::decode_etf_elements<Opts>(value.data(), parse_len, ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]] return;
               }
               else {
                  for (size_t i = 0; i < parse_len; ++i) {
                     parse<EETF>::op<Opts>(value[i], ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]] return;
                  }
               }
               for (size_t i = parse_len; i < len; ++i) {
                  skip_value<EETF>::op<Opts>(ctx, it, end);
                  if (parse_failed(ctx.error)) [[unlikely]] return;
               }
            }

            // Consume tail term (typically NIL_EXT)
            if (it < end && static_cast<uint8_t>(*it) == etf::tag::NIL_EXT) [[likely]] {
               ++it;
            }
            else {
               skip_value<EETF>::op<Opts>(ctx, it, end);
            }
            return;
         }

         if (tag == etf::tag::STRING_EXT) {
            if constexpr (std::is_arithmetic_v<V> || std::is_enum_v<V> || std::is_same_v<V, std::byte> || std::is_constructible_v<V, uint8_t>) {
               if (end - it < 2) [[unlikely]] {
                  ctx.error = error_code::unexpected_end;
                  return;
               }
               const uint16_t len = etf::detail::read_be<uint16_t>(it);
               it += 2;
               if (static_cast<size_t>(end - it) < len) [[unlikely]] {
                  ctx.error = error_code::unexpected_end;
                  return;
               }

               if constexpr (check_max_array_size(Opts) > 0) {
                  if (len > check_max_array_size(Opts)) [[unlikely]] {
                     ctx.error = error_code::invalid_length;
                     return;
                  }
               }
               if constexpr (has_runtime_max_array_size<std::decay_t<decltype(ctx)>>) {
                  if (ctx.max_array_size > 0) [[unlikely]] {
                     if (len > ctx.max_array_size) [[unlikely]] {
                        ctx.error = error_code::invalid_length;
                        return;
                     }
                  }
               }

               const auto orig_size = check_append_arrays(Opts) ? value.size() : 0;
               if constexpr (sizeof(V) == 1 && !std::same_as<std::remove_cvref_t<V>, bool> &&
                             (std::is_integral_v<V> || std::is_enum_v<V> || std::is_same_v<V, std::byte>)) {
                  if constexpr (resizable<T>) {
                     if (exceeds_capacity(value, orig_size + len, ctx)) [[unlikely]] return;
                     resize_unfilled(value, orig_size + len);
                     if constexpr (requires { value.data(); }) {
                        if (len > 0) {
                           std::memcpy(value.data() + orig_size, it, len);
                        }
                     }
                     else {
                        for (size_t i = 0; i < len; ++i) {
                           value[orig_size + i] = static_cast<V>(static_cast<uint8_t>(it[i]));
                        }
                     }
                  }
                  else if constexpr (emplace_backable<T>) {
                     if (exceeds_capacity(value, orig_size + len, ctx)) [[unlikely]] return;
                     if constexpr (!check_append_arrays(Opts)) {
                        value.clear();
                     }
                     if constexpr (has_reserve<std::remove_cvref_t<T>>) {
                        value.reserve(orig_size + len);
                     }
                     for (size_t i = 0; i < len; ++i) {
                        value.emplace_back(static_cast<V>(static_cast<uint8_t>(it[i])));
                     }
                  }
                  else if constexpr (set_like) {
                     if (exceeds_capacity(value, orig_size + len, ctx)) [[unlikely]] return;
                     if constexpr (!check_append_arrays(Opts)) {
                        value.clear();
                     }
                     if constexpr (has_reserve<std::remove_cvref_t<T>>) {
                        value.reserve(orig_size + len);
                     }
                     for (size_t i = 0; i < len; ++i) {
                        value.emplace(static_cast<V>(static_cast<uint8_t>(it[i])));
                     }
                  }
                  else {
                     if constexpr (not check_partial_read(Opts)) {
                        if (len > value.size()) [[unlikely]] {
                           ctx.error = error_code::exceeded_static_array_size;
                           return;
                        }
                     }
                     const size_t copy_len = (len < value.size()) ? len : value.size();
                     if constexpr (requires { value.data(); }) {
                        if (copy_len > 0) {
                           std::memcpy(value.data(), it, copy_len);
                        }
                        if (copy_len < value.size()) {
                           std::memset(value.data() + copy_len, 0, value.size() - copy_len);
                        }
                     }
                     else {
                        for (size_t i = 0; i < copy_len; ++i) {
                           value[i] = static_cast<V>(static_cast<uint8_t>(it[i]));
                        }
                        for (size_t i = copy_len; i < value.size(); ++i) {
                           value[i] = V{};
                        }
                     }
                  }
               }
               else {
                  if constexpr (resizable<T>) {
                     if (exceeds_capacity(value, orig_size + len, ctx)) [[unlikely]] return;
                     if constexpr (std::is_trivially_copyable_v<V>) {
                        resize_unfilled(value, orig_size + len);
                     }
                     else {
                        value.resize(orig_size + len);
                     }
                     for (size_t i = 0; i < len; ++i) {
                        value[orig_size + i] = static_cast<V>(static_cast<uint8_t>(it[i]));
                     }
                  }
                  else if constexpr (emplace_backable<T>) {
                     if (exceeds_capacity(value, orig_size + len, ctx)) [[unlikely]] return;
                     if constexpr (!check_append_arrays(Opts)) {
                        value.clear();
                     }
                     if constexpr (has_reserve<std::remove_cvref_t<T>>) {
                        value.reserve(orig_size + len);
                     }
                     for (size_t i = 0; i < len; ++i) {
                        value.emplace_back(static_cast<V>(static_cast<uint8_t>(it[i])));
                     }
                  }
                  else if constexpr (set_like) {
                     if (exceeds_capacity(value, orig_size + len, ctx)) [[unlikely]] return;
                     if constexpr (!check_append_arrays(Opts)) {
                        value.clear();
                     }
                     if constexpr (has_reserve<std::remove_cvref_t<T>>) {
                        value.reserve(orig_size + len);
                     }
                     for (size_t i = 0; i < len; ++i) {
                        value.emplace(static_cast<V>(static_cast<uint8_t>(it[i])));
                     }
                  }
                  else {
                     if constexpr (not check_partial_read(Opts)) {
                        if (len > value.size()) [[unlikely]] {
                           ctx.error = error_code::exceeded_static_array_size;
                           return;
                        }
                     }
                     const size_t copy_len = (len < value.size()) ? len : value.size();
                     for (size_t i = 0; i < copy_len; ++i) {
                        value[i] = static_cast<V>(static_cast<uint8_t>(it[i]));
                     }
                  }
               }
               it += len;
               return;
            }
            else {
               ctx.error = error_code::syntax_error;
               return;
            }
         }

         if (tag == etf::tag::BINARY_EXT) {
            if constexpr (sizeof(V) == 1 && !std::same_as<std::remove_cvref_t<V>, bool> &&
                          (std::is_integral_v<V> || std::is_enum_v<V> || std::is_same_v<V, std::byte>)) {
               if (end - it < 4) [[unlikely]] {
                  ctx.error = error_code::unexpected_end;
                  return;
               }
               const uint32_t len = etf::detail::read_be<uint32_t>(it);
               it += 4;
               if (static_cast<size_t>(end - it) < len) [[unlikely]] {
                  ctx.error = error_code::unexpected_end;
                  return;
               }

               if constexpr (check_max_array_size(Opts) > 0) {
                  if (len > check_max_array_size(Opts)) [[unlikely]] {
                     ctx.error = error_code::invalid_length;
                     return;
                  }
               }
               if constexpr (has_runtime_max_array_size<std::decay_t<decltype(ctx)>>) {
                  if (ctx.max_array_size > 0) [[unlikely]] {
                     if (len > ctx.max_array_size) [[unlikely]] {
                        ctx.error = error_code::invalid_length;
                        return;
                     }
                  }
               }

               const auto orig_size = check_append_arrays(Opts) ? value.size() : 0;
               if constexpr (resizable<T>) {
                  if (exceeds_capacity(value, orig_size + len, ctx)) [[unlikely]] return;
                  resize_unfilled(value, orig_size + len);
                  if constexpr (requires { value.data(); }) {
                     if (len > 0) {
                        std::memcpy(value.data() + orig_size, it, len);
                     }
                  }
                  else {
                     for (size_t i = 0; i < len; ++i) {
                        value[orig_size + i] = static_cast<V>(static_cast<uint8_t>(it[i]));
                     }
                  }
               }
               else if constexpr (emplace_backable<T>) {
                  if (exceeds_capacity(value, orig_size + len, ctx)) [[unlikely]] return;
                  if constexpr (!check_append_arrays(Opts)) {
                     value.clear();
                  }
                  if constexpr (has_reserve<std::remove_cvref_t<T>>) {
                     value.reserve(orig_size + len);
                  }
                  for (size_t i = 0; i < len; ++i) {
                     value.emplace_back(static_cast<V>(static_cast<uint8_t>(it[i])));
                  }
               }
               else if constexpr (set_like) {
                  if (exceeds_capacity(value, orig_size + len, ctx)) [[unlikely]] return;
                  if constexpr (!check_append_arrays(Opts)) {
                     value.clear();
                  }
                  if constexpr (has_reserve<std::remove_cvref_t<T>>) {
                     value.reserve(orig_size + len);
                  }
                  for (size_t i = 0; i < len; ++i) {
                     value.emplace(static_cast<V>(static_cast<uint8_t>(it[i])));
                  }
               }
               else {
                  if constexpr (not check_partial_read(Opts)) {
                     if (len > value.size()) [[unlikely]] {
                        ctx.error = error_code::exceeded_static_array_size;
                        return;
                     }
                  }
                  const size_t copy_len = (len < value.size()) ? len : value.size();
                  if constexpr (requires { value.data(); }) {
                     if (copy_len > 0) {
                        std::memcpy(value.data(), it, copy_len);
                     }
                     if (copy_len < value.size()) {
                        std::memset(value.data() + copy_len, 0, value.size() - copy_len);
                     }
                  }
                  else {
                     for (size_t i = 0; i < copy_len; ++i) {
                        value[i] = static_cast<V>(static_cast<uint8_t>(it[i]));
                     }
                     for (size_t i = copy_len; i < value.size(); ++i) {
                        value[i] = V{};
                     }
                  }
               }
               it += len;
               return;
            }
            else {
               ctx.error = error_code::syntax_error;
               return;
            }
         }

         if (tag == etf::tag::SMALL_TUPLE_EXT || tag == etf::tag::LARGE_TUPLE_EXT) {
            uint32_t len = 0;
            if (tag == etf::tag::SMALL_TUPLE_EXT) {
               if (it >= end) [[unlikely]] { ctx.error = error_code::unexpected_end; return; }
               len = static_cast<uint8_t>(*it++);
            }
            else {
               if (end - it < 4) [[unlikely]] { ctx.error = error_code::unexpected_end; return; }
               len = etf::detail::read_be<uint32_t>(it);
               it += 4;
            }

            if (len > static_cast<size_t>(end - it)) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }

            if constexpr (check_max_array_size(Opts) > 0) {
               if (len > check_max_array_size(Opts)) [[unlikely]] {
                  ctx.error = error_code::invalid_length;
                  return;
               }
            }
            if constexpr (has_runtime_max_array_size<std::decay_t<decltype(ctx)>>) {
               if (ctx.max_array_size > 0 && len > ctx.max_array_size) [[unlikely]] {
                  ctx.error = error_code::invalid_length;
                  return;
               }
            }

            if constexpr (resizable<T>) {
               if constexpr (check_append_arrays(Opts)) {
                  const auto orig_size = value.size();
                  if (exceeds_capacity(value, orig_size + len, ctx)) [[unlikely]] return;
                  if constexpr (sizeof(V) > 64) {
                     if (len > 4096) {
                        value.resize(orig_size + 4096);
                        for (size_t i = 0; i < len; ++i) {
                           if (orig_size + i >= value.size()) {
                              value.resize(std::min<size_t>(orig_size + len, value.size() + 4096));
                           }
                           parse<EETF>::op<Opts>(value[orig_size + i], ctx, it, end);
                           if (bool(ctx.error)) [[unlikely]] return;
                        }
                     }
                     else {
                        if constexpr (std::is_trivially_copyable_v<V>) {
                           resize_unfilled(value, orig_size + len);
                        }
                        else {
                           value.resize(orig_size + len);
                        }
                        if constexpr (requires { value.data(); }) {
                           etf::detail::decode_etf_elements<Opts>(value.data() + orig_size, len, ctx, it, end);
                           if (bool(ctx.error)) [[unlikely]] return;
                        }
                        else {
                           for (size_t i = orig_size; i < orig_size + len; ++i) {
                              parse<EETF>::op<Opts>(value[i], ctx, it, end);
                              if (bool(ctx.error)) [[unlikely]] return;
                           }
                        }
                     }
                  }
                  else {
                     if constexpr (std::is_trivially_copyable_v<V>) {
                        resize_unfilled(value, orig_size + len);
                     }
                     else {
                        value.resize(orig_size + len);
                     }
                     if constexpr (requires { value.data(); }) {
                        etf::detail::decode_etf_elements<Opts>(value.data() + orig_size, len, ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]] return;
                     }
                     else {
                        for (size_t i = orig_size; i < orig_size + len; ++i) {
                           parse<EETF>::op<Opts>(value[i], ctx, it, end);
                           if (bool(ctx.error)) [[unlikely]] return;
                        }
                     }
                  }
               }
               else {
                  if (exceeds_capacity(value, len, ctx)) [[unlikely]] return;
                  if constexpr (sizeof(V) > 64) {
                     if (len > 4096) {
                        value.clear();
                        value.resize(4096);
                        for (size_t i = 0; i < len; ++i) {
                           if (i >= value.size()) {
                              value.resize(std::min<size_t>(len, value.size() + 4096));
                           }
                           parse<EETF>::op<Opts>(value[i], ctx, it, end);
                           if (bool(ctx.error)) [[unlikely]] return;
                        }
                     }
                     else {
                        if constexpr (std::is_trivially_copyable_v<V>) {
                           resize_unfilled(value, len);
                        }
                        else {
                           value.resize(len);
                        }
                        if constexpr (requires { value.data(); }) {
                           etf::detail::decode_etf_elements<Opts>(value.data(), len, ctx, it, end);
                           if (bool(ctx.error)) [[unlikely]] return;
                        }
                        else {
                           for (auto&& elem : value) {
                              parse<EETF>::op<Opts>(elem, ctx, it, end);
                              if (bool(ctx.error)) [[unlikely]] return;
                           }
                        }
                     }
                  }
                  else {
                     if constexpr (std::is_trivially_copyable_v<V>) {
                        resize_unfilled(value, len);
                     }
                     else {
                        value.resize(len);
                     }
                     if constexpr (requires { value.data(); }) {
                        etf::detail::decode_etf_elements<Opts>(value.data(), len, ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]] return;
                     }
                     else {
                        for (auto&& elem : value) {
                           parse<EETF>::op<Opts>(elem, ctx, it, end);
                           if (bool(ctx.error)) [[unlikely]] return;
                        }
                     }
                  }
               }
            }
            else if constexpr (emplace_backable<T>) {
               if constexpr (check_append_arrays(Opts)) {
                  if (exceeds_capacity(value, value.size() + len, ctx)) [[unlikely]] return;
                  if constexpr (has_reserve<std::remove_cvref_t<T>>) {
                     value.reserve(std::min<size_t>(value.size() + len, value.size() + 4096));
                  }
                  for (size_t i = 0; i < len; ++i) {
                     if constexpr (has_reserve<std::remove_cvref_t<T>>) {
                        if (i > 0 && (i % 4096 == 0)) {
                           value.reserve(std::min<size_t>(value.size() + len - i, value.size() + 4096));
                        }
                     }
                     auto& elem = value.emplace_back();
                     parse<EETF>::op<Opts>(elem, ctx, it, end);
                     if (parse_failed(ctx.error)) [[unlikely]] {
                        if constexpr (requires { value.pop_back(); }) {
                           value.pop_back();
                        }
                        return;
                     }
                     if (bool(ctx.error)) [[unlikely]] return;
                  }
               }
               else {
                  if (exceeds_capacity(value, len, ctx)) [[unlikely]] return;
                  value.clear();
                  if constexpr (has_reserve<std::remove_cvref_t<T>>) {
                     value.reserve(std::min<size_t>(len, 4096));
                  }
                  for (size_t i = 0; i < len; ++i) {
                     if constexpr (has_reserve<std::remove_cvref_t<T>>) {
                        if (i > 0 && (i % 4096 == 0)) {
                           value.reserve(std::min<size_t>(len, i + 4096));
                        }
                     }
                     auto& elem = value.emplace_back();
                     parse<EETF>::op<Opts>(elem, ctx, it, end);
                     if (parse_failed(ctx.error)) [[unlikely]] {
                        if constexpr (requires { value.pop_back(); }) {
                           value.pop_back();
                        }
                        return;
                     }
                     if (bool(ctx.error)) [[unlikely]] return;
                  }
               }
            }
            else if constexpr (set_like) {
               if constexpr (check_append_arrays(Opts)) {
                  if (exceeds_capacity(value, value.size() + len, ctx)) [[unlikely]] return;
               }
               else {
                  if (exceeds_capacity(value, len, ctx)) [[unlikely]] return;
                  value.clear();
               }
               if constexpr (has_reserve<std::remove_cvref_t<T>>) {
                  value.reserve(std::min<size_t>(len, 4096));
               }
               for (size_t i = 0; i < len; ++i) {
                  if constexpr (has_reserve<std::remove_cvref_t<T>>) {
                     if (i > 0 && (i % 4096 == 0)) {
                        value.reserve(std::min<size_t>(len, i + 4096));
                     }
                  }
                  V elem{};
                  parse<EETF>::op<Opts>(elem, ctx, it, end);
                  if (parse_failed(ctx.error)) [[unlikely]] return;
                  value.emplace(std::move(elem));
                  if (bool(ctx.error)) [[unlikely]] return;
               }
            }
            else {
               if constexpr (not check_partial_read(Opts)) {
                  if (len > value.size()) [[unlikely]] {
                     ctx.error = error_code::exceeded_static_array_size;
                     return;
                  }
               }
               const size_t parse_len = (len < value.size()) ? len : value.size();
               if constexpr (requires { value.data(); }) {
                  etf::detail::decode_etf_elements<Opts>(value.data(), parse_len, ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]] return;
               }
               else {
                  for (size_t i = 0; i < parse_len; ++i) {
                     parse<EETF>::op<Opts>(value[i], ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]] return;
                  }
               }
               for (size_t i = parse_len; i < len; ++i) {
                  skip_value<EETF>::op<Opts>(ctx, it, end);
                  if (parse_failed(ctx.error)) [[unlikely]] return;
               }
            }
            return;
         }

         ctx.error = error_code::syntax_error;
      }
   };

   // Tuples
   template <class T>
      requires(tuple_t<T> || is_std_tuple<T>)
   struct from<EETF, T> final
   {
      template <auto Opts>
      static void op(auto& value, is_context auto& ctx, auto& it, auto end)
      {
         if (it >= end) [[unlikely]] { ctx.error = error_code::unexpected_end; return; }
         const auto tag = static_cast<uint8_t>(*it++);
         uint32_t arity = 0;
         if (tag == etf::tag::SMALL_TUPLE_EXT) {
            if (it >= end) [[unlikely]] { ctx.error = error_code::unexpected_end; return; }
            arity = static_cast<uint8_t>(*it++);
         }
         else if (tag == etf::tag::LARGE_TUPLE_EXT || tag == etf::tag::LIST_EXT) {
            if (end - it < 4) [[unlikely]] { ctx.error = error_code::unexpected_end; return; }
            arity = etf::detail::read_be<uint32_t>(it);
            it += 4;
         }
         else {
            ctx.error = error_code::syntax_error;
            return;
         }

         using V = std::decay_t<T>;
         static constexpr auto N = glz::tuple_size_v<V>;

         if constexpr (not check_partial_read(Opts)) {
            if (arity != N) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }
         }
         else {
            if (arity < N) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }
         }

         if constexpr (is_std_tuple<V>) {
            for_each<N>([&]<size_t I>() {
               if (bool(ctx.error)) [[unlikely]] return;
               parse<EETF>::op<Opts>(std::get<I>(value), ctx, it, end);
            });
         }
         else {
            for_each<N>([&]<size_t I>() {
               if (bool(ctx.error)) [[unlikely]] return;
               parse<EETF>::op<Opts>(glz::get<I>(value), ctx, it, end);
            });
         }
         if (bool(ctx.error)) [[unlikely]] return;
         for (uint32_t i = N; i < arity; ++i) {
            skip_value<EETF>::op<Opts>(ctx, it, end);
            if (parse_failed(ctx.error)) [[unlikely]] return;
         }
         if (tag == etf::tag::LIST_EXT) {
            skip_value<EETF>::op<Opts>(ctx, it, end); // tail term
         }
      }
   };

   // Maps
   template <readable_map_t T>
      requires(!custom_read<T>)
   struct from<EETF, T> final
   {
      template <auto Opts>
      static void op(auto& value, is_context auto& ctx, auto& it, auto end)
      {
         if (it >= end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         const auto tag = static_cast<uint8_t>(*it++);
         if (tag != etf::tag::MAP_EXT) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         if (end - it < 4) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }
         const uint32_t arity = etf::detail::read_be<uint32_t>(it);
         it += 4;

         if (arity > static_cast<size_t>(end - it) / 2) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         if constexpr (check_max_array_size(Opts) > 0) {
            if (arity > check_max_array_size(Opts)) [[unlikely]] {
               ctx.error = error_code::invalid_length;
               return;
            }
         }
         if constexpr (has_runtime_max_map_size<std::decay_t<decltype(ctx)>>) {
            if (ctx.max_map_size > 0) [[unlikely]] {
               if (arity > ctx.max_map_size) [[unlikely]] {
                  ctx.error = error_code::invalid_length;
                  return;
               }
            }
         }
         if (exceeds_capacity(value, arity, ctx)) [[unlikely]] return;

         value.clear();
         if constexpr (has_reserve<std::remove_cvref_t<T>>) {
            value.reserve(std::min<size_t>(arity, 4096));
         }
         using Key = std::remove_cvref_t<T>::key_type;
         for (uint32_t i = 0; i < arity; ++i) {
            if constexpr (has_reserve<std::remove_cvref_t<T>>) {
               if (i > 0 && (i % 4096 == 0)) {
                  value.reserve(std::min<size_t>(arity, i + 4096));
               }
            }
            Key key{};
            if constexpr (str_t<Key>) {
               std::string_view sv{};
               if (!etf::detail::read_key(ctx, it, end, sv)) [[unlikely]] return;
               key = Key(sv);
            }
            else {
               parse<EETF>::op<Opts>(key, ctx, it, end);
               if (parse_failed(ctx.error)) [[unlikely]] return;
            }
            parse<EETF>::op<Opts>(value[std::move(key)], ctx, it, end);
            if (bool(ctx.error)) [[unlikely]] return;
         }
      }
   };

   // Glaze objects & reflectable structs with compile-time hash map deserialization
   template <class T>
      requires((glaze_object_t<T> || reflectable<T>) && !custom_read<T>)
   struct from<EETF, T> final
   {
      template <auto Opts, class Value, is_context Ctx, class It0, class It1>
      static void op(Value&& value, Ctx&& ctx, It0&& it, It1 end)
      {
         if (end - it < 5) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }
         const auto tag = static_cast<uint8_t>(*it);
         if (tag != etf::tag::MAP_EXT) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         const uint32_t arity = etf::detail::read_be<uint32_t>(it + 1);
         it += 5;

         if (arity > static_cast<size_t>(end - it) / 2) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         static constexpr auto N = reflect<T>::size;
         [[maybe_unused]] size_t read_count{};
         [[maybe_unused]] glz::bit_array<N> seen_fields{};

         [[maybe_unused]] auto fields = [&] {
            if constexpr (reflectable<T>) {
               return to_tie(value);
            }
            else {
               return nullptr;
            }
         }();

         if constexpr (N == 1) {
            if (arity == 1) [[likely]] {
               static constexpr auto TargetKey = get<0>(reflect<T>::keys);
               static constexpr size_t KeyLen = TargetKey.size();
               static constexpr size_t WireKeyLen = 5 + KeyLen;
               static constexpr auto ExpectedWireKey = []() constexpr {
                  std::array<char, WireKeyLen> arr{};
                  arr[0] = static_cast<char>(etf::tag::BINARY_EXT);
                  arr[1] = static_cast<char>((KeyLen >> 24) & 0xFF);
                  arr[2] = static_cast<char>((KeyLen >> 16) & 0xFF);
                  arr[3] = static_cast<char>((KeyLen >> 8) & 0xFF);
                  arr[4] = static_cast<char>(KeyLen & 0xFF);
                  for (size_t i = 0; i < KeyLen; ++i) {
                     arr[5 + i] = TargetKey[i];
                  }
                  return arr;
               }();

               if (end - it >= static_cast<std::ptrdiff_t>(WireKeyLen) &&
                   glz::compare<WireKeyLen>(reinterpret_cast<const char*>(it), ExpectedWireKey.data())) [[likely]] {
                  it += WireKeyLen;
                  if constexpr (skipped_by_meta<T, 0, operation::parse>) {
                     skip_value<EETF>::op<Opts>(ctx, it, end);
                  }
                  else {
                     using val_t = refl_t<T, 0>;
                     if constexpr (always_skipped<val_t>) {
                        skip_value<EETF>::op<Opts>(ctx, it, end);
                     }
                     else if constexpr (reflectable<T>) {
                        parse<EETF>::op<Opts>(get_member(value, get<0>(fields)), ctx, it, end);
                     }
                     else {
                        parse<EETF>::op<Opts>(get_member(value, get<0>(reflect<T>::values)), ctx, it, end);
                     }
                  }
                  if (bool(ctx.error)) [[unlikely]] {
                     return;
                  }
                  if constexpr (Opts.error_on_missing_keys || Opts.partial_read) {
                     seen_fields[0] = true;
                     read_count = 1;
                  }
                  return;
               }
            }
         }

         for (uint32_t key_idx = 0; key_idx < arity; ++key_idx) {
            if constexpr (Opts.partial_read) {
               if (read_count >= N) {
                  for (uint32_t rem = key_idx; rem < arity; ++rem) {
                     skip_value<EETF>::op<Opts>(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]] return;
                     skip_value<EETF>::op<Opts>(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]] return;
                  }
                  return;
               }
            }

            std::string_view key;
            if (!etf::detail::read_key(ctx, it, end, key)) [[unlikely]] {
               return;
            }

            if constexpr (N > 0) {
               static constexpr auto HashInfo = hash_info<T>;
               const auto index = decode_hash_with_size<EETF, T, HashInfo, HashInfo.type>::op(
                  key.data(), key.data() + key.size(), key.size());

               bool matched = false;
               if (index < N) [[likely]] {
                  etf::detail::direct_visit<N>(
                     [&]<size_t I>() {
                        static constexpr auto TargetKey = get<I>(reflect<T>::keys);
                        static constexpr auto Length = TargetKey.size();
                        if ((Length == key.size()) && compare<Length>(TargetKey.data(), key.data())) [[likely]] {
                           matched = true;
                           if constexpr (Opts.error_on_missing_keys || Opts.partial_read) {
                              if (!seen_fields[I]) {
                                 seen_fields[I] = true;
                                 ++read_count;
                              }
                           }
                           else {
                              ++read_count;
                           }
                           if constexpr (skipped_by_meta<T, I, operation::parse>) {
                              skip_value<EETF>::op<Opts>(ctx, it, end);
                           }
                           else {
                              using val_t = refl_t<T, I>;
                              if constexpr (always_skipped<val_t>) {
                                 skip_value<EETF>::op<Opts>(ctx, it, end);
                              }
                              else if constexpr (reflectable<T>) {
                                 parse<EETF>::op<Opts>(get_member(value, get<I>(fields)), ctx, it, end);
                              }
                              else {
                                 parse<EETF>::op<Opts>(get_member(value, get<I>(reflect<T>::values)), ctx, it, end);
                              }
                           }
                        }
                     },
                     index);
               }

               if (!matched) [[unlikely]] {
                  if constexpr (Opts.error_on_unknown_keys) {
                     ctx.error = error_code::unknown_key;
                     return;
                  }
                  else {
                     skip_value<EETF>::op<Opts>(ctx, it, end);
                     if (parse_failed(ctx.error)) [[unlikely]] {
                        return;
                     }
                  }
               }

               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }

               if constexpr (Opts.partial_read) {
                  if (read_count >= N) {
                     for (uint32_t rem = key_idx + 1; rem < arity; ++rem) {
                        skip_value<EETF>::op<Opts>(ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]] return;
                        skip_value<EETF>::op<Opts>(ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]] return;
                     }
                     return;
                  }
               }
            }
            else {
               if constexpr (Opts.error_on_unknown_keys) {
                  ctx.error = error_code::unknown_key;
                  return;
               }
               else {
                  skip_value<EETF>::op<Opts>(ctx, it, end);
                  if (parse_failed(ctx.error)) [[unlikely]] {
                     return;
                  }
               }
            }
         }

         if constexpr (Opts.error_on_missing_keys) {
            static constexpr auto req_fields = required_fields<T, Opts>();
            if ((req_fields & seen_fields) != req_fields) {
               for (size_t i = 0; i < N; ++i) {
                  if (not seen_fields[i] && req_fields[i]) {
                     ctx.custom_error_message = reflect<T>::keys[i];
                     break;
                  }
               }
               ctx.error = error_code::missing_key;
               return;
            }
         }
      }
   };

   // Glaze value wrapper
   template <class T>
      requires(glaze_value_t<T> && !custom_read<T>)
   struct from<EETF, T> final
   {
      template <auto Opts, class Value, is_context Ctx, class It0, class It1>
      GLZ_ALWAYS_INLINE static void op(Value&& value, Ctx&& ctx, It0&& it, It1 end)
      {
         using V = std::decay_t<decltype(get_member(std::declval<Value>(), meta_wrapper_v<T>))>;
         from<EETF, V>::template op<Opts>(get_member(std::forward<Value>(value), meta_wrapper_v<T>),
                                          std::forward<Ctx>(ctx), std::forward<It0>(it), end);
      }
   };

   // Custom read specialization (types with static constexpr bool custom_read = true in glz::meta)
   template <custom_read T>
   struct from<EETF, T> final
   {
      template <auto Opts, class Value, is_context Ctx, class It0, class It1>
      GLZ_ALWAYS_INLINE static void op(Value&& value, Ctx&& ctx, It0&& it, It1 end)
      {
         using V = std::decay_t<T>;
         if constexpr (glaze_value_t<V>) {
            using M = std::decay_t<decltype(get_member(value, meta_wrapper_v<V>))>;
            from<EETF, M>::template op<Opts>(get_member(value, meta_wrapper_v<V>), ctx, it, end);
         }
         else if constexpr (requires { from<EETF, meta_t<V>>::template op<Opts>(std::forward<Value>(value), ctx, it, end); }) {
            from<EETF, meta_t<V>>::template op<Opts>(
               std::forward<Value>(value), ctx, it, end);
         }
      }
   };

   namespace etf::detail
   {
      template <class V>
      inline constexpr bool variant_is_object_alt = [] {
         using X = std::decay_t<V>;
         return glaze_object_t<X> || reflectable<X> || is_memory_object<X> || readable_map_t<X> || pair_t<X>;
      }();

      template <class V>
      inline constexpr bool variant_is_open_ended_object_alt = [] {
         using X = std::decay_t<V>;
         if constexpr (glaze_object_t<X> || reflectable<X> || is_memory_object<X>) {
            return false;
         }
         else {
            return readable_map_t<X> || pair_t<X>;
         }
      }();

      template <class V>
      inline constexpr bool variant_is_integer_alt = [] {
         using X = remove_meta_wrapper_t<std::decay_t<V>>;
         if constexpr (is_variant<X> || bool_t<X> || mimics_bool_t<X> || custom_bool_t<X> || always_null_t<X>) {
            return false;
         }
         else if constexpr (std::floating_point<X>) {
            return false;
         }
         else if constexpr (int_t<X>) {
            return true;
         }
         else if constexpr (has_mimic<X>) {
            return int_t<mimic_type<X>>;
         }
         else if constexpr (has_custom_meta_v<X>) {
            return custom_num_t<X> && !std::floating_point<custom_read_input_t<X>>;
         }
         else {
            return false;
         }
      }();

      template <class V>
      inline constexpr bool variant_is_float_alt = [] {
         using X = remove_meta_wrapper_t<std::decay_t<V>>;
         if constexpr (is_variant<X> || bool_t<X> || mimics_bool_t<X> || custom_bool_t<X> || always_null_t<X>) {
            return false;
         }
         else if constexpr (std::floating_point<X>) {
            return true;
         }
         else if constexpr (has_mimic<X>) {
            return std::floating_point<mimic_type<X>>;
         }
         else if constexpr (has_custom_meta_v<X>) {
            return custom_num_t<X> && std::floating_point<custom_read_input_t<X>>;
         }
         else {
            return false;
         }
      }();

      template <class V>
      inline constexpr bool variant_is_bool_alt = [] {
         using X = remove_meta_wrapper_t<std::decay_t<V>>;
         if constexpr (is_variant<X> || always_null_t<X>) {
            return false;
         }
         else {
            return bool_t<X> || mimics_bool_t<X> || custom_bool_t<X>;
         }
      }();

      template <class V>
      inline constexpr bool variant_is_str_alt = [] {
         using X = remove_meta_wrapper_t<std::decay_t<V>>;
         if constexpr (is_variant<X> || bool_t<X> || mimics_bool_t<X> || custom_bool_t<X> || always_null_t<X>) {
            return false;
         }
         else {
            return str_t<X> || glaze_enum_t<X> || std::is_enum_v<X> || mimics_str_t<X> || custom_str_t<X>;
         }
      }();

      template <class V>
      inline constexpr bool variant_is_null_alt = [] {
         using X = remove_meta_wrapper_t<std::decay_t<V>>;
         if constexpr (is_variant<X>) {
            return false;
         }
         else {
            return null_t<X> || always_null_t<X> || std::same_as<X, std::nullptr_t>;
         }
      }();

      template <class V>
      inline constexpr bool variant_is_array_alt = [] {
         using X = remove_meta_wrapper_t<std::decay_t<V>>;
         if constexpr (is_variant<X> || str_t<X> || readable_map_t<X> || writable_map_t<X> || pair_t<X> || glaze_object_t<X> || reflectable<X> || is_memory_object<X>) {
            return false;
         }
         else {
            return array_t<X> || readable_array_t<X> || glaze_array_t<X>;
         }
      }();

      template <class V>
      inline constexpr bool variant_is_tuple_alt = [] {
         using X = remove_meta_wrapper_t<std::decay_t<V>>;
         if constexpr (is_variant<X> || str_t<X>) {
            return false;
         }
         else {
            return tuple_t<X> || is_std_tuple<X> || pair_t<X>;
         }
      }();

      template <class V>
      inline constexpr bool variant_is_enum_alt = [] {
         using X = remove_meta_wrapper_t<std::decay_t<V>>;
         return std::is_enum_v<X> && !custom_read<X>;
      }();

      template <class T, class IdType, bool IsEnum = std::is_enum_v<IdType>, bool IsIntegral = std::integral<IdType>>
      struct variant_id_resolver;

      // Specialization for Enums: searches ids_v<T> linearly without ever instantiating variant_id_to_index
      template <class T, class IdType, bool IsIntegral>
      struct variant_id_resolver<T, IdType, true, IsIntegral>
      {
         GLZ_ALWAYS_INLINE static size_t resolve(IdType id) noexcept
         {
            for (size_t i = 0; i < ids_v<T>.size(); ++i) {
               if (ids_v<T>[i] == id) {
                  return i;
               }
            }
            return ids_v<T>.size();
         }
      };

      // Specialization for Integrals
      template <class T, class IdType>
      struct variant_id_resolver<T, IdType, false, true>
      {
         GLZ_ALWAYS_INLINE static size_t resolve(IdType id) noexcept
         {
            return variant_id_to_index<T>::op(id);
         }
      };

      // Specialization for Strings (neither enum nor integral)
      template <class T, class IdType>
      struct variant_id_resolver<T, IdType, false, false>
      {
         GLZ_ALWAYS_INLINE static size_t resolve(std::string_view id_view) noexcept
         {
            return variant_id_to_index<T>::op(id_view.data(), id_view.data() + id_view.size(), id_view.size());
         }
      };
   }

   // Variants
   template <is_variant T>
      requires(not custom_read<T>)
   struct from<EETF, T> final
   {
      static constexpr size_t variant_size = std::variant_size_v<T>;

      static constexpr size_t n_int = []<size_t... I>(std::index_sequence<I...>) {
         return (size_t(etf::detail::variant_is_integer_alt<std::variant_alternative_t<I, T>>) + ... + 0);
      }(std::make_index_sequence<variant_size>{});

      static constexpr size_t unique_int_idx = [] {
         size_t r = 0;
         size_t cnt = 0;
         for_each<variant_size>([&]<size_t I>() {
            if constexpr (etf::detail::variant_is_integer_alt<std::variant_alternative_t<I, T>>) {
               r = I;
               ++cnt;
            }
         });
         return (cnt == 1) ? r : 0;
      }();

      static constexpr size_t n_float = []<size_t... I>(std::index_sequence<I...>) {
         return (size_t(etf::detail::variant_is_float_alt<std::variant_alternative_t<I, T>>) + ... + 0);
      }(std::make_index_sequence<variant_size>{});

      static constexpr size_t unique_float_idx = [] {
         size_t r = 0;
         size_t cnt = 0;
         for_each<variant_size>([&]<size_t I>() {
            if constexpr (etf::detail::variant_is_float_alt<std::variant_alternative_t<I, T>>) {
               r = I;
               ++cnt;
            }
         });
         return (cnt == 1) ? r : 0;
      }();

      static constexpr size_t n_bool = []<size_t... I>(std::index_sequence<I...>) {
         return (size_t(etf::detail::variant_is_bool_alt<std::variant_alternative_t<I, T>>) + ... + 0);
      }(std::make_index_sequence<variant_size>{});

      static constexpr size_t unique_bool_idx = [] {
         size_t r = 0;
         size_t cnt = 0;
         for_each<variant_size>([&]<size_t I>() {
            if constexpr (etf::detail::variant_is_bool_alt<std::variant_alternative_t<I, T>>) {
               r = I;
               ++cnt;
            }
         });
         return (cnt == 1) ? r : 0;
      }();

      static constexpr size_t n_str = []<size_t... I>(std::index_sequence<I...>) {
         return (size_t(etf::detail::variant_is_str_alt<std::variant_alternative_t<I, T>>) + ... + 0);
      }(std::make_index_sequence<variant_size>{});

      static constexpr size_t unique_str_idx = [] {
         size_t r = 0;
         size_t cnt = 0;
         for_each<variant_size>([&]<size_t I>() {
            if constexpr (etf::detail::variant_is_str_alt<std::variant_alternative_t<I, T>>) {
               r = I;
               ++cnt;
            }
         });
         return (cnt == 1) ? r : 0;
      }();

      static constexpr size_t n_null = []<size_t... I>(std::index_sequence<I...>) {
         return (size_t(etf::detail::variant_is_null_alt<std::variant_alternative_t<I, T>>) + ... + 0);
      }(std::make_index_sequence<variant_size>{});

      static constexpr size_t unique_null_idx = [] {
         size_t r = 0;
         size_t cnt = 0;
         for_each<variant_size>([&]<size_t I>() {
            if constexpr (etf::detail::variant_is_null_alt<std::variant_alternative_t<I, T>>) {
               r = I;
               ++cnt;
            }
         });
         return (cnt == 1) ? r : 0;
      }();

      static constexpr size_t n_array = []<size_t... I>(std::index_sequence<I...>) {
         return (size_t(etf::detail::variant_is_array_alt<std::variant_alternative_t<I, T>>) + ... + 0);
      }(std::make_index_sequence<variant_size>{});

      static constexpr size_t unique_array_idx = [] {
         size_t r = 0;
         size_t cnt = 0;
         for_each<variant_size>([&]<size_t I>() {
            if constexpr (etf::detail::variant_is_array_alt<std::variant_alternative_t<I, T>>) {
               r = I;
               ++cnt;
            }
         });
         return (cnt == 1) ? r : 0;
      }();

      static constexpr size_t n_tuple = []<size_t... I>(std::index_sequence<I...>) {
         return (size_t(etf::detail::variant_is_tuple_alt<std::variant_alternative_t<I, T>>) + ... + 0);
      }(std::make_index_sequence<variant_size>{});

      static constexpr size_t unique_tuple_idx = [] {
         size_t r = 0;
         size_t cnt = 0;
         for_each<variant_size>([&]<size_t I>() {
            if constexpr (etf::detail::variant_is_tuple_alt<std::variant_alternative_t<I, T>>) {
               r = I;
               ++cnt;
            }
         });
         return (cnt == 1) ? r : 0;
      }();

      static constexpr size_t n_enum = []<size_t... I>(std::index_sequence<I...>) {
         return (size_t(etf::detail::variant_is_enum_alt<std::variant_alternative_t<I, T>>) + ... + 0);
      }(std::make_index_sequence<variant_size>{});

      static constexpr size_t unique_enum_idx = [] {
         size_t r = 0;
         size_t cnt = 0;
         for_each<variant_size>([&]<size_t I>() {
            if constexpr (etf::detail::variant_is_enum_alt<std::variant_alternative_t<I, T>>) {
               r = I;
               ++cnt;
            }
         });
         return (cnt == 1) ? r : 0;
      }();

      template <auto Opts>
      static void op(auto& value, is_context auto& ctx, auto& it, auto end) noexcept
      {
         if (it >= end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         const auto start = it;
         const auto depth = ctx.depth;

         // Check if input is an ETF map (object) and the variant has object alternatives
         if (static_cast<uint8_t>(*it) == etf::tag::MAP_EXT) {
            constexpr size_t n_obj = []<size_t... I>(std::index_sequence<I...>) {
               return (size_t(etf::detail::variant_is_object_alt<std::variant_alternative_t<I, T>>) + ... + 0);
            }(std::make_index_sequence<variant_size>{});

            if constexpr (n_obj > 0) {
               if constexpr (n_obj == 1 && tag_v<T>.empty()) {
                  constexpr size_t target_idx = [] {
                     size_t r = 0;
                     for_each<variant_size>([&]<size_t J>() {
                        if constexpr (etf::detail::variant_is_object_alt<std::variant_alternative_t<J, T>>) {
                           r = J;
                        }
                     });
                     return r;
                  }();

                  if (value.index() != target_idx) {
                     value.template emplace<target_idx>();
                  }
                  parse<EETF>::template op<Opts>(std::get<target_idx>(value), ctx, it, end);
                  if (!parse_failed(ctx.error)) {
                     return;
                  }
                  it = start;
                  ctx.depth = depth;
                  ctx.error = error_code::none;
                  ctx.custom_error_message = {};
               }
               else {
                  auto scan = it;
                  ++scan; // skip tag::MAP_EXT
                  if (end - scan < 4) [[unlikely]] {
                     ctx.error = error_code::unexpected_end;
                     return;
                  }
                  const uint32_t arity = etf::detail::read_be<uint32_t>(scan);
                  scan += 4;

                  constexpr bool tagged = not tag_v<T>.empty();

                  auto possible = bit_array<variant_size>{};
                  for_each<variant_size>([&]<size_t I>() {
                     if constexpr (etf::detail::variant_is_object_alt<std::variant_alternative_t<I, T>>) {
                        possible[I] = true;
                     }
                  });

                  static constexpr auto open_ended = [] {
                     auto m = bit_array<variant_size>{};
                     [&]<size_t... I>(std::index_sequence<I...>) {
                        ((m[I] = etf::detail::variant_is_open_ended_object_alt<std::variant_alternative_t<I, T>>), ...);
                     }(std::make_index_sequence<variant_size>{});
                     return m;
                  }();

                  static constexpr auto narrowing_bits = [] {
                     auto b = variant_deduction_bits<T>;
                     for (size_t k = 0; k < b.size(); ++k) {
                        for (size_t i = 0; i < variant_size; ++i) {
                           if (open_ended[i]) {
                              b[k][i] = true;
                           }
                        }
                     }
                     return b;
                  }();

                  bool foreign_key = false;
                  size_t tag_index = ids_v<T>.size();
                  bool tag_decoded = false;

                  for (uint32_t k = 0; k < arity; ++k) {
                     std::string_view key;
                     if (!etf::detail::read_key(ctx, scan, end, key)) [[unlikely]] {
                        return;
                     }

                     if constexpr (tagged) {
                        static constexpr auto Tag = tag_v<T>;
                        if (key.size() == Tag.size() && compare<Tag.size()>(key.data(), Tag.data())) {
                           using id_type = std::decay_t<decltype(ids_v<T>[0])>;
                           if constexpr (std::integral<id_type> || std::is_enum_v<id_type>) {
                              id_type id{};
                              parse<EETF>::template op<Opts>(id, ctx, scan, end);
                              if (parse_failed(ctx.error)) [[unlikely]] {
                                 return;
                              }
                              tag_index = etf::detail::variant_id_resolver<T, id_type>::resolve(id);
                              tag_decoded = true;
                           }
                           else {
                              std::string_view id_view{};
                              if (!etf::detail::read_atom_or_str(ctx, scan, end, id_view)) [[unlikely]] {
                                 return;
                              }
                              tag_index = etf::detail::variant_id_resolver<T, id_type>::resolve(id_view);
                              tag_decoded = true;
                           }
                           break;
                        }
                     }

                     if constexpr (variant_deduction_key_count<T> > 0) {
                        using dk = keys_wrapper<variant_deduction_keys<T>>;
                        static constexpr auto& H = hash_info<dk>;
                        const auto di = decode_hash_with_size<EETF, dk, H, H.type>::op(
                           key.data(), key.data() + key.size(), key.size());
                        if (di < variant_deduction_key_count<T> && variant_deduction_keys<T>[di] == key) {
                           possible = possible & narrowing_bits[di];
                           if constexpr (not tagged) {
                              if (possible.popcount() == 1) {
                                 break;
                              }
                           }
                        }
                        else {
                           foreign_key = true;
                        }
                     }
                     else {
                        foreign_key = true;
                     }

                     skip_value<EETF>::op<Opts>(ctx, scan, end);
                     if (parse_failed(ctx.error)) [[unlikely]] {
                        return;
                     }
                  }

                  size_t resolved = variant_size;
                  if constexpr (tagged) {
                     if (tag_decoded) {
                        if (tag_index < ids_v<T>.size()) [[likely]] {
                           resolved = tag_index;
                        }
                        else if constexpr (ids_v<T>.size() < variant_size) {
                           resolved = ids_v<T>.size();
                        }
                        else {
                           ctx.error = error_code::no_matching_variant_type;
                           ctx.custom_error_message = variant_ids_string_v<T>;
                           return;
                        }
                     }
                  }

                  if (resolved >= variant_size) {
                     if (foreign_key) {
                        auto open_possible = possible & open_ended;
                        if (open_possible.popcount() > 0) {
                           possible = open_possible;
                        }
                     }
                     const auto pc = possible.popcount();
                     if (pc == 1) {
                        resolved = size_t(possible.countr_zero());
                     }
                     else if (pc > 1) {
                        // Ambiguous: prefer the alternative with the fewest declared fields (JSON parity)
                        size_t best = variant_size;
                        size_t best_fields = std::numeric_limits<size_t>::max();
                        for_each<variant_size>([&]<size_t I>() {
                           if (possible[I]) {
                              using V = std::variant_alternative_t<I, T>;
                              using X = std::conditional_t<is_memory_object<V>, memory_type<V>, V>;
                              size_t f = std::numeric_limits<size_t>::max();
                              if constexpr (glaze_object_t<X> || reflectable<X>) {
                                 f = reflect<X>::size;
                              }
                              if (f < best_fields) {
                                 best_fields = f;
                                 best = I;
                              }
                           }
                        });
                        resolved = best;
                     }
                  }

                  if (resolved < variant_size) {
                     ctx.depth = depth;
                     ctx.error = error_code::none;
                     ctx.custom_error_message = {};
                     bool deduced_matched = false;
                     etf::detail::direct_visit<variant_size>(
                        [&]<size_t I>() {
                           if (value.index() != I) {
                              value.template emplace<I>();
                           }
                           parse<EETF>::template op<Opts>(std::get<I>(value), ctx, it, end);
                           if (!parse_failed(ctx.error)) {
                              deduced_matched = true;
                           }
                        },
                        resolved);

                     if (deduced_matched) {
                        return;
                     }

                     // If the resolved candidate failed, try other candidates that were also possible
                     bool matched = false;
                     for_each<variant_size>([&]<size_t I>() {
                        if (matched || I == resolved || !possible[I]) return;
                        it = start;
                        ctx.depth = depth;
                        ctx.error = error_code::none;
                        ctx.custom_error_message = {};
                        using V = std::variant_alternative_t<I, T>;
                        V v{};
                        parse<EETF>::template op<Opts>(v, ctx, it, end);
                        if (!parse_failed(ctx.error)) {
                           value.template emplace<I>(std::move(v));
                           matched = true;
                        }
                     });
                     if (matched) {
                        return;
                     }
                     it = start;
                     ctx.depth = depth;
                     ctx.error = error_code::none;
                     ctx.custom_error_message = {};
                  }
               }
            }
         }

         bool matched = false;
         const auto tag = static_cast<uint8_t>(*it);

         switch (tag) {
         case etf::tag::SMALL_INTEGER_EXT:
         case etf::tag::INTEGER_EXT:
         case etf::tag::SMALL_BIG_EXT:
         case etf::tag::LARGE_BIG_EXT: {
            if constexpr (n_int == 1) {
               if (value.index() != unique_int_idx) {
                  value.template emplace<unique_int_idx>();
               }
               parse<EETF>::template op<Opts>(std::get<unique_int_idx>(value), ctx, it, end);
               if (!parse_failed(ctx.error)) {
                  return;
               }
               it = start;
               ctx.depth = depth;
               ctx.error = error_code::none;
               ctx.custom_error_message = {};
            }
            else if constexpr (n_int > 1) {
               for_each<variant_size>([&]<size_t I>() {
                  if (matched) return;
                  using V = std::variant_alternative_t<I, T>;
                  if constexpr (etf::detail::variant_is_integer_alt<V>) {
                     it = start;
                     ctx.depth = depth;
                     ctx.error = error_code::none;
                     ctx.custom_error_message = {};
                     V v{};
                     parse<EETF>::template op<Opts>(v, ctx, it, end);
                     if (!parse_failed(ctx.error)) {
                        value.template emplace<I>(std::move(v));
                        matched = true;
                     }
                  }
               });
               if (matched) return;
               it = start;
               ctx.depth = depth;
               ctx.error = error_code::none;
               ctx.custom_error_message = {};
            }
            if constexpr (n_enum > 0) {
               if constexpr (n_enum == 1) {
                  if (value.index() != unique_enum_idx) {
                     value.template emplace<unique_enum_idx>();
                  }
                  parse<EETF>::template op<Opts>(std::get<unique_enum_idx>(value), ctx, it, end);
                  if (!parse_failed(ctx.error)) {
                     return;
                  }
                  it = start;
                  ctx.depth = depth;
                  ctx.error = error_code::none;
                  ctx.custom_error_message = {};
               }
               else {
                  for_each<variant_size>([&]<size_t I>() {
                     if (matched) return;
                     using V = std::variant_alternative_t<I, T>;
                     if constexpr (etf::detail::variant_is_enum_alt<V>) {
                        it = start;
                        ctx.depth = depth;
                        ctx.error = error_code::none;
                        ctx.custom_error_message = {};
                        V v{};
                        parse<EETF>::template op<Opts>(v, ctx, it, end);
                        if (!parse_failed(ctx.error)) {
                           value.template emplace<I>(std::move(v));
                           matched = true;
                        }
                     }
                  });
                  if (matched) return;
                  it = start;
                  ctx.depth = depth;
                  ctx.error = error_code::none;
                  ctx.custom_error_message = {};
               }
            }
            if constexpr (n_float > 0) {
               if constexpr (n_float == 1) {
                  if (value.index() != unique_float_idx) {
                     value.template emplace<unique_float_idx>();
                  }
                  parse<EETF>::template op<Opts>(std::get<unique_float_idx>(value), ctx, it, end);
                  if (!parse_failed(ctx.error)) {
                     return;
                  }
                  it = start;
                  ctx.depth = depth;
                  ctx.error = error_code::none;
                  ctx.custom_error_message = {};
               }
               else {
                  for_each<variant_size>([&]<size_t I>() {
                     if (matched) return;
                     using V = std::variant_alternative_t<I, T>;
                     if constexpr (etf::detail::variant_is_float_alt<V>) {
                        it = start;
                        ctx.depth = depth;
                        ctx.error = error_code::none;
                        ctx.custom_error_message = {};
                        V v{};
                        parse<EETF>::template op<Opts>(v, ctx, it, end);
                        if (!parse_failed(ctx.error)) {
                           value.template emplace<I>(std::move(v));
                           matched = true;
                        }
                     }
                  });
                  if (matched) return;
                  it = start;
                  ctx.depth = depth;
                  ctx.error = error_code::none;
                  ctx.custom_error_message = {};
               }
            }
            break;
         }
         case etf::tag::NEW_FLOAT_EXT:
         case etf::tag::FLOAT_EXT: {
            if constexpr (n_float == 1) {
               if (value.index() != unique_float_idx) {
                  value.template emplace<unique_float_idx>();
               }
               parse<EETF>::template op<Opts>(std::get<unique_float_idx>(value), ctx, it, end);
               if (!parse_failed(ctx.error)) {
                  return;
               }
               it = start;
               ctx.depth = depth;
               ctx.error = error_code::none;
               ctx.custom_error_message = {};
            }
            else if constexpr (n_float > 1) {
               for_each<variant_size>([&]<size_t I>() {
                  if (matched) return;
                  using V = std::variant_alternative_t<I, T>;
                  if constexpr (etf::detail::variant_is_float_alt<V>) {
                     it = start;
                     ctx.depth = depth;
                     ctx.error = error_code::none;
                     ctx.custom_error_message = {};
                     V v{};
                     parse<EETF>::template op<Opts>(v, ctx, it, end);
                     if (!parse_failed(ctx.error)) {
                        value.template emplace<I>(std::move(v));
                        matched = true;
                     }
                  }
               });
               if (matched) return;
               it = start;
               ctx.depth = depth;
               ctx.error = error_code::none;
               ctx.custom_error_message = {};
            }
            break;
         }
         case etf::tag::SMALL_ATOM_UTF8_EXT:
         case etf::tag::ATOM_UTF8_EXT:
         case etf::tag::SMALL_ATOM_EXT:
         case etf::tag::ATOM_EXT: {
            auto scan = it;
            std::string_view atom_view{};
            if (etf::detail::read_atom_or_str(ctx, scan, end, atom_view)) {
               ctx.error = error_code::none;
               if (atom_view == "true" || atom_view == "false") {
                  if constexpr (n_bool == 1) {
                     if (value.index() != unique_bool_idx) {
                        value.template emplace<unique_bool_idx>();
                     }
                     parse<EETF>::template op<Opts>(std::get<unique_bool_idx>(value), ctx, it, end);
                     if (!parse_failed(ctx.error)) {
                        return;
                     }
                     it = start;
                     ctx.depth = depth;
                     ctx.error = error_code::none;
                     ctx.custom_error_message = {};
                  }
                  else if constexpr (n_bool > 1) {
                     for_each<variant_size>([&]<size_t I>() {
                        if (matched) return;
                        using V = std::variant_alternative_t<I, T>;
                        if constexpr (etf::detail::variant_is_bool_alt<V>) {
                           it = start;
                           ctx.depth = depth;
                           ctx.error = error_code::none;
                           ctx.custom_error_message = {};
                           V v{};
                           parse<EETF>::template op<Opts>(v, ctx, it, end);
                           if (!parse_failed(ctx.error)) {
                              value.template emplace<I>(std::move(v));
                              matched = true;
                           }
                        }
                     });
                     if (matched) return;
                     it = start;
                     ctx.depth = depth;
                     ctx.error = error_code::none;
                     ctx.custom_error_message = {};
                  }
               }
               else if (atom_view == "nil" || atom_view == "null" || atom_view == "undefined") {
                  if constexpr (n_null == 1) {
                     if (value.index() != unique_null_idx) {
                        value.template emplace<unique_null_idx>();
                     }
                     parse<EETF>::template op<Opts>(std::get<unique_null_idx>(value), ctx, it, end);
                     if (!parse_failed(ctx.error)) {
                        return;
                     }
                     it = start;
                     ctx.depth = depth;
                     ctx.error = error_code::none;
                     ctx.custom_error_message = {};
                  }
                  else if constexpr (n_null > 1) {
                     for_each<variant_size>([&]<size_t I>() {
                        if (matched) return;
                        using V = std::variant_alternative_t<I, T>;
                        if constexpr (etf::detail::variant_is_null_alt<V>) {
                           it = start;
                           ctx.depth = depth;
                           ctx.error = error_code::none;
                           ctx.custom_error_message = {};
                           V v{};
                           parse<EETF>::template op<Opts>(v, ctx, it, end);
                           if (!parse_failed(ctx.error)) {
                              value.template emplace<I>(std::move(v));
                              matched = true;
                           }
                        }
                     });
                     if (matched) return;
                     it = start;
                     ctx.depth = depth;
                     ctx.error = error_code::none;
                     ctx.custom_error_message = {};
                  }
               }
               else {
                  if constexpr (n_enum > 0) {
                     if constexpr (n_enum == 1) {
                        if (value.index() != unique_enum_idx) {
                           value.template emplace<unique_enum_idx>();
                        }
                        parse<EETF>::template op<Opts>(std::get<unique_enum_idx>(value), ctx, it, end);
                        if (!parse_failed(ctx.error)) {
                           return;
                        }
                        it = start;
                        ctx.depth = depth;
                        ctx.error = error_code::none;
                        ctx.custom_error_message = {};
                     }
                     else {
                        for_each<variant_size>([&]<size_t I>() {
                           if (matched) return;
                           using V = std::variant_alternative_t<I, T>;
                           if constexpr (etf::detail::variant_is_enum_alt<V>) {
                              it = start;
                              ctx.depth = depth;
                              ctx.error = error_code::none;
                              ctx.custom_error_message = {};
                              V v{};
                              parse<EETF>::template op<Opts>(v, ctx, it, end);
                              if (!parse_failed(ctx.error)) {
                                 value.template emplace<I>(std::move(v));
                                 matched = true;
                              }
                           }
                        });
                        if (matched) return;
                        it = start;
                        ctx.depth = depth;
                        ctx.error = error_code::none;
                        ctx.custom_error_message = {};
                     }
                  }
                  if constexpr (n_str > 0) {
                     if constexpr (n_str == 1) {
                        if (value.index() != unique_str_idx) {
                           value.template emplace<unique_str_idx>();
                        }
                        parse<EETF>::template op<Opts>(std::get<unique_str_idx>(value), ctx, it, end);
                        if (!parse_failed(ctx.error)) {
                           return;
                        }
                        it = start;
                        ctx.depth = depth;
                        ctx.error = error_code::none;
                        ctx.custom_error_message = {};
                     }
                     else {
                        for_each<variant_size>([&]<size_t I>() {
                           if (matched) return;
                           using V = std::variant_alternative_t<I, T>;
                           if constexpr (etf::detail::variant_is_str_alt<V>) {
                              it = start;
                              ctx.depth = depth;
                              ctx.error = error_code::none;
                              ctx.custom_error_message = {};
                              V v{};
                              parse<EETF>::template op<Opts>(v, ctx, it, end);
                              if (!parse_failed(ctx.error)) {
                                 value.template emplace<I>(std::move(v));
                                 matched = true;
                              }
                           }
                        });
                        if (matched) return;
                        it = start;
                        ctx.depth = depth;
                        ctx.error = error_code::none;
                        ctx.custom_error_message = {};
                     }
                  }
               }
            }
            else {
               ctx.error = error_code::none;
            }
            break;
         }
         case etf::tag::BINARY_EXT: {
            if constexpr (n_str == 1 && n_array == 0) {
               if (value.index() != unique_str_idx) {
                  value.template emplace<unique_str_idx>();
               }
               parse<EETF>::template op<Opts>(std::get<unique_str_idx>(value), ctx, it, end);
               if (!parse_failed(ctx.error)) {
                  return;
               }
               it = start;
               ctx.depth = depth;
               ctx.error = error_code::none;
               ctx.custom_error_message = {};
            }
            else if constexpr (n_str == 0 && n_array == 1) {
               if (value.index() != unique_array_idx) {
                  value.template emplace<unique_array_idx>();
               }
               parse<EETF>::template op<Opts>(std::get<unique_array_idx>(value), ctx, it, end);
               if (!parse_failed(ctx.error)) {
                  return;
               }
               it = start;
               ctx.depth = depth;
               ctx.error = error_code::none;
               ctx.custom_error_message = {};
            }
            else if constexpr (n_str > 0 || n_array > 0) {
               if constexpr (n_str > 0) {
                  for_each<variant_size>([&]<size_t I>() {
                     if (matched) return;
                     using V = std::variant_alternative_t<I, T>;
                     if constexpr (etf::detail::variant_is_str_alt<V>) {
                        it = start;
                        ctx.depth = depth;
                        ctx.error = error_code::none;
                        ctx.custom_error_message = {};
                        V v{};
                        parse<EETF>::template op<Opts>(v, ctx, it, end);
                        if (!parse_failed(ctx.error)) {
                           value.template emplace<I>(std::move(v));
                           matched = true;
                        }
                     }
                  });
                  if (matched) return;
                  it = start;
                  ctx.depth = depth;
                  ctx.error = error_code::none;
                  ctx.custom_error_message = {};
               }
               if constexpr (n_array > 0) {
                  for_each<variant_size>([&]<size_t I>() {
                     if (matched) return;
                     using V = std::variant_alternative_t<I, T>;
                     if constexpr (etf::detail::variant_is_array_alt<V>) {
                        it = start;
                        ctx.depth = depth;
                        ctx.error = error_code::none;
                        ctx.custom_error_message = {};
                        V v{};
                        parse<EETF>::template op<Opts>(v, ctx, it, end);
                        if (!parse_failed(ctx.error)) {
                           value.template emplace<I>(std::move(v));
                           matched = true;
                        }
                     }
                  });
                  if (matched) return;
                  it = start;
                  ctx.depth = depth;
                  ctx.error = error_code::none;
                  ctx.custom_error_message = {};
               }
            }
            break;
         }
         case etf::tag::STRING_EXT: {
            if constexpr (n_str == 1 && n_array == 0) {
               if (value.index() != unique_str_idx) {
                  value.template emplace<unique_str_idx>();
               }
               parse<EETF>::template op<Opts>(std::get<unique_str_idx>(value), ctx, it, end);
               if (!parse_failed(ctx.error)) {
                  return;
               }
               it = start;
               ctx.depth = depth;
               ctx.error = error_code::none;
               ctx.custom_error_message = {};
            }
            else if constexpr (n_str == 0 && n_array == 1) {
               if (value.index() != unique_array_idx) {
                  value.template emplace<unique_array_idx>();
               }
               parse<EETF>::template op<Opts>(std::get<unique_array_idx>(value), ctx, it, end);
               if (!parse_failed(ctx.error)) {
                  return;
               }
               it = start;
               ctx.depth = depth;
               ctx.error = error_code::none;
               ctx.custom_error_message = {};
            }
            else if constexpr (n_str > 0 || n_array > 0) {
               if constexpr (n_str > 0) {
                  for_each<variant_size>([&]<size_t I>() {
                     if (matched) return;
                     using V = std::variant_alternative_t<I, T>;
                     if constexpr (etf::detail::variant_is_str_alt<V>) {
                        it = start;
                        ctx.depth = depth;
                        ctx.error = error_code::none;
                        ctx.custom_error_message = {};
                        V v{};
                        parse<EETF>::template op<Opts>(v, ctx, it, end);
                        if (!parse_failed(ctx.error)) {
                           value.template emplace<I>(std::move(v));
                           matched = true;
                        }
                     }
                  });
                  if (matched) return;
                  it = start;
                  ctx.depth = depth;
                  ctx.error = error_code::none;
                  ctx.custom_error_message = {};
               }
               if constexpr (n_array > 0) {
                  for_each<variant_size>([&]<size_t I>() {
                     if (matched) return;
                     using V = std::variant_alternative_t<I, T>;
                     if constexpr (etf::detail::variant_is_array_alt<V>) {
                        it = start;
                        ctx.depth = depth;
                        ctx.error = error_code::none;
                        ctx.custom_error_message = {};
                        V v{};
                        parse<EETF>::template op<Opts>(v, ctx, it, end);
                        if (!parse_failed(ctx.error)) {
                           value.template emplace<I>(std::move(v));
                           matched = true;
                        }
                     }
                  });
                  if (matched) return;
                  it = start;
                  ctx.depth = depth;
                  ctx.error = error_code::none;
                  ctx.custom_error_message = {};
               }
            }
            break;
         }
         case etf::tag::LIST_EXT: {
            if constexpr (n_array == 1) {
               if (value.index() != unique_array_idx) {
                  value.template emplace<unique_array_idx>();
               }
               parse<EETF>::template op<Opts>(std::get<unique_array_idx>(value), ctx, it, end);
               if (!parse_failed(ctx.error)) {
                  return;
               }
               it = start;
               ctx.depth = depth;
               ctx.error = error_code::none;
               ctx.custom_error_message = {};
            }
            else if constexpr (n_array > 1) {
               for_each<variant_size>([&]<size_t I>() {
                  if (matched) return;
                  using V = std::variant_alternative_t<I, T>;
                  if constexpr (etf::detail::variant_is_array_alt<V>) {
                     it = start;
                     ctx.depth = depth;
                     ctx.error = error_code::none;
                     ctx.custom_error_message = {};
                     V v{};
                     parse<EETF>::template op<Opts>(v, ctx, it, end);
                     if (!parse_failed(ctx.error)) {
                        value.template emplace<I>(std::move(v));
                        matched = true;
                     }
                  }
               });
               if (matched) return;
               it = start;
               ctx.depth = depth;
               ctx.error = error_code::none;
               ctx.custom_error_message = {};
            }
            break;
         }
         case etf::tag::NIL_EXT: {
            if constexpr (n_null == 1 && n_array == 0) {
               if (value.index() != unique_null_idx) {
                  value.template emplace<unique_null_idx>();
               }
               parse<EETF>::template op<Opts>(std::get<unique_null_idx>(value), ctx, it, end);
               if (!parse_failed(ctx.error)) {
                  return;
               }
               it = start;
               ctx.depth = depth;
               ctx.error = error_code::none;
               ctx.custom_error_message = {};
            }
            else if constexpr (n_array == 1 && n_null == 0) {
               if (value.index() != unique_array_idx) {
                  value.template emplace<unique_array_idx>();
               }
               parse<EETF>::template op<Opts>(std::get<unique_array_idx>(value), ctx, it, end);
               if (!parse_failed(ctx.error)) {
                  return;
               }
               it = start;
               ctx.depth = depth;
               ctx.error = error_code::none;
               ctx.custom_error_message = {};
            }
            else if constexpr (n_null > 0 || n_array > 0) {
               if constexpr (n_null > 0) {
                  for_each<variant_size>([&]<size_t I>() {
                     if (matched) return;
                     using V = std::variant_alternative_t<I, T>;
                     if constexpr (etf::detail::variant_is_null_alt<V>) {
                        it = start;
                        ctx.depth = depth;
                        ctx.error = error_code::none;
                        ctx.custom_error_message = {};
                        V v{};
                        parse<EETF>::template op<Opts>(v, ctx, it, end);
                        if (!parse_failed(ctx.error)) {
                           value.template emplace<I>(std::move(v));
                           matched = true;
                        }
                     }
                  });
                  if (matched) return;
                  it = start;
                  ctx.depth = depth;
                  ctx.error = error_code::none;
                  ctx.custom_error_message = {};
               }
               if constexpr (n_array > 0) {
                  for_each<variant_size>([&]<size_t I>() {
                     if (matched) return;
                     using V = std::variant_alternative_t<I, T>;
                     if constexpr (etf::detail::variant_is_array_alt<V>) {
                        it = start;
                        ctx.depth = depth;
                        ctx.error = error_code::none;
                        ctx.custom_error_message = {};
                        V v{};
                        parse<EETF>::template op<Opts>(v, ctx, it, end);
                        if (!parse_failed(ctx.error)) {
                           value.template emplace<I>(std::move(v));
                           matched = true;
                        }
                     }
                  });
                  if (matched) return;
                  it = start;
                  ctx.depth = depth;
                  ctx.error = error_code::none;
                  ctx.custom_error_message = {};
               }
            }
            break;
         }
         case etf::tag::SMALL_TUPLE_EXT:
         case etf::tag::LARGE_TUPLE_EXT: {
            if constexpr (n_tuple == 1) {
               if (value.index() != unique_tuple_idx) {
                  value.template emplace<unique_tuple_idx>();
               }
               parse<EETF>::template op<Opts>(std::get<unique_tuple_idx>(value), ctx, it, end);
               if (!parse_failed(ctx.error)) {
                  return;
               }
               it = start;
               ctx.depth = depth;
               ctx.error = error_code::none;
               ctx.custom_error_message = {};
            }
            else if constexpr (n_tuple > 1) {
               for_each<variant_size>([&]<size_t I>() {
                  if (matched) return;
                  using V = std::variant_alternative_t<I, T>;
                  if constexpr (etf::detail::variant_is_tuple_alt<V>) {
                     it = start;
                     ctx.depth = depth;
                     ctx.error = error_code::none;
                     ctx.custom_error_message = {};
                     V v{};
                     parse<EETF>::template op<Opts>(v, ctx, it, end);
                     if (!parse_failed(ctx.error)) {
                        value.template emplace<I>(std::move(v));
                        matched = true;
                     }
                  }
               });
               if (matched) return;
               it = start;
               ctx.depth = depth;
               ctx.error = error_code::none;
               ctx.custom_error_message = {};
            }
            break;
         }
         default:
            break;
         }

         // Pass 1: Try all non-skip alternatives first
         for_each<variant_size>([&]<size_t I>() {
            if (matched) return;
            using V = std::variant_alternative_t<I, T>;
            if constexpr (!std::same_as<V, skip>) {
               it = start;
               ctx.depth = depth;
               ctx.error = error_code::none;
               ctx.custom_error_message = {};
               V v{};
               parse<EETF>::template op<Opts>(v, ctx, it, end);
               if (!parse_failed(ctx.error)) {
                  value.template emplace<I>(std::move(v));
                  matched = true;
               }
            }
         });

         // Pass 2: Fall back to skip if present and no concrete alternative matched
         if (!matched) {
            for_each<variant_size>([&]<size_t I>() {
               if (matched) return;
               using V = std::variant_alternative_t<I, T>;
               if constexpr (std::same_as<V, skip>) {
                  it = start;
                  ctx.depth = depth;
                  ctx.error = error_code::none;
                  ctx.custom_error_message = {};
                  V v{};
                  parse<EETF>::template op<Opts>(v, ctx, it, end);
                  if (!parse_failed(ctx.error)) {
                     value.template emplace<I>(std::move(v));
                     matched = true;
                  }
               }
            });
         }

         if (!matched) {
            it = start;
            ctx.depth = depth;
            ctx.error = error_code::no_matching_variant_type;
         }
      }
   };

   // Generic JSON wrapper for dynamically typed payloads (e.g. glz::generic in rescue_payload)
   template <num_mode Mode, template <class> class MapType>
   struct from<EETF, generic_json<Mode, MapType>> final
   {
      template <auto Opts>
      static void op(generic_json<Mode, MapType>& value, is_context auto& ctx, auto& it, auto end)
      {
         if (it >= end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         const auto tag = static_cast<uint8_t>(*it);
         switch (tag) {
         case etf::tag::SMALL_INTEGER_EXT: {
            if (end - it < 2) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            const auto n = static_cast<uint8_t>(*(it + 1));
            it += 2;
            if constexpr (Mode == num_mode::u64) {
               value.data = static_cast<uint64_t>(n);
            }
            else if constexpr (Mode == num_mode::i64) {
               value.data = static_cast<int64_t>(n);
            }
            else {
               value.data = static_cast<double>(n);
            }
            break;
         }
         case etf::tag::INTEGER_EXT: {
            if (end - it < 5) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            const auto val = static_cast<int32_t>(etf::detail::read_be<uint32_t>(it + 1));
            it += 5;
            if constexpr (Mode == num_mode::u64) {
               if (val >= 0) {
                  value.data = static_cast<uint64_t>(val);
               }
               else {
                  value.data = static_cast<int64_t>(val);
               }
            }
            else if constexpr (Mode == num_mode::i64) {
               value.data = static_cast<int64_t>(val);
            }
            else {
               value.data = static_cast<double>(val);
            }
            break;
         }
         case etf::tag::SMALL_BIG_EXT:
         case etf::tag::LARGE_BIG_EXT: {
            const auto start = it;
            const auto try_alternative = [&]<class V>(std::type_identity<V>) {
               ctx.error = error_code::none;
               ctx.custom_error_message = {};
               it = start;
               V v{};
               if (!etf::detail::read_number(ctx, it, end, v)) return false;
               value.data = v;
               return true;
            };

            if constexpr (Mode == num_mode::u64) {
               if (!try_alternative(std::type_identity<uint64_t>{}) &&
                   !try_alternative(std::type_identity<int64_t>{})) {
                  if (!try_alternative(std::type_identity<double>{})) return;
               }
            }
            else if constexpr (Mode == num_mode::i64) {
               if (!try_alternative(std::type_identity<int64_t>{})) {
                  if (!try_alternative(std::type_identity<double>{})) return;
               }
            }
            else {
               double d{};
               if (!etf::detail::read_number(ctx, it, end, d)) return;
               value.data = d;
            }
            break;
         }
         case etf::tag::NEW_FLOAT_EXT:
         case etf::tag::FLOAT_EXT: {
            double d{};
            if (!etf::detail::read_number(ctx, it, end, d)) [[unlikely]] return;
            value.data = d;
            break;
         }
         case etf::tag::SMALL_ATOM_UTF8_EXT:
         case etf::tag::SMALL_ATOM_EXT:
         case etf::tag::ATOM_UTF8_EXT:
         case etf::tag::ATOM_EXT: {
            std::string_view atom;
            if (!etf::detail::read_atom_or_str(ctx, it, end, atom)) [[unlikely]] return;
            if (atom.size() == 4 && compare<4>(atom.data(), "true")) { // NOLINT(bugprone-suspicious-stringview-data-usage)
               value.data = true;
            }
            else if (atom.size() == 5 && compare<5>(atom.data(), "false")) { // NOLINT(bugprone-suspicious-stringview-data-usage)
               value.data = false;
            }
            else if ((atom.size() == 3 && compare<3>(atom.data(), "nil")) || // NOLINT(bugprone-suspicious-stringview-data-usage)
                     (atom.size() == 4 && compare<4>(atom.data(), "null")) || // NOLINT(bugprone-suspicious-stringview-data-usage)
                     (atom.size() == 9 && compare<9>(atom.data(), "undefined"))) { // NOLINT(bugprone-suspicious-stringview-data-usage)
               value.data = nullptr;
            }
            else {
               value.data = std::string(atom);
            }
            break;
         }
         case etf::tag::BINARY_EXT:
         case etf::tag::STRING_EXT: {
            std::string s;
            from<EETF, std::string>::template op<Opts>(s, ctx, it, end);
            if (parse_failed(ctx.error)) [[unlikely]] return;
            value.data = std::move(s);
            break;
         }
         case etf::tag::NIL_EXT: {
            ++it;
            value.data = typename generic_json<Mode, MapType>::array_t{};
            break;
         }
         case etf::tag::LIST_EXT:
         case etf::tag::SMALL_TUPLE_EXT:
         case etf::tag::LARGE_TUPLE_EXT: {
            typename generic_json<Mode, MapType>::array_t arr;
            from<EETF, decltype(arr)>::template op<Opts>(arr, ctx, it, end);
            if (parse_failed(ctx.error)) [[unlikely]] return;
            value.data = std::move(arr);
            break;
         }
         case etf::tag::MAP_EXT: {
            typename generic_json<Mode, MapType>::object_t obj;
            from<EETF, decltype(obj)>::template op<Opts>(obj, ctx, it, end);
            if (parse_failed(ctx.error)) [[unlikely]] return;
            value.data = std::move(obj);
            break;
         }
         default:
            ctx.error = error_code::syntax_error;
            return;
         }
      }
   };

   // std::chrono::system_clock::time_point (discusy::timestamp): parse from ISO 8601 string or numeric timestamp
   template <is_system_time_point T>
      requires(not custom_read<T>)
   struct from<EETF, T> final
   {
      template <auto Opts>
      static void op(auto& value, is_context auto& ctx, auto& it, auto end) noexcept
      {
         if (it >= end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         const auto tag = static_cast<uint8_t>(*it);
         if (tag == etf::tag::BINARY_EXT || tag == etf::tag::STRING_EXT) {
            std::string_view str{};
            if (etf::detail::read_str(ctx, it, end, str)) {
               using Duration = std::remove_cvref_t<T>::duration;
               using Period = Duration::period;
               if constexpr (std::ratio_equal_v<Period, std::ratio<86400>>) {
                  if (str.size() == 10) {
                     std::chrono::year_month_day ymd{};
                     chrono_detail::parse_ymd(str, ymd, ctx.error);
                     if (parse_failed(ctx.error)) [[unlikely]] return;
                     value = std::chrono::time_point_cast<Duration>(std::chrono::sys_days{ymd});
                     return;
                  }
               }
               chrono_detail::parse_iso8601(str, value, ctx.error);
               return;
            }
         }
         else {
            int64_t num{};
            if (etf::detail::read_number(ctx, it, end, num)) {
               using Duration = std::remove_cvref_t<T>::duration;
               using Period = Duration::period;
               bool ok = false;
               if constexpr (std::ratio_equal_v<Period, std::milli>) {
                  ok = chrono_detail::make_sys_time(
                     value, std::chrono::seconds{num / 1000}, std::chrono::milliseconds{num % 1000});
               }
               else if constexpr (std::ratio_equal_v<Period, std::ratio<1>>) {
                  ok = chrono_detail::make_sys_time(
                     value, std::chrono::seconds{num}, std::chrono::nanoseconds{0});
               }
               else {
                  if (num > 10000000000LL || num < -10000000000LL) {
                     ok = chrono_detail::make_sys_time(
                        value, std::chrono::seconds{num / 1000}, std::chrono::milliseconds{num % 1000});
                  }
                  else {
                     ok = chrono_detail::make_sys_time(
                        value, std::chrono::seconds{num}, std::chrono::nanoseconds{0});
                  }
               }
               if (!ok) [[unlikely]] {
                  ctx.error = error_code::parse_error;
               }
               return;
            }
         }

         ctx.error = error_code::syntax_error;
      }
   };

   // std::chrono::year_month_day
   template <is_year_month_day T>
      requires(not custom_read<T>)
   struct from<EETF, T> final
   {
      template <auto Opts>
      static void op(auto& value, is_context auto& ctx, auto& it, auto end) noexcept
      {
         std::string_view str{};
         if (!etf::detail::read_str(ctx, it, end, str)) {
            ctx.error = error_code::syntax_error;
            return;
         }
         chrono_detail::parse_ymd(str, value, ctx.error);
      }
   };

   // Raw JSON & Text wrappers
   template <class T>
   struct from<EETF, basic_raw_json<T>> final
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& it, auto end)
      {
         auto it_start = it;
         skip_value<EETF>::op<Opts>(ctx, it, end);
         if (parse_failed(ctx.error)) [[unlikely]] {
            return;
         }
         value.str = T{std::to_address(it_start), static_cast<size_t>(it - it_start)};
      }
   };

   template <class T>
   struct from<EETF, basic_text<T>> final
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& it, auto end)
      {
         auto it_start = it;
         skip_value<EETF>::op<Opts>(ctx, it, end);
         if (parse_failed(ctx.error)) [[unlikely]] {
            return;
         }
         value.str = T{std::to_address(it_start), static_cast<size_t>(it - it_start)};
      }
   };

   // Top-level read helper functions for ETF
   template <auto Opts = etf_opts, read_supported<EETF> T, class Buffer>
   [[nodiscard]] inline error_ctx read_etf(T& value, Buffer&& buffer)
   {
      return read<Opts>(value, std::forward<Buffer>(buffer));
   }

   template <auto Opts = etf_opts, read_supported<EETF> T, class Buffer>
   [[nodiscard]] inline error_ctx read_etf(T& value, Buffer&& buffer, is_context auto&& ctx)
   {
      return read<Opts>(value, std::forward<Buffer>(buffer), ctx);
   }
} // namespace glz
