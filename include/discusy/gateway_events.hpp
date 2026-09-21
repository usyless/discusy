#pragma once


#include <string>
#include <vector>
#include <array>
#include <variant>

#include <glaze/glaze.hpp>

#include "opcode.hpp"
#include "types.hpp"
#include "intents.hpp"

#include "events.hpp"
#include "api_types.hpp"

static_assert(true, "Clangd bug fix");
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-braces"
#endif

#pragma push_macro("DISCUSY_X")
#undef DISCUSY_X
#pragma push_macro("DISCUSY_D")
#undef DISCUSY_D
#define DISCUSY_D
#pragma push_macro("DISCORD_GATEWAY_EVENTS")
#undef DISCORD_GATEWAY_EVENTS
#define DISCORD_GATEWAY_EVENTS \
    DISCUSY_X(HELLO, hello) DISCUSY_D \
    DISCUSY_X(READY, ready) DISCUSY_D \
    DISCUSY_X(RESUMED, resumed) DISCUSY_D \
    DISCUSY_X(RECONNECT, reconnect) DISCUSY_D \
    DISCUSY_X(RATE_LIMITED, rate_limited) DISCUSY_D \
    DISCUSY_X(INVALID_SESSION, invalid_session) DISCUSY_D \
    DISCUSY_X(APPLICATION_COMMAND_PERMISSIONS_UPDATE, application_command_permissions_update) DISCUSY_D \
    DISCUSY_X(AUTO_MODERATION_RULE_CREATE, auto_moderation_rule_create) DISCUSY_D \
    DISCUSY_X(AUTO_MODERATION_RULE_UPDATE, auto_moderation_rule_update) DISCUSY_D \
    DISCUSY_X(AUTO_MODERATION_RULE_DELETE, auto_moderation_rule_delete) DISCUSY_D \
    DISCUSY_X(AUTO_MODERATION_ACTION_EXECUTION, auto_moderation_action_execution) DISCUSY_D \
    DISCUSY_X(CHANNEL_CREATE, channel_create) DISCUSY_D \
    DISCUSY_X(CHANNEL_UPDATE, channel_update) DISCUSY_D \
    DISCUSY_X(CHANNEL_DELETE, channel_delete) DISCUSY_D \
    DISCUSY_X(CHANNEL_INFO, channel_info) DISCUSY_D \
    DISCUSY_X(CHANNEL_PINS_UPDATE, channel_pins_update) DISCUSY_D \
    DISCUSY_X(THREAD_CREATE, thread_create) DISCUSY_D \
    DISCUSY_X(THREAD_UPDATE, thread_update) DISCUSY_D \
    DISCUSY_X(THREAD_DELETE, thread_delete) DISCUSY_D \
    DISCUSY_X(THREAD_LIST_SYNC, thread_list_sync) DISCUSY_D \
    DISCUSY_X(THREAD_MEMBER_UPDATE, thread_member_update) DISCUSY_D \
    DISCUSY_X(THREAD_MEMBERS_UPDATE, thread_members_update) DISCUSY_D \
    DISCUSY_X(ENTITLEMENT_CREATE, entitlement_create) DISCUSY_D \
    DISCUSY_X(ENTITLEMENT_UPDATE, entitlement_update) DISCUSY_D \
    DISCUSY_X(ENTITLEMENT_DELETE, entitlement_delete) DISCUSY_D \
    DISCUSY_X(GUILD_CREATE, guild_create) DISCUSY_D \
    DISCUSY_X(GUILD_UPDATE, guild_update) DISCUSY_D \
    DISCUSY_X(GUILD_DELETE, guild_delete) DISCUSY_D \
    DISCUSY_X(GUILD_AUDIT_LOG_ENTRY_CREATE, guild_audit_log_entry_create) DISCUSY_D \
    DISCUSY_X(GUILD_BAN_ADD, guild_ban_add) DISCUSY_D \
    DISCUSY_X(GUILD_BAN_REMOVE, guild_ban_remove) DISCUSY_D \
    DISCUSY_X(GUILD_EMOJIS_UPDATE, guild_emojis_update) DISCUSY_D \
    DISCUSY_X(GUILD_STICKERS_UPDATE, guild_stickers_update) DISCUSY_D \
    DISCUSY_X(GUILD_INTEGRATIONS_UPDATE, guild_integrations_update) DISCUSY_D \
    DISCUSY_X(GUILD_MEMBER_ADD, guild_member_add) DISCUSY_D \
    DISCUSY_X(GUILD_MEMBER_REMOVE, guild_member_remove) DISCUSY_D \
    DISCUSY_X(GUILD_MEMBER_UPDATE, guild_member_update) DISCUSY_D \
    DISCUSY_X(GUILD_MEMBERS_CHUNK, guild_members_chunk) DISCUSY_D \
    DISCUSY_X(GUILD_ROLE_CREATE, guild_role_create) DISCUSY_D \
    DISCUSY_X(GUILD_ROLE_UPDATE, guild_role_update) DISCUSY_D \
    DISCUSY_X(GUILD_ROLE_DELETE, guild_role_delete) DISCUSY_D \
    DISCUSY_X(GUILD_SCHEDULED_EVENT_CREATE, guild_scheduled_event_create) DISCUSY_D \
    DISCUSY_X(GUILD_SCHEDULED_EVENT_UPDATE, guild_scheduled_event_update) DISCUSY_D \
    DISCUSY_X(GUILD_SCHEDULED_EVENT_DELETE, guild_scheduled_event_delete) DISCUSY_D \
    DISCUSY_X(GUILD_SCHEDULED_EVENT_USER_ADD, guild_scheduled_event_user_add) DISCUSY_D \
    DISCUSY_X(GUILD_SCHEDULED_EVENT_USER_REMOVE, guild_scheduled_event_user_remove) DISCUSY_D \
    DISCUSY_X(GUILD_SOUNDBOARD_SOUND_CREATE, guild_soundboard_sound_create) DISCUSY_D \
    DISCUSY_X(GUILD_SOUNDBOARD_SOUND_UPDATE, guild_soundboard_sound_update) DISCUSY_D \
    DISCUSY_X(GUILD_SOUNDBOARD_SOUND_DELETE, guild_soundboard_sound_delete) DISCUSY_D \
    DISCUSY_X(GUILD_SOUNDBOARD_SOUNDS_UPDATE, guild_soundboard_sounds_update) DISCUSY_D \
    DISCUSY_X(SOUNDBOARD_SOUNDS, soundboard_sounds) DISCUSY_D \
    DISCUSY_X(INTEGRATION_CREATE, integration_create) DISCUSY_D \
    DISCUSY_X(INTEGRATION_UPDATE, integration_update) DISCUSY_D \
    DISCUSY_X(INTEGRATION_DELETE, integration_delete) DISCUSY_D \
    DISCUSY_X(INTERACTION_CREATE, interaction_create) DISCUSY_D \
    DISCUSY_X(INVITE_CREATE, invite_create) DISCUSY_D \
    DISCUSY_X(INVITE_DELETE, invite_delete) DISCUSY_D \
    DISCUSY_X(MESSAGE_CREATE, message_create) DISCUSY_D \
    DISCUSY_X(MESSAGE_UPDATE, message_update) DISCUSY_D \
    DISCUSY_X(MESSAGE_DELETE, message_delete) DISCUSY_D \
    DISCUSY_X(MESSAGE_DELETE_BULK, message_delete_bulk) DISCUSY_D \
    DISCUSY_X(MESSAGE_REACTION_ADD, message_reaction_add) DISCUSY_D \
    DISCUSY_X(MESSAGE_REACTION_REMOVE, message_reaction_remove) DISCUSY_D \
    DISCUSY_X(MESSAGE_REACTION_REMOVE_ALL, message_reaction_remove_all) DISCUSY_D \
    DISCUSY_X(MESSAGE_REACTION_REMOVE_EMOJI, message_reaction_remove_emoji) DISCUSY_D \
    DISCUSY_X(PRESENCE_UPDATE, presence_update) DISCUSY_D \
    DISCUSY_X(STAGE_INSTANCE_CREATE, stage_instance_create) DISCUSY_D \
    DISCUSY_X(STAGE_INSTANCE_UPDATE, stage_instance_update) DISCUSY_D \
    DISCUSY_X(STAGE_INSTANCE_DELETE, stage_instance_delete) DISCUSY_D \
    DISCUSY_X(SUBSCRIPTION_CREATE, subscription_create) DISCUSY_D \
    DISCUSY_X(SUBSCRIPTION_UPDATE, subscription_update) DISCUSY_D \
    DISCUSY_X(SUBSCRIPTION_DELETE, subscription_delete) DISCUSY_D \
    DISCUSY_X(TYPING_START, typing_start) DISCUSY_D \
    DISCUSY_X(USER_UPDATE, user_update) DISCUSY_D \
    DISCUSY_X(VOICE_CHANNEL_EFFECT_SEND, voice_channel_effect_send) DISCUSY_D \
    DISCUSY_X(VOICE_CHANNEL_START_TIME_UPDATE, voice_channel_start_time_update) DISCUSY_D \
    DISCUSY_X(VOICE_CHANNEL_STATUS_UPDATE, voice_channel_status_update) DISCUSY_D \
    DISCUSY_X(VOICE_STATE_UPDATE, voice_state_update) DISCUSY_D \
    DISCUSY_X(VOICE_SERVER_UPDATE, voice_server_update) DISCUSY_D \
    DISCUSY_X(WEBHOOKS_UPDATE, webhooks_update) DISCUSY_D \
    DISCUSY_X(MESSAGE_POLL_VOTE_ADD, message_poll_vote_add) DISCUSY_D \
    DISCUSY_X(MESSAGE_POLL_VOTE_REMOVE, message_poll_vote_remove)

namespace discusy {

namespace recieve_event {

enum class event : std::uint8_t {
#pragma push_macro("DISCUSY_D")
#undef DISCUSY_D
#define DISCUSY_D ,
#define DISCUSY_X(name, _) name
    DISCORD_GATEWAY_EVENTS
#undef DISCUSY_X
#undef DISCUSY_D
#pragma pop_macro("DISCUSY_D")
    ,
};

struct payload_base {
    // Gateway opcode
    Opcode op{};
    // Sequence number used for resuming sessions and heartbeating
    opt<std::int64_t> s{};
    // The event type for this payload
    opt<event> t{};
};

template <typename T>
struct payload {
    // Event data
    T d;
};

struct hello {
    // Interval (in milliseconds) an app should heartbeat with
    integer heartbeat_interval{};
};

struct ready_application_partial {
    // Application ID
    snowflake id{};
    opt<flags_t<discusy::application::application_flags>> flags{};
};

struct ready {
    // API version
    integer v{};
    discusy::user::user user{};
    std::vector<discusy::guild::unavailable_guild> guilds{};
    std::string session_id{};
    std::string resume_gateway_url{};
    opt<std::array<std::uint32_t, 2>> shard{};
    ready_application_partial application{};
};

using resumed = glz::skip;
using reconnect = glz::skip;

using invalid_session = bool;

namespace application_commands {
    using application_command_permissions_update = discusy::application_commands::application_command_permissions;
}
using namespace application_commands;

namespace auto_moderation {
    using auto_moderation_rule_create = discusy::auto_moderation::auto_moderation_rule;

    using auto_moderation_rule_update = discusy::auto_moderation::auto_moderation_rule;

    using auto_moderation_rule_delete = discusy::auto_moderation::auto_moderation_rule;

    struct auto_moderation_action_execution {
        // ID of the guild in which action was executed
        snowflake guild_id{};
        // Action which was executed
        discusy::auto_moderation::auto_moderation_action action{};
        // ID of the rule which action belongs to
        snowflake rule_id{};
        // Trigger type of rule which was triggered (1=KEYWORD, 3=SPAM, 4=KEYWORD_PRESET, 5=MENTION_SPAM, 6=MEMBER_PROFILE)
        discusy::auto_moderation::auto_moderation_rule_trigger_type rule_trigger_type{};
        // ID of the user which generated the content which triggered the rule
        snowflake user_id{};
        // ID of the channel in which user content was posted
        opt<snowflake> channel_id{};
        // ID of any user message which content belongs to
        opt<snowflake> message_id{};
        // ID of any system auto moderation messages posted as a result of this action
        opt<snowflake> alert_system_message_id{};
        // User-generated text content (requires MESSAGE_CONTENT intent)
        std::string content{};
        // Word or phrase configured in the rule that triggered the rule
        opt<std::string> matched_keyword{};
        // Substring in content that triggered the rule (requires MESSAGE_CONTENT intent)
        opt<std::string> matched_content{};
    };
}
using namespace auto_moderation;

namespace channels {
    using channel_create = discusy::channel::channel;

    using channel_update = discusy::channel::channel;

    using channel_delete = discusy::channel::channel;

    struct channel_info_channel {
        snowflake id{};
        opt<std::string> status{};
        opt<integer> voice_start_time{};
    };

    struct channel_info {
        snowflake guild_id{};
        std::vector<channel_info_channel> channels;
    };

    struct voice_channel_status_update {
        snowflake id{};
        snowflake guild_id{};
        opt<std::string> status{};
    };

    struct voice_channel_start_time_update {
        snowflake id{};
        snowflake guild_id{};
        opt<integer> voice_start_time{};
    };

    using thread_create = discusy::channel::channel;

    using thread_update = discusy::channel::channel;

    using thread_delete = discusy::channel::channel;

    struct thread_list_sync {
        snowflake guild_id{};
        opt<std::vector<snowflake>> channel_ids{};
        std::vector<discusy::channel::channel> threads{};
        std::vector<discusy::channel::thread_member> members{};
    };

    using thread_member_update = discusy::channel::thread_member_with_guild;

    struct thread_members_update {
        snowflake id{};
        snowflake guild_id{};
        integer member_count{};
        opt<std::vector<discusy::channel::thread_member>> added_members{};
        opt<std::vector<snowflake>> removed_member_ids{};
    };

    struct channel_pins_update {
        opt<snowflake> guild_id{};
        snowflake channel_id{};
        opt<timestamp> last_pin_timestamp{};
    };
}
using namespace channels;

namespace entitlements {
    using entitlement_create = discusy::entitlement::entitlement;

    using entitlement_update = discusy::entitlement::entitlement;

    using entitlement_delete = discusy::entitlement::entitlement;
}
using namespace entitlements;

namespace presence {
    enum class activity_type : std::uint8_t {
        playing = 0,
        streaming = 1,
        listening = 2,
        watching = 3,
        custom = 4,
        competing = 5,
    };

    enum class status : std::uint8_t { // update glaze meta
        idle,
        dnd,
        online,
        offline,
    };

    enum class status_display_type : std::uint8_t {
        Name = 0, // ”Listening to Spotify”
        State = 1, // ”Listening to Rick Astley”
        Details = 2, // ”Listening to Never Gonna Give You Up”
    };

    struct activity_timestamps {
        opt<integer> start{};
        opt<integer> end{};

        static activity_timestamps create(opt<integer> start_ = {}, opt<integer> end_ = {}) noexcept {
            return activity_timestamps{.start = start_, .end = end_};
        }
        decltype(auto) set_start(this auto&& self, opt<integer> s) noexcept { self.start = s; return std::forward<decltype(self)>(self); }
        decltype(auto) set_end(this auto&& self, opt<integer> e) noexcept { self.end = e; return std::forward<decltype(self)>(self); }
    };

    struct activity_party {
        opt<std::string> id{};
        opt<std::array<integer, 2>> size{};

        static activity_party create(opt<std::string> id_ = {}, opt<std::array<integer, 2>> size_ = {}) {
            return activity_party{.id = std::move(id_), .size = size_};
        }
        decltype(auto) set_id(this auto&& self, opt<std::string> id_) { self.id = std::move(id_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_size(this auto&& self, opt<std::array<integer, 2>> s) noexcept { self.size = s; return std::forward<decltype(self)>(self); }
        decltype(auto) set_size(this auto&& self, integer current, integer max) noexcept { self.size = std::array<integer, 2>{current, max}; return std::forward<decltype(self)>(self); }
    };

    struct activity_assets {
        opt<std::string> large_image{};
        opt<std::string> large_text{};
        opt<std::string> large_url{};
        opt<std::string> small_image{};
        opt<std::string> small_text{};
        opt<std::string> small_url{};
        opt<std::string> invite_cover_image{};

        static activity_assets create() {
            return activity_assets{};
        }
        decltype(auto) set_large_image(this auto&& self, opt<std::string> li) { self.large_image = std::move(li); return std::forward<decltype(self)>(self); }
        decltype(auto) set_large_text(this auto&& self, opt<std::string> lt) { self.large_text = std::move(lt); return std::forward<decltype(self)>(self); }
        decltype(auto) set_large_url(this auto&& self, opt<std::string> lu) { self.large_url = std::move(lu); return std::forward<decltype(self)>(self); }
        decltype(auto) set_small_image(this auto&& self, opt<std::string> si) { self.small_image = std::move(si); return std::forward<decltype(self)>(self); }
        decltype(auto) set_small_text(this auto&& self, opt<std::string> st) { self.small_text = std::move(st); return std::forward<decltype(self)>(self); }
        decltype(auto) set_small_url(this auto&& self, opt<std::string> su) { self.small_url = std::move(su); return std::forward<decltype(self)>(self); }
        decltype(auto) set_invite_cover_image(this auto&& self, opt<std::string> ici) { self.invite_cover_image = std::move(ici); return std::forward<decltype(self)>(self); }
    };

    struct activity_secrets {
        opt<std::string> join{};
        opt<std::string> spectate{};
        opt<std::string> match{};

        static activity_secrets create(opt<std::string> join_ = {}, opt<std::string> spectate_ = {}, opt<std::string> match_ = {}) {
            return activity_secrets{.join = std::move(join_), .spectate = std::move(spectate_), .match = std::move(match_)};
        }
        decltype(auto) set_join(this auto&& self, opt<std::string> j) { self.join = std::move(j); return std::forward<decltype(self)>(self); }
        decltype(auto) set_spectate(this auto&& self, opt<std::string> s) { self.spectate = std::move(s); return std::forward<decltype(self)>(self); }
        decltype(auto) set_match(this auto&& self, opt<std::string> m) { self.match = std::move(m); return std::forward<decltype(self)>(self); }
    };

    enum class activity_flags : std::uint16_t {
        INSTANCE = 1ULL << 0,
        JOIN = 1ULL << 1,
        SPECTATE = 1ULL << 2,
        JOIN_REQUEST = 1ULL << 3,
        SYNC = 1ULL << 4,
        PLAY = 1ULL << 5,
        PARTY_PRIVACY_FRIENDS = 1ULL << 6,
        PARTY_PRIVACY_VOICE_CHANNEL = 1ULL << 7,
        EMBEDDED = 1ULL << 8,
    };

    struct activity_button {
        std::string label{};
        std::string url{};

        static activity_button create(std::string label_ = {}, std::string url_ = {}) {
            return activity_button{.label = std::move(label_), .url = std::move(url_)};
        }
        decltype(auto) set_label(this auto&& self, std::string l) { self.label = std::move(l); return std::forward<decltype(self)>(self); }
        decltype(auto) set_url(this auto&& self, std::string u) { self.url = std::move(u); return std::forward<decltype(self)>(self); }
    };

    struct activity {
        std::string name{};
        activity_type type{};
        opt<std::string> url{};
        integer created_at{};
        opt<activity_timestamps> timestamps{};
        opt<snowflake> application_id{};
        opt<discusy::recieve_event::presence::status_display_type> status_display_type{};
        opt<std::string> details{};
        opt<std::string> details_url{};
        opt<std::string> state{};
        opt<std::string> state_url{};
        opt<discusy::emoji::emoji> emoji{};
        opt<activity_party> party{};
        opt<activity_assets> assets{};
        opt<activity_secrets> secrets{};
        opt<bool> instance{};
        opt<flags_t<activity_flags>> flags{};
        opt<std::vector<activity_button>> buttons{};

        decltype(auto) set_name(this auto&& self, std::string n) {
            if (self.type == activity_type::custom) {
                self.name = "custom activity";
                self.state = std::move(n);
            } else {
                self.name = std::move(n);
            }
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_type(this auto&& self, activity_type t) {
            if (t == activity_type::custom) {
                if (self.type != activity_type::custom) {
                    if (self.name != "custom activity" && !self.name.empty()) {
                        self.state = std::move(self.name);
                    }
                    self.name = "custom activity";
                }
            } else if (self.type == activity_type::custom) {
                if (self.name == "custom activity" && self.state) {
                    self.name = std::move(*self.state);
                    self.state.reset();
                }
            }
            self.type = t;
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_url(this auto&& self, opt<std::string> u) { self.url = std::move(u); return std::forward<decltype(self)>(self); }
        decltype(auto) set_created_at(this auto&& self, integer ca) noexcept { self.created_at = ca; return std::forward<decltype(self)>(self); }
        decltype(auto) set_timestamps(this auto&& self, opt<activity_timestamps> ts) noexcept { self.timestamps = ts; return std::forward<decltype(self)>(self); }
        decltype(auto) set_application_id(this auto&& self, opt<snowflake> aid) noexcept { self.application_id = aid; return std::forward<decltype(self)>(self); }
        decltype(auto) set_status_display_type(this auto&& self, opt<discusy::recieve_event::presence::status_display_type> sdt) noexcept { self.status_display_type = sdt; return std::forward<decltype(self)>(self); }
        decltype(auto) set_details(this auto&& self, opt<std::string> d) { self.details = std::move(d); return std::forward<decltype(self)>(self); }
        decltype(auto) set_details_url(this auto&& self, opt<std::string> du) { self.details_url = std::move(du); return std::forward<decltype(self)>(self); }
        decltype(auto) set_state(this auto&& self, opt<std::string> s) { self.state = std::move(s); return std::forward<decltype(self)>(self); }
        decltype(auto) set_state_url(this auto&& self, opt<std::string> su) { self.state_url = std::move(su); return std::forward<decltype(self)>(self); }
        decltype(auto) set_emoji(this auto&& self, opt<discusy::emoji::emoji> e) { self.emoji = std::move(e); return std::forward<decltype(self)>(self); }
        decltype(auto) set_party(this auto&& self, opt<activity_party> p) { self.party = std::move(p); return std::forward<decltype(self)>(self); }
        decltype(auto) set_assets(this auto&& self, opt<activity_assets> a) { self.assets = std::move(a); return std::forward<decltype(self)>(self); }
        decltype(auto) set_secrets(this auto&& self, opt<activity_secrets> s) { self.secrets = std::move(s); return std::forward<decltype(self)>(self); }
        decltype(auto) set_instance(this auto&& self, opt<bool> i) noexcept { self.instance = i; return std::forward<decltype(self)>(self); }
        decltype(auto) set_flags(this auto&& self, opt<flags_t<activity_flags>> f) noexcept { self.flags = f; return std::forward<decltype(self)>(self); }
        decltype(auto) set_buttons(this auto&& self, opt<std::vector<activity_button>> b) { self.buttons = std::move(b); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_buttons, activity_button)
        decltype(auto) add_button(this auto&& self, activity_button b) {
            if (!self.buttons) self.buttons.emplace();
            self.buttons->emplace_back(std::move(b));
            return std::forward<decltype(self)>(self);
        }

        static activity create(const activity_type type_, std::string name_) {
            activity act{};
            act.type = type_;
            if (!name_.empty()) {
                act.set_name(std::move(name_));
            } else if (type_ == activity_type::custom) {
                act.name = "custom activity";
            }
            return act;
        }
    };

    struct client_status {
        opt<std::string> desktop{};
        opt<std::string> mobile{};
        opt<std::string> web{};
        opt<std::string> vr{};

        static client_status create() {
            return client_status{};
        }
        decltype(auto) set_desktop(this auto&& self, opt<std::string> d) { self.desktop = std::move(d); return std::forward<decltype(self)>(self); }
        decltype(auto) set_mobile(this auto&& self, opt<std::string> m) { self.mobile = std::move(m); return std::forward<decltype(self)>(self); }
        decltype(auto) set_web(this auto&& self, opt<std::string> w) { self.web = std::move(w); return std::forward<decltype(self)>(self); }
        decltype(auto) set_vr(this auto&& self, opt<std::string> v) { self.vr = std::move(v); return std::forward<decltype(self)>(self); }
    };

    struct presence_update {
        discusy::user::user user{};
        snowflake guild_id{};
        discusy::recieve_event::presence::status status{};
        std::vector<activity> activities{};
        discusy::recieve_event::presence::client_status client_status{};

        static presence_update create(discusy::user::user u = {}, snowflake gid = {}, discusy::recieve_event::presence::status st = discusy::recieve_event::presence::status::online) {
            return presence_update{.user = std::move(u), .guild_id = gid, .status = st};
        }
        decltype(auto) set_user(this auto&& self, discusy::user::user u) { self.user = std::move(u); return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild_id(this auto&& self, snowflake gid) noexcept { self.guild_id = gid; return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild(this auto&& self, snowflake gid) noexcept { return std::forward<decltype(self)>(self).set_guild_id(gid); }
        decltype(auto) set_status(this auto&& self, discusy::recieve_event::presence::status st) noexcept { self.status = st; return std::forward<decltype(self)>(self); }
        decltype(auto) set_activities(this auto&& self, std::vector<activity> acts) { self.activities = std::move(acts); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_activities, activity)
        decltype(auto) add_activity(this auto&& self, activity act) { self.activities.emplace_back(std::move(act)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_client_status(this auto&& self, discusy::recieve_event::presence::client_status cs) { self.client_status = std::move(cs); return std::forward<decltype(self)>(self); }
    };

    struct typing_start {
        snowflake channel_id{};
        opt<snowflake> guild_id{};
        snowflake user_id{};
        integer timestamp{};
        opt<discusy::guild::guild_member> member{};
    };

    using user_update = discusy::user::user;
}
using namespace presence;

namespace guilds {
    struct guild_create_struct {
        discusy::guild::guild guild{};

        struct Other {
            timestamp joined_at{};
            bool large{};
            opt<bool> unavailable{};
            integer member_count{};
            std::vector<discusy::voice::voice_state> voice_states{}; // partial, double check
            std::vector<discusy::guild::guild_member> members{};
            std::vector<discusy::channel::channel> channels{};
            std::vector<discusy::channel::channel> threads{};
            std::vector<presence::presence_update> presences{};
            std::vector<discusy::stage_instance::stage_instance> stage_instances{};
            std::vector<discusy::guild_scheduled_event::guild_scheduled_event> guild_scheduled_events{};
            std::vector<discusy::soundboard::soundboard_sound> soundboard_sounds{};
        } other;

        constexpr auto* operator->(this auto&& self) noexcept { return std::addressof(self.guild); }
        constexpr decltype(auto) operator*(this auto&& self) noexcept { return (std::forward<decltype(self)>(self).guild); }
        constexpr decltype(auto) operator()(this auto&& self) noexcept { return (std::forward<decltype(self)>(self).other); }

        struct glaze {
            using T = guild_create_struct;

            static constexpr auto value = glz::merge{
                &T::guild,
                &T::other,
            };
        };
    };

    using guild_create = std::variant<guild_create_struct, discusy::guild::unavailable_guild>;
    
    using guild_update = discusy::guild::guild;

    using guild_delete = discusy::guild::unavailable_guild;

    using guild_audit_log_entry_create = discusy::audit_log::audit_log_entry_with_guild;

    struct guild_ban_add {
        snowflake guild_id{};
        discusy::user::user user{};
    };

    using guild_ban_remove = guild_ban_add;

    struct guild_emojis_update {
        snowflake guild_id{};
        std::vector<discusy::emoji::emoji> emojis{};
    };

    struct guild_stickers_update {
        snowflake guild_id{};
        std::vector<discusy::sticker::sticker> stickers{};
    };

    struct guild_integrations_update {
        snowflake guild_id{};
    };

    using guild_member_add = discusy::guild::guild_member_with_guild;

    using guild_member_remove = guild_ban_add;

    struct guild_member_update {
        snowflake guild_id{};
        std::vector<snowflake> roles{};
        discusy::user::user user{};
        opt<std::string> nick{};
        opt<guild_member_avatar_hash> avatar{};
        opt<guild_member_banner_hash> banner{};
        opt<timestamp> joined_at{};
        opt<timestamp> premium_since{};
        opt<bool> deaf{};
        opt<bool> mute{};
        opt<bool> pending{};
        opt<timestamp> communication_disabled_until{};
        opt<discusy::user::avatar_decoration_data> avatar_decoration_data{};
        opt<discusy::user::collectibles> collectibles{};
    };

    struct guild_members_chunk {
        snowflake guild_id{};
        std::vector<discusy::guild::guild_member> members{};
        integer chunk_index{};
        integer chunk_count{};
        opt<std::vector<glz::raw_json_view>> not_found{};
        opt<std::vector<presence::presence_update>> presences{};
        opt<std::string> nonce{};
    };

    struct guild_role_create {
        snowflake guild_id{};
        discusy::permissions::role role{};
    };

    using guild_role_update = guild_role_create;

    struct guild_role_delete {
        snowflake guild_id{};
        snowflake role_id{};
    };

    using guild_scheduled_event_create = discusy::guild_scheduled_event::guild_scheduled_event;

    using guild_scheduled_event_update = discusy::guild_scheduled_event::guild_scheduled_event;

    using guild_scheduled_event_delete = discusy::guild_scheduled_event::guild_scheduled_event;

    struct guild_scheduled_event_user_add {
        snowflake guild_scheduled_event_id{};
        snowflake user_id{};
        snowflake guild_id{};
    };

    using guild_scheduled_event_user_remove = guild_scheduled_event_user_add;

    using guild_soundboard_sound_create = discusy::soundboard::soundboard_sound;

    using guild_soundboard_sound_update = discusy::soundboard::soundboard_sound;

    struct guild_soundboard_sound_delete {
        snowflake sound_id{};
        snowflake guild_id{};
    };

    struct guild_soundboard_sounds_update {
        std::vector<discusy::soundboard::soundboard_sound> soundboard_sounds{};
        snowflake guild_id{};
    };

    using soundboard_sounds = guild_soundboard_sounds_update;
}
using namespace guilds;

namespace integrations {
    using integration_create = discusy::guild::integration_with_guild;

    using integration_update = discusy::guild::integration_with_guild;

    struct integration_delete {
        snowflake id{};
        snowflake guild_id{};
        opt<snowflake> application_id{};
    };
}
using namespace integrations;

namespace invites {
    struct invite_create {
        snowflake channel_id{};
        std::string code{};
        timestamp created_at{};
        opt<snowflake> guild_id{};
        opt<discusy::user::user> inviter{};
        integer max_age{};
        integer max_uses{};
        opt<discusy::invite::invite_target_type> target_type{};
        opt<discusy::user::user> target_user{};
        opt<discusy::application::application> target_application{}; // partial, double check
        bool temporary{};
        integer uses{};
        opt<timestamp> expires_at{};
        opt<std::vector<snowflake>> role_ids{};
    };

    struct invite_delete {
        snowflake channel_id{};
        opt<snowflake> guild_id{};
        std::string code{};
    };
}
using namespace invites;

namespace messages {
    struct message_create {
        discusy::message::message message{};

        struct Other {
            opt<snowflake> guild_id{};
            opt<discusy::guild::guild_member> member{};
            opt<discusy::channel::channel_type> channel_type{};
        } other;

        constexpr auto* operator->(this auto&& self) noexcept { return std::addressof(self.message); }
        constexpr decltype(auto) operator*(this auto&& self) noexcept { return (std::forward<decltype(self)>(self).message); }
        constexpr decltype(auto) operator()(this auto&& self) noexcept { return (std::forward<decltype(self)>(self).other); }

        discusy::bot* bot_ptr_{nullptr};

        void set_bot_void(this auto&& self, discusy::bot* b) noexcept {
            self.bot_ptr_ = b;
            self.message.set_bot_void(b);
            if (self.other.member) {
                self.other.member->set_bot_void(b);
                self.other.member->set_guild_id_context_void(self.other.guild_id);
            }
        }
        decltype(auto) set_bot(this auto&& self, discusy::bot* b) noexcept { self.set_bot_void(b); return std::forward<decltype(self)>(self); }

        struct glaze {
            using T = message_create;

            static constexpr auto value = glz::merge{
                &T::message,
                &T::other,
            };
        };

        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto send(this auto&& self, CompletionToken&& token = ctx::io_context::dct_t()) {
            return std::forward<decltype(self)>(self).message.template send<ReturnResult>(std::forward<CompletionToken>(token));
        }
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto reply(this auto&& self, std::string content, bool ping = false, CompletionToken&& token = ctx::io_context::dct_t()) {
            return self.message.template reply<ReturnResult>(std::move(content), ping, std::forward<CompletionToken>(token));
        }
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto reply_embed(this auto&& self, discusy::message::embed e, bool ping = false, CompletionToken&& token = ctx::io_context::dct_t()) {
            return self.message.template reply_embed<ReturnResult>(std::move(e), ping, std::forward<CompletionToken>(token));
        }
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto reply_file(this auto&& self, discusy::upload_file f, std::string content = {}, bool ping = false, CompletionToken&& token = ctx::io_context::dct_t()) {
            return self.message.template reply_file<ReturnResult>(std::move(f), std::move(content), ping, std::forward<CompletionToken>(token));
        }
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto reply_file(this auto&& self, discusy::upload_file_view f, std::string content = {}, bool ping = false, CompletionToken&& token = ctx::io_context::dct_t()) {
            return self.message.template reply_file<ReturnResult>(f, std::move(content), ping, std::forward<CompletionToken>(token));
        }
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto reply_files(this auto&& self, discusy::upload_files_param files, std::string content = {}, bool ping = false, CompletionToken&& token = ctx::io_context::dct_t()) {
            return self.message.template reply_files<ReturnResult>(files, std::move(content), ping, std::forward<CompletionToken>(token));
        }
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto reply_with(this auto&& self, api::message::create_message msg, CompletionToken&& token = ctx::io_context::dct_t()) {
            return self.message.template reply_with<ReturnResult>(std::move(msg), std::forward<CompletionToken>(token));
        }
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto edit(this auto&& self, std::string content, CompletionToken&& token = ctx::io_context::dct_t()) {
            return self.message.template edit<ReturnResult>(std::move(content), std::forward<CompletionToken>(token));
        }
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto edit_embed(this auto&& self, discusy::message::embed e, CompletionToken&& token = ctx::io_context::dct_t()) {
            return self.message.template edit_embed<ReturnResult>(std::move(e), std::forward<CompletionToken>(token));
        }
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto edit_with(this auto&& self, api::message::edit_message msg, CompletionToken&& token = ctx::io_context::dct_t()) {
            return self.message.template edit_with<ReturnResult>(std::move(msg), std::forward<CompletionToken>(token));
        }
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto delete_message(this auto&& self, CompletionToken&& token = ctx::io_context::dct_t()) {
            return self.message.template delete_message<ReturnResult>(std::forward<CompletionToken>(token));
        }
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto add_reaction(this auto&& self, std::string emoji, CompletionToken&& token = ctx::io_context::dct_t()) {
            return self.message.template add_reaction<ReturnResult>(std::move(emoji), std::forward<CompletionToken>(token));
        }
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto remove_reaction(this auto&& self, std::string emoji, snowflake user_id = {}, CompletionToken&& token = ctx::io_context::dct_t()) {
            return self.message.template remove_reaction<ReturnResult>(std::move(emoji), user_id, std::forward<CompletionToken>(token));
        }
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto pin(this auto&& self, CompletionToken&& token = ctx::io_context::dct_t()) {
            return self.message.template pin<ReturnResult>(std::forward<CompletionToken>(token));
        }
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto unpin(this auto&& self, CompletionToken&& token = ctx::io_context::dct_t()) {
            return self.message.template unpin<ReturnResult>(std::forward<CompletionToken>(token));
        }
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto create_thread(this auto&& self, std::string name, CompletionToken&& token = ctx::io_context::dct_t()) {
            return self.message.template create_thread<ReturnResult>(std::move(name), std::forward<CompletionToken>(token));
        }

        [[nodiscard]] opt<snowflake> guild_id(this const auto& self) noexcept { return self.other.guild_id; }
    };

    using message_update = message_create;

    struct message_delete {
        snowflake id{};
        snowflake channel_id{};
        opt<snowflake> guild_id{};
    };

    struct message_delete_bulk {
        std::vector<snowflake> ids{};
        snowflake channel_id{};
        opt<snowflake> guild_id{};
    };

    enum class reaction_type : std::uint8_t {
        NORMAL = 0,
        BURST = 1,
    };

    struct message_reaction_add {
        snowflake user_id{};
        snowflake channel_id{};
        snowflake message_id{};
        opt<snowflake> guild_id{};
        opt<discusy::guild::guild_member> member{};
        discusy::emoji::emoji emoji{}; // partial, double check
        opt<snowflake> message_author_id{};
        bool burst{};
        opt<std::vector<std::string>> burst_colors{};
        reaction_type type{};
    };

    struct message_reaction_remove {
        snowflake user_id{};
        snowflake channel_id{};
        snowflake message_id{};
        opt<snowflake> guild_id{};
        discusy::emoji::emoji emoji{}; // partial, double check
        bool burst{};
        reaction_type type{};
    };

    struct message_reaction_remove_all {
        snowflake channel_id{};
        snowflake message_id{};
        opt<snowflake> guild_id{};
    };

    struct message_reaction_remove_emoji {
        snowflake channel_id{};
        snowflake message_id{};
        opt<snowflake> guild_id{};
        discusy::emoji::emoji emoji{};
    };
}
using namespace messages;

namespace voice {
    enum class animation_type : std::uint8_t {
        PREMIUM = 0, // A fun animation, sent by a Nitro subscriber
        BASIC = 1, // The standard animation
    };


    struct voice_channel_effect_send {
        snowflake channel_id{};
        snowflake guild_id{};
        snowflake user_id{};
        opt<discusy::emoji::emoji> emoji{};
        opt<discusy::recieve_event::voice::animation_type> animation_type{};
        opt<integer> animation_id{};
        opt<std::variant<snowflake, integer>> sound_id{};
        opt<double> sound_volume{};
    };

    using voice_state_update = discusy::voice::voice_state;

    struct voice_server_update {
        std::string token{};
        snowflake guild_id{};
        opt<std::string> endpoint{}; // Voice server host (null means the voice server allocated has gone away)
    };
}
using namespace voice;

namespace webhooks {
    struct webhooks_update {
        snowflake guild_id{};
        snowflake channel_id{};
    };
}
using namespace webhooks;

namespace interactions {
    using interaction_create = discusy::interaction::interaction;
}
using namespace interactions;

namespace stage_instances {
    using stage_instance_create = discusy::stage_instance::stage_instance;

    using stage_instance_update = discusy::stage_instance::stage_instance;

    using stage_instance_delete = discusy::stage_instance::stage_instance;
}
using namespace stage_instances;

namespace subscriptions {
    using subscription_create = discusy::subscription::subscription;

    using subscription_update = discusy::subscription::subscription;

    using subscription_delete = discusy::subscription::subscription;
}
using namespace subscriptions;

namespace polls {
    struct message_poll_vote_add {
        snowflake user_id{};
        snowflake channel_id{};
        snowflake message_id{};
        opt<snowflake> guild_id{};
        integer answer_id{};
    };

    struct message_poll_vote_remove {
        snowflake user_id{};
        snowflake channel_id{};
        snowflake message_id{};
        opt<snowflake> guild_id{};
        integer answer_id{};
    };
}
using namespace polls;

namespace rate_limits {
    struct rate_limit_metadata {
        snowflake guild_id{};
        opt<std::string> nonce{};
    };

    struct rate_limited {
        Opcode opcode{}; // opcode of the rate limited event
        float retry_after{};
        rate_limit_metadata meta{};
    };
}
using namespace rate_limits;
}




namespace send_event {

enum class status_type : std::uint8_t { // update glaze meta
    online,
    dnd,
    idle,
    invisible,
    offline,
};

template <Opcode code, typename T>
struct payload_base {
    Opcode op = code;
    T& d;

    payload_base(T& d_) : d{d_} {}

    struct glaze {
        using U = payload_base<code, T>;
        
        static constexpr auto value = glz::object(
            "op", &U::op,
            "d", [](auto&& self) -> auto& { return self.d; }
        );
    };
};

// for sharding required re-routing
struct rescue_payload {
    glz::generic d{};

    static rescue_payload create(glz::generic d_ = {}) {
        return rescue_payload{.d = std::move(d_)};
    }
    decltype(auto) set_d(this auto&& self, glz::generic d_) { self.d = std::move(d_); return std::forward<decltype(self)>(self); }
};

struct update_presence {
    // Unix time (in milliseconds) of when the client went idle, or null if the client is not idle
    explicit_null<std::int64_t> since{}; 
    // User's activities
    std::vector<recieve_event::presence::activity> activities{};
    // User's new status
    status_type status{status_type::online};
    // Whether or not the client is afk
    bool afk{false};

    static update_presence create(status_type status_ = status_type::online, bool afk_ = false) {
        return update_presence{.status = status_, .afk = afk_};
    }
    decltype(auto) set_since(this auto&& self, explicit_null<std::int64_t> s) noexcept { self.since = s; return std::forward<decltype(self)>(self); }
    decltype(auto) set_activities(this auto&& self, std::vector<recieve_event::presence::activity> acts) { self.activities = std::move(acts); return std::forward<decltype(self)>(self); }
    DISCUSY_VARIADIC_SETTER(set_activities, recieve_event::presence::activity)
    decltype(auto) add_activity(this auto&& self, recieve_event::presence::activity act) { self.activities.emplace_back(std::move(act)); return std::forward<decltype(self)>(self); }
    decltype(auto) set_status(this auto&& self, status_type st) noexcept { self.status = st; return std::forward<decltype(self)>(self); }
    decltype(auto) set_afk(this auto&& self, bool a) noexcept { self.afk = a; return std::forward<decltype(self)>(self); }
};

using update_presence_payload = payload_base<Opcode::PresenceUpdate, update_presence>;

struct identify_connection_properties {
    // Your operating system
    std::string os{
    #if defined(_WIN32) || defined(WIN32)
        "windows"
    #elifdef __linux__
        "linux"
    #elifdef __APPLE__
        "osx"
    #else
        "unknown"
    #endif
    };
    // Your library name
    std::string browser{"discusy"};
    // Your library name
    std::string device{"discusy"};

    static identify_connection_properties create(std::string os_ = {}, std::string browser_ = "discusy", std::string device_ = "discusy") {
        identify_connection_properties p{};
        if (!os_.empty()) p.os = std::move(os_);
        p.browser = std::move(browser_);
        p.device = std::move(device_);
        return p;
    }
    decltype(auto) set_os(this auto&& self, std::string o) { self.os = std::move(o); return std::forward<decltype(self)>(self); }
    decltype(auto) set_browser(this auto&& self, std::string b) { self.browser = std::move(b); return std::forward<decltype(self)>(self); }
    decltype(auto) set_device(this auto&& self, std::string d) { self.device = std::move(d); return std::forward<decltype(self)>(self); }
};

enum class capability : std::uint16_t {
    CHANNEL_OBFUSCATION = 1ULL << 15, // Opts the client into receiving obfuscated channel metadata over the Gateway for channels it can’t view
};

struct identify {
    // Authentication token
    std::string token{};
    // Connection properties
    identify_connection_properties properties{};
    // Whether this connection supports compression of packets
    // opt<bool> compress;
    // Value between 50 and 250, total number of members where the gateway will stop sending offline members in the guild member list
    // opt<integer> large_threshold;
    // Used for Guild Sharding
    opt<std::array<std::uint32_t, 2>> shard{};
    // Presence structure for initial presence information
    opt<update_presence> presence{};
    // Gateway Intents you wish to receive
    intent intents{};
    opt<capability> capabilities{};

    static identify create(std::string token_ = {}, intent intents_ = {}) {
        return identify{.token = std::move(token_), .intents = intents_};
    }
    decltype(auto) set_token(this auto&& self, std::string t) { self.token = std::move(t); return std::forward<decltype(self)>(self); }
    decltype(auto) set_properties(this auto&& self, identify_connection_properties p) { self.properties = std::move(p); return std::forward<decltype(self)>(self); }
    decltype(auto) set_shard(this auto&& self, opt<std::array<std::uint32_t, 2>> sh) noexcept { self.shard = sh; return std::forward<decltype(self)>(self); }
    decltype(auto) set_presence(this auto&& self, opt<update_presence> p) { self.presence = std::move(p); return std::forward<decltype(self)>(self); }
    decltype(auto) set_intents(this auto&& self, intent ints) noexcept { self.intents = ints; return std::forward<decltype(self)>(self); }
    decltype(auto) set_capabilities(this auto&& self, opt<capability> c) noexcept { self.capabilities = c; return std::forward<decltype(self)>(self); }
};

using identify_payload = payload_base<Opcode::Identify, identify>;

struct resume {
    // Session token
    std::string token{};
    // Session ID
    std::string session_id{};
    // Last sequence number received
    std::int64_t seq{};

    static resume create(std::string token_ = {}, std::string session_id_ = {}, std::int64_t seq_ = 0) {
        return resume{.token = std::move(token_), .session_id = std::move(session_id_), .seq = seq_};
    }
    decltype(auto) set_token(this auto&& self, std::string t) { self.token = std::move(t); return std::forward<decltype(self)>(self); }
    decltype(auto) set_session_id(this auto&& self, std::string sid) { self.session_id = std::move(sid); return std::forward<decltype(self)>(self); }
    decltype(auto) set_seq(this auto&& self, std::int64_t s) noexcept { self.seq = s; return std::forward<decltype(self)>(self); }
};

using resume_payload = payload_base<Opcode::Resume, resume>;

// Used to maintain an active gateway connection. Must be sent every heartbeat_interval milliseconds after the Opcode 10 Hello payload is received. The inner d key is the last sequence number—s—received by the client. If you have not yet received one, send null.
using heartbeat = opt<std::int64_t>;
using heartbeat_payload = payload_base<Opcode::Heartbeat, heartbeat>;

struct request_guild_members {
    // ID of the guild to get members for
    snowflake guild_id{};
    // string that username starts with, or an empty string to return all members
    opt<std::string> query{};
    // maximum number of members to send matching the query; a limit of 0 can be used with an empty string query to return all members
    std::int64_t limit{};
    // used to specify if we want the presences of the matched members
    opt<bool> presences{};
    // used to specify which users you wish to fetch
    opt<std::variant<snowflake, std::vector<snowflake>>> user_ids{};
    // nonce to identify the Guild Members Chunk response
    opt<std::string> nonce{};

    static request_guild_members create(snowflake gid = {}, std::string query_ = {}, std::int64_t limit_ = 0) {
        return request_guild_members{.guild_id = gid, .query = std::move(query_), .limit = limit_};
    }
    decltype(auto) set_guild_id(this auto&& self, snowflake gid) noexcept { self.guild_id = gid; return std::forward<decltype(self)>(self); }
    decltype(auto) set_guild(this auto&& self, snowflake gid) noexcept { return std::forward<decltype(self)>(self).set_guild_id(gid); }
    decltype(auto) set_query(this auto&& self, opt<std::string> q) { self.query = std::move(q); return std::forward<decltype(self)>(self); }
    decltype(auto) set_limit(this auto&& self, std::int64_t l) noexcept { self.limit = l; return std::forward<decltype(self)>(self); }
    decltype(auto) set_presences(this auto&& self, opt<bool> p) noexcept { self.presences = p; return std::forward<decltype(self)>(self); }
    decltype(auto) set_user_ids(this auto&& self, opt<std::variant<snowflake, std::vector<snowflake>>> uids) { self.user_ids = std::move(uids); return std::forward<decltype(self)>(self); }
    DISCUSY_VARIADIC_SETTER(set_user_ids, snowflake)
    decltype(auto) add_user_id(this auto&& self, snowflake uid) {
        if (!self.user_ids) {
            self.user_ids = make_vector(uid);
        } else if (std::holds_alternative<snowflake>(*self.user_ids)) {
            snowflake existing = std::get<snowflake>(*self.user_ids);
            self.user_ids = make_vector(existing, uid);
        } else {
            std::get<std::vector<snowflake>>(*self.user_ids).emplace_back(uid);
        }
        return std::forward<decltype(self)>(self);
    }
    decltype(auto) set_nonce(this auto&& self, opt<std::string> n) { self.nonce = std::move(n); return std::forward<decltype(self)>(self); }
};

using request_guild_members_payload = payload_base<Opcode::RequestGuildMembers, request_guild_members>;

struct request_soundboard_sounds {
    // IDs of the guilds to get soundboard sounds for
    std::vector<snowflake> guild_ids{};

    static request_soundboard_sounds create(std::vector<snowflake> gids = {}) {
        return request_soundboard_sounds{.guild_ids = std::move(gids)};
    }
    decltype(auto) set_guild_ids(this auto&& self, std::vector<snowflake> gids) { self.guild_ids = std::move(gids); return std::forward<decltype(self)>(self); }
    DISCUSY_VARIADIC_SETTER(set_guild_ids, snowflake)
    decltype(auto) add_guild_id(this auto&& self, snowflake gid) { self.guild_ids.emplace_back(gid); return std::forward<decltype(self)>(self); }
};

using request_soundboard_sounds_payload = payload_base<Opcode::RequestSoundboardSounds, request_soundboard_sounds>;

struct request_channel_info {
    // The guild id to request channel info for
    snowflake guild_id{};
    // The fields to request. The current available fields are status and voice_start_time.
    std::vector<std::string> fields{};

    static request_channel_info create(snowflake gid = {}, std::vector<std::string> fields_ = {}) {
        return request_channel_info{.guild_id = gid, .fields = std::move(fields_)};
    }
    decltype(auto) set_guild_id(this auto&& self, snowflake gid) noexcept { self.guild_id = gid; return std::forward<decltype(self)>(self); }
    decltype(auto) set_guild(this auto&& self, snowflake gid) noexcept { return std::forward<decltype(self)>(self).set_guild_id(gid); }
    decltype(auto) set_fields(this auto&& self, std::vector<std::string> f) { self.fields = std::move(f); return std::forward<decltype(self)>(self); }
    DISCUSY_VARIADIC_SETTER(set_fields, std::string)
    decltype(auto) add_field(this auto&& self, std::string f) { self.fields.emplace_back(std::move(f)); return std::forward<decltype(self)>(self); }
};

using request_channel_info_payload = payload_base<Opcode::RequestChannelInfo, request_channel_info>;

struct update_voice_state {
    // ID of the guild
    snowflake guild_id{};
    // ID of the voice channel client wants to join (null if disconnecting)
    explicit_null<snowflake> channel_id{};
    // Whether the client is muted
    bool self_mute{};
    // Whether the client deafened
    bool self_deaf{};

    static update_voice_state create(snowflake gid = {}, explicit_null<snowflake> cid = {}, bool mute_ = false, bool deaf_ = false) noexcept {
        return update_voice_state{.guild_id = gid, .channel_id = cid, .self_mute = mute_, .self_deaf = deaf_};
    }
    decltype(auto) set_guild_id(this auto&& self, snowflake gid) noexcept { self.guild_id = gid; return std::forward<decltype(self)>(self); }
    decltype(auto) set_guild(this auto&& self, snowflake gid) noexcept { return std::forward<decltype(self)>(self).set_guild_id(gid); }
    decltype(auto) set_channel_id(this auto&& self, explicit_null<snowflake> cid) noexcept { self.channel_id = cid; return std::forward<decltype(self)>(self); }
    decltype(auto) set_channel(this auto&& self, snowflake cid) noexcept { return std::forward<decltype(self)>(self).set_channel_id(cid); }
    decltype(auto) set_self_mute(this auto&& self, bool m) noexcept { self.self_mute = m; return std::forward<decltype(self)>(self); }
    decltype(auto) set_self_deaf(this auto&& self, bool d) noexcept { self.self_deaf = d; return std::forward<decltype(self)>(self); }
};

using update_voice_state_payload = payload_base<Opcode::VoiceStateUpdate, update_voice_state>;

}

}

template <>
struct glz::meta<discusy::recieve_event::event> {
    using enum discusy::recieve_event::event;
    static constexpr auto value = enumerate(
#pragma push_macro("DISCUSY_D")
#undef DISCUSY_D
#define DISCUSY_D ,
#define DISCUSY_X(name, _) name
        DISCORD_GATEWAY_EVENTS
#undef DISCUSY_X
#undef DISCUSY_D
#pragma pop_macro("DISCUSY_D")
    );
};

template <>
struct glz::meta<discusy::send_event::status_type> {
    using enum discusy::send_event::status_type;
    static constexpr auto value = glz::enumerate(
        online, dnd, idle, invisible, offline
    );
};

template <>
struct glz::meta<discusy::recieve_event::presence::status> {
    using enum discusy::recieve_event::presence::status;
    static constexpr auto value = glz::enumerate(
        online, dnd, idle, offline
    );
};

#ifdef __clang__
#pragma clang diagnostic pop
#endif

// DISCUSY_X, DISCUSY_D and DISCORD_GATEWAY_EVENTS not popped as they are popped in end_macros.hpp
