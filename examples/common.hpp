#pragma once

#include <discusy/discusy.hpp>
#include <cstdlib>
#include <string>

// Do I recommend using namespace discusy? Probably not, although you could
// alias it to not have such long types
using namespace discusy;
namespace h = discusy::helpers;

// Retrieves the bot token from environment variables
static std::string get_bot_token() {
    if (const char* env_token = std::getenv("DISCORD_BOT_TOKEN")) {
        return env_token;
    }
    if (const char* env_token2 = std::getenv("BOT_TOKEN")) {
        return env_token2;
    }
    return {};
}