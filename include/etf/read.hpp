#pragma once

#include <charconv>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <variant>
#include <optional>
#include <memory>
#include <type_traits>
#include <chrono>

#include <glaze/core/context.hpp>
#include <glaze/core/opts.hpp>
#include <glaze/core/read.hpp>
#include <glaze/core/reflect.hpp>
#include <glaze/core/chrono.hpp>
#include <glaze/util/for_each.hpp>
#include <glaze/util/variant.hpp>
#include <glaze/util/bit_array.hpp>
#include <glaze/json/generic_fwd.hpp>

#include "tags.hpp"
#include "opts.hpp"
#include "skip.hpp"

namespace glz
{
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
            const uint8_t etf_version = static_cast<uint8_t>(*it++);
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
         if (etf::detail::is_nil(it, end)) {
            etf::detail::skip_nil(it, end);
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
         if constexpr (string_view_t<T>) {
            value = sv;
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
         const uint8_t tag = static_cast<uint8_t>(*it);
         if (tag == etf::tag::SMALL_INTEGER_EXT || tag == etf::tag::INTEGER_EXT ||
             tag == etf::tag::SMALL_BIG_EXT || tag == etf::tag::LARGE_BIG_EXT) {
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
                     visit<N>([&]<size_t I>() {
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
            auto [p, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), u);
            if (ec == std::errc{} && p == sv.data() + sv.size()) {
               value = static_cast<std::decay_t<T>>(u);
               return;
            }
            ctx.error = error_code::unexpected_enum;
            return;
         }
         ctx.error = error_code::syntax_error;
      }
   };

   // Nullable types (std::optional, std::unique_ptr, std::shared_ptr)
   template <nullable_like T>
   struct from<EETF, T> final
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto& value, is_context auto& ctx, auto& it, auto end)
      {
         if (it >= end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         if (etf::detail::is_nil(it, end)) {
            etf::detail::skip_nil(it, end);
            if constexpr (is_specialization_v<T, std::optional>) {
               value = std::nullopt;
            }
            else if constexpr (is_specialization_v<T, std::unique_ptr> || is_specialization_v<T, std::shared_ptr>) {
               value = nullptr;
            }
            else {
               value = {};
            }
            return;
         }

         if (!value) {
            if constexpr (is_specialization_v<T, std::optional>) {
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
         parse<EETF>::op<Opts>(*value, ctx, it, end);
      }
   };

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

         depth_guard guard{ctx};
         if (!guard) [[unlikely]] return;

         const uint8_t tag = static_cast<uint8_t>(*it++);
         if (tag == etf::tag::NIL_EXT) {
            if constexpr (growable) {
               value.clear();
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

            if constexpr (resizable<T>) {
               value.resize(len);
               for (size_t i = 0; i < len; ++i) {
                  parse<EETF>::op<Opts>(value[i], ctx, it, end);
                  if (static_cast<bool>(ctx.error)) [[unlikely]] return;
               }
            }
            else if constexpr (emplace_backable<T>) {
               value.clear();
               for (size_t i = 0; i < len; ++i) {
                  V elem{};
                  parse<EETF>::op<Opts>(elem, ctx, it, end);
                  if (static_cast<bool>(ctx.error)) [[unlikely]] return;
                  value.emplace_back(std::move(elem));
               }
            }
            else if constexpr (set_like) {
               value.clear();
               for (size_t i = 0; i < len; ++i) {
                  V elem{};
                  parse<EETF>::op<Opts>(elem, ctx, it, end);
                  if (static_cast<bool>(ctx.error)) [[unlikely]] return;
                  value.emplace(std::move(elem));
               }
            }
            else {
               for (size_t i = 0; i < len && i < value.size(); ++i) {
                  parse<EETF>::op<Opts>(value[i], ctx, it, end);
                  if (static_cast<bool>(ctx.error)) [[unlikely]] return;
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
            if constexpr (std::is_arithmetic_v<V> || std::is_enum_v<V> || std::is_same_v<V, std::byte>) {
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

               if constexpr (resizable<T>) {
                  value.resize(len);
                  for (size_t i = 0; i < len; ++i) {
                     value[i] = static_cast<V>(static_cast<uint8_t>(it[i]));
                  }
               }
               else if constexpr (emplace_backable<T>) {
                  value.clear();
                  for (size_t i = 0; i < len; ++i) {
                     value.emplace_back(static_cast<V>(static_cast<uint8_t>(it[i])));
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
            if constexpr (std::is_arithmetic_v<V> || std::is_enum_v<V> || std::is_same_v<V, std::byte>) {
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

               if constexpr (resizable<T>) {
                  value.resize(len);
                  for (size_t i = 0; i < len; ++i) {
                     value[i] = static_cast<V>(static_cast<uint8_t>(it[i]));
                  }
               }
               else if constexpr (emplace_backable<T>) {
                  value.clear();
                  for (size_t i = 0; i < len; ++i) {
                     value.emplace_back(static_cast<V>(static_cast<uint8_t>(it[i])));
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

            if constexpr (resizable<T>) {
               value.resize(len);
               for (size_t i = 0; i < len; ++i) {
                  parse<EETF>::op<Opts>(value[i], ctx, it, end);
                  if (static_cast<bool>(ctx.error)) [[unlikely]] return;
               }
            }
            else if constexpr (emplace_backable<T>) {
               value.clear();
               for (size_t i = 0; i < len; ++i) {
                  V elem{};
                  parse<EETF>::op<Opts>(elem, ctx, it, end);
                  if (static_cast<bool>(ctx.error)) [[unlikely]] return;
                  value.emplace_back(std::move(elem));
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
         const uint8_t tag = static_cast<uint8_t>(*it++);
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
         if constexpr (is_std_tuple<V>) {
            for_each<N>([&]<size_t I>() {
               if (I < arity) {
                  parse<EETF>::op<Opts>(std::get<I>(value), ctx, it, end);
               }
            });
         }
         else {
            for_each<N>([&]<size_t I>() {
               if (I < arity) {
                  parse<EETF>::op<Opts>(glz::get<I>(value), ctx, it, end);
               }
            });
         }
         for (uint32_t i = N; i < arity; ++i) {
            skip_value<EETF>::op<Opts>(ctx, it, end);
            if (static_cast<bool>(ctx.error)) return;
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
         depth_guard guard{ctx};
         if (!guard) [[unlikely]] return;

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

         value.clear();
         using Key = T::key_type;
         for (uint32_t i = 0; i < arity; ++i) {
            Key key{};
            parse<EETF>::op<Opts>(key, ctx, it, end);
            if (static_cast<bool>(ctx.error)) [[unlikely]] return;
            parse<EETF>::op<Opts>(value[key], ctx, it, end);
            if (static_cast<bool>(ctx.error)) [[unlikely]] return;
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
         if (it >= end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         depth_guard guard{ctx};
         if (!guard) [[unlikely]] {
            return;
         }

         const uint8_t tag = static_cast<uint8_t>(*it++);
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

         static constexpr auto N = reflect<T>::size;
         [[maybe_unused]] size_t read_count{};

         for (uint32_t key_idx = 0; key_idx < arity; ++key_idx) {
            if constexpr (Opts.partial_read) {
               if (read_count >= N) {
                  ctx.error = error_code::partial_read_complete;
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

               if (index < N) [[likely]] {
                  visit<N>(
                     [&]<size_t I>() {
                        static constexpr auto TargetKey = get<I>(reflect<T>::keys);
                        static constexpr auto Length = TargetKey.size();
                        if ((Length == key.size()) && compare<Length>(TargetKey.data(), key.data())) [[likely]] {
                           ++read_count;
                           if constexpr (skipped_by_meta<T, I, operation::parse>) {
                              skip_value<EETF>::op<Opts>(ctx, it, end);
                           }
                           else if constexpr (reflectable<T>) {
                              decltype(auto) member = get_member(value, get<I>(to_tie(value)));
                              using val_t = std::decay_t<decltype(member)>;
                              if constexpr (always_skipped<val_t>) {
                                 skip_value<EETF>::op<Opts>(ctx, it, end);
                              }
                              else {
                                 parse<EETF>::op<Opts>(member, ctx, it, end);
                              }
                           }
                           else {
                              decltype(auto) member = get_member(value, get<I>(reflect<T>::values));
                              using val_t = std::decay_t<decltype(member)>;
                              if constexpr (always_skipped<val_t>) {
                                 skip_value<EETF>::op<Opts>(ctx, it, end);
                              }
                              else {
                                 parse<EETF>::op<Opts>(member, ctx, it, end);
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
                              if (static_cast<bool>(ctx.error)) [[unlikely]] {
                                 return;
                              }
                           }
                        }
                     },
                     index);

                  if (static_cast<bool>(ctx.error)) [[unlikely]] {
                     return;
                  }

                  if constexpr (Opts.partial_read) {
                     if (read_count >= N) {
                        ctx.error = error_code::partial_read_complete;
                        return;
                     }
                  }
               }
               else [[unlikely]] {
                  if constexpr (Opts.error_on_unknown_keys) {
                     ctx.error = error_code::unknown_key;
                     return;
                  }
                  else {
                     skip_value<EETF>::op<Opts>(ctx, it, end);
                     if (static_cast<bool>(ctx.error)) [[unlikely]] {
                        return;
                     }
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
                  if (static_cast<bool>(ctx.error)) [[unlikely]] {
                     return;
                  }
               }
            }
         }

         if constexpr (Opts.error_on_missing_keys) {
            if (read_count < N) {
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
   }

   // Variants
   template <is_variant T>
      requires(not custom_read<T>)
   struct from<EETF, T> final
   {
      static constexpr size_t variant_size = std::variant_size_v<T>;

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
                     size_t r = variant_size;
                     [&]<size_t... J>(std::index_sequence<J...>) {
                        ((etf::detail::variant_is_object_alt<std::variant_alternative_t<J, T>> && r == variant_size
                             ? (void)(r = J)
                             : (void)0),
                         ...);
                     }(std::make_index_sequence<variant_size>{});
                     return r;
                  }();

                  using V = std::variant_alternative_t<target_idx, T>;
                  V v{};
                  parse<EETF>::template op<Opts>(v, ctx, it, end);
                  if (!static_cast<bool>(ctx.error)) {
                     value.template emplace<target_idx>(std::move(v));
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
                        if (key == tag_v<T>) {
                           using id_type = std::decay_t<decltype(ids_v<T>[0])>;
                           if constexpr (std::integral<id_type>) {
                              id_type id{};
                              parse<EETF>::template op<Opts>(id, ctx, scan, end);
                              if (static_cast<bool>(ctx.error)) [[unlikely]] {
                                 return;
                              }
                              tag_index = variant_id_to_index<T>::op(id);
                              tag_decoded = true;
                           }
                           else {
                              std::string_view id_view{};
                              if (!etf::detail::read_atom_or_str(ctx, scan, end, id_view)) [[unlikely]] {
                                 return;
                              }
                              tag_index = variant_id_to_index<T>::op(
                                 id_view.data(), id_view.data() + id_view.size(), id_view.size());
                              tag_decoded = true;
                           }
                           break;
                        }
                     }

                     if constexpr (variant_deduction_key_count<T> > 0) {
                        using dk = keys_wrapper<variant_deduction_keys<T>>;
                        static constexpr auto& H = hash_info<dk>;
                        const auto di = decode_hash_with_size<JSON, dk, H, H.type>::op(
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
                     if (static_cast<bool>(ctx.error)) [[unlikely]] {
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
                     visit<variant_size>(
                        [&]<size_t I>() {
                           using V = std::variant_alternative_t<I, T>;
                           V v{};
                           parse<EETF>::template op<Opts>(v, ctx, it, end);
                           if (!static_cast<bool>(ctx.error)) {
                              value.template emplace<I>(std::move(v));
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
                        if (!static_cast<bool>(ctx.error)) {
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
               if (!static_cast<bool>(ctx.error)) {
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
                  if (!static_cast<bool>(ctx.error)) {
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
         depth_guard guard{ctx};
         if (!guard) [[unlikely]] return;

         if (etf::detail::is_nil(it, end)) {
            etf::detail::skip_nil(it, end);
            value.data = nullptr;
            return;
         }

         const uint8_t tag = static_cast<uint8_t>(*it);
         switch (tag) {
         case etf::tag::SMALL_INTEGER_EXT:
         case etf::tag::INTEGER_EXT:
         case etf::tag::SMALL_BIG_EXT:
         case etf::tag::LARGE_BIG_EXT: {
            if constexpr (Mode == num_mode::u64) {
               uint64_t n{};
               if (!etf::detail::read_number(ctx, it, end, n)) return;
               value.data = n;
            }
            else if constexpr (Mode == num_mode::i64) {
               int64_t n{};
               if (!etf::detail::read_number(ctx, it, end, n)) return;
               value.data = n;
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
            if (!etf::detail::read_number(ctx, it, end, d)) return;
            value.data = d;
            break;
         }
         case etf::tag::SMALL_ATOM_UTF8_EXT:
         case etf::tag::SMALL_ATOM_EXT:
         case etf::tag::ATOM_UTF8_EXT:
         case etf::tag::ATOM_EXT: {
            std::string_view atom;
            if (!etf::detail::read_atom_or_str(ctx, it, end, atom)) return;
            if (atom == "true") {
               value.data = true;
            }
            else if (atom == "false") {
               value.data = false;
            }
            else if (atom == "nil" || atom == "null" || atom == "undefined") {
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
            if (static_cast<bool>(ctx.error)) return;
            value.data = std::move(s);
            break;
         }
         case etf::tag::NIL_EXT: {
            ++it;
            value.data = typename generic_json<Mode, MapType>::array_t{};
            break;
         }
         case etf::tag::LIST_EXT: {
            typename generic_json<Mode, MapType>::array_t arr;
            from<EETF, decltype(arr)>::template op<Opts>(arr, ctx, it, end);
            if (static_cast<bool>(ctx.error)) return;
            value.data = std::move(arr);
            break;
         }
         case etf::tag::MAP_EXT: {
            typename generic_json<Mode, MapType>::object_t obj;
            from<EETF, decltype(obj)>::template op<Opts>(obj, ctx, it, end);
            if (static_cast<bool>(ctx.error)) return;
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
         std::string_view str{};
         if (etf::detail::read_str(ctx, it, end, str)) {
            using Duration = std::remove_cvref_t<T>::duration;
            using Period = Duration::period;
            if constexpr (std::ratio_equal_v<Period, std::ratio<86400>>) {
               if (str.size() == 10) {
                  std::chrono::year_month_day ymd{};
                  chrono_detail::parse_ymd(str, ymd, ctx.error);
                  if (static_cast<bool>(ctx.error)) [[unlikely]] return;
                  value = std::chrono::time_point_cast<Duration>(std::chrono::sys_days{ymd});
                  return;
               }
            }
            chrono_detail::parse_iso8601(str, value, ctx.error);
            return;
         }

         // Numeric timestamp in seconds or milliseconds
         int64_t num{};
         if (etf::detail::read_number(ctx, it, end, num)) {
            using Duration = std::remove_cvref_t<T>::duration;
            if (num > 100000000000LL) {
               value = std::chrono::time_point_cast<Duration>(
                  std::chrono::system_clock::time_point{std::chrono::milliseconds{num}});
            }
            else {
               value = std::chrono::time_point_cast<Duration>(
                  std::chrono::system_clock::time_point{std::chrono::seconds{num}});
            }
            return;
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
         if (static_cast<bool>(ctx.error)) [[unlikely]] {
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
         if (static_cast<bool>(ctx.error)) [[unlikely]] {
            return;
         }
         value.str = T{std::to_address(it_start), static_cast<size_t>(it - it_start)};
      }
   };

   // Top-level read helper functions for ETF
   template <read_supported<EETF> T, class Buffer>
   [[nodiscard]] inline error_ctx read_etf(T& value, Buffer&& buffer)
   {
      return read<etf_opts>(value, std::forward<Buffer>(buffer));
   }

   template <read_supported<EETF> T, class Buffer>
   [[nodiscard]] inline error_ctx read_etf(T& value, Buffer&& buffer, is_context auto&& ctx)
   {
      return read<etf_opts>(value, std::forward<Buffer>(buffer), ctx);
   }
} // namespace glz
