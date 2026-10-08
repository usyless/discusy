#pragma once

#include <bit>
#include <charconv>
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
#include <glaze/util/for_each.hpp>
#include <glaze/util/variant.hpp>
#include <glaze/json/generic_fwd.hpp>

#include "tags.hpp"
#include "opts.hpp"

namespace glz
{
   template <>
   struct serialize<EETF>
   {
      template <auto Opts, class T, is_context Ctx, class B, class IX>
      GLZ_ALWAYS_INLINE static void op(T&& value, Ctx&& ctx, B&& b, IX&& ix)
      {
         if constexpr (!check_no_header(Opts)) {
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
            char buf[64];
            auto [p, ec] = std::to_chars(buf, buf + sizeof(buf), value);
            const uint32_t len = static_cast<uint32_t>(p - buf);
            etf::detail::dump_byte(etf::tag::BINARY_EXT, b, ix);
            etf::detail::dump_be<uint32_t>(len, b, ix);
            etf::detail::dump_bytes(buf, len, b, ix);
            return;
         }
         else if constexpr (std::floating_point<std::decay_t<T>>) {
            const auto d = static_cast<double>(value);
            uint64_t bits = 0;
            std::memcpy(&bits, &d, sizeof(double));
            etf::detail::dump_byte(etf::tag::NEW_FLOAT_EXT, b, ix);
            etf::detail::dump_be<uint64_t>(bits, b, ix);
         }
         else if constexpr (std::integral<std::decay_t<T>>) {
            using U = std::decay_t<T>;
            if constexpr (std::is_unsigned_v<U>) {
               if (value <= 255) {
                  const uint8_t buf[2] = {etf::tag::SMALL_INTEGER_EXT, static_cast<uint8_t>(value)};
                  etf::detail::dump_bytes(buf, 2, b, ix);
               }
               else if (value <= 2147483647ULL) {
                  uint8_t buf[5];
                  buf[0] = etf::tag::INTEGER_EXT;
                  int32_t be = static_cast<int32_t>(value);
                  if constexpr (std::endian::native == std::endian::little) be = std::byteswap(be);
                  std::memcpy(&buf[1], &be, 4);
                  etf::detail::dump_bytes(buf, 5, b, ix);
               }
               else {
                  // 64-bit integer (e.g. Snowflake) -> SMALL_BIG_EXT (8 bytes, little-endian digits)
                  uint8_t buf[11];
                  buf[0] = etf::tag::SMALL_BIG_EXT;
                  buf[1] = 8;
                  buf[2] = 0; // positive sign = 0
                  uint64_t raw = static_cast<uint64_t>(value);
                  if constexpr (std::endian::native == std::endian::big) {
                     raw = std::byteswap(raw);
                  }
                  std::memcpy(&buf[3], &raw, 8);
                  etf::detail::dump_bytes(buf, 11, b, ix);
               }
            }
            else { // signed integer
               if (value >= 0 && value <= 255) {
                  const uint8_t buf[2] = {etf::tag::SMALL_INTEGER_EXT, static_cast<uint8_t>(value)};
                  etf::detail::dump_bytes(buf, 2, b, ix);
               }
               else if (value >= -2147483648LL && value <= 2147483647LL) {
                  uint8_t buf[5];
                  buf[0] = etf::tag::INTEGER_EXT;
                  int32_t be = static_cast<int32_t>(value);
                  if constexpr (std::endian::native == std::endian::little) be = std::byteswap(be);
                  std::memcpy(&buf[1], &be, 4);
                  etf::detail::dump_bytes(buf, 5, b, ix);
               }
               else {
                  // 64-bit signed integer -> SMALL_BIG_EXT
                  uint8_t buf[11];
                  buf[0] = etf::tag::SMALL_BIG_EXT;
                  buf[1] = 8;
                  if (value < 0) {
                     buf[2] = 1; // negative sign = 1
                     uint64_t raw = static_cast<uint64_t>(-static_cast<int64_t>(value));
                     if constexpr (std::endian::native == std::endian::big) {
                        raw = std::byteswap(raw);
                     }
                     std::memcpy(&buf[3], &raw, 8);
                  }
                  else {
                     buf[2] = 0; // positive sign = 0
                     uint64_t raw = static_cast<uint64_t>(value);
                     if constexpr (std::endian::native == std::endian::big) {
                        raw = std::byteswap(raw);
                     }
                     std::memcpy(&buf[3], &raw, 8);
                  }
                  etf::detail::dump_bytes(buf, 11, b, ix);
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
         const uint32_t len = static_cast<uint32_t>(sv.size());
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
            etf::detail::dump_byte(etf::magic_version, b, ix);
            op<no_header_on<Opts>()>(std::forward<decltype(value)>(value), std::forward<decltype(ctx)>(ctx), b, ix);
            return;
         }
         const size_t sz = value.size();
         if (sz == 0) {
            etf::detail::dump_byte(etf::tag::NIL_EXT, b, ix);
            return;
         }

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
            etf::detail::dump_byte(etf::magic_version, b, ix);
            op<no_header_on<Opts>()>(std::forward<decltype(value)>(value), std::forward<decltype(ctx)>(ctx), b, ix);
            return;
         }
         etf::detail::dump_tag_be<uint32_t>(etf::tag::MAP_EXT, static_cast<uint32_t>(value.size()), b, ix);
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
            etf::detail::dump_byte(etf::magic_version, b, ix);
            op<no_header_on<Opts>()>(std::forward<decltype(value)>(value), std::forward<decltype(ctx)>(ctx), b, ix);
            return;
         }
         static constexpr auto N = reflect<T>::size;

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
            etf::detail::dump_byte(etf::magic_version, b, ix);
            op<no_header_on<Opts>()>(value, std::forward<decltype(ctx)>(ctx), b, ix);
            return;
         }
         char buf[64];
         size_t buf_ix = 0;
         chrono_detail::write_iso_time_point<false>(value, ctx, buf, buf_ix);
         if (static_cast<bool>(ctx.error)) [[unlikely]] return;

         etf::detail::dump_byte(etf::tag::BINARY_EXT, b, ix);
         etf::detail::dump_be<uint32_t>(static_cast<uint32_t>(buf_ix), b, ix);
         etf::detail::dump_bytes(buf, buf_ix, b, ix);
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
            etf::detail::dump_byte(etf::magic_version, b, ix);
            op<no_header_on<Opts>()>(value, std::forward<decltype(ctx)>(ctx), b, ix);
            return;
         }
         char buf[16];
         size_t buf_ix = 0;
         const int yr = static_cast<int>(value.year());
         const auto mo = static_cast<unsigned>(value.month());
         const auto dy = static_cast<unsigned>(value.day());
         chrono_detail::write_iso_date<false>(yr, mo, dy, ctx, buf, buf_ix);
         if (static_cast<bool>(ctx.error)) [[unlikely]] return;

         etf::detail::dump_byte(etf::tag::BINARY_EXT, b, ix);
         etf::detail::dump_be<uint32_t>(static_cast<uint32_t>(buf_ix), b, ix);
         etf::detail::dump_bytes(buf, buf_ix, b, ix);
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
