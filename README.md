# discusy

**This library is still in development and may potentially have breaking changes from time to time**

A modern, high-performance and stable C++23 Discord bot library built around **Boost.Asio** for its networking, asynchronous model and coroutine support.

All of the bot facing Discord API is covered and accessible, with types matching it identically for documentation re-usability.

## Features
- Discord API v10 with [zstd](https://github.com/facebook/zstd) + voice API v8 with [DAVE](https://github.com/discord/libdave)
- Header-only with statically linked dependencies (this does lead to high compiler memory requirements)
- [Glaze](https://github.com/stephenberry/glaze) for compile time json serialisation and deserialisation algorithms
- Works with 1 to N threads9
- Asynchronous methods support coroutines (lazy, eager), callbacks and detached execution with the same function (Asio completion tokens)
- Small memory footprint
- Optional cache for guilds, channels, members, users, roles, emojis, voice_states

Feel free to PR or make any additions/changes as you wish, they will all be considered!

## System Requirements
A recent CMake version, `3.31+`


A compiler supporting C++23 and at least meeting [Glaze's](https://github.com/stephenberry/glaze) minimum requirements is needed, this means needing to use libc++ with clang for older OS's such as debian 12 which some Raspberry Pi's may be running.

- Clang, MSVC and GCC are supported, other compilers may be supported, try compile and find out!
    - **You will likely need to use libc++ instead of libstdc++**
    - An example of this can be founds under [tests.yml](/.github/workflows/tests.yml)

### Platform Support
- Linux
    - Dependencies: `io_uring` if asynchronous file reading is used, can also enable it for networking
- Windows
    - Works out of the box with the bundled OpenSSL 4.0.1 (make sure to copy its dll's into your build directory), you can also provide your own
- MacOS
    - Untested, but should likely work as it is mostly platform-agnostic

All other dependencies are fetched via CMake as needed
- Due to the this, I would recommend only including `<discusy/discusy.hpp>` in only one translation unit to avoid polluting your project with all the dependencies

## Documentation
This does not really exist for now, due to the api types being very tightly coupled to Discord's API.
However there are some basic concepts to know:
- The main class you construct is `discusy::bot`
    - The thread count in the config is by default 1, you should increase it for very large bots or if you will have many voice connections, however for most bots a single thread should be enough, having more threads will also mean you need to worry about race conditions more (these can still apply within coroutines even if you have a single thread - as another task can be run during one suspension and change your state)
- The `discusy::bot`, `discusy::bot::gateway_events`, `discusy::bot::voice::client` and `discusy::bot::voice::connection` all hold `on_...` callback objects
    - You can attach callbacks to these, either synchronous or `discusy::coro::awaitable<T>` handlers, and they always must take a single argument of type `const T&`, where the `T` depends upon the handler itself, if you inspect its definition it will be visible
    - For the `gateway_events` object, the respective objects all hold the same name without the `on_` prefix and are within the `discusy::recieve_events` namespace
    - These callback objects also take an optional executor argument before or after the callback, if you wish to dispatch to your own `Asio` compatible executor, or your own strand for serialised execution
- The voice client is under `discusy::bot::voice` and has a few methods to join, disconnect and get voice calls
    - When joining a voice channel, a callback can be passed in the options to assign handlers to the `on_...` callbacks of the connection to prevent race conditions with assigning handlers to events that may have already fired once you recieve the object
    - Connecting to a voice channel gives you a shared pointer to a connection, on which you can send audio asynchronously and synchronously, pause/play, and handle events.
    - This shared pointer is safe to hold even past the bot being in a voice channel, although it will not perform any operations at that point and should be dropped to free memory
    - Once you recieve the voice connection object, it is ready to send audio!
- The http client is under `discusy::bot::client` and can perform HTTP/1.1 requests with a few configuration options
    - The `api` methods are rate limited for Discords API - don't use these yourself unless if youre manually making Discord API requests
- The http api is under `discusy::bot::api` and has a method for every single path within the Discord API, however these may only be called after the `discusy::bot::on_shards_ready` event fires at least once to populate internal state
- Use `discusy::make_vector` and the equivalent set methods instead of initialiser lists when creating vectors - initialiser lists cause an unnecessary copy of all arguments
    - Try to use `discusy::make_array` when you need a non-dynamically sized array, such as for creating global commands
- Coroutines are `discusy::coro::awaitable<T>`, which is an alias for `boost::asio::awaitable<T>`, make sure you know how to use coroutines before writing your own ones, including how they handle their arguments
### **Make sure to check out the examples to have some code to work from!**

## Getting help
Are you stuck trying to figure out how to do something? Or do you have an idea/suggestion for the library? Found a bug?
- Github issues will always be looked at
- If you prefer to chat there is the discusy help [Discord server](https://discord.gg/y3BX9rMcgz), feel free to ask any questions!

## Usage
### CMake `FetchContent` (Recommended)
Add the following to your `CMakeLists.txt`:

```cmake
include(FetchContent)
FetchContent_Declare(
    discusy
    GIT_REPOSITORY https://github.com/usyless/discusy.git
    GIT_TAG        main # Or a specific release tag/commit
    EXCLUDE_FROM_ALL
    SYSTEM
)

# Optional configuration flags prior to MakeAvailable:
# set(DISCUSY_ENABLE_VOICE ON CACHE BOOL "" FORCE) # If using voice
# set(DISCUSY_USE_MIMALLOC ON CACHE BOOL "" FORCE) # If using mimalloc
# View the rest of the flags in the main CMakeLists.txt

FetchContent_MakeAvailable(discusy)

target_link_libraries(${PROJECT_NAME} PRIVATE discusy::discusy)
```

### Option B: Git Submodule / Local Directory

```cmake
add_subdirectory("path/to/discusy")
target_link_libraries(${PROJECT_NAME} PRIVATE discusy::discusy)
```

### Recommended flags to leave on
- `set(DISCUSY_USE_INTERPROCEDURAL_OPTIMIZATION ON CACHE BOOL "" FORCE)`
    - The build may fail if this isn't on, due to the large binaries created
    - This simply just enables interprocedural optimisation

### All CMake Flags

| Option | Default | Description |
| :--- | :--- | :--- |
| `DISCUSY_ENABLE_VOICE` | `OFF` | Enables voice capabilities (Opus, DAVE E2EE, libsodium, mlspp). |
| `DISCUSY_LOGGING_ENABLED` | `OFF` | Forces logging in release builds (always enabled in Debug). |
| `DISCUSY_USE_MIMALLOC` | `OFF` | Uses Microsoft `mimalloc` as the memory allocator. |
| `DISCUSY_USE_INTERPROCEDURAL_OPTIMIZATION` | `ON` | Enables Link-Time Optimization (LTO/IPO). |
| `DISCUSY_ENABLE_PCH` | `ON` | Generates Precompiled Headers to speed up builds. |
| `DISCUSY_NO_CACHES` | `ON` | Disables internal caching of guilds/channels/members to reduce memory usage. |
| `DISCUSY_ENABLE_IO_URING` | `OFF` | (Linux only) Enables Linux `io_uring` for async file operations. |
| `DISCUSY_ENABLE_IO_URING_NETWORKING` | `OFF` | (Linux only) Enables `io_uring` networking instead of `epoll`. |
| `DISCUSY_ENABLE_ASAN` | `OFF` | Enables AddressSanitizer. |
| `DISCUSY_ENABLE_UBSAN` | `OFF` | Enables UndefinedBehaviorSanitizer (GCC/Clang). |
| `DISCUSY_ENABLE_TSAN` | `OFF` | Enables ThreadSanitizer. |
| `DISCUSY_BUILD_TESTS` | `OFF` | Build unit and integration tests. |
| `DISCUSY_BUILD_EXAMPLES` | `OFF` | Build discusy examples. |

## Quickstart

Create a bot to handle slash commands
```cpp
#include <discusy/discusy.hpp>
#include <print>

using namespace discusy;

int main() {
    config cfg{
        .token = "YOUR_DISCORD_BOT_TOKEN",
        .intents = DEFAULT_INTENTS,
        .threads = 1 // A Single thread is all most bots will ever need and simplifies the code significantly
        // If you dont specify a thread count, the bot defaults to 1 thread
    };

    bot bot{cfg};

    // 1. Declare and register slash commands once when shards are ready
    bot.on_shards_ready([&bot](const user::user& me) {
        std::println("Connected as @{} ({})", me.username, me.id.str());

        static std::once_flag registered_flag;
        std::call_once(registered_flag, [&bot]() {
            auto commands = make_array(
                helpers::cmd::slash("ping", "Replies with pong!"),
                helpers::cmd::slash("hello", "Says hello to the user")
            );

            // Register commands globally using fire-and-forget token::detached
            bot.api.bulk_overwrite_global_application_commands<false>(commands)(token::detached);
            std::println("Slash commands registered.");
        });
    });

    // 2. Handle incoming slash commands via interaction events
    bot.gateway_callbacks.on_interaction_create([](const recieve_event::interaction_create& e) -> coro::awaitable<void> {
        if (!e.is_command()) co_return;

        const auto* cmd = e.command_data();
        if (!cmd) co_return;

        // /ping - Immediate fire-and-forget reply:
        if (cmd->name == "ping") {
            e.reply<false>("Pong! 🏓")(token::detached);
            co_return;
        }

        // /hello - Ephemeral reply awaited with error handling:
        if (cmd->name == "hello") {
            auto [ec, _] = co_await e.reply_ephemeral<false>("Hello! 👋", token::t_deferred);
            if (ec) {
                std::println(stderr, "Failed to reply: {}", ec.message());
            }
            co_return;
        }
    });

    // 3. Start the bot blocking event loop
    bot.run();
}
```

- In order to apply basic optimisations, link your target to `discusy_exe_options` aswell

## Examples
Located in the [`examples/`](./examples) directory:

| Directory | Feature Demonstrated |
| :--- | :--- |
| [`examples/slash_commands`](./examples/slash_commands) | Registering global commands, argument extraction, and immediate/ephemeral/thinking responses. |
| [`examples/components`](./examples/components) | Buttons, action rows, select menus, state management, and collision-safe custom IDs. |
| [`examples/components_with_when`](./examples/components_with_when) | Inlined interactive flows using `bot.when(...)` and `token::cancel_after` timeouts. |
| [`examples/voice`](./examples/voice) | Audio thread pools, connecting to voice channels, 48kHz stereo PCM generation, and streaming. |
| [`examples/receiving_events`](./examples/receiving_events) | Subscribing to gateway events (`on_ready`, `on_message_create`, reactions, members). |
| [`examples/updating_presences`](./examples/updating_presences) | Status updates (`online`, `idle`, `dnd`), rich activity statuses, and timed rotation. |
| [`examples/multithreading`](./examples/multithreading) | Multi-threaded runtime (`threads > 1`), strand execution, and thread-safe cancellation. |
| [`examples/using_completion_tokens`](./examples/using_completion_tokens) | In-depth walkthrough of all completion token types (`t_deferred`, `t_eager`, `detached`, awaitable operators). |

### Building the Examples

To build any example individually:

```bash
cd examples/slash_commands
cmake -B build
cmake --build build -j
export DISCORD_BOT_TOKEN="your_token_here"
./build/example_slash_commands
```

To build all examples together:

```bash
cd examples
cmake -B build
cmake --build build -j
```