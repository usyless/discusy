#pragma once

#include <cstdint>
#include <cstring>
#include <glaze/core/context.hpp>
#include <glaze/core/opts.hpp>

#include "tags.hpp"

namespace glz
{
   template <>
   struct skip_value<EETF>
   {
      template <auto Opts>
      static void op(is_context auto& ctx, auto& it, auto end) noexcept
      {
         using etf::detail::read_be;

         if (it >= end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         depth_guard guard{ctx};
         if (!guard) [[unlikely]] {
            return;
         }

         const uint8_t tag = static_cast<uint8_t>(*it++);
         switch (tag) {
         case etf::tag::SMALL_INTEGER_EXT: {
            if (it >= end) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            ++it;
            break;
         }
         case etf::tag::INTEGER_EXT: {
            if (end - it < 4) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            it += 4;
            break;
         }
         case etf::tag::FLOAT_EXT: {
            if (end - it < 31) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            it += 31;
            break;
         }
         case etf::tag::NEW_FLOAT_EXT: {
            if (end - it < 8) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            it += 8;
            break;
         }
         case etf::tag::ATOM_UTF8_EXT:
         case etf::tag::ATOM_EXT:
         case etf::tag::STRING_EXT: {
            if (end - it < 2) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            const uint16_t len = read_be<uint16_t>(it);
            it += 2;
            if (static_cast<size_t>(end - it) < len) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            it += len;
            break;
         }
         case etf::tag::SMALL_ATOM_UTF8_EXT:
         case etf::tag::SMALL_ATOM_EXT: {
            if (it >= end) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            const uint8_t len = static_cast<uint8_t>(*it++);
            if (static_cast<size_t>(end - it) < len) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            it += len;
            break;
         }
         case etf::tag::BINARY_EXT: {
            if (end - it < 4) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            const uint32_t len = read_be<uint32_t>(it);
            it += 4;
            if (static_cast<size_t>(end - it) < len) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            it += len;
            break;
         }
         case etf::tag::BIT_BINARY_EXT: {
            if (end - it < 5) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            const uint32_t len = read_be<uint32_t>(it);
            it += 5; // 4 bytes len + 1 byte bits
            if (static_cast<size_t>(end - it) < len) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            it += len;
            break;
         }
         case etf::tag::SMALL_BIG_EXT: {
            if (it >= end) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            const uint8_t n = static_cast<uint8_t>(*it++);
            if (static_cast<size_t>(end - it) < 1 + n) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            it += 1 + n; // sign + digits
            break;
         }
         case etf::tag::LARGE_BIG_EXT: {
            if (end - it < 4) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            const uint32_t n = read_be<uint32_t>(it);
            it += 4;
            if (static_cast<size_t>(end - it) <= n) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            it += static_cast<size_t>(n) + 1; // sign + digits
            break;
         }
         case etf::tag::NIL_EXT: {
            // Empty list, 0 payload bytes
            break;
         }
         case etf::tag::SMALL_TUPLE_EXT: {
            if (it >= end) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            const uint8_t arity = static_cast<uint8_t>(*it++);
            for (uint8_t i = 0; i < arity; ++i) {
               op<Opts>(ctx, it, end);
               if (static_cast<bool>(ctx.error)) [[unlikely]] return;
            }
            break;
         }
         case etf::tag::LARGE_TUPLE_EXT: {
            if (end - it < 4) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            const uint32_t arity = read_be<uint32_t>(it);
            it += 4;
            if (arity > static_cast<size_t>(end - it)) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            for (uint32_t i = 0; i < arity; ++i) {
               op<Opts>(ctx, it, end);
               if (static_cast<bool>(ctx.error)) [[unlikely]] return;
            }
            break;
         }
         case etf::tag::LIST_EXT: {
            if (end - it < 4) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            const uint32_t len = read_be<uint32_t>(it);
            it += 4;
            if (static_cast<size_t>(end - it) <= len) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            for (uint32_t i = 0; i < len; ++i) {
               op<Opts>(ctx, it, end);
               if (static_cast<bool>(ctx.error)) [[unlikely]] return;
            }
            // Followed by tail term (typically NIL_EXT)
            op<Opts>(ctx, it, end);
            break;
         }
         case etf::tag::MAP_EXT: {
            if (end - it < 4) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            const uint32_t arity = read_be<uint32_t>(it);
            it += 4;
            if (arity > static_cast<size_t>(end - it) / 2) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            for (uint32_t i = 0; i < arity; ++i) {
               op<Opts>(ctx, it, end); // key
               if (static_cast<bool>(ctx.error)) [[unlikely]] return;
               op<Opts>(ctx, it, end); // val
               if (static_cast<bool>(ctx.error)) [[unlikely]] return;
            }
            break;
         }
         case etf::tag::ATOM_CACHE_REF: {
            if (it >= end) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            ++it;
            break;
         }
         case etf::tag::REFERENCE_EXT: {
            op<Opts>(ctx, it, end); // node
            if (static_cast<bool>(ctx.error)) [[unlikely]] return;
            if (end - it < 5) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            it += 5; // ID (4) + Creation (1)
            break;
         }
         case etf::tag::NEW_REFERENCE_EXT: {
            if (end - it < 2) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            const uint16_t n = read_be<uint16_t>(it);
            it += 2;
            op<Opts>(ctx, it, end); // node
            if (static_cast<bool>(ctx.error)) [[unlikely]] return;
            if (static_cast<size_t>(end - it) < 1 + (n * 4)) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            it += 1 + (n * 4); // Creation (1) + IDs (n*4)
            break;
         }
         case etf::tag::NEWER_REFERENCE_EXT: {
            if (end - it < 2) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            const uint16_t n = read_be<uint16_t>(it);
            it += 2;
            op<Opts>(ctx, it, end); // node
            if (static_cast<bool>(ctx.error)) [[unlikely]] return;
            if (static_cast<size_t>(end - it) < 4 + (n * 4)) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            it += 4 + (n * 4); // Creation (4) + IDs (n*4)
            break;
         }
         case etf::tag::PORT_EXT: {
            op<Opts>(ctx, it, end); // node
            if (static_cast<bool>(ctx.error)) [[unlikely]] return;
            if (end - it < 5) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            it += 5; // ID (4) + Creation (1)
            break;
         }
         case etf::tag::NEW_PORT_EXT: {
            op<Opts>(ctx, it, end); // node
            if (static_cast<bool>(ctx.error)) [[unlikely]] return;
            if (end - it < 8) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            it += 8; // ID (4) + Creation (4)
            break;
         }
         case etf::tag::V4_PORT_EXT: {
            op<Opts>(ctx, it, end); // node
            if (static_cast<bool>(ctx.error)) [[unlikely]] return;
            if (end - it < 12) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            it += 12; // ID (8) + Creation (4)
            break;
         }
         case etf::tag::PID_EXT: {
            op<Opts>(ctx, it, end); // node
            if (static_cast<bool>(ctx.error)) [[unlikely]] return;
            if (end - it < 9) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            it += 9; // ID (4) + Serial (4) + Creation (1)
            break;
         }
         case etf::tag::NEW_PID_EXT: {
            op<Opts>(ctx, it, end); // node
            if (static_cast<bool>(ctx.error)) [[unlikely]] return;
            if (end - it < 12) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            it += 12; // ID (4) + Serial (4) + Creation (4)
            break;
         }
         case etf::tag::EXPORT_EXT: {
            op<Opts>(ctx, it, end); // Module
            if (static_cast<bool>(ctx.error)) [[unlikely]] return;
            op<Opts>(ctx, it, end); // Function
            if (static_cast<bool>(ctx.error)) [[unlikely]] return;
            op<Opts>(ctx, it, end); // Arity
            break;
         }
         case etf::tag::NEW_FUN_EXT: {
            if (end - it < 4) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            const uint32_t total_size = read_be<uint32_t>(it);
            it += 4;
            if (total_size < 4) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }
            const uint32_t rem = total_size - 4;
            if (static_cast<size_t>(end - it) < rem) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            it += rem;
            break;
         }
         default:
            ctx.error = error_code::syntax_error;
            return;
         }
      }
   };
} // namespace glz
