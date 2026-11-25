#pragma once

#include <cstdint>

namespace discusy {

enum class intent : std::uint32_t {
    none                           = 0,
    guilds                         = 1ULL << 0U,  // GUILDS
    guild_members                  = 1ULL << 1U,  // GUILD_MEMBERS (privileged)
    guild_moderation               = 1ULL << 2U,  // GUILD_MODERATION (aka GUILD_BANS in older libs)
    guild_emojis_and_stickers      = 1ULL << 3U,  // GUILD_EMOJIS_AND_STICKERS
    guild_integrations             = 1ULL << 4U,  // GUILD_INTEGRATIONS
    guild_webhooks                 = 1ULL << 5U,  // GUILD_WEBHOOKS
    guild_invites                  = 1ULL << 6U,  // GUILD_INVITES
    guild_voice_states             = 1ULL << 7U,  // GUILD_VOICE_STATES
    guild_presences                = 1ULL << 8U,  // GUILD_PRESENCES (privileged)
    guild_messages                 = 1ULL << 9U,  // GUILD_MESSAGES
    guild_message_reactions        = 1ULL << 10U, // GUILD_MESSAGE_REACTIONS
    guild_message_typing           = 1ULL << 11U, // GUILD_MESSAGE_TYPING
    direct_messages                = 1ULL << 12U, // DIRECT_MESSAGES
    direct_message_reactions       = 1ULL << 13U, // DIRECT_MESSAGE_REACTIONS
    direct_message_typing          = 1ULL << 14U, // DIRECT_MESSAGE_TYPING
    message_content                = 1ULL << 15U, // MESSAGE_CONTENT (privileged)
    guild_scheduled_events         = 1ULL << 16U, // GUILD_SCHEDULED_EVENTS
    auto_moderation_configuration  = 1ULL << 20U, // AUTO_MODERATION_CONFIGURATION
    auto_moderation_execution      = 1ULL << 21U, // AUTO_MODERATION_EXECUTION
    guild_message_polls            = 1ULL << 24U, // GUILD_MESSAGE_POLLS
    direct_message_polls           = 1ULL << 25U, // DIRECT_MESSAGE_POLLS
};

inline constexpr auto DEFAULT_INTENTS = intent::guilds;

}