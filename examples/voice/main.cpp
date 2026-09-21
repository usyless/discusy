#include "../common.hpp"

#include <cmath>
#include <cstdlib>
#include <format>
#include <memory>
#include <numbers>
#include <print>
#include <string>
#include <unordered_map>
#include <vector>

// Generates a 440 Hz (concert A) sine wave tone formatted as 48kHz stereo signed 16-bit PCM
static std::vector<std::int16_t> generate_sine_wave(const double duration_seconds = 3.0, const double frequency = 440.0) {
    constexpr double amplitude = 10000.0; // Moderate volume level for 16-bit audio

    const auto total_frames = static_cast<std::size_t>(discusy::voice::DISCORD_SAMPLING_RATE * duration_seconds);
    std::vector<std::int16_t> pcm(total_frames * discusy::voice::DISCORD_CHANNELS);

    for (size_t i = 0; i < total_frames; ++i) {
        const double t = static_cast<double>(i) / discusy::voice::DISCORD_SAMPLING_RATE;
        const auto sample = static_cast<std::int16_t>(amplitude * std::sin(2.0 * std::numbers::pi * frequency * t));
        pcm[i * discusy::voice::DISCORD_CHANNELS] = sample;       // Left channel
        pcm[(i * discusy::voice::DISCORD_CHANNELS) + 1] = sample; // Right channel
    }

    return pcm;
}

int main() {
    auto token = get_bot_token();
    if (token.empty()) {
        std::println(stderr, "Error: DISCORD_BOT_TOKEN environment variable is not set.");
        std::println(stderr, "Usage: export DISCORD_BOT_TOKEN=\"your_bot_token_here\"");
        return 1;
    }

    // Setting add_voice_intent = true ensures voice intents are added
    bot bot{config{
        .token{std::move(token)},
        .threads = 1,
        .audio_threads = 1,
    }, true};

    // Map to keep track of active voice connections per guild
    std::unordered_map<snowflake, bot::voice_client::connection_ref> active_connections;

    // Register voice slash commands
    std::atomic<callback_id> shards_ready_id{0};
    shards_ready_id = bot.on_shards_ready([&bot, &shards_ready_id](const user::user& me) {
        if (const auto id = shards_ready_id.load()) {
            bot.on_shards_ready.unregister(id);
        }

        // Even with unregistering, it can still be called again in a multi-threaded situation
        // So this doesn't apply here but it is added for clarity anyway
        static std::once_flag flag;

        std::call_once(flag, [&bot, &me](){
            std::println("Logged in as {} (ID: {})", me.username, me.id);
            std::println("Registering voice slash commands...");

            auto commands = h::cmd::commands(
                h::cmd::slash("join", "Joins a specified voice channel",
                    h::cmd::channel_opt("channel", "The voice channel to join", true)
                ),
                h::cmd::slash("play_tone", "Plays a 440 Hz test sine wave in the connected voice channel"),
                h::cmd::slash("leave", "Leaves the voice channel in this server")
            );

            // Can also be defined as follows:
            // We use discusy::make_array to prevent the std::initialiser_list copying

            // auto commands = make_array<api::application_commands::application_command>(
            //     api::application_commands::application_command::create("join", "Joins a specified voice channel")
            //         .set_options(
            //             discusy::application_commands::application_command_option::create(
            //                 discusy::application_commands::application_command_option_type::CHANNEL,
            //                 "channel", "The voice channel to join"
            //             ).set_required(true)
            //         ),
            //     api::application_commands::application_command::create("play_tone", "Plays a 440 Hz test sine wave in the connected voice channel"),
            //     api::application_commands::application_command::create("leave", "Leaves the voice channel in this server")
            // );

            // Token detached runs this and ignores all errors and such
            // The <false> is used to ignore the returned json from the command, avoiding a parse when it isn't needed
            bot.api.bulk_overwrite_global_application_commands<false>(commands)(token::detached);
            std::println("Voice commands registered.");
        });
    });

    // Handle voice interactions
    // This is written as a coroutine, it does not need to be, this does have a bit more overhead compared to a normal callback but can
    // make the syntax simpler for some situations
    // Important thing to note: while the callbacks in this library require you to use const references if you write your own coroutines
    // you should pass by value
    bot.gateway_callbacks.on_interaction_create([&bot, &active_connections](const recieve_event::interaction_create& e) -> coro::awaitable<void> {
        if (!e.is_command()) co_return;

        const auto* cmd = e.command_data();
        if (!cmd) co_return;

        if (!e.in_guild()) {
            // We could use a coroutine and co_await this here, but there is no need
            e.reply_ephemeral<false>("Voice commands can only be used in a server!")(token::detached);
            co_return;
        }

        const auto guild_id = e.guild_id_of();
        const auto& name = cmd->name;

        // /join <channel>
        if (name == "join") {
            const auto channel_id = cmd->get_snowflake("channel");
            if (!channel_id) {
                e.reply_ephemeral<false>("Please provide a valid voice channel.")(token::detached);
                co_return;
            }

            // Using the token t_deferred returns the result of defer as a tuple:
            // that is [ec, value], by default with the deferred or awaitable tokens
            // if an error code is returned it will be thrown as an exception
            // which may be wanted in some cases, but not here
            // 
            // We could have also used token::as_tuple(token::deferred)
            //
            // Also as we are deferring here, "thinking", we must edit_reply afterwards
            // instead of replying as this counts as the initial reply
            co_await e.defer(false)(token::t_deferred);

            // Attempt to join the voice channel
            auto [join_ec, conn] = co_await bot.voice.join(guild_id, *channel_id, {
                .muted = false,
                .deaf = true,
                .on_create = [](bot::voice_client::connection& conn) {
                    // Here we can attach stuff to the connection before anything happens, so if we want to listen
                    // to the connection closing we would attach it here rather than later to be sure it hasnt closed
                    // by the time we attach it

                    conn.on_closed([](const voice::voice_closed& e) {
                        std::println("Voice connection closed for guild id: {}", e.guild_id);
                    });

                    // We can also listen for the other on_ events

                    conn.on_channel_move([](const voice::channel_moved& e) {
                        std::println("Voice connection channel moved from channel with id {} to {}", e.old_channel_id, e.new_channel_id);
                    });

                    conn.on_audio_stopped([](const voice::audio_stopped& stopped) {
                        std::println("Audio playback finished. Reason code: {}", std::to_underlying(stopped.reason));
                    });

                    // Keep in mind this callback won't fire if we are re-using an existing connection
                },
            })(token::t_deferred);

            if (join_ec || !conn) {
                e.edit_reply<false>("Failed to join the voice channel. Check bot permissions.")(token::detached);
                co_return;
            }

            // This is safe here as the entire bot is only running on a single thread, in a multi threaded bot this would
            // not be a safe assignment without a mutex
            active_connections[guild_id] = conn;
            e.edit_reply<false>(std::format("Successfully joined <#{}>! \xF0\x9F\x8E\xA7", channel_id->value))(token::detached);
        }
        // /play_tone
        else if (name == "play_tone") {
            const auto it = active_connections.find(guild_id);
            if (it == active_connections.end() || !it->second) {
                e.reply_ephemeral<false>("I am not connected to a voice channel in this server! Use `/join` first.")(token::detached);
                co_return;
            }

            e.reply<false>("Playing 440 Hz test tone for 3 seconds... \xF0\x9F\x8E\xB5")(token::detached);

            // Generate PCM 48kHz stereo sine wave and transmit asynchronously
            // This method has a bit more overhead than the synchronous send_pcm however it will not block the main thread
            // for as long, as it will chunk up the encode and post itself to the back of the queue occasionally
            it->second->send_pcm_async(generate_sine_wave(3.0, 440.0));
        }
        // /leave
        else if (name == "leave") {
            if (!active_connections.contains(guild_id)) {
                e.reply_ephemeral<false>("Not connected to any voice channel here.")(token::detached);
                co_return;
            }

            bot.voice.disconnect(guild_id);
            active_connections.erase(guild_id);

            e.reply<false>("Disconnected from voice channel. \xF0\x9F\x91\x8B")(token::detached);
        }

        co_return;
    });

    std::println("Starting voice example bot...");
    bot.run();
    return 0;
}
