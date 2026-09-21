# Discusy Bot Examples

Each example resides in its own directory with a standalone `CMakeLists.txt` and `main.cpp`.

---

## Examples Overview

| Directory | Activity | Description |
| :--- | :--- | :--- |
| [`slash_commands`](./slash_commands) | Replying to Slash Commands | Declaring and bulk registering global slash commands, reading options, immediate replies, ephemeral replies, and deferred thinking replies. |
| [`components`](./components) | Interactive Components | Buttons (Primary, Secondary, Success, Danger, Link), Action Rows, String Select dropdown menus, handling component interactions, and updating messages in-place. |
| [`components_with_when`](./components_with_when) | Interactive Components | Same as above, however using the `.when` syntax for interactions to he handled inline. |
| [`voice`](./voice) | Voice & Audio Streaming | Configuring audio runtime threads, joining voice channels, synthesizing 48kHz stereo 16-bit PCM audio, streaming audio with `send_pcm_async`, and disconnecting. |
| [`receiving_events`](./receiving_events) | Receiving Gateway Events | Subscribing to gateway callbacks (`on_ready`, `on_message_create`, `on_message_update`, `on_message_delete`, `on_message_reaction_add`, `on_guild_member_add`, `on_typing_start`). |
| [`updating_presences`](./updating_presences) | Updating Presences | Initial bot presence, dynamic status updates (`online`, `idle`, `dnd`), activity types (`playing`, `listening`, `watching`, `competing`, `custom`), and timed presence rotation via intervals. |
| [`multithreading`](./multithreading) | Multithreading | Basic examples on how to be safe with multithreading in discusy. |
| [`using_completion_tokens`](./using_completion_tokens) | Using Completion Tokens | Explanations of all the aliased Asio completion tokens within discusy. |
| [`random_image_bot`](./random_image_bot) | Random Image Bot | A full featured bot to create/delete slash commands to send random images based on channel messages. |

---

## Prerequisites

- A Discord Bot Token from the [Discord Developer Portal](https://discord.com/developers/applications)
  - Ensure necessary Gateway Intents (such as `MESSAGE_CONTENT` or `GUILD_MEMBERS` if running `receiving_events`) are enabled in the bot settings.

---

## Building and Running an Example

Set your bot token in your environment:

```bash
export DISCORD_BOT_TOKEN="your_bot_token_here"
```

```bat
set "DISCORD_BOT_TOKEN=your_bot_token_here"
```

### Standalone Build (Single Example)

Navigate to any example directory and configure with CMake:

```bash
cd examples/slash_commands
cmake -B build
cmake --build build -j
./build/example_slash_commands
```

For the voice example:

```bash
cd examples/voice
cmake -B build
cmake --build build -j
./build/example_voice
```

### Building All Examples

You can also build all examples at once from the `examples` folder or by adding `add_subdirectory(examples)` to the root `CMakeLists.txt`:

```bash
cd examples
cmake -B build
cmake --build build -j
```
