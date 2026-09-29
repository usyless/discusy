#pragma once

#include <atomic>
#include <memory>
#include <version>

#if !defined(__cpp_lib_atomic_shared_ptr) || __cpp_lib_atomic_shared_ptr < 201711L
#include <concepts>
#include <mutex>
#include <shared_mutex>
#include <type_traits>
#include <utility>
#endif

namespace discusy {

#if defined(__cpp_lib_atomic_shared_ptr) && __cpp_lib_atomic_shared_ptr >= 201711L

template <typename T>
using atomic_shared_ptr = std::atomic<std::shared_ptr<T>>;

#else

template <typename T>
class atomic_shared_ptr {
public:
    using value_type = std::shared_ptr<T>;

    constexpr atomic_shared_ptr() noexcept = default;
    constexpr atomic_shared_ptr(std::nullptr_t) noexcept : ptr_{nullptr} {}

    explicit atomic_shared_ptr(std::shared_ptr<T> p) noexcept : ptr_{std::move(p)} {}

    template <typename Y>
    requires (!std::is_same_v<Y, T> && std::convertible_to<std::shared_ptr<Y>, std::shared_ptr<T>>)
    explicit atomic_shared_ptr(std::shared_ptr<Y> p) noexcept : ptr_{std::move(p)} {}

    atomic_shared_ptr(const atomic_shared_ptr&) = delete;
    atomic_shared_ptr& operator=(const atomic_shared_ptr&) = delete;
    atomic_shared_ptr(atomic_shared_ptr&&) = delete;
    atomic_shared_ptr& operator=(atomic_shared_ptr&&) = delete;

    atomic_shared_ptr& operator=(std::shared_ptr<T> desired) noexcept {
        store(std::move(desired));
        return *this;
    }

    template <typename Y>
    requires (!std::is_same_v<Y, T> && std::convertible_to<std::shared_ptr<Y>, std::shared_ptr<T>>)
    atomic_shared_ptr& operator=(std::shared_ptr<Y> desired) noexcept {
        store(std::move(desired));
        return *this;
    }

    atomic_shared_ptr& operator=(std::nullptr_t) noexcept {
        store(nullptr);
        return *this;
    }

    void store(std::shared_ptr<T> desired, std::memory_order = std::memory_order_seq_cst) noexcept {
        std::unique_lock lock{mtx_};
        ptr_ = std::move(desired);
    }

    template <typename Y>
    requires (!std::is_same_v<Y, T> && std::convertible_to<std::shared_ptr<Y>, std::shared_ptr<T>>)
    void store(std::shared_ptr<Y> desired, std::memory_order = std::memory_order_seq_cst) noexcept {
        std::unique_lock lock{mtx_};
        ptr_ = std::move(desired);
    }

    void store(std::nullptr_t, std::memory_order = std::memory_order_seq_cst) noexcept {
        std::unique_lock lock{mtx_};
        ptr_ = nullptr;
    }

    [[nodiscard]] std::shared_ptr<T> load(std::memory_order = std::memory_order_seq_cst) const noexcept {
        std::shared_lock lock{mtx_};
        return ptr_;
    }

    std::shared_ptr<T> exchange(std::shared_ptr<T> desired, std::memory_order = std::memory_order_seq_cst) noexcept {
        std::unique_lock lock{mtx_};
        return std::exchange(ptr_, std::move(desired));
    }

    template <typename Y>
    requires (!std::is_same_v<Y, T> && std::convertible_to<std::shared_ptr<Y>, std::shared_ptr<T>>)
    std::shared_ptr<T> exchange(std::shared_ptr<Y> desired, std::memory_order = std::memory_order_seq_cst) noexcept {
        std::unique_lock lock{mtx_};
        return std::exchange(ptr_, std::move(desired));
    }

    bool compare_exchange_strong(std::shared_ptr<T>& expected, std::shared_ptr<T> desired,
                                  std::memory_order = std::memory_order_seq_cst,
                                  std::memory_order = std::memory_order_seq_cst) noexcept {
        std::unique_lock lock{mtx_};
        if (ptr_ == expected) {
            ptr_ = std::move(desired);
            return true;
        }
        expected = ptr_;
        return false;
    }

    bool compare_exchange_weak(std::shared_ptr<T>& expected, std::shared_ptr<T> desired,
                                std::memory_order success = std::memory_order_seq_cst,
                                std::memory_order failure = std::memory_order_seq_cst) noexcept {
        return compare_exchange_strong(expected, std::move(desired), success, failure);
    }

    void reset() noexcept {
        store(nullptr);
    }

    explicit operator std::shared_ptr<T>() const noexcept {
        return load();
    }

    [[nodiscard]] bool is_lock_free() const noexcept {
        return false;
    }

    static constexpr bool is_always_lock_free = false;

private:
    mutable std::shared_mutex mtx_{};
    std::shared_ptr<T> ptr_{nullptr};
};

#endif

} // namespace discusy
