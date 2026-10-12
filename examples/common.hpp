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
    auto clean = [](const char* env) -> std::string {
        if (!env) return {};
        std::string s{env};
        if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
            s = s.substr(1, s.size() - 2);
        }
        return s;
    };
    if (const char* env_token = std::getenv("DISCORD_BOT_TOKEN")) {
        if (auto s = clean(env_token); !s.empty()) return s;
    }
    if (const char* env_token2 = std::getenv("BOT_TOKEN")) {
        if (auto s = clean(env_token2); !s.empty()) return s;
    }
    return {};
}