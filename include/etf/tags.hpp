#pragma once

#include <cmath>
#include <limits>

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
#include <glaze/util/glaze_fast_float.hpp>
#include <glaze/util/compare.hpp>
#include <glaze/util/dump.hpp>
#include <glaze/util/bit.hpp>

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
            if constexpr (requires { b.capacity(); }) {
               if (b.capacity() >= needed) {
                  glz::resize_unfilled(b, b.capacity());
                  return;
               }
            }
            glz::grow_buffer(b, (std::max)(b.size() * 2, needed + 64));
         }
      }

      template <typename B>
      GLZ_ALWAYS_INLINE bool ensure_space(is_context auto& ctx, B& b, size_t needed)
      {
         return glz::ensure_space(ctx, b, needed);
      }

      template <typename B, typename IX>
      GLZ_ALWAYS_INLINE void dump_byte_unchecked(uint8_t byte, B& b, IX& ix) noexcept
      {
         b[ix] = static_cast<std::decay_t<B>::value_type>(byte);
         ++ix;
      }

      template <typename B, typename IX>
      GLZ_ALWAYS_INLINE void dump_byte(uint8_t byte, B& b, IX& ix)
      {
         ensure_space(b, ix + 1);
         dump_byte_unchecked(byte, b, ix);
      }

      template <typename B, typename IX>
      GLZ_ALWAYS_INLINE bool dump_byte(is_context auto& ctx, uint8_t byte, B& b, IX& ix)
      {
         if (!glz::ensure_space(ctx, b, ix + 1 + write_padding_bytes)) [[unlikely]] return false;
         dump_byte_unchecked(byte, b, ix);
         return true;
      }

      template <typename B, typename IX>
      GLZ_ALWAYS_INLINE void dump_bytes_unchecked(const void* data, size_t count, B& b, IX& ix) noexcept
      {
         if (count == 0) return;
         std::memcpy(&b[ix], data, count);
         ix += count;
      }

      template <typename B, typename IX>
      GLZ_ALWAYS_INLINE void dump_bytes(const void* data, size_t count, B& b, IX& ix)
      {
         if (count == 0) return;
         ensure_space(b, ix + count);
         dump_bytes_unchecked(data, count, b, ix);
      }

      template <typename B, typename IX>
      GLZ_ALWAYS_INLINE bool dump_bytes(is_context auto& ctx, const void* data, size_t count, B& b, IX& ix)
      {
         if (count == 0) return true;
         if (!glz::ensure_space(ctx, b, ix + count + write_padding_bytes)) [[unlikely]] return false;
         dump_bytes_unchecked(data, count, b, ix);
         return true;
      }

      template <typename T, typename B, typename IX>
      GLZ_ALWAYS_INLINE void dump_be_unchecked(T val, B& b, IX& ix) noexcept
      {
         constexpr auto n = sizeof(T);
         if constexpr (std::endian::native == std::endian::little && n > 1) {
            val = std::byteswap(val);
         }
         std::memcpy(&b[ix], &val, n);
         ix += n;
      }

      template <typename T, typename B, typename IX>
      GLZ_ALWAYS_INLINE void dump_be(T val, B& b, IX& ix)
      {
         constexpr auto n = sizeof(T);
         ensure_space(b, ix + n);
         dump_be_unchecked(val, b, ix);
      }

      template <typename T, typename B, typename IX>
      GLZ_ALWAYS_INLINE bool dump_be(is_context auto& ctx, T val, B& b, IX& ix)
      {
         constexpr auto n = sizeof(T);
         if (!glz::ensure_space(ctx, b, ix + n + write_padding_bytes)) [[unlikely]] return false;
         dump_be_unchecked(val, b, ix);
         return true;
      }

      template <typename T, typename B, typename IX>
      GLZ_ALWAYS_INLINE void dump_tag_be_unchecked(uint8_t tag_val, T val, B& b, IX& ix) noexcept
      {
         constexpr auto n = 1 + sizeof(T);
         b[ix] = static_cast<std::decay_t<B>::value_type>(tag_val);
         if constexpr (std::endian::native == std::endian::little && sizeof(T) > 1) {
            val = std::byteswap(val);
         }
         std::memcpy(&b[ix + 1], &val, sizeof(T));
         ix += n;
      }

      template <typename T, typename B, typename IX>
      GLZ_ALWAYS_INLINE void dump_tag_be(uint8_t tag_val, T val, B& b, IX& ix)
      {
         constexpr auto n = 1 + sizeof(T);
         ensure_space(b, ix + n);
         dump_tag_be_unchecked(tag_val, val, b, ix);
      }

      template <typename T, typename B, typename IX>
      GLZ_ALWAYS_INLINE bool dump_tag_be(is_context auto& ctx, uint8_t tag_val, T val, B& b, IX& ix)
      {
         constexpr auto n = 1 + sizeof(T);
         if (!glz::ensure_space(ctx, b, ix + n + write_padding_bytes)) [[unlikely]] return false;
         dump_tag_be_unchecked(tag_val, val, b, ix);
         return true;
      }

      template <typename B, typename IX>
      GLZ_ALWAYS_INLINE void dump_binary_unchecked(const void* data, uint32_t len, B& b, IX& ix) noexcept
      {
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

      template <typename B, typename IX>
      GLZ_ALWAYS_INLINE void dump_binary(const void* data, uint32_t len, B& b, IX& ix)
      {
         ensure_space(b, ix + 5 + len);
         dump_binary_unchecked(data, len, b, ix);
      }

      template <typename B, typename IX>
      GLZ_ALWAYS_INLINE bool dump_binary(is_context auto& ctx, const void* data, uint32_t len, B& b, IX& ix)
      {
         if (!glz::ensure_space(ctx, b, ix + 5 + len + write_padding_bytes)) [[unlikely]] return false;
         dump_binary_unchecked(data, len, b, ix);
         return true;
      }

      template <size_t N>
      struct static_etf_key {
         std::array<char, 5 + N> buf{};
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
         const auto tag = static_cast<uint8_t>(*it);
         if (tag == tag::SMALL_ATOM_UTF8_EXT || tag == tag::SMALL_ATOM_EXT) {
            if (end - it >= 2) {
               const auto len = static_cast<uint8_t>(*(it + 1));
               if (len == 3 && end - it >= 5) {
                  return glz::compare<3>(reinterpret_cast<const char*>(it + 2), "nil");
               }
               if (len == 4 && end - it >= 6) {
                  return glz::compare<4>(reinterpret_cast<const char*>(it + 2), "null");
               }
               if (len == 9 && end - it >= 11) {
                  return glz::compare<9>(reinterpret_cast<const char*>(it + 2), "undefined");
               }
            }
         }
         else if (tag == tag::ATOM_UTF8_EXT || tag == tag::ATOM_EXT) {
            if (end - it >= 3) {
               const uint16_t len = read_be<uint16_t>(it + 1);
               if (len == 3 && end - it >= 6) {
                  return glz::compare<3>(reinterpret_cast<const char*>(it + 3), "nil");
               }
               if (len == 4 && end - it >= 7) {
                  return glz::compare<4>(reinterpret_cast<const char*>(it + 3), "null");
               }
               if (len == 9 && end - it >= 12) {
                  return glz::compare<9>(reinterpret_cast<const char*>(it + 3), "undefined");
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
               const auto len = static_cast<uint8_t>(*it++);
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

      template <typename It0, typename It1>
      GLZ_ALWAYS_INLINE bool read_nil(It0& it, It1 end) noexcept
      {
         if (it >= end) return false;
         const auto tag = static_cast<uint8_t>(*it);
         if (tag == tag::SMALL_ATOM_UTF8_EXT || tag == tag::SMALL_ATOM_EXT) {
            if (end - it >= 2) {
               const auto len = static_cast<uint8_t>(*(it + 1));
               if (len == 3 && end - it >= 5 && glz::compare<3>(reinterpret_cast<const char*>(it + 2), "nil")) {
                  it += 5;
                  return true;
               }
               if (len == 4 && end - it >= 6 && glz::compare<4>(reinterpret_cast<const char*>(it + 2), "null")) {
                  it += 6;
                  return true;
               }
               if (len == 9 && end - it >= 11 && glz::compare<9>(reinterpret_cast<const char*>(it + 2), "undefined")) {
                  it += 11;
                  return true;
               }
            }
         }
         else if (tag == tag::ATOM_UTF8_EXT || tag == tag::ATOM_EXT) {
            if (end - it >= 3) {
               const uint16_t len = read_be<uint16_t>(it + 1);
               if (len == 3 && end - it >= 6 && glz::compare<3>(reinterpret_cast<const char*>(it + 3), "nil")) {
                  it += 6;
                  return true;
               }
               if (len == 4 && end - it >= 7 && glz::compare<4>(reinterpret_cast<const char*>(it + 3), "null")) {
                  it += 7;
                  return true;
               }
               if (len == 9 && end - it >= 12 && glz::compare<9>(reinterpret_cast<const char*>(it + 3), "undefined")) {
                  it += 12;
                  return true;
               }
            }
         }
         return false;
      }

      template <is_context Ctx, typename It0, typename It1>
      GLZ_ALWAYS_INLINE bool read_str(Ctx& ctx, It0& it, It1 end, std::string_view& sv) noexcept
      {
         if (it >= end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return false;
         }
         const auto tag = static_cast<uint8_t>(*it++);
         if (tag == tag::BINARY_EXT) [[likely]] {
            if (end - it < 4) [[unlikely]] { ctx.error = error_code::unexpected_end; return false; }
            const uint32_t len = read_be<uint32_t>(it);
            it += 4;
            if (static_cast<size_t>(end - it) < len) [[unlikely]] { ctx.error = error_code::unexpected_end; return false; }
            sv = std::string_view{reinterpret_cast<const char*>(it), len};
            it += len;
            return true;
         }
         switch (tag) {
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

      template <typename T>
      GLZ_ALWAYS_INLINE bool parse_number_from_string(std::string_view sv, T& num) noexcept
      {
         if constexpr (std::is_unsigned_v<T>) {
            if (sv.empty()) [[unlikely]] return false;
            size_t start = 0;
            const size_t sz = sv.size();
            while (start < sz && sv[start] == '0') {
               ++start;
            }
            if (start == sz) {
               num = 0;
               return true;
            }
            const size_t sig_digits = sz - start;
            if (sig_digits <= 18) [[likely]] {
               uint64_t val = 0;
               for (size_t i = start; i < sz; ++i) {
                  const uint8_t c = static_cast<uint8_t>(sv[i]) - static_cast<uint8_t>('0');
                  if (c > 9) [[unlikely]] return false;
                  val = (val * 10) + c;
               }
               if constexpr (sizeof(T) < 8) {
                  if (val > static_cast<uint64_t>((std::numeric_limits<T>::max)())) [[unlikely]] {
                     return false;
                  }
               }
               num = static_cast<T>(val);
               return true;
            }
            if (sig_digits > 20) [[unlikely]] {
               return false;
            }
            auto [p, ec] = std::from_chars(sv.data() + start, sv.data() + sz, num);
            return (ec == std::errc{}) && (p == sv.data() + sz);
         }
         else if constexpr (std::integral<T>) {
            auto [p, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), num);
            return (ec == std::errc{}) && (p == sv.data() + sv.size());
         }
         else if constexpr (glz::fast_float::is_supported_float_type<T>::value) {
            auto [p, ec] = glz::from_chars<false>(sv.data(), sv.data() + sv.size(), num);
            return (ec == std::errc{}) && (p == sv.data() + sv.size());
         }
         else {
            auto [p, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), num);
            return (ec == std::errc{}) && (p == sv.data() + sv.size());
         }
      }

      template <is_context Ctx, typename It0, typename It1>
      GLZ_ALWAYS_INLINE bool read_bool(Ctx& ctx, It0& it, It1 end, bool& b) noexcept
      {
         if (it >= end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return false;
         }
         const auto tag = static_cast<uint8_t>(*it);
         if (tag == tag::SMALL_ATOM_UTF8_EXT || tag == tag::SMALL_ATOM_EXT) [[likely]] {
            if (end - it >= 2) [[likely]] {
               const auto len = static_cast<uint8_t>(*(it + 1));
               if (len == 4 && end - it >= 6) {
                  if (glz::compare<4>(reinterpret_cast<const char*>(it + 2), "true")) {
                     it += 6;
                     b = true;
                     return true;
                  }
               }
               else if (len == 5 && end - it >= 7) {
                  if (glz::compare<5>(reinterpret_cast<const char*>(it + 2), "false")) {
                     it += 7;
                     b = false;
                     return true;
                  }
               }
            }
         }
         else if (tag == tag::ATOM_UTF8_EXT || tag == tag::ATOM_EXT) {
            if (end - it >= 3) {
               const uint16_t len = read_be<uint16_t>(it + 1);
               if (len == 4 && end - it >= 7) {
                  if (glz::compare<4>(reinterpret_cast<const char*>(it + 3), "true")) {
                     it += 7;
                     b = true;
                     return true;
                  }
               }
               else if (len == 5 && end - it >= 8) {
                  if (glz::compare<5>(reinterpret_cast<const char*>(it + 3), "false")) {
                     it += 8;
                     b = false;
                     return true;
                  }
               }
            }
         }

         std::string_view atom;
         if (!read_atom_or_str(ctx, it, end, atom)) return false;
         if (atom.size() == 4 && glz::compare<4>(atom.data(), "true")) {
            b = true;
            return true;
         }
         if (atom.size() == 5 && glz::compare<5>(atom.data(), "false")) {
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
            if constexpr (std::integral<T>) {
               if constexpr (std::is_signed_v<T>) {
                  if (val > static_cast<uint64_t>((std::numeric_limits<T>::max)())) [[unlikely]] {
                     ctx.error = error_code::parse_number_failure;
                     return false;
                  }
               }
            }
            num = static_cast<T>(val);
            return true;
         }
         case tag::INTEGER_EXT: {
            if (end - it < 4) [[unlikely]] { ctx.error = error_code::unexpected_end; return false; }
            const int32_t val = read_be<int32_t>(it);
            it += 4;
            if constexpr (std::integral<T>) {
               if constexpr (std::is_unsigned_v<T>) {
                  if (val < 0) [[unlikely]] {
                     ctx.error = error_code::parse_number_failure;
                     return false;
                  }
                  if (static_cast<uint64_t>(val) > static_cast<uint64_t>((std::numeric_limits<T>::max)())) [[unlikely]] {
                     ctx.error = error_code::parse_number_failure;
                     return false;
                  }
               }
               else {
                  if (val < static_cast<int64_t>((std::numeric_limits<T>::min)()) ||
                      val > static_cast<int64_t>((std::numeric_limits<T>::max)())) [[unlikely]] {
                     ctx.error = error_code::parse_number_failure;
                     return false;
                  }
               }
            }
            num = static_cast<T>(val);
            return true;
         }
         case tag::SMALL_BIG_EXT: {
            if (it >= end) [[unlikely]] { ctx.error = error_code::unexpected_end; return false; }
            const auto n = static_cast<uint8_t>(*it++);
            if (end - it < 1 || static_cast<size_t>(end - it - 1) < n) [[unlikely]] { ctx.error = error_code::unexpected_end; return false; }
            const auto sign = static_cast<uint8_t>(*it++);
            uint64_t raw = 0;
            bool overflow = false;
            if (n <= 8) [[likely]] {
               if constexpr (std::endian::native == std::endian::little) {
                  std::memcpy(&raw, it, n);
               }
               else {
                  for (uint8_t i = 0; i < n; ++i) {
                     raw |= (static_cast<uint64_t>(static_cast<uint8_t>(it[i])) << (i * 8));
                  }
               }
            }
            else {
               std::memcpy(&raw, it, 8);
               if constexpr (std::endian::native == std::endian::big) {
                  raw = std::byteswap(raw);
               }
               for (uint8_t i = 8; i < n; ++i) {
                  if (static_cast<uint8_t>(it[i]) != 0) {
                     overflow = true;
                     break;
                  }
               }
            }
            it += n;
            if constexpr (std::integral<T>) {
               if (overflow) [[unlikely]] {
                  ctx.error = error_code::parse_number_failure;
                  return false;
               }
               if constexpr (std::is_unsigned_v<T>) {
                  if (sign) [[unlikely]] {
                     ctx.error = error_code::parse_number_failure;
                     return false;
                  }
                  if (raw > static_cast<uint64_t>((std::numeric_limits<T>::max)())) [[unlikely]] {
                     ctx.error = error_code::parse_number_failure;
                     return false;
                  }
                  num = static_cast<T>(raw);
               }
               else {
                  if (sign == 0) {
                     if (raw > static_cast<uint64_t>((std::numeric_limits<T>::max)())) [[unlikely]] {
                        ctx.error = error_code::parse_number_failure;
                        return false;
                     }
                     num = static_cast<T>(raw);
                  }
                  else {
                     if (raw > static_cast<uint64_t>((std::numeric_limits<T>::max)()) + 1ULL) [[unlikely]] {
                        ctx.error = error_code::parse_number_failure;
                        return false;
                     }
                     if (raw == static_cast<uint64_t>((std::numeric_limits<T>::max)()) + 1ULL) {
                        num = (std::numeric_limits<T>::min)();
                     }
                     else {
                        num = static_cast<T>(-static_cast<int64_t>(raw));
                     }
                  }
               }
            }
            else {
               double d = sign ? -static_cast<double>(raw) : static_cast<double>(raw);
               num = static_cast<T>(d);
            }
            return true;
         }
         case tag::LARGE_BIG_EXT: {
            if (end - it < 4) [[unlikely]] { ctx.error = error_code::unexpected_end; return false; }
            const uint32_t n = read_be<uint32_t>(it);
            it += 4;
            if (end - it < 1 || static_cast<size_t>(end - it - 1) < n) [[unlikely]] { ctx.error = error_code::unexpected_end; return false; }
            const auto sign = static_cast<uint8_t>(*it++);
            bool overflow = false;
            uint64_t raw = 0;
            if (n <= 8) [[likely]] {
               if constexpr (std::endian::native == std::endian::little) {
                  std::memcpy(&raw, it, n);
               }
               else {
                  for (uint32_t i = 0; i < n; ++i) {
                     raw |= (static_cast<uint64_t>(static_cast<uint8_t>(it[i])) << (i * 8));
                  }
               }
            }
            else {
               std::memcpy(&raw, it, 8);
               if constexpr (std::endian::native == std::endian::big) {
                  raw = std::byteswap(raw);
               }
               for (uint32_t i = 8; i < n; ++i) {
                  if (static_cast<uint8_t>(it[i]) != 0) {
                     overflow = true;
                     break;
                  }
               }
            }
            it += n;
            if constexpr (std::integral<T>) {
               if (overflow) [[unlikely]] {
                  ctx.error = error_code::parse_number_failure;
                  return false;
               }
               if constexpr (std::is_unsigned_v<T>) {
                  if (sign) [[unlikely]] {
                     ctx.error = error_code::parse_number_failure;
                     return false;
                  }
                  if (raw > static_cast<uint64_t>((std::numeric_limits<T>::max)())) [[unlikely]] {
                     ctx.error = error_code::parse_number_failure;
                     return false;
                  }
                  num = static_cast<T>(raw);
               }
               else {
                  if (sign == 0) {
                     if (raw > static_cast<uint64_t>((std::numeric_limits<T>::max)())) [[unlikely]] {
                        ctx.error = error_code::parse_number_failure;
                        return false;
                     }
                     num = static_cast<T>(raw);
                  }
                  else {
                     if (raw > static_cast<uint64_t>((std::numeric_limits<T>::max)()) + 1ULL) [[unlikely]] {
                        ctx.error = error_code::parse_number_failure;
                        return false;
                     }
                     if (raw == static_cast<uint64_t>((std::numeric_limits<T>::max)()) + 1ULL) {
                        num = (std::numeric_limits<T>::min)();
                     }
                     else {
                        num = static_cast<T>(-static_cast<int64_t>(raw));
                     }
                  }
               }
            }
            else {
               double d = sign ? -static_cast<double>(raw) : static_cast<double>(raw);
               num = static_cast<T>(d);
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
               const char* p_it = reinterpret_cast<const char*>(it);
               it += 31;
               if constexpr (glz::fast_float::is_supported_float_type<T>::value) {
                  auto [p, ec] = glz::from_chars<false>(p_it, p_it + 31, num);
                  if (ec != std::errc{}) [[unlikely]] {
                     ctx.error = error_code::parse_number_failure;
                     return false;
                  }
               }
               else {
                  auto [p, ec] = std::from_chars(p_it, p_it + 31, num);
                  if (ec != std::errc{}) [[unlikely]] {
                     ctx.error = error_code::parse_number_failure;
                     return false;
                  }
               }
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
            if (!parse_number_from_string(sv, num)) [[unlikely]] {
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
            if (!parse_number_from_string(sv, num)) [[unlikely]] {
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
            if (!parse_number_from_string(sv, num)) [[unlikely]] {
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
