#pragma once

#include <cmath>

#include <bit>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <algorithm>
#include <type_traits>

#include <glaze/core/context.hpp>
#include <glaze/core/buffer_traits.hpp>
#include <glaze/util/inline.hpp>

namespace glz
{
   inline constexpr std::uint32_t ETF = 20000;
}

namespace glz::etf
{
   inline constexpr uint8_t magic_version = 131;

   namespace tag
   {
      inline constexpr uint8_t NEW_FLOAT_EXT = 70;         // 'F' - 8 bytes IEEE double float, big-endian
      inline constexpr uint8_t BIT_BINARY_EXT = 77;        // 'M' - bit binary
      inline constexpr uint8_t COMPRESSED = 80;            // 'P' - zlib compressed term
      inline constexpr uint8_t ATOM_CACHE_REF = 82;        // 'R' - cached atom
      inline constexpr uint8_t NEW_PID_EXT = 88;           // 'X'
      inline constexpr uint8_t NEW_PORT_EXT = 89;          // 'Y'
      inline constexpr uint8_t NEWER_REFERENCE_EXT = 90;   // 'Z'
      inline constexpr uint8_t SMALL_INTEGER_EXT = 97;     // 'a' - 1 byte unsigned integer (0-255)
      inline constexpr uint8_t INTEGER_EXT = 98;           // 'b' - 4 bytes signed integer, big-endian
      inline constexpr uint8_t FLOAT_EXT = 99;             // 'c' - 31 bytes float string
      inline constexpr uint8_t ATOM_EXT = 100;             // 'd' - 2 bytes length Latin-1 atom, big-endian
      inline constexpr uint8_t REFERENCE_EXT = 101;        // 'e'
      inline constexpr uint8_t PORT_EXT = 102;             // 'f'
      inline constexpr uint8_t PID_EXT = 103;              // 'g'
      inline constexpr uint8_t SMALL_TUPLE_EXT = 104;      // 'h' - 1 byte arity
      inline constexpr uint8_t LARGE_TUPLE_EXT = 105;      // 'i' - 4 bytes arity, big-endian
      inline constexpr uint8_t NIL_EXT = 106;              // 'j' - empty list []
      inline constexpr uint8_t STRING_EXT = 107;           // 'k' - 2 bytes length byte list, big-endian
      inline constexpr uint8_t LIST_EXT = 108;             // 'l' - 4 bytes length elements + tail term, big-endian
      inline constexpr uint8_t BINARY_EXT = 109;           // 'm' - 4 bytes length raw binary / string, big-endian
      inline constexpr uint8_t SMALL_BIG_EXT = 110;        // 'n' - 1 byte length, 1 byte sign, little-endian digits
      inline constexpr uint8_t LARGE_BIG_EXT = 111;        // 'o' - 4 bytes length, 1 byte sign, little-endian digits
      inline constexpr uint8_t NEW_FUN_EXT = 112;          // 'p'
      inline constexpr uint8_t EXPORT_EXT = 113;           // 'q'
      inline constexpr uint8_t NEW_REFERENCE_EXT = 114;    // 'r'
      inline constexpr uint8_t SMALL_ATOM_EXT = 115;       // 's' - 1 byte length Latin-1 atom
      inline constexpr uint8_t MAP_EXT = 116;              // 't' - 4 bytes arity (pairs), big-endian
      inline constexpr uint8_t FUN_EXT = 117;              // 'u'
      inline constexpr uint8_t ATOM_UTF8_EXT = 118;        // 'v' - 2 bytes length UTF-8 atom, big-endian
      inline constexpr uint8_t SMALL_ATOM_UTF8_EXT = 119;  // 'w' - 1 byte length UTF-8 atom
      inline constexpr uint8_t V4_PORT_EXT = 120;          // 'x'
   }

   namespace detail
   {
      template <typename T>
      GLZ_ALWAYS_INLINE constexpr T to_big_endian(T val) noexcept
      {
         if constexpr (std::endian::native == std::endian::little && sizeof(T) > 1) {
            return std::byteswap(val);
         }
         return val;
      }

      template <typename T>
      GLZ_ALWAYS_INLINE constexpr T from_big_endian(T val) noexcept
      {
         if constexpr (std::endian::native == std::endian::little && sizeof(T) > 1) {
            return std::byteswap(val);
         }
         return val;
      }

      template <typename T>
      GLZ_ALWAYS_INLINE T read_be(const void* ptr) noexcept
      {
         T val;
         std::memcpy(&val, ptr, sizeof(T));
         if constexpr (std::endian::native == std::endian::little && sizeof(T) > 1) {
            val = std::byteswap(val);
         }
         return val;
      }

      template <typename B>
      GLZ_ALWAYS_INLINE void ensure_space(B& b, size_t needed)
      {
         if (needed > b.size()) [[unlikely]] {
            glz::grow_buffer(b, (std::max)(b.size() * 2, needed + 64));
         }
      }

      template <typename B, typename IX>
      GLZ_ALWAYS_INLINE void dump_byte(uint8_t byte, B& b, IX& ix)
      {
         if (ix >= b.size()) [[unlikely]] {
            glz::grow_buffer(b, (std::max)(b.size() * 2, ix + 64));
         }
         b[ix] = static_cast<std::decay_t<B>::value_type>(byte);
         ++ix;
      }

      template <typename B, typename IX>
      GLZ_ALWAYS_INLINE void dump_bytes(const void* data, size_t count, B& b, IX& ix)
      {
         if (count == 0) return;
         const auto needed = ix + count;
         if (needed > b.size()) [[unlikely]] {
            glz::grow_buffer(b, (std::max)(b.size() * 2, needed + 64));
         }
         std::memcpy(&b[ix], data, count);
         ix += count;
      }

      template <typename T, typename B, typename IX>
      GLZ_ALWAYS_INLINE void dump_be(T val, B& b, IX& ix)
      {
         constexpr auto n = sizeof(T);
         const auto needed = ix + n;
         if (needed > b.size()) [[unlikely]] {
            glz::grow_buffer(b, (std::max)(b.size() * 2, needed + 64));
         }
         if constexpr (std::endian::native == std::endian::little && n > 1) {
            val = std::byteswap(val);
         }
         std::memcpy(&b[ix], &val, n);
         ix += n;
      }

      template <typename T, typename B, typename IX>
      GLZ_ALWAYS_INLINE void dump_tag_be(uint8_t tag_val, T val, B& b, IX& ix)
      {
         constexpr auto n = 1 + sizeof(T);
         const auto needed = ix + n;
         if (needed > b.size()) [[unlikely]] {
            glz::grow_buffer(b, (std::max)(b.size() * 2, needed + 64));
         }
         b[ix] = static_cast<std::decay_t<B>::value_type>(tag_val);
         if constexpr (std::endian::native == std::endian::little && sizeof(T) > 1) {
            val = std::byteswap(val);
         }
         std::memcpy(&b[ix + 1], &val, sizeof(T));
         ix += n;
      }

      template <typename B, typename IX>
      GLZ_ALWAYS_INLINE void dump_binary(const void* data, uint32_t len, B& b, IX& ix)
      {
         const auto needed = ix + 5 + len;
         if (needed > b.size()) [[unlikely]] {
            glz::grow_buffer(b, (std::max)(b.size() * 2, needed + 64));
         }
         b[ix] = static_cast<std::decay_t<B>::value_type>(tag::BINARY_EXT);
         uint32_t be_len = len;
         if constexpr (std::endian::native == std::endian::little) {
            be_len = std::byteswap(be_len);
         }
         std::memcpy(&b[ix + 1], &be_len, 4);
         if (len > 0) {
            std::memcpy(&b[ix + 5], data, len);
         }
         ix += 5 + len;
      }

      template <size_t N>
      struct static_etf_key {
         char buf[5 + N]{};
         constexpr static_etf_key(std::string_view sv) noexcept {
            buf[0] = static_cast<char>(tag::BINARY_EXT);
            const auto sz = static_cast<uint32_t>(N);
            buf[1] = static_cast<char>((sz >> 24) & 0xFF);
            buf[2] = static_cast<char>((sz >> 16) & 0xFF);
            buf[3] = static_cast<char>((sz >> 8) & 0xFF);
            buf[4] = static_cast<char>(sz & 0xFF);
            for (size_t i = 0; i < N; ++i) {
               buf[5 + i] = sv[i];
            }
         }
      };

      template <typename T, typename B>
      GLZ_ALWAYS_INLINE void write_be_at(T val, B& b, size_t pos)
      {
         constexpr auto n = sizeof(T);
         if constexpr (std::endian::native == std::endian::little && n > 1) {
            val = std::byteswap(val);
         }
         std::memcpy(&b[pos], &val, n);
      }

      template <is_context Ctx, typename It0, typename It1>
      GLZ_ALWAYS_INLINE bool read_key(Ctx& ctx, It0& it, It1 end, std::string_view& key) noexcept
      {
         if (it >= end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return false;
         }
         const auto tag = static_cast<uint8_t>(*it++);
         if (tag == tag::BINARY_EXT) [[likely]] {
            if (end - it < 4) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return false;
            }
            const uint32_t len = read_be<uint32_t>(it);
            it += 4;
            if (static_cast<size_t>(end - it) < len) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return false;
            }
            key = std::string_view{reinterpret_cast<const char*>(it), len};
            it += len;
            return true;
         }
         switch (tag) {
         case tag::STRING_EXT: {
            if (end - it < 2) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return false;
            }
            const uint16_t len = read_be<uint16_t>(it);
            it += 2;
            if (static_cast<size_t>(end - it) < len) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return false;
            }
            key = std::string_view{reinterpret_cast<const char*>(it), len};
            it += len;
            return true;
         }
         case tag::ATOM_UTF8_EXT:
         case tag::ATOM_EXT: {
            if (end - it < 2) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return false;
            }
            const uint16_t len = read_be<uint16_t>(it);
            it += 2;
            if (static_cast<size_t>(end - it) < len) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return false;
            }
            key = std::string_view{reinterpret_cast<const char*>(it), len};
            it += len;
            return true;
         }
         case tag::SMALL_ATOM_UTF8_EXT:
         case tag::SMALL_ATOM_EXT: {
            if (it >= end) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return false;
            }
            const auto len = static_cast<uint8_t>(*it++);
            if (static_cast<size_t>(end - it) < len) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return false;
            }
            key = std::string_view{reinterpret_cast<const char*>(it), len};
            it += len;
            return true;
         }
         default:
            ctx.error = error_code::syntax_error;
            return false;
         }
      }

      template <typename It0, typename It1>
      GLZ_ALWAYS_INLINE bool is_nil(It0 it, It1 end) noexcept
      {
         if (it >= end) return false;
         const uint8_t tag = static_cast<uint8_t>(*it);
         if (tag == tag::SMALL_ATOM_UTF8_EXT || tag == tag::SMALL_ATOM_EXT) {
            if (end - it >= 2) {
               const auto len = static_cast<uint8_t>(*(it + 1));
               if (len == 3 && end - it >= 5) {
                  return (it[2] == 'n' && it[3] == 'i' && it[4] == 'l');
               }
               if (len == 4 && end - it >= 6) {
                  return (it[2] == 'n' && it[3] == 'u' && it[4] == 'l' && it[5] == 'l');
               }
               if (len == 9 && end - it >= 11) {
                  std::string_view s{reinterpret_cast<const char*>(it + 2), 9};
                  return s == "undefined";
               }
            }
         }
         else if (tag == tag::ATOM_UTF8_EXT || tag == tag::ATOM_EXT) {
            if (end - it >= 3) {
               const uint16_t len = read_be<uint16_t>(it + 1);
               if (len == 3 && end - it >= 6) {
                  return (it[3] == 'n' && it[4] == 'i' && it[5] == 'l');
               }
               if (len == 4 && end - it >= 7) {
                  return (it[3] == 'n' && it[4] == 'u' && it[5] == 'l' && it[6] == 'l');
               }
               if (len == 9 && end - it >= 12) {
                  std::string_view s{reinterpret_cast<const char*>(it + 3), 9};
                  return s == "undefined";
               }
            }
         }
         return false;
      }

      template <typename It0, typename It1>
      GLZ_ALWAYS_INLINE void skip_nil(It0& it, It1 end) noexcept
      {
         if (it >= end) return;
         const auto tag = static_cast<uint8_t>(*it++);
         if (tag == tag::SMALL_ATOM_UTF8_EXT || tag == tag::SMALL_ATOM_EXT) {
            if (it < end) {
               const uint8_t len = static_cast<uint8_t>(*it++);
               it += (std::min)(static_cast<size_t>(end - it), static_cast<size_t>(len));
            }
         }
         else if (tag == tag::ATOM_UTF8_EXT || tag == tag::ATOM_EXT) {
            if (end - it >= 2) {
               const uint16_t len = read_be<uint16_t>(it);
               it += 2;
               it += (std::min)(static_cast<size_t>(end - it), static_cast<size_t>(len));
            }
         }
      }

      template <is_context Ctx, typename It0, typename It1>
      GLZ_ALWAYS_INLINE bool read_str(Ctx& ctx, It0& it, It1 end, std::string_view& sv) noexcept
      {
         if (it >= end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return false;
         }
         const auto tag = static_cast<uint8_t>(*it++);
         switch (tag) {
         case tag::BINARY_EXT: {
            if (end - it < 4) [[unlikely]] { ctx.error = error_code::unexpected_end; return false; }
            const uint32_t len = read_be<uint32_t>(it);
            it += 4;
            if (static_cast<size_t>(end - it) < len) [[unlikely]] { ctx.error = error_code::unexpected_end; return false; }
            sv = std::string_view{reinterpret_cast<const char*>(it), len};
            it += len;
            return true;
         }
         case tag::STRING_EXT: {
            if (end - it < 2) [[unlikely]] { ctx.error = error_code::unexpected_end; return false; }
            const uint16_t len = read_be<uint16_t>(it);
            it += 2;
            if (static_cast<size_t>(end - it) < len) [[unlikely]] { ctx.error = error_code::unexpected_end; return false; }
            sv = std::string_view{reinterpret_cast<const char*>(it), len};
            it += len;
            return true;
         }
         case tag::NIL_EXT: {
            sv = std::string_view{};
            return true;
         }
         default:
            ctx.error = error_code::syntax_error;
            return false;
         }
      }

      template <is_context Ctx, typename It0, typename It1>
      GLZ_ALWAYS_INLINE bool read_atom_or_str(Ctx& ctx, It0& it, It1 end, std::string_view& sv) noexcept
      {
         if (it >= end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return false;
         }
         const auto tag = static_cast<uint8_t>(*it++);
         switch (tag) {
         case tag::ATOM_UTF8_EXT:
         case tag::ATOM_EXT:
         case tag::STRING_EXT: {
            if (end - it < 2) [[unlikely]] { ctx.error = error_code::unexpected_end; return false; }
            const uint16_t len = read_be<uint16_t>(it);
            it += 2;
            if (static_cast<size_t>(end - it) < len) [[unlikely]] { ctx.error = error_code::unexpected_end; return false; }
            sv = std::string_view{reinterpret_cast<const char*>(it), len};
            it += len;
            return true;
         }
         case tag::SMALL_ATOM_UTF8_EXT:
         case tag::SMALL_ATOM_EXT: {
            if (it >= end) [[unlikely]] { ctx.error = error_code::unexpected_end; return false; }
            const auto len = static_cast<uint8_t>(*it++);
            if (static_cast<size_t>(end - it) < len) [[unlikely]] { ctx.error = error_code::unexpected_end; return false; }
            sv = std::string_view{reinterpret_cast<const char*>(it), len};
            it += len;
            return true;
         }
         case tag::BINARY_EXT: {
            if (end - it < 4) [[unlikely]] { ctx.error = error_code::unexpected_end; return false; }
            const uint32_t len = read_be<uint32_t>(it);
            it += 4;
            if (static_cast<size_t>(end - it) < len) [[unlikely]] { ctx.error = error_code::unexpected_end; return false; }
            sv = std::string_view{reinterpret_cast<const char*>(it), len};
            it += len;
            return true;
         }
         case tag::NIL_EXT: {
            sv = std::string_view{};
            return true;
         }
         default:
            ctx.error = error_code::syntax_error;
            return false;
         }
      }

      template <is_context Ctx, typename It0, typename It1>
      GLZ_ALWAYS_INLINE bool read_bool(Ctx& ctx, It0& it, It1 end, bool& b) noexcept
      {
         if (it >= end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return false;
         }
         std::string_view atom;
         if (!read_atom_or_str(ctx, it, end, atom)) return false;
         if (atom == "true") {
            b = true;
            return true;
         }
         if (atom == "false") {
            b = false;
            return true;
         }
         ctx.error = error_code::syntax_error;
         return false;
      }

      template <typename T, is_context Ctx, typename It0, typename It1>
      GLZ_ALWAYS_INLINE bool read_number(Ctx& ctx, It0& it, It1 end, T& num) noexcept
      {
         if (it >= end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return false;
         }
         const auto tag = static_cast<uint8_t>(*it++);
         switch (tag) {
         case tag::SMALL_INTEGER_EXT: {
            if (it >= end) [[unlikely]] { ctx.error = error_code::unexpected_end; return false; }
            const auto val = static_cast<uint8_t>(*it++);
            num = static_cast<T>(val);
            return true;
         }
         case tag::INTEGER_EXT: {
            if (end - it < 4) [[unlikely]] { ctx.error = error_code::unexpected_end; return false; }
            const int32_t val = read_be<int32_t>(it);
            it += 4;
            num = static_cast<T>(val);
            return true;
         }
         case tag::SMALL_BIG_EXT: {
            if (it >= end) [[unlikely]] { ctx.error = error_code::unexpected_end; return false; }
            const auto n = static_cast<uint8_t>(*it++);
            if (end - it < 1 + n) [[unlikely]] { ctx.error = error_code::unexpected_end; return false; }
            const uint8_t sign = static_cast<uint8_t>(*it++);
            uint64_t raw = 0;
            if (n == 8) [[likely]] {
               std::memcpy(&raw, it, 8);
               if constexpr (std::endian::native == std::endian::big) {
                  raw = std::byteswap(raw);
               }
            }
            else {
               for (uint8_t i = 0; i < n && i < 8; ++i) {
                  raw |= (static_cast<uint64_t>(static_cast<uint8_t>(it[i])) << (i * 8));
               }
            }
            it += n;
            if constexpr (std::is_signed_v<T>) {
               if (sign) {
                  num = static_cast<T>(-static_cast<int64_t>(raw));
               } else {
                  num = static_cast<T>(raw);
               }
            } else {
               num = static_cast<T>(raw);
            }
            return true;
         }
         case tag::LARGE_BIG_EXT: {
            if (end - it < 4) [[unlikely]] { ctx.error = error_code::unexpected_end; return false; }
            const uint32_t n = read_be<uint32_t>(it);
            it += 4;
            if (static_cast<size_t>(end - it) <= n) [[unlikely]] { ctx.error = error_code::unexpected_end; return false; }
            const auto sign = static_cast<uint8_t>(*it++);
            uint64_t raw = 0;
            for (uint32_t i = 0; i < n; ++i) {
               if (i < 8) {
                  raw |= (static_cast<uint64_t>(static_cast<uint8_t>(it[i])) << (i * 8));
               }
            }
            it += n;
            if constexpr (std::is_signed_v<T>) {
               if (sign) {
                  num = static_cast<T>(-static_cast<int64_t>(raw));
               } else {
                  num = static_cast<T>(raw);
               }
            } else {
               num = static_cast<T>(raw);
            }
            return true;
         }
         case tag::NEW_FLOAT_EXT: {
            if constexpr (!std::is_floating_point_v<T>) {
               ctx.error = error_code::parse_number_failure;
               return false;
            }
            else {
               if (end - it < 8) [[unlikely]] { ctx.error = error_code::unexpected_end; return false; }
               const uint64_t bits = read_be<uint64_t>(it);
               it += 8;
               double d = NAN;
               std::memcpy(&d, &bits, sizeof(double));
               num = static_cast<T>(d);
               return true;
            }
         }
         case tag::FLOAT_EXT: {
            if constexpr (!std::is_floating_point_v<T>) {
               ctx.error = error_code::parse_number_failure;
               return false;
            }
            else {
               if (end - it < 31) [[unlikely]] { ctx.error = error_code::unexpected_end; return false; }
               std::string_view sv{reinterpret_cast<const char*>(it), 31};
               it += 31;
               double d = 0.0;
               auto [p, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), d);
               if (ec != std::errc{}) {
                  ctx.error = error_code::parse_number_failure;
                  return false;
               }
               num = static_cast<T>(d);
               return true;
            }
         }
         case tag::BINARY_EXT: {
            if (end - it < 4) [[unlikely]] { ctx.error = error_code::unexpected_end; return false; }
            const uint32_t len = read_be<uint32_t>(it);
            it += 4;
            if (static_cast<size_t>(end - it) < len) [[unlikely]] { ctx.error = error_code::unexpected_end; return false; }
            std::string_view sv{reinterpret_cast<const char*>(it), len};
            it += len;
            auto [p, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), num);
            if (ec != std::errc{} || p != sv.data() + sv.size()) {
               ctx.error = error_code::parse_number_failure;
               return false;
            }
            return true;
         }
         case tag::STRING_EXT:
         case tag::ATOM_UTF8_EXT:
         case tag::ATOM_EXT: {
            if (end - it < 2) [[unlikely]] { ctx.error = error_code::unexpected_end; return false; }
            const uint16_t len = read_be<uint16_t>(it);
            it += 2;
            if (static_cast<size_t>(end - it) < len) [[unlikely]] { ctx.error = error_code::unexpected_end; return false; }
            std::string_view sv{reinterpret_cast<const char*>(it), len};
            it += len;
            auto [p, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), num);
            if (ec != std::errc{} || p != sv.data() + sv.size()) {
               ctx.error = error_code::parse_number_failure;
               return false;
            }
            return true;
         }
         case tag::SMALL_ATOM_UTF8_EXT:
         case tag::SMALL_ATOM_EXT: {
            if (it >= end) [[unlikely]] { ctx.error = error_code::unexpected_end; return false; }
            const auto len = static_cast<uint8_t>(*it++);
            if (static_cast<size_t>(end - it) < len) [[unlikely]] { ctx.error = error_code::unexpected_end; return false; }
            std::string_view sv{reinterpret_cast<const char*>(it), len};
            it += len;
            auto [p, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), num);
            if (ec != std::errc{} || p != sv.data() + sv.size()) {
               ctx.error = error_code::parse_number_failure;
               return false;
            }
            return true;
         }
         default:
            ctx.error = error_code::syntax_error;
            return false;
         }
      }
   } // namespace detail
} // namespace glz::etf
