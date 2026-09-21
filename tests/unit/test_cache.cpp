#include <catch2/catch_test_macros.hpp>
#include <discusy/caches.hpp>
#include <discusy/intents.hpp>
#include <string>

struct test_entity {
    discusy::snowflake id;
    std::string name;
    int counter{0};
};

TEST_CASE("Cache: Grouped cache insertion, querying, and unlinking", "[cache]") {
    discusy::cache<discusy::snowflake, test_entity, discusy::snowflake> cache;
    cache.set_enabled(true);

    discusy::snowflake g1{100};
    discusy::snowflake g2{200};
    discusy::snowflake item_a{1};
    discusy::snowflake item_b{2};

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
    bool modified = cache.modify(item_a, [](test_entity& e) {
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

TEST_CASE("Cache: Required intents calculation", "[cache]") {
    using namespace discusy;

    auto flags_all = cache_flags::all;
    auto intents_all = cache_manager::required_intents(flags_all);

    // Verify guild, members, voice state, and emoji intents are required when all caches enabled
    CHECK(contains_bit(intents_all, intent::guilds));
    CHECK(contains_bit(intents_all, intent::guild_members));
    CHECK(contains_bit(intents_all, intent::guild_emojis_and_stickers));
    CHECK(contains_bit(intents_all, intent::guild_voice_states));

    // When only guilds flag is set, only intent::guilds is needed
    auto intents_guilds = cache_manager::required_intents(cache_flags::guilds);
    CHECK(intents_guilds == intent::guilds);
}
