#pragma once

#include <cstddef>
#include <cstdint>

namespace discusy::constants {

// https://discord.com/developers/docs/interactions/application-commands
namespace application_commands {
    constexpr inline std::size_t NAME_MIN_LENGTH = 1;
    constexpr inline std::size_t NAME_MAX_LENGTH = 32;
    constexpr inline std::size_t CHAT_INPUT_NAME_MIN_LENGTH = 1;
    constexpr inline std::size_t CHAT_INPUT_NAME_MAX_LENGTH = 32;
    constexpr inline std::size_t USER_COMMAND_NAME_MIN_LENGTH = 1;
    constexpr inline std::size_t USER_COMMAND_NAME_MAX_LENGTH = 32;
    constexpr inline std::size_t MESSAGE_COMMAND_NAME_MIN_LENGTH = 1;
    constexpr inline std::size_t MESSAGE_COMMAND_NAME_MAX_LENGTH = 32;

    constexpr inline std::size_t DESCRIPTION_MIN_LENGTH = 1;
    constexpr inline std::size_t DESCRIPTION_MAX_LENGTH = 100;

    constexpr inline std::size_t OPTIONS_MAX = 25;
    constexpr inline std::size_t OPTION_NAME_MIN_LENGTH = 1;
    constexpr inline std::size_t OPTION_NAME_MAX_LENGTH = 32;
    constexpr inline std::size_t OPTION_DESCRIPTION_MIN_LENGTH = 1;
    constexpr inline std::size_t OPTION_DESCRIPTION_MAX_LENGTH = 100;

    constexpr inline std::size_t CHOICES_MAX = 25;
    constexpr inline std::size_t CHOICE_NAME_MIN_LENGTH = 1;
    constexpr inline std::size_t CHOICE_NAME_MAX_LENGTH = 100;
    constexpr inline std::size_t CHOICE_STRING_VALUE_MAX_LENGTH = 100;

    constexpr inline std::size_t AUTOCOMPLETE_CHOICES_MAX = 25;
    constexpr inline std::size_t COMMAND_PERMISSIONS_MAX = 100;

    constexpr inline std::size_t GLOBAL_CHAT_INPUT_COMMANDS_MAX = 100;
    constexpr inline std::size_t GLOBAL_USER_COMMANDS_MAX = 5;
    constexpr inline std::size_t GLOBAL_MESSAGE_COMMANDS_MAX = 5;
    constexpr inline std::size_t GLOBAL_PRIMARY_ENTRY_POINT_COMMANDS_MAX = 1;

    constexpr inline std::size_t GUILD_CHAT_INPUT_COMMANDS_MAX = 100;
    constexpr inline std::size_t GUILD_USER_COMMANDS_MAX = 5;
    constexpr inline std::size_t GUILD_MESSAGE_COMMANDS_MAX = 5;
    constexpr inline std::size_t GUILD_PRIMARY_ENTRY_POINT_COMMANDS_MAX = 1;
}

// https://discord.com/developers/docs/resources/application-role-connection-metadata
namespace application_role_connection_metadata {
    constexpr inline std::size_t RECORDS_MAX = 8;
    constexpr inline std::size_t KEY_MIN_LENGTH = 1;
    constexpr inline std::size_t KEY_MAX_LENGTH = 50;
    constexpr inline std::size_t NAME_MIN_LENGTH = 1;
    constexpr inline std::size_t NAME_MAX_LENGTH = 100;
    constexpr inline std::size_t DESCRIPTION_MIN_LENGTH = 1;
    constexpr inline std::size_t DESCRIPTION_MAX_LENGTH = 200;
}

// https://discord.com/developers/docs/resources/application
namespace application {
    constexpr inline std::size_t CUSTOM_INSTALL_URL_MAX_LENGTH = 400;
    constexpr inline std::size_t DESCRIPTION_MAX_LENGTH = 400;
    constexpr inline std::size_t ROLE_CONNECTIONS_VERIFICATION_URL_MAX_LENGTH = 400;
    constexpr inline std::size_t INTERACTIONS_ENDPOINT_URL_MAX_LENGTH = 400;
    constexpr inline std::size_t EVENT_WEBHOOKS_URL_MAX_LENGTH = 400;

    constexpr inline std::size_t TAGS_MAX = 5;
    constexpr inline std::size_t TAG_MAX_LENGTH = 20;

    constexpr inline std::size_t ICON_MAX_SIZE_BYTES = 10UZ * 1024 * 1024; // 10 MB
    constexpr inline int ICON_WIDTH = 512;
    constexpr inline int ICON_HEIGHT = 512;
    constexpr inline int ICON_MIN_WIDTH = 128;
    constexpr inline int ICON_MIN_HEIGHT = 128;

    constexpr inline std::size_t COVER_IMAGE_MAX_SIZE_BYTES = 10UZ * 1024 * 1024; // 10 MB
    constexpr inline int COVER_IMAGE_WIDTH = 1920;
    constexpr inline int COVER_IMAGE_HEIGHT = 1080;
    constexpr inline int COVER_IMAGE_MIN_WIDTH = 1024;
    constexpr inline int COVER_IMAGE_MIN_HEIGHT = 1024;

    constexpr inline std::size_t REDIRECT_URIS_MAX = 100;
    constexpr inline std::size_t USER_INSTALLED_INTERACTION_FOLLOWUP_REPLIES_MAX = 5;
}

// https://discord.com/developers/docs/resources/audit-log
namespace audit_log {
    constexpr inline std::size_t QUERY_LIMIT_MIN = 1;
    constexpr inline std::size_t QUERY_LIMIT_DEFAULT = 50;
    constexpr inline std::size_t QUERY_LIMIT_MAX = 100;
    constexpr inline std::size_t REASON_MAX_LENGTH = 512;
}

// https://discord.com/developers/docs/resources/auto-moderation
namespace auto_moderation {
    constexpr inline std::size_t RULE_NAME_MAX_LENGTH = 100;

    constexpr inline std::size_t KEYWORD_FILTER_MAX = 1000;
    constexpr inline std::size_t KEYWORD_MAX_LENGTH = 60;

    constexpr inline std::size_t REGEX_PATTERNS_MAX = 10;
    constexpr inline std::size_t REGEX_PATTERN_MAX_LENGTH = 260;

    constexpr inline std::size_t ALLOW_LIST_MAX = 100;
    constexpr inline std::size_t ALLOW_LIST_KEYWORD_MAX_LENGTH = 60;
    constexpr inline std::size_t ALLOW_LIST_PRESETS_MAX = 1000;
    constexpr inline std::size_t ALLOW_LIST_REGEX_PATTERNS_MAX = 10;
    constexpr inline std::size_t ALLOW_LIST_REGEX_PATTERN_MAX_LENGTH = 260;

    constexpr inline std::size_t MENTION_TOTAL_LIMIT_MAX = 50;
    constexpr inline std::size_t EXEMPT_ROLES_MAX = 20;
    constexpr inline std::size_t EXEMPT_CHANNELS_MAX = 50;

    constexpr inline std::size_t CUSTOM_MESSAGE_MAX_LENGTH = 150;
    constexpr inline std::uint32_t TIMEOUT_DURATION_SECONDS_MAX = 2419200; // 4 weeks / 28 days

    constexpr inline std::size_t RULES_PER_GUILD_KEYWORD_MAX = 6;
    constexpr inline std::size_t RULES_PER_GUILD_SPAM_MAX = 1;
    constexpr inline std::size_t RULES_PER_GUILD_MENTION_SPAM_MAX = 1;
    constexpr inline std::size_t RULES_PER_GUILD_KEYWORD_PRESET_MAX = 1;
    constexpr inline std::size_t RULES_PER_GUILD_MEMBER_PROFILE_MAX = 1;
}

// https://discord.com/developers/docs/resources/channel
namespace channel {
    constexpr inline std::size_t NAME_MIN_LENGTH = 1;
    constexpr inline std::size_t NAME_MAX_LENGTH = 100;

    constexpr inline std::size_t TOPIC_MAX_LENGTH = 1024;
    constexpr inline std::size_t FORUM_TOPIC_MAX_LENGTH = 4096;
    constexpr inline std::size_t VOICE_STATUS_MAX_LENGTH = 500;

    constexpr inline int VOICE_BITRATE_MIN = 8000;
    constexpr inline int VOICE_BITRATE_DEFAULT = 64000;
    constexpr inline int VOICE_BITRATE_MAX_TIER_0 = 96000;
    constexpr inline int VOICE_BITRATE_MAX_TIER_1 = 128000;
    constexpr inline int VOICE_BITRATE_MAX_TIER_2 = 256000;
    constexpr inline int VOICE_BITRATE_MAX_TIER_3 = 384000;

    constexpr inline int VOICE_USER_LIMIT_MIN = 0;
    constexpr inline int VOICE_USER_LIMIT_MAX = 99;
    constexpr inline int STAGE_USER_LIMIT_MAX = 10000;

    constexpr inline std::uint32_t RATE_LIMIT_PER_USER_MIN = 0;
    constexpr inline std::uint32_t RATE_LIMIT_PER_USER_DEFAULT = 0;
    constexpr inline std::uint32_t RATE_LIMIT_PER_USER_MAX = 21600; // 6 hours

    constexpr inline std::size_t PERMISSION_OVERWRITES_MAX = 1000;
    constexpr inline std::size_t WEBHOOKS_PER_CHANNEL_MAX = 10;

    constexpr inline std::size_t FORUM_TAGS_MAX = 20;
    constexpr inline std::size_t FORUM_TAG_NAME_MAX_LENGTH = 20;

    constexpr inline int AUTO_ARCHIVE_DURATION_1_HOUR = 60;
    constexpr inline int AUTO_ARCHIVE_DURATION_24_HOURS = 1440;
    constexpr inline int AUTO_ARCHIVE_DURATION_3_DAYS = 4320;
    constexpr inline int AUTO_ARCHIVE_DURATION_1_WEEK = 10080;

    constexpr inline std::size_t THREAD_MEMBER_LIMIT_DEFAULT = 100;
    constexpr inline std::size_t THREAD_MEMBER_LIMIT_MAX = 100;
    constexpr inline std::size_t LIST_ARCHIVED_THREADS_LIMIT_MAX = 100;
}

// https://discord.com/developers/docs/resources/emoji
namespace emoji {
    constexpr inline std::size_t MAX_SIZE_BYTES = 256UZ * 1024; // 256 KB
    constexpr inline int RECOMMENDED_WIDTH = 128;
    constexpr inline int RECOMMENDED_HEIGHT = 128;
    constexpr inline std::size_t NAME_MIN_LENGTH = 2;
    constexpr inline std::size_t NAME_MAX_LENGTH = 32;

    constexpr inline std::size_t GUILD_EMOJIS_MAX_TIER_0 = 50;
    constexpr inline std::size_t GUILD_EMOJIS_MAX_TIER_1 = 100;
    constexpr inline std::size_t GUILD_EMOJIS_MAX_TIER_2 = 150;
    constexpr inline std::size_t GUILD_EMOJIS_MAX_TIER_3 = 250;

    constexpr inline std::size_t APPLICATION_EMOJIS_MAX = 2000;
}

// https://discord.com/developers/docs/resources/entitlement
namespace entitlement {
    constexpr inline std::size_t QUERY_LIMIT_MIN = 1;
    constexpr inline std::size_t QUERY_LIMIT_DEFAULT = 100;
    constexpr inline std::size_t QUERY_LIMIT_MAX = 100;
}

// https://discord.com/developers/docs/events/gateway
namespace gateway {
    constexpr inline std::size_t REQUEST_GUILD_MEMBERS_LIMIT_MIN = 0;
    constexpr inline std::size_t REQUEST_GUILD_MEMBERS_LIMIT_MAX = 100;
    constexpr inline std::size_t PRESENCE_ACTIVITIES_MAX = 5;
    constexpr inline std::uint32_t IDENTIFY_RATE_LIMIT_SECONDS = 5;
}

// https://discord.com/developers/docs/resources/guild-scheduled-event
namespace guild_scheduled_event {
    constexpr inline std::size_t NAME_MIN_LENGTH = 1;
    constexpr inline std::size_t NAME_MAX_LENGTH = 100;
    constexpr inline std::size_t DESCRIPTION_MAX_LENGTH = 1000;
    constexpr inline std::size_t LOCATION_MIN_LENGTH = 1;
    constexpr inline std::size_t LOCATION_MAX_LENGTH = 100;

    constexpr inline std::size_t IMAGE_MAX_SIZE_BYTES = 10UZ * 1024 * 1024; // 10 MB
    constexpr inline int IMAGE_WIDTH = 1920;
    constexpr inline int IMAGE_HEIGHT = 1080;
    constexpr inline int IMAGE_MIN_WIDTH = 1024;
    constexpr inline int IMAGE_MIN_HEIGHT = 1024;

    constexpr inline std::size_t QUERY_LIMIT_MIN = 1;
    constexpr inline std::size_t QUERY_LIMIT_DEFAULT = 100;
    constexpr inline std::size_t QUERY_LIMIT_MAX = 100;
}

// https://discord.com/developers/docs/resources/guild-template
namespace guild_template {
    constexpr inline std::size_t NAME_MIN_LENGTH = 1;
    constexpr inline std::size_t NAME_MAX_LENGTH = 100;
    constexpr inline std::size_t DESCRIPTION_MAX_LENGTH = 120;
    constexpr inline std::size_t CODE_LENGTH = 12;
}

// https://discord.com/developers/docs/resources/guild
namespace guild {
    constexpr inline std::size_t NAME_MIN_LENGTH = 2;
    constexpr inline std::size_t NAME_MAX_LENGTH = 100;

    constexpr inline std::size_t DESCRIPTION_MAX_LENGTH = 120;
    constexpr inline std::size_t DISCOVERY_DESCRIPTION_MAX_LENGTH = 1000;

    constexpr inline std::size_t ICON_MAX_SIZE_BYTES = 10UZ * 1024 * 1024; // 10 MB
    constexpr inline int ICON_WIDTH = 512;
    constexpr inline int ICON_HEIGHT = 512;
    constexpr inline int ICON_MIN_WIDTH = 128;
    constexpr inline int ICON_MIN_HEIGHT = 128;
    constexpr inline int ICON_MAX_WIDTH = 4096;
    constexpr inline int ICON_MAX_HEIGHT = 4096;

    constexpr inline std::size_t BANNER_MAX_SIZE_BYTES = 10UZ * 1024 * 1024; // 10 MB
    constexpr inline int BANNER_WIDTH = 1920;
    constexpr inline int BANNER_HEIGHT = 1080;
    constexpr inline int BANNER_MIN_WIDTH = 960;
    constexpr inline int BANNER_MIN_HEIGHT = 540;
    constexpr inline int BANNER_MAX_WIDTH = 4096;
    constexpr inline int BANNER_MAX_HEIGHT = 4096;

    constexpr inline std::size_t SPLASH_MAX_SIZE_BYTES = 10UZ * 1024 * 1024; // 10 MB
    constexpr inline int SPLASH_WIDTH = 2048;
    constexpr inline int SPLASH_HEIGHT = 1152;
    constexpr inline int SPLASH_MIN_WIDTH = 1280;
    constexpr inline int SPLASH_MIN_HEIGHT = 720;

    constexpr inline std::size_t DISCOVERY_SPLASH_MAX_SIZE_BYTES = 10UZ * 1024 * 1024; // 10 MB
    constexpr inline int DISCOVERY_SPLASH_WIDTH = 2048;
    constexpr inline int DISCOVERY_SPLASH_HEIGHT = 1152;
    constexpr inline int DISCOVERY_SPLASH_MIN_WIDTH = 1920;
    constexpr inline int DISCOVERY_SPLASH_MIN_HEIGHT = 1080;

    constexpr inline std::size_t ROLES_MAX = 250;
    constexpr inline std::size_t ROLE_NAME_MAX_LENGTH = 100;
    constexpr inline std::size_t ROLE_ICON_MAX_SIZE_BYTES = 256UZ * 1024; // 256 KB

    constexpr inline std::size_t NICKNAME_MIN_LENGTH = 1;
    constexpr inline std::size_t NICKNAME_MAX_LENGTH = 32;

    constexpr inline int PRUNE_DAYS_MIN = 1;
    constexpr inline int PRUNE_DAYS_MAX = 30;
    constexpr inline int PRUNE_DAYS_DEFAULT = 7;

    constexpr inline std::size_t CHANNELS_MAX = 500;
    constexpr inline std::size_t WEBHOOKS_PER_GUILD_MAX = 50;

    constexpr inline int AFK_TIMEOUT_SECONDS_1_MIN = 60;
    constexpr inline int AFK_TIMEOUT_SECONDS_5_MIN = 300;
    constexpr inline int AFK_TIMEOUT_SECONDS_15_MIN = 900;
    constexpr inline int AFK_TIMEOUT_SECONDS_30_MIN = 1800;
    constexpr inline int AFK_TIMEOUT_SECONDS_1_HOUR = 3600;

    constexpr inline std::size_t WELCOME_SCREEN_DESCRIPTION_MAX_LENGTH = 140;
    constexpr inline std::size_t WELCOME_SCREEN_CHANNELS_MAX = 5;
    constexpr inline std::size_t WELCOME_SCREEN_CHANNEL_DESCRIPTION_MAX_LENGTH = 42;

    constexpr inline std::size_t ONBOARDING_PROMPT_TITLE_MAX_LENGTH = 100;
    constexpr inline std::size_t ONBOARDING_PROMPT_OPTIONS_MAX = 50;
    constexpr inline std::size_t ONBOARDING_PROMPT_OPTION_TITLE_MAX_LENGTH = 50;
    constexpr inline std::size_t ONBOARDING_PROMPT_OPTION_DESCRIPTION_MAX_LENGTH = 100;
    constexpr inline std::size_t ONBOARDING_PROMPTS_MAX = 15;
    constexpr inline std::size_t ONBOARDING_DEFAULT_CHANNELS_MIN = 7;

    constexpr inline std::size_t MEMBERS_QUERY_LIMIT_DEFAULT = 1;
    constexpr inline std::size_t MEMBERS_QUERY_LIMIT_MAX = 1000;
}

// https://discord.com/developers/docs/resources/invite
namespace invite {
    constexpr inline std::uint32_t MAX_AGE_SECONDS_MIN = 0; // 0 = never
    constexpr inline std::uint32_t MAX_AGE_SECONDS_MAX = 604800; // 7 days
    constexpr inline std::size_t MAX_USES_MIN = 0; // 0 = unlimited
    constexpr inline std::size_t MAX_USES_MAX = 100;
    constexpr inline std::size_t CODE_MIN_LENGTH = 2;
    constexpr inline std::size_t CODE_MAX_LENGTH = 32;
}

// https://discord.com/developers/docs/resources/lobby
namespace lobby {
    constexpr inline std::uint32_t IDLE_TIMEOUT_SECONDS_DEFAULT = 300;
    constexpr inline std::uint32_t IDLE_TIMEOUT_SECONDS_MAX = 86400; // 24 hours

    constexpr inline std::size_t METADATA_KEYS_MAX = 16;
    constexpr inline std::size_t METADATA_KEY_MAX_LENGTH = 64;
    constexpr inline std::size_t METADATA_VALUE_MAX_LENGTH = 256;
    constexpr inline std::size_t METADATA_TOTAL_SIZE_MAX_BYTES = 4096;

    constexpr inline std::size_t MEMBERS_MAX = 100;
    constexpr inline std::size_t MESSAGE_CONTENT_MAX_LENGTH = 2000;

    constexpr inline std::size_t GET_MESSAGES_LIMIT_MIN = 1;
    constexpr inline std::size_t GET_MESSAGES_LIMIT_DEFAULT = 50;
    constexpr inline std::size_t GET_MESSAGES_LIMIT_MAX = 100;
}

// https://discord.com/developers/docs/resources/message
// https://discord.com/developers/docs/interactions/message-components
namespace message {
    constexpr inline std::size_t MAX_LENGTH = 2000;
    constexpr inline std::size_t MAX_CONTENT_LENGTH = 2000;
    constexpr inline std::size_t MAX_CONTENT_LENGTH_NITRO = 4000;

    constexpr inline std::size_t EMBEDS_MAX = 10;
    constexpr inline std::size_t EMBED_TITLE_MAX_LENGTH = 256;
    constexpr inline std::size_t EMBED_DESCRIPTION_MAX_LENGTH = 4096;
    constexpr inline std::size_t EMBED_FIELDS_MAX = 25;
    constexpr inline std::size_t EMBED_FIELD_NAME_MAX_LENGTH = 256;
    constexpr inline std::size_t EMBED_FIELD_VALUE_MAX_LENGTH = 1024;
    constexpr inline std::size_t EMBED_FOOTER_TEXT_MAX_LENGTH = 2048;
    constexpr inline std::size_t EMBED_AUTHOR_NAME_MAX_LENGTH = 256;
    constexpr inline std::size_t EMBED_TOTAL_MAX_LENGTH = 6000;

    constexpr inline std::size_t ATTACHMENTS_MAX = 10;
    constexpr inline std::size_t ATTACHMENT_MAX_SIZE_BYTES = 20UZ * 1024 * 1024; // 20 MB
    constexpr inline std::size_t ATTACHMENT_MAX_SIZE_NITRO_BASIC_BYTES = 50UZ * 1024 * 1024; // 50 MB
    constexpr inline std::size_t ATTACHMENT_MAX_SIZE_NITRO_BYTES = 500UZ * 1024 * 1024; // 500 MB

    constexpr inline std::size_t REACTIONS_MAX_PER_MESSAGE = 20;
    constexpr inline std::size_t REACTIONS_GET_LIMIT_DEFAULT = 25;
    constexpr inline std::size_t REACTIONS_GET_LIMIT_MAX = 100;

    constexpr inline std::size_t GET_MESSAGES_LIMIT_DEFAULT = 50;
    constexpr inline std::size_t GET_MESSAGES_LIMIT_MAX = 100;

    constexpr inline std::size_t BULK_DELETE_MESSAGES_MIN = 2;
    constexpr inline std::size_t BULK_DELETE_MESSAGES_MAX = 100;
    constexpr inline std::uint32_t BULK_DELETE_MAX_AGE_SECONDS = 14 * 24 * 60 * 60; // 14 days

    constexpr inline std::size_t PINNED_MESSAGES_MAX = 50;

    // Components
    constexpr inline std::size_t COMPONENTS_MAX_PER_MESSAGE = 5;
    constexpr inline std::size_t COMPONENTS_MAX_PER_ACTION_ROW = 5;

    constexpr inline std::size_t BUTTON_LABEL_MAX_LENGTH = 80;
    constexpr inline std::size_t BUTTON_CUSTOM_ID_MAX_LENGTH = 100;
    constexpr inline std::size_t BUTTON_URL_MAX_LENGTH = 512;

    constexpr inline std::size_t SELECT_MENU_CUSTOM_ID_MAX_LENGTH = 100;
    constexpr inline std::size_t SELECT_MENU_PLACEHOLDER_MAX_LENGTH = 150;
    constexpr inline std::size_t SELECT_MENU_OPTIONS_MIN = 1;
    constexpr inline std::size_t SELECT_MENU_OPTIONS_MAX = 25;
    constexpr inline std::size_t SELECT_MENU_OPTION_LABEL_MAX_LENGTH = 100;
    constexpr inline std::size_t SELECT_MENU_OPTION_VALUE_MAX_LENGTH = 100;
    constexpr inline std::size_t SELECT_MENU_OPTION_DESCRIPTION_MAX_LENGTH = 100;
    constexpr inline std::size_t SELECT_MENU_DEFAULT_VALUES_MAX = 25;

    // Modals & Text Inputs
    constexpr inline std::size_t MODAL_CUSTOM_ID_MAX_LENGTH = 100;
    constexpr inline std::size_t MODAL_TITLE_MAX_LENGTH = 45;
    constexpr inline std::size_t MODAL_COMPONENTS_MAX = 5;

    constexpr inline std::size_t TEXT_INPUT_CUSTOM_ID_MAX_LENGTH = 100;
    constexpr inline std::size_t TEXT_INPUT_LABEL_MAX_LENGTH = 45;
    constexpr inline std::size_t TEXT_INPUT_MIN_LENGTH_MIN = 0;
    constexpr inline std::size_t TEXT_INPUT_MIN_LENGTH_MAX = 4000;
    constexpr inline std::size_t TEXT_INPUT_MAX_LENGTH_MIN = 1;
    constexpr inline std::size_t TEXT_INPUT_MAX_LENGTH_MAX = 4000;
    constexpr inline std::size_t TEXT_INPUT_PLACEHOLDER_MAX_LENGTH = 100;
    constexpr inline std::size_t TEXT_INPUT_VALUE_MAX_LENGTH = 4000;
}

// https://discord.com/developers/docs/resources/poll
namespace poll {
    constexpr inline std::size_t QUESTION_TEXT_MAX_LENGTH = 300;
    constexpr inline std::size_t ANSWERS_MIN = 1;
    constexpr inline std::size_t ANSWERS_MAX = 10;
    constexpr inline std::size_t ANSWER_TEXT_MAX_LENGTH = 55;

    constexpr inline std::uint32_t DURATION_HOURS_DEFAULT = 24;
    constexpr inline std::uint32_t DURATION_HOURS_MAX = 768; // 32 days = 768 hours

    constexpr inline std::size_t VOTERS_QUERY_LIMIT_MIN = 1;
    constexpr inline std::size_t VOTERS_QUERY_LIMIT_DEFAULT = 25;
    constexpr inline std::size_t VOTERS_QUERY_LIMIT_MAX = 100;
}

// https://discord.com/developers/docs/resources/soundboard
namespace soundboard {
    constexpr inline std::size_t NAME_MIN_LENGTH = 2;
    constexpr inline std::size_t NAME_MAX_LENGTH = 32;

    constexpr inline double VOLUME_MIN = 0.0;
    constexpr inline double VOLUME_DEFAULT = 1.0;
    constexpr inline double VOLUME_MAX = 1.0;

    constexpr inline std::size_t MAX_SIZE_BYTES = 512UZ * 1024; // 512 KB
    constexpr inline double MAX_DURATION_SECONDS = 5.2;
    constexpr inline std::size_t MAX_DURATION_MS = 5200;

    constexpr inline std::size_t GUILD_SOUNDS_MAX_TIER_0 = 8;
    constexpr inline std::size_t GUILD_SOUNDS_MAX_TIER_1 = 24;
    constexpr inline std::size_t GUILD_SOUNDS_MAX_TIER_2 = 36;
    constexpr inline std::size_t GUILD_SOUNDS_MAX_TIER_3 = 48;
}

// https://discord.com/developers/docs/resources/stage-instance
namespace stage_instance {
    constexpr inline std::size_t TOPIC_MIN_LENGTH = 1;
    constexpr inline std::size_t TOPIC_MAX_LENGTH = 120;
}

// https://discord.com/developers/docs/resources/sticker
namespace sticker {
    constexpr inline std::size_t MAX_SIZE_BYTES = 512UZ * 1024; // 512 KB
    constexpr inline int WIDTH = 320;
    constexpr inline int HEIGHT = 320;
    constexpr inline std::size_t NAME_MIN_LENGTH = 2;
    constexpr inline std::size_t NAME_MAX_LENGTH = 30;
    constexpr inline std::size_t TAGS_MAX_LENGTH = 200;
    constexpr inline std::size_t DESCRIPTION_MAX_LENGTH = 100;

    constexpr inline std::size_t GUILD_STICKERS_MAX_TIER_0 = 5;
    constexpr inline std::size_t GUILD_STICKERS_MAX_TIER_1 = 15;
    constexpr inline std::size_t GUILD_STICKERS_MAX_TIER_2 = 30;
    constexpr inline std::size_t GUILD_STICKERS_MAX_TIER_3 = 60;
}

// https://discord.com/developers/docs/resources/subscription
namespace subscription {
    constexpr inline std::size_t QUERY_LIMIT_MIN = 1;
    constexpr inline std::size_t QUERY_LIMIT_DEFAULT = 30;
    constexpr inline std::size_t QUERY_LIMIT_MAX = 100;
}

// https://discord.com/developers/docs/resources/user
namespace user {
    constexpr inline std::size_t AVATAR_MAX_SIZE_BYTES = 8UZ * 1024 * 1024; // 8 MB
    constexpr inline int AVATAR_DEFAULT_WIDTH = 512;
    constexpr inline int AVATAR_DEFAULT_HEIGHT = 512;
    constexpr inline int AVATAR_MIN_WIDTH = 128;
    constexpr inline int AVATAR_MIN_HEIGHT = 128;
    constexpr inline int AVATAR_MAX_WIDTH = 4096;
    constexpr inline int AVATAR_MAX_HEIGHT = 4096;

    constexpr inline std::size_t BANNER_MAX_SIZE_BYTES = 10UZ * 1024 * 1024; // 10 MB
    constexpr inline int BANNER_WIDTH = 1920;
    constexpr inline int BANNER_HEIGHT = 1080;
    constexpr inline int BANNER_MIN_WIDTH = 600;
    constexpr inline int BANNER_MIN_HEIGHT = 240;
    constexpr inline int BANNER_MAX_WIDTH = 4096;
    constexpr inline int BANNER_MAX_HEIGHT = 4096;

    constexpr inline std::size_t USERNAME_MIN_LENGTH = 2;
    constexpr inline std::size_t USERNAME_MAX_LENGTH = 32;
    constexpr inline std::size_t GLOBAL_NAME_MIN_LENGTH = 1;
    constexpr inline std::size_t GLOBAL_NAME_MAX_LENGTH = 32;
    constexpr inline std::size_t BIO_MAX_LENGTH = 190;

    constexpr inline std::size_t GET_CURRENT_USER_GUILDS_LIMIT_DEFAULT = 200;
    constexpr inline std::size_t GET_CURRENT_USER_GUILDS_LIMIT_MAX = 200;
}

// https://discord.com/developers/docs/topics/voice-connections
namespace voice {
    constexpr inline std::uint32_t SAMPLE_RATE_HZ = 48000;
    constexpr inline std::size_t CHANNELS_STEREO = 2;
    constexpr inline std::size_t CHANNELS_MONO = 1;
    constexpr inline std::size_t FRAME_DURATION_MS = 20;
    constexpr inline std::size_t SAMPLES_PER_FRAME = 960; // 48000 * 20 / 1000
    constexpr inline std::size_t MAX_FRAME_SIZE_BYTES = 1276;
    constexpr inline std::size_t RTP_HEADER_SIZE_BYTES = 12;
    constexpr inline std::size_t UDP_DISCOVERY_PACKET_SIZE_BYTES = 74;
}

// https://discord.com/developers/docs/resources/webhook
namespace webhook {
    constexpr inline std::size_t NAME_MIN_LENGTH = 1;
    constexpr inline std::size_t NAME_MAX_LENGTH = 80;
    constexpr inline std::size_t AVATAR_MAX_SIZE_BYTES = 8UZ * 1024 * 1024; // 8 MB
    constexpr inline int AVATAR_DEFAULT_WIDTH = 512;
    constexpr inline int AVATAR_DEFAULT_HEIGHT = 512;
}

constexpr inline auto MAX_AUTOCOMPLETE_CHOICES = application_commands::AUTOCOMPLETE_CHOICES_MAX;
constexpr inline auto MAX_USER_INSTALLED_INTERACTION_FOLLOWUP_REPLIES = application::USER_INSTALLED_INTERACTION_FOLLOWUP_REPLIES_MAX;
constexpr inline auto MAX_MESSAGE_LENGTH = message::MAX_LENGTH;
constexpr inline auto MAX_MESSAGE_CONTENT_LENGTH = message::MAX_CONTENT_LENGTH;

constexpr inline auto STICKER_MAX_SIZE_BYTES = sticker::MAX_SIZE_BYTES;
constexpr inline auto STICKER_WIDTH = sticker::WIDTH;
constexpr inline auto STICKER_HEIGHT = sticker::HEIGHT;
constexpr inline auto STICKER_NAME_MIN_LENGTH = sticker::NAME_MIN_LENGTH;
constexpr inline auto STICKER_NAME_MAX_LENGTH = sticker::NAME_MAX_LENGTH;
constexpr inline auto STICKER_TAGS_MAX_LENGTH = sticker::TAGS_MAX_LENGTH;
constexpr inline auto STICKER_DESCRIPTION_MAX_LENGTH = sticker::DESCRIPTION_MAX_LENGTH;

constexpr inline auto EMOJI_MAX_SIZE_BYTES = emoji::MAX_SIZE_BYTES;
constexpr inline auto EMOJI_RECOMMENDED_WIDTH = emoji::RECOMMENDED_WIDTH;
constexpr inline auto EMOJI_RECOMMENDED_HEIGHT = emoji::RECOMMENDED_HEIGHT;
constexpr inline auto EMOJI_NAME_MIN_LENGTH = emoji::NAME_MIN_LENGTH;
constexpr inline auto EMOJI_NAME_MAX_LENGTH = emoji::NAME_MAX_LENGTH;

constexpr inline auto AVATAR_MAX_SIZE_BYTES = user::AVATAR_MAX_SIZE_BYTES;
constexpr inline auto AVATAR_DEFAULT_WIDTH = user::AVATAR_DEFAULT_WIDTH;
constexpr inline auto AVATAR_DEFAULT_HEIGHT = user::AVATAR_DEFAULT_HEIGHT;

constexpr inline auto GUILD_ICON_MAX_SIZE_BYTES = guild::ICON_MAX_SIZE_BYTES;
constexpr inline auto GUILD_ICON_WIDTH = guild::ICON_WIDTH;
constexpr inline auto GUILD_ICON_HEIGHT = guild::ICON_HEIGHT;

constexpr inline auto GUILD_BANNER_MAX_SIZE_BYTES = guild::BANNER_MAX_SIZE_BYTES;
constexpr inline auto GUILD_BANNER_WIDTH = guild::BANNER_WIDTH;
constexpr inline auto GUILD_BANNER_HEIGHT = guild::BANNER_HEIGHT;
constexpr inline auto GUILD_BANNER_MIN_WIDTH = guild::BANNER_MIN_WIDTH;
constexpr inline auto GUILD_BANNER_MIN_HEIGHT = guild::BANNER_MIN_HEIGHT;

constexpr inline auto MESSAGE_MAX_CONTENT_LENGTH = message::MAX_CONTENT_LENGTH;

}
