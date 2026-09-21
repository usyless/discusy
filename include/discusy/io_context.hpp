#pragma once

#include <chrono>
#include <cstdint>
#include <exception>
#include <functional>
#include <utility>

#include <boost/asio.hpp>
#include <boost/asio/experimental/promise.hpp>
#include <boost/asio/experimental/use_promise.hpp>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include "coro.hpp"
#include "log.hpp" // IWYU pragma: keep
#include "asio_helpers.hpp"

namespace discusy::ctx {

enum class launch : std::uint8_t {
    inline_if_on_executor,
    continuation,
    fresh,
};

namespace detail {

template <launch L, boost::asio::execution::executor Executor>
[[nodiscard]] inline auto with_launch(const Executor& ex) {
    if constexpr (L == launch::inline_if_on_executor) {
        return ex;
    } else if constexpr (L == launch::continuation) {
        return boost::asio::prefer(
            boost::asio::prefer(ex, boost::asio::execution::blocking_t::never),
            boost::asio::execution::relationship_t::continuation);
    } else {
        return boost::asio::prefer(
            boost::asio::prefer(ex, boost::asio::execution::blocking_t::never),
            boost::asio::execution::relationship_t::fork);
    }
}

template <typename T>
struct completion_signature_helper {
    using type = void(T);
};

template <>
struct completion_signature_helper<void> {
    using type = void();
};

template <typename R, typename... Args>
struct completion_signature_helper<R(Args...)> {
    using type = R(Args...);
};

template <typename T>
using completion_signature_t = completion_signature_helper<T>::type;

template <typename T>
struct is_known_token : std::false_type {};

template <typename... T>
struct is_known_token<boost::asio::use_awaitable_t<T...>> : std::true_type {};

template <typename... T>
struct is_known_token<boost::asio::experimental::use_promise_t<T...>> : std::true_type {};

template <>
struct is_known_token<boost::asio::deferred_t> : std::true_type {};

template <>
struct is_known_token<boost::asio::detached_t> : std::true_type {};

template <typename Inner>
struct is_known_token<boost::asio::as_tuple_t<Inner>> : std::true_type {};

template <typename Target, typename Executor>
struct is_known_token<boost::asio::executor_binder<Target, Executor>> : std::true_type {};

template <typename Target, typename Allocator>
struct is_known_token<boost::asio::allocator_binder<Target, Allocator>> : std::true_type {};

template <typename Target, typename CancellationSlot>
struct is_known_token<boost::asio::cancellation_slot_binder<Target, CancellationSlot>> : std::true_type {};

template <typename Inner, typename... More>
struct is_known_token<boost::asio::cancel_after_t<Inner, More...>> : std::true_type {};

template <typename T>
inline constexpr bool is_known_token_v = is_known_token<std::remove_cvref_t<T>>::value;

}

struct detached_log_t {
    void operator()(const std::exception_ptr& e) const noexcept {
        if (!e) return;
        #ifdef DISCUSY_LOGGING
        try {
            std::rethrow_exception(e);
        } catch (const std::exception& ex) {
            log::Logger{}("Detached coroutine terminated with exception: {}", ex.what());
        } catch (...) {
            log::Logger{}("Detached coroutine terminated with unknown exception");
        }
        #endif
    }

    template <typename T>
    void operator()(const std::exception_ptr& e, T&&) const noexcept {
        (*this)(e);
    }
};

inline constexpr detached_log_t detached_log{};

class io_context {
public:
    boost::asio::io_context io_ctx_;
    using strand_t = boost::asio::strand<boost::asio::io_context::executor_type>;;
    using executor_t = boost::asio::io_context::executor_type;

    template <boost::asio::execution::executor Executor>
    using timer_t = boost::asio::basic_waitable_timer<
        std::chrono::steady_clock,
        boost::asio::wait_traits<std::chrono::steady_clock>,
        Executor
    >;

    using strand_timer_t = timer_t<strand_t>;
    using executor_timer_t = timer_t<executor_t>;

    template <typename type, boost::asio::execution::executor Executor>
    using resolver_t = boost::asio::ip::basic_resolver<type, Executor>;

    using strand_tcp_resolver_t = resolver_t<boost::asio::ip::tcp, strand_t>;
    using strand_udp_resolver_t = resolver_t<boost::asio::ip::udp, strand_t>;

    using strand_udp_socket_t = boost::asio::basic_datagram_socket<
        boost::asio::ip::udp, strand_t
    >;

    using strand_tcp_stream_t = boost::beast::basic_stream<
        boost::asio::ip::tcp,
        ctx::io_context::strand_t,
        boost::beast::unlimited_rate_policy
    >;

    using dct_t = boost::asio::default_completion_token_t<executor_t>;

    executor_t executor_;

    io_context(auto&&... args) : io_ctx_{std::forward<decltype(args)>(args)...}, executor_{io_ctx_.get_executor()} {}

    [[nodiscard]] auto* operator->(this auto&& self) noexcept { return std::addressof(self.io_ctx_); }

    [[nodiscard]] const auto& executor() const noexcept { return executor_; }

    [[nodiscard]] auto make_strand() {
        return boost::asio::make_strand(io_ctx_);
    }

    // You must ensure safety with cancellation
    template <typename Rep, typename Period, discusy::asio::ctf<> CompletionToken = ctx::io_context::dct_t>
    [[nodiscard]] auto sleep(std::chrono::duration<Rep, Period> time, CompletionToken&& token = ctx::io_context::dct_t()) {
        return boost::asio::async_initiate<CompletionToken, void(boost::system::error_code)>(
            [](discusy::asio::chf<> auto&& handler, io_context* self, auto&& time) {
                auto ex = boost::asio::get_associated_executor(handler, self->executor_);
                auto alloc = boost::asio::get_associated_allocator(
                    handler, 
                    boost::asio::recycling_allocator<void>{}
                );

                auto timer = std::allocate_shared<boost::asio::steady_timer>(alloc, ex, time);
                
                timer->async_wait(
                    boost::asio::bind_allocator(alloc, boost::asio::consign(std::forward<decltype(handler)>(handler), timer))
                );
            },
            token,
            this,
            std::move(time)
        );
    }

    template <launch L = launch::continuation, typename F, boost::asio::execution::executor Executor>
    requires (std::invocable<F&> && coro::IsAwaitable<std::invoke_result_t<F&>>)
    static auto co_launch_detached(F&& f, Executor&& executor) {
        return boost::asio::co_spawn(detail::with_launch<L>(std::forward<Executor>(executor)), std::forward<F>(f), detached_log_t{});
    }

    template <launch L = launch::continuation, typename F>
    requires (std::invocable<F&> && coro::IsAwaitable<std::invoke_result_t<F&>>)
    auto co_launch_detached(F&& f) {
        return co_launch_detached<L>(std::forward<F>(f), executor_);
    }

    template <launch L = launch::continuation, typename F, boost::asio::execution::executor Executor, typename Token>
    requires (std::invocable<F&> && coro::IsAwaitable<std::invoke_result_t<F&>>)
    static auto co_launch(F&& f, Executor&& executor, Token&& token) {
        return boost::asio::co_spawn(detail::with_launch<L>(std::forward<Executor>(executor)), std::forward<F>(f), std::forward<Token>(token));
    }

    template <launch L = launch::continuation, typename F, typename Token>
    requires (std::invocable<F&> && coro::IsAwaitable<std::invoke_result_t<F&>>)
    auto co_launch(F&& f, Token token) {
        return co_launch<L>(std::forward<F>(f), executor_, std::move(token));
    }

    template <launch L = launch::inline_if_on_executor, typename F, boost::asio::execution::executor Executor>
    requires (std::invocable<F&> && coro::IsAwaitable<std::invoke_result_t<F&>>)
    [[nodiscard]] static auto co_launch_promise(F&& f, Executor&& executor) {
        return boost::asio::co_spawn(
            detail::with_launch<L>(std::forward<Executor>(executor)),
            std::forward<F>(f),
            boost::asio::experimental::use_promise
        );
    }

    template <launch L = launch::inline_if_on_executor, typename F>
    requires (std::invocable<F&> && coro::IsAwaitable<std::invoke_result_t<F&>>)
    [[nodiscard]] auto co_launch_promise(F&& f) {
        return co_launch_promise<L>(std::forward<F>(f), executor_);
    }

    template <launch L = launch::continuation, typename F, boost::asio::execution::executor Executor>
    requires (std::invocable<F&> && !coro::IsAwaitable<std::invoke_result_t<F&>>)
    static auto submit(F&& f, Executor&& executor) {
        if constexpr (L == launch::inline_if_on_executor) {
            return boost::asio::dispatch(std::forward<Executor>(executor), std::forward<F>(f));
        } else if constexpr (L == launch::continuation) {
            return boost::asio::defer(std::forward<Executor>(executor), std::forward<F>(f));
        } else {
            return boost::asio::post(std::forward<Executor>(executor), std::forward<F>(f));
        }
    }

    template <launch L = launch::continuation, typename F>
    requires (std::invocable<F&> && !coro::IsAwaitable<std::invoke_result_t<F&>>)
    auto submit(F&& f) {
        return submit<L>(std::forward<F>(f), executor_);
    }

    template <typename F, boost::asio::execution::executor Executor>
    static auto post(F&& f, Executor&& executor) {
        return submit<launch::fresh>(std::forward<F>(f), std::forward<Executor>(executor));
    }

    template <typename F>
    auto post(F&& f) {
        return submit<launch::fresh>(std::forward<F>(f), executor_);
    }

    template <typename F, boost::asio::execution::executor Executor>
    static auto defer(F&& f, Executor&& executor) {
        return submit<launch::continuation>(std::forward<F>(f), std::forward<Executor>(executor));
    }

    template <typename F>
    auto defer(F&& f) {
        return submit<launch::continuation>(std::forward<F>(f), executor_);
    }

    template <typename F, boost::asio::execution::executor Executor>
    static auto dispatch(F&& f, Executor&& executor) {
        return submit<launch::inline_if_on_executor>(std::forward<F>(f), std::forward<Executor>(executor));
    }

    template <typename F>
    auto dispatch(F&& f) {
        return submit<launch::inline_if_on_executor>(std::forward<F>(f), executor_);
    }

    template <typename SigOrType = void, launch L = launch::inline_if_on_executor,
              boost::asio::execution::executor Executor, typename CompletionToken,
              typename Func, typename... Args>
    requires (detail::is_known_token_v<CompletionToken>)
    [[nodiscard]] static auto cb_to_coro(Executor&& executor, CompletionToken&& token, Func&& func, Args&&... args) {
        using Sig = detail::completion_signature_t<SigOrType>;
        return boost::asio::async_initiate<CompletionToken, Sig>(
            [](auto&& handler, auto exec, auto&& func, auto&&... bound_args) mutable {
                auto target_exec = boost::asio::get_associated_executor(handler, exec);
                auto alloc = boost::asio::get_associated_allocator(
                    handler,
                    boost::asio::recycling_allocator<void>{}
                );

                auto invoke = [func = std::forward<decltype(func)>(func),
                               handler = std::forward<decltype(handler)>(handler),
                               target_exec,
                               alloc,
                               work = boost::asio::make_work_guard(target_exec),
                               ...bound_args = std::forward<decltype(bound_args)>(bound_args)]() mutable {
                    std::invoke(
                        std::move(func),
                        std::move(bound_args)...,
                        [handler = std::move(handler), target_exec, alloc, work = std::move(work)](auto&&... result_args) mutable {
                            boost::asio::dispatch(
                                target_exec,
                                boost::asio::bind_allocator(
                                    alloc,
                                    [h = std::move(handler), work = std::move(work), ...res = std::forward<decltype(result_args)>(result_args)]() mutable {
                                        std::move(h)(std::forward<decltype(res)>(res)...);
                                    }
                                )
                            );
                        }
                    );
                };

                if constexpr (L == launch::inline_if_on_executor) {
                    std::move(invoke)();
                } else {
                    submit<L>(boost::asio::bind_allocator(alloc, std::move(invoke)), exec);
                }
            },
            token,
            std::forward<Executor>(executor),
            std::forward<Func>(func),
            std::forward<Args>(args)...
        );
    }

    template <typename SigOrType = void, launch L = launch::inline_if_on_executor,
              boost::asio::execution::executor Executor,
              typename Func, typename... Args>
    requires (boost::asio::execution::executor<std::remove_cvref_t<Executor>> &&
              !detail::is_known_token_v<Func>)
    [[nodiscard]] static auto cb_to_coro(Executor&& executor, Func&& func, Args&&... args) {
        return cb_to_coro<SigOrType, L>(
            std::forward<Executor>(executor),
            boost::asio::default_completion_token_t<std::decay_t<Executor>>{},
            std::forward<Func>(func),
            std::forward<Args>(args)...
        );
    }

    template <typename SigOrType = void, launch L = launch::inline_if_on_executor,
              typename CompletionToken, typename Func, typename... Args>
    requires (detail::is_known_token_v<CompletionToken>)
    [[nodiscard]] auto cb_to_coro(CompletionToken&& token, Func&& func, Args&&... args) {
        return cb_to_coro<SigOrType, L>(
            executor_,
            std::forward<CompletionToken>(token),
            std::forward<Func>(func),
            std::forward<Args>(args)...
        );
    }

    template <typename SigOrType = void, launch L = launch::inline_if_on_executor,
              typename Func, typename... Args>
    requires (!boost::asio::execution::executor<std::remove_cvref_t<Func>> &&
              !detail::is_known_token_v<Func>)
    [[nodiscard]] auto cb_to_coro(Func&& func, Args&&... args) {
        return cb_to_coro<SigOrType, L>(
            executor_,
            dct_t{},
            std::forward<Func>(func),
            std::forward<Args>(args)...
        );
    }

    template <typename F, typename... Args>
    requires ( std::invocable<F&, Args...> )
    void handle_callback_coro_normal(F&& cb, Args&&... args) noexcept {
        try {
            if constexpr (coro::IsAwaitable<std::invoke_result_t<F&, Args...>>) {
                co_launch_detached([cb = std::forward<F>(cb), ...args = std::forward<Args>(args)]() mutable {
                    return std::invoke(std::move(cb), std::move(args)...);
                });
            } else {
                submit<launch::continuation>([cb = std::forward<F>(cb), ...args = std::forward<Args>(args)]() mutable -> void {
                    try {
                        std::invoke(std::move(cb), std::move(args)...);
                    }
                    #ifdef DISCUSY_LOGGING
                    catch (const std::exception& e) {
                        log::Logger{}("handle_callback_coro_normal async post exception: {}", e.what());
                    }
                    #endif
                    catch (...) {
                        #ifdef DISCUSY_LOGGING
                        log::Logger{}("handle_callback_coro_normal unknown async post exception");
                        #endif
                    }
                });
            }
        }
        #ifdef DISCUSY_LOGGING
        catch (const std::exception& e) {
            log::Logger{}("handle_callback_coro_normal exception: {}", e.what());
        }
        #endif
        catch (...) {
            #ifdef DISCUSY_LOGGING
            log::Logger{}("handle_callback_coro_normal unknown exception");
            #endif
        }
    }
};

}
