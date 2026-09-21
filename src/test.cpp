#ifdef DISCUSY_USE_MODULES
#include <boost/asio.hpp>
#include <glaze/glaze.hpp>
import discusy;
#else
#include <discusy/discusy.hpp>
#endif

#include <print>
#include <cstdio>

int main() {
    using namespace discusy;
    bot bot{"config.json", send_event::update_presence{
        .activities{
            recieve_event::presence::activity::create(recieve_event::presence::activity_type::custom, "discusy is alive?!?")
        }
    }, true};
    
    bot.gateway_callbacks.on_ready([](const recieve_event::ready&) -> coro::awaitable<void> {
        std::println(stderr, "On ready callback 1!");
        co_return;
    });

    bot.gateway_callbacks.on_ready([](const recieve_event::ready&) -> coro::awaitable<bool> {
        std::println(stderr, "On ready callback 2! Cancelling callback 3");
        co_return false;
    });

    bot.gateway_callbacks.on_ready([](const recieve_event::ready&) {
        std::println(stderr, "On ready callback 3!");
    });

    bot.on_shards_ready([&bot](const user::user& user) -> void {
        std::println(stderr, "Shards ready callback\nBot user id: {}\nBot username: {}", user.id, user.username);
        bot.timers.start_timer([&bot](timer) {
            bot.set_presence(send_event::update_presence{
                .activities{
                    recieve_event::presence::activity::create(recieve_event::presence::activity_type::custom, "discusy has now changed their status?!?")
                },
                .status = send_event::status_type::idle,
            });
        }, std::chrono::seconds{10});
    });

    bot.spawn([&bot]() -> coro::awaitable<void> {
        auto [file_ec, file_data] = co_await bot.read_file("config.json")(token::t_deferred);
        std::println(stderr, "Config.json data: {}", file_data);
        co_return;
    });

    // bot.timers.start_interval([](auto) -> void {
    //     std::println(stderr, "5 Second non-coro interval hit!");
    // }, std::chrono::seconds{5});

    // bot.timers.start_interval([](auto) -> coro::awaitable<void> {
    //     std::println(stderr, "5 Second coro interval hit!");
    //     co_return;
    // }, std::chrono::seconds{5});

    bot.gateway_callbacks.on_message_create([&bot](const recieve_event::message_create& msg) -> coro::awaitable<void> {
        std::println(stderr, "msg, content: {}", msg.message.content);

        if (msg.message.author.id == bot.get_user_id()) {
            co_return;
        }
        
        if (msg.message.content == "hi or ho") {
            auto [reply_ec, sent_msg] = co_await msg.reply_with<true>(
                api::message::create_message::create()
                    .add_text_display("hello")
                    .add_action_row(
                        components::ActionRow::create()
                            .add_button(components::Button::create(components::button_style::Primary, "hi", "hi"))
                            .add_button(components::Button::create(components::button_style::Primary, "ho", "ho"))
                    )
            )(token::t_deferred);

            if (reply_ec) {
                co_return;
            }

            // this is only safe because it is running on a single threaded io context!
            boost::asio::steady_timer timer{bot.io_ctx.executor_};
            auto [interaction_ec, interaction] = co_await bot.gateway_callbacks.on_interaction_create.when(
                [msg_id = sent_msg->id](const recieve_event::interaction_create& e) -> bool {
                    if (!e.is_component()) return false;
                    if (e.message && e.message->id != msg_id) return false;
                    const auto custom_id = e.component_custom_id();
                    return custom_id == "hi" || custom_id == "ho";
                },
                token::cancel_after(timer, std::chrono::seconds{15}, token::t_deferred)
            );

            if (!interaction_ec) {
                const auto custom_id = interaction.component_custom_id();
                if (custom_id == "hi") {
                    interaction.update_message<false>(api::interaction::interaction_callback_data_message::create()
                        .add_text_display("hi"))(token::detached);
                } else if (custom_id == "ho") {
                    interaction.update_message<false>(api::interaction::interaction_callback_data_message::create()
                        .add_text_display("ho"))(token::detached);
                }
            }
        }
    });

    bot.run();
}