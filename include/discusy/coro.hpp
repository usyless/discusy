#pragma once

#include <boost/asio.hpp>
#include <boost/asio/experimental/parallel_group.hpp>

namespace discusy::coro {

using namespace boost::asio::experimental;

template <typename ...T>
using awaitable = boost::asio::awaitable<T...>;

template <typename T>
struct is_asio_awaitable : std::false_type {};

template <typename T, typename Executor>
struct is_asio_awaitable<boost::asio::awaitable<T, Executor>> : std::true_type {};

template <typename T>
inline constexpr bool is_asio_awaitable_v = is_asio_awaitable<std::remove_cvref_t<T>>::value;

template <typename T>
concept IsAwaiter = requires(T&& t) {
    { std::forward<T>(t).await_ready() } -> std::convertible_to<bool>;
    std::forward<T>(t).await_suspend(std::declval<std::coroutine_handle<>>());
    std::forward<T>(t).await_resume();
};

template <typename T>
concept HasMemberCoAwait = requires(T&& t) {
    std::forward<T>(t).operator co_await();
};

template <typename T>
concept HasFreeCoAwait = requires(T&& t) {
    operator co_await(std::forward<T>(t));
};

template <typename T>
struct get_awaiter { 
    using type = T; 
};

template <HasMemberCoAwait T>
struct get_awaiter<T> {
    using type = decltype(std::declval<T>().operator co_await());
};

template <HasFreeCoAwait T>
    requires (!HasMemberCoAwait<T>)
struct get_awaiter<T> {
    using type = decltype(operator co_await(std::declval<T>()));
};

template <typename T>
using get_awaiter_t = get_awaiter<std::remove_cvref_t<T>>::type;

template <typename T>
concept IsAwaitable = is_asio_awaitable_v<T> || IsAwaiter<get_awaiter_t<T>>;

[[nodiscard]] inline awaitable<void> ready_awaitable() {
    co_return;
}

template <typename T>
[[nodiscard]] inline coro::awaitable<T> ready_awaitable(T value) {
    co_return std::move(value);
}

[[nodiscard]] inline coro::awaitable<void> to_awaitable(auto awaitable) {
    co_await std::move(awaitable);
    co_return;
}

template <typename T>
[[nodiscard]] inline coro::awaitable<T> to_awaitable(auto awaitable) {
    co_return co_await std::move(awaitable);
}

}

namespace discusy::token {

// Use to co_await most expressions, lazy
inline constexpr auto deferred = boost::asio::deferred;
// Use when concrete types are needed instead of deferred chunks
inline constexpr auto explicit_awaitable = boost::asio::use_awaitable;
// Use when you wish a task to start running immediately and a result to be available later
inline constexpr auto eager = boost::asio::experimental::use_promise;
// Use when you wish a task to run completely disregarding the return value
inline constexpr auto detached = boost::asio::detached;

// Use to return [ec, value] from a coroutine instead of throwing exceptions
inline constexpr auto as_tuple = boost::asio::as_tuple;

// Use to co_await most expressions, lazy
inline constexpr auto t_deferred = as_tuple(deferred);
// Use when concrete types are needed instead of deferred chunks
inline constexpr auto t_explicit_awaitable = as_tuple(explicit_awaitable);
// Use when you wish a task to start running immediately and a result to be available later
inline constexpr auto t_eager = as_tuple(eager);

using boost::asio::cancel_after;
using boost::asio::bind_executor;
using boost::asio::bind_immediate_executor;
using boost::asio::bind_allocator;
using boost::asio::bind_cancellation_slot;

};