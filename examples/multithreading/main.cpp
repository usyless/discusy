#include "../common.hpp"

#include <atomic>
#include <chrono>
#include <format>
#include <mutex>
#include <print>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// Thread-Safe Shared State
// When running with multiple threads (.threads > 1), gateway callbacks and coroutines execute
// concurrently across the worker thread pool. Any mutable state accessed by these handlers MUST be
// protected against data races.
//
// In discusy + Boost.Asio, you have three primary tools for concurrency safety:
// 1. std::atomic: For simple, independent metrics or flags with minimal overhead.
// 2. std::mutex / std::scoped_lock: For protecting synchronous access to non-thread-safe containers.
// 3. boost::asio::strand: For sequential, non-concurrent execution of asynchronous handlers/continuations
//    without blocking threads. Strands are also REQUIRED when using token::cancel_after on multiple threads!

// 1. Atomic counters:
static std::atomic<std::uint64_t> g_total_commands{0};
static std::atomic_uint64_t custom_id_counter{0};

// 2. Mutex-protected structures:
struct GuildStats {
    std::uint64_t command_count{0};
    std::string last_command_user;
};

static std::mutex g_guild_mutex;
static std::unordered_map<snowflake, GuildStats> g_guild_data;

int main() {
    auto token = get_bot_token();
    if (token.empty()) {
        std::println(stderr, "Error: DISCORD_BOT_TOKEN environment variable is not set.");
        std::println(stderr, "Usage: export DISCORD_BOT_TOKEN=\"your_bot_token_here\"");
        return 1;
    }

    // Unlike our other examples which set .threads = 1 for single-threaded simplicity, here we explicitly
    // configure multiple worker threads. Discusy's internal boost::asio::io_context will run on a thread
    // pool of this size. (If omitted, discusy defaults to 1).
    constexpr std::uint32_t worker_threads = 4;
    bot bot{config{
        .token{std::move(token)},
        .intents = DEFAULT_INTENTS | intent::guild_messages,
        .threads = worker_threads,
    }};

    // In a multi-threaded bot, on_shards_ready might be invoked across different threads or re-triggered
    // if shards reconnect. We use an atomic callback_id to cleanly unregister it, combined with
    // std::call_once to guarantee global command registration happens exactly once.
    std::atomic<callback_id> shards_ready_id{0};
    shards_ready_id = bot.on_shards_ready([&bot, &shards_ready_id](const user::user& me) {
        if (const auto id = shards_ready_id.load()) {
            bot.on_shards_ready.unregister(id);
        }

        static std::once_flag flag;
        std::call_once(flag, [&bot, &me]() {
            std::ostringstream ss;
            ss << std::this_thread::get_id();
            std::println("Logged in as {} (ID: {}) on thread {}", me.username, me.id, ss.str());
            std::println("Registering multithreading slash commands...");

            auto commands = h::cmd::commands(
                h::cmd::slash("thread_info", "Displays the current worker thread handling your interaction"),
                h::cmd::slash("count", "Safely updates and reads shared guild stats across threads using a mutex"),
                h::cmd::slash("heavy_work", "Simulates long async work without blocking other threads from processing events"),
                h::cmd::slash("strand_cancel", "Demonstrates safe token::cancel_after and strands in a multi-threaded bot")
            );

            // <false> avoids unnecessary JSON parsing, token::detached fires asynchronously without blocking
            bot.api.bulk_overwrite_global_application_commands<false>(commands)(token::detached);
            std::println("Slash commands registered.");
        });
    });

    // =========================================================================
    // SPAWNING A CALLBACK DIRECTLY ON A STRAND (Overload Demonstration):
    //
    // Discusy's Callback type provides overloads that accept an executor (such as an asio strand):
    //   callback(strand, [](const T& data) { ... })
    //   callback([](const T& data) { ... }, strand)
    //
    // When you pass a strand directly to the callback registration, discusy uses boost::asio::co_spawn
    // onto that strand executor. This guarantees that all invocations of this callback are strictly
    // serialized (executed one at a time) on that strand, even though the bot has multiple worker threads!
    //
    // Consequently, any state accessed inside this callback does NOT need a mutex or atomic synchronization.
    // =========================================================================
    auto message_strand = bot.io_ctx.make_strand();

    struct MessageCounter {
        std::size_t count{0};
        std::string last_author;
    } message_counter; // Plain struct: completely safe because message_strand guarantees serialized execution!

    bot.gateway_callbacks.on_message_create(message_strand, [&message_counter](const recieve_event::message_create& msg) {
        if (msg->author.bot.value_or(false)) return;

        message_counter.count++;
        message_counter.last_author = msg->author.username;
        std::println("[STRAND CALLBACK] Message #{} from {} (safely handled on strand without mutex on thread {})",
            message_counter.count, message_counter.last_author, std::this_thread::get_id());
    });

    // General Interaction handler (without a strand):
    // In contrast to the strand-bound callback above, registering without a strand dispatches
    // callbacks concurrently onto the general worker thread pool. This allows parallel execution
    // across threads for maximum throughput, where you synchronize shared state with atomics/mutexes
    // or spawn per-workflow strands (e.g. for cancel_after).
    // (Note: you could also pass a strand here: bot.gateway_callbacks.on_interaction_create(strand, ...)
    // if you wanted all interaction handlers serialized as well!)
    bot.gateway_callbacks.on_interaction_create([&bot](const recieve_event::interaction_create& e) -> coro::awaitable<void> {
        if (!e.is_command()) co_return;

        const auto* cmd = e.command_data();
        if (!cmd) co_return;

        // Atomically increment our global counter without needing a mutex
        g_total_commands.fetch_add(1, std::memory_order_relaxed);

        const auto& name = cmd->name;
        const auto guild_id = e.guild_id_of();

        // /thread_info
        if (name == "thread_info") {
            // Demonstrate that different interactions can run on different worker threads
            std::ostringstream ss;
            ss << std::this_thread::get_id();

            const auto reply_text = std::format(
                "Handled by worker thread ID: **{}**\n"
                "Total commands processed across all threads: **{}**",
                ss.str(), g_total_commands.load()
            );

            // Using token::detached here is completely safe and avoids unnecessary coroutine suspension
            e.reply<false>(reply_text)(token::detached);
            co_return;
        }

        // /count
        if (name == "count") {
            std::uint64_t guild_count = 0;
            std::string prev_user;
            const auto user_name = e.user ? e.user->username : (e.member ? e.member->display_name() : "Unknown");

            // SAFETY CRITICAL RULE:
            // Always acquire locks for the shortest possible scope.
            // NEVER hold a std::mutex across a co_await or network call!
            // If you hold a mutex across co_await e.reply(...), all other threads attempting to access
            // the map will be blocked waiting for the Discord network round-trip to complete.
            {
                std::scoped_lock lock{g_guild_mutex};
                auto& stats = g_guild_data[guild_id];
                stats.command_count++;
                guild_count = stats.command_count;
                prev_user = stats.last_command_user.empty() ? "None (first run)" : stats.last_command_user;
                stats.last_command_user = user_name;
            } // Lock is released here immediately before we touch the network

            const auto reply_text = std::format(
                "Guild count: **{}**\n"
                "Previous caller: **{}**\n"
                "Current caller: **{}**\n"
                "Total global bot interactions: **{}**",
                guild_count, prev_user, user_name, g_total_commands.load()
            );

            e.reply<false>(reply_text)(token::detached);
            co_return;
        }

        // /heavy_work
        if (name == "heavy_work") {
            // Defer reply to enter "thinking..." state
            co_await e.defer(false)(token::t_deferred);

            // Simulate doing 3 seconds of asynchronous work.
            // While this coroutine is suspended, other worker threads (and even this thread) remain
            // completely free to process incoming events or other commands without stalling.
            co_await bot.sleep(std::chrono::seconds(3));

            // When this coroutine resumes, it may resume on any available worker thread in the thread pool!
            std::ostringstream ss;
            ss << std::this_thread::get_id();

            const auto reply_text = std::format(
                "Heavy work completed! Resumed on thread **{}** \xE2\x9C\x85",
                ss.str()
            );

            e.edit_reply<false>(reply_text)(token::detached);
            co_return;
        }

        // /strand_cancel
        // Demonstrates the critical thread-safety rules when using token::cancel_after and strands
        // in a multi-threaded application.
        if (name == "strand_cancel") {
            // =========================================================================
            // MULTITHREADED CANCEL_AFTER & STRAND RULES:
            //
            // 1. In a single-threaded bot (.threads = 1), executor_timer_t on bot.io_ctx.executor_
            //    is safe because everything runs deterministically on one thread.
            //
            // 2. In a multi-threaded bot (.threads > 1):
            //    - If the cancellation timer expires on Thread A while the underlying operation
            //      completes on Thread B, a data race on the cancellation state / completion handler
            //      would occur without synchronization.
            //    - Therefore, Boost.Asio and discusy REQUIRE you to provide a strand_timer_t
            //      and bind the completion token to that same strand using token::bind_executor.
            //    - A strand guarantees sequential, non-concurrent execution of all tasks associated
            //      with it, guaranteeing that timer expiry and operation completion never race.
            // =========================================================================

            // Create a strand from the bot's IO context
            auto strand = bot.io_ctx.make_strand();

            // The timer MUST be a strand_timer_t associated with this strand
            ctx::io_context::strand_timer_t timer{strand};

            // Send an interactive button with a unique custom ID
            const auto current_id_num = custom_id_counter.fetch_add(1);
            const auto btn_id = std::format("btn_strand_{}", current_id_num);

            e.reply_components<false>(make_vector<discusy::components::component>(
                h::text_display("Testing strand-safe cancel_after in a multi-threaded runtime:"),
                h::action_row(
                    h::button(components::button_style::Primary, "Click me within 10s!", btn_id)
                )
            ))(token::detached);

            // Await the button click using .when combined with token::cancel_after.
            // NOTICE: We MUST wrap our token in token::bind_executor(strand, ...) so that the completion
            // handler executes safely on the strand!
            auto [ec, interaction_event] = co_await bot.gateway_callbacks.on_interaction_create.when(
                [&btn_id](const recieve_event::interaction_create& ev) -> bool {
                    return ev.is_component() && ev.component_custom_id() == btn_id;
                },
                token::cancel_after(timer, std::chrono::seconds{10}, token::bind_executor(strand, token::t_deferred))
            );

            if (ec) {
                // If the user didn't click within 10 seconds, boost::asio::error::operation_aborted is returned.
                // We edit the message to notify about the timeout and remove/disable components.
                e.edit_reply_with<false>(api::webhook::edit_webhook_message::create()
                    .set_components(make_vector<discusy::components::component>(
                        h::text_display("Timed out waiting for button click! Operation cancelled safely via strand. \xE2\x8F\xB3")
                    )))(token::detached);
                co_return;
            }

            // Button was clicked in time!
            interaction_event.reply_component_interaction<false>(api::interaction::interaction_callback_data_message::create()
                .set_components(make_vector<discusy::components::component>(
                    h::text_display("Successfully clicked! Handled safely across threads using strand & cancel_after \xE2\x9C\xA8")
                )))(token::detached);
            co_return;
        }

        co_return;
    });

    std::println("Starting multithreading example bot with {} worker threads...", worker_threads);
    bot.run();
    return 0;
}
