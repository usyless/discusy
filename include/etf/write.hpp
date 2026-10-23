#pragma once

#include <bit>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <type_traits>

#include <glaze/core/context.hpp>
#include <glaze/core/opts.hpp>
#include <glaze/core/write.hpp>
#include <glaze/core/buffer_traits.hpp>
#include <glaze/core/reflect.hpp>
#include <glaze/core/to.hpp>
#include <glaze/core/chrono.hpp>
#include <glaze/core/custom.hpp>
#include <glaze/core/write_chars.hpp>
#include <glaze/core/wrappers.hpp>
#include <glaze/util/for_each.hpp>
#include <glaze/util/variant.hpp>
#include <glaze/json/generic_fwd.hpp>

#include "tags.hpp"
#include "opts.hpp"

namespace glz
{
   namespace etf::detail
   {
      template <class T>
      consteval size_t min_etf_val_size();

      template <class V, size_t I>
      consteval size_t min_field_etf_size()
      {
         if constexpr (skipped_by_meta<V, I, operation::serialize> || meta_has_skip_if<V>) {
            return 0;
         }
         else {
            using field_val_t = std::decay_t<field_t<V, I>>;
            if constexpr (never_written<etf_opts{}, field_val_t> || null_t<field_val_t> ||
                          nullable_like<field_val_t> || nullable_value_t<field_val_t> ||
                          is_specialization_v<field_val_t, custom_t> ||
                          glaze_value_is_nullable<field_val_t>()) { // NOLINT(readability-trailing-comma)
               return 0;
            }
            else {
               constexpr auto key = glz::get<I>(reflect<V>::keys);
               return 5 + key.size() + min_etf_val_size<field_val_t>();
            }
         }
      }

      template <class T>
      consteval size_t min_etf_struct_size()
      {
         using V = std::decay_t<T>;
         if constexpr ((glaze_object_t<V> || reflectable<V>) && !custom_write<V>) {
            constexpr auto N = reflect<V>::size;
            if constexpr (N == 0) {
               return 5;
            }
            else {
               return [&]<size_t... I>(std::index_sequence<I...>) {
                  return (5 + ... + min_field_etf_size<V, I>());
               }(std::make_index_sequence<N>{});
            }
         }
         else {
            return 5;
         }
      }

      template <class T>
      consteval size_t min_etf_val_size()
      {
         using V = std::decay_t<T>;
         if constexpr (custom_write<V> || has_custom_meta_v<V>) {
            return 1;
         }
         else if constexpr (std::is_void_v<V>) {
            return 0;
         }
         else if constexpr (std::is_pointer_v<V>) {
            return 1;
         }
         else if constexpr (std::same_as<V, std::nullptr_t> || always_null_t<V>) {
            return 5; // \x77\x03nil
         }
         else if constexpr (boolean_like<V>) {
            return 7; // \x77\x05false (or 6 for true)
         }
         else if constexpr (num_t<V>) {
            if constexpr (std::floating_point<V>) {
               return 9; // NEW_FLOAT_EXT + 8 bytes
            }
            else {
               return 2; // SMALL_INTEGER_EXT + 1 byte
            }
         }
         else if constexpr (is_system_time_point<V>) {
            return 25; // BINARY_EXT + 4-byte len + ISO string min ~20 chars
         }
         else if constexpr (is_year_month_day<V>) {
            return 15; // BINARY_EXT + 4-byte len + 10 chars
         }
         else if constexpr (str_t<V>) {
            return 5; // BINARY_EXT + 4-byte len (empty string)
         }
         else if constexpr (contiguous_byte_range<V>) {
            return 5; // BINARY_EXT + 4-byte len (empty binary)
         }
         else if constexpr (nullable_like<V> || nullable_value_t<V>) {
            return 5; // \x77\x03nil
         }
         else if constexpr (readable_array_t<V>) {
            return 1; // NIL_EXT
         }
         else if constexpr (readable_map_t<V>) {
            return 5; // MAP_EXT + 4-byte arity 0
         }
         else if constexpr (std::is_enum_v<V>) {
            return 5; // BINARY_EXT or INTEGER_EXT
         }
         else if constexpr (is_opts_wrapper<V>) {
            return min_etf_val_size<typename V::value_type>();
         }
         else if constexpr (glaze_value_t<V>) {
            using M = std::decay_t<decltype(get_member(std::declval<V&>(), meta_wrapper_v<V>))>;
            return min_etf_val_size<M>();
         }
         else if constexpr ((glaze_object_t<V> || reflectable<V>) && !custom_write<V>) {
            return min_etf_struct_size<V>();
         }
         else {
            return 1;
         }
      }

      template <class T>
      concept is_pointer_like = std::is_pointer_v<std::decay_t<T>> ||
                                is_specialization_v<std::decay_t<T>, std::unique_ptr> ||
                                is_specialization_v<std::decay_t<T>, std::shared_ptr> ||
                                is_specialization_v<std::decay_t<T>, std::weak_ptr> ||
                                requires { typename std::decay_t<T>::element_type; };

      template <class T>
      inline constexpr bool is_indirect_v = [] {
         using V = std::decay_t<T>;
         if constexpr (is_pointer_like<V>) {
            return true;
         }
         else if constexpr (nullable_like<V>) {
            using inner_t = std::decay_t<decltype(*std::declval<V>())>;
            return is_pointer_like<inner_t>;
         }
         else if constexpr (nullable_value_t<V>) {
            using inner_t = std::decay_t<decltype(std::declval<V>().value())>;
            return is_pointer_like<inner_t>;
         }
         else {
            return false;
         }
      }();

      template <class T>
      constexpr size_t max_etf_val_size();

      template <class T>
      constexpr size_t max_scalar_etf_val_size()
      {
         using V = std::decay_t<T>;
         if constexpr (custom_write<V> || has_custom_meta_v<V>) {
            return 0;
         }
         else if constexpr (std::is_void_v<V> || is_indirect_v<V>) {
            return 0;
         }
         else if constexpr (std::same_as<V, std::nullptr_t> || always_null_t<V>) {
            return 5; // \x77\x03nil
         }
         else if constexpr (boolean_like<V>) {
            return 7; // \x77\x05false (or 6 for true)
         }
         else if constexpr (num_t<V>) {
            if constexpr (std::floating_point<V>) {
               return 9; // NEW_FLOAT_EXT + 8 bytes
            }
            else if constexpr (sizeof(V) <= 1) {
               return std::is_signed_v<V> ? 5 : 2; // negative int8_t uses INTEGER_EXT (5 bytes)
            }
            else if constexpr (sizeof(V) <= 2) {
               return 5; // INTEGER_EXT + 4 bytes
            }
            else if constexpr (sizeof(V) <= 4) {
               if constexpr (std::is_signed_v<V>) {
                  return 5; // INTEGER_EXT + 4 bytes
               }
               else {
                  return 7; // SMALL_BIG_EXT: tag(1) + len(1) + sign(1) + 4 bytes = 7 bytes
               }
            }
            else {
               return 11; // SMALL_BIG_EXT: tag(1) + len(1) + sign(1) + 8 = 11 bytes
            }
         }
         else if constexpr (std::is_enum_v<V>) {
            using U = std::underlying_type_t<V>;
            constexpr size_t num_max = max_scalar_etf_val_size<U>();
            if constexpr (glaze_enum_t<V> || meta_keys<V>) {
               constexpr size_t str_max = [] {
                  size_t m = 0;
                  for (const auto& k : reflect<V>::keys) {
                     m = (std::max)(m, size_t{5} + k.size());
                  }
                  return m;
               }();
               return (std::max)(num_max, str_max);
            }
            else {
               return num_max;
            }
         }
         else if constexpr (is_opts_wrapper<V>) {
            return max_scalar_etf_val_size<typename V::value_type>();
         }
         else if constexpr (glaze_value_t<V>) {
            using M = std::decay_t<decltype(get_member(std::declval<V&>(), meta_wrapper_v<V>))>;
            return max_scalar_etf_val_size<M>();
         }
         else if constexpr (nullable_like<V>) {
            using inner_t = std::decay_t<decltype(*std::declval<V>())>;
            if constexpr (is_indirect_v<inner_t>) {
               return 0;
            }
            else {
               constexpr size_t inner_max = max_etf_val_size<inner_t>();
               if constexpr (inner_max > 0) {
                  return (std::max)(size_t{5}, inner_max);
               }
               else {
                  return 0;
               }
            }
         }
         else if constexpr (nullable_value_t<V>) {
            using inner_t = std::decay_t<decltype(std::declval<V>().value())>;
            if constexpr (is_indirect_v<inner_t>) {
               return 0;
            }
            else {
               constexpr size_t inner_max = max_etf_val_size<inner_t>();
               if constexpr (inner_max > 0) {
                  return (std::max)(size_t{5}, inner_max);
               }
               else {
                  return 0;
               }
            }
         }
         else if constexpr (is_system_time_point<V>) {
            using Period = V::duration::period;
            return 5 + chrono_detail::iso_time_point_max_size<Period>;
         }
         else if constexpr (is_year_month_day<V>) {
            return 5 + chrono_detail::iso_date_max_size;
         }
         else if constexpr (is_variant<V>) {
            size_t max_alt = 0;
            bool all_bounded = true;
            for_each_short_circuit<std::variant_size_v<V>>([&]<auto I>() -> bool {
               using alt_t = std::decay_t<std::variant_alternative_t<I, V>>;
               if constexpr (std::same_as<alt_t, skip>) {
                  return false;
               }
               else if constexpr (is_indirect_v<alt_t>) {
                  all_bounded = false;
                  return true;
               }
               else {
                  constexpr size_t alt_max = max_etf_val_size<alt_t>();
                  if constexpr (alt_max > 0) {
                     max_alt = (std::max)(max_alt, alt_max);
                     return false;
                  }
                  else {
                     all_bounded = false;
                     return true;
                  }
               }
            });
            return all_bounded ? max_alt : 0;
         }
         else {
            return 0;
         }
      }

      template <class T>
      inline constexpr bool is_etf_str_like = [] {
         using V = std::decay_t<T>;
         if constexpr (custom_write<V> || has_custom_meta_v<V>) {
            return false;
         }
         else if constexpr (str_t<V>) {
            return true;
         }
         else if constexpr (glaze_value_t<V>) {
            using M = std::decay_t<decltype(get_member(std::declval<V&>(), meta_wrapper_v<V>))>;
            return str_t<M> && !custom_write<M>;
         }
         else {
            return false;
         }
      }();

      template <class T>
      GLZ_ALWAYS_INLINE std::string_view get_etf_str_view(T&& val) noexcept
      {
         using V = std::decay_t<T>;
         if constexpr (str_t<V>) {
            return str_view<V>(val);
         }
         else {
            decltype(auto) m = get_member(val, meta_wrapper_v<V>);
            using M = std::decay_t<decltype(m)>;
            return str_view<M>(m);
         }
      }

      template <class T>
      inline constexpr size_t fixed_etf_padding = [] {
         using V = std::decay_t<T>;
         if constexpr ((glaze_object_t<V> || reflectable<V>) && !custom_write<V>) {
            constexpr auto N = reflect<V>::size;
            if constexpr (N == 0) {
               return size_t{5}; // MAP_EXT + 0 arity
            }
            else {
               size_t fixed = 5;
               bool all_bounded = true;
               for_each_short_circuit<N>([&]<auto I>() -> bool {
                  if constexpr (skipped_by_meta<V, I, operation::serialize>) {
                     return false; // skip
                  }
                  else {
                     using field_val_t = std::decay_t<field_t<V, I>>;
                     if constexpr (always_skipped<field_val_t>) {
                        return false; // skip
                     }
                     else if constexpr (is_indirect_v<field_val_t>) {
                        all_bounded = false;
                        return true; // abort
                     }
                     else {
                        constexpr auto key = glz::get<I>(reflect<V>::keys);
                        constexpr size_t val_max = max_etf_val_size<field_val_t>();
                        if constexpr (val_max > 0) {
                           fixed += 5 + key.size() + val_max;
                           return false; // continue
                        }
                        else {
                           all_bounded = false;
                           return true; // abort
                        }
                     }
                  }
               });
               return all_bounded ? fixed : size_t{0};
            }
         }
         else {
            return size_t{0};
         }
      }();

      template <class T>
      constexpr size_t max_etf_val_size()
      {
         using V = std::decay_t<T>;
         constexpr size_t scalar_max = max_scalar_etf_val_size<V>();
         if constexpr (scalar_max > 0) {
            return scalar_max;
         }
         else if constexpr ((glaze_object_t<V> || reflectable<V>) && !custom_write<V>) {
            return fixed_etf_padding<V>;
         }
         else {
            return 0;
         }
      }

      template <auto Opts, class T, size_t Start>
      consteval size_t bounded_run_size()
      {
         using V = std::decay_t<T>;
         if constexpr ((glaze_object_t<V> || reflectable<V>) && !custom_write<V>) {
            constexpr auto N = reflect<V>::size;
            if constexpr (Start >= N) {
               return 0;
            }
            else {
               size_t total = 0;
               for_each_short_circuit<N - Start>([&]<auto J>() -> bool {
                  constexpr size_t I = Start + J;
                  if constexpr (skipped_on_write<Opts, V, I>) {
                     return false; // skip field
                  }
                  else {
                     using field_val_t = std::decay_t<field_t<V, I>>;
                     if constexpr (is_indirect_v<field_val_t>) {
                        return true; // abort run on unbounded field
                     }
                     else {
                        constexpr size_t val_max = max_etf_val_size<field_val_t>();
                        if constexpr (val_max > 0) {
                           constexpr auto key = glz::get<I>(reflect<V>::keys);
                           total += 5 + key.size() + val_max;
                           return false; // continue run
                        }
                        else {
                           return true; // abort run on unbounded field
                        }
                     }
                  }
               });
               return total;
            }
         }
         else {
            return 0;
         }
      }

      template <class T>
      inline constexpr size_t min_etf_size = [] {
         using V = std::decay_t<T>;
         if constexpr (std::is_void_v<V>) {
            return 1;
         }
         else {
            if constexpr (fixed_etf_padding<V> > 0) {
               return 1 + fixed_etf_padding<V>;
            }
            else {
               return 1 + min_etf_val_size<V>(); // 1 for ETF magic_version
            }
         }
      }();

      template <class Derived, class T>
      struct to_etf_base // NOLINT(bugprone-crtp-constructor-accessibility)
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& b, auto& ix)
         {
            if constexpr (!check_no_header(Opts)) {
               constexpr size_t prealloc = etf::detail::min_etf_size<std::decay_t<T>>;
               etf::detail::ensure_space(b, ix + prealloc);
               etf::detail::dump_byte_unchecked(etf::magic_version, b, ix);
               constexpr auto next_opts = [] {
                  if constexpr (etf::detail::fixed_etf_padding<std::decay_t<T>> > 0) {
                     return write_unchecked_on<no_header_on<Opts>()>();
                  }
                  else {
                     return no_header_on<Opts>();
                  }
               }();
               Derived::template no_header<next_opts>(
                  std::forward<decltype(value)>(value), ctx, b, ix);
            }
            else {
               Derived::template no_header<Opts>(
                  std::forward<decltype(value)>(value), ctx, b, ix);
            }
         }
      };
   } // namespace etf::detail

   template <>
   struct serialize<EETF>
   {
      template <auto Opts, class T, is_context Ctx, class B, class IX>
      GLZ_ALWAYS_INLINE static void op(T&& value, Ctx&& ctx, B&& b, IX&& ix)
      {
         to<EETF, std::remove_cvref_t<T>>::template op<Opts>(
            std::forward<T>(value), std::forward<Ctx>(ctx), std::forward<B>(b), ix);
      }

      template <auto Opts, class T, is_context Ctx, class B, class IX>
      GLZ_ALWAYS_INLINE static void no_header(T&& value, Ctx&& ctx, B&& b, IX&& ix)
      {
         to<EETF, std::remove_cvref_t<T>>::template no_header<Opts>(
            std::forward<T>(value), std::forward<Ctx>(ctx), std::forward<B>(b), ix);
      }
   };

   // Null (atom nil)
   template <always_null_t T>
   struct to<EETF, T> final : etf::detail::to_etf_base<to<EETF, T>, T>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void no_header(auto&&, is_context auto&, auto& b, auto& ix) noexcept
      {
         if constexpr (check_write_unchecked(Opts)) {
            etf::detail::dump_bytes_unchecked<5>("\x77\x03" "nil", b, ix);
         }
         else {
            etf::detail::dump_bytes("\x77\x03" "nil", 5, b, ix);
         }
      }
   };

   // Skip type (no-op)
   template <>
   struct to<EETF, skip> final : etf::detail::to_etf_base<to<EETF, skip>, skip>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void no_header(auto&&, is_context auto&&, auto&&, auto&&) noexcept
      {}
   };

   // Booleans (atom true / false)
   template <boolean_like T>
      requires(!custom_write<T>)
   struct to<EETF, T> final : etf::detail::to_etf_base<to<EETF, T>, T>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void no_header(const bool value, is_context auto&, auto& b, auto& ix) noexcept
      {
         if constexpr (check_write_unchecked(Opts)) {
            if (value) {
               etf::detail::dump_bytes_unchecked<6>("\x77\x04" "true", b, ix);
            }
            else {
               etf::detail::dump_bytes_unchecked<7>("\x77\x05" "false", b, ix);
            }
         }
         else {
            if (value) {
               etf::detail::dump_bytes("\x77\x04" "true", 6, b, ix);
            }
            else {
               etf::detail::dump_bytes("\x77\x05" "false", 7, b, ix);
            }
         }
      }
   };

   // Numbers (integers, floats, bignums; ignores quoted_num in binary ETF)
   template <num_t T>
      requires(!custom_write<T>)
   struct to<EETF, T> final : etf::detail::to_etf_base<to<EETF, T>, T>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void no_header(auto&& value, [[maybe_unused]] is_context auto& ctx, auto& b, auto& ix) noexcept
      {
         if constexpr (std::floating_point<std::decay_t<T>>) {
            const auto d = static_cast<double>(value);
            uint64_t bits = 0;
            std::memcpy(&bits, &d, sizeof(double));
            if constexpr (check_write_unchecked(Opts)) {
               etf::detail::dump_tag_be_unchecked<uint64_t>(etf::tag::NEW_FLOAT_EXT, bits, b, ix);
            }
            else {
               etf::detail::dump_tag_be<uint64_t>(etf::tag::NEW_FLOAT_EXT, bits, b, ix);
            }
         }
         else if constexpr (std::integral<std::decay_t<T>>) {
            using U = std::decay_t<T>;
            if constexpr (std::is_unsigned_v<U>) {
               if constexpr (sizeof(U) == 8) {
                  if (value >= (1ULL << 56)) [[likely]] { // 8-byte Snowflake fast path (no countl_zero needed)
                     if constexpr (!check_write_unchecked(Opts)) {
                        etf::detail::ensure_space(b, ix + 11);
                     }
                     b[ix] = static_cast<std::decay_t<decltype(b)>::value_type>(etf::tag::SMALL_BIG_EXT);
                     b[ix + 1] = static_cast<std::decay_t<decltype(b)>::value_type>(8);
                     b[ix + 2] = static_cast<std::decay_t<decltype(b)>::value_type>(0);
                     uint64_t raw = value;
                     if constexpr (std::endian::native == std::endian::big) {
                        raw = std::byteswap(raw);
                     }
                     std::memcpy(&b[ix + 3], &raw, 8);
                     ix += 11;
                     return;
                  }
               }
               if (value <= 255) {
                  if constexpr (check_write_unchecked(Opts)) {
                     etf::detail::dump_tag_be_unchecked<uint8_t>(etf::tag::SMALL_INTEGER_EXT, static_cast<uint8_t>(value), b, ix);
                  }
                  else {
                     etf::detail::dump_tag_be<uint8_t>(etf::tag::SMALL_INTEGER_EXT, static_cast<uint8_t>(value), b, ix);
                  }
               }
               else if (value <= 2147483647ULL) {
                  if constexpr (check_write_unchecked(Opts)) {
                     etf::detail::dump_tag_be_unchecked<int32_t>(etf::tag::INTEGER_EXT, static_cast<int32_t>(value), b, ix);
                  }
                  else {
                     etf::detail::dump_tag_be<int32_t>(etf::tag::INTEGER_EXT, static_cast<int32_t>(value), b, ix);
                  }
               }
               else if constexpr (sizeof(U) <= 4) {
                  // uint32_t > 2147483647 always has bit 31 set -> n is always 4 (7 bytes total)
                  if constexpr (!check_write_unchecked(Opts)) {
                     etf::detail::ensure_space(b, ix + 7);
                  }
                  b[ix] = static_cast<std::decay_t<decltype(b)>::value_type>(etf::tag::SMALL_BIG_EXT);
                  b[ix + 1] = static_cast<std::decay_t<decltype(b)>::value_type>(4);
                  b[ix + 2] = static_cast<std::decay_t<decltype(b)>::value_type>(0);
                  auto raw = static_cast<uint32_t>(value);
                  if constexpr (std::endian::native == std::endian::big) {
                     raw = std::byteswap(raw);
                  }
                  std::memcpy(&b[ix + 3], &raw, 4);
                  ix += 7;
               }
               else {
                  // 64-bit integer (e.g. Snowflake) -> SMALL_BIG_EXT (stripped little-endian bytes)
                  const auto uval = static_cast<uint64_t>(value);
                  const auto n = static_cast<uint8_t>(8 - (std::countl_zero(uval) >> 3));
                  if constexpr (!check_write_unchecked(Opts)) {
                     etf::detail::ensure_space(b, ix + 11);
                  }
                  b[ix] = static_cast<std::decay_t<decltype(b)>::value_type>(etf::tag::SMALL_BIG_EXT);
                  b[ix + 1] = static_cast<std::decay_t<decltype(b)>::value_type>(n);
                  b[ix + 2] = static_cast<std::decay_t<decltype(b)>::value_type>(0); // positive sign = 0
                  auto raw = uval;
                  if constexpr (std::endian::native == std::endian::big) {
                     raw = std::byteswap(raw);
                  }
                  std::memcpy(&b[ix + 3], &raw, n);
                  ix += 3 + n;
               }
            }
            else { // signed integer
               if (value >= 0 && value <= 255) {
                  if constexpr (check_write_unchecked(Opts)) {
                     etf::detail::dump_tag_be_unchecked<uint8_t>(etf::tag::SMALL_INTEGER_EXT, static_cast<uint8_t>(value), b, ix);
                  }
                  else {
                     etf::detail::dump_tag_be<uint8_t>(etf::tag::SMALL_INTEGER_EXT, static_cast<uint8_t>(value), b, ix);
                  }
               }
               else if (value >= -2147483648LL && value <= 2147483647LL) {
                  if constexpr (check_write_unchecked(Opts)) {
                     etf::detail::dump_tag_be_unchecked<int32_t>(etf::tag::INTEGER_EXT, static_cast<int32_t>(value), b, ix);
                  }
                  else {
                     etf::detail::dump_tag_be<int32_t>(etf::tag::INTEGER_EXT, static_cast<int32_t>(value), b, ix);
                  }
               }
               else {
                  // 64-bit signed integer -> SMALL_BIG_EXT (stripped little-endian bytes)
                  const bool is_negative = value < 0;
                  const uint64_t uval = is_negative ? (0ULL - static_cast<uint64_t>(value)) : static_cast<uint64_t>(value);
                  const auto n = static_cast<uint8_t>(8 - (std::countl_zero(uval) >> 3));
                  if constexpr (!check_write_unchecked(Opts)) {
                     etf::detail::ensure_space(b, ix + 11);
                  }
                  b[ix] = static_cast<std::decay_t<decltype(b)>::value_type>(etf::tag::SMALL_BIG_EXT);
                  b[ix + 1] = static_cast<std::decay_t<decltype(b)>::value_type>(n);
                  b[ix + 2] = static_cast<std::decay_t<decltype(b)>::value_type>(is_negative ? 1 : 0);
                  auto raw = uval;
                  if constexpr (std::endian::native == std::endian::big) {
                     raw = std::byteswap(raw);
                  }
                  std::memcpy(&b[ix + 3], &raw, n);
                  ix += 3 + n;
               }
            }
         }
      }
   };

   // Strings (BINARY_EXT)
   template <str_t T>
      requires(!custom_write<T>)
   struct to<EETF, T> final : etf::detail::to_etf_base<to<EETF, T>, T>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void no_header(auto&& value, is_context auto&, auto& b, auto& ix) noexcept
      {
         const auto sv = str_view<T>(value);
         const auto len = static_cast<uint32_t>(sv.size());
         if constexpr (check_write_unchecked(Opts)) {
            etf::detail::dump_binary_unchecked(sv.data(), len, b, ix);
         }
         else {
            etf::detail::dump_binary(sv.data(), len, b, ix);
         }
      }
   };

   // Byte sequences (BINARY_EXT)
   template <class T>
      requires(contiguous_byte_range<std::remove_cvref_t<T>> && !str_t<T>)
   struct to<EETF, T> final : etf::detail::to_etf_base<to<EETF, T>, T>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void no_header(auto&& value, is_context auto&, auto& b, auto& ix) noexcept
      {
         const auto len = static_cast<uint32_t>(value.size());
         if constexpr (check_write_unchecked(Opts)) {
            etf::detail::dump_binary_unchecked(value.data(), len, b, ix);
         }
         else {
            etf::detail::dump_binary(value.data(), len, b, ix);
         }
      }
   };

   // Enums (reflected names as BINARY_EXT or underlying numbers)
   template <class T>
      requires(std::is_enum_v<T> && !custom_write<T>)
   struct to<EETF, T> final : etf::detail::to_etf_base<to<EETF, T>, T>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void no_header(auto&& value, is_context auto&& ctx, auto&& b, auto& ix)
      {
         if constexpr (glaze_enum_t<T> || (meta_keys<T> && std::is_enum_v<std::decay_t<T>>)) {
            const std::string_view str = get_enum_name(value);
            if (!str.empty()) {
               to<EETF, std::string_view>::template no_header<Opts>(str, ctx, b, ix);
               return;
            }
         }
         using V = std::underlying_type_t<std::decay_t<T>>;
         to<EETF, V>::template no_header<Opts>(static_cast<V>(value), ctx, b, ix);
      }
   };

   // Nullable types (std::optional, std::unique_ptr, std::shared_ptr, nullable_value_t)
   template <class T>
      requires(nullable_like<T> || nullable_value_t<T>)
   struct to<EETF, T> final : etf::detail::to_etf_base<to<EETF, T>, T>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void no_header(auto&& value, is_context auto&& ctx, auto&& b, auto& ix)
      {
         const bool has_val = [&] {
            if constexpr (nullable_like<std::decay_t<T>>) {
               return bool(value);
            }
            else {
               return value.has_value();
            }
         }();
         if (has_val) {
            decltype(auto) val = [&]() -> decltype(auto) {
               if constexpr (nullable_like<std::decay_t<T>>) {
                  return *value;
               }
               else {
                  return value.value();
               }
            }();
            to<EETF, std::decay_t<decltype(val)>>::template no_header<Opts>(val, ctx, b, ix);
         }
         else {
            to<EETF, std::nullptr_t>::template no_header<Opts>(nullptr, ctx, b, ix);
         }
      }
   };

   // Arrays (std::vector, std::deque, std::span, etc.)
   template <readable_array_t T>
      requires(!contiguous_byte_range<std::remove_cvref_t<T>>)
   struct to<EETF, T> final : etf::detail::to_etf_base<to<EETF, T>, T>
   {
      template <auto Opts>
      static void no_header(auto&& value, is_context auto&& ctx, auto&& b, auto& ix)
      {
         const size_t sz = value.size();
         if (sz == 0) {
            if constexpr (check_write_unchecked(Opts)) {
               etf::detail::dump_byte_unchecked(etf::tag::NIL_EXT, b, ix);
            }
            else {
               etf::detail::dump_byte(etf::tag::NIL_EXT, b, ix);
            }
            return;
         }

         using item_t = std::decay_t<range_value_t<T>>;
         constexpr size_t max_item_sz = etf::detail::max_etf_val_size<item_t>();
         if constexpr (max_item_sz > 0) {
            etf::detail::ensure_space(b, ix + 6 + (sz * max_item_sz));
            etf::detail::dump_tag_be_unchecked<uint32_t>(etf::tag::LIST_EXT, static_cast<uint32_t>(sz), b, ix);
            constexpr auto item_opts = write_unchecked_on<Opts>();
            for (auto&& item : value) {
               to<EETF, item_t>::template no_header<item_opts>(item, ctx, b, ix);
            }
            etf::detail::dump_byte_unchecked(etf::tag::NIL_EXT, b, ix);
         }
         else {
            constexpr size_t item_alloc = etf::detail::min_etf_val_size<item_t>();
            etf::detail::ensure_space(b, ix + 6 + (sz * item_alloc));
            etf::detail::dump_tag_be_unchecked<uint32_t>(etf::tag::LIST_EXT, static_cast<uint32_t>(sz), b, ix);
            for (auto&& item : value) {
               to<EETF, item_t>::template no_header<Opts>(item, ctx, b, ix);
            }
            etf::detail::dump_byte(etf::tag::NIL_EXT, b, ix);
         }
      }
   };

   // Tuples
   template <class T>
      requires(tuple_t<T> || is_std_tuple<T>)
   struct to<EETF, T> final : etf::detail::to_etf_base<to<EETF, T>, T>
   {
      template <auto Opts>
      static void no_header(auto&& value, is_context auto&& ctx, auto&& b, auto& ix)
      {
         using V = std::decay_t<T>;
         static constexpr auto N = glz::tuple_size_v<V>;
         if constexpr (N <= 255) {
            if constexpr (!check_write_unchecked(Opts)) {
               etf::detail::ensure_space(b, ix + 2);
            }
            etf::detail::dump_byte_unchecked(etf::tag::SMALL_TUPLE_EXT, b, ix);
            etf::detail::dump_byte_unchecked(static_cast<uint8_t>(N), b, ix);
         }
         else {
            if constexpr (!check_write_unchecked(Opts)) {
               etf::detail::ensure_space(b, ix + 5);
            }
            etf::detail::dump_byte_unchecked(etf::tag::LARGE_TUPLE_EXT, b, ix);
            etf::detail::dump_be_unchecked<uint32_t>(static_cast<uint32_t>(N), b, ix);
         }
         if constexpr (is_std_tuple<V>) {
            for_each<N>([&]<size_t I>() {
               using item_t = std::decay_t<std::tuple_element_t<I, V>>;
               to<EETF, item_t>::template no_header<Opts>(std::get<I>(value), ctx, b, ix);
            });
         }
         else {
            for_each<N>([&]<size_t I>() {
               using item_t = std::decay_t<glz::tuple_element_t<I, V>>;
               to<EETF, item_t>::template no_header<Opts>(glz::get<I>(value), ctx, b, ix);
            });
         }
      }
   };

   // Maps
   template <readable_map_t T>
      requires(!custom_write<T>)
   struct to<EETF, T> final : etf::detail::to_etf_base<to<EETF, T>, T>
   {
      template <auto Opts>
      static void no_header(auto&& value, is_context auto&& ctx, auto&& b, auto& ix)
      {
         const size_t sz = value.size();
         if (sz == 0) {
            if constexpr (check_write_unchecked(Opts)) {
               etf::detail::dump_tag_be_unchecked<uint32_t>(etf::tag::MAP_EXT, 0, b, ix);
            }
            else {
               etf::detail::dump_tag_be<uint32_t>(etf::tag::MAP_EXT, 0, b, ix);
            }
            return;
         }

         using key_t = std::decay_t<typename std::decay_t<T>::key_type>;
         using mapped_t = std::decay_t<typename std::decay_t<T>::mapped_type>;
         constexpr size_t pair_alloc = (std::max)(sizeof(key_t), etf::detail::min_etf_val_size<key_t>()) +
                                       (std::max)(sizeof(mapped_t), etf::detail::min_etf_val_size<mapped_t>());
         etf::detail::ensure_space(b, ix + 5 + (sz * pair_alloc));

         etf::detail::dump_tag_be_unchecked<uint32_t>(etf::tag::MAP_EXT, static_cast<uint32_t>(sz), b, ix);
         for (auto&& [k, v] : value) {
            to<EETF, key_t>::template no_header<Opts>(k, ctx, b, ix);
            to<EETF, mapped_t>::template no_header<Opts>(v, ctx, b, ix);
         }
      }
   };

   // Glaze objects & reflectable structs with STRING KEYS ONLY (BINARY_EXT)
   template <class T>
      requires((glaze_object_t<T> || reflectable<T>) && !custom_write<T>)
   struct to<EETF, T> final : etf::detail::to_etf_base<to<EETF, T>, T>
   {
      static constexpr auto N = reflect<T>::size;

      template <auto Opts, size_t I>
      static consteval bool should_skip_field()
      {
         return skipped_on_write<Opts, T, I>;
      }

      template <auto Opts>
      static consteval size_t count_to_write()
      {
         return []<size_t... I>(std::index_sequence<I...>) {
            return (size_t{} + ... + (should_skip_field<Opts, I>() ? size_t{} : size_t{1}));
         }(std::make_index_sequence<N>{});
      }

      template <auto Opts>
      static void no_header(auto&& value, is_context auto&& ctx, auto&& b, auto& ix)
      {
         static constexpr size_t fixed_max_size = etf::detail::fixed_etf_padding<std::decay_t<T>>;
         if constexpr (fixed_max_size > 0) {
            if constexpr (!check_write_unchecked(Opts)) {
               etf::detail::ensure_space(b, ix + fixed_max_size);
            }
         }
         else {
            constexpr size_t initial_run = 5 + etf::detail::bounded_run_size<Opts, T, 0>();
            constexpr size_t struct_min = etf::detail::min_etf_struct_size<std::decay_t<T>>();
            if constexpr (!check_write_unchecked(Opts)) {
               etf::detail::ensure_space(b, ix + (std::max)(initial_run, struct_min));
            }
         }

         [[maybe_unused]] decltype(auto) t = [&]() -> decltype(auto) {
            if constexpr (reflectable<T>) {
               return to_tie(value);
            }
            else {
               return nullptr;
            }
         }();

         if constexpr (maybe_skipped<Opts, T>) {
            // Dynamic path: back-patching arity for runtime-skippable fields
            const size_t count_pos = ix + 1;
            etf::detail::dump_tag_be_unchecked<uint32_t>(etf::tag::MAP_EXT, 0, b, ix); // placeholder for arity

            uint32_t pair_count = 0;

            for_each<N>([&]<size_t I>() {
               if constexpr (should_skip_field<Opts, I>()) {
                  return;
               }

               decltype(auto) member_val = [&]() -> decltype(auto) {
                  if constexpr (reflectable<T>) {
                     return get_member(value, get<I>(t));
                  }
                  else {
                     return get_member(value, get<I>(reflect<T>::values));
                  }
               }();
               using val_t = std::decay_t<decltype(member_val)>;

               static constexpr auto key = reflect<T>::keys[I];
               constexpr size_t val_max = etf::detail::is_indirect_v<val_t> ? 0 : etf::detail::max_etf_val_size<val_t>();

               auto on_skipped = [&] {
                  if constexpr (not fixed_max_size && val_max == 0) {
                     constexpr size_t next_run = etf::detail::bounded_run_size<Opts, T, I + 1>();
                     if constexpr (next_run > 0) {
                        etf::detail::ensure_space(b, ix + next_run);
                     }
                  }
               };

               if constexpr (meta_has_skip_if<T>) {
                  static constexpr auto skip_if_key = glz::get<I>(reflect<T>::keys);
                  static constexpr meta_context mctx{.op = operation::serialize};
                  if (meta<T>::skip_if(member_val, skip_if_key, mctx)) {
                     on_skipped();
                     return;
                  }
               }

               if constexpr (Opts.skip_null_members) {
                  if constexpr (always_null_t<val_t>) {
                     on_skipped();
                     return;
                  }
                  else if constexpr (null_t<val_t> || nullable_like<val_t>) {
                     if constexpr (nullable_wrapper<val_t>) {
                        if (!bool(member_val.val)) {
                           on_skipped();
                           return;
                        }
                     }
                     else if constexpr (nullable_value_t<val_t>) {
                        if (!member_val.has_value()) {
                           on_skipped();
                           return;
                        }
                     }
                     else if constexpr (requires { bool(member_val); }) {
                        if (!bool(member_val)) {
                           on_skipped();
                           return;
                        }
                     }
                  }
                  else if constexpr (is_specialization_v<val_t, custom_t> && custom_getter_returns_nullable<val_t>()) {
                     if (is_custom_field_null<T, I>(value, t, ctx)) {
                        on_skipped();
                        return;
                     }
                  }
                  else if constexpr (glaze_value_is_nullable<val_t>()) {
                     if (is_glaze_value_field_null<T, I>(value, t)) {
                        on_skipped();
                        return;
                     }
                  }
               }

               if constexpr (etf::detail::is_etf_str_like<val_t>) {
                  const auto sv = etf::detail::get_etf_str_view(member_val);
                  const auto len = static_cast<uint32_t>(sv.size());
                  if constexpr (not fixed_max_size) {
                     constexpr size_t next_run = etf::detail::bounded_run_size<Opts, T, I + 1>();
                     etf::detail::ensure_space(b, ix + 5 + key.size() + 5 + len + next_run);
                  }
                  static constexpr etf::detail::static_etf_key<key.size()> formatted_key{key};
                  etf::detail::dump_bytes_unchecked<formatted_key.buf.size()>(formatted_key.buf.data(), b, ix);
                  etf::detail::dump_binary_unchecked(sv.data(), len, b, ix);
               }
               else if constexpr (Opts.skip_null_members && !nullable_wrapper<val_t> &&
                                  (nullable_like<val_t> || nullable_value_t<val_t>)) {
                  decltype(auto) inner_val = [&]() -> decltype(auto) {
                     if constexpr (nullable_like<val_t>) {
                        return *member_val;
                     }
                     else {
                        return member_val.value();
                     }
                  }();
                  using inner_t = std::decay_t<decltype(inner_val)>;
                  if constexpr (etf::detail::is_etf_str_like<inner_t>) {
                     const auto sv = etf::detail::get_etf_str_view(inner_val);
                     const auto len = static_cast<uint32_t>(sv.size());
                     if constexpr (not fixed_max_size) {
                        constexpr size_t next_run = etf::detail::bounded_run_size<Opts, T, I + 1>();
                        etf::detail::ensure_space(b, ix + 5 + key.size() + 5 + len + next_run);
                     }
                     static constexpr etf::detail::static_etf_key<key.size()> formatted_key{key};
                     etf::detail::dump_bytes_unchecked<formatted_key.buf.size()>(formatted_key.buf.data(), b, ix);
                     etf::detail::dump_binary_unchecked(sv.data(), len, b, ix);
                  }
                  else {
                     if constexpr (not fixed_max_size) {
                        if constexpr (val_max == 0) {
                           etf::detail::ensure_space(b, ix + 5 + key.size());
                        }
                     }

                     static constexpr etf::detail::static_etf_key<key.size()> formatted_key{key};
                     etf::detail::dump_bytes_unchecked<formatted_key.buf.size()>(formatted_key.buf.data(), b, ix);

                     if constexpr (val_max > 0) {
                        to<EETF, inner_t>::template no_header<write_unchecked_on<Opts>()>(inner_val, ctx, b, ix);
                     }
                     else {
                        to<EETF, inner_t>::template no_header<Opts>(inner_val, ctx, b, ix);
                        if constexpr (not fixed_max_size) {
                           constexpr size_t next_run = etf::detail::bounded_run_size<Opts, T, I + 1>();
                           if constexpr (next_run > 0) {
                              etf::detail::ensure_space(b, ix + next_run);
                           }
                        }
                     }
                  }
               }
               else {
                  if constexpr (not fixed_max_size) {
                     if constexpr (val_max == 0) {
                        etf::detail::ensure_space(b, ix + 5 + key.size());
                     }
                  }

                  static constexpr etf::detail::static_etf_key<key.size()> formatted_key{key};
                  etf::detail::dump_bytes_unchecked<formatted_key.buf.size()>(formatted_key.buf.data(), b, ix);

                  if constexpr (val_max > 0) {
                     to<EETF, val_t>::template no_header<write_unchecked_on<Opts>()>(member_val, ctx, b, ix);
                  }
                  else {
                     to<EETF, val_t>::template no_header<Opts>(member_val, ctx, b, ix);
                     if constexpr (not fixed_max_size) {
                        constexpr size_t next_run = etf::detail::bounded_run_size<Opts, T, I + 1>();
                        if constexpr (next_run > 0) {
                           etf::detail::ensure_space(b, ix + next_run);
                        }
                     }
                  }
               }
               ++pair_count;
            });

            // Back-patch the actual written pair count
            etf::detail::write_be_at<uint32_t>(pair_count, b, count_pos);
         }
         else {
            constexpr auto count = static_cast<uint32_t>(count_to_write<Opts>());
            etf::detail::dump_tag_be_unchecked<uint32_t>(etf::tag::MAP_EXT, count, b, ix);

            for_each<N>([&]<size_t I>() {
               if constexpr (should_skip_field<Opts, I>()) {
                  return;
               }

               decltype(auto) member_val = [&]() -> decltype(auto) {
                  if constexpr (reflectable<T>) {
                     return get_member(value, get<I>(t));
                  }
                  else {
                     return get_member(value, get<I>(reflect<T>::values));
                  }
               }();
               using val_t = std::decay_t<decltype(member_val)>;

               static constexpr auto key = reflect<T>::keys[I];
               constexpr size_t val_max = etf::detail::is_indirect_v<val_t> ? 0 : etf::detail::max_etf_val_size<val_t>();

               if constexpr (etf::detail::is_etf_str_like<val_t>) {
                  const auto sv = etf::detail::get_etf_str_view(member_val);
                  const auto len = static_cast<uint32_t>(sv.size());
                  if constexpr (not fixed_max_size) {
                     constexpr size_t next_run = etf::detail::bounded_run_size<Opts, T, I + 1>();
                     etf::detail::ensure_space(b, ix + 5 + key.size() + 5 + len + next_run);
                  }
                  static constexpr etf::detail::static_etf_key<key.size()> formatted_key{key};
                  etf::detail::dump_bytes_unchecked<formatted_key.buf.size()>(formatted_key.buf.data(), b, ix);
                  etf::detail::dump_binary_unchecked(sv.data(), len, b, ix);
               }
               else {
                  if constexpr (not fixed_max_size) {
                     if constexpr (val_max == 0) {
                        etf::detail::ensure_space(b, ix + 5 + key.size());
                     }
                  }

                  static constexpr etf::detail::static_etf_key<key.size()> formatted_key{key};
                  etf::detail::dump_bytes_unchecked<formatted_key.buf.size()>(formatted_key.buf.data(), b, ix);

                  if constexpr (val_max > 0) {
                     to<EETF, val_t>::template no_header<write_unchecked_on<Opts>()>(member_val, ctx, b, ix);
                  }
                  else {
                     to<EETF, val_t>::template no_header<Opts>(member_val, ctx, b, ix);
                     if constexpr (not fixed_max_size) {
                        constexpr size_t next_run = etf::detail::bounded_run_size<Opts, T, I + 1>();
                        if constexpr (next_run > 0) {
                           etf::detail::ensure_space(b, ix + next_run);
                        }
                     }
                  }
               }
            });
         }
      }
   };

   // Custom serialization wrapper (glz::custom)
   template <class T>
      requires(is_specialization_v<T, custom_t>)
   struct to<EETF, T> final : etf::detail::to_etf_base<to<EETF, T>, T>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void no_header(auto&& value, is_context auto&& ctx, auto&& b, auto& ix)
      {
         detail::dispatch_custom_write<EETF, no_header_on<Opts>(), T>(value, ctx, b, ix);
      }
   };

   // Glaze value wrapper
   template <class T>
      requires(glaze_value_t<T> && !custom_write<T>)
   struct to<EETF, T> final : etf::detail::to_etf_base<to<EETF, T>, T>
   {
      template <auto Opts, class Value, is_context Ctx, class B, class IX>
      GLZ_ALWAYS_INLINE static void no_header(Value&& value, Ctx&& ctx, B&& b, IX&& ix)
      {
         using V = std::decay_t<decltype(get_member(std::declval<Value>(), meta_wrapper_v<T>))>;
         to<EETF, V>::template no_header<Opts>(get_member(std::forward<Value>(value), meta_wrapper_v<T>),
                                               std::forward<Ctx>(ctx), std::forward<B>(b), ix);
      }
   };

   // Custom write specialization (types with static constexpr bool custom_write = true in glz::meta)
   template <custom_write T>
   struct to<EETF, T> final : etf::detail::to_etf_base<to<EETF, T>, T>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void no_header(auto&& value, is_context auto&& ctx, auto&& b, auto& ix)
      {
         using V = std::decay_t<T>;
         if constexpr (glaze_value_t<V>) {
            using M = std::decay_t<decltype(get_member(value, meta_wrapper_v<V>))>;
            to<EETF, M>::template no_header<Opts>(get_member(value, meta_wrapper_v<V>), ctx, b, ix);
         }
         else if constexpr (requires { to<EETF, meta_t<V>>::template no_header<Opts>(std::forward<decltype(value)>(value), ctx, b, ix); }) {
            to<EETF, meta_t<V>>::template no_header<Opts>(
               std::forward<decltype(value)>(value), ctx, b, ix);
         }
      }
   };

   // Variants
   template <is_variant T>
      requires(not custom_write<T>)
   struct to<EETF, T> final : etf::detail::to_etf_base<to<EETF, T>, T>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void no_header(auto&& value, is_context auto&& ctx, auto&& b, auto& ix)
      {
         glz::visit<std::variant_size_v<std::decay_t<T>>>(
            [&]<size_t I>() {
               using var_t = std::decay_t<std::variant_alternative_t<I, std::decay_t<T>>>;
               to<EETF, var_t>::template no_header<Opts>(std::get<I>(value), ctx, b, ix);
            },
            value.index());
      }
   };

   // Generic JSON wrapper
   template <num_mode Mode, template <class> class MapType>
   struct to<EETF, generic_json<Mode, MapType>> final
      : etf::detail::to_etf_base<to<EETF, generic_json<Mode, MapType>>, generic_json<Mode, MapType>>
   {
      template <auto Opts>
      static void no_header(const generic_json<Mode, MapType>& value, is_context auto& ctx, auto& b, auto& ix)
      {
         glz::visit<std::variant_size_v<decltype(value.data)>>(
            [&]<size_t I>() {
               using var_t = std::decay_t<std::variant_alternative_t<I, decltype(value.data)>>;
               to<EETF, var_t>::template no_header<Opts>(std::get<I>(value.data), ctx, b, ix);
            },
            value.data.index());
      }
   };

   // std::chrono::system_clock::time_point (discusy::timestamp): serialize as ISO 8601 string (BINARY_EXT)
   template <is_system_time_point T>
      requires(not custom_write<T>)
   struct to<EETF, T> final : etf::detail::to_etf_base<to<EETF, T>, T>
   {
      template <auto Opts>
      static void no_header(const auto& value, is_context auto&& ctx, auto&& b, auto& ix) noexcept
      {
         using Period = std::remove_cvref_t<decltype(value)>::duration::period;
         constexpr auto max_ts_len = chrono_detail::iso_time_point_max_size<Period>;
         if constexpr (!check_write_unchecked(Opts)) {
            etf::detail::ensure_space(b, ix + 5 + max_ts_len);
         }
         const auto start_ix = ix;
         ix += 5;
         chrono_detail::write_iso_time_point<false>(value, ctx, b, ix);
         if (static_cast<bool>(ctx.error)) [[unlikely]] {
            ix = start_ix;
            return;
         }

         const auto len = static_cast<uint32_t>(ix - (start_ix + 5));
         b[start_ix] = static_cast<std::decay_t<decltype(b)>::value_type>(etf::tag::BINARY_EXT);
         etf::detail::write_be_at<uint32_t>(len, b, start_ix + 1);
      }
   };

   // std::chrono::year_month_day
   template <is_year_month_day T>
      requires(not custom_write<T>)
   struct to<EETF, T> final : etf::detail::to_etf_base<to<EETF, T>, T>
   {
      template <auto Opts>
      static void no_header(const auto& value, is_context auto&& ctx, auto&& b, auto& ix) noexcept
      {
         if constexpr (!check_write_unchecked(Opts)) {
            etf::detail::ensure_space(b, ix + 5 + chrono_detail::iso_date_max_size);
         }
         const auto start_ix = ix;
         ix += 5;
         const int yr = static_cast<int>(value.year());
         const auto mo = static_cast<unsigned>(value.month());
         const auto dy = static_cast<unsigned>(value.day());
         chrono_detail::write_iso_date<false>(yr, mo, dy, ctx, b, ix);
         if (static_cast<bool>(ctx.error)) [[unlikely]] {
            ix = start_ix;
            return;
         }

         const auto len = static_cast<uint32_t>(ix - (start_ix + 5));
         b[start_ix] = static_cast<std::decay_t<decltype(b)>::value_type>(etf::tag::BINARY_EXT);
         etf::detail::write_be_at<uint32_t>(len, b, start_ix + 1);
      }
   };

   // Raw JSON & Text wrappers
   template <class T>
   struct to<EETF, basic_raw_json<T>> final : etf::detail::to_etf_base<to<EETF, basic_raw_json<T>>, basic_raw_json<T>>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void no_header(auto&& value, is_context auto&&, auto&& b, auto& ix)
      {
         etf::detail::dump_bytes(value.str.data(), value.str.size(), b, ix);
      }
   };

   template <class T>
   struct to<EETF, basic_text<T>> final : etf::detail::to_etf_base<to<EETF, basic_text<T>>, basic_text<T>>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void no_header(auto&& value, is_context auto&&, auto&& b, auto& ix)
      {
         etf::detail::dump_bytes(value.str.data(), value.str.size(), b, ix);
      }
   };

   // Top-level write helper functions for ETF
   template <write_supported<EETF> T, output_buffer Buffer>
   [[nodiscard]] inline error_ctx write_etf(T&& value, Buffer& buffer)
   {
      return write<etf_opts>(std::forward<T>(value), buffer);
   }

   template <write_supported<EETF> T, output_buffer Buffer>
   [[nodiscard]] inline error_ctx write_etf(T&& value, Buffer& buffer, is_context auto&& ctx)
   {
      return write<etf_opts>(std::forward<T>(value), buffer, ctx);
   }

   template <write_supported<EETF> T>
   [[nodiscard]] inline expected<std::string, error_ctx> write_etf(T&& value)
   {
      std::string buffer;
      const auto ec = write<etf_opts>(std::forward<T>(value), buffer);
      if (static_cast<bool>(ec)) {
         return unexpected(ec);
      }
      return buffer;
   }
} // namespace glz
