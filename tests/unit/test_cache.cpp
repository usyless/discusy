#include <catch2/catch_test_macros.hpp>
#include <discusy/caches.hpp>
#include <discusy/intents.hpp>
#include <discusy/io_context.hpp>
#include <atomic>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

struct test_entity {
    discusy::snowflake id{};
    std::string name{};
    int counter{0};
};

TEST_CASE("Cache: Grouped cache insertion, querying, and unlinking", "[cache]") {
    discusy::cache<discusy::snowflake, test_entity, discusy::snowflake> cache;
    cache.set_enabled(true);

    const discusy::snowflake g1{100};
    const discusy::snowflake g2{200};
    const discusy::snowflake item_a{1};
    const discusy::snowflake item_b{2};

    // Insert into group 1
    cache.insert(item_a, test_entity{.id=item_a, .name="alpha", .counter=10}, g1);
    cache.insert(item_b, test_entity{.id=item_b, .name="beta", .counter=20}, g1);

    CHECK(cache.size() == 2);
    CHECK(cache.group_size(g1) == 2);
    CHECK(cache.group_size(g2) == 0);

    // Verify lookup by key
    auto ptr_a = cache.get(item_a);
    REQUIRE(ptr_a != nullptr);
    CHECK(ptr_a->name == "alpha");
    CHECK(ptr_a->counter == 10);

    // Migration: insert item_a into g2. Should automatically unlink from g1
    cache.insert(item_a, test_entity{.id=item_a, .name="alpha-moved", .counter=15}, g2);
    CHECK(cache.group_size(g1) == 1);
    CHECK(cache.group_size(g2) == 1);
    CHECK(cache.get(item_a)->name == "alpha-moved");

    // Modify in place
    bool const modified = cache.modify(item_a, [](test_entity& e) {
        e.counter += 5;
    });
    CHECK(modified);
    CHECK(cache.get(item_a)->counter == 20);

    // Erase group 1
    cache.erase_group(g1);
    CHECK(cache.group_size(g1) == 0);
    CHECK_FALSE(cache.contains(item_b));
    CHECK(cache.contains(item_a)); // item_a is in g2, so still present

    // Erasing individual key
    CHECK(cache.erase(item_a));
    CHECK(cache.empty());
}

TEST_CASE("Cache: Null ptr insertion maintains invariants", "[cache]") {
    using grouped_t = discusy::cache<discusy::snowflake, test_entity, discusy::snowflake>;
    using flat_t = discusy::cache<discusy::snowflake, test_entity>;

    grouped_t grouped;
    flat_t flat;
    const discusy::snowflake k1{1};
    const discusy::snowflake g1{10};

    grouped.insert(k1, grouped_t::ptr{}, g1);
    CHECK(grouped.empty());
    CHECK(grouped.empty());
    CHECK(grouped.group_size(g1) == 0);
    CHECK_FALSE(grouped.contains(k1));

    grouped.insert(k1, test_entity{.id=k1, .name="live", .counter=1}, g1);
    CHECK(grouped.size() == 1);
    grouped.insert(k1, grouped_t::ptr{}, g1);
    CHECK(grouped.empty());
    CHECK(grouped.empty());
    CHECK(grouped.group_size(g1) == 0);
    CHECK_FALSE(grouped.contains(k1));

    flat.insert(k1, flat_t::ptr{});
    CHECK(flat.empty());
    CHECK(flat.empty());
    CHECK_FALSE(flat.contains(k1));

    flat.insert(k1, test_entity{.id=k1, .name="live", .counter=1});
    CHECK(flat.size() == 1);
    flat.insert(k1, flat_t::ptr{});
    CHECK(flat.empty());
    CHECK(flat.empty());
    CHECK_FALSE(flat.contains(k1));
}

TEST_CASE("Cache: Ungrouped upsert exact-once semantics and exception safety", "[cache]") {
    discusy::cache<discusy::snowflake, test_entity> cache;
    const discusy::snowflake k1{42};
    const discusy::snowflake k2{99};

    int calls = 0;
    cache.upsert(k1, [&](test_entity& e) {
        ++calls;
        e.id = k1;
        e.counter += 10;
    });
    CHECK(calls == 1);
    REQUIRE(cache.contains(k1));
    CHECK(cache.get(k1)->counter == 10);

    cache.upsert(k1, [&](test_entity& e) {
        ++calls;
        e.counter += 5;
    });
    CHECK(calls == 2);
    CHECK(cache.get(k1)->counter == 15);

    // Exception during initial upsert must not leave a zombie null entry
    CHECK_THROWS_AS(cache.upsert(k2, [](test_entity&) {
        throw std::runtime_error("boom");
    }), std::runtime_error);
    CHECK_FALSE(cache.contains(k2));
    CHECK(cache.get(k2) == nullptr);
    CHECK(cache.size() == 1);

    // Exception during existing-entry upsert must leave previous value intact
    CHECK_THROWS_AS(cache.upsert(k1, [](test_entity& e) {
        e.counter = 999;
        throw std::runtime_error("boom2");
    }), std::runtime_error);
    REQUIRE(cache.get(k1) != nullptr);
    CHECK(cache.get(k1)->counter == 15);
}

TEST_CASE("Cache: Concurrent ungrouped upsert exact-once under contention", "[cache]") {
    discusy::cache<discusy::snowflake, test_entity> cache;
    const discusy::snowflake key{777};
    constexpr int num_threads = 4;
    constexpr int iters_per_thread = 500;
    std::atomic<int> invocations{0};

    std::vector<std::thread> threads;
    threads.reserve(num_threads);
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&] {
            for (int i = 0; i < iters_per_thread; ++i) {
                cache.upsert(key, [&](test_entity& e) {
                    invocations.fetch_add(1, std::memory_order_relaxed);
                    ++e.counter;
                });
            }
        });
    }
    for (auto& th : threads) th.join();

    REQUIRE(cache.get(key) != nullptr);
    CHECK(invocations.load() == num_threads * iters_per_thread);
    CHECK(cache.get(key)->counter == num_threads * iters_per_thread);
}

TEST_CASE("Cache: Required intents calculation", "[cache]") {
    using namespace discusy;

    const auto flags_all = cache_flags::all;
    const auto intents_all = cache_manager::required_intents(flags_all);

    // Verify guild, members, voice state, and emoji intents are required when all caches enabled
    CHECK(contains_bit(intents_all, intent::guilds));
    CHECK(contains_bit(intents_all, intent::guild_members));
    CHECK(contains_bit(intents_all, intent::guild_emojis_and_stickers));
    CHECK(contains_bit(intents_all, intent::guild_voice_states));

    // When only guilds flag is set, only intent::guilds is needed
    const auto intents_guilds = cache_manager::required_intents(cache_flags::guilds);
    CHECK(intents_guilds == intent::guilds);
}

TEST_CASE("Cache manager: Lazy listener registration, detachment, and lifetime safety", "[cache]") {
    using namespace discusy;

    ctx::io_context io_ctx{1};
    gateway_callbacks cbs{io_ctx};

    {
        cache_manager mgr{cbs};
        mgr.attach();

        // No flags enabled -> zero listeners attached
        CHECK_FALSE(cbs.on_ready.has_listeners());
        CHECK_FALSE(cbs.on_guild_create.has_listeners());
        CHECK_FALSE(cbs.on_voice_state_update.has_listeners());
        CHECK_FALSE(cbs.on_channel_create.has_listeners());

        // Enabling only guilds attaches only guild-relevant listeners
        mgr.enable(cache_flags::guilds);
        CHECK(cbs.on_ready.has_listeners());
        CHECK(cbs.on_guild_create.has_listeners());
        CHECK(cbs.on_guild_update.has_listeners());
        CHECK(cbs.on_guild_delete.has_listeners());
        CHECK(cbs.on_guild_member_add.has_listeners());
        CHECK(cbs.on_guild_member_remove.has_listeners());
        CHECK_FALSE(cbs.on_channel_create.has_listeners());
        CHECK_FALSE(cbs.on_guild_role_create.has_listeners());
        CHECK_FALSE(cbs.on_guild_emojis_update.has_listeners());
        CHECK_FALSE(cbs.on_voice_state_update.has_listeners());

        // Direct toggle on a public cache also syncs listeners
        mgr.channels.set_enabled(true);
        CHECK(cbs.on_channel_create.has_listeners());
        CHECK(cbs.on_thread_list_sync.has_listeners());

        // only() preserves already-enabled caches that remain selected
        const snowflake gid{1000};
        mgr.guilds.insert(gid, cached_guild{.guild=guild::guild::create(gid, "kept")});
        mgr.channels.insert(snowflake{2000}, channel::channel::create(snowflake{2000}), gid);

        mgr.only(cache_flags::guilds);
        CHECK(mgr.guilds.enabled());
        CHECK_FALSE(mgr.channels.enabled());
        REQUIRE(mgr.get_guild(gid) != nullptr);
        CHECK(mgr.get_guild(gid)->guild.name == "kept");
        CHECK(mgr.channels.empty());
        CHECK_FALSE(cbs.on_channel_create.has_listeners());
    }

    // After cache_manager destruction, all listeners must be unregistered (no UAF)
    CHECK_FALSE(cbs.on_ready.has_listeners());
    CHECK_FALSE(cbs.on_guild_create.has_listeners());
    CHECK_FALSE(cbs.on_guild_delete.has_listeners());

    // Firing an event after cache_manager destruction must be completely safe
    recieve_event::guild_delete del{};
    del.id = snowflake{1000};
    cbs.on_guild_delete.fire(std::move(del));
    io_ctx->run();
}

TEST_CASE("Cache manager: Guild create, ready purge without shard/guilds, and voice queries", "[cache]") {
    using namespace discusy;

    ctx::io_context io_ctx{1};
    gateway_callbacks cbs{io_ctx};
    cache_manager mgr{cbs};
    mgr.enable(cache_flags::all);
    mgr.attach();

    const snowflake g1{1ULL << 22U};
    const snowflake g2{2ULL << 22U};
    const snowflake dm_ch{9001};
    const snowflake vc1{3001};
    const snowflake vc2{3002};
    const snowflake u1{4001};
    const snowflake u2{4002};

    // Populate a DM channel (guild_id = nullopt -> group 0)
    cbs.on_channel_create.fire(channel::channel::create(dm_ch, channel::channel_type::DM));

    // Populate two guilds via GUILD_CREATE
    const auto make_guild_create = [&](snowflake gid, snowflake ch_id, snowflake user_id, snowflake voice_ch) {
        recieve_event::guild_create_struct gc{};
        gc.guild = guild::guild::create(gid, "guild");
        gc.guild.roles.push_back(permissions::role::create(snowflake{gid.value + 1}, "role"));
        gc.guild.emojis.push_back(emoji::emoji::create(snowflake{gid.value + 2}, "emoji"));
        gc.guild.emojis.push_back(emoji::emoji::create(std::nullopt, "unicode")); // skipped
        gc.other.member_count = 1;
        gc.other.channels.push_back(channel::channel::create(ch_id));

        guild::guild_member mem{};
        mem.user = user::user::create(user_id, "user");
        gc.other.members.push_back(std::move(mem));

        voice::voice_state vs{};
        vs.user_id = user_id;
        vs.channel_id = voice_ch;
        gc.other.voice_states.push_back(std::move(vs));
        return recieve_event::guild_create{std::move(gc)};
    };

    cbs.on_guild_create.fire(make_guild_create(g1, vc1, u1, vc1));
    cbs.on_guild_create.fire(make_guild_create(g2, vc2, u2, vc2));

    // Add second user into g1's vc1 via voice_state_update
    {
        recieve_event::voice_state_update vsu{};
        vsu.guild_id = g1;
        vsu.channel_id = vc1;
        vsu.user_id = u2;
        cbs.on_voice_state_update.fire(std::move(vsu));
    }

    io_ctx->run();
    io_ctx->restart();

    CHECK(mgr.guilds.size() == 2);
    CHECK(mgr.channels.size() == 3); // dm_ch + vc1 + vc2
    CHECK(mgr.roles.size() == 2);
    CHECK(mgr.emojis.size() == 2); // unicode emoji skipped
    CHECK(mgr.members.size() == 2);
    CHECK(mgr.voice_channel_member_count(g1, vc1) == 2);
    CHECK(mgr.voice_channel_members(g1, vc1).size() == 2);
    CHECK(mgr.voice_channel_states(g1, vc1).size() == 2);
    CHECK(mgr.voice_channel_of(g1, u1) == vc1);

    // Disable guilds cache, then fire unsharded READY containing only g1:
    // g2's channels/roles/emojis/members/voice_states must still be purged, while dm_ch is preserved!
    mgr.disable(cache_flags::guilds);
    CHECK(mgr.guilds.empty());

    recieve_event::ready ready_ev{};
    ready_ev.user = user::user::create(snowflake{9999}, "bot");
    ready_ev.guilds.push_back(guild::unavailable_guild::create(g1, true));
    // Note: ready_ev.shard intentionally left std::nullopt (unsharded READY)
    cbs.on_ready.fire(std::move(ready_ev));

    io_ctx->run();

    CHECK(mgr.get_channel(dm_ch) != nullptr);
    CHECK(mgr.get_channel(vc1) != nullptr);
    CHECK(mgr.get_channel(vc2) == nullptr);
    CHECK(mgr.guild_roles(g1).size() == 1);
    CHECK(mgr.guild_roles(g2).empty());
    CHECK(mgr.guild_emojis(g2).empty());
    CHECK(mgr.guild_members(g2).empty());
    CHECK(mgr.voice_channel_member_count(g2, vc2) == 0);
    CHECK(mgr.voice_channel_member_count(g1, vc1) == 2);
    CHECK(mgr.get_user(snowflake{9999}) != nullptr);
}

TEST_CASE("Cache manager: Concurrent event writes and disable races leave no stale entries", "[cache]") {
    using namespace discusy;

    ctx::io_context io_ctx{2};
    gateway_callbacks cbs{io_ctx};
    cache_manager mgr{cbs};
    mgr.attach();

    std::atomic<bool> stop{false};

    std::thread writer([&] {
        for (std::uint64_t i = 1; i <= 500; ++i) {
            recieve_event::user_update u{};
            u.id = snowflake{i};
            u.username = "u";
            cbs.on_user_update.fire(std::move(u));

            recieve_event::channel_create ch{};
            ch.id = snowflake{i};
            ch.guild_id = snowflake{10};
            cbs.on_channel_create.fire(std::move(ch));
        }
    });

    std::thread toggler([&] {
        for (int i = 0; i < 60; ++i) {
            mgr.enable(cache_flags::users | cache_flags::channels);
            std::this_thread::yield();
            mgr.disable(cache_flags::users | cache_flags::channels);
        }
    });

    std::thread runner([&] {
        while (!stop.load(std::memory_order_acquire)) {
            io_ctx->poll();
            std::this_thread::yield();
        }
        io_ctx->run();
    });

    writer.join();
    toggler.join();
    mgr.disable(cache_flags::all);
    stop.store(true, std::memory_order_release);
    runner.join();

    // After disabling all caches and draining io_ctx, disabled caches MUST be empty
    CHECK(mgr.users.empty());
    CHECK(mgr.channels.empty());
}
