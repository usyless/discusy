#pragma once

#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/unordered/concurrent_flat_map.hpp>

#include "types.hpp"
#include "enum_helpers.hpp"
#include "intents.hpp"
#include "events.hpp"
#include "gateway_events.hpp" // IWYU pragma: keep
#include "state.hpp"
#include "callback.hpp"

namespace discusy {

enum class cache_flags : std::uint8_t {
    none              = 0,
    guilds            = 1ULL << 0,
    channels          = 1ULL << 1, // includes threads
    members           = 1ULL << 2,
    users             = 1ULL << 3,
    roles             = 1ULL << 4,
    emojis            = 1ULL << 5,
    voice_states      = 1ULL << 6, // who is in which voice channel

    members_and_users = (1ULL << 2) | (1ULL << 3),
    all               = (1ULL << 7) - 1,
};

namespace cache_detail {

[[nodiscard]] constexpr std::uint64_t splitmix64(std::uint64_t x) noexcept {
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return x;
}

} // namespace cache_detail

struct guild_scoped_id {
    snowflake guild_id{};
    snowflake id{};

    [[nodiscard]] constexpr bool operator==(const guild_scoped_id& other) const noexcept = default;
    [[nodiscard]] constexpr auto operator<=>(const guild_scoped_id& other) const noexcept = default;

    [[nodiscard]] friend constexpr std::size_t hash_value(const guild_scoped_id& k) noexcept {
        std::uint64_t h = (k.guild_id.value ^ 0x9e3779b97f4a7c15ULL) * 0xbf58476d1ce4e5b9ULL;
        h ^= k.id.value + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        return static_cast<std::size_t>(cache_detail::splitmix64(h));
    }
};

} // namespace discusy

template <>
struct std::hash<discusy::guild_scoped_id> {
    [[nodiscard]] constexpr std::size_t operator()(const discusy::guild_scoped_id& k) const noexcept {
        return hash_value(k);
    }
};

namespace discusy {

class cache_manager;

namespace cache_detail {

struct snowflake_hash {
    using is_avalanching = std::true_type;

    [[nodiscard]] constexpr std::size_t operator()(const snowflake s) const noexcept {
        return static_cast<std::size_t>(splitmix64(s.value));
    }
};

struct guild_scoped_id_hash {
    using is_avalanching = std::true_type;

    [[nodiscard]] constexpr std::size_t operator()(const guild_scoped_id& k) const noexcept {
        return hash_value(k);
    }
};

struct no_group {
    [[nodiscard]] constexpr bool operator==(const no_group&) const noexcept = default;
    [[nodiscard]] constexpr auto operator<=>(const no_group&) const noexcept = default;
};

template <typename T> struct hash_for { using type = std::hash<T>; };
template <> struct hash_for<snowflake> { using type = snowflake_hash; };
template <> struct hash_for<guild_scoped_id> { using type = guild_scoped_id_hash; };
template <> struct hash_for<void> { using type = void; };
template <typename T> using hash_for_t = hash_for<T>::type;

struct by_key {};
struct by_group {};

} // namespace cache_detail

template <typename Key, typename Value, typename GroupKey = void>
class cache {
public:
    static constexpr bool grouped = true;

    using key_type = Key;
    using value_type = Value;
    using group_key_type = GroupKey;
    using ptr = std::shared_ptr<const Value>;
    using snapshot_t = std::vector<std::pair<Key, ptr>>;

    cache() = default;

    cache(const cache&) = delete;
    cache& operator=(const cache&) = delete;
    cache(cache&&) = delete;
    cache& operator=(cache&&) = delete;

    ~cache() noexcept = default;

    [[nodiscard]] bool enabled() const noexcept {
        return enabled_.load(std::memory_order_acquire);
    }

    void set_enabled(const bool value) {
        set_enabled_impl(value, true);
    }

    // null if absent
    [[nodiscard]] ptr get(const Key& key) const {
        std::shared_lock lock{mtx_};
        const auto& key_idx = container_.template get<by_key>();
        const auto it = key_idx.find(key);
        return (it == key_idx.end()) ? ptr{} : it->value;
    }

    [[nodiscard]] opt<Value> copy(const Key& key) const {
        const auto p = get(key);
        if (!p) return std::nullopt;
        return *p;
    }

    [[nodiscard]] bool contains(const Key& key) const {
        std::shared_lock lock{mtx_};
        return container_.template get<by_key>().contains(key);
    }

    [[nodiscard]] std::size_t size() const {
        std::shared_lock lock{mtx_};
        return container_.size();
    }

    [[nodiscard]] bool empty() const {
        std::shared_lock lock{mtx_};
        return container_.empty();
    }

    [[nodiscard]] std::vector<Key> keys() const {
        std::vector<Key> out;
        {
            std::shared_lock lock{mtx_};
            out.reserve(container_.size());
            for (const auto& entry : container_.template get<by_key>()) {
                if (entry.value) out.emplace_back(entry.key);
            }
        }
        return out;
    }

    [[nodiscard]] std::vector<ptr> all() const {
        std::vector<ptr> out;
        {
            std::shared_lock lock{mtx_};
            out.reserve(container_.size());
            for (const auto& entry : container_.template get<by_key>()) {
                if (entry.value) out.emplace_back(entry.value);
            }
        }
        return out;
    }

    [[nodiscard]] snapshot_t snapshot() const {
        snapshot_t out;
        {
            std::shared_lock lock{mtx_};
            out.reserve(container_.size());
            for (const auto& entry : container_.template get<by_key>()) {
                if (entry.value) out.emplace_back(entry.key, entry.value);
            }
        }
        return out;
    }

    template <typename F>
    requires ( std::invocable<F&, const Key&, const ptr&> || std::invocable<F&, const Key&, const Value&> )
    void for_each(F&& f) const {
        for (const auto& [key, value] : snapshot()) {
            if constexpr (std::invocable<F&, const Key&, const ptr&>) {
                if constexpr (std::is_same_v<std::invoke_result_t<F&, const Key&, const ptr&>, bool>) {
                    if (!std::invoke(f, key, value)) return;
                } else {
                    std::invoke(f, key, value);
                }
            } else {
                if (value) {
                    if constexpr (std::is_same_v<std::invoke_result_t<F&, const Key&, const Value&>, bool>) {
                        if (!std::invoke(f, key, *value)) return;
                    } else {
                        std::invoke(f, key, *value);
                    }
                }
            }
        }
    }

    [[nodiscard]] std::vector<Key> keys_in_group(const group_key_type& group) const {
        std::vector<Key> out;
        {
            std::shared_lock lock{mtx_};
            const auto& group_idx = container_.template get<by_group>();
            const auto [first, last] = group_idx.equal_range(group);
            if (first != last) {
                out.reserve(static_cast<std::size_t>(std::distance(first, last)));
                for (auto it = first; it != last; ++it) {
                    if (it->value) out.emplace_back(it->key);
                }
            }
        }
        return out;
    }

    [[nodiscard]] std::vector<ptr> in_group(const group_key_type& group) const {
        std::vector<ptr> out;
        {
            std::shared_lock lock{mtx_};
            const auto& group_idx = container_.template get<by_group>();
            const auto [first, last] = group_idx.equal_range(group);
            if (first != last) {
                out.reserve(static_cast<std::size_t>(std::distance(first, last)));
                for (auto it = first; it != last; ++it) {
                    if (it->value) out.emplace_back(it->value);
                }
            }
        }
        return out;
    }

    [[nodiscard]] snapshot_t snapshot_group(const group_key_type& group) const {
        snapshot_t out;
        {
            std::shared_lock lock{mtx_};
            const auto& group_idx = container_.template get<by_group>();
            const auto [first, last] = group_idx.equal_range(group);
            if (first != last) {
                out.reserve(static_cast<std::size_t>(std::distance(first, last)));
                for (auto it = first; it != last; ++it) {
                    if (it->value) out.emplace_back(it->key, it->value);
                }
            }
        }
        return out;
    }

    [[nodiscard]] std::size_t group_size(const group_key_type& group) const {
        std::shared_lock lock{mtx_};
        return container_.template get<by_group>().count(group);
    }

    template <typename F>
    requires ( std::invocable<F&, const Key&, const ptr&> || std::invocable<F&, const Key&, const Value&> )
    void for_each_in_group(const group_key_type& group, F&& f) const {
        for (const auto& [key, value] : snapshot_group(group)) {
            if constexpr (std::invocable<F&, const Key&, const ptr&>) {
                if constexpr (std::is_same_v<std::invoke_result_t<F&, const Key&, const ptr&>, bool>) {
                    if (!std::invoke(f, key, value)) return;
                } else {
                    std::invoke(f, key, value);
                }
            } else {
                if (value) {
                    if constexpr (std::is_same_v<std::invoke_result_t<F&, const Key&, const Value&>, bool>) {
                        if (!std::invoke(f, key, *value)) return;
                    } else {
                        std::invoke(f, key, *value);
                    }
                }
            }
        }
    }

    void insert(const Key& key, Value value, const group_key_type& group) {
        insert(key, std::make_shared<const Value>(std::move(value)), group);
    }

    void insert(const Key& key, ptr value, const group_key_type& group) {
        if (!value) {
            erase(key);
            return;
        }

        ptr old;
        {
            std::scoped_lock lock{mtx_};
            insert_locked(key, std::move(value), group, old);
        }
    }

    template <typename F>
    requires ( std::invocable<F&, Value&> )
    bool modify(const Key& key, F&& f) {
        ptr old;
        {
            std::scoped_lock lock{mtx_};
            auto& key_idx = container_.template get<by_key>();
            const auto it = key_idx.find(key);
            if ((it == key_idx.end()) || !it->value) return false;

            auto updated = std::make_shared<Value>(*it->value);
            std::invoke(f, *updated);
            old = std::exchange(it->value, std::move(updated));
        }
        return true;
    }

    template <typename F>
    requires ( std::invocable<F&, Value&> )
    void upsert(const Key& key, const group_key_type& group, F&& f) {
        ptr old;
        {
            std::scoped_lock lock{mtx_};
            upsert_locked(key, group, std::forward<F>(f), old);
        }
    }

    bool erase(const Key& key) {
        ptr old;
        {
            std::scoped_lock lock{mtx_};
            auto& key_idx = container_.template get<by_key>();
            const auto it = key_idx.find(key);
            if (it == key_idx.end()) return false;
            old = std::move(it->value);
            key_idx.erase(it);
        }
        return true;
    }

    void erase_group(const group_key_type& group) {
        std::vector<ptr> dropped;
        {
            std::scoped_lock lock{mtx_};
            erase_group_locked(group, dropped);
        }
    }

    void clear() {
        container_t tmp;
        {
            std::scoped_lock lock{mtx_};
            container_.swap(tmp);
        }
    }

    void reserve(const std::size_t n) {
        std::scoped_lock lock{mtx_};
        container_.template get<by_key>().reserve(n);
        container_.template get<by_group>().reserve(n);
    }

private:
    friend class cache_manager;

    using by_key = cache_detail::by_key;
    using by_group = cache_detail::by_group;

    using key_hash = cache_detail::hash_for_t<Key>;
    using group_hash = cache_detail::hash_for_t<GroupKey>;

    struct entry_t {
        Key key;
        group_key_type group;
        mutable ptr value{};
    };

    using container_t = boost::multi_index_container<
        entry_t,
        boost::multi_index::indexed_by<
            boost::multi_index::hashed_unique<
                boost::multi_index::tag<by_key>,
                boost::multi_index::member<entry_t, Key, &entry_t::key>,
                key_hash
            >,
            boost::multi_index::hashed_non_unique<
                boost::multi_index::tag<by_group>,
                boost::multi_index::member<entry_t, group_key_type, &entry_t::group>,
                group_hash
            >
        >
    >;

    void set_enabled_hook(void (*fn)(void*), void* ctx) noexcept {
        on_enabled_change_ = fn;
        on_enabled_change_ctx_ = ctx;
    }

    void set_enabled_impl(const bool value, const bool notify) {
        if (!value) {
            container_t tmp;
            {
                std::scoped_lock lock{mtx_};
                enabled_.store(false, std::memory_order_release);
                container_.swap(tmp);
            }
        } else {
            enabled_.store(true, std::memory_order_release);
        }

        if (notify && on_enabled_change_) {
            on_enabled_change_(on_enabled_change_ctx_);
        }
    }

    void insert_locked(const Key& key, ptr value, const group_key_type& group, ptr& old) {
        auto& key_idx = container_.template get<by_key>();
        const auto it = key_idx.find(key);
        if (it != key_idx.end()) {
            if (it->group != group) {
                auto new_group = group;
                auto prev_group = it->group;
                key_idx.modify(
                    it,
                    [&](entry_t& entry) { entry.group = std::move(new_group); },
                    [&](entry_t& entry) { entry.group = std::move(prev_group); }
                );
            }
            old = std::exchange(it->value, std::move(value));
        } else {
            key_idx.emplace(entry_t{key, group, std::move(value)});
        }
    }

    template <typename F>
    void upsert_locked(const Key& key, const group_key_type& group, F&& f, ptr& old) {
        auto& key_idx = container_.template get<by_key>();
        const auto it = key_idx.find(key);
        if (it != key_idx.end()) {
            auto updated = it->value ? std::make_shared<Value>(*it->value) : std::make_shared<Value>();
            std::invoke(f, *updated);
            if (it->group != group) {
                auto new_group = group;
                auto prev_group = it->group;
                key_idx.modify(
                    it,
                    [&](entry_t& entry) { entry.group = std::move(new_group); },
                    [&](entry_t& entry) { entry.group = std::move(prev_group); }
                );
            }
            old = std::exchange(it->value, std::move(updated));
        } else {
            auto updated = std::make_shared<Value>();
            std::invoke(f, *updated);
            key_idx.emplace(entry_t{key, group, std::move(updated)});
        }
    }

    void erase_group_locked(const group_key_type& group, std::vector<ptr>& dropped) {
        auto& group_idx = container_.template get<by_group>();
        auto [it, last] = group_idx.equal_range(group);
        while (it != last) {
            ptr tmp = std::move(it->value);
            it = group_idx.erase(it);
            if (tmp) {
                try {
                    dropped.emplace_back(std::move(tmp));
                } catch (...) {} // NOLINT(bugprone-empty-catch)
            }
        }
    }

    bool insert_if_enabled(const Key& key, Value value, const group_key_type& group) {
        if (!enabled()) return false;
        return insert_if_enabled(key, std::make_shared<const Value>(std::move(value)), group);
    }

    bool insert_if_enabled(const Key& key, ptr value, const group_key_type& group) {
        if (!enabled()) return false;
        if (!value) {
            erase(key);
            return false;
        }

        ptr old;
        {
            std::scoped_lock lock{mtx_};
            if (!enabled_.load(std::memory_order_acquire)) return false;
            insert_locked(key, std::move(value), group, old);
        }
        return true;
    }

    template <typename F>
    requires ( std::invocable<F&, Value&> )
    bool upsert_if_enabled(const Key& key, const group_key_type& group, F&& f) {
        if (!enabled()) return false;

        ptr old;
        {
            std::scoped_lock lock{mtx_};
            if (!enabled_.load(std::memory_order_acquire)) return false;
            upsert_locked(key, group, std::forward<F>(f), old);
        }
        return true;
    }

    bool replace_group_if_enabled(const group_key_type& group, snapshot_t& items) {
        if (!enabled()) return false;

        std::vector<ptr> dropped;
        {
            std::scoped_lock lock{mtx_};
            if (!enabled_.load(std::memory_order_acquire)) return false;
            erase_group_locked(group, dropped);
            if (!items.empty()) {
                const auto needed = container_.size() + items.size();
                container_.template get<by_key>().reserve(needed);
                container_.template get<by_group>().reserve(needed);
                for (auto& [key, value] : items) {
                    if (!value) continue;
                    ptr old;
                    insert_locked(key, std::move(value), group, old);
                    if (old) {
                        try {
                            dropped.emplace_back(std::move(old));
                        } catch (...) {} // NOLINT(bugprone-empty-catch)
                    }
                }
            }
        }
        return true;
    }

    bool insert_batch_if_enabled(const group_key_type& group, snapshot_t& items) {
        if (!enabled() || items.empty()) return false;

        std::vector<ptr> dropped;
        {
            std::scoped_lock lock{mtx_};
            if (!enabled_.load(std::memory_order_acquire)) return false;
            const auto needed = container_.size() + items.size();
            container_.template get<by_key>().reserve(needed);
            container_.template get<by_group>().reserve(needed);
            for (auto& [key, value] : items) {
                if (!value) continue;
                ptr old;
                insert_locked(key, std::move(value), group, old);
                if (old) {
                    try {
                        dropped.emplace_back(std::move(old));
                    } catch (...) {} // NOLINT(bugprone-empty-catch)
                }
            }
        }
        return true;
    }

    template <typename Pred>
    requires ( std::invocable<Pred&, const group_key_type&> )
    void erase_groups_if(Pred&& pred) {
        std::vector<ptr> dropped;
        {
            std::scoped_lock lock{mtx_};
            auto& group_idx = container_.template get<by_group>();
            for (auto it = group_idx.begin(); it != group_idx.end(); ) {
                const auto group = it->group;
                if (std::invoke(pred, group)) {
                    while (it != group_idx.end() && it->group == group) {
                        ptr tmp = std::move(it->value);
                        it = group_idx.erase(it);
                        if (tmp) {
                            try {
                                dropped.emplace_back(std::move(tmp));
                            } catch (...) {} // NOLINT(bugprone-empty-catch)
                        }
                    }
                } else {
                    while (it != group_idx.end() && it->group == group) {
                        ++it;
                    }
                }
            }
        }
    }

    // Internal helper: invokes f(key, ptr) under shared_lock. Caller must NOT re-enter this cache.
    template <typename F>
    requires ( std::invocable<F&, const Key&, const ptr&> )
    void visit_group_locked(const group_key_type& group, F&& f) const {
        std::shared_lock lock{mtx_};
        const auto& group_idx = container_.template get<by_group>();
        const auto [first, last] = group_idx.equal_range(group);
        for (auto it = first; it != last; ++it) {
            if (it->value) std::invoke(f, it->key, it->value);
        }
    }

    mutable std::shared_mutex mtx_;
    container_t container_;
    std::atomic_bool enabled_{false};
    void (*on_enabled_change_)(void*){nullptr};
    void* on_enabled_change_ctx_{nullptr};
};

template <typename Key, typename Value>
class cache<Key, Value, void> {
public:
    static constexpr bool grouped = false;

    using key_type = Key;
    using value_type = Value;
    using group_key_type = cache_detail::no_group;
    using ptr = std::shared_ptr<const Value>;
    using snapshot_t = std::vector<std::pair<Key, ptr>>;

    cache() = default;

    cache(const cache&) = delete;
    cache& operator=(const cache&) = delete;
    cache(cache&&) = delete;
    cache& operator=(cache&&) = delete;

    ~cache() noexcept = default;

    [[nodiscard]] bool enabled() const noexcept {
        return enabled_.load(std::memory_order_acquire);
    }

    void set_enabled(const bool value) {
        set_enabled_impl(value, true);
    }

    // null if absent
    [[nodiscard]] ptr get(const Key& key) const {
        ptr res{};
        map_.cvisit(key, [&](const auto& entry) {
            res = entry.second;
        });
        return res;
    }

    [[nodiscard]] opt<Value> copy(const Key& key) const {
        const auto p = get(key);
        if (!p) return std::nullopt;
        return *p;
    }

    [[nodiscard]] bool contains(const Key& key) const {
        return map_.contains(key);
    }

    [[nodiscard]] std::size_t size() const {
        return map_.size();
    }

    [[nodiscard]] bool empty() const {
        return map_.empty();
    }

    [[nodiscard]] std::vector<Key> keys() const {
        std::vector<Key> out;
        out.reserve(map_.size());
        map_.cvisit_all([&](const auto& entry) {
            if (entry.second) out.emplace_back(entry.first);
        });
        return out;
    }

    [[nodiscard]] std::vector<ptr> all() const {
        std::vector<ptr> out;
        out.reserve(map_.size());
        map_.cvisit_all([&](const auto& entry) {
            if (entry.second) out.emplace_back(entry.second);
        });
        return out;
    }

    [[nodiscard]] snapshot_t snapshot() const {
        snapshot_t out;
        out.reserve(map_.size());
        map_.cvisit_all([&](const auto& entry) {
            if (entry.second) out.emplace_back(entry.first, entry.second);
        });
        return out;
    }

    template <typename F>
    requires ( std::invocable<F&, const Key&, const ptr&> || std::invocable<F&, const Key&, const Value&> )
    void for_each(F&& f) const {
        for (const auto& [key, value] : snapshot()) {
            if constexpr (std::invocable<F&, const Key&, const ptr&>) {
                if constexpr (std::is_same_v<std::invoke_result_t<F&, const Key&, const ptr&>, bool>) {
                    if (!std::invoke(f, key, value)) return;
                } else {
                    std::invoke(f, key, value);
                }
            } else {
                if (value) {
                    if constexpr (std::is_same_v<std::invoke_result_t<F&, const Key&, const Value&>, bool>) {
                        if (!std::invoke(f, key, *value)) return;
                    } else {
                        std::invoke(f, key, *value);
                    }
                }
            }
        }
    }

    void insert(const Key& key, Value value) {
        insert(key, std::make_shared<const Value>(std::move(value)));
    }

    void insert(const Key& key, ptr value) {
        if (!value) {
            erase(key);
            return;
        }

        ptr old;
        map_.try_emplace_or_visit(key, std::move(value), [&](auto& entry) {
            old = std::exchange(entry.second, std::move(value));
        });
    }

    template <typename F>
    requires ( std::invocable<F&, Value&> )
    bool modify(const Key& key, F&& f) {
        ptr old;
        bool modified = false;
        map_.visit(key, [&](auto& entry) {
            if (!entry.second) return;
            auto updated = std::make_shared<Value>(*entry.second);
            std::invoke(f, *updated);
            old = std::exchange(entry.second, std::move(updated));
            modified = true;
        });
        return modified;
    }

    template <typename F>
    requires ( std::invocable<F&, Value&> )
    void upsert(const Key& key, F&& f) {
        struct lazy_value_factory {
            F& fn;
            operator ptr() const {
                auto val = std::make_shared<Value>();
                std::invoke(fn, *val);
                return val;
            }
        };

        ptr old;
        map_.try_emplace_or_visit(key, lazy_value_factory{f}, [&](auto& entry) {
            auto updated = entry.second ? std::make_shared<Value>(*entry.second) : std::make_shared<Value>();
            std::invoke(f, *updated);
            old = std::exchange(entry.second, std::move(updated));
        });
    }

    bool erase(const Key& key) {
        ptr old;
        return map_.erase_if(key, [&](auto& entry) {
            old = std::move(entry.second);
            return true;
        }) > 0;
    }

    void clear() {
        map_t tmp;
        map_.swap(tmp);
    }

    void reserve(const std::size_t n) {
        map_.reserve(n);
    }

private:
    friend class cache_manager;

    using key_hash = cache_detail::hash_for_t<Key>;
    using map_t = boost::unordered::concurrent_flat_map<Key, ptr, key_hash>;

    void set_enabled_hook(void (*fn)(void*), void* ctx) noexcept {
        on_enabled_change_ = fn;
        on_enabled_change_ctx_ = ctx;
    }

    void set_enabled_impl(const bool value, const bool notify) {
        enabled_.store(value, std::memory_order_release);
        if (!value) clear();

        if (notify && on_enabled_change_) {
            on_enabled_change_(on_enabled_change_ctx_);
        }
    }

    void rollback_if_disabled(const Key& key, const ptr& inserted) {
        if (!enabled_.load(std::memory_order_acquire)) [[unlikely]] {
            ptr dropped;
            map_.erase_if(key, [&](auto& entry) {
                if (entry.second == inserted) {
                    dropped = std::move(entry.second);
                    return true;
                }
                return false;
            });
        }
    }

    bool insert_if_enabled(const Key& key, Value value) {
        if (!enabled()) return false;
        return insert_if_enabled(key, std::make_shared<const Value>(std::move(value)));
    }

    bool insert_if_enabled(const Key& key, ptr value) {
        if (!enabled()) return false;
        if (!value) {
            erase(key);
            return false;
        }

        const ptr inserted = value;
        ptr old;
        map_.try_emplace_or_visit(key, std::move(value), [&](auto& entry) {
            old = std::exchange(entry.second, std::move(value));
        });
        rollback_if_disabled(key, inserted);
        return enabled();
    }

    template <typename F>
    requires ( std::invocable<F&, Value&> )
    bool modify_if_enabled(const Key& key, F&& f) {
        if (!enabled()) return false;

        ptr old;
        ptr inserted;
        bool modified = false;
        map_.visit(key, [&](auto& entry) {
            if (!entry.second) return;
            auto updated = std::make_shared<Value>(*entry.second);
            std::invoke(f, *updated);
            inserted = updated;
            old = std::exchange(entry.second, std::move(updated));
            modified = true;
        });
        if (modified) rollback_if_disabled(key, inserted);
        return modified && enabled();
    }

    template <typename F>
    requires ( std::invocable<F&, Value&> )
    bool upsert_if_enabled(const Key& key, F&& f) {
        if (!enabled()) return false;

        ptr inserted;
        struct lazy_value_factory {
            F& fn;
            ptr& out;
            operator ptr() const {
                auto val = std::make_shared<Value>();
                std::invoke(fn, *val);
                out = val;
                return val;
            }
        };

        ptr old;
        map_.try_emplace_or_visit(key, lazy_value_factory{f, inserted}, [&](auto& entry) {
            auto updated = entry.second ? std::make_shared<Value>(*entry.second) : std::make_shared<Value>();
            std::invoke(f, *updated);
            inserted = updated;
            old = std::exchange(entry.second, std::move(updated));
        });
        rollback_if_disabled(key, inserted);
        return enabled();
    }

    template <typename Pred>
    requires ( std::invocable<Pred&, const Key&> )
    void erase_if(Pred&& pred) {
        std::vector<ptr> dropped;
        map_.erase_if([&](auto& entry) {
            if (!std::invoke(pred, entry.first)) return false;
            ptr tmp = std::move(entry.second);
            if (tmp) {
                try {
                    dropped.emplace_back(std::move(tmp));
                } catch (...) {} // NOLINT(bugprone-empty-catch)
            }
            return true;
        });
    }

    map_t map_;
    std::atomic_bool enabled_{false};
    void (*on_enabled_change_)(void*){nullptr};
    void* on_enabled_change_ctx_{nullptr};
};

struct cached_guild {
    discusy::guild::guild guild{};
    integer member_count{0};
    bool large{false};
    bool unavailable{false};
    opt<timestamp> joined_at{};
};

namespace cache_detail {

inline void apply_member_update(discusy::guild::guild_member& dst, const recieve_event::guild_member_update& src) {
    dst.set_guild_id_context_void(src.guild_id);
    dst.user = src.user;
    dst.roles = src.roles;
    dst.nick = src.nick;
    dst.avatar = src.avatar;
    dst.banner = src.banner;
    dst.premium_since = src.premium_since;
    dst.communication_disabled_until = src.communication_disabled_until;
    dst.avatar_decoration_data = src.avatar_decoration_data;
    dst.collectibles = src.collectibles;

    if (src.joined_at) dst.joined_at = src.joined_at;
    if (src.deaf) dst.deaf = src.deaf;
    if (src.mute) dst.mute = src.mute;
    if (src.pending) dst.pending = src.pending;
}

} // namespace cache_detail

class cache_manager {
public:
    explicit cache_manager(gateway_callbacks& callbacks) noexcept : callbacks_{callbacks} {
        guilds.set_enabled_hook(&cache_manager::on_cache_toggle, this);
        channels.set_enabled_hook(&cache_manager::on_cache_toggle, this);
        users.set_enabled_hook(&cache_manager::on_cache_toggle, this);
        members.set_enabled_hook(&cache_manager::on_cache_toggle, this);
        roles.set_enabled_hook(&cache_manager::on_cache_toggle, this);
        emojis.set_enabled_hook(&cache_manager::on_cache_toggle, this);
        voice_states.set_enabled_hook(&cache_manager::on_cache_toggle, this);
    }

    cache_manager(const cache_manager&) = delete;
    cache_manager& operator=(const cache_manager&) = delete;
    cache_manager(cache_manager&&) = delete;
    cache_manager& operator=(cache_manager&&) = delete;

    ~cache_manager() noexcept {
        detach();
    }

    // guild id -> guild
    cache<snowflake, cached_guild> guilds;
    // channel id -> channel (threads included), grouped by guild id
    cache<snowflake, discusy::channel::channel, snowflake> channels;
    // user id -> user
    cache<snowflake, discusy::user::user> users;
    // {guild id, user id} -> member, grouped by guild id
    cache<guild_scoped_id, discusy::guild::guild_member_with_guild, snowflake> members;
    // {guild id, role id} -> role, grouped by guild id
    cache<guild_scoped_id, discusy::permissions::role, snowflake> roles;
    // {guild id, emoji id} -> emoji, grouped by guild id
    cache<guild_scoped_id, discusy::emoji::emoji, snowflake> emojis;
    // {guild id, user id} -> voice state, grouped by guild id. only holds users who are
    // currently in a voice channel, entries are dropped as soon as they leave
    cache<guild_scoped_id, discusy::voice::voice_state, snowflake> voice_states;

    cache_manager& enable(const cache_flags flags) {
        apply(flags, true);
        sync_listeners();
        return *this;
    }

    cache_manager& disable(const cache_flags flags) {
        apply(flags, false);
        sync_listeners();
        return *this;
    }

    cache_manager& only(const cache_flags flags) {
        apply(cache_flags::all & ~flags, false);
        apply(flags, true);
        sync_listeners();
        return *this;
    }

    [[nodiscard]] bool enabled(const cache_flags flags) const noexcept {
        return (enabled_flags() & flags) == flags;
    }

    [[nodiscard]] cache_flags enabled_flags() const noexcept {
        auto out = cache_flags::none;
        if (guilds.enabled())       out |= cache_flags::guilds;
        if (channels.enabled())     out |= cache_flags::channels;
        if (members.enabled())      out |= cache_flags::members;
        if (users.enabled())        out |= cache_flags::users;
        if (roles.enabled())        out |= cache_flags::roles;
        if (emojis.enabled())       out |= cache_flags::emojis;
        if (voice_states.enabled()) out |= cache_flags::voice_states;
        return out;
    }

    [[nodiscard]] static constexpr intent required_intents(const cache_flags flags) noexcept {
        auto out = intent::none;
        if (contains_bit(flags, cache_flags::guilds))       out |= intent::guilds;
        if (contains_bit(flags, cache_flags::channels))     out |= intent::guilds;
        if (contains_bit(flags, cache_flags::roles))        out |= intent::guilds;
        if (contains_bit(flags, cache_flags::emojis))       out |= intent::guilds | intent::guild_emojis_and_stickers;
        if (contains_bit(flags, cache_flags::members))      out |= intent::guilds | intent::guild_members;
        if (contains_bit(flags, cache_flags::users))        out |= intent::guilds;
        if (contains_bit(flags, cache_flags::voice_states)) out |= intent::guilds | intent::guild_voice_states;
        return out;
    }

    void clear_all() {
        guilds.clear();
        channels.clear();
        users.clear();
        members.clear();
        roles.clear();
        emojis.clear();
        voice_states.clear();
    }

    [[nodiscard]] auto get_guild(const snowflake guild_id) const {
        return guilds.get(guild_id);
    }

    [[nodiscard]] auto get_channel(const snowflake channel_id) const {
        return channels.get(channel_id);
    }

    [[nodiscard]] auto get_user(const snowflake user_id) const {
        return users.get(user_id);
    }

    [[nodiscard]] auto get_member(const snowflake guild_id, const snowflake user_id) const {
        return members.get(guild_scoped_id{.guild_id=guild_id, .id=user_id});
    }

    [[nodiscard]] auto get_role(const snowflake guild_id, const snowflake role_id) const {
        return roles.get(guild_scoped_id{.guild_id=guild_id, .id=role_id});
    }

    [[nodiscard]] auto get_emoji(const snowflake guild_id, const snowflake emoji_id) const {
        return emojis.get(guild_scoped_id{.guild_id=guild_id, .id=emoji_id});
    }

    [[nodiscard]] auto get_voice_state(const snowflake guild_id, const snowflake user_id) const {
        return voice_states.get(guild_scoped_id{.guild_id=guild_id, .id=user_id});
    }

    [[nodiscard]] auto guild_channels(const snowflake guild_id) const {
        return channels.in_group(guild_id);
    }

    [[nodiscard]] auto guild_roles(const snowflake guild_id) const {
        return roles.in_group(guild_id);
    }

    [[nodiscard]] auto guild_emojis(const snowflake guild_id) const {
        return emojis.in_group(guild_id);
    }

    [[nodiscard]] auto guild_members(const snowflake guild_id) const {
        return members.in_group(guild_id);
    }

    // the voice channel this user is currently sat in, if any
    [[nodiscard]] opt<snowflake> voice_channel_of(const snowflake guild_id, const snowflake user_id) const {
        const auto state = voice_states.get(guild_scoped_id{.guild_id=guild_id, .id=user_id});
        if (!state || !state->channel_id || !*state->channel_id) return std::nullopt;
        return *state->channel_id;
    }

    // everyone currently in the given voice channel
    [[nodiscard]] std::vector<snowflake> voice_channel_members(const snowflake guild_id, const snowflake channel_id) const {
        std::vector<snowflake> out;
        voice_states.visit_group_locked(guild_id, [&](const guild_scoped_id& key, const auto& state) {
            if (state->channel_id && (*state->channel_id == channel_id)) {
                out.emplace_back(key.id);
            }
        });
        return out;
    }

    [[nodiscard]] std::vector<std::shared_ptr<const discusy::voice::voice_state>> voice_channel_states(
        const snowflake guild_id, const snowflake channel_id) const {
        std::vector<std::shared_ptr<const discusy::voice::voice_state>> out;
        voice_states.visit_group_locked(guild_id, [&](const guild_scoped_id&, const auto& state) {
            if (state->channel_id && (*state->channel_id == channel_id)) {
                out.emplace_back(state);
            }
        });
        return out;
    }

    [[nodiscard]] std::size_t voice_channel_member_count(const snowflake guild_id, const snowflake channel_id) const {
        std::size_t count = 0;
        voice_states.visit_group_locked(guild_id, [&](const guild_scoped_id&, const auto& state) noexcept {
            if (state->channel_id && (*state->channel_id == channel_id)) ++count;
        });
        return count;
    }

    void purge_guild(const snowflake guild_id) {
        guilds.erase(guild_id);
        channels.erase_group(guild_id);
        members.erase_group(guild_id);
        roles.erase_group(guild_id);
        emojis.erase_group(guild_id);
        voice_states.erase_group(guild_id);
    }

    void attach() {
        std::scoped_lock lock{listeners_mtx_};
        if (attached_.load(std::memory_order_acquire)) return;
        attached_.store(true, std::memory_order_release);
        try {
            sync_listeners_locked();
        } catch (...) {
            attached_.store(false, std::memory_order_release);
            unregister_all_locked();
            throw;
        }
    }

    void detach() noexcept {
        std::scoped_lock lock{listeners_mtx_};
        attached_.store(false, std::memory_order_release);
        unregister_all_locked();
    }

private:
    friend class bot;

    struct listener_ids {
        callback_id ready{0};
        callback_id guild_create{0};
        callback_id guild_update{0};
        callback_id guild_delete{0};
        callback_id channel_create{0};
        callback_id channel_update{0};
        callback_id channel_delete{0};
        callback_id thread_create{0};
        callback_id thread_update{0};
        callback_id thread_delete{0};
        callback_id thread_list_sync{0};
        callback_id guild_member_add{0};
        callback_id guild_member_remove{0};
        callback_id guild_member_update{0};
        callback_id guild_members_chunk{0};
        callback_id guild_role_create{0};
        callback_id guild_role_update{0};
        callback_id guild_role_delete{0};
        callback_id guild_emojis_update{0};
        callback_id user_update{0};
        callback_id voice_state_update{0};
    };

    static void on_cache_toggle(void* ctx) {
        static_cast<cache_manager*>(ctx)->sync_listeners();
    }

    template <typename Cb>
    static void unregister_noexcept(Cb& cb, callback_id& id) noexcept {
        if (id == 0) return;
        const auto target = std::exchange(id, 0);
        try {
            cb.unregister(target);
        } catch (...) {} // NOLINT(bugprone-empty-catch)
    }

    template <typename Cb, typename Fn>
    static void sync_one(Cb& cb, callback_id& id, const bool needed, Fn&& fn) {
        if (needed) {
            if (id == 0) id = cb.listen_system(std::forward<Fn>(fn));
        } else {
            unregister_noexcept(cb, id);
        }
    }

    void sync_listeners() {
        if (!attached_.load(std::memory_order_acquire)) return;
        std::scoped_lock lock{listeners_mtx_};
        if (!attached_.load(std::memory_order_acquire)) return;
        sync_listeners_locked();
    }

    void sync_listeners_locked() {
        const auto flags = enabled_flags();
        const bool any_enabled = flags != cache_flags::none;
        const bool has_guilds = contains_bit(flags, cache_flags::guilds);
        const bool has_channels = contains_bit(flags, cache_flags::channels);
        const bool has_members = contains_bit(flags, cache_flags::members);
        const bool has_users = contains_bit(flags, cache_flags::users);
        const bool has_roles = contains_bit(flags, cache_flags::roles);
        const bool has_emojis = contains_bit(flags, cache_flags::emojis);
        const bool has_voice = contains_bit(flags, cache_flags::voice_states);
        const bool any_guild_scoped = has_guilds || has_channels || has_members || has_roles || has_emojis || has_voice;

        sync_one(callbacks_.on_ready, listener_ids_.ready, any_enabled, [this](const recieve_event::ready& e) {
            handle_ready(e);
        });

        sync_one(callbacks_.on_guild_create, listener_ids_.guild_create, any_enabled, [this](const recieve_event::guild_create& e) {
            handle_guild_create(e);
        });
        sync_one(callbacks_.on_guild_update, listener_ids_.guild_update, has_guilds || has_roles || has_emojis, [this](const recieve_event::guild_update& e) {
            handle_guild_update(e);
        });
        sync_one(callbacks_.on_guild_delete, listener_ids_.guild_delete, any_guild_scoped, [this](const recieve_event::guild_delete& e) {
            handle_guild_delete(e);
        });

        sync_one(callbacks_.on_channel_create, listener_ids_.channel_create, has_channels, [this](const recieve_event::channel_create& e) {
            store_channel(e);
        });
        sync_one(callbacks_.on_channel_update, listener_ids_.channel_update, has_channels, [this](const recieve_event::channel_update& e) {
            store_channel(e);
        });
        sync_one(callbacks_.on_channel_delete, listener_ids_.channel_delete, has_channels, [this](const recieve_event::channel_delete& e) {
            if (channels.enabled()) channels.erase(e.id);
        });

        sync_one(callbacks_.on_thread_create, listener_ids_.thread_create, has_channels, [this](const recieve_event::thread_create& e) {
            store_channel(e);
        });
        sync_one(callbacks_.on_thread_update, listener_ids_.thread_update, has_channels, [this](const recieve_event::thread_update& e) {
            store_channel(e);
        });
        sync_one(callbacks_.on_thread_delete, listener_ids_.thread_delete, has_channels, [this](const recieve_event::thread_delete& e) {
            if (channels.enabled()) channels.erase(e.id);
        });
        sync_one(callbacks_.on_thread_list_sync, listener_ids_.thread_list_sync, has_channels, [this](const recieve_event::thread_list_sync& e) {
            handle_thread_list_sync(e);
        });

        sync_one(callbacks_.on_guild_member_add, listener_ids_.guild_member_add, has_members || has_users || has_guilds, [this](const recieve_event::guild_member_add& e) {
            store_member(e, e.other.guild_id);
            if (guilds.enabled()) {
                guilds.modify_if_enabled(e.other.guild_id, [](cached_guild& g) { ++g.member_count; });
            }
        });
        sync_one(callbacks_.on_guild_member_remove, listener_ids_.guild_member_remove, has_members || has_voice || has_guilds, [this](const recieve_event::guild_member_remove& e) {
            if (members.enabled()) members.erase(guild_scoped_id{.guild_id=e.guild_id, .id=e.user.id});
            if (voice_states.enabled()) voice_states.erase(guild_scoped_id{.guild_id=e.guild_id, .id=e.user.id});
            if (guilds.enabled()) {
                guilds.modify_if_enabled(e.guild_id, [](cached_guild& g) { if (g.member_count > 0) --g.member_count; });
            }
        });
        sync_one(callbacks_.on_guild_member_update, listener_ids_.guild_member_update, has_members || has_users, [this](const recieve_event::guild_member_update& e) {
            handle_member_update(e);
        });
        sync_one(callbacks_.on_guild_members_chunk, listener_ids_.guild_members_chunk, has_members || has_users, [this](const recieve_event::guild_members_chunk& e) {
            handle_members_chunk(e);
        });

        sync_one(callbacks_.on_guild_role_create, listener_ids_.guild_role_create, has_roles, [this](const recieve_event::guild_role_create& e) {
            store_role(e);
        });
        sync_one(callbacks_.on_guild_role_update, listener_ids_.guild_role_update, has_roles, [this](const recieve_event::guild_role_update& e) {
            store_role(e);
        });
        sync_one(callbacks_.on_guild_role_delete, listener_ids_.guild_role_delete, has_roles, [this](const recieve_event::guild_role_delete& e) {
            if (roles.enabled()) roles.erase(guild_scoped_id{.guild_id=e.guild_id, .id=e.role_id});
        });

        sync_one(callbacks_.on_guild_emojis_update, listener_ids_.guild_emojis_update, has_emojis, [this](const recieve_event::guild_emojis_update& e) {
            handle_emojis_update(e);
        });

        sync_one(callbacks_.on_user_update, listener_ids_.user_update, has_users, [this](const recieve_event::user_update& e) {
            if (users.enabled()) {
                auto copy = std::make_shared<discusy::user::user>(e);
                attach_bot(*copy, callbacks_.get_bot());
                users.insert_if_enabled(e.id, std::move(copy));
            }
        });

        sync_one(callbacks_.on_voice_state_update, listener_ids_.voice_state_update, has_voice || has_members || has_users, [this](const recieve_event::voice_state_update& e) {
            handle_voice_state(e);
        });
    }

    void unregister_all_locked() noexcept {
        unregister_noexcept(callbacks_.on_ready, listener_ids_.ready);
        unregister_noexcept(callbacks_.on_guild_create, listener_ids_.guild_create);
        unregister_noexcept(callbacks_.on_guild_update, listener_ids_.guild_update);
        unregister_noexcept(callbacks_.on_guild_delete, listener_ids_.guild_delete);
        unregister_noexcept(callbacks_.on_channel_create, listener_ids_.channel_create);
        unregister_noexcept(callbacks_.on_channel_update, listener_ids_.channel_update);
        unregister_noexcept(callbacks_.on_channel_delete, listener_ids_.channel_delete);
        unregister_noexcept(callbacks_.on_thread_create, listener_ids_.thread_create);
        unregister_noexcept(callbacks_.on_thread_update, listener_ids_.thread_update);
        unregister_noexcept(callbacks_.on_thread_delete, listener_ids_.thread_delete);
        unregister_noexcept(callbacks_.on_thread_list_sync, listener_ids_.thread_list_sync);
        unregister_noexcept(callbacks_.on_guild_member_add, listener_ids_.guild_member_add);
        unregister_noexcept(callbacks_.on_guild_member_remove, listener_ids_.guild_member_remove);
        unregister_noexcept(callbacks_.on_guild_member_update, listener_ids_.guild_member_update);
        unregister_noexcept(callbacks_.on_guild_members_chunk, listener_ids_.guild_members_chunk);
        unregister_noexcept(callbacks_.on_guild_role_create, listener_ids_.guild_role_create);
        unregister_noexcept(callbacks_.on_guild_role_update, listener_ids_.guild_role_update);
        unregister_noexcept(callbacks_.on_guild_role_delete, listener_ids_.guild_role_delete);
        unregister_noexcept(callbacks_.on_guild_emojis_update, listener_ids_.guild_emojis_update);
        unregister_noexcept(callbacks_.on_user_update, listener_ids_.user_update);
        unregister_noexcept(callbacks_.on_voice_state_update, listener_ids_.voice_state_update);
    }

    void apply(const cache_flags flags, const bool value) {
        if (contains_bit(flags, cache_flags::guilds))       guilds.set_enabled_impl(value, false);
        if (contains_bit(flags, cache_flags::channels))     channels.set_enabled_impl(value, false);
        if (contains_bit(flags, cache_flags::members))      members.set_enabled_impl(value, false);
        if (contains_bit(flags, cache_flags::users))        users.set_enabled_impl(value, false);
        if (contains_bit(flags, cache_flags::roles))        roles.set_enabled_impl(value, false);
        if (contains_bit(flags, cache_flags::emojis))       emojis.set_enabled_impl(value, false);
        if (contains_bit(flags, cache_flags::voice_states)) voice_states.set_enabled_impl(value, false);
    }

    void handle_ready(const recieve_event::ready& e) {
        if (users.enabled()) {
            auto copy = std::make_shared<discusy::user::user>(e.user);
            attach_bot(*copy, callbacks_.get_bot());
            users.insert_if_enabled(e.user.id, std::move(copy));
        }

        const bool has_guilds = guilds.enabled();
        const bool has_channels = channels.enabled();
        const bool has_members = members.enabled();
        const bool has_roles = roles.enabled();
        const bool has_emojis = emojis.enabled();
        const bool has_voice = voice_states.enabled();
        if (!has_guilds && !has_channels && !has_members && !has_roles && !has_emojis && !has_voice) return;

        std::uint32_t shard_id = 0;
        std::uint32_t total_shards = 1;
        if (e.shard) {
            shard_id = (*e.shard)[0];
            total_shards = (*e.shard)[1];
            if (total_shards == 0) return;
        }

        std::unordered_set<snowflake, cache_detail::snowflake_hash> present;
        if (!e.guilds.empty()) {
            present.reserve(e.guilds.size());
            for (const auto& guild : e.guilds) present.emplace(guild.id);
        }

        auto should_purge = [&](const snowflake gid) noexcept -> bool {
            if (!gid) return false; // keep DM channels (group 0)
            if (gid.guild_shard_id(total_shards) != shard_id) return false; // another shard's problem
            return !present.contains(gid);
        };

        if (has_guilds)   guilds.erase_if(should_purge);
        if (has_channels) channels.erase_groups_if(should_purge);
        if (has_members)  members.erase_groups_if(should_purge);
        if (has_roles)    roles.erase_groups_if(should_purge);
        if (has_emojis)   emojis.erase_groups_if(should_purge);
        if (has_voice)    voice_states.erase_groups_if(should_purge);
    }

    void handle_guild_create(const recieve_event::guild_create& e) {
        auto* const bot_ptr = callbacks_.get_bot();

        if (const auto* unavailable = std::get_if<discusy::guild::unavailable_guild>(&e)) {
            if (guilds.enabled()) {
                const auto id = unavailable->id;
                guilds.upsert_if_enabled(id, [id, bot_ptr](cached_guild& g) {
                    g.guild.id = id;
                    attach_bot(g.guild, bot_ptr);
                    g.unavailable = true;
                });
            }
            return;
        }

        const auto* created = std::get_if<recieve_event::guild_create_struct>(&e);
        if (created == nullptr) return;

        const auto& incoming = *created;
        const auto guild_id = incoming.guild.id;

        if (guilds.enabled()) {
            auto cached = std::make_shared<cached_guild>();
            cached->guild = incoming.guild;
            attach_bot(cached->guild, bot_ptr);
            cached->member_count = incoming.other.member_count;
            cached->large = incoming.other.large;
            cached->unavailable = incoming.other.unavailable.value_or(false);
            cached->joined_at = incoming.other.joined_at;

            guilds.insert_if_enabled(guild_id, std::move(cached));
        }

        if (roles.enabled()) {
            cache<guild_scoped_id, discusy::permissions::role, snowflake>::snapshot_t batch;
            batch.reserve(incoming.guild.roles.size());
            for (const auto& role : incoming.guild.roles) {
                batch.emplace_back(
                    guild_scoped_id{.guild_id=guild_id, .id=role.id},
                    std::make_shared<const discusy::permissions::role>(role)
                );
            }
            roles.replace_group_if_enabled(guild_id, batch);
        }

        if (emojis.enabled()) {
            cache<guild_scoped_id, discusy::emoji::emoji, snowflake>::snapshot_t batch;
            batch.reserve(incoming.guild.emojis.size());
            for (const auto& emoji : incoming.guild.emojis) {
                if (!emoji.id || !*emoji.id) continue; // unicode emoji, not ours to cache
                batch.emplace_back(
                    guild_scoped_id{.guild_id=guild_id, .id=*emoji.id},
                    std::make_shared<const discusy::emoji::emoji>(emoji)
                );
            }
            emojis.replace_group_if_enabled(guild_id, batch);
        }

        if (channels.enabled()) {
            cache<snowflake, discusy::channel::channel, snowflake>::snapshot_t batch;
            batch.reserve(incoming.other.channels.size() + incoming.other.threads.size());
            auto append_channel = [&](const discusy::channel::channel& ch) {
                auto copy = std::make_shared<discusy::channel::channel>(ch);
                copy->guild_id = guild_id;
                attach_bot(*copy, bot_ptr);
                batch.emplace_back(ch.id, std::move(copy));
            };
            for (const auto& channel : incoming.other.channels) append_channel(channel);
            for (const auto& thread : incoming.other.threads) append_channel(thread);
            channels.replace_group_if_enabled(guild_id, batch);
        }

        if (members.enabled() || users.enabled()) {
            store_members_batch(incoming.other.members, guild_id, bot_ptr);
        }

        if (voice_states.enabled()) {
            cache<guild_scoped_id, discusy::voice::voice_state, snowflake>::snapshot_t batch;
            batch.reserve(incoming.other.voice_states.size());
            for (const auto& state : incoming.other.voice_states) {
                if (!state.channel_id || !*state.channel_id) continue;

                auto copy = std::make_shared<discusy::voice::voice_state>(state);
                copy->guild_id = guild_id; // omitted inside GUILD_CREATE
                if (copy->member) {
                    copy->member->set_guild_id_context_void(guild_id);
                    attach_bot(*copy->member, bot_ptr);
                }
                batch.emplace_back(guild_scoped_id{.guild_id=guild_id, .id=state.user_id}, std::move(copy));
            }
            voice_states.replace_group_if_enabled(guild_id, batch);
        }
    }

    void handle_guild_update(const recieve_event::guild_update& e) {
        auto* const bot_ptr = callbacks_.get_bot();

        if (guilds.enabled()) {
            guilds.upsert_if_enabled(e.id, [bot_ptr, &e](cached_guild& g) {
                g.guild = e;
                attach_bot(g.guild, bot_ptr);
                g.unavailable = false;
            });
        }

        if (roles.enabled()) {
            cache<guild_scoped_id, discusy::permissions::role, snowflake>::snapshot_t batch;
            batch.reserve(e.roles.size());
            for (const auto& role : e.roles) {
                batch.emplace_back(
                    guild_scoped_id{.guild_id=e.id, .id=role.id},
                    std::make_shared<const discusy::permissions::role>(role)
                );
            }
            roles.replace_group_if_enabled(e.id, batch);
        }

        if (emojis.enabled()) {
            cache<guild_scoped_id, discusy::emoji::emoji, snowflake>::snapshot_t batch;
            batch.reserve(e.emojis.size());
            for (const auto& emoji : e.emojis) {
                if (!emoji.id || !*emoji.id) continue;
                batch.emplace_back(
                    guild_scoped_id{.guild_id=e.id, .id=*emoji.id},
                    std::make_shared<const discusy::emoji::emoji>(emoji)
                );
            }
            emojis.replace_group_if_enabled(e.id, batch);
        }
    }

    void handle_guild_delete(const recieve_event::guild_delete& e) {
        if (e.unavailable) {
            if (guilds.enabled()) {
                guilds.modify_if_enabled(e.id, [](cached_guild& g) {
                    g.unavailable = true;
                });
            }
        } else {
            purge_guild(e.id);
        }
    }

    void handle_thread_list_sync(const recieve_event::thread_list_sync& e) {
        if (!channels.enabled() || e.threads.empty()) return;
        if (!e.guild_id) {
            for (const auto& thread : e.threads) store_channel(thread);
            return;
        }

        auto* const bot_ptr = callbacks_.get_bot();
        cache<snowflake, discusy::channel::channel, snowflake>::snapshot_t batch;
        batch.reserve(e.threads.size());
        for (const auto& thread : e.threads) {
            auto copy = std::make_shared<discusy::channel::channel>(thread);
            copy->guild_id = e.guild_id;
            attach_bot(*copy, bot_ptr);
            batch.emplace_back(thread.id, std::move(copy));
        }
        channels.insert_batch_if_enabled(e.guild_id, batch);
    }

    void handle_member_update(const recieve_event::guild_member_update& e) {
        auto* const bot_ptr = callbacks_.get_bot();

        if (users.enabled()) {
            auto copy = std::make_shared<discusy::user::user>(e.user);
            attach_bot(*copy, bot_ptr);
            users.insert_if_enabled(e.user.id, std::move(copy));
        }
        if (!members.enabled() || !e.guild_id) return;

        members.upsert_if_enabled(
            guild_scoped_id{.guild_id=e.guild_id, .id=e.user.id},
            e.guild_id,
            [bot_ptr, &e](discusy::guild::guild_member_with_guild& m) {
                m.other.guild_id = e.guild_id;
                cache_detail::apply_member_update(m.guild_member, e);
                attach_bot(m.guild_member, bot_ptr);
            }
        );
    }

    void handle_members_chunk(const recieve_event::guild_members_chunk& e) {
        if ((!members.enabled() && !users.enabled()) || e.members.empty()) return;
        store_members_batch(e.members, e.guild_id, callbacks_.get_bot());
    }

    void handle_emojis_update(const recieve_event::guild_emojis_update& e) {
        if (!emojis.enabled()) return;

        cache<guild_scoped_id, discusy::emoji::emoji, snowflake>::snapshot_t batch;
        batch.reserve(e.emojis.size());
        for (const auto& emoji : e.emojis) {
            if (!emoji.id || !*emoji.id) continue;
            batch.emplace_back(
                guild_scoped_id{.guild_id=e.guild_id, .id=*emoji.id},
                std::make_shared<const discusy::emoji::emoji>(emoji)
            );
        }
        emojis.replace_group_if_enabled(e.guild_id, batch);
    }

    void handle_voice_state(const recieve_event::voice_state_update& e) {
        if (!e.guild_id || !*e.guild_id) return;

        const auto guild_id = *e.guild_id;

        if (e.member) store_member(*e.member, guild_id);
        if (!voice_states.enabled()) return;

        if (!e.channel_id || !*e.channel_id) { // left voice
            voice_states.erase(guild_scoped_id{.guild_id=guild_id, .id=e.user_id});
            return;
        }

        auto copy = std::make_shared<discusy::voice::voice_state>(e);
        if (copy->member) {
            copy->member->set_guild_id_context_void(guild_id);
            attach_bot(*copy->member, callbacks_.get_bot());
        }
        voice_states.insert_if_enabled(guild_scoped_id{.guild_id=guild_id, .id=e.user_id}, std::move(copy), guild_id);
    }

    void store_channel(const discusy::channel::channel& channel, const snowflake guild_id = snowflake{}) {
        if (!channels.enabled()) return;

        auto copy = std::make_shared<discusy::channel::channel>(channel);
        if (guild_id) {
            copy->guild_id = guild_id;
        } else if (!copy->guild_id) {
            if (const auto existing = channels.get(channel.id); existing && existing->guild_id) {
                copy->guild_id = existing->guild_id;
            }
        }

        attach_bot(*copy, callbacks_.get_bot());
        const auto group = copy->guild_id.value_or(snowflake{});
        channels.insert_if_enabled(channel.id, std::move(copy), group);
    }

    void store_members_batch(const std::vector<discusy::guild::guild_member>& list, const snowflake guild_id, bot* const bot_ptr) {
        const bool cache_users = users.enabled();
        const bool cache_members = members.enabled() && static_cast<bool>(guild_id);

        cache<guild_scoped_id, discusy::guild::guild_member_with_guild, snowflake>::snapshot_t batch;
        if (cache_members) batch.reserve(list.size());

        for (const auto& member : list) {
            if (!member.user) continue;
            const auto user_id = member.user->id;

            if (cache_users) {
                auto user_copy = std::make_shared<discusy::user::user>(*member.user);
                attach_bot(*user_copy, bot_ptr);
                users.insert_if_enabled(user_id, std::move(user_copy));
            }
            if (cache_members) {
                auto stored = std::make_shared<discusy::guild::guild_member_with_guild>();
                stored->guild_member = member;
                stored->guild_member.set_guild_id_context_void(guild_id);
                attach_bot(stored->guild_member, bot_ptr);
                stored->other.guild_id = guild_id;
                batch.emplace_back(guild_scoped_id{.guild_id=guild_id, .id=user_id}, std::move(stored));
            }
        }

        if (cache_members && !batch.empty()) {
            members.insert_batch_if_enabled(guild_id, batch);
        }
    }

    void store_member(const discusy::guild::guild_member& member, const snowflake guild_id) {
        if (!member.user) return;

        auto* const bot_ptr = callbacks_.get_bot();
        const auto user_id = member.user->id;

        if (users.enabled()) {
            auto user_copy = std::make_shared<discusy::user::user>(*member.user);
            attach_bot(*user_copy, bot_ptr);
            users.insert_if_enabled(user_id, std::move(user_copy));
        }
        if (!members.enabled() || !guild_id) return;

        auto stored = std::make_shared<discusy::guild::guild_member_with_guild>();
        stored->guild_member = member;
        stored->guild_member.set_guild_id_context_void(guild_id);
        attach_bot(stored->guild_member, bot_ptr);
        stored->other.guild_id = guild_id;

        members.insert_if_enabled(guild_scoped_id{.guild_id=guild_id, .id=user_id}, std::move(stored), guild_id);
    }

    void store_member(const discusy::guild::guild_member_with_guild& member, const snowflake guild_id = snowflake{}) {
        const auto effective_guild_id = guild_id ? guild_id : member.other.guild_id;
        store_member(member.guild_member, effective_guild_id);
    }

    void store_role(const recieve_event::guild_role_create& e) {
        if (!roles.enabled()) return;
        roles.insert_if_enabled(guild_scoped_id{.guild_id=e.guild_id, .id=e.role.id}, e.role, e.guild_id);
    }

    gateway_callbacks& callbacks_;
    std::mutex listeners_mtx_;
    listener_ids listener_ids_{};
    std::atomic<bool> attached_{false};
};

} // namespace discusy
