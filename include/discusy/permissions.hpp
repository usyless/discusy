#pragma once

namespace discusy::permissions {
    // this can be a std::bitset in the future?
    // can switch to enum of just the shifts then use those as indexes into the bitset
    enum class permissions : std::uint64_t {
        CREATE_INSTANT_INVITE = 1ULL << 0U, // Allows creation of instant invites	T, V, S
        KICK_MEMBERS = 1ULL << 1U, // Allows kicking members	
        BAN_MEMBERS = 1ULL << 2U, // Allows banning members	
        ADMINISTRATOR = 1ULL << 3U, // Allows all permissions and bypasses channel permission overwrites	
        MANAGE_CHANNELS = 1ULL << 4U, // Allows management and editing of channels	T, V, S
        MANAGE_GUILD = 1ULL << 5U, // Allows management and editing of the guild	
        ADD_REACTIONS = 1ULL << 6U, // Allows for adding new reactions to messages. This permission does not apply to reacting with an existing reaction on a message.	T, V, S
        VIEW_AUDIT_LOG = 1ULL << 7U, // Allows for viewing of audit logs	
        PRIORITY_SPEAKER = 1ULL << 8U, // Allows for using priority speaker in a voice channel	V
        STREAM = 1ULL << 9U, // Allows the user to go live	V, S
        VIEW_CHANNEL = 1ULL << 10U, // Allows guild members to view a channel, which includes reading messages in text channels and joining voice channels	T, V, S
        SEND_MESSAGES = 1ULL << 11U, // Allows for sending messages in a channel and creating threads in a forum (does not allow sending messages in threads)	T, V, S
        SEND_TTS_MESSAGES = 1ULL << 12U, // Allows for sending of /tts messages	T, V, S
        MANAGE_MESSAGES = 1ULL << 13U, // Allows for deletion of other users messages	T, V, S
        EMBED_LINKS = 1ULL << 14U, // Links sent by users with this permission will be auto-embedded	T, V, S
        ATTACH_FILES = 1ULL << 15U, // Allows for uploading images and files	T, V, S
        READ_MESSAGE_HISTORY = 1ULL << 16U, // Allows for reading of message history	T, V, S
        MENTION_EVERYONE = 1ULL << 17U, // Allows for using the @everyone tag to notify all users in a channel, and the @here tag to notify all online users in a channel	T, V, S
        USE_EXTERNAL_EMOJIS = 1ULL << 18U, // Allows the usage of custom emojis from other servers	T, V, S
        VIEW_GUILD_INSIGHTS = 1ULL << 19U, // Allows for viewing guild insights	
        CONNECT = 1ULL << 20U, // Allows for joining of a voice channel	V, S
        SPEAK = 1ULL << 21U, // Allows for speaking in a voice channel	V
        MUTE_MEMBERS = 1ULL << 22U, // Allows for muting members in a voice channel	V, S
        DEAFEN_MEMBERS = 1ULL << 23U, // Allows for deafening of members in a voice channel	V
        MOVE_MEMBERS = 1ULL << 24U, // Allows for moving of members between voice channels	V, S
        USE_VAD = 1ULL << 25U, // Allows for using voice-activity-detection in a voice channel	V
        CHANGE_NICKNAME = 1ULL << 26U, // Allows for modification of own nickname	
        MANAGE_NICKNAMES = 1ULL << 27U, // Allows for modification of other users nicknames	
        MANAGE_ROLES = 1ULL << 28U, // Allows management and editing of roles	T, V, S
        MANAGE_WEBHOOKS = 1ULL << 29U, // Allows management and editing of webhooks	T, V, S
        MANAGE_GUILD_EXPRESSIONS = 1ULL << 30U, // Allows for editing and deleting emojis, stickers, and soundboard sounds created by all users	
        USE_APPLICATION_COMMANDS = 1ULL << 31U, // Allows members to use application commands, including slash commands and context menu commands.	T, V, S
        REQUEST_TO_SPEAK = 1ULL << 32U, // Allows for requesting to speak in stage channels	S
        MANAGE_EVENTS = 1ULL << 33U, // Allows for editing and deleting scheduled events created by all users	V, S
        MANAGE_THREADS = 1ULL << 34U, // Allows for deleting and archiving threads, and viewing all private threads	T
        CREATE_PUBLIC_THREADS = 1ULL << 35U, // Allows for creating public and announcement threads	T
        CREATE_PRIVATE_THREADS = 1ULL << 36U, // Allows for creating private threads	T
        USE_EXTERNAL_STICKERS = 1ULL << 37U, // Allows the usage of custom stickers from other servers	T, V, S
        SEND_MESSAGES_IN_THREADS = 1ULL << 38U, // Allows for sending messages in threads	T
        USE_EMBEDDED_ACTIVITIES = 1ULL << 39U, // Allows for using Activities (applications with the EMBEDDED flag)	T, V
        MODERATE_MEMBERS = 1ULL << 40U, // Allows for timing out users to prevent them from sending or reacting to messages in chat and threads, and from speaking in voice and stage channels	
        VIEW_CREATOR_MONETIZATION_ANALYTICS = 1ULL << 41U, // Allows for viewing role subscription insights	
        USE_SOUNDBOARD = 1ULL << 42U, // Allows for using soundboard in a voice channel	V
        CREATE_GUILD_EXPRESSIONS = 1ULL << 43U, // Allows for creating emojis, stickers, and soundboard sounds, and editing and deleting those created by the current user.	
        CREATE_EVENTS = 1ULL << 44U, // Allows for creating scheduled events, and editing and deleting those created by the current user.	V, S
        USE_EXTERNAL_SOUNDS = 1ULL << 45U, // Allows the usage of custom soundboard sounds from other servers	V
        SEND_VOICE_MESSAGES = 1ULL << 46U, // Allows sending voice messages	T, V, S
        SET_VOICE_CHANNEL_STATUS = 1ULL << 48U, // Allows setting voice channel status	V
        SEND_POLLS = 1ULL << 49U, // Allows sending polls	T, V, S
        USE_EXTERNAL_APPS = 1ULL << 50U, // Allows user-installed apps to send public responses. When disabled, users will still be allowed to use their apps but the responses will be ephemeral. This only applies to apps not also installed to the server.	T, V, S
        PIN_MESSAGES = 1ULL << 51U, // Allows pinning and unpinning messages	T
        BYPASS_SLOWMODE = 1ULL << 52U, // Allows bypassing slowmode restrictions	
    };
}