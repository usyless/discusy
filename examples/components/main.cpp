#include "../common.hpp"

#include <atomic>
#include <format>
#include <print>
#include <string>
#include <vector>

// In a single-threaded bot (.threads = 1), a normal int would be fine here without synchronization
// because events run sequentially on the main IO thread. However, using std::atomic demonstrates how
// you would keep state safe if you switch to multiple threads (.threads > 1).
static std::atomic<int> g_counter{0};
static std::atomic_uint64_t custom_id_counter{0};

// Helper to build the counter components (TextDisplay + ActionRow with buttons)
static std::vector<components::component> build_counter_components(const int count, const std::string_view id_suffix = "") {
    return make_vector<components::component>(
        h::text_display(std::format("Current Counter Value: **{}**", count)),
        h::action_row(
            h::button(components::button_style::Success, "+1 Increment", std::format("counter_inc{}", id_suffix)),
            h::button(components::button_style::Danger, "-1 Decrement", std::format("counter_dec{}", id_suffix)),
            h::button(components::button_style::Secondary, "Reset", std::format("counter_reset{}", id_suffix))
        )
    );
}

int main() {
    auto token = get_bot_token();
    if (token.empty()) {
        std::println(stderr, "Error: DISCORD_BOT_TOKEN environment variable is not set.");
        std::println(stderr, "Usage: export DISCORD_BOT_TOKEN=\"your_bot_token_here\"");
        return 1;
    }

    // Defaulting to 1 worker thread keeps state handling straightforward and thread-safe without locks
    bot bot{config{
        .token{std::move(token)},
        .threads = 1,
    }};

    // Register slash commands to trigger component demos
    std::atomic<callback_id> shards_ready_id{0};
    shards_ready_id = bot.on_shards_ready([&bot, &shards_ready_id](const user::user& me) {
        if (const auto id = shards_ready_id.load()) {
            bot.on_shards_ready.unregister(id);
        }

        // Use std::call_once to protect against potential duplicate invocation during reconnects
        static std::once_flag flag;

        std::call_once(flag, [&bot, &me]() {
            std::println("Logged in as {} (ID: {})", me.username, me.id);
            std::println("Registering slash commands...");

            auto commands = h::cmd::commands(
                h::cmd::slash("buttons", "Showcases various button styles and link buttons"),
                h::cmd::slash("counter", "Interactive counter that updates the message in-place"),
                h::cmd::slash("select", "Showcases a dropdown select menu")
            );

            // Using <false> avoids unnecessary parsing of the Discord API response
            // token::detached runs this asynchronously without needing to co_await or block
            bot.api.bulk_overwrite_global_application_commands<false>(commands)(token::detached);
            std::println("Slash commands registered.");
        });
    });

    // Handle all interactions: slash commands and component clicks/selections
    // This is implemented as a coroutine, which is optional. For fire-and-forget replies, regular callbacks
    // or token::detached can be used directly without coroutine suspension overhead.
    // Remember: pass parameters by value if your coroutine needs to access them across co_await points.
    bot.gateway_callbacks.on_interaction_create([](const recieve_event::interaction_create& e) -> coro::awaitable<void> {
        // Slash Commands
        if (e.is_command()) {
            const auto* cmd = e.command_data();
            if (!cmd) co_return;

            if (cmd->name == "buttons") {
                // Construct components with TextDisplay and an ActionRow containing different button styles
                const auto current_id_num = custom_id_counter.fetch_add(1);
                e.reply_components<false>(make_vector<components::component>(
                    h::text_display("Here are examples of Discord button components:"),
                    h::action_row(
                        h::button(components::button_style::Primary, "Primary", std::format("btn_primary{}", current_id_num)),
                        h::button(components::button_style::Secondary, "Secondary", std::format("btn_secondary{}", current_id_num)),
                        h::button(components::button_style::Success, "Success", std::format("btn_success{}", current_id_num)),
                        h::button(components::button_style::Danger, "Danger", std::format("btn_danger{}", current_id_num)),
                        h::link_button("Discord Docs", "https://discord.com/developers/docs/interactions/message-components")
                    )
                ))(token::detached);
            }
            else if (cmd->name == "counter") {
                const auto current_id_num = custom_id_counter.fetch_add(1);
                const auto current_id_str = ulp::str::to_string(current_id_num);

                // Interactive message that will be updated in-place on button clicks
                e.reply_components<false>(build_counter_components(g_counter.load(), current_id_str))(token::detached);
            }
            else if (cmd->name == "select") {
                // Construct a StringSelect menu with a unique custom ID
                auto current_id = std::format("favorite_language{}", custom_id_counter.fetch_add(1));
                auto select = h::string_select(current_id, make_vector(
                    h::select_option("C++", "cpp", "Fast, modern, systems programming"),
                    h::select_option("Python", "python", "Readable and versatile"),
                    h::select_option("Rust", "rust", "Memory safe and concurrent"),
                    h::select_option("Go", "go", "Simple, concurrent networking")
                ), "Select your favorite programming language...");

                e.reply_components<false>(make_vector<components::component>(
                    h::text_display("Please choose an option from the dropdown below:"),
                    h::action_row(std::move(select))
                ))(token::detached);
            }

            co_return;
        }

        // Component Interactions (Buttons & Select Menus)
        // This is one way of handling components, directly in the main interaction handler,
        // however we can also handle them in individual cases using .when on the callback
        if (e.is_component()) {
            const auto custom_id = e.component_custom_id();
            std::println("Component interaction received: custom_id={}", custom_id);

            // Handle Counter buttons (in-place message update)
            if (custom_id.starts_with("counter_inc") || custom_id.starts_with("counter_dec") || custom_id.starts_with("counter_reset")) {
                std::string_view id_suffix;
                if (custom_id.starts_with("counter_inc")) {
                    g_counter.fetch_add(1);
                    id_suffix = std::string_view{custom_id}.substr(sizeof("counter_inc") - 1);
                } else if (custom_id.starts_with("counter_dec")) {
                    g_counter.fetch_sub(1);
                    id_suffix = std::string_view{custom_id}.substr(sizeof("counter_dec") - 1);
                } else if (custom_id.starts_with("counter_reset")) {
                    g_counter.store(0);
                    id_suffix = std::string_view{custom_id}.substr(sizeof("counter_reset") - 1);
                }

                // Update the original message components (TextDisplay + buttons) in-place
                auto data = api::interaction::interaction_callback_data_message::create()
                    .set_components(build_counter_components(g_counter.load(), id_suffix));

                // update_message updates the message the component is attached to in-place
                // If you instead had a long-running calculation, you could call co_await e.defer_update()(token::t_deferred);
                // and edit the message later
                e.update_message<false>(std::move(data))(token::detached);
                co_return;
            }

            // Handle general button clicks with ephemeral response
            if (custom_id.starts_with("btn_")) {
                const auto response = std::format("You clicked the **{}** button! \xE2\x9C\xA8", custom_id);
                e.reply_ephemeral<false>(response)(token::detached);
                co_return;
            }

            // Handle dropdown select menu
            if (custom_id.starts_with("favorite_language")) {
                // component_first_value returns the first selected option value
                // You can also use e.component_values() if multiple selections are allowed
                const auto selected_val = e.component_first_value();
                const auto response = std::format("Great choice! You picked: **{}** \xF0\x9F\x8E\x89", selected_val);
                e.reply_ephemeral<false>(response)(token::detached);
                co_return;
            }
        }

        co_return;
    });

    std::println("Starting components example bot...");
    bot.run();
    return 0;
}
