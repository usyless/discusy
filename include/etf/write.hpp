#pragma once

#include <bit>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <variant>
#include <type_traits>

#include <glaze/core/context.hpp>
#include <glaze/core/opts.hpp>
#include <glaze/core/write.hpp>
#include <glaze/core/reflect.hpp>
#include <glaze/core/to.hpp>
#include <glaze/core/chrono.hpp>
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
      constexpr size_t min_etf_val_size();

      template <class V, size_t I>
      constexpr size_t min_field_etf_size()
      {
         if constexpr (skipped_by_meta<V, I, operation::serialize>) {
            return 0;
         }
         else {
            using field_val_t = std::decay_t<field_t<V, I>>;
            if constexpr (never_written<etf_opts{}, field_val_t>) {
               return 0;
            }
            else {
               constexpr auto key = glz::get<I>(reflect<V>::keys);
               return 5 + key.size() + min_etf_val_size<field_val_t>();
            }
         }
      }

      template <class T>
      constexpr size_t min_etf_struct_size()
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
      constexpr size_t min_etf_val_size()
      {
         using V = std::decay_t<T>;
         if constexpr (std::is_void_v<V>) {
            return 0;
         }
         else if constexpr (std::is_pointer_v<V>) {
            return 1;
         }
         else if constexpr (std::same_as<V, std::nullptr_t> || always_null_t<V>) {
            return 5; // \x77\x03nil
         }
         else if constexpr (boolean_like<V>) {
            return 6; // \x77\x04true
         }
         else if constexpr (num_t<V>) {
            if constexpr (std::floating_point<V>) {
               return 9; // NEW_FLOAT_EXT + 8 bytes
            }
            else if constexpr (sizeof(V) <= 1) {
               return 2; // SMALL_INTEGER_EXT + 1 byte
            }
            else if constexpr (sizeof(V) <= 4) {
               return 5; // INTEGER_EXT + 4 bytes
            }
            else {
               return 11; // SMALL_BIG_EXT (or quoted_num)
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
         else if constexpr (nullable_like<V>) {
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
            using M = std::decay_t<decltype(get_member(std::declval<V>(), meta_wrapper_v<V>))>;
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
      inline constexpr size_t min_etf_size = [] {
         using V = std::decay_t<T>;
         if constexpr (std::is_void_v<V>) {
            return 1;
         }
         else {
            constexpr size_t val_sz = min_etf_val_size<V>();
            constexpr size_t obj_sz = sizeof(V);
            // Pre-allocate buffer with a minimum size of the size of the object
            return 1 + (std::max)(val_sz, obj_sz); // 1 for ETF magic_version
         }
      }();
   } // namespace etf::detail

   template <>
   struct serialize<EETF>
   {
      template <auto Opts, class T, is_context Ctx, class B, class IX>
      GLZ_ALWAYS_INLINE static void op(T&& value, Ctx&& ctx, B&& b, IX&& ix)
      {
         if constexpr (!check_no_header(Opts)) {
            constexpr size_t prealloc = etf::detail::min_etf_size<std::decay_t<T>>;
            etf::detail::ensure_space(b, ix + prealloc);
            etf::detail::dump_byte(etf::magic_version, b, ix);
            to<EETF, std::remove_cvref_t<T>>::template op<no_header_on<Opts>()>(
               std::forward<T>(value), std::forward<Ctx>(ctx), std::forward<B>(b), ix);
         }
         else {
            to<EETF, std::remove_cvref_t<T>>::template op<Opts>(
               std::forward<T>(value), std::forward<Ctx>(ctx), std::forward<B>(b), ix);
         }
      }
   };

   // Null (atom nil)
   template <always_null_t T>
   struct to<EETF, T> final
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto& ctx, auto& b, auto& ix) noexcept
      {
         if constexpr (!check_no_header(Opts)) {
            etf::detail::dump_byte(etf::magic_version, b, ix);
            op<no_header_on<Opts>()>(value, ctx, b, ix);
            return;
         }
         etf::detail::dump_bytes("\x77\x03" "nil", 5, b, ix);
      }
   };

   // Skip type (no-op)
   template <>
   struct to<EETF, skip> final
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&&, is_context auto&&, auto&&, auto&&) noexcept
      {}
   };

   // Booleans (atom true / false)
   template <boolean_like T>
      requires(!custom_write<T>)
   struct to<EETF, T> final
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(const bool value, is_context auto& ctx, auto& b, auto& ix) noexcept
      {
         if constexpr (!check_no_header(Opts)) {
            etf::detail::dump_byte(etf::magic_version, b, ix);
            op<no_header_on<Opts>()>(value, ctx, b, ix);
            return;
         }
         if (value) {
            etf::detail::dump_bytes("\x77\x04" "true", 6, b, ix);
         }
         else {
            etf::detail::dump_bytes("\x77\x05" "false", 7, b, ix);
         }
      }
   };

   // Numbers (integers, floats, bignums, and quoted-numbers)
   template <num_t T>
      requires(!custom_write<T>)
   struct to<EETF, T> final
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto& ctx, auto& b, auto& ix) noexcept
      {
         if constexpr (!check_no_header(Opts)) {
            etf::detail::dump_byte(etf::magic_version, b, ix);
            op<no_header_on<Opts>()>(std::forward<decltype(value)>(value), ctx, b, ix);
            return;
         }
         if constexpr (check_quoted_num(Opts)) {
            etf::detail::ensure_space(b, ix + 5 + 64);
            const auto start_ix = ix;
            ix += 5;
            glz::write_chars::op<write_unchecked_on<opt_false<Opts, quoted_num_opt_tag{}>>()>(value, ctx, b, ix);
            const auto len = static_cast<uint32_t>(ix - (start_ix + 5));
            b[start_ix] = static_cast<std::decay_t<decltype(b)>::value_type>(etf::tag::BINARY_EXT);
            etf::detail::write_be_at<uint32_t>(len, b, start_ix + 1);
            return;
         }
         else if constexpr (std::floating_point<std::decay_t<T>>) {
            const auto d = static_cast<double>(value);
            uint64_t bits = 0;
            std::memcpy(&bits, &d, sizeof(double));
            etf::detail::dump_tag_be<uint64_t>(etf::tag::NEW_FLOAT_EXT, bits, b, ix);
         }
         else if constexpr (std::integral<std::decay_t<T>>) {
            using U = std::decay_t<T>;
            if constexpr (std::is_unsigned_v<U>) {
               if (value <= 255) {
                  etf::detail::ensure_space(b, ix + 2);
                  b[ix] = static_cast<std::decay_t<decltype(b)>::value_type>(etf::tag::SMALL_INTEGER_EXT);
                  b[ix + 1] = static_cast<std::decay_t<decltype(b)>::value_type>(value);
                  ix += 2;
               }
               else if (value <= 2147483647ULL) {
                  etf::detail::dump_tag_be<int32_t>(etf::tag::INTEGER_EXT, static_cast<int32_t>(value), b, ix);
               }
               else {
                  // 64-bit integer (e.g. Snowflake) -> SMALL_BIG_EXT (8 bytes, little-endian digits)
                  etf::detail::ensure_space(b, ix + 11);
                  b[ix] = static_cast<std::decay_t<decltype(b)>::value_type>(etf::tag::SMALL_BIG_EXT);
                  b[ix + 1] = static_cast<std::decay_t<decltype(b)>::value_type>(8);
                  b[ix + 2] = static_cast<std::decay_t<decltype(b)>::value_type>(0); // positive sign = 0
                  auto raw = static_cast<uint64_t>(value);
                  if constexpr (std::endian::native == std::endian::big) {
                     raw = std::byteswap(raw);
                  }
                  std::memcpy(&b[ix + 3], &raw, 8);
                  ix += 11;
               }
            }
            else { // signed integer
               if (value >= 0 && value <= 255) {
                  etf::detail::ensure_space(b, ix + 2);
                  b[ix] = static_cast<std::decay_t<decltype(b)>::value_type>(etf::tag::SMALL_INTEGER_EXT);
                  b[ix + 1] = static_cast<std::decay_t<decltype(b)>::value_type>(value);
                  ix += 2;
               }
               else if (value >= -2147483648LL && value <= 2147483647LL) {
                  etf::detail::dump_tag_be<int32_t>(etf::tag::INTEGER_EXT, static_cast<int32_t>(value), b, ix);
               }
               else {
                  // 64-bit signed integer -> SMALL_BIG_EXT
                  etf::detail::ensure_space(b, ix + 11);
                  b[ix] = static_cast<std::decay_t<decltype(b)>::value_type>(etf::tag::SMALL_BIG_EXT);
                  b[ix + 1] = static_cast<std::decay_t<decltype(b)>::value_type>(8);
                  if (value < 0) {
                     b[ix + 2] = static_cast<std::decay_t<decltype(b)>::value_type>(1); // negative sign = 1
                     auto raw = static_cast<uint64_t>(-static_cast<int64_t>(value));
                     if constexpr (std::endian::native == std::endian::big) {
                        raw = std::byteswap(raw);
                     }
                     std::memcpy(&b[ix + 3], &raw, 8);
                  }
                  else {
                     b[ix + 2] = static_cast<std::decay_t<decltype(b)>::value_type>(0); // positive sign = 0
                     auto raw = static_cast<uint64_t>(value);
                     if constexpr (std::endian::native == std::endian::big) {
                        raw = std::byteswap(raw);
                     }
                     std::memcpy(&b[ix + 3], &raw, 8);
                  }
                  ix += 11;
               }
            }
         }
      }
   };

   // Strings (BINARY_EXT)
   template <str_t T>
      requires(!custom_write<T>)
   struct to<EETF, T> final
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto& ctx, auto& b, auto& ix) noexcept
      {
         if constexpr (!check_no_header(Opts)) {
            etf::detail::dump_byte(etf::magic_version, b, ix);
            op<no_header_on<Opts>()>(std::forward<decltype(value)>(value), ctx, b, ix);
            return;
         }
         const auto sv = str_view<T>(value);
         const auto len = static_cast<uint32_t>(sv.size());
         etf::detail::dump_binary(sv.data(), len, b, ix);
      }
   };

   // Enums (reflected names as BINARY_EXT or underlying numbers)
   template <class T>
      requires(std::is_enum_v<T> && !custom_write<T>)
   struct to<EETF, T> final
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& b, auto& ix)
      {
         if constexpr (!check_no_header(Opts)) {
            etf::detail::dump_byte(etf::magic_version, b, ix);
            op<no_header_on<Opts>()>(std::forward<decltype(value)>(value), std::forward<decltype(ctx)>(ctx), b, ix);
            return;
         }
         if constexpr (glaze_enum_t<T> || (meta_keys<T> && std::is_enum_v<std::decay_t<T>>)) {
            const std::string_view str = get_enum_name(value);
            if (!str.empty()) {
               to<EETF, std::string_view>::template op<Opts>(str, ctx, b, ix);
               return;
            }
         }
         using V = std::underlying_type_t<std::decay_t<T>>;
         to<EETF, V>::template op<Opts>(static_cast<V>(value), ctx, b, ix);
      }
   };

   // Nullable types (std::optional, std::unique_ptr, std::shared_ptr)
   template <nullable_like T>
   struct to<EETF, T> final
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& b, auto& ix)
      {
         if constexpr (!check_no_header(Opts)) {
            etf::detail::dump_byte(etf::magic_version, b, ix);
            op<no_header_on<Opts>()>(std::forward<decltype(value)>(value), std::forward<decltype(ctx)>(ctx), b, ix);
            return;
         }
         if (value) {
            serialize<EETF>::op<Opts>(*value, ctx, b, ix);
         }
         else {
            to<EETF, std::nullptr_t>::template op<Opts>(nullptr, ctx, b, ix);
         }
      }
   };

   // Arrays (std::vector, std::deque, std::span, etc.)
   template <readable_array_t T>
   struct to<EETF, T> final
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& b, auto& ix)
      {
         if constexpr (!check_no_header(Opts)) {
            constexpr size_t prealloc = etf::detail::min_etf_size<std::decay_t<T>>;
            etf::detail::ensure_space(b, ix + prealloc);
            etf::detail::dump_byte(etf::magic_version, b, ix);
            op<no_header_on<Opts>()>(std::forward<decltype(value)>(value), std::forward<decltype(ctx)>(ctx), b, ix);
            return;
         }
         const size_t sz = value.size();
         if (sz == 0) {
            etf::detail::dump_byte(etf::tag::NIL_EXT, b, ix);
            return;
         }

         using item_t = std::decay_t<range_value_t<T>>;
         constexpr size_t item_alloc = (std::max)(sizeof(item_t), etf::detail::min_etf_val_size<item_t>());
         etf::detail::ensure_space(b, ix + 6 + (sz * item_alloc));

         etf::detail::dump_tag_be<uint32_t>(etf::tag::LIST_EXT, static_cast<uint32_t>(sz), b, ix);
         for (auto&& item : value) {
            serialize<EETF>::op<Opts>(item, ctx, b, ix);
         }
         // Tail term is NIL_EXT
         etf::detail::dump_byte(etf::tag::NIL_EXT, b, ix);
      }
   };

   // Tuples
   template <class T>
      requires(tuple_t<T> || is_std_tuple<T>)
   struct to<EETF, T> final
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& b, auto& ix)
      {
         if constexpr (!check_no_header(Opts)) {
            constexpr size_t prealloc = etf::detail::min_etf_size<std::decay_t<T>>;
            etf::detail::ensure_space(b, ix + prealloc);
            etf::detail::dump_byte(etf::magic_version, b, ix);
            op<no_header_on<Opts>()>(std::forward<decltype(value)>(value), std::forward<decltype(ctx)>(ctx), b, ix);
            return;
         }
         using V = std::decay_t<T>;
         static constexpr auto N = glz::tuple_size_v<V>;
         if constexpr (N <= 255) {
            etf::detail::dump_byte(etf::tag::SMALL_TUPLE_EXT, b, ix);
            etf::detail::dump_byte(static_cast<uint8_t>(N), b, ix);
         }
         else {
            etf::detail::dump_byte(etf::tag::LARGE_TUPLE_EXT, b, ix);
            etf::detail::dump_be<uint32_t>(static_cast<uint32_t>(N), b, ix);
         }
         if constexpr (is_std_tuple<V>) {
            for_each<N>([&]<size_t I>() {
               serialize<EETF>::op<Opts>(std::get<I>(value), ctx, b, ix);
            });
         }
         else {
            for_each<N>([&]<size_t I>() {
               serialize<EETF>::op<Opts>(glz::get<I>(value), ctx, b, ix);
            });
         }
      }
   };

   // Maps
   template <readable_map_t T>
      requires(!custom_write<T>)
   struct to<EETF, T> final
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& b, auto& ix)
      {
         if constexpr (!check_no_header(Opts)) {
            constexpr size_t prealloc = etf::detail::min_etf_size<std::decay_t<T>>;
            etf::detail::ensure_space(b, ix + prealloc);
            etf::detail::dump_byte(etf::magic_version, b, ix);
            op<no_header_on<Opts>()>(std::forward<decltype(value)>(value), std::forward<decltype(ctx)>(ctx), b, ix);
            return;
         }
         const size_t sz = value.size();
         if (sz == 0) {
            etf::detail::dump_tag_be<uint32_t>(etf::tag::MAP_EXT, 0, b, ix);
            return;
         }
         using key_t = std::decay_t<typename std::decay_t<T>::key_type>;
         using mapped_t = std::decay_t<typename std::decay_t<T>::mapped_type>;
         constexpr size_t pair_alloc = (std::max)(sizeof(key_t), etf::detail::min_etf_val_size<key_t>()) +
                                       (std::max)(sizeof(mapped_t), etf::detail::min_etf_val_size<mapped_t>());
         etf::detail::ensure_space(b, ix + 5 + (sz * pair_alloc));

         etf::detail::dump_tag_be<uint32_t>(etf::tag::MAP_EXT, static_cast<uint32_t>(sz), b, ix);
         for (auto&& [k, v] : value) {
            serialize<EETF>::op<Opts>(k, ctx, b, ix);
            serialize<EETF>::op<Opts>(v, ctx, b, ix);
         }
      }
   };

   // Glaze objects & reflectable structs with STRING KEYS ONLY (BINARY_EXT)
   // and single-pass arity back-patching.
   template <class T>
      requires((glaze_object_t<T> || reflectable<T>) && !custom_write<T>)
   struct to<EETF, T> final
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& b, auto& ix)
      {
         if constexpr (!check_no_header(Opts)) {
            constexpr size_t prealloc = etf::detail::min_etf_size<std::decay_t<T>>;
            etf::detail::ensure_space(b, ix + prealloc);
            etf::detail::dump_byte(etf::magic_version, b, ix);
            op<no_header_on<Opts>()>(std::forward<decltype(value)>(value), std::forward<decltype(ctx)>(ctx), b, ix);
            return;
         }
         static constexpr auto N = reflect<T>::size;

         constexpr size_t struct_min = etf::detail::min_etf_struct_size<std::decay_t<T>>();
         etf::detail::ensure_space(b, ix + struct_min);

         const size_t count_pos = ix + 1;
         etf::detail::dump_tag_be<uint32_t>(etf::tag::MAP_EXT, 0, b, ix); // placeholder for arity

         uint32_t pair_count = 0;

         decltype(auto) t = [&]() -> decltype(auto) {
            if constexpr (reflectable<T>) {
               return to_tie(value);
            }
            else {
               return nullptr;
            }
         }();

         for_each<N>([&]<size_t I>() {
            if constexpr (skipped_by_meta<T, I, operation::serialize>) {
               return;
            }

            decltype(auto) member = [&]() -> decltype(auto) {
               if constexpr (reflectable<T>) {
                  return get<I>(t);
               }
               else {
                  return get<I>(reflect<T>::values);
               }
            }();

            decltype(auto) member_val = get_member(value, member);
            using val_t = std::decay_t<decltype(member_val)>;

            if constexpr (never_written<Opts, val_t>) {
               return;
            }

            if constexpr (Opts.skip_null_members && nullable_like<val_t>) {
               if (!member_val) {
                  return;
               }
            }

            // CRITICAL: Discord requires string keys (BINARY_EXT), never atoms!
            static constexpr auto key = reflect<T>::keys[I];
            static constexpr etf::detail::static_etf_key<key.size()> formatted_key{key};
            etf::detail::dump_bytes(formatted_key.buf, sizeof(formatted_key.buf), b, ix);

            serialize<EETF>::op<Opts>(member_val, ctx, b, ix);
            ++pair_count;
         });

         // Back-patch the actual written pair count
         etf::detail::write_be_at<uint32_t>(pair_count, b, count_pos);
      }
   };

   // Glaze value wrapper
   template <class T>
      requires(glaze_value_t<T> && !custom_write<T>)
   struct to<EETF, T> final
   {
      template <auto Opts, class Value, is_context Ctx, class B, class IX>
      GLZ_ALWAYS_INLINE static void op(Value&& value, Ctx&& ctx, B&& b, IX&& ix)
      {
         if constexpr (!check_no_header(Opts)) {
            etf::detail::dump_byte(etf::magic_version, b, ix);
            op<no_header_on<Opts>()>(std::forward<Value>(value), std::forward<Ctx>(ctx), std::forward<B>(b), ix);
            return;
         }
         using V = std::decay_t<decltype(get_member(std::declval<Value>(), meta_wrapper_v<T>))>;
         to<EETF, V>::template op<Opts>(get_member(std::forward<Value>(value), meta_wrapper_v<T>),
                                        std::forward<Ctx>(ctx), std::forward<B>(b), ix);
      }
   };

   // Variants
   template <is_variant T>
      requires(not custom_write<T>)
   struct to<EETF, T> final
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& b, auto& ix)
      {
         if constexpr (!check_no_header(Opts)) {
            etf::detail::dump_byte(etf::magic_version, b, ix);
            op<no_header_on<Opts>()>(std::forward<decltype(value)>(value), std::forward<decltype(ctx)>(ctx), b, ix);
            return;
         }
         std::visit([&](auto&& v) {
            serialize<EETF>::op<Opts>(v, ctx, b, ix);
         }, value);
      }
   };

   // Generic JSON wrapper
   template <num_mode Mode, template <class> class MapType>
   struct to<EETF, generic_json<Mode, MapType>> final
   {
      template <auto Opts>
      static void op(const generic_json<Mode, MapType>& value, is_context auto& ctx, auto& b, auto& ix)
      {
         if constexpr (!check_no_header(Opts)) {
            etf::detail::dump_byte(etf::magic_version, b, ix);
            op<no_header_on<Opts>()>(value, ctx, b, ix);
            return;
         }
         std::visit([&](auto&& v) {
            serialize<EETF>::op<Opts>(v, ctx, b, ix);
         }, value.data);
      }
   };

   // std::chrono::system_clock::time_point (discusy::timestamp): serialize as ISO 8601 string (BINARY_EXT)
   template <is_system_time_point T>
      requires(not custom_write<T>)
   struct to<EETF, T> final
   {
      template <auto Opts>
      static void op(const auto& value, is_context auto&& ctx, auto&& b, auto& ix) noexcept
      {
         if constexpr (!check_no_header(Opts)) {
            constexpr size_t prealloc = etf::detail::min_etf_size<std::decay_t<T>>;
            etf::detail::ensure_space(b, ix + prealloc);
            etf::detail::dump_byte(etf::magic_version, b, ix);
            op<no_header_on<Opts>()>(value, std::forward<decltype(ctx)>(ctx), b, ix);
            return;
         }
         using Period = std::remove_cvref_t<decltype(value)>::duration::period;
         constexpr auto max_ts_len = chrono_detail::iso_time_point_max_size<Period>;
         etf::detail::ensure_space(b, ix + 5 + max_ts_len);
         const auto start_ix = ix;
         ix += 5;
         chrono_detail::write_iso_time_point<false>(value, ctx, b, ix);
         if (static_cast<bool>(ctx.error)) [[unlikely]] return;

         const auto len = static_cast<uint32_t>(ix - (start_ix + 5));
         b[start_ix] = static_cast<std::decay_t<decltype(b)>::value_type>(etf::tag::BINARY_EXT);
         etf::detail::write_be_at<uint32_t>(len, b, start_ix + 1);
      }
   };

   // std::chrono::year_month_day
   template <is_year_month_day T>
      requires(not custom_write<T>)
   struct to<EETF, T> final
   {
      template <auto Opts>
      static void op(const auto& value, is_context auto&& ctx, auto&& b, auto& ix) noexcept
      {
         if constexpr (!check_no_header(Opts)) {
            constexpr size_t prealloc = etf::detail::min_etf_size<std::decay_t<T>>;
            etf::detail::ensure_space(b, ix + prealloc);
            etf::detail::dump_byte(etf::magic_version, b, ix);
            op<no_header_on<Opts>()>(value, std::forward<decltype(ctx)>(ctx), b, ix);
            return;
         }
         etf::detail::ensure_space(b, ix + 5 + chrono_detail::iso_date_max_size);
         const auto start_ix = ix;
         ix += 5;
         const int yr = static_cast<int>(value.year());
         const auto mo = static_cast<unsigned>(value.month());
         const auto dy = static_cast<unsigned>(value.day());
         chrono_detail::write_iso_date<false>(yr, mo, dy, ctx, b, ix);
         if (static_cast<bool>(ctx.error)) [[unlikely]] return;

         const auto len = static_cast<uint32_t>(ix - (start_ix + 5));
         b[start_ix] = static_cast<std::decay_t<decltype(b)>::value_type>(etf::tag::BINARY_EXT);
         etf::detail::write_be_at<uint32_t>(len, b, start_ix + 1);
      }
   };

   // Raw JSON & Text wrappers
   template <class T>
   struct to<EETF, basic_raw_json<T>> final
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& b, auto& ix)
      {
         if constexpr (!check_no_header(Opts)) {
            etf::detail::dump_byte(etf::magic_version, b, ix);
            op<no_header_on<Opts>()>(std::forward<decltype(value)>(value), std::forward<decltype(ctx)>(ctx), b, ix);
            return;
         }
         etf::detail::dump_bytes(value.str.data(), value.str.size(), b, ix);
      }
   };

   template <class T>
   struct to<EETF, basic_text<T>> final
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& b, auto& ix)
      {
         if constexpr (!check_no_header(Opts)) {
            etf::detail::dump_byte(etf::magic_version, b, ix);
            op<no_header_on<Opts>()>(std::forward<decltype(value)>(value), std::forward<decltype(ctx)>(ctx), b, ix);
            return;
         }
         etf::detail::dump_bytes(value.str.data(), value.str.size(), b, ix);
      }
   };

   // Top-level write helper functions for ETF
   template <write_supported<EETF> T, output_buffer Buffer>
   [[nodiscard]] inline error_ctx write_etf(T&& value, Buffer& buffer)
   {
      constexpr size_t min_sz = etf::detail::min_etf_size<std::decay_t<T>>;
      etf::detail::ensure_space(buffer, min_sz);
      return write<etf_opts>(std::forward<T>(value), buffer);
   }

   template <write_supported<EETF> T, output_buffer Buffer>
   [[nodiscard]] inline error_ctx write_etf(T&& value, Buffer& buffer, is_context auto&& ctx)
   {
      constexpr size_t min_sz = etf::detail::min_etf_size<std::decay_t<T>>;
      etf::detail::ensure_space(buffer, min_sz);
      return write<etf_opts>(std::forward<T>(value), buffer, ctx);
   }

   template <write_supported<EETF> T>
   [[nodiscard]] inline expected<std::string, error_ctx> write_etf(T&& value)
   {
      std::string buffer;
      constexpr size_t min_sz = etf::detail::min_etf_size<std::decay_t<T>>;
      etf::detail::ensure_space(buffer, min_sz);
      const auto ec = write<etf_opts>(std::forward<T>(value), buffer);
      if (static_cast<bool>(ec)) {
         return unexpected(ec);
      }
      return buffer;
   }
} // namespace glz
