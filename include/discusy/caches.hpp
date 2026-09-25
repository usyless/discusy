#pragma once

#include <atomic>
#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
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

struct guild_scoped_id {
    snowflake guild_id{};
    snowflake id{};

    [[nodiscard]] constexpr bool operator==(const guild_scoped_id& other) const noexcept = default;
    [[nodiscard]] constexpr auto operator<=>(const guild_scoped_id& other) const noexcept = default;
};

}

template <>
struct std::hash<discusy::guild_scoped_id> {
    [[nodiscard]] constexpr std::size_t operator()(const discusy::guild_scoped_id& k) const noexcept {
        std::uint64_t h = k.guild_id.value * 0x9E3779B97F4A7C15ULL;
        h ^= k.id.value + 0x9E3779B97F4A7C15ULL + (h << 6) + (h >> 2);
        return static_cast<std::size_t>(h);
    }
};

namespace discusy {

namespace cache_detail {

struct guild_scoped_id_hash {
    [[nodiscard]] constexpr std::size_t operator()(const guild_scoped_id& k) const noexcept {
        return std::hash<guild_scoped_id>{}(k);
    }
};

struct no_group {
    [[nodiscard]] constexpr bool operator==(const no_group&) const noexcept = default;
    [[nodiscard]] constexpr auto operator<=>(const no_group&) const noexcept = default;
};

template <typename T> struct hash_for { using type = std::hash<T>; };
template <> struct hash_for<guild_scoped_id> { using type = guild_scoped_id_hash; };
template <> struct hash_for<void> { using type = void; };
template <typename T> using hash_for_t = hash_for<T>::type;

}

template <typename Key, typename Value, typename GroupKey = void>
// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
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

    [[nodiscard]] bool enabled() const noexcept {
        return enabled_.load(std::memory_order_acquire);
    }

    void set_enabled(const bool value) {
        enabled_.store(value, std::memory_order_release);
        if (!value) clear();
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
        const auto& key_idx = container_.template get<by_key>();
        const auto it = key_idx.find(key);
        return (it != key_idx.end()) && bool(it->value);
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
            for (auto it = first; it != last; ++it) {
                if (it->value) out.emplace_back(it->key);
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
            for (auto it = first; it != last; ++it) {
                if (it->value) out.emplace_back(it->value);
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
            for (auto it = first; it != last; ++it) {
                if (it->value) out.emplace_back(it->key, it->value);
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
        auto shared = std::make_shared<const Value>(std::move(value));

        std::scoped_lock lock{mtx_};
        auto& key_idx = container_.template get<by_key>();
        auto it = key_idx.find(key);
        if (it != key_idx.end()) {
            key_idx.modify(it, [&](entry_t& entry) {
                entry.group = group;
                entry.value = std::move(shared);
            });
        } else {
            key_idx.emplace(entry_t{key, group, std::move(shared)});
        }
    }

    void insert(const Key& key, ptr value, const group_key_type& group) {
        std::scoped_lock lock{mtx_};
        auto& key_idx = container_.template get<by_key>();
        auto it = key_idx.find(key);
        if (it != key_idx.end()) {
            key_idx.modify(it, [&](entry_t& entry) {
                entry.group = group;
                entry.value = std::move(value);
            });
        } else {
            key_idx.emplace(entry_t{key, group, std::move(value)});
        }
    }

    template <typename F>
    requires ( std::invocable<F&, Value&> )
    bool modify(const Key& key, F&& f) {
        std::scoped_lock lock{mtx_};
        auto& key_idx = container_.template get<by_key>();
        const auto it = key_idx.find(key);
        if ((it == key_idx.end()) || !it->value) return false;

        auto updated = std::make_shared<Value>(*it->value);
        std::invoke(f, *updated);
        key_idx.modify(it, [&](entry_t& entry) {
            entry.value = std::move(updated);
        });
        return true;
    }

    template <typename F>
    requires ( std::invocable<F&, Value&> )
    void upsert(const Key& key, const group_key_type& group, F&& f) {
        std::scoped_lock lock{mtx_};
        auto& key_idx = container_.template get<by_key>();
        auto it = key_idx.find(key);
        if (it != key_idx.end()) {
            auto updated = it->value ? std::make_shared<Value>(*it->value) : std::make_shared<Value>();
            std::invoke(f, *updated);
            key_idx.modify(it, [&](entry_t& entry) {
                entry.group = group;
                entry.value = std::move(updated);
            });
        } else {
            auto updated = std::make_shared<Value>();
            std::invoke(f, *updated);
            key_idx.emplace(entry_t{key, group, std::move(updated)});
        }
    }

    bool erase(const Key& key) {
        std::scoped_lock lock{mtx_};
        return container_.template get<by_key>().erase(key) > 0;
    }

    void erase_group(const group_key_type& group) {
        std::scoped_lock lock{mtx_};
        container_.template get<by_group>().erase(group);
    }

    void clear() {
        std::scoped_lock lock{mtx_};
        container_.clear();
    }

    void reserve(const std::size_t n) {
        std::scoped_lock lock{mtx_};
        container_.template get<by_key>().reserve(n);
    }

private:
    struct by_key {};
    struct by_group {};

    using key_hash = cache_detail::hash_for_t<Key>;
    using group_hash = cache_detail::hash_for_t<GroupKey>;

    struct entry_t {
        Key key;
        group_key_type group;
        ptr value{};
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

    mutable std::shared_mutex mtx_;
    container_t container_;
    std::atomic_bool enabled_{false};
};

template <typename Key, typename Value>
// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
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

    [[nodiscard]] bool enabled() const noexcept {
        return enabled_.load(std::memory_order_acquire);
    }

    void set_enabled(const bool value) {
        enabled_.store(value, std::memory_order_release);
        if (!value) clear();
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
        map_.insert_or_assign(key, std::make_shared<const Value>(std::move(value)));
    }

    void insert(const Key& key, ptr value) {
        map_.insert_or_assign(key, std::move(value));
    }

    template <typename F>
    requires ( std::invocable<F&, Value&> )
    bool modify(const Key& key, F&& f) {
        bool modified = false;
        map_.visit(key, [&](auto& entry) {
            if (!entry.second) return;
            auto updated = std::make_shared<Value>(*entry.second);
            std::invoke(f, *updated);
            entry.second = std::move(updated);
            modified = true;
        });
        return modified;
    }

    template <typename F>
    requires ( std::invocable<F&, Value&> )
    void upsert(const Key& key, F&& f) {
        const bool visited = map_.visit(key, [&](auto& entry) {
            auto updated = entry.second ? std::make_shared<Value>(*entry.second) : std::make_shared<Value>();
            std::invoke(f, *updated);
            entry.second = std::move(updated);
        }) > 0;
        if (!visited) {
            auto val = std::make_shared<Value>();
            std::invoke(f, *val);
            map_.try_emplace_or_visit(key, val, [&](auto& entry) {
                auto updated = entry.second ? std::make_shared<Value>(*entry.second) : std::make_shared<Value>();
                std::invoke(f, *updated);
                entry.second = std::move(updated);
            });
        }
    }

    bool erase(const Key& key) {
        return map_.erase(key) > 0;
    }

    void clear() {
        map_.clear();
    }

    void reserve(const std::size_t n) {
        map_.reserve(n);
    }

private:
    using key_hash = cache_detail::hash_for_t<Key>;
    boost::unordered::concurrent_flat_map<Key, ptr, key_hash> map_;
    std::atomic_bool enabled_{false};
};

struct cached_guild {
    discusy::guild::guild guild{};
    integer member_count{0};
    bool large{false};
    bool unavailable{false};
    opt<timestamp> joined_at{};
};

namespace cache_detail {

inline void copy_base_guild(discusy::guild::guild& dst, const recieve_event::guild_create_struct& src) {
    dst = src.guild;
}

inline void copy_member_fields(discusy::guild::guild_member& dst, const discusy::guild::guild_member& src) {
    dst = src;
}

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

}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
class cache_manager {
public:
    explicit cache_manager(gateway_callbacks& callbacks) noexcept : callbacks_{callbacks} {}

    cache_manager(const cache_manager&) = delete;
    cache_manager& operator=(const cache_manager&) = delete;
    cache_manager(cache_manager&&) = delete;
    cache_manager& operator=(cache_manager&&) = delete;

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
        return *this;
    }

    cache_manager& disable(const cache_flags flags) {
        apply(flags, false);
        return *this;
    }

    cache_manager& only(const cache_flags flags) {
        apply(cache_flags::all, false);
        return enable(flags);
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
        return members.get(guild_scoped_id{guild_id, user_id});
    }

    [[nodiscard]] auto get_role(const snowflake guild_id, const snowflake role_id) const {
        return roles.get(guild_scoped_id{guild_id, role_id});
    }

    [[nodiscard]] auto get_emoji(const snowflake guild_id, const snowflake emoji_id) const {
        return emojis.get(guild_scoped_id{guild_id, emoji_id});
    }

    [[nodiscard]] auto get_voice_state(const snowflake guild_id, const snowflake user_id) const {
        return voice_states.get(guild_scoped_id{guild_id, user_id});
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
        const auto state = voice_states.get(guild_scoped_id{guild_id, user_id});
        if (!state || !state->channel_id || !*state->channel_id) return std::nullopt;
        return *state->channel_id;
    }

    // everyone currently in the given voice channel
    [[nodiscard]] std::vector<snowflake> voice_channel_members(const snowflake guild_id, const snowflake channel_id) const {
        std::vector<snowflake> out;
        for (const auto& [key, state] : voice_states.snapshot_group(guild_id)) {
            if (!state || !state->channel_id || (*state->channel_id != channel_id)) continue;
            out.emplace_back(key.id);
        }
        return out;
    }

    [[nodiscard]] std::vector<std::shared_ptr<const discusy::voice::voice_state>> voice_channel_states(
        const snowflake guild_id, const snowflake channel_id) const {
        std::vector<std::shared_ptr<const discusy::voice::voice_state>> out;
        for (const auto& state : voice_states.in_group(guild_id)) {
            if (!state || !state->channel_id || (*state->channel_id != channel_id)) continue;
            out.emplace_back(state);
        }
        return out;
    }

    [[nodiscard]] std::size_t voice_channel_member_count(const snowflake guild_id, const snowflake channel_id) const {
        std::size_t count = 0;
        for (const auto& state : voice_states.in_group(guild_id)) {
            if (state && state->channel_id && (*state->channel_id == channel_id)) ++count;
        }
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

private:
    friend class bot;

    void apply(const cache_flags flags, const bool value) {
        if (contains_bit(flags, cache_flags::guilds))       guilds.set_enabled(value);
        if (contains_bit(flags, cache_flags::channels))     channels.set_enabled(value);
        if (contains_bit(flags, cache_flags::members))      members.set_enabled(value);
        if (contains_bit(flags, cache_flags::users))        users.set_enabled(value);
        if (contains_bit(flags, cache_flags::roles))        roles.set_enabled(value);
        if (contains_bit(flags, cache_flags::emojis))       emojis.set_enabled(value);
        if (contains_bit(flags, cache_flags::voice_states)) voice_states.set_enabled(value);
    }

    void attach() {
        if (attached_.exchange(true, std::memory_order_acq_rel)) return;

        callbacks_.on_ready.listen_system([this](const recieve_event::ready& e) { handle_ready(e); });

        callbacks_.on_guild_create.listen_system([this](const recieve_event::guild_create& e) { handle_guild_create(e); });
        callbacks_.on_guild_update.listen_system([this](const recieve_event::guild_update& e) { handle_guild_update(e); });
        callbacks_.on_guild_delete.listen_system([this](const recieve_event::guild_delete& e) { handle_guild_delete(e); });

        callbacks_.on_channel_create.listen_system([this](const recieve_event::channel_create& e) { store_channel(e); });
        callbacks_.on_channel_update.listen_system([this](const recieve_event::channel_update& e) { store_channel(e); });
        callbacks_.on_channel_delete.listen_system([this](const recieve_event::channel_delete& e) { channels.erase(e.id); });

        callbacks_.on_thread_create.listen_system([this](const recieve_event::thread_create& e) { store_channel(e); });
        callbacks_.on_thread_update.listen_system([this](const recieve_event::thread_update& e) { store_channel(e); });
        callbacks_.on_thread_delete.listen_system([this](const recieve_event::thread_delete& e) { channels.erase(e.id); });
        callbacks_.on_thread_list_sync.listen_system([this](const recieve_event::thread_list_sync& e) {
            for (const auto& thread : e.threads) store_channel(thread, e.guild_id);
        });

        callbacks_.on_guild_member_add.listen_system([this](const recieve_event::guild_member_add& e) {
            store_member(e, e.other.guild_id);
            guilds.modify(e.other.guild_id, [](cached_guild& g) { ++g.member_count; });
        });
        callbacks_.on_guild_member_remove.listen_system([this](const recieve_event::guild_member_remove& e) {
            members.erase(guild_scoped_id{e.guild_id, e.user.id});
            voice_states.erase(guild_scoped_id{e.guild_id, e.user.id});
            guilds.modify(e.guild_id, [](cached_guild& g) { if (g.member_count > 0) --g.member_count; });
        });
        callbacks_.on_guild_member_update.listen_system([this](const recieve_event::guild_member_update& e) { handle_member_update(e); });
        callbacks_.on_guild_members_chunk.listen_system([this](const recieve_event::guild_members_chunk& e) {
            for (const auto& member : e.members) store_member(member, e.guild_id);
        });

        callbacks_.on_guild_role_create.listen_system([this](const recieve_event::guild_role_create& e) { store_role(e); });
        callbacks_.on_guild_role_update.listen_system([this](const recieve_event::guild_role_update& e) { store_role(e); });
        callbacks_.on_guild_role_delete.listen_system([this](const recieve_event::guild_role_delete& e) {
            roles.erase(guild_scoped_id{e.guild_id, e.role_id});
        });

        callbacks_.on_guild_emojis_update.listen_system([this](const recieve_event::guild_emojis_update& e) {
            if (!emojis.enabled()) return;

            emojis.erase_group(e.guild_id);
            for (const auto& emoji : e.emojis) store_emoji(emoji, e.guild_id);
        });

        callbacks_.on_user_update.listen_system([this](const recieve_event::user_update& e) {
            if (users.enabled()) {
                auto copy = e;
                attach_bot(copy, callbacks_.bot_ptr_);
                users.insert(e.id, std::move(copy));
            }
        });

        callbacks_.on_voice_state_update.listen_system([this](const recieve_event::voice_state_update& e) { handle_voice_state(e); });
    }

    void handle_ready(const recieve_event::ready& e) {
        if (users.enabled()) {
            auto copy = e.user;
            attach_bot(copy, callbacks_.bot_ptr_);
            users.insert(e.user.id, std::move(copy));
        }

        if (!guilds.enabled() || !e.shard) return;

        const auto shard_id = (*e.shard)[0];
        const auto total_shards = (*e.shard)[1];
        if (total_shards == 0) return;

        std::unordered_set<snowflake> present;
        present.reserve(e.guilds.size());
        for (const auto& guild : e.guilds) present.emplace(guild.id);

        for (const auto& id : guilds.keys()) {
            if (present.contains(id)) continue;
            if (id.guild_shard_id(total_shards) != shard_id) continue; // another shard's problem

            purge_guild(id);
        }
    }

    void handle_guild_create(const recieve_event::guild_create& e) {
        if (const auto* unavailable = std::get_if<discusy::guild::unavailable_guild>(&e)) {
            if (guilds.enabled()) {
                const auto id = unavailable->id;
                guilds.upsert(id, [id](cached_guild& g) {
                    g.guild.id = id;
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
            cached_guild cached{};
            cached.guild = incoming.guild;
            attach_bot(cached.guild, callbacks_.bot_ptr_);
            cached.member_count = incoming.other.member_count;
            cached.large = incoming.other.large;
            cached.unavailable = incoming.other.unavailable.value_or(false);
            cached.joined_at = incoming.other.joined_at;

            guilds.insert(guild_id, std::move(cached));
        }

        if (roles.enabled()) {
            roles.erase_group(guild_id);
            for (const auto& role : incoming.guild.roles) {
                roles.insert(guild_scoped_id{guild_id, role.id}, role, guild_id);
            }
        }

        if (emojis.enabled()) {
            emojis.erase_group(guild_id);
            for (const auto& emoji : incoming.guild.emojis) {
                store_emoji(emoji, guild_id);
            }
        }

        if (channels.enabled()) {
            channels.erase_group(guild_id);
            for (const auto& channel : incoming.other.channels) store_channel(channel, guild_id);
            for (const auto& thread : incoming.other.threads) store_channel(thread, guild_id);
        }

        if (members.enabled() || users.enabled()) {
            for (const auto& member : incoming.other.members) store_member(member, guild_id);
        }

        if (voice_states.enabled()) {
            voice_states.erase_group(guild_id);
            for (const auto& state : incoming.other.voice_states) {
                if (!state.channel_id || !*state.channel_id) continue;

                auto copy = state;
                copy.guild_id = guild_id; // omitted inside GUILD_CREATE
                voice_states.insert(guild_scoped_id{guild_id, state.user_id}, std::move(copy), guild_id);
            }
        }
    }

    void handle_guild_update(const recieve_event::guild_update& e) {
        if (guilds.enabled()) {
            guilds.upsert(e.id, [this, &e](cached_guild& g) {
                g.guild = e;
                attach_bot(g.guild, callbacks_.bot_ptr_);
                g.unavailable = false;
            });
        }

        if (roles.enabled()) {
            roles.erase_group(e.id);
            for (const auto& role : e.roles) roles.insert(guild_scoped_id{e.id, role.id}, role, e.id);
        }

        if (emojis.enabled()) {
            emojis.erase_group(e.id);
            for (const auto& emoji : e.emojis) store_emoji(emoji, e.id);
        }
    }

    void handle_guild_delete(const recieve_event::guild_delete& e) {
        if (e.unavailable) {
            if (guilds.enabled()) {
                guilds.modify(e.id, [](cached_guild& g) {
                    g.unavailable = true;
                });
            }
        } else {
            purge_guild(e.id);
        }
    }

    void handle_member_update(const recieve_event::guild_member_update& e) {
        if (users.enabled()) {
            auto copy = e.user;
            attach_bot(copy, callbacks_.bot_ptr_);
            users.insert(e.user.id, std::move(copy));
        }
        if (!members.enabled()) return;

        members.upsert(guild_scoped_id{e.guild_id, e.user.id}, e.guild_id, [this, &e](discusy::guild::guild_member_with_guild& m) {
            m.other.guild_id = e.guild_id;
            cache_detail::apply_member_update(m.guild_member, e);
            attach_bot(m.guild_member, callbacks_.bot_ptr_);
        });
    }

    void handle_voice_state(const recieve_event::voice_state_update& e) {
        if (!e.guild_id || !*e.guild_id) return;

        const auto guild_id = *e.guild_id;

        if (e.member) store_member(*e.member, guild_id);
        if (!voice_states.enabled()) return;

        if (!e.channel_id || !*e.channel_id) { // left voice
            voice_states.erase(guild_scoped_id{guild_id, e.user_id});
            return;
        }

        voice_states.insert(guild_scoped_id{guild_id, e.user_id}, e, guild_id);
    }

    void store_channel(const discusy::channel::channel& channel, const snowflake guild_id = snowflake{}) {
        if (!channels.enabled()) return;

        auto copy = channel;
        if (guild_id) {
            copy.guild_id = guild_id;
        } else if (!copy.guild_id) {
            if (const auto existing = channels.get(channel.id); existing && existing->guild_id) {
                copy.guild_id = existing->guild_id;
            }
        }

        attach_bot(copy, callbacks_.bot_ptr_);
        const auto group = copy.guild_id.value_or(snowflake{});
        channels.insert(channel.id, std::move(copy), group);
    }

    void store_member(const discusy::guild::guild_member& member, const snowflake guild_id) {
        if (!member.user) return;

        const auto user_id = member.user->id;

        if (users.enabled()) {
            auto user_copy = *member.user;
            attach_bot(user_copy, callbacks_.bot_ptr_);
            users.insert(user_id, std::move(user_copy));
        }
        if (!members.enabled() || !guild_id) return;

        discusy::guild::guild_member_with_guild stored{};
        stored.guild_member = member;
        stored.guild_member.set_guild_id_context_void(guild_id);
        attach_bot(stored.guild_member, callbacks_.bot_ptr_);
        stored.other.guild_id = guild_id;

        members.insert(guild_scoped_id{guild_id, user_id}, std::move(stored), guild_id);
    }

    void store_member(const discusy::guild::guild_member_with_guild& member, const snowflake guild_id = snowflake{}) {
        const auto effective_guild_id = guild_id ? guild_id : member.other.guild_id;
        store_member(member.guild_member, effective_guild_id);
    }

    void store_role(const recieve_event::guild_role_create& e) {
        if (!roles.enabled()) return;
        roles.insert(guild_scoped_id{e.guild_id, e.role.id}, e.role, e.guild_id);
    }

    void store_emoji(const discusy::emoji::emoji& emoji, const snowflake guild_id) {
        if (!emojis.enabled() || !emoji.id || !*emoji.id) return; // unicode emoji, not ours to cache
        emojis.insert(guild_scoped_id{guild_id, *emoji.id}, emoji, guild_id);
    }

    gateway_callbacks& callbacks_;
    std::atomic<bool> attached_{false};
};

}
