#pragma once

#include <random>
#include <span>
#include <cstdint>
#include <array>
#include <atomic>

namespace discusy::rnd {

[[nodiscard]] inline double jitter() noexcept {
    static thread_local std::mt19937 gen{std::random_device{}()};
    static std::uniform_real_distribution<double> dis(0.05,0.95);
    return dis(gen);
}

/**
 * Generate a 128 bit nonce for discords needs, just to be unique, not secure
 */
inline void nonce(std::span<char, 32> str) noexcept {
    using State = std::array<std::uint64_t, 2>;
    static std::atomic_uint64_t thread_counter{0};

    thread_local auto state = []() -> State {
        std::random_device rd;
        return State{0, ((static_cast<std::uint64_t>(rd()) << 32) | rd()) ^ (++thread_counter)};
    }();

    if (++state[0] == 0) {
        ++state[1];
    }

    static constexpr std::array<char, 16> hex_digits{
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
    };
    for (std::size_t i = 0; i < 2; ++i) {
        const auto val = state[i];
        for (std::size_t j = 0; j < 16; ++j) {
            str[i * 16 + j] = hex_digits[(val >> (j * 4)) & 0x0F];
        }
    }
}

class Random64 {
private:
    uint64_t state;
public:
    constexpr explicit Random64(uint64_t seed) noexcept : state(seed) {}

    [[nodiscard]] constexpr int64_t next() noexcept {
        uint64_t z = (state += 0x9e3779b97f4a7c15ULL);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return static_cast<int64_t>(z ^ (z >> 31));
    }
};

}