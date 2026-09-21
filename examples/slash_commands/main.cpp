#include "../common.hpp"

#include <chrono>
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

    // Default to running on a single thread. For most bots, running on 1 thread avoids the need
    // for mutexes or atomic synchronisation when updating shared bot state.
    bot bot{config{
        .token{std::move(token)},
        .threads = 1,
    }};

    // Register slash commands when shards are ready
    std::atomic<callback_id> shards_ready_id{0};
    shards_ready_id = bot.on_shards_ready([&bot, &shards_ready_id](const user::user& me) {
        if (const auto id = shards_ready_id.load()) {
            bot.on_shards_ready.unregister(id);
        }

        // Even with unregistering, it can still be called again if shards reconnect or in a multi-threaded bot,
        // so std::call_once ensures slash commands are registered only once per bot lifetime
        static std::once_flag flag;

        std::call_once(flag, [&bot, &me]() {
            std::println("Logged in as {} (ID: {})", me.username, me.id);
            std::println("Registering slash commands...");

            // Helpers provide a concise, readable way to declare commands and options
            auto commands = h::cmd::commands(
                // /ping - Simple instantaneous reply
                h::cmd::slash("ping", "Responds with pong!"),

                // /echo <message> - Reads a string option and echoes it back
                h::cmd::slash("echo", "Echoes back the message you provide",
                    h::cmd::string_opt("message", "The text to repeat", true)
                ),

                // /secret - Sends an ephemeral reply visible only to the invoking user
                h::cmd::slash("secret", "Replies with a private, ephemeral message"),

                // /slow - Demonstrates deferring the response while processing
                h::cmd::slash("slow", "Demonstrates deferred thinking state before replying"),

                // /slow_eager - Demonstrates eagerly deferring the response while processing - this is mainly code differences
                h::cmd::slash("slow_eager", "Demonstrates eager deferred thinking state before replying")
            );

            // Can also be defined using raw types instead of helpers:
            // We use discusy::make_array to avoid std::initializer_list copying
            // auto commands = make_array<api::application_commands::application_command>(
            //     api::application_commands::application_command::create("ping", "Responds with pong!"),
            //     api::application_commands::application_command::create("echo", "Echoes back the message you provide")
            //         .set_options(
            //             application_commands::application_command_option::create(
            //                 application_commands::application_command_option_type::STRING,
            //                 "message", "The text to repeat"
            //             ).set_required(true)
            //         ),
            //     api::application_commands::application_command::create("secret", "Replies with a private, ephemeral message"),
            //     api::application_commands::application_command::create("slow", "Demonstrates deferred thinking state before replying")
            // );

            // Token detached runs this asynchronously and ignores errors/returns
            // The <false> template parameter tells discusy not to parse the returned JSON response,
            // which saves an unnecessary allocation and parse since we don't inspect the returned command array
            bot.api.bulk_overwrite_global_application_commands<false>(commands)(token::detached);
            std::println("Slash commands registered.");
        });
    });

    // Handle incoming interactions (slash commands)
    // This is written as a coroutine, but it doesn't strictly have to be. Coroutines carry slight allocation/frame
    // overhead compared to regular callbacks, but make asynchronous sequential logic much easier to read.
    // Important thing to note: while callbacks in this library pass parameters by const reference,
    // if you write your own coroutines you should pass parameters by value to avoid lifetime pitfalls across suspension points.
    bot.gateway_callbacks.on_interaction_create([&bot](const recieve_event::interaction_create& e) -> coro::awaitable<void> {
        if (!e.is_command()) {
            co_return;
        }

        const auto* cmd = e.command_data();
        if (!cmd) {
            co_return;
        }

        const auto& name = cmd->name;

        // /ping
        if (name == "ping") {
            // We could co_await e.reply(...) here, but there is no need to suspend the coroutine
            // when we just want to fire and forget. Using token::detached and <false> is optimal here.
            e.reply<false>("Pong! \xF0\x9F\x8F\x93")(token::detached);
            co_return;
        }

        // /echo
        if (name == "echo") {
            // There are multiple ways to retrieve options:
            // 1. cmd->get_string("name")
            // 2. cmd->get_integer, cmd->get_bool, cmd->get_snowflake, cmd->get_double
            // 3. h::get_string(cmd, "name") or e.get_string("name")
            const auto message = cmd->get_string("message").value_or("Nothing provided.");

            // Here we use token::t_deferred to demonstrate awaiting with error checking.
            // Using token::t_deferred returns [ec, result] as a tuple instead of throwing on HTTP errors.
            // (You can also use token::as_tuple(token::deferred))
            auto [ec, _] = co_await e.reply(std::format("Echo: {}", message))(token::t_deferred);
            if (ec) {
                std::println(stderr, "Failed to reply to echo command: {}", ec.message());
            }
            co_return;
        }

        // /secret
        if (name == "secret") {
            // Ephemeral replies are only visible to the user who invoked the command.
            // Again, token::detached is great for fire-and-forget replies.
            e.reply_ephemeral<false>("Shh! Only you can see this message. \xF0\x9F\xA4\xAB")(token::detached);
            co_return;
        }

        // /slow
        if (name == "slow") {
            // Discord requires an interaction response within 3 seconds. If your processing takes longer,
            // defer first to show the bot's "thinking..." state.
            // Note that e.defer(false) is for public thinking; e.defer_ephemeral() or e.defer(true) is for private thinking.
            // (There is also e.thinking(ephemeral) as an alias)
            co_await e.defer(false)(token::t_deferred);

            // Simulate doing asynchronous work without blocking the thread
            co_await bot.sleep(std::chrono::seconds(2));

            // Once deferred, you MUST use edit_reply (or followup) because the initial acknowledgement
            // was already consumed by deferring!
            e.edit_reply<false>("Finished processing! Here is your answer. \xE2\x9C\x85")(token::detached);
            co_return;
        }

        // /slow_eager
        if (name == "slow_eager") {
            // Here we launch the thinking eagerly, so that it happens concurrently while we perform work
            // instead of waiting for a response back first.
            // This does cause more issues of then remembering to await it before performing other message requests
            // to ensure no race-conditions between message edits occur on the clients viewing the message
            auto thinking = e.defer(false)(token::t_eager);

            // Simulate doing asynchronous work without blocking the thread
            co_await bot.sleep(std::chrono::seconds(2));

            // Now we can await the thinking to ensure it has finished, ideally this returns immediately
            // as it should be done by the time the 2 seconds is up
            co_await std::move(thinking);

            // Once deferred, you MUST use edit_reply (or followup) because the initial acknowledgement
            // was already consumed by deferring!
            e.edit_reply<false>("Finished processing! Here is your answer. \xE2\x9C\x85")(token::detached);
            co_return;
        }

        co_return;
    });

    std::println("Starting slash commands example bot...");
    bot.run();
    return 0;
}
