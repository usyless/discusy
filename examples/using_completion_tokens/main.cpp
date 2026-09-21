#include "../common.hpp" // IWYU pragma: keep

#include <format>
#include <print>

int main() {
    // This overload of the bot just actives the io context for running tasks that dotn need an actual bot token
    // for example here we just use it for sleeping, but you could also perform network requests in this mode
    // likely not api requests as those might need some real data from your live bot
    bot bot{1};

    // This posts a task onto the bots event queue, use it to start a coroutine
    bot.spawn([&bot]() -> coro::awaitable<void> {
        // Here we go through all the completion token types: (these are just aliases for asio's types)
        // using the t_ variants as we do not want exceptions

        // The one you will usually want: t_deferred
        auto [ec] = co_await bot.sleep(std::chrono::seconds{1})(token::t_deferred);
        if (ec) {
            std::println("t_deferred sleep ec: {}", ec.message());
        }

        // You can also store their value for later and await them later
        auto sleep_t_deferred = bot.sleep(std::chrono::seconds{1})(token::t_deferred);

        // ...

        // They MUST be moved in order to be co_await'ed later
        // Also: as it is a deferred operation it does NOT start running until 
        co_await std::move(sleep_t_deferred);

        // If you want the task to run eagerly to be awaited later:
        auto sleep_t_eager = bot.sleep(std::chrono::seconds{1})(token::t_eager);

        // ...

        // When we co_await it here the task has already been running, so if we had launched other work in the middle it would have had that
        // amount of time deducted from the sleep, or it can immediately return if the sleep is over
        co_await std::move(sleep_t_eager);

        // Completion tokens can also be used as callbacks, for any situation where they apply
        // Unfortunately you cannot pass coroutines into completion tokens
        bot.sleep(std::chrono::seconds{1})([](boost::system::error_code) {
            // The bot has now slept for one second when this lambda runs!
        });

        // Sometimes you want to run a task in the background and ignore anything that happens to it:
        bot.sleep(std::chrono::seconds{1})(token::detached);
        // Though this isnt very useful in this case....

        // Finally we have the explicit awaitable:
        // In this situation it is only harmful compared to deferred, it does not run until it is awaited
        // and it may spawn a new coroutine frame, which is unnecessary
        // HOWEVER if you wish to use awaitable operators, this will become useful
        co_await bot.sleep(std::chrono::seconds{1})(token::t_explicit_awaitable);

        // Awaitable operators:
        auto result = co_await (bot.sleep(std::chrono::seconds{1})(token::t_explicit_awaitable) 
                    || bot.sleep(std::chrono::seconds{2})(token::t_explicit_awaitable));
        if (result.index() == 0) {
            // The first timer won
        } else {
            // the second timer won
        }
        // When using these, ensure you are running in an implicit or explicit strand, that is:
            // As our bot is running on a single thread this is safe, however if the bot was running multithreaded
            // we would need to ensure the coroutine is running on a strand of its own that we spawn this from, or we 
            // manually bind a strand executor to each token
            // If we have multiple arguments complete at the same time on different threads that is a race condition
            // and it will be prevented by using a strand: via bot.io_ctx.make_strand()
        
        // If our coroutine is not running on a strand of ours...
        auto strand = bot.io_ctx.make_strand();
        co_await (bot.sleep(std::chrono::seconds{1})(token::bind_executor(strand, token::t_explicit_awaitable)) 
                    || bot.sleep(std::chrono::seconds{2})(token::bind_executor(strand, token::t_explicit_awaitable)));

        co_return;
    });

    // If we do have multiple threads...
    ctx::io_context::co_launch_detached([]() -> coro::awaitable<void> {
        // Due to bot.io_ctx.make_strand() this coroutine is serialised when using
        // completion tokens we await with

        // As such we can safely use awaitable operators without the explicit executor binding to a strand
        co_return;
    }, bot.io_ctx.make_strand());

    // FYI: the native callbacks dispatched by the library support passing an executor into their assign method
    // this allows you to ensure serialised execution of specific handlers, removing the need for mutexes
    // and increasing code safety
    // For example: (this wont ever fire here as we are running without a token)

    bot.on_shards_ready([](auto&&...) {
        // Our callback is now executing on the newly made strand!
        // As such this specific callback can never run in parallel, it will always be serialised
    }, bot.io_ctx.make_strand());

    std::println("Starting completion token example bot...");
    bot.run();
    return 0;
}
