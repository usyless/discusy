#include "../common.hpp"

#include <format>
#include <print>
#include <string>

int main() {
    auto token = get_bot_token();
    if (token.empty()) {
        std::println(stderr, "Error: DISCORD_BOT_TOKEN environment variable is not set.");
        std::println(stderr, "Usage: export DISCORD_BOT_TOKEN=\"your_bot_token_here\"");
        return 1;
    }

    // Configure required gateway intents to receive these events.
    // Keep in mind: MESSAGE_CONTENT and GUILD_MEMBERS are privileged gateway intents that must
    // be explicitly enabled in your bot's Discord Developer Portal settings, otherwise the gateway
    // connection will be rejected with an invalid intents error code.
    //
    // By default we run on a single thread. When threads = 1, all callbacks and coroutines execute
    // deterministically on the single IO thread, which means you can safely read and modify bot
    // state (like caching users or message history) without needing mutexes or atomics.
    bot bot{config{
        .token{std::move(token)},
        .intents = DEFAULT_INTENTS
            | intent::guild_messages
            | intent::message_content
            | intent::guild_message_reactions
            | intent::guild_message_typing
            | intent::guild_members,
        .threads = 1,
    }};

    // Event 1: All Shards Ready
    // Dispatched when all gateway shards have successfully connected and identified
    bot.on_shards_ready([](const user::user& me) {
        std::println("[SHARDS READY] Logged in as {} (ID: {})", me.username, me.id);
    });

    // Event 2: Gateway Ready (Per-shard)
    // Dispatched when an individual shard gateway session is initialized
    bot.gateway_callbacks.on_ready([](const recieve_event::ready& r) {
        std::println("[GATEWAY READY] Shard connected. Session ID: {}", r.session_id);
    });

    // Event 3: Message Create
    // Callbacks in discusy can be either standard functions or coroutines (returning coro::awaitable<void>).
    // Standard synchronous callbacks have lower overhead and are ideal for quick tasks using token::detached.
    // Coroutines are great when you want to co_await sequential async operations (like typing, fetching data, or awaiting API replies).
    //
    // Important note on parameter passing: synchronous callbacks receive const references,
    // but if you write custom coroutines that co_await across suspension points, pass parameters by value
    // to avoid dangling references when stack frames suspend!
    bot.gateway_callbacks.on_message_create([](const recieve_event::message_create& msg) -> coro::awaitable<void> {
        // Ignore messages sent by bots (including our own)
        if (msg->author.bot.value_or(false)) {
            co_return;
        }

        std::println("[MESSAGE CREATE] Author: {} | Content: \"{}\"", msg->author.username, msg->content);

        // Simple prefix command demo using message reply
        if (msg->content == "!ping") {
            // Option A: Using co_await with token::t_deferred returns [ec, reply_msg]
            // This is useful if you want to inspect the sent message or check for error codes
            auto [ec, reply_msg] = co_await msg.reply("Pong from events example! \xF0\x9F\x8F\x93", false)(token::t_deferred);
            if (ec) {
                std::println(stderr, "Failed to reply to message: {}", ec.message());
            }

            // Option B: Fire-and-forget without coroutine suspension
            // msg.reply<false>("Pong from events example! \xF0\x9F\x8F\x93")(token::detached);

            // Option C: Callbacks! Also no coroutine suspension
            // msg.reply("Pong from events example! \xF0\x9F\x8F\x93", false)([](boost::system::error_code ec, const api::result<void>&) {
            //     if (ec) {
            //         std::println(stderr, "Failed to reply to message: {}", ec.message());
            //     }
            // });
            // Why is the result <void>? All the helper methods by default ignore parsing the return value of the result (equivalent to <false> on the http api)
            // If you wish to have the actual result value use the template <true> on the method call

            // Option C: Add an emoji reaction instead
            // msg.add_reaction<false>("\xF0\x9F\x8F\x93")(token::detached);
        }

        co_return;
    });

    // Event 4: Message Update
    // Dispatched when a message is edited or pinned
    bot.gateway_callbacks.on_message_update([](const recieve_event::message_update& msg) {
        std::println("[MESSAGE UPDATE] Message ID: {} | New Content: \"{}\"", msg->id, msg->content);
    });

    // Event 5: Message Delete
    // Note that Discord does not send message content when a message is deleted, only its ID and channel ID
    bot.gateway_callbacks.on_message_delete([](const recieve_event::message_delete& del) {
        std::println("[MESSAGE DELETE] Message ID: {} was deleted in channel: {}", del.id, del.channel_id);
    });

    // Event 6: Reaction Add
    bot.gateway_callbacks.on_message_reaction_add([](const recieve_event::message_reaction_add& react) {
        const auto emoji_name = react.emoji.name.value_or("unknown");
        std::println("[REACTION ADD] User {} reacted with '{}' on message {}", react.user_id, emoji_name, react.message_id);
    });

    // Event 7: Guild Member Add (requires GUILD_MEMBERS intent)
    bot.gateway_callbacks.on_guild_member_add([](const recieve_event::guild_member_add& m) {
        if (m->user) {
            std::println("[MEMBER JOIN] User {} (ID: {}) joined guild {}", m->user->username, m->user->id, m().guild_id);
        }
    });

    // Event 8: Typing Start
    bot.gateway_callbacks.on_typing_start([](const recieve_event::typing_start& typing) {
        std::println("[TYPING] User {} is typing in channel {}", typing.user_id, typing.channel_id);
    });

    std::println("Starting event listener example bot...");
    bot.run();
    return 0;
}
