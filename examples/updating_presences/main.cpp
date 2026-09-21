#include "../common.hpp"

#include <chrono>
#include <cstdlib>
#include <format>
#include <print>
#include <string>

// Structure for rotation entries
struct PresenceItem {
    send_event::status_type status;
    recieve_event::presence::activity_type activity;
    std::string text;
};

// Converts a string to status_type
static send_event::status_type parse_status_type(const std::string_view str) {
    if (str == "idle") return send_event::status_type::idle;
    if (str == "dnd") return send_event::status_type::dnd;
    if (str == "invisible") return send_event::status_type::invisible;
    return send_event::status_type::online;
}

// Converts a string to activity_type
static recieve_event::presence::activity_type parse_activity_type(const std::string_view str) {
    if (str == "listening") return recieve_event::presence::activity_type::listening;
    if (str == "watching") return recieve_event::presence::activity_type::watching;
    if (str == "competing") return recieve_event::presence::activity_type::competing;
    if (str == "custom") return recieve_event::presence::activity_type::custom;
    return recieve_event::presence::activity_type::playing;
}

int main() {
    auto token = get_bot_token();
    if (token.empty()) {
        std::println(stderr, "Error: DISCORD_BOT_TOKEN environment variable is not set.");
        std::println(stderr, "Usage: export DISCORD_BOT_TOKEN=\"your_bot_token_here\"");
        return 1;
    }

    // Setting initial_presence directly in config ensures Discord receives your status
    // during the gateway IDENTIFY handshake itself, so the bot never appears briefly offline.
    //
    // By default we run single-threaded (.threads = 1), so timer callbacks and interaction callbacks
    // won't interleave unexpectedly.
    bot bot{config{
        .token{std::move(token)},
        .threads = 1,
        .initial_presence = send_event::update_presence{
            .activities = make_vector(
                recieve_event::presence::activity::create(
                    recieve_event::presence::activity_type::custom,
                    "Starting up discusy bot... \xE2\x8F\xB3"
                )
            ),
            .status = send_event::status_type::online,
        },
    }};

    // Periodic presence rotation list
    const auto rotation = make_array(
        PresenceItem{.status=send_event::status_type::online, .activity=recieve_event::presence::activity_type::playing, .text="Half-Life 3"},
        PresenceItem{.status=send_event::status_type::idle,   .activity=recieve_event::presence::activity_type::listening, .text="Spotify"},
        PresenceItem{.status=send_event::status_type::dnd,    .activity=recieve_event::presence::activity_type::watching, .text="C++23 Tutorials"},
        PresenceItem{.status=send_event::status_type::online, .activity=recieve_event::presence::activity_type::competing, .text="Code Golf"},
        PresenceItem{.status=send_event::status_type::online, .activity=recieve_event::presence::activity_type::custom, .text="Built with discusy! \xF0\x9F\x9A\x80"}
    );

    // Register slash commands and start rotation timer when shards are ready
    std::atomic<callback_id> shards_ready_id{0};
    shards_ready_id = bot.on_shards_ready([&bot, &shards_ready_id, &rotation](const user::user& me) {
        if (const auto id = shards_ready_id.load()) {
            bot.on_shards_ready.unregister(id);
        }

        // Use std::call_once so timer startup and command registration don't duplicate on reconnect
        static std::once_flag flag;

        std::call_once(flag, [&bot, &me, &rotation]() {
            std::println("Logged in as {} (ID: {})", me.username, me.id);

            // Register an interactive slash command to manually test presences
            auto commands = h::cmd::commands(
                h::cmd::slash("set_status", "Manually updates the bot's status and activity",
                    h::cmd::string_opt("status", "The status indicator", true).with_choices(
                        h::choice("Online", "online"),
                        h::choice("Idle", "idle"),
                        h::choice("Do Not Disturb", "dnd"),
                        h::choice("Invisible", "invisible")
                    ),
                    h::cmd::string_opt("type", "The activity type", true).with_choices(
                        h::choice("Playing", "playing"),
                        h::choice("Listening to", "listening"),
                        h::choice("Watching", "watching"),
                        h::choice("Competing in", "competing"),
                        h::choice("Custom status", "custom")
                    ),
                    h::cmd::string_opt("text", "Activity text description", true)
                )
            );

            // <false> skips JSON response parsing since we don't need the returned command entities
            bot.api.bulk_overwrite_global_application_commands<false>(commands)(token::detached);

            // Start a 15-second interval timer for presence rotation.
            // Timers in discusy run on the bot's internal IO context.
            // Since we configured .threads = 1, rotation_index can safely be modified here without a mutex.
            // In a multi-threaded bot, you would need std::atomic or a mutex because timers and interaction
            // handlers could touch state concurrently.
            static std::size_t rotation_index = 0;
            bot.timers.start_interval([&bot, &rotation](timer) {
                const auto& item = rotation[rotation_index % rotation.size()];
                rotation_index++;

                // bot.set_presence updates presence globally across all shards
                // (If you ever need per-shard presence, you can call shard.set_presence directly)
                bot.set_presence(send_event::update_presence{
                    .activities = make_vector(
                        recieve_event::presence::activity::create(item.activity, item.text)
                    ),
                    .status = item.status,
                });

                std::println("[PRESENCE ROTATION] Updated presence to: \"{}\"", item.text);
            }, std::chrono::seconds{15});
        });
    });

    // Manual Presence Update via Slash Command
    bot.gateway_callbacks.on_interaction_create([&bot](const recieve_event::interaction_create& e) {
        if (!e.is_command()) return;

        const auto* cmd = e.command_data();
        if (!cmd || cmd->name != "set_status") return;

        const auto status_str = cmd->get_string("status").value_or("online");
        const auto type_str = cmd->get_string("type").value_or("playing");
        const auto text = cmd->get_string("text").value_or("discusy");

        const auto new_status = parse_status_type(status_str);
        const auto new_activity = parse_activity_type(type_str);

        // Apply new presence immediately
        bot.set_presence(send_event::update_presence{
            .activities = make_vector(
                recieve_event::presence::activity::create(new_activity, text)
            ),
            .status = new_status,
        });

        // Use token::detached for instant response without awaiting
        e.reply<false>(
            std::format("Presence updated: [{}] {} \"{}\" \xE2\x9C\x85", status_str, type_str, text)
        )(token::detached);
    });

    std::println("Starting presence example bot...");
    bot.run();
    return 0;
}
