#pragma once

#include <string>
#include <variant>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <optional>
#include <utility>
#include <span>

#include <glaze/glaze.hpp>

#include "events.hpp"
#include "types.hpp"

namespace discusy::api {

struct error {
    std::uint32_t code{};
    std::optional<glz::raw_json_view> errors{};
    std::string message{};

    decltype(auto) set_code(this auto&& self, std::uint32_t c) noexcept { self.code = c; return std::forward<decltype(self)>(self); }
    decltype(auto) set_errors(this auto&& self, std::optional<glz::raw_json_view> errs) noexcept { self.errors = errs; return std::forward<decltype(self)>(self); }
    decltype(auto) set_message(this auto&& self, std::string msg) { self.message = std::move(msg); return std::forward<decltype(self)>(self); }
};

struct error_info {
    std::string message{};
    std::uint32_t code{0};
    int http_status{0};
    std::optional<glz::raw_json_view> errors{};

    decltype(auto) set_message(this auto&& self, std::string msg) { self.message = std::move(msg); return std::forward<decltype(self)>(self); }
    decltype(auto) set_code(this auto&& self, std::uint32_t c) noexcept { self.code = c; return std::forward<decltype(self)>(self); }
    decltype(auto) set_http_status(this auto&& self, int status) noexcept { self.http_status = status; return std::forward<decltype(self)>(self); }
    decltype(auto) set_errors(this auto&& self, std::optional<glz::raw_json_view> errs) noexcept { self.errors = errs; return std::forward<decltype(self)>(self); }
};

template <typename T>
struct result {
    std::optional<T> data{};
    std::optional<api::error_info> error_info_{};

    [[nodiscard]] constexpr bool has_value() const noexcept { return data.has_value(); }
    [[nodiscard]] constexpr explicit operator bool() const noexcept { return has_value(); }

    [[nodiscard]] constexpr decltype(auto) value(this auto&& self) { return std::forward<decltype(self)>(self).data.value(); }

    [[nodiscard]] constexpr auto* operator->(this auto&& self) noexcept { return std::addressof(*self.data); }

    [[nodiscard]] constexpr decltype(auto) operator*(this auto&& self) noexcept { return *std::forward<decltype(self)>(self).data; }

    [[nodiscard]] const std::string& error() const noexcept { 
        static const std::string empty;
        return error_info_ ? error_info_->message : empty; 
    }

    [[nodiscard]] constexpr decltype(auto) error_info(this auto&& self) noexcept { return (std::forward<decltype(self)>(self).error_info_); }
    [[nodiscard]] std::string_view error_message() const noexcept { 
        return error_info_ ? std::string_view{error_info_->message} : std::string_view{}; 
    }
};

template <>
struct result<void> {
    bool success{false};
    std::optional<api::error_info> error_info_{};

    [[nodiscard]] constexpr bool has_value() const noexcept { return success; }
    [[nodiscard]] constexpr explicit operator bool() const noexcept { return success; }

    [[nodiscard]] const std::string& error() const noexcept { 
        static const std::string empty;
        return error_info_ ? error_info_->message : empty; 
    }
    [[nodiscard]] const std::optional<api::error_info>& error_info() const noexcept { return error_info_; }
    [[nodiscard]] std::string_view error_message() const noexcept { 
        return error_info_ ? std::string_view{error_info_->message} : std::string_view{}; 
    }
};

struct session_start_limit {
    std::uint16_t total{};
    std::uint16_t remaining{};
    std::uint64_t reset_after{};
    std::uint8_t max_concurrency{};

    static session_start_limit create(std::uint16_t total_ = 0, std::uint16_t remaining_ = 0) noexcept {
        return session_start_limit{.total = total_, .remaining = remaining_};
    }
    decltype(auto) set_total(this auto&& self, std::uint16_t t) noexcept { self.total = t; return std::forward<decltype(self)>(self); }
    decltype(auto) set_remaining(this auto&& self, std::uint16_t r) noexcept { self.remaining = r; return std::forward<decltype(self)>(self); }
    decltype(auto) set_reset_after(this auto&& self, std::uint64_t ra) noexcept { self.reset_after = ra; return std::forward<decltype(self)>(self); }
    decltype(auto) set_max_concurrency(this auto&& self, std::uint8_t mc) noexcept { self.max_concurrency = mc; return std::forward<decltype(self)>(self); }
};

struct get_gateway_bot {
    std::string url{};
    std::uint32_t shards{};
    api::session_start_limit session_start_limit{};

    static get_gateway_bot create(std::string url_ = {}, std::uint32_t shards_ = 1) {
        return get_gateway_bot{.url = std::move(url_), .shards = shards_};
    }
    decltype(auto) set_url(this auto&& self, std::string u) { self.url = std::move(u); return std::forward<decltype(self)>(self); }
    decltype(auto) set_shards(this auto&& self, std::uint32_t s) noexcept { self.shards = s; return std::forward<decltype(self)>(self); }
    decltype(auto) set_session_start_limit(this auto&& self, api::session_start_limit ssl) noexcept { self.session_start_limit = ssl; return std::forward<decltype(self)>(self); }
};

namespace application {
    struct edit_current_application {
        opt<std::string> custom_install_url{};
        opt<std::string> description{};
        opt<std::string> role_connections_verification_url{};
        opt<discusy::application::install_params> install_params{};
        opt<std::unordered_map<discusy::application::application_integration_type, discusy::application::integration_type_config>> integration_types_config{};
        opt<discusy::application::application_flags> flags{};
        opt<explicit_null<image_data>> icon{};
        opt<explicit_null<image_data>> cover_image{};
        opt<std::string> interactions_endpoint_url{};
        opt<std::vector<std::string>> tags{};
        opt<std::string> event_webhooks_url{};
        opt<discusy::application::application_event_webhook_status> event_webhooks_status{};
        opt<std::vector<std::string>> event_webhooks_types{};

        static edit_current_application create() noexcept {
            return edit_current_application{};
        }
        decltype(auto) set_custom_install_url(this auto&& self, opt<std::string> u) { self.custom_install_url = std::move(u); return std::forward<decltype(self)>(self); }
        decltype(auto) set_description(this auto&& self, opt<std::string> d) { self.description = std::move(d); return std::forward<decltype(self)>(self); }
        decltype(auto) set_role_connections_verification_url(this auto&& self, opt<std::string> u) { self.role_connections_verification_url = std::move(u); return std::forward<decltype(self)>(self); }
        decltype(auto) set_install_params(this auto&& self, opt<discusy::application::install_params> ip) { self.install_params = std::move(ip); return std::forward<decltype(self)>(self); }
        decltype(auto) set_integration_types_config(this auto&& self, opt<std::unordered_map<discusy::application::application_integration_type, discusy::application::integration_type_config>> itc) { self.integration_types_config = std::move(itc); return std::forward<decltype(self)>(self); }
        decltype(auto) set_flags(this auto&& self, opt<discusy::application::application_flags> f) noexcept { self.flags = f; return std::forward<decltype(self)>(self); }
        decltype(auto) set_icon(this auto&& self, opt<explicit_null<image_data>> ic) { self.icon = std::move(ic); return std::forward<decltype(self)>(self); }
        decltype(auto) set_cover_image(this auto&& self, opt<explicit_null<image_data>> ci) { self.cover_image = std::move(ci); return std::forward<decltype(self)>(self); }
        decltype(auto) set_interactions_endpoint_url(this auto&& self, opt<std::string> u) { self.interactions_endpoint_url = std::move(u); return std::forward<decltype(self)>(self); }
        decltype(auto) set_tags(this auto&& self, opt<std::vector<std::string>> t) { self.tags = std::move(t); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_tags, std::string)
        decltype(auto) add_tag(this auto&& self, std::string tag) {
            if (!self.tags) self.tags.emplace();
            self.tags->emplace_back(std::move(tag));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_event_webhooks_url(this auto&& self, opt<std::string> u) { self.event_webhooks_url = std::move(u); return std::forward<decltype(self)>(self); }
        decltype(auto) set_event_webhooks_status(this auto&& self, opt<discusy::application::application_event_webhook_status> s) noexcept { self.event_webhooks_status = s; return std::forward<decltype(self)>(self); }
        decltype(auto) set_event_webhooks_types(this auto&& self, opt<std::vector<std::string>> t) { self.event_webhooks_types = std::move(t); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_event_webhooks_types, std::string)
        decltype(auto) add_event_webhooks_type(this auto&& self, std::string type) {
            if (!self.event_webhooks_types) self.event_webhooks_types.emplace();
            self.event_webhooks_types->emplace_back(std::move(type));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) add_event_webhook_type(this auto&& self, std::string type) {
            return std::forward<decltype(self)>(self).add_event_webhooks_type(std::move(type));
        }
    };

    enum class activity_location_kind : std::uint8_t { // has glz::meta
        guild_channel, private_channel,
    };

    struct activity_location {
        std::string id{};
        activity_location_kind kind{};
        snowflake channel_id{};
        opt<snowflake> guild_id{};

        static activity_location create(std::string id_ = {}, activity_location_kind kind_ = activity_location_kind::guild_channel, snowflake cid = {}) {
            return activity_location{.id = std::move(id_), .kind = kind_, .channel_id = cid};
        }
        decltype(auto) set_id(this auto&& self, std::string id_) { self.id = std::move(id_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_kind(this auto&& self, activity_location_kind k) noexcept { self.kind = k; return std::forward<decltype(self)>(self); }
        decltype(auto) set_channel_id(this auto&& self, snowflake cid) noexcept { self.channel_id = cid; return std::forward<decltype(self)>(self); }
        decltype(auto) set_channel(this auto&& self, snowflake cid) noexcept { return std::forward<decltype(self)>(self).set_channel_id(cid); }
        decltype(auto) set_guild_id(this auto&& self, opt<snowflake> gid) noexcept { self.guild_id = gid; return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild(this auto&& self, snowflake gid) noexcept { return std::forward<decltype(self)>(self).set_guild_id(gid); }
    };

    struct activity_instance {
        snowflake application_id{};
        std::string instance_id{};
        snowflake launch_id{};
        activity_location location{};
        std::vector<snowflake> users{};

        static activity_instance create(snowflake aid = {}, std::string inst_id = {}, snowflake lid = {}) {
            return activity_instance{.application_id = aid, .instance_id = std::move(inst_id), .launch_id = lid};
        }
        decltype(auto) set_application_id(this auto&& self, snowflake aid) noexcept { self.application_id = aid; return std::forward<decltype(self)>(self); }
        decltype(auto) set_instance_id(this auto&& self, std::string iid) { self.instance_id = std::move(iid); return std::forward<decltype(self)>(self); }
        decltype(auto) set_launch_id(this auto&& self, snowflake lid) noexcept { self.launch_id = lid; return std::forward<decltype(self)>(self); }
        decltype(auto) set_location(this auto&& self, activity_location loc) { self.location = std::move(loc); return std::forward<decltype(self)>(self); }
        decltype(auto) set_users(this auto&& self, std::vector<snowflake> us) { self.users = std::move(us); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_users, snowflake)
        decltype(auto) add_user(this auto&& self, snowflake uid) { self.users.emplace_back(uid); return std::forward<decltype(self)>(self); }
    };
}

namespace audit_log {
    struct get_guild_audit_log_query_params {
        opt<snowflake> user_id{};
        opt<discusy::audit_log::audit_log_event> action_type{};
        opt<snowflake> before{};
        opt<snowflake> after{};
        opt<std::uint8_t> limit{};

        static get_guild_audit_log_query_params create() noexcept {
            return get_guild_audit_log_query_params{};
        }
        decltype(auto) set_user_id(this auto&& self, opt<snowflake> uid) noexcept { self.user_id = uid; return std::forward<decltype(self)>(self); }
        decltype(auto) set_action_type(this auto&& self, opt<discusy::audit_log::audit_log_event> at) noexcept { self.action_type = at; return std::forward<decltype(self)>(self); }
        decltype(auto) set_before(this auto&& self, opt<snowflake> b) noexcept { self.before = b; return std::forward<decltype(self)>(self); }
        decltype(auto) set_after(this auto&& self, opt<snowflake> a) noexcept { self.after = a; return std::forward<decltype(self)>(self); }
        decltype(auto) set_limit(this auto&& self, opt<std::uint8_t> l) noexcept { self.limit = l; return std::forward<decltype(self)>(self); }
    };
}

namespace auto_moderation {
    struct create_auto_moderation_rule {
        std::string name{};
        discusy::auto_moderation::auto_moderation_rule_event_type event_type{};
        discusy::auto_moderation::auto_moderation_rule_trigger_type trigger_type{};
        opt<discusy::auto_moderation::auto_moderation_trigger_metadata> trigger_metadata{};
        std::vector<discusy::auto_moderation::auto_moderation_action> actions{};
        opt<bool> enabled{false};
        opt<std::vector<snowflake>> exempt_roles{};
        opt<std::vector<snowflake>> exempt_channels{};

        static create_auto_moderation_rule create(std::string name_ = {}, discusy::auto_moderation::auto_moderation_rule_event_type event_type_ = discusy::auto_moderation::auto_moderation_rule_event_type::MESSAGE_SEND, discusy::auto_moderation::auto_moderation_rule_trigger_type trigger_type_ = discusy::auto_moderation::auto_moderation_rule_trigger_type::KEYWORD) {
            return create_auto_moderation_rule{.name = std::move(name_), .event_type = event_type_, .trigger_type = trigger_type_};
        }
        template <typename From>
        static create_auto_moderation_rule from(From&& r) {
            DISCUSY_FORWARD_IF_SAME(create_auto_moderation_rule, r);

            static_assert(
                DISCUSY_HAS_FIELD(From, name) ||
                DISCUSY_HAS_FIELD(From, event_type) ||
                DISCUSY_HAS_FIELD(From, trigger_type) ||
                DISCUSY_HAS_FIELD(From, trigger_metadata) ||
                DISCUSY_HAS_FIELD(From, actions) ||
                DISCUSY_HAS_FIELD(From, enabled) ||
                DISCUSY_HAS_FIELD(From, exempt_roles) ||
                DISCUSY_HAS_FIELD(From, exempt_channels),
                "create_auto_moderation_rule::from: Source type contains no compatible auto-moderation fields."
            );

            create_auto_moderation_rule res{};
            DISCUSY_TRANSFER_FIELD(res, r, name);
            DISCUSY_TRANSFER_FIELD(res, r, event_type);
            DISCUSY_TRANSFER_FIELD(res, r, trigger_type);
            DISCUSY_TRANSFER_FIELD(res, r, trigger_metadata);
            DISCUSY_TRANSFER_FIELD(res, r, actions);
            DISCUSY_TRANSFER_FIELD(res, r, enabled);
            DISCUSY_TRANSFER_FIELD(res, r, exempt_roles);
            DISCUSY_TRANSFER_FIELD(res, r, exempt_channels);
            return res;

            DISCUSY_END_FROM
        }
        decltype(auto) set_name(this auto&& self, std::string n) { self.name = std::move(n); return std::forward<decltype(self)>(self); }
        decltype(auto) set_event_type(this auto&& self, discusy::auto_moderation::auto_moderation_rule_event_type et) noexcept { self.event_type = et; return std::forward<decltype(self)>(self); }
        decltype(auto) set_trigger_type(this auto&& self, discusy::auto_moderation::auto_moderation_rule_trigger_type tt) noexcept { self.trigger_type = tt; return std::forward<decltype(self)>(self); }
        decltype(auto) set_trigger_metadata(this auto&& self, opt<discusy::auto_moderation::auto_moderation_trigger_metadata> tm) { self.trigger_metadata = std::move(tm); return std::forward<decltype(self)>(self); }
        decltype(auto) set_actions(this auto&& self, std::vector<discusy::auto_moderation::auto_moderation_action> acts) { self.actions = std::move(acts); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_actions, discusy::auto_moderation::auto_moderation_action)
        decltype(auto) add_action(this auto&& self, discusy::auto_moderation::auto_moderation_action act) { self.actions.emplace_back(std::move(act)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_enabled(this auto&& self, opt<bool> en) noexcept { self.enabled = en; return std::forward<decltype(self)>(self); }
        decltype(auto) set_exempt_roles(this auto&& self, opt<std::vector<snowflake>> er) { self.exempt_roles = std::move(er); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_exempt_roles, snowflake)
        decltype(auto) add_exempt_role(this auto&& self, snowflake rid) {
            if (!self.exempt_roles) self.exempt_roles.emplace();
            self.exempt_roles->emplace_back(rid);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_exempt_channels(this auto&& self, opt<std::vector<snowflake>> ec) { self.exempt_channels = std::move(ec); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_exempt_channels, snowflake)
        decltype(auto) add_exempt_channel(this auto&& self, snowflake cid) {
            if (!self.exempt_channels) self.exempt_channels.emplace();
            self.exempt_channels->emplace_back(cid);
            return std::forward<decltype(self)>(self);
        }
    };

    struct modify_auto_moderation_rule {
        opt<std::string> name{};
        opt<discusy::auto_moderation::auto_moderation_rule_event_type> event_type{};
        opt<discusy::auto_moderation::auto_moderation_trigger_metadata> trigger_metadata{};
        opt<std::vector<discusy::auto_moderation::auto_moderation_action>> actions{};
        opt<bool> enabled{};
        opt<std::vector<snowflake>> exempt_roles{};
        opt<std::vector<snowflake>> exempt_channels{};

        static modify_auto_moderation_rule create() noexcept {
            return modify_auto_moderation_rule{};
        }
        template <typename From>
        static modify_auto_moderation_rule from(From&& r) {
            DISCUSY_FORWARD_IF_SAME(modify_auto_moderation_rule, r);

            static_assert(
                DISCUSY_HAS_FIELD(From, name) ||
                DISCUSY_HAS_FIELD(From, event_type) ||
                DISCUSY_HAS_FIELD(From, trigger_metadata) ||
                DISCUSY_HAS_FIELD(From, actions) ||
                DISCUSY_HAS_FIELD(From, enabled) ||
                DISCUSY_HAS_FIELD(From, exempt_roles) ||
                DISCUSY_HAS_FIELD(From, exempt_channels),
                "modify_auto_moderation_rule::from: Source type contains no compatible auto-moderation fields."
            );

            modify_auto_moderation_rule res{};
            DISCUSY_TRANSFER_FIELD(res, r, name);
            DISCUSY_TRANSFER_FIELD(res, r, event_type);
            DISCUSY_TRANSFER_FIELD(res, r, trigger_metadata);
            DISCUSY_TRANSFER_FIELD(res, r, actions);
            DISCUSY_TRANSFER_FIELD(res, r, enabled);
            DISCUSY_TRANSFER_FIELD(res, r, exempt_roles);
            DISCUSY_TRANSFER_FIELD(res, r, exempt_channels);
            return res;

            DISCUSY_END_FROM
        }
        decltype(auto) set_name(this auto&& self, opt<std::string> n) { self.name = std::move(n); return std::forward<decltype(self)>(self); }
        decltype(auto) set_event_type(this auto&& self, opt<discusy::auto_moderation::auto_moderation_rule_event_type> et) noexcept { self.event_type = et; return std::forward<decltype(self)>(self); }
        decltype(auto) set_trigger_metadata(this auto&& self, opt<discusy::auto_moderation::auto_moderation_trigger_metadata> tm) { self.trigger_metadata = std::move(tm); return std::forward<decltype(self)>(self); }
        decltype(auto) set_actions(this auto&& self, opt<std::vector<discusy::auto_moderation::auto_moderation_action>> acts) { self.actions = std::move(acts); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_actions, discusy::auto_moderation::auto_moderation_action)
        decltype(auto) add_action(this auto&& self, discusy::auto_moderation::auto_moderation_action act) {
            if (!self.actions) self.actions.emplace();
            self.actions->emplace_back(std::move(act));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_enabled(this auto&& self, opt<bool> en) noexcept { self.enabled = en; return std::forward<decltype(self)>(self); }
        decltype(auto) set_exempt_roles(this auto&& self, opt<std::vector<snowflake>> er) { self.exempt_roles = std::move(er); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_exempt_roles, snowflake)
        decltype(auto) add_exempt_role(this auto&& self, snowflake rid) {
            if (!self.exempt_roles) self.exempt_roles.emplace();
            self.exempt_roles->emplace_back(rid);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_exempt_channels(this auto&& self, opt<std::vector<snowflake>> ec) { self.exempt_channels = std::move(ec); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_exempt_channels, snowflake)
        decltype(auto) add_exempt_channel(this auto&& self, snowflake cid) {
            if (!self.exempt_channels) self.exempt_channels.emplace();
            self.exempt_channels->emplace_back(cid);
            return std::forward<decltype(self)>(self);
        }
    };
}

namespace message {
    enum class allowed_mentions : std::uint8_t { // has glz meta
        roles, users, everyone,
    };

    struct allowed_mentions_obj {
        opt<std::unordered_set<allowed_mentions>> parse{};
        opt<std::vector<snowflake>> roles{}; // max 100
        opt<std::vector<snowflake>> users{}; // max 100
        opt<bool> replied_user{};

        static allowed_mentions_obj create(opt<bool> replied_user_ = {}) noexcept {
            return allowed_mentions_obj{.replied_user = replied_user_};
        }
        decltype(auto) set_parse(this auto&& self, opt<std::unordered_set<allowed_mentions>> p) { self.parse = std::move(p); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER_SET(set_parse, allowed_mentions)
        decltype(auto) add_parse(this auto&& self, allowed_mentions m) {
            if (!self.parse) self.parse.emplace();
            self.parse->emplace(m);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_roles(this auto&& self, opt<std::vector<snowflake>> r) { self.roles = std::move(r); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_roles, snowflake)
        decltype(auto) add_role(this auto&& self, snowflake rid) {
            if (!self.roles) self.roles.emplace();
            self.roles->emplace_back(rid);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_users(this auto&& self, opt<std::vector<snowflake>> u) { self.users = std::move(u); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_users, snowflake)
        decltype(auto) add_user(this auto&& self, snowflake uid) {
            if (!self.users) self.users.emplace();
            self.users->emplace_back(uid);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_replied_user(this auto&& self, opt<bool> ru) noexcept { self.replied_user = ru; return std::forward<decltype(self)>(self); }
    };

    struct attachment_request {
        snowflake id{};
        opt<std::string> filename{};
        opt<std::string> title{};
        opt<std::string> description{};
        opt<float> duration_secs{};
        opt<std::string> waveform{}; // base64 encoded bytearray representing a sampled waveform (currently for voice messages)
        opt<bool> is_spoiler{};

        static attachment_request create(snowflake id_ = {}, opt<std::string> filename_ = {}) {
            return attachment_request{.id = id_, .filename = std::move(filename_)};
        }
        template <typename From>
        static attachment_request from(From&& att) {
            DISCUSY_FORWARD_IF_SAME(attachment_request, att);

            static_assert(
                DISCUSY_HAS_FIELD(From, id) ||
                DISCUSY_HAS_FIELD(From, filename) ||
                DISCUSY_HAS_FIELD(From, title) ||
                DISCUSY_HAS_FIELD(From, duration_secs) ||
                DISCUSY_HAS_FIELD(From, waveform),
                "attachment_request::from: Source type contains no compatible attachment fields."
            );

            attachment_request res{};
            DISCUSY_TRANSFER_FIELD(res, att, id);
            DISCUSY_TRANSFER_FIELD(res, att, filename);
            DISCUSY_TRANSFER_FIELD(res, att, title);
            DISCUSY_TRANSFER_FIELD(res, att, duration_secs);
            DISCUSY_TRANSFER_FIELD(res, att, waveform);
            return res;

            DISCUSY_END_FROM
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_filename(this auto&& self, opt<std::string> fn) { self.filename = std::move(fn); return std::forward<decltype(self)>(self); }
        decltype(auto) set_title(this auto&& self, opt<std::string> t) { self.title = std::move(t); return std::forward<decltype(self)>(self); }
        decltype(auto) set_description(this auto&& self, opt<std::string> d) { self.description = std::move(d); return std::forward<decltype(self)>(self); }
        decltype(auto) set_duration_secs(this auto&& self, opt<float> ds) noexcept { self.duration_secs = ds; return std::forward<decltype(self)>(self); }
        decltype(auto) set_waveform(this auto&& self, opt<std::string> wf) { self.waveform = std::move(wf); return std::forward<decltype(self)>(self); }
        decltype(auto) set_is_spoiler(this auto&& self, opt<bool> is) noexcept { self.is_spoiler = is; return std::forward<decltype(self)>(self); }
    };

    struct create_message {
        opt<std::string> content{};
        opt<std::variant<integer, std::string>> nonce{};
        opt<bool> tts{};
        opt<std::vector<discusy::message::embed>> embeds{};
        opt<allowed_mentions_obj> allowed_mentions{};
        opt<discusy::message::message_reference> message_reference{};
        opt<std::vector<discusy::components::component>> components{};
        opt<std::vector<snowflake>> sticker_ids{};
        opt<std::vector<attachment_request>> attachments{};
        opt<flags_t<discusy::message::message_flags>> flags{};
        opt<bool> enforce_nonce{};
        opt<discusy::poll::poll_create_request> poll{};
        opt<discusy::message::shared_client_theme> shared_client_theme{};

        static create_message create(opt<std::string> content_ = {}) {
            return create_message{.content = std::move(content_)};
        }

        template <typename From>
        static create_message from(From&& msg) {
            DISCUSY_FORWARD_IF_SAME(create_message, msg);

            static_assert(
                DISCUSY_HAS_FIELD(From, content) ||
                DISCUSY_HAS_FIELD(From, tts) ||
                DISCUSY_HAS_FIELD(From, embeds) ||
                DISCUSY_HAS_FIELD(From, flags) ||
                DISCUSY_HAS_FIELD(From, components) ||
                DISCUSY_HAS_FIELD(From, message_reference) ||
                DISCUSY_HAS_FIELD(From, shared_client_theme),
                "create_message::from: Source type contains no compatible message fields."
            );

            create_message req{};
            DISCUSY_TRANSFER_FIELD(req, msg, content);
            DISCUSY_TRANSFER_FIELD(req, msg, tts);
            DISCUSY_TRANSFER_FIELD(req, msg, embeds);
            DISCUSY_TRANSFER_FIELD(req, msg, flags);
            DISCUSY_TRANSFER_FIELD(req, msg, components);
            DISCUSY_TRANSFER_FIELD(req, msg, message_reference);
            DISCUSY_TRANSFER_FIELD(req, msg, shared_client_theme);
            return req;

            DISCUSY_END_FROM
        }

        static create_message reply_to(const discusy::message::message& msg, std::string content_ = {}, bool ping = false) {
            create_message req{};
            if (!content_.empty()) req.content = std::move(content_);
            req.message_reference = discusy::message::message_reference{
                .message_id = msg.id,
                // .channel_id = msg.channel_id, // TODO: check
                // .guild_id = msg.guild_id
            };
            if (!ping) {
                req.allowed_mentions = allowed_mentions_obj{.replied_user = false};
            }
            return req;
        }

        decltype(auto) set_content(this auto&& self, opt<std::string> c) { self.content = std::move(c); return std::forward<decltype(self)>(self); }
        decltype(auto) set_nonce(this auto&& self, opt<std::variant<integer, std::string>> n) { self.nonce = std::move(n); return std::forward<decltype(self)>(self); }
        decltype(auto) set_tts(this auto&& self, opt<bool> t) noexcept { self.tts = t; return std::forward<decltype(self)>(self); }
        decltype(auto) set_embeds(this auto&& self, opt<std::vector<discusy::message::embed>> embs) { self.embeds = std::move(embs); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_embeds, discusy::message::embed)
        decltype(auto) add_embed(this auto&& self, discusy::message::embed emb) {
            if (!self.embeds) self.embeds.emplace();
            self.embeds->emplace_back(std::move(emb));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_allowed_mentions(this auto&& self, opt<allowed_mentions_obj> am) { self.allowed_mentions = std::move(am); return std::forward<decltype(self)>(self); }
        decltype(auto) set_message_reference(this auto&& self, opt<discusy::message::message_reference> mr) noexcept { self.message_reference = mr; return std::forward<decltype(self)>(self); }
        decltype(auto) set_components(this auto&& self, opt<std::vector<discusy::components::component>> comps) {
            self.components = std::move(comps);
            if (self.components && !self.components->empty()) {
                self.add_flags(discusy::message::message_flags::IS_COMPONENTS_V2);
            }
            return std::forward<decltype(self)>(self);
        }
        DISCUSY_VARIADIC_SETTER(set_components, discusy::components::component)
        decltype(auto) add_component(this auto&& self, discusy::components::component comp) {
            if (!self.components) self.components.emplace();
            self.components->emplace_back(std::move(comp));
            self.add_flags(discusy::message::message_flags::IS_COMPONENTS_V2);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) add_action_row(this auto&& self, discusy::components::ActionRow row) {
            return std::forward<decltype(self)>(self).add_component(std::move(row));
        }
        decltype(auto) add_container(this auto&& self, discusy::components::Container container) {
            return std::forward<decltype(self)>(self).add_component(std::move(container));
        }
        decltype(auto) add_section(this auto&& self, discusy::components::Section section) {
            return std::forward<decltype(self)>(self).add_component(std::move(section));
        }
        decltype(auto) add_text_display(this auto&& self, discusy::components::TextDisplay td) {
            return std::forward<decltype(self)>(self).add_component(std::move(td));
        }
        decltype(auto) add_text_display(this auto&& self, std::string text) {
            return std::forward<decltype(self)>(self).add_component(discusy::components::TextDisplay::create(std::move(text)));
        }
        decltype(auto) add_media_gallery(this auto&& self, discusy::components::MediaGallery mg) {
            return std::forward<decltype(self)>(self).add_component(std::move(mg));
        }
        decltype(auto) add_file(this auto&& self, discusy::components::File f) {
            return std::forward<decltype(self)>(self).add_component(std::move(f));
        }
        decltype(auto) add_separator(this auto&& self, discusy::components::Separator sep = discusy::components::Separator::create()) {
            return std::forward<decltype(self)>(self).add_component(std::move(sep));
        }
        decltype(auto) set_sticker_ids(this auto&& self, opt<std::vector<snowflake>> sids) { self.sticker_ids = std::move(sids); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_sticker_ids, snowflake)
        decltype(auto) add_sticker_id(this auto&& self, snowflake sid) {
            if (!self.sticker_ids) self.sticker_ids.emplace();
            self.sticker_ids->emplace_back(sid);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_attachments(this auto&& self, opt<std::vector<attachment_request>> atts) { self.attachments = std::move(atts); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_attachments, attachment_request)
        decltype(auto) add_attachment(this auto&& self, attachment_request att) {
            if (!self.attachments) self.attachments.emplace();
            self.attachments->emplace_back(std::move(att));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_flags(this auto&& self, opt<flags_t<discusy::message::message_flags>> f) noexcept { self.flags = f; return std::forward<decltype(self)>(self); }
        decltype(auto) add_flags(this auto&& self, discusy::message::message_flags f) noexcept {
            if (!self.flags) self.flags.emplace();
            self.flags->add_flags(f);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) add_flag(this auto&& self, discusy::message::message_flags f) noexcept { return std::forward<decltype(self)>(self).add_flags(f); }
        decltype(auto) set_enforce_nonce(this auto&& self, opt<bool> en) noexcept { self.enforce_nonce = en; return std::forward<decltype(self)>(self); }
        decltype(auto) set_poll(this auto&& self, opt<discusy::poll::poll_create_request> p) { self.poll = std::move(p); return std::forward<decltype(self)>(self); }
        decltype(auto) set_shared_client_theme(this auto&& self, opt<discusy::message::shared_client_theme> sct) noexcept { self.shared_client_theme = sct; return std::forward<decltype(self)>(self); }
    };

    struct get_channel_messages_query {
        opt<snowflake> around{};
        opt<snowflake> before{};
        opt<snowflake> after{};
        opt<integer> limit{};

        static get_channel_messages_query create() noexcept {
            return get_channel_messages_query{};
        }
        decltype(auto) set_around(this auto&& self, opt<snowflake> a) noexcept { self.around = a; return std::forward<decltype(self)>(self); }
        decltype(auto) set_before(this auto&& self, opt<snowflake> b) noexcept { self.before = b; return std::forward<decltype(self)>(self); }
        decltype(auto) set_after(this auto&& self, opt<snowflake> a) noexcept { self.after = a; return std::forward<decltype(self)>(self); }
        decltype(auto) set_limit(this auto&& self, opt<integer> l) noexcept { self.limit = l; return std::forward<decltype(self)>(self); }
    };

    struct search_guild_messages_query {
        opt<integer> limit{};
        opt<integer> offset{};
        opt<snowflake> max_id{};
        opt<snowflake> min_id{};
        opt<integer> slop{};
        opt<std::string> content{};
        opt<std::vector<snowflake>> channel_id{};
        opt<std::vector<std::string>> author_type{};
        opt<std::vector<snowflake>> author_id{};
        opt<std::vector<snowflake>> mentions{};
        opt<std::vector<snowflake>> mentions_role_id{};
        opt<bool> mention_everyone{};
        opt<std::vector<snowflake>> replied_to_user_id{};
        opt<std::vector<snowflake>> replied_to_message_id{};
        opt<bool> pinned{};
        opt<std::vector<std::string>> has{};
        opt<std::vector<std::string>> embed_type{};
        opt<std::vector<std::string>> embed_provider{};
        opt<std::vector<std::string>> link_hostname{};
        opt<std::vector<std::string>> attachment_filename{};
        opt<std::vector<std::string>> attachment_extension{};
        opt<std::string> sort_by{};
        opt<std::string> sort_order{};
        opt<bool> include_nsfw{};

        static search_guild_messages_query create() noexcept {
            return search_guild_messages_query{};
        }
        decltype(auto) set_limit(this auto&& self, opt<integer> l) noexcept { self.limit = l; return std::forward<decltype(self)>(self); }
        decltype(auto) set_offset(this auto&& self, opt<integer> o) noexcept { self.offset = o; return std::forward<decltype(self)>(self); }
        decltype(auto) set_max_id(this auto&& self, opt<snowflake> m) noexcept { self.max_id = m; return std::forward<decltype(self)>(self); }
        decltype(auto) set_min_id(this auto&& self, opt<snowflake> m) noexcept { self.min_id = m; return std::forward<decltype(self)>(self); }
        decltype(auto) set_slop(this auto&& self, opt<integer> s) noexcept { self.slop = s; return std::forward<decltype(self)>(self); }
        decltype(auto) set_content(this auto&& self, opt<std::string> c) { self.content = std::move(c); return std::forward<decltype(self)>(self); }
        decltype(auto) set_channel_id(this auto&& self, opt<std::vector<snowflake>> cid) { self.channel_id = std::move(cid); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_channel_id, snowflake)
        decltype(auto) add_channel_id(this auto&& self, snowflake cid) {
            if (!self.channel_id) self.channel_id.emplace();
            self.channel_id->emplace_back(cid);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_author_type(this auto&& self, opt<std::vector<std::string>> at) { self.author_type = std::move(at); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_author_type, std::string)
        decltype(auto) add_author_type(this auto&& self, std::string at) {
            if (!self.author_type) self.author_type.emplace();
            self.author_type->emplace_back(std::move(at));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_author_id(this auto&& self, opt<std::vector<snowflake>> aid) { self.author_id = std::move(aid); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_author_id, snowflake)
        decltype(auto) add_author_id(this auto&& self, snowflake aid) {
            if (!self.author_id) self.author_id.emplace();
            self.author_id->emplace_back(aid);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_mentions(this auto&& self, opt<std::vector<snowflake>> m) { self.mentions = std::move(m); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_mentions, snowflake)
        decltype(auto) add_mention(this auto&& self, snowflake m) {
            if (!self.mentions) self.mentions.emplace();
            self.mentions->emplace_back(m);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_mentions_role_id(this auto&& self, opt<std::vector<snowflake>> mri) { self.mentions_role_id = std::move(mri); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_mentions_role_id, snowflake)
        decltype(auto) add_mentions_role_id(this auto&& self, snowflake mri) {
            if (!self.mentions_role_id) self.mentions_role_id.emplace();
            self.mentions_role_id->emplace_back(mri);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_mention_everyone(this auto&& self, opt<bool> me) noexcept { self.mention_everyone = me; return std::forward<decltype(self)>(self); }
        decltype(auto) set_replied_to_user_id(this auto&& self, opt<std::vector<snowflake>> rtu) { self.replied_to_user_id = std::move(rtu); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_replied_to_user_id, snowflake)
        decltype(auto) add_replied_to_user_id(this auto&& self, snowflake uid) {
            if (!self.replied_to_user_id) self.replied_to_user_id.emplace();
            self.replied_to_user_id->emplace_back(uid);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_replied_to_message_id(this auto&& self, opt<std::vector<snowflake>> rtm) { self.replied_to_message_id = std::move(rtm); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_replied_to_message_id, snowflake)
        decltype(auto) add_replied_to_message_id(this auto&& self, snowflake mid) {
            if (!self.replied_to_message_id) self.replied_to_message_id.emplace();
            self.replied_to_message_id->emplace_back(mid);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_pinned(this auto&& self, opt<bool> p) noexcept { self.pinned = p; return std::forward<decltype(self)>(self); }
        decltype(auto) set_has(this auto&& self, opt<std::vector<std::string>> h) { self.has = std::move(h); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_has, std::string)
        decltype(auto) add_has(this auto&& self, std::string h) {
            if (!self.has) self.has.emplace();
            self.has->emplace_back(std::move(h));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_embed_type(this auto&& self, opt<std::vector<std::string>> et) { self.embed_type = std::move(et); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_embed_type, std::string)
        decltype(auto) add_embed_type(this auto&& self, std::string et) {
            if (!self.embed_type) self.embed_type.emplace();
            self.embed_type->emplace_back(std::move(et));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_embed_provider(this auto&& self, opt<std::vector<std::string>> ep) { self.embed_provider = std::move(ep); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_embed_provider, std::string)
        decltype(auto) add_embed_provider(this auto&& self, std::string ep) {
            if (!self.embed_provider) self.embed_provider.emplace();
            self.embed_provider->emplace_back(std::move(ep));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_link_hostname(this auto&& self, opt<std::vector<std::string>> lh) { self.link_hostname = std::move(lh); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_link_hostname, std::string)
        decltype(auto) add_link_hostname(this auto&& self, std::string lh) {
            if (!self.link_hostname) self.link_hostname.emplace();
            self.link_hostname->emplace_back(std::move(lh));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_attachment_filename(this auto&& self, opt<std::vector<std::string>> af) { self.attachment_filename = std::move(af); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_attachment_filename, std::string)
        decltype(auto) add_attachment_filename(this auto&& self, std::string af) {
            if (!self.attachment_filename) self.attachment_filename.emplace();
            self.attachment_filename->emplace_back(std::move(af));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_attachment_extension(this auto&& self, opt<std::vector<std::string>> ae) { self.attachment_extension = std::move(ae); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_attachment_extension, std::string)
        decltype(auto) add_attachment_extension(this auto&& self, std::string ae) {
            if (!self.attachment_extension) self.attachment_extension.emplace();
            self.attachment_extension->emplace_back(std::move(ae));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_sort_by(this auto&& self, opt<std::string> sb) { self.sort_by = std::move(sb); return std::forward<decltype(self)>(self); }
        decltype(auto) set_sort_order(this auto&& self, opt<std::string> so) { self.sort_order = std::move(so); return std::forward<decltype(self)>(self); }
        decltype(auto) set_include_nsfw(this auto&& self, opt<bool> in) noexcept { self.include_nsfw = in; return std::forward<decltype(self)>(self); }
    };

    struct search_guild_messages_response {
        std::vector<discusy::message::message> messages{};
        integer total_results{};

        static search_guild_messages_response create(std::vector<discusy::message::message> msgs = {}, integer total = 0) {
            return search_guild_messages_response{.messages = std::move(msgs), .total_results = total};
        }
        decltype(auto) set_messages(this auto&& self, std::vector<discusy::message::message> msgs) { self.messages = std::move(msgs); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_messages, discusy::message::message)
        decltype(auto) add_message(this auto&& self, discusy::message::message msg) { self.messages.emplace_back(std::move(msg)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_total_results(this auto&& self, integer total) noexcept { self.total_results = total; return std::forward<decltype(self)>(self); }
    };

    struct edit_message {
        opt<explicit_null<std::string>> content{};
        opt<explicit_null<std::vector<discusy::message::embed>>> embeds{};
        opt<explicit_null<flags_t<discusy::message::message_flags>>> flags{};
        opt<explicit_null<allowed_mentions_obj>> allowed_mentions{};
        opt<explicit_null<std::vector<discusy::components::component>>> components{};
        opt<explicit_null<std::vector<attachment_request>>> attachments{};

        static edit_message create(opt<std::string> content_ = {}) {
            edit_message req{};
            if (content_) req.content = explicit_null<std::string>{*content_};
            return req;
        }

        template <typename From>
        static edit_message from(From&& msg) {
            DISCUSY_FORWARD_IF_SAME(edit_message, msg);

            static_assert(
                DISCUSY_HAS_FIELD(From, content) ||
                DISCUSY_HAS_FIELD(From, embeds) ||
                DISCUSY_HAS_FIELD(From, flags) ||
                DISCUSY_HAS_FIELD(From, components),
                "edit_message::from: Source type contains no compatible message fields."
            );

            edit_message req{};
            DISCUSY_TRANSFER_FIELD(req, msg, content);
            DISCUSY_TRANSFER_FIELD(req, msg, embeds);
            DISCUSY_TRANSFER_FIELD(req, msg, flags);
            DISCUSY_TRANSFER_FIELD(req, msg, components);
            return req;

            DISCUSY_END_FROM
        }

        decltype(auto) set_content(this auto&& self, opt<explicit_null<std::string>> c) { self.content = std::move(c); return std::forward<decltype(self)>(self); }
        decltype(auto) set_content(this auto&& self, std::string c) { self.content = explicit_null<std::string>{std::move(c)}; return std::forward<decltype(self)>(self); }
        decltype(auto) set_embeds(this auto&& self, opt<explicit_null<std::vector<discusy::message::embed>>> embs) { self.embeds = std::move(embs); return std::forward<decltype(self)>(self); }
        decltype(auto) set_embeds(this auto&& self, std::vector<discusy::message::embed> embs) { self.embeds = explicit_null<std::vector<discusy::message::embed>>{std::move(embs)}; return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_embeds, discusy::message::embed)
        decltype(auto) add_embed(this auto&& self, discusy::message::embed emb) {
            if (!self.embeds || !self.embeds->has_value()) self.embeds.emplace().emplace();
            (**self.embeds).emplace_back(std::move(emb));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_flags(this auto&& self, opt<explicit_null<flags_t<discusy::message::message_flags>>> f) noexcept { self.flags = f; return std::forward<decltype(self)>(self); }
        decltype(auto) set_flags(this auto&& self, flags_t<discusy::message::message_flags> f) noexcept { self.flags = explicit_null<flags_t<discusy::message::message_flags>>{f}; return std::forward<decltype(self)>(self); }
        decltype(auto) add_flags(this auto&& self, discusy::message::message_flags f) noexcept {
            if (!self.flags || !self.flags->has_value()) self.flags.emplace().emplace();
            (**self.flags).add_flags(f);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) add_flag(this auto&& self, discusy::message::message_flags f) noexcept { return std::forward<decltype(self)>(self).add_flags(f); }
        decltype(auto) set_allowed_mentions(this auto&& self, opt<explicit_null<allowed_mentions_obj>> am) { self.allowed_mentions = std::move(am); return std::forward<decltype(self)>(self); }
        decltype(auto) set_allowed_mentions(this auto&& self, allowed_mentions_obj am) { self.allowed_mentions = explicit_null<allowed_mentions_obj>{std::move(am)}; return std::forward<decltype(self)>(self); }
        decltype(auto) set_components(this auto&& self, opt<explicit_null<std::vector<discusy::components::component>>> comps) {
            self.components = std::move(comps);
            if (self.components && self.components->has_value() && !(*self.components)->empty()) {
                self.add_flags(discusy::message::message_flags::IS_COMPONENTS_V2);
            }
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_components(this auto&& self, std::vector<discusy::components::component> comps) {
            const bool not_empty = !comps.empty();
            self.components = explicit_null<std::vector<discusy::components::component>>{std::move(comps)};
            if (not_empty) {
                self.add_flags(discusy::message::message_flags::IS_COMPONENTS_V2);
            }
            return std::forward<decltype(self)>(self);
        }
        DISCUSY_VARIADIC_SETTER(set_components, discusy::components::component)
        decltype(auto) add_component(this auto&& self, discusy::components::component comp) {
            if (!self.components || !self.components->has_value()) self.components.emplace().emplace();
            (**self.components).emplace_back(std::move(comp));
            self.add_flags(discusy::message::message_flags::IS_COMPONENTS_V2);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) add_action_row(this auto&& self, discusy::components::ActionRow row) {
            return std::forward<decltype(self)>(self).add_component(std::move(row));
        }
        decltype(auto) add_container(this auto&& self, discusy::components::Container container) {
            return std::forward<decltype(self)>(self).add_component(std::move(container));
        }
        decltype(auto) add_section(this auto&& self, discusy::components::Section section) {
            return std::forward<decltype(self)>(self).add_component(std::move(section));
        }
        decltype(auto) add_text_display(this auto&& self, discusy::components::TextDisplay td) {
            return std::forward<decltype(self)>(self).add_component(std::move(td));
        }
        decltype(auto) add_text_display(this auto&& self, std::string text) {
            return std::forward<decltype(self)>(self).add_component(discusy::components::TextDisplay::create(std::move(text)));
        }
        decltype(auto) add_media_gallery(this auto&& self, discusy::components::MediaGallery mg) {
            return std::forward<decltype(self)>(self).add_component(std::move(mg));
        }
        decltype(auto) add_file(this auto&& self, discusy::components::File f) {
            return std::forward<decltype(self)>(self).add_component(std::move(f));
        }
        decltype(auto) add_separator(this auto&& self, discusy::components::Separator sep = discusy::components::Separator::create()) {
            return std::forward<decltype(self)>(self).add_component(std::move(sep));
        }
        decltype(auto) set_attachments(this auto&& self, opt<explicit_null<std::vector<attachment_request>>> atts) { self.attachments = std::move(atts); return std::forward<decltype(self)>(self); }
        decltype(auto) set_attachments(this auto&& self, std::vector<attachment_request> atts) { self.attachments = explicit_null<std::vector<attachment_request>>{std::move(atts)}; return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_attachments, attachment_request)
        decltype(auto) add_attachment(this auto&& self, attachment_request att) {
            if (!self.attachments || !self.attachments->has_value()) self.attachments.emplace().emplace();
            (**self.attachments).emplace_back(std::move(att));
            return std::forward<decltype(self)>(self);
        }
    };

    struct bulk_delete_messages {
        std::vector<snowflake> messages{};

        static bulk_delete_messages create(std::vector<snowflake> msgs = {}) {
            return bulk_delete_messages{.messages = std::move(msgs)};
        }
        decltype(auto) set_messages(this auto&& self, std::vector<snowflake> msgs) { self.messages = std::move(msgs); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_messages, snowflake)
        decltype(auto) add_message(this auto&& self, snowflake mid) { self.messages.emplace_back(mid); return std::forward<decltype(self)>(self); }
    };

    enum class reaction_type : std::uint8_t {
        NORMAL = 0,
        BURST = 1,
    };

    struct get_reactions_query {
        opt<reaction_type> type{};
        opt<snowflake> after{};
        opt<integer> limit{};

        static get_reactions_query create() noexcept {
            return get_reactions_query{};
        }
        decltype(auto) set_type(this auto&& self, opt<reaction_type> t) noexcept { self.type = t; return std::forward<decltype(self)>(self); }
        decltype(auto) set_after(this auto&& self, opt<snowflake> a) noexcept { self.after = a; return std::forward<decltype(self)>(self); }
        decltype(auto) set_limit(this auto&& self, opt<integer> l) noexcept { self.limit = l; return std::forward<decltype(self)>(self); }
    };

    struct get_channel_pins_query {
        opt<timestamp> before{};
        opt<integer> limit{};

        static get_channel_pins_query create() noexcept {
            return get_channel_pins_query{};
        }
        decltype(auto) set_before(this auto&& self, opt<timestamp> b) noexcept { self.before = b; return std::forward<decltype(self)>(self); }
        decltype(auto) set_limit(this auto&& self, opt<integer> l) noexcept { self.limit = l; return std::forward<decltype(self)>(self); }
    };

    struct message_pin {
        timestamp pinned_at{};
        discusy::message::message message{};

        static message_pin create(timestamp pat = {}, discusy::message::message msg = {}) {
            return message_pin{.pinned_at = pat, .message = std::move(msg)};
        }
        decltype(auto) set_pinned_at(this auto&& self, timestamp pat) noexcept { self.pinned_at = pat; return std::forward<decltype(self)>(self); }
        decltype(auto) set_message(this auto&& self, discusy::message::message msg) { self.message = std::move(msg); return std::forward<decltype(self)>(self); }
    };

    struct get_channel_pins_response {
        std::vector<message_pin> items{};
        bool has_more{};

        static get_channel_pins_response create(std::vector<message_pin> itms = {}, bool hm = false) {
            return get_channel_pins_response{.items = std::move(itms), .has_more = hm};
        }
        decltype(auto) set_items(this auto&& self, std::vector<message_pin> itms) { self.items = std::move(itms); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_items, message_pin)
        decltype(auto) add_item(this auto&& self, message_pin itm) { self.items.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_has_more(this auto&& self, bool hm) noexcept { self.has_more = hm; return std::forward<decltype(self)>(self); }
    };
}

namespace channels {
    struct modify_group_dm {
        opt<std::string> name{};
        opt<image_data> icon{};

        static modify_group_dm create(opt<std::string> name_ = {}) {
            return modify_group_dm{.name = std::move(name_)};
        }
        decltype(auto) set_name(this auto&& self, opt<std::string> n) { self.name = std::move(n); return std::forward<decltype(self)>(self); }
        decltype(auto) set_icon(this auto&& self, opt<image_data> ic) { self.icon = std::move(ic); return std::forward<decltype(self)>(self); }
    };

    struct modify_guild_channel {
        opt<std::string> name{};
        opt<discusy::channel::channel_type> type{};
        opt<explicit_null<integer>> position{};
        opt<explicit_null<std::string>> topic{};
        opt<explicit_null<bool>> nsfw{};
        opt<explicit_null<std::uint16_t>> rate_limit_per_user{};
        opt<explicit_null<integer>> bitrate{};
        opt<explicit_null<std::uint16_t>> user_limit{};
        opt<explicit_null<std::vector<discusy::channel::overwrite>>> permission_overwrites{};
        opt<explicit_null<snowflake>> parent_id{};
        opt<explicit_null<std::string>> rtc_region{};
        opt<explicit_null<discusy::channel::video_quality_mode>> video_quality_mode{};
        opt<explicit_null<integer>> default_auto_archive_duration{};
        opt<flags_t<discusy::channel::channel_flags>> flags{};
        opt<std::vector<discusy::channel::forum_tag>> available_tags{};
        opt<explicit_null<discusy::channel::default_reaction>> default_reaction_emoji{};
        opt<integer> default_thread_rate_limit_per_user{};
        opt<explicit_null<discusy::channel::sort_order_type>> default_sort_order{};
        opt<discusy::channel::forum_layout_type> default_forum_layout{};

        static modify_guild_channel create(opt<std::string> name_ = {}) {
            return modify_guild_channel{.name = std::move(name_)};
        }
        template <typename From>
        static modify_guild_channel from(From&& ch) {
            DISCUSY_FORWARD_IF_SAME(modify_guild_channel, ch);

            static_assert(
                DISCUSY_HAS_FIELD(From, name) ||
                DISCUSY_HAS_FIELD(From, type) ||
                DISCUSY_HAS_FIELD(From, position) ||
                DISCUSY_HAS_FIELD(From, topic) ||
                DISCUSY_HAS_FIELD(From, nsfw) ||
                DISCUSY_HAS_FIELD(From, rate_limit_per_user) ||
                DISCUSY_HAS_FIELD(From, bitrate) ||
                DISCUSY_HAS_FIELD(From, user_limit) ||
                DISCUSY_HAS_FIELD(From, permission_overwrites) ||
                DISCUSY_HAS_FIELD(From, parent_id) ||
                DISCUSY_HAS_FIELD(From, rtc_region) ||
                DISCUSY_HAS_FIELD(From, video_quality_mode) ||
                DISCUSY_HAS_FIELD(From, default_auto_archive_duration) ||
                DISCUSY_HAS_FIELD(From, flags) ||
                DISCUSY_HAS_FIELD(From, available_tags) ||
                DISCUSY_HAS_FIELD(From, default_reaction_emoji) ||
                DISCUSY_HAS_FIELD(From, default_thread_rate_limit_per_user) ||
                DISCUSY_HAS_FIELD(From, default_sort_order) ||
                DISCUSY_HAS_FIELD(From, default_forum_layout),
                "modify_guild_channel::from: Source type contains no compatible channel fields."
            );

            modify_guild_channel m{};
            DISCUSY_TRANSFER_FIELD(m, ch, name);
            DISCUSY_TRANSFER_FIELD(m, ch, type);
            DISCUSY_TRANSFER_FIELD(m, ch, position);
            DISCUSY_TRANSFER_FIELD(m, ch, topic);
            DISCUSY_TRANSFER_FIELD(m, ch, nsfw);
            DISCUSY_TRANSFER_FIELD(m, ch, rate_limit_per_user);
            DISCUSY_TRANSFER_FIELD(m, ch, bitrate);
            DISCUSY_TRANSFER_FIELD(m, ch, user_limit);
            DISCUSY_TRANSFER_FIELD(m, ch, permission_overwrites);
            DISCUSY_TRANSFER_FIELD(m, ch, parent_id);
            DISCUSY_TRANSFER_FIELD(m, ch, rtc_region);
            DISCUSY_TRANSFER_FIELD(m, ch, video_quality_mode);
            DISCUSY_TRANSFER_FIELD(m, ch, default_auto_archive_duration);
            DISCUSY_TRANSFER_FIELD(m, ch, flags);
            DISCUSY_TRANSFER_FIELD(m, ch, available_tags);
            DISCUSY_TRANSFER_FIELD(m, ch, default_reaction_emoji);
            DISCUSY_TRANSFER_FIELD(m, ch, default_thread_rate_limit_per_user);
            DISCUSY_TRANSFER_FIELD(m, ch, default_sort_order);
            DISCUSY_TRANSFER_FIELD(m, ch, default_forum_layout);
            return m;

            DISCUSY_END_FROM
        }
        decltype(auto) set_name(this auto&& self, opt<std::string> n) { self.name = std::move(n); return std::forward<decltype(self)>(self); }
        decltype(auto) set_type(this auto&& self, opt<discusy::channel::channel_type> t) noexcept { self.type = t; return std::forward<decltype(self)>(self); }
        decltype(auto) set_position(this auto&& self, opt<explicit_null<integer>> pos) noexcept { self.position = pos; return std::forward<decltype(self)>(self); }
        decltype(auto) set_topic(this auto&& self, opt<explicit_null<std::string>> top) { self.topic = std::move(top); return std::forward<decltype(self)>(self); }
        decltype(auto) set_nsfw(this auto&& self, opt<explicit_null<bool>> n) noexcept { self.nsfw = n; return std::forward<decltype(self)>(self); }
        decltype(auto) set_rate_limit_per_user(this auto&& self, opt<explicit_null<std::uint16_t>> r) noexcept { self.rate_limit_per_user = r; return std::forward<decltype(self)>(self); }
        decltype(auto) set_bitrate(this auto&& self, opt<explicit_null<integer>> br) noexcept { self.bitrate = br; return std::forward<decltype(self)>(self); }
        decltype(auto) set_user_limit(this auto&& self, opt<explicit_null<std::uint16_t>> ul) noexcept { self.user_limit = ul; return std::forward<decltype(self)>(self); }
        decltype(auto) set_permission_overwrites(this auto&& self, opt<explicit_null<std::vector<discusy::channel::overwrite>>> ov) { self.permission_overwrites = std::move(ov); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_permission_overwrites, discusy::channel::overwrite)
        decltype(auto) set_parent_id(this auto&& self, opt<explicit_null<snowflake>> pid) noexcept { self.parent_id = pid; return std::forward<decltype(self)>(self); }
        decltype(auto) set_rtc_region(this auto&& self, opt<explicit_null<std::string>> reg) { self.rtc_region = std::move(reg); return std::forward<decltype(self)>(self); }
        decltype(auto) set_video_quality_mode(this auto&& self, opt<explicit_null<discusy::channel::video_quality_mode>> vqm) noexcept { self.video_quality_mode = vqm; return std::forward<decltype(self)>(self); }
        decltype(auto) set_default_auto_archive_duration(this auto&& self, opt<explicit_null<integer>> daad) noexcept { self.default_auto_archive_duration = daad; return std::forward<decltype(self)>(self); }
        decltype(auto) set_flags(this auto&& self, opt<flags_t<discusy::channel::channel_flags>> f) noexcept { self.flags = f; return std::forward<decltype(self)>(self); }
        decltype(auto) set_available_tags(this auto&& self, opt<std::vector<discusy::channel::forum_tag>> tags) { self.available_tags = std::move(tags); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_available_tags, discusy::channel::forum_tag)
        decltype(auto) add_available_tag(this auto&& self, discusy::channel::forum_tag tag) {
            if (!self.available_tags) self.available_tags.emplace();
            self.available_tags->emplace_back(std::move(tag));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_default_reaction_emoji(this auto&& self, opt<explicit_null<discusy::channel::default_reaction>> dre) { self.default_reaction_emoji = std::move(dre); return std::forward<decltype(self)>(self); }
        decltype(auto) set_default_thread_rate_limit_per_user(this auto&& self, opt<integer> dtrlpu) noexcept { self.default_thread_rate_limit_per_user = dtrlpu; return std::forward<decltype(self)>(self); }
        decltype(auto) set_default_sort_order(this auto&& self, opt<explicit_null<discusy::channel::sort_order_type>> dso) noexcept { self.default_sort_order = dso; return std::forward<decltype(self)>(self); }
        decltype(auto) set_default_forum_layout(this auto&& self, opt<discusy::channel::forum_layout_type> dfl) noexcept { self.default_forum_layout = dfl; return std::forward<decltype(self)>(self); }
    };
    
    struct modify_thread {
        opt<std::string> name{};
        opt<bool> archived{};
        opt<std::uint16_t> auto_archive_duration{};
        opt<bool> locked{};
        opt<bool> invitable{};
        opt<explicit_null<std::uint16_t>> rate_limit_per_user{};
        opt<flags_t<discusy::channel::channel_flags>> flags{};
        opt<std::vector<snowflake>> applied_tags{};

        static modify_thread create(opt<std::string> name_ = {}) {
            return modify_thread{.name = std::move(name_)};
        }
        decltype(auto) set_name(this auto&& self, opt<std::string> n) { self.name = std::move(n); return std::forward<decltype(self)>(self); }
        decltype(auto) set_archived(this auto&& self, opt<bool> a) noexcept { self.archived = a; return std::forward<decltype(self)>(self); }
        decltype(auto) set_auto_archive_duration(this auto&& self, opt<std::uint16_t> aad) noexcept { self.auto_archive_duration = aad; return std::forward<decltype(self)>(self); }
        decltype(auto) set_locked(this auto&& self, opt<bool> l) noexcept { self.locked = l; return std::forward<decltype(self)>(self); }
        decltype(auto) set_invitable(this auto&& self, opt<bool> inv) noexcept { self.invitable = inv; return std::forward<decltype(self)>(self); }
        decltype(auto) set_rate_limit_per_user(this auto&& self, opt<explicit_null<std::uint16_t>> r) noexcept { self.rate_limit_per_user = r; return std::forward<decltype(self)>(self); }
        decltype(auto) set_flags(this auto&& self, opt<flags_t<discusy::channel::channel_flags>> f) noexcept { self.flags = f; return std::forward<decltype(self)>(self); }
        decltype(auto) set_applied_tags(this auto&& self, opt<std::vector<snowflake>> tags) { self.applied_tags = std::move(tags); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_applied_tags, snowflake)
        decltype(auto) add_applied_tag(this auto&& self, snowflake tag) {
            if (!self.applied_tags) self.applied_tags.emplace();
            self.applied_tags->emplace_back(tag);
            return std::forward<decltype(self)>(self);
        }
    };

    using modify_channel = std::variant<modify_group_dm, modify_guild_channel, modify_thread>;

    struct set_voice_channel_status {
        explicit_null<std::string> status{};

        static set_voice_channel_status create(std::string status_ = {}) {
            return set_voice_channel_status{.status = explicit_null<std::string>{std::move(status_)}};
        }
        decltype(auto) set_status(this auto&& self, explicit_null<std::string> s) { self.status = std::move(s); return std::forward<decltype(self)>(self); }
        decltype(auto) set_status(this auto&& self, std::string s) { self.status = explicit_null<std::string>{std::move(s)}; return std::forward<decltype(self)>(self); }
    };

    enum class edit_channel_permissions_type : std::uint8_t {
        role = 0,
        member = 1,
    };

    struct edit_channel_permissions {
        opt<explicit_null<permissions_t>> allow{};
        opt<explicit_null<permissions_t>> deny{};
        edit_channel_permissions_type type{};

        static edit_channel_permissions create(edit_channel_permissions_type t = edit_channel_permissions_type::role) noexcept {
            return edit_channel_permissions{.type = t};
        }
        decltype(auto) set_allow(this auto&& self, opt<explicit_null<permissions_t>> a) noexcept { self.allow = a; return std::forward<decltype(self)>(self); }
        decltype(auto) set_deny(this auto&& self, opt<explicit_null<permissions_t>> d) noexcept { self.deny = d; return std::forward<decltype(self)>(self); }
        decltype(auto) set_type(this auto&& self, edit_channel_permissions_type t) noexcept { self.type = t; return std::forward<decltype(self)>(self); }
    };

    struct create_channel_invite {
        opt<std::uint32_t> max_age{}; // default 86400 (24h), 0 = never, max 604800
        opt<std::uint8_t> max_uses{}; // default 0 (unlimited), max 100
        opt<bool> temporary{}; // default false
        opt<bool> unique{}; // default false
        opt<discusy::invite::invite_target_type> target_type{};
        opt<snowflake> target_user_id{}; // required if target_type == STREAM
        opt<snowflake> target_application_id{}; // required if target_type == EMBEDDED_APPLICATION
        opt<std::vector<snowflake>> target_user_ids{};
        opt<std::vector<snowflake>> role_ids{};

        static create_channel_invite create() noexcept {
            return create_channel_invite{};
        }
        decltype(auto) set_max_age(this auto&& self, opt<std::uint32_t> ma) noexcept { self.max_age = ma; return std::forward<decltype(self)>(self); }
        decltype(auto) set_max_uses(this auto&& self, opt<std::uint8_t> mu) noexcept { self.max_uses = mu; return std::forward<decltype(self)>(self); }
        decltype(auto) set_temporary(this auto&& self, opt<bool> t) noexcept { self.temporary = t; return std::forward<decltype(self)>(self); }
        decltype(auto) set_unique(this auto&& self, opt<bool> u) noexcept { self.unique = u; return std::forward<decltype(self)>(self); }
        decltype(auto) set_target_type(this auto&& self, opt<discusy::invite::invite_target_type> tt) noexcept { self.target_type = tt; return std::forward<decltype(self)>(self); }
        decltype(auto) set_target_user_id(this auto&& self, opt<snowflake> tuid) noexcept { self.target_user_id = tuid; return std::forward<decltype(self)>(self); }
        decltype(auto) set_target_application_id(this auto&& self, opt<snowflake> taid) noexcept { self.target_application_id = taid; return std::forward<decltype(self)>(self); }
        decltype(auto) set_target_user_ids(this auto&& self, opt<std::vector<snowflake>> user_ids) { self.target_user_ids = std::move(user_ids); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_target_user_ids, snowflake)
        decltype(auto) add_target_user_id(this auto&& self, snowflake uid) {
            if (!self.target_user_ids) self.target_user_ids.emplace();
            self.target_user_ids->emplace_back(uid);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_role_ids(this auto&& self, opt<std::vector<snowflake>> rids) { self.role_ids = std::move(rids); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_role_ids, snowflake)
        decltype(auto) add_role_id(this auto&& self, snowflake rid) {
            if (!self.role_ids) self.role_ids.emplace();
            self.role_ids->emplace_back(rid);
            return std::forward<decltype(self)>(self);
        }
    };

    struct follow_announcement_channel {
        snowflake webhook_channel_id{};

        static follow_announcement_channel create(snowflake id = {}) noexcept {
            return follow_announcement_channel{.webhook_channel_id = id};
        }
        decltype(auto) set_webhook_channel_id(this auto&& self, snowflake id) noexcept { self.webhook_channel_id = id; return std::forward<decltype(self)>(self); }
    };

    struct group_dm_add_recipient {
        std::string access_token{};
        std::string nick{};

        static group_dm_add_recipient create(std::string token_ = {}, std::string nick_ = {}) {
            return group_dm_add_recipient{.access_token = std::move(token_), .nick = std::move(nick_)};
        }
        decltype(auto) set_access_token(this auto&& self, std::string token_) { self.access_token = std::move(token_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_nick(this auto&& self, std::string nick_) { self.nick = std::move(nick_); return std::forward<decltype(self)>(self); }
    };

    struct start_thread_from_message {
        std::string name{};
        opt<std::uint16_t> auto_archive_duration{};
        opt<explicit_null<std::uint16_t>> rate_limit_per_user{};

        static start_thread_from_message create(std::string name_ = {}) {
            return start_thread_from_message{.name = std::move(name_)};
        }
        decltype(auto) set_name(this auto&& self, std::string n) { self.name = std::move(n); return std::forward<decltype(self)>(self); }
        decltype(auto) set_auto_archive_duration(this auto&& self, opt<std::uint16_t> aad) noexcept { self.auto_archive_duration = aad; return std::forward<decltype(self)>(self); }
        decltype(auto) set_rate_limit_per_user(this auto&& self, opt<explicit_null<std::uint16_t>> r) noexcept { self.rate_limit_per_user = r; return std::forward<decltype(self)>(self); }
    };

    struct start_thread_without_message {
        std::string name{};
        opt<std::uint16_t> auto_archive_duration{};
        opt<discusy::channel::channel_type> type{};
        opt<bool> invitable{};
        opt<explicit_null<std::uint16_t>> rate_limit_per_user{};

        static start_thread_without_message create(std::string name_ = {}, opt<discusy::channel::channel_type> t = {}) {
            return start_thread_without_message{.name = std::move(name_), .type = t};
        }
        decltype(auto) set_name(this auto&& self, std::string n) { self.name = std::move(n); return std::forward<decltype(self)>(self); }
        decltype(auto) set_auto_archive_duration(this auto&& self, opt<std::uint16_t> aad) noexcept { self.auto_archive_duration = aad; return std::forward<decltype(self)>(self); }
        decltype(auto) set_type(this auto&& self, opt<discusy::channel::channel_type> t) noexcept { self.type = t; return std::forward<decltype(self)>(self); }
        decltype(auto) set_invitable(this auto&& self, opt<bool> inv) noexcept { self.invitable = inv; return std::forward<decltype(self)>(self); }
        decltype(auto) set_rate_limit_per_user(this auto&& self, opt<explicit_null<std::uint16_t>> r) noexcept { self.rate_limit_per_user = r; return std::forward<decltype(self)>(self); }
    };

    struct forum_thread_message_params {
        opt<std::string> content{};
        opt<std::vector<discusy::message::embed>> embeds{};
        opt<message::allowed_mentions_obj> allowed_mentions{};
        opt<std::vector<discusy::components::component>> components{};
        opt<std::vector<snowflake>> sticker_ids{};
        opt<std::vector<message::attachment_request>> attachments{};
        opt<flags_t<discusy::message::message_flags>> flags{};

        static forum_thread_message_params create(opt<std::string> content_ = {}) {
            return forum_thread_message_params{.content = std::move(content_)};
        }
        decltype(auto) set_content(this auto&& self, opt<std::string> c) { self.content = std::move(c); return std::forward<decltype(self)>(self); }
        decltype(auto) set_embeds(this auto&& self, opt<std::vector<discusy::message::embed>> embs) { self.embeds = std::move(embs); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_embeds, discusy::message::embed)
        decltype(auto) add_embed(this auto&& self, discusy::message::embed emb) {
            if (!self.embeds) self.embeds.emplace();
            self.embeds->emplace_back(std::move(emb));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_allowed_mentions(this auto&& self, opt<message::allowed_mentions_obj> am) { self.allowed_mentions = std::move(am); return std::forward<decltype(self)>(self); }
        decltype(auto) set_components(this auto&& self, opt<std::vector<discusy::components::component>> comps) {
            self.components = std::move(comps);
            if (self.components && !self.components->empty()) {
                self.add_flags(discusy::message::message_flags::IS_COMPONENTS_V2);
            }
            return std::forward<decltype(self)>(self);
        }
        DISCUSY_VARIADIC_SETTER(set_components, discusy::components::component)
        decltype(auto) add_component(this auto&& self, discusy::components::component comp) {
            if (!self.components) self.components.emplace();
            self.components->emplace_back(std::move(comp));
            self.add_flags(discusy::message::message_flags::IS_COMPONENTS_V2);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) add_action_row(this auto&& self, discusy::components::ActionRow row) {
            return std::forward<decltype(self)>(self).add_component(std::move(row));
        }
        decltype(auto) add_container(this auto&& self, discusy::components::Container container) {
            return std::forward<decltype(self)>(self).add_component(std::move(container));
        }
        decltype(auto) add_section(this auto&& self, discusy::components::Section section) {
            return std::forward<decltype(self)>(self).add_component(std::move(section));
        }
        decltype(auto) add_text_display(this auto&& self, discusy::components::TextDisplay td) {
            return std::forward<decltype(self)>(self).add_component(std::move(td));
        }
        decltype(auto) add_text_display(this auto&& self, std::string text) {
            return std::forward<decltype(self)>(self).add_component(discusy::components::TextDisplay::create(std::move(text)));
        }
        decltype(auto) add_media_gallery(this auto&& self, discusy::components::MediaGallery mg) {
            return std::forward<decltype(self)>(self).add_component(std::move(mg));
        }
        decltype(auto) add_file(this auto&& self, discusy::components::File f) {
            return std::forward<decltype(self)>(self).add_component(std::move(f));
        }
        decltype(auto) add_separator(this auto&& self, discusy::components::Separator sep = discusy::components::Separator::create()) {
            return std::forward<decltype(self)>(self).add_component(std::move(sep));
        }
        decltype(auto) set_sticker_ids(this auto&& self, opt<std::vector<snowflake>> sids) { self.sticker_ids = std::move(sids); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_sticker_ids, snowflake)
        decltype(auto) add_sticker_id(this auto&& self, snowflake sid) {
            if (!self.sticker_ids) self.sticker_ids.emplace();
            self.sticker_ids->emplace_back(sid);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_attachments(this auto&& self, opt<std::vector<message::attachment_request>> atts) { self.attachments = std::move(atts); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_attachments, message::attachment_request)
        decltype(auto) add_attachment(this auto&& self, message::attachment_request att) {
            if (!self.attachments) self.attachments.emplace();
            self.attachments->emplace_back(std::move(att));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_flags(this auto&& self, opt<flags_t<discusy::message::message_flags>> f) noexcept { self.flags = f; return std::forward<decltype(self)>(self); }
        decltype(auto) add_flags(this auto&& self, discusy::message::message_flags f) noexcept {
            if (!self.flags) self.flags.emplace();
            self.flags->add_flags(f);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) add_flag(this auto&& self, discusy::message::message_flags f) noexcept { return std::forward<decltype(self)>(self).add_flags(f); }
    };

    struct start_thread_in_forum_or_media_channel {
        std::string name{};
        opt<std::uint16_t> auto_archive_duration{};
        opt<explicit_null<std::uint16_t>> rate_limit_per_user{};
        forum_thread_message_params message{};
        opt<std::vector<snowflake>> applied_tags{};

        static start_thread_in_forum_or_media_channel create(std::string name_ = {}, forum_thread_message_params msg = {}) {
            return start_thread_in_forum_or_media_channel{.name = std::move(name_), .message = std::move(msg)};
        }
        decltype(auto) set_name(this auto&& self, std::string n) { self.name = std::move(n); return std::forward<decltype(self)>(self); }
        decltype(auto) set_auto_archive_duration(this auto&& self, opt<std::uint16_t> aad) noexcept { self.auto_archive_duration = aad; return std::forward<decltype(self)>(self); }
        decltype(auto) set_rate_limit_per_user(this auto&& self, opt<explicit_null<std::uint16_t>> r) noexcept { self.rate_limit_per_user = r; return std::forward<decltype(self)>(self); }
        decltype(auto) set_message(this auto&& self, forum_thread_message_params msg) { self.message = std::move(msg); return std::forward<decltype(self)>(self); }
        decltype(auto) set_applied_tags(this auto&& self, opt<std::vector<snowflake>> t) { self.applied_tags = std::move(t); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_applied_tags, snowflake)
        decltype(auto) add_applied_tag(this auto&& self, snowflake t) {
            if (!self.applied_tags) self.applied_tags.emplace();
            self.applied_tags->emplace_back(t);
            return std::forward<decltype(self)>(self);
        }
    };

    struct get_thread_member {
        opt<bool> with_member{};

        static get_thread_member create(opt<bool> wm = {}) noexcept {
            return get_thread_member{.with_member = wm};
        }
        decltype(auto) set_with_member(this auto&& self, opt<bool> wm) noexcept { self.with_member = wm; return std::forward<decltype(self)>(self); }
    };

    struct list_thread_members {
        opt<bool> with_member{};
        opt<snowflake> after{};
        opt<std::uint8_t> limit{};

        static list_thread_members create() noexcept {
            return list_thread_members{};
        }
        decltype(auto) set_with_member(this auto&& self, opt<bool> wm) noexcept { self.with_member = wm; return std::forward<decltype(self)>(self); }
        decltype(auto) set_after(this auto&& self, opt<snowflake> a) noexcept { self.after = a; return std::forward<decltype(self)>(self); }
        decltype(auto) set_limit(this auto&& self, opt<std::uint8_t> l) noexcept { self.limit = l; return std::forward<decltype(self)>(self); }
    };

    struct list_archived_threads_query {
        opt<timestamp> before{};
        opt<integer> limit{};

        static list_archived_threads_query create() noexcept {
            return list_archived_threads_query{};
        }
        decltype(auto) set_before(this auto&& self, opt<timestamp> b) noexcept { self.before = b; return std::forward<decltype(self)>(self); }
        decltype(auto) set_limit(this auto&& self, opt<integer> l) noexcept { self.limit = l; return std::forward<decltype(self)>(self); }
    };

    struct list_archived_threads_response {
        std::vector<discusy::channel::channel> threads{};
        std::vector<discusy::channel::thread_member> members{};
        bool has_more{};

        static list_archived_threads_response create() noexcept {
            return list_archived_threads_response{};
        }
        decltype(auto) set_threads(this auto&& self, std::vector<discusy::channel::channel> ths) { self.threads = std::move(ths); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_threads, discusy::channel::channel)
        decltype(auto) add_thread(this auto&& self, discusy::channel::channel th) { self.threads.emplace_back(std::move(th)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_members(this auto&& self, std::vector<discusy::channel::thread_member> mems) { self.members = std::move(mems); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_members, discusy::channel::thread_member)
        decltype(auto) add_member(this auto&& self, discusy::channel::thread_member mem) { self.members.emplace_back(std::move(mem)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_has_more(this auto&& self, bool hm) noexcept { self.has_more = hm; return std::forward<decltype(self)>(self); }
    };

    struct list_joined_private_archived_threads_query {
        opt<snowflake> before{};
        opt<integer> limit{};

        static list_joined_private_archived_threads_query create() noexcept {
            return list_joined_private_archived_threads_query{};
        }
        decltype(auto) set_before(this auto&& self, opt<snowflake> b) noexcept { self.before = b; return std::forward<decltype(self)>(self); }
        decltype(auto) set_limit(this auto&& self, opt<integer> l) noexcept { self.limit = l; return std::forward<decltype(self)>(self); }
    };
}

namespace guild {
    struct get_guild_query {
        opt<bool> with_counts{};

        static get_guild_query create(opt<bool> wc = {}) noexcept {
            return get_guild_query{.with_counts = wc};
        }
        decltype(auto) set_with_counts(this auto&& self, opt<bool> wc) noexcept { self.with_counts = wc; return std::forward<decltype(self)>(self); }
    };

    struct modify_guild {
        opt<std::string> name{};
        opt<explicit_null<std::string>> region{};
        opt<explicit_null<discusy::guild::verification_level>> verification_level{};
        opt<explicit_null<discusy::guild::default_message_notification_level>> default_message_notifications{};
        opt<explicit_null<discusy::guild::explicit_content_filter_level>> explicit_content_filter{};
        opt<explicit_null<snowflake>> afk_channel_id{};
        opt<integer> afk_timeout{};
        opt<explicit_null<image_data>> icon{};
        opt<explicit_null<image_data>> splash{};
        opt<explicit_null<image_data>> discovery_splash{};
        opt<explicit_null<image_data>> banner{};
        opt<explicit_null<snowflake>> system_channel_id{};
        opt<flags_t<discusy::guild::system_channel_flags>> system_channel_flags{};
        opt<explicit_null<snowflake>> rules_channel_id{};
        opt<explicit_null<snowflake>> public_updates_channel_id{};
        opt<explicit_null<std::string>> preferred_locale{};
        opt<std::vector<std::string>> features{};
        opt<explicit_null<std::string>> description{};
        opt<bool> premium_progress_bar_enabled{};
        opt<explicit_null<snowflake>> safety_alerts_channel_id{};

        static modify_guild create(opt<std::string> name_ = {}) {
            return modify_guild{.name = std::move(name_)};
        }
        template <typename From>
        static modify_guild from(From&& g) {
            DISCUSY_FORWARD_IF_SAME(modify_guild, g);

            static_assert(
                DISCUSY_HAS_FIELD(From, name) ||
                DISCUSY_HAS_FIELD(From, region) ||
                DISCUSY_HAS_FIELD(From, verification_level) ||
                DISCUSY_HAS_FIELD(From, default_message_notifications) ||
                DISCUSY_HAS_FIELD(From, explicit_content_filter) ||
                DISCUSY_HAS_FIELD(From, afk_channel_id) ||
                DISCUSY_HAS_FIELD(From, afk_timeout) ||
                DISCUSY_HAS_FIELD(From, system_channel_id) ||
                DISCUSY_HAS_FIELD(From, system_channel_flags) ||
                DISCUSY_HAS_FIELD(From, rules_channel_id) ||
                DISCUSY_HAS_FIELD(From, public_updates_channel_id) ||
                DISCUSY_HAS_FIELD(From, preferred_locale) ||
                DISCUSY_HAS_FIELD(From, features) ||
                DISCUSY_HAS_FIELD(From, description) ||
                DISCUSY_HAS_FIELD(From, premium_progress_bar_enabled) ||
                DISCUSY_HAS_FIELD(From, safety_alerts_channel_id),
                "modify_guild::from: Source type contains no compatible guild fields."
            );

            modify_guild m{};
            DISCUSY_TRANSFER_FIELD(m, g, name);
            DISCUSY_TRANSFER_FIELD(m, g, region);
            DISCUSY_TRANSFER_FIELD(m, g, verification_level);
            DISCUSY_TRANSFER_FIELD(m, g, default_message_notifications);
            DISCUSY_TRANSFER_FIELD(m, g, explicit_content_filter);
            DISCUSY_TRANSFER_FIELD(m, g, afk_channel_id);
            DISCUSY_TRANSFER_FIELD(m, g, afk_timeout);
            DISCUSY_TRANSFER_FIELD(m, g, system_channel_id);
            DISCUSY_TRANSFER_FIELD(m, g, system_channel_flags);
            DISCUSY_TRANSFER_FIELD(m, g, rules_channel_id);
            DISCUSY_TRANSFER_FIELD(m, g, public_updates_channel_id);
            DISCUSY_TRANSFER_FIELD(m, g, preferred_locale);
            DISCUSY_TRANSFER_FIELD(m, g, features);
            DISCUSY_TRANSFER_FIELD(m, g, description);
            DISCUSY_TRANSFER_FIELD(m, g, premium_progress_bar_enabled);
            DISCUSY_TRANSFER_FIELD(m, g, safety_alerts_channel_id);
            return m;

            DISCUSY_END_FROM
        }
        decltype(auto) set_name(this auto&& self, opt<std::string> n) { self.name = std::move(n); return std::forward<decltype(self)>(self); }
        decltype(auto) set_region(this auto&& self, opt<explicit_null<std::string>> r) { self.region = std::move(r); return std::forward<decltype(self)>(self); }
        decltype(auto) set_verification_level(this auto&& self, opt<explicit_null<discusy::guild::verification_level>> vl) noexcept { self.verification_level = vl; return std::forward<decltype(self)>(self); }
        decltype(auto) set_default_message_notifications(this auto&& self, opt<explicit_null<discusy::guild::default_message_notification_level>> dmn) noexcept { self.default_message_notifications = dmn; return std::forward<decltype(self)>(self); }
        decltype(auto) set_explicit_content_filter(this auto&& self, opt<explicit_null<discusy::guild::explicit_content_filter_level>> ecf) noexcept { self.explicit_content_filter = ecf; return std::forward<decltype(self)>(self); }
        decltype(auto) set_afk_channel_id(this auto&& self, opt<explicit_null<snowflake>> acid) noexcept { self.afk_channel_id = acid; return std::forward<decltype(self)>(self); }
        decltype(auto) set_afk_timeout(this auto&& self, opt<integer> at) noexcept { self.afk_timeout = at; return std::forward<decltype(self)>(self); }
        decltype(auto) set_icon(this auto&& self, opt<explicit_null<image_data>> ic) { self.icon = std::move(ic); return std::forward<decltype(self)>(self); }
        decltype(auto) set_splash(this auto&& self, opt<explicit_null<image_data>> sp) { self.splash = std::move(sp); return std::forward<decltype(self)>(self); }
        decltype(auto) set_discovery_splash(this auto&& self, opt<explicit_null<image_data>> dsp) { self.discovery_splash = std::move(dsp); return std::forward<decltype(self)>(self); }
        decltype(auto) set_banner(this auto&& self, opt<explicit_null<image_data>> b) { self.banner = std::move(b); return std::forward<decltype(self)>(self); }
        decltype(auto) set_system_channel_id(this auto&& self, opt<explicit_null<snowflake>> scid) noexcept { self.system_channel_id = scid; return std::forward<decltype(self)>(self); }
        decltype(auto) set_system_channel_flags(this auto&& self, opt<flags_t<discusy::guild::system_channel_flags>> scf) noexcept { self.system_channel_flags = scf; return std::forward<decltype(self)>(self); }
        decltype(auto) set_rules_channel_id(this auto&& self, opt<explicit_null<snowflake>> rcid) noexcept { self.rules_channel_id = rcid; return std::forward<decltype(self)>(self); }
        decltype(auto) set_public_updates_channel_id(this auto&& self, opt<explicit_null<snowflake>> puc) noexcept { self.public_updates_channel_id = puc; return std::forward<decltype(self)>(self); }
        decltype(auto) set_preferred_locale(this auto&& self, opt<explicit_null<std::string>> pl) { self.preferred_locale = std::move(pl); return std::forward<decltype(self)>(self); }
        decltype(auto) set_features(this auto&& self, opt<std::vector<std::string>> f) { self.features = std::move(f); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_features, std::string)
        decltype(auto) add_feature(this auto&& self, std::string feat) {
            if (!self.features) self.features.emplace();
            self.features->emplace_back(std::move(feat));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_description(this auto&& self, opt<explicit_null<std::string>> d) { self.description = std::move(d); return std::forward<decltype(self)>(self); }
        decltype(auto) set_premium_progress_bar_enabled(this auto&& self, opt<bool> ppbe) noexcept { self.premium_progress_bar_enabled = ppbe ? *ppbe : false; return std::forward<decltype(self)>(self); }
        decltype(auto) set_safety_alerts_channel_id(this auto&& self, opt<explicit_null<snowflake>> sacid) noexcept { self.safety_alerts_channel_id = sacid; return std::forward<decltype(self)>(self); }
    };

    struct create_guild_channel {
        std::string name{};
        opt<discusy::channel::channel_type> type{};
        opt<explicit_null<std::string>> topic{};
        opt<explicit_null<integer>> bitrate{};
        opt<explicit_null<std::uint16_t>> user_limit{};
        opt<explicit_null<std::uint16_t>> rate_limit_per_user{};
        opt<explicit_null<integer>> position{};
        opt<explicit_null<std::vector<discusy::channel::overwrite>>> permission_overwrites{};
        opt<explicit_null<snowflake>> parent_id{};
        opt<explicit_null<bool>> nsfw{};
        opt<explicit_null<std::string>> rtc_region{};
        opt<explicit_null<discusy::channel::video_quality_mode>> video_quality_mode{};
        opt<explicit_null<integer>> default_auto_archive_duration{};
        opt<explicit_null<discusy::channel::default_reaction>> default_reaction_emoji{};
        opt<std::vector<discusy::channel::forum_tag>> available_tags{};
        opt<explicit_null<discusy::channel::sort_order_type>> default_sort_order{};
        opt<discusy::channel::forum_layout_type> default_forum_layout{};
        opt<integer> default_thread_rate_limit_per_user{};
        opt<flags_t<discusy::channel::channel_flags>> flags{};

        static create_guild_channel create(std::string name_ = {}, opt<discusy::channel::channel_type> type_ = {}) {
            return create_guild_channel{.name = std::move(name_), .type = type_};
        }
        template <typename From>
        static create_guild_channel from(From&& ch) {
            DISCUSY_FORWARD_IF_SAME(create_guild_channel, ch);

            static_assert(
                DISCUSY_HAS_FIELD(From, name) ||
                DISCUSY_HAS_FIELD(From, type) ||
                DISCUSY_HAS_FIELD(From, topic) ||
                DISCUSY_HAS_FIELD(From, bitrate) ||
                DISCUSY_HAS_FIELD(From, user_limit) ||
                DISCUSY_HAS_FIELD(From, rate_limit_per_user) ||
                DISCUSY_HAS_FIELD(From, position) ||
                DISCUSY_HAS_FIELD(From, permission_overwrites) ||
                DISCUSY_HAS_FIELD(From, parent_id) ||
                DISCUSY_HAS_FIELD(From, nsfw) ||
                DISCUSY_HAS_FIELD(From, rtc_region) ||
                DISCUSY_HAS_FIELD(From, video_quality_mode) ||
                DISCUSY_HAS_FIELD(From, default_auto_archive_duration) ||
                DISCUSY_HAS_FIELD(From, default_reaction_emoji) ||
                DISCUSY_HAS_FIELD(From, available_tags) ||
                DISCUSY_HAS_FIELD(From, default_sort_order) ||
                DISCUSY_HAS_FIELD(From, default_forum_layout) ||
                DISCUSY_HAS_FIELD(From, default_thread_rate_limit_per_user) ||
                DISCUSY_HAS_FIELD(From, flags),
                "create_guild_channel::from: Source type contains no compatible channel fields."
            );

            create_guild_channel c{};
            DISCUSY_TRANSFER_FIELD(c, ch, name);
            DISCUSY_TRANSFER_FIELD(c, ch, type);
            DISCUSY_TRANSFER_FIELD(c, ch, topic);
            DISCUSY_TRANSFER_FIELD(c, ch, bitrate);
            DISCUSY_TRANSFER_FIELD(c, ch, user_limit);
            DISCUSY_TRANSFER_FIELD(c, ch, rate_limit_per_user);
            DISCUSY_TRANSFER_FIELD(c, ch, position);
            DISCUSY_TRANSFER_FIELD(c, ch, permission_overwrites);
            DISCUSY_TRANSFER_FIELD(c, ch, parent_id);
            DISCUSY_TRANSFER_FIELD(c, ch, nsfw);
            DISCUSY_TRANSFER_FIELD(c, ch, rtc_region);
            DISCUSY_TRANSFER_FIELD(c, ch, video_quality_mode);
            DISCUSY_TRANSFER_FIELD(c, ch, default_auto_archive_duration);
            DISCUSY_TRANSFER_FIELD(c, ch, default_reaction_emoji);
            DISCUSY_TRANSFER_FIELD(c, ch, available_tags);
            DISCUSY_TRANSFER_FIELD(c, ch, default_sort_order);
            DISCUSY_TRANSFER_FIELD(c, ch, default_forum_layout);
            DISCUSY_TRANSFER_FIELD(c, ch, default_thread_rate_limit_per_user);
            DISCUSY_TRANSFER_FIELD(c, ch, flags);
            return c;

            DISCUSY_END_FROM
        }
        decltype(auto) set_name(this auto&& self, std::string n) { self.name = std::move(n); return std::forward<decltype(self)>(self); }
        decltype(auto) set_type(this auto&& self, opt<discusy::channel::channel_type> t) noexcept { self.type = t; return std::forward<decltype(self)>(self); }
        decltype(auto) set_topic(this auto&& self, opt<explicit_null<std::string>> top) { self.topic = std::move(top); return std::forward<decltype(self)>(self); }
        decltype(auto) set_bitrate(this auto&& self, opt<explicit_null<integer>> br) noexcept { self.bitrate = br; return std::forward<decltype(self)>(self); }
        decltype(auto) set_user_limit(this auto&& self, opt<explicit_null<std::uint16_t>> ul) noexcept { self.user_limit = ul; return std::forward<decltype(self)>(self); }
        decltype(auto) set_rate_limit_per_user(this auto&& self, opt<explicit_null<std::uint16_t>> r) noexcept { self.rate_limit_per_user = r; return std::forward<decltype(self)>(self); }
        decltype(auto) set_position(this auto&& self, opt<explicit_null<integer>> pos) noexcept { self.position = pos; return std::forward<decltype(self)>(self); }
        decltype(auto) set_permission_overwrites(this auto&& self, opt<explicit_null<std::vector<discusy::channel::overwrite>>> ov) { self.permission_overwrites = std::move(ov); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_permission_overwrites, discusy::channel::overwrite)
        decltype(auto) set_parent_id(this auto&& self, opt<explicit_null<snowflake>> pid) noexcept { self.parent_id = pid; return std::forward<decltype(self)>(self); }
        decltype(auto) set_nsfw(this auto&& self, opt<explicit_null<bool>> n) noexcept { self.nsfw = n; return std::forward<decltype(self)>(self); }
        decltype(auto) set_rtc_region(this auto&& self, opt<explicit_null<std::string>> reg) { self.rtc_region = std::move(reg); return std::forward<decltype(self)>(self); }
        decltype(auto) set_video_quality_mode(this auto&& self, opt<explicit_null<discusy::channel::video_quality_mode>> vqm) noexcept { self.video_quality_mode = vqm; return std::forward<decltype(self)>(self); }
        decltype(auto) set_default_auto_archive_duration(this auto&& self, opt<explicit_null<integer>> daad) noexcept { self.default_auto_archive_duration = daad; return std::forward<decltype(self)>(self); }
        decltype(auto) set_default_reaction_emoji(this auto&& self, opt<explicit_null<discusy::channel::default_reaction>> dre) { self.default_reaction_emoji = std::move(dre); return std::forward<decltype(self)>(self); }
        decltype(auto) set_available_tags(this auto&& self, opt<std::vector<discusy::channel::forum_tag>> tags) { self.available_tags = std::move(tags); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_available_tags, discusy::channel::forum_tag)
        decltype(auto) add_available_tag(this auto&& self, discusy::channel::forum_tag tag) {
            if (!self.available_tags) self.available_tags.emplace();
            self.available_tags->emplace_back(std::move(tag));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_default_sort_order(this auto&& self, opt<explicit_null<discusy::channel::sort_order_type>> dso) noexcept { self.default_sort_order = dso; return std::forward<decltype(self)>(self); }
        decltype(auto) set_default_forum_layout(this auto&& self, opt<discusy::channel::forum_layout_type> dfl) noexcept { self.default_forum_layout = dfl; return std::forward<decltype(self)>(self); }
        decltype(auto) set_default_thread_rate_limit_per_user(this auto&& self, opt<integer> dtrlpu) noexcept { self.default_thread_rate_limit_per_user = dtrlpu; return std::forward<decltype(self)>(self); }
        decltype(auto) set_flags(this auto&& self, opt<flags_t<discusy::channel::channel_flags>> f) noexcept { self.flags = f; return std::forward<decltype(self)>(self); }
    };

    struct modify_guild_channel_position {
        snowflake id{};
        opt<explicit_null<integer>> position{};
        opt<explicit_null<bool>> lock_permissions{};
        opt<explicit_null<snowflake>> parent_id{};
        opt<explicit_null<flags_t<discusy::channel::channel_flags>>> flags{};

        static modify_guild_channel_position create(snowflake id_ = {}) noexcept {
            return modify_guild_channel_position{.id = id_};
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_position(this auto&& self, opt<explicit_null<integer>> pos) noexcept { self.position = pos; return std::forward<decltype(self)>(self); }
        decltype(auto) set_lock_permissions(this auto&& self, opt<explicit_null<bool>> lp) noexcept { self.lock_permissions = lp; return std::forward<decltype(self)>(self); }
        decltype(auto) set_parent_id(this auto&& self, opt<explicit_null<snowflake>> pid) noexcept { self.parent_id = pid; return std::forward<decltype(self)>(self); }
        decltype(auto) set_flags(this auto&& self, opt<explicit_null<flags_t<discusy::channel::channel_flags>>> f) noexcept { self.flags = f; return std::forward<decltype(self)>(self); }
    };

    using modify_guild_channel_positions = std::span<const modify_guild_channel_position>;

    struct list_active_guild_threads_response {
        std::vector<discusy::channel::channel> threads{};
        std::vector<discusy::channel::thread_member> members{};

        static list_active_guild_threads_response create() noexcept {
            return list_active_guild_threads_response{};
        }
        decltype(auto) set_threads(this auto&& self, std::vector<discusy::channel::channel> ths) { self.threads = std::move(ths); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_threads, discusy::channel::channel)
        decltype(auto) add_thread(this auto&& self, discusy::channel::channel th) { self.threads.emplace_back(std::move(th)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_members(this auto&& self, std::vector<discusy::channel::thread_member> mems) { self.members = std::move(mems); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_members, discusy::channel::thread_member)
        decltype(auto) add_member(this auto&& self, discusy::channel::thread_member mem) { self.members.emplace_back(std::move(mem)); return std::forward<decltype(self)>(self); }
    };

    struct list_guild_members_query {
        opt<integer> limit{};
        opt<snowflake> after{};

        static list_guild_members_query create() noexcept {
            return list_guild_members_query{};
        }
        decltype(auto) set_limit(this auto&& self, opt<integer> l) noexcept { self.limit = l; return std::forward<decltype(self)>(self); }
        decltype(auto) set_after(this auto&& self, opt<snowflake> a) noexcept { self.after = a; return std::forward<decltype(self)>(self); }
    };

    struct search_guild_members_query {
        std::string query{};
        opt<integer> limit{};

        static search_guild_members_query create(std::string query_ = {}) {
            return search_guild_members_query{.query = std::move(query_)};
        }
        decltype(auto) set_query(this auto&& self, std::string q) { self.query = std::move(q); return std::forward<decltype(self)>(self); }
        decltype(auto) set_limit(this auto&& self, opt<integer> l) noexcept { self.limit = l; return std::forward<decltype(self)>(self); }
    };

    struct add_guild_member {
        std::string access_token{};
        opt<std::string> nick{};
        opt<std::vector<snowflake>> roles{};
        opt<bool> mute{};
        opt<bool> deaf{};

        static add_guild_member create(std::string token_ = {}) {
            return add_guild_member{.access_token = std::move(token_)};
        }
        decltype(auto) set_access_token(this auto&& self, std::string token_) { self.access_token = std::move(token_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_nick(this auto&& self, opt<std::string> n) { self.nick = std::move(n); return std::forward<decltype(self)>(self); }
        decltype(auto) set_roles(this auto&& self, opt<std::vector<snowflake>> r) { self.roles = std::move(r); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_roles, snowflake)
        decltype(auto) add_role(this auto&& self, snowflake rid) {
            if (!self.roles) self.roles.emplace();
            self.roles->emplace_back(rid);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_mute(this auto&& self, opt<bool> m) noexcept { self.mute = m; return std::forward<decltype(self)>(self); }
        decltype(auto) set_deaf(this auto&& self, opt<bool> d) noexcept { self.deaf = d; return std::forward<decltype(self)>(self); }
    };

    struct modify_guild_member {
        opt<explicit_null<std::string>> nick{};
        opt<explicit_null<std::vector<snowflake>>> roles{};
        opt<explicit_null<bool>> mute{};
        opt<explicit_null<bool>> deaf{};
        opt<explicit_null<snowflake>> channel_id{};
        opt<explicit_null<timestamp>> communication_disabled_until{};
        opt<flags_t<discusy::guild::guild_member_flags>> flags{};

        static modify_guild_member create() noexcept {
            return modify_guild_member{};
        }
        decltype(auto) set_nick(this auto&& self, opt<explicit_null<std::string>> n) { self.nick = std::move(n); return std::forward<decltype(self)>(self); }
        decltype(auto) set_roles(this auto&& self, opt<explicit_null<std::vector<snowflake>>> r) { self.roles = std::move(r); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_roles, snowflake)
        decltype(auto) set_mute(this auto&& self, opt<explicit_null<bool>> m) noexcept { self.mute = m; return std::forward<decltype(self)>(self); }
        decltype(auto) set_deaf(this auto&& self, opt<explicit_null<bool>> d) noexcept { self.deaf = d; return std::forward<decltype(self)>(self); }
        decltype(auto) set_channel_id(this auto&& self, opt<explicit_null<snowflake>> cid) noexcept { self.channel_id = cid; return std::forward<decltype(self)>(self); }
        decltype(auto) set_channel(this auto&& self, snowflake cid) noexcept { return std::forward<decltype(self)>(self).set_channel_id(explicit_null<snowflake>{cid}); }
        decltype(auto) set_communication_disabled_until(this auto&& self, opt<explicit_null<timestamp>> cdu) noexcept { self.communication_disabled_until = cdu; return std::forward<decltype(self)>(self); }
        decltype(auto) set_flags(this auto&& self, opt<flags_t<discusy::guild::guild_member_flags>> f) noexcept { self.flags = f; return std::forward<decltype(self)>(self); }
    };

    struct modify_current_member {
        opt<explicit_null<std::string>> nick{};
        opt<explicit_null<image_data>> banner{};
        opt<explicit_null<image_data>> avatar{};
        opt<explicit_null<std::string>> bio{};

        static modify_current_member create() noexcept {
            return modify_current_member{};
        }
        decltype(auto) set_nick(this auto&& self, opt<explicit_null<std::string>> n) { self.nick = std::move(n); return std::forward<decltype(self)>(self); }
        decltype(auto) set_nick(this auto&& self, std::string n) { self.nick = explicit_null<std::string>{std::move(n)}; return std::forward<decltype(self)>(self); }
        decltype(auto) set_banner(this auto&& self, opt<explicit_null<image_data>> b) { self.banner = std::move(b); return std::forward<decltype(self)>(self); }
        decltype(auto) set_avatar(this auto&& self, opt<explicit_null<image_data>> a) { self.avatar = std::move(a); return std::forward<decltype(self)>(self); }
        decltype(auto) set_bio(this auto&& self, opt<explicit_null<std::string>> b) { self.bio = std::move(b); return std::forward<decltype(self)>(self); }
        decltype(auto) set_bio(this auto&& self, std::string b) { self.bio = explicit_null<std::string>{std::move(b)}; return std::forward<decltype(self)>(self); }
    };

    struct modify_current_user_nick {
        opt<explicit_null<std::string>> nick{};

        static modify_current_user_nick create(opt<std::string> nick_ = {}) {
            modify_current_user_nick m{};
            if (nick_) m.nick = explicit_null<std::string>{*nick_};
            return m;
        }
        decltype(auto) set_nick(this auto&& self, opt<explicit_null<std::string>> n) { self.nick = std::move(n); return std::forward<decltype(self)>(self); }
        decltype(auto) set_nick(this auto&& self, std::string n) { self.nick = explicit_null<std::string>{std::move(n)}; return std::forward<decltype(self)>(self); }
    };

    struct get_guild_bans_query {
        opt<integer> limit{};
        opt<snowflake> before{};
        opt<snowflake> after{};

        static get_guild_bans_query create() noexcept {
            return get_guild_bans_query{};
        }
        decltype(auto) set_limit(this auto&& self, opt<integer> l) noexcept { self.limit = l; return std::forward<decltype(self)>(self); }
        decltype(auto) set_before(this auto&& self, opt<snowflake> b) noexcept { self.before = b; return std::forward<decltype(self)>(self); }
        decltype(auto) set_after(this auto&& self, opt<snowflake> a) noexcept { self.after = a; return std::forward<decltype(self)>(self); }
    };

    struct create_guild_ban {
        opt<integer> delete_message_days{}; // deprecated
        opt<integer> delete_message_seconds{};

        static create_guild_ban create(opt<integer> seconds = {}) noexcept {
            return create_guild_ban{.delete_message_seconds = seconds};
        }
        decltype(auto) set_delete_message_days(this auto&& self, opt<integer> d) noexcept { self.delete_message_days = d; return std::forward<decltype(self)>(self); }
        decltype(auto) set_delete_message_seconds(this auto&& self, opt<integer> s) noexcept { self.delete_message_seconds = s; return std::forward<decltype(self)>(self); }
    };

    struct bulk_guild_ban {
        std::vector<snowflake> user_ids{};
        opt<integer> delete_message_seconds{};

        static bulk_guild_ban create(std::vector<snowflake> ids = {}) {
            return bulk_guild_ban{.user_ids = std::move(ids)};
        }
        decltype(auto) set_user_ids(this auto&& self, std::vector<snowflake> ids) { self.user_ids = std::move(ids); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_user_ids, snowflake)
        decltype(auto) add_user_id(this auto&& self, snowflake uid) { self.user_ids.emplace_back(uid); return std::forward<decltype(self)>(self); }
        decltype(auto) set_delete_message_seconds(this auto&& self, opt<integer> s) noexcept { self.delete_message_seconds = s; return std::forward<decltype(self)>(self); }
    };

    struct bulk_guild_ban_response {
        std::vector<snowflake> banned_users{};
        std::vector<snowflake> failed_users{};

        static bulk_guild_ban_response create() noexcept {
            return bulk_guild_ban_response{};
        }
        decltype(auto) set_banned_users(this auto&& self, std::vector<snowflake> u) { self.banned_users = std::move(u); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_banned_users, snowflake)
        decltype(auto) add_banned_user(this auto&& self, snowflake uid) { self.banned_users.emplace_back(uid); return std::forward<decltype(self)>(self); }
        decltype(auto) set_failed_users(this auto&& self, std::vector<snowflake> u) { self.failed_users = std::move(u); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_failed_users, snowflake)
        decltype(auto) add_failed_user(this auto&& self, snowflake uid) { self.failed_users.emplace_back(uid); return std::forward<decltype(self)>(self); }
    };

    struct create_guild_role {
        opt<std::string> name{};
        opt<permissions_t> permissions{};
        opt<integer> color{}; // deprecated, use colors
        opt<discusy::permissions::role_colors> colors{};
        opt<bool> hoist{};
        opt<explicit_null<image_data>> icon{};
        opt<explicit_null<std::string>> unicode_emoji{};
        opt<bool> mentionable{};

        static create_guild_role create(opt<std::string> name_ = {}, opt<integer> color_ = {}) {
            return create_guild_role{.name = std::move(name_), .color = color_};
        }
        template <typename From>
        static create_guild_role from(From&& r) {
            DISCUSY_FORWARD_IF_SAME(create_guild_role, r);

            static_assert(
                DISCUSY_HAS_FIELD(From, name) ||
                DISCUSY_HAS_FIELD(From, permissions) ||
                DISCUSY_HAS_FIELD(From, color) ||
                DISCUSY_HAS_FIELD(From, colors) ||
                DISCUSY_HAS_FIELD(From, hoist) ||
                DISCUSY_HAS_FIELD(From, unicode_emoji) ||
                DISCUSY_HAS_FIELD(From, mentionable),
                "create_guild_role::from: Source type contains no compatible role fields."
            );

            create_guild_role res{};
            DISCUSY_TRANSFER_FIELD(res, r, name);
            DISCUSY_TRANSFER_FIELD(res, r, permissions);
            DISCUSY_TRANSFER_FIELD(res, r, color);
            DISCUSY_TRANSFER_FIELD(res, r, colors);
            DISCUSY_TRANSFER_FIELD(res, r, hoist);
            DISCUSY_TRANSFER_FIELD(res, r, unicode_emoji);
            DISCUSY_TRANSFER_FIELD(res, r, mentionable);
            return res;

            DISCUSY_END_FROM
        }
        decltype(auto) set_name(this auto&& self, opt<std::string> n) { self.name = std::move(n); return std::forward<decltype(self)>(self); }
        decltype(auto) set_permissions(this auto&& self, opt<permissions_t> p) noexcept { self.permissions = p; return std::forward<decltype(self)>(self); }
        decltype(auto) set_color(this auto&& self, opt<integer> c) noexcept { self.color = c; return std::forward<decltype(self)>(self); }
        decltype(auto) set_colors(this auto&& self, opt<discusy::permissions::role_colors> c) noexcept { self.colors = c; return std::forward<decltype(self)>(self); }
        decltype(auto) set_hoist(this auto&& self, opt<bool> h) noexcept { self.hoist = h; return std::forward<decltype(self)>(self); }
        decltype(auto) set_icon(this auto&& self, opt<explicit_null<image_data>> ic) { self.icon = std::move(ic); return std::forward<decltype(self)>(self); }
        decltype(auto) set_unicode_emoji(this auto&& self, opt<explicit_null<std::string>> ue) { self.unicode_emoji = std::move(ue); return std::forward<decltype(self)>(self); }
        decltype(auto) set_mentionable(this auto&& self, opt<bool> m) noexcept { self.mentionable = m; return std::forward<decltype(self)>(self); }
    };

    struct modify_guild_role_position {
        snowflake id{};
        opt<explicit_null<integer>> position{};

        static modify_guild_role_position create(snowflake id_ = {}, opt<integer> pos = {}) noexcept {
            return modify_guild_role_position{.id = id_, .position = pos ? explicit_null<integer>{*pos} : explicit_null<integer>{}};
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_position(this auto&& self, opt<explicit_null<integer>> pos) noexcept { self.position = pos; return std::forward<decltype(self)>(self); }
    };

    using modify_guild_role_positions = std::span<const modify_guild_role_position>;

    struct modify_guild_role {
        opt<explicit_null<std::string>> name{};
        opt<explicit_null<permissions_t>> permissions{};
        opt<explicit_null<integer>> color{}; // deprecated, use colors
        opt<explicit_null<discusy::permissions::role_colors>> colors{};
        opt<explicit_null<bool>> hoist{};
        opt<explicit_null<image_data>> icon{};
        opt<explicit_null<std::string>> unicode_emoji{};
        opt<explicit_null<bool>> mentionable{};

        static modify_guild_role create() noexcept {
            return modify_guild_role{};
        }
        template <typename From>
        static modify_guild_role from(From&& r) {
            DISCUSY_FORWARD_IF_SAME(modify_guild_role, r);

            static_assert(
                DISCUSY_HAS_FIELD(From, name) ||
                DISCUSY_HAS_FIELD(From, permissions) ||
                DISCUSY_HAS_FIELD(From, color) ||
                DISCUSY_HAS_FIELD(From, colors) ||
                DISCUSY_HAS_FIELD(From, hoist) ||
                DISCUSY_HAS_FIELD(From, unicode_emoji) ||
                DISCUSY_HAS_FIELD(From, mentionable),
                "modify_guild_role::from: Source type contains no compatible role fields."
            );

            modify_guild_role res{};
            DISCUSY_TRANSFER_FIELD(res, r, name);
            DISCUSY_TRANSFER_FIELD(res, r, permissions);
            DISCUSY_TRANSFER_FIELD(res, r, color);
            DISCUSY_TRANSFER_FIELD(res, r, colors);
            DISCUSY_TRANSFER_FIELD(res, r, hoist);
            DISCUSY_TRANSFER_FIELD(res, r, unicode_emoji);
            DISCUSY_TRANSFER_FIELD(res, r, mentionable);
            return res;

            DISCUSY_END_FROM
        }
        decltype(auto) set_name(this auto&& self, opt<explicit_null<std::string>> n) { self.name = std::move(n); return std::forward<decltype(self)>(self); }
        decltype(auto) set_permissions(this auto&& self, opt<explicit_null<permissions_t>> p) noexcept { self.permissions = p; return std::forward<decltype(self)>(self); }
        decltype(auto) set_color(this auto&& self, opt<explicit_null<integer>> c) noexcept { self.color = c; return std::forward<decltype(self)>(self); }
        decltype(auto) set_colors(this auto&& self, opt<explicit_null<discusy::permissions::role_colors>> c) noexcept { self.colors = c; return std::forward<decltype(self)>(self); }
        decltype(auto) set_hoist(this auto&& self, opt<explicit_null<bool>> h) noexcept { self.hoist = h; return std::forward<decltype(self)>(self); }
        decltype(auto) set_icon(this auto&& self, opt<explicit_null<image_data>> ic) { self.icon = std::move(ic); return std::forward<decltype(self)>(self); }
        decltype(auto) set_unicode_emoji(this auto&& self, opt<explicit_null<std::string>> ue) { self.unicode_emoji = std::move(ue); return std::forward<decltype(self)>(self); }
        decltype(auto) set_mentionable(this auto&& self, opt<explicit_null<bool>> m) noexcept { self.mentionable = m; return std::forward<decltype(self)>(self); }
    };

    using role_member_counts = std::unordered_map<std::string, integer>;

    struct get_guild_prune_count_query {
        opt<integer> days{};
        opt<std::string> include_roles{}; // comma-delimited array of snowflakes

        static get_guild_prune_count_query create() noexcept {
            return get_guild_prune_count_query{};
        }
        decltype(auto) set_days(this auto&& self, opt<integer> d) noexcept { self.days = d; return std::forward<decltype(self)>(self); }
        decltype(auto) set_include_roles(this auto&& self, opt<std::string> ir) { self.include_roles = std::move(ir); return std::forward<decltype(self)>(self); }
    };

    struct begin_guild_prune {
        opt<integer> days{};
        opt<bool> compute_prune_count{};
        opt<std::vector<snowflake>> include_roles{};

        static begin_guild_prune create() noexcept {
            return begin_guild_prune{};
        }
        decltype(auto) set_days(this auto&& self, opt<integer> d) noexcept { self.days = d; return std::forward<decltype(self)>(self); }
        decltype(auto) set_compute_prune_count(this auto&& self, opt<bool> cpc) noexcept { self.compute_prune_count = cpc; return std::forward<decltype(self)>(self); }
        decltype(auto) set_include_roles(this auto&& self, opt<std::vector<snowflake>> ir) { self.include_roles = std::move(ir); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_include_roles, snowflake)
        decltype(auto) add_include_role(this auto&& self, snowflake rid) {
            if (!self.include_roles) self.include_roles.emplace();
            self.include_roles->emplace_back(rid);
            return std::forward<decltype(self)>(self);
        }
    };

    struct guild_prune_count_response {
        opt<integer> pruned{}; // null when compute_prune_count is false

        static guild_prune_count_response create(opt<integer> p = {}) noexcept {
            return guild_prune_count_response{.pruned = p};
        }
        decltype(auto) set_pruned(this auto&& self, opt<integer> p) noexcept { self.pruned = p; return std::forward<decltype(self)>(self); }
    };

    struct modify_guild_widget {
        opt<bool> enabled{};
        opt<explicit_null<snowflake>> channel_id{};

        static modify_guild_widget create(opt<bool> en = {}, opt<snowflake> cid = {}) noexcept {
            return modify_guild_widget{.enabled = en, .channel_id = cid ? explicit_null<snowflake>{*cid} : explicit_null<snowflake>{}};
        }
        decltype(auto) set_enabled(this auto&& self, opt<bool> en) noexcept { self.enabled = en; return std::forward<decltype(self)>(self); }
        decltype(auto) set_channel_id(this auto&& self, opt<explicit_null<snowflake>> cid) noexcept { self.channel_id = cid; return std::forward<decltype(self)>(self); }
        decltype(auto) set_channel(this auto&& self, snowflake cid) noexcept { return std::forward<decltype(self)>(self).set_channel_id(explicit_null<snowflake>{cid}); }
    };

    struct get_guild_widget_image_query {
        opt<std::string> style{};

        static get_guild_widget_image_query create(opt<std::string> s = {}) {
            return get_guild_widget_image_query{.style = std::move(s)};
        }
        decltype(auto) set_style(this auto&& self, opt<std::string> s) { self.style = std::move(s); return std::forward<decltype(self)>(self); }
    };

    struct modify_guild_welcome_screen {
        opt<explicit_null<bool>> enabled{};
        opt<explicit_null<std::vector<discusy::guild::welcome_screen_channel>>> welcome_channels{};
        opt<explicit_null<std::string>> description{};

        static modify_guild_welcome_screen create() noexcept {
            return modify_guild_welcome_screen{};
        }
        decltype(auto) set_enabled(this auto&& self, opt<explicit_null<bool>> en) noexcept { self.enabled = en; return std::forward<decltype(self)>(self); }
        decltype(auto) set_welcome_channels(this auto&& self, opt<explicit_null<std::vector<discusy::guild::welcome_screen_channel>>> wc) { self.welcome_channels = std::move(wc); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_welcome_channels, discusy::guild::welcome_screen_channel)
        decltype(auto) add_welcome_channel(this auto&& self, discusy::guild::welcome_screen_channel wc) {
            if (!self.welcome_channels || !self.welcome_channels->has_value()) self.welcome_channels.emplace().emplace();
            (**self.welcome_channels).emplace_back(std::move(wc));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_description(this auto&& self, opt<explicit_null<std::string>> d) { self.description = std::move(d); return std::forward<decltype(self)>(self); }
    };

    struct modify_guild_onboarding {
        std::vector<discusy::guild::onboarding_prompt> prompts{};
        std::vector<snowflake> default_channel_ids{};
        bool enabled{};
        discusy::guild::onboarding_mode mode{};

        static modify_guild_onboarding create(bool enabled_ = false) noexcept {
            return modify_guild_onboarding{.enabled = enabled_};
        }
        decltype(auto) set_prompts(this auto&& self, std::vector<discusy::guild::onboarding_prompt> p) { self.prompts = std::move(p); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_prompts, discusy::guild::onboarding_prompt)
        decltype(auto) add_prompt(this auto&& self, discusy::guild::onboarding_prompt p) { self.prompts.emplace_back(std::move(p)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_default_channel_ids(this auto&& self, std::vector<snowflake> ids) { self.default_channel_ids = std::move(ids); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_default_channel_ids, snowflake)
        decltype(auto) add_default_channel_id(this auto&& self, snowflake cid) { self.default_channel_ids.emplace_back(cid); return std::forward<decltype(self)>(self); }
        decltype(auto) set_enabled(this auto&& self, bool en) noexcept { self.enabled = en; return std::forward<decltype(self)>(self); }
        decltype(auto) set_mode(this auto&& self, discusy::guild::onboarding_mode m) noexcept { self.mode = m; return std::forward<decltype(self)>(self); }
    };

    struct modify_guild_incident_actions {
        opt<explicit_null<timestamp>> invites_disabled_until{};
        opt<explicit_null<timestamp>> dms_disabled_until{};

        static modify_guild_incident_actions create() noexcept {
            return modify_guild_incident_actions{};
        }
        decltype(auto) set_invites_disabled_until(this auto&& self, opt<explicit_null<timestamp>> idu) noexcept { self.invites_disabled_until = idu; return std::forward<decltype(self)>(self); }
        decltype(auto) set_dms_disabled_until(this auto&& self, opt<explicit_null<timestamp>> ddu) noexcept { self.dms_disabled_until = ddu; return std::forward<decltype(self)>(self); }
    };
}

namespace webhook {
    struct get_webhook_message_query {
        opt<snowflake> thread_id{};

        static get_webhook_message_query create(opt<snowflake> tid = {}) noexcept {
            return get_webhook_message_query{.thread_id = tid};
        }
        decltype(auto) set_thread_id(this auto&& self, opt<snowflake> tid) noexcept { self.thread_id = tid; return std::forward<decltype(self)>(self); }
    };

    struct edit_webhook_message_query {
        opt<snowflake> thread_id{};
        opt<bool> with_components{};

        static edit_webhook_message_query create() noexcept {
            return edit_webhook_message_query{};
        }
        decltype(auto) set_thread_id(this auto&& self, opt<snowflake> tid) noexcept { self.thread_id = tid; return std::forward<decltype(self)>(self); }
        decltype(auto) set_with_components(this auto&& self, opt<bool> wc) noexcept { self.with_components = wc; return std::forward<decltype(self)>(self); }
    };

    struct edit_webhook_message {
        opt<explicit_null<std::string>> content{};
        opt<explicit_null<std::vector<discusy::message::embed>>> embeds{};
        opt<explicit_null<flags_t<discusy::message::message_flags>>> flags{};
        opt<explicit_null<message::allowed_mentions_obj>> allowed_mentions{};
        opt<explicit_null<std::vector<discusy::components::component>>> components{};
        opt<explicit_null<std::vector<message::attachment_request>>> attachments{};
        opt<explicit_null<discusy::poll::poll_create_request>> poll{};

        static edit_webhook_message create(opt<std::string> content_ = {}) {
            edit_webhook_message req{};
            if (content_) req.content = explicit_null<std::string>{*content_};
            return req;
        }

        template <typename From>
        static edit_webhook_message from(From&& msg) {
            DISCUSY_FORWARD_IF_SAME(edit_webhook_message, msg);

            static_assert(
                DISCUSY_HAS_FIELD(From, content) ||
                DISCUSY_HAS_FIELD(From, embeds) ||
                DISCUSY_HAS_FIELD(From, flags) ||
                DISCUSY_HAS_FIELD(From, components),
                "edit_webhook_message::from: Source type contains no compatible message fields."
            );

            edit_webhook_message req{};
            DISCUSY_TRANSFER_FIELD(req, msg, content);
            DISCUSY_TRANSFER_FIELD(req, msg, embeds);
            DISCUSY_TRANSFER_FIELD(req, msg, flags);
            DISCUSY_TRANSFER_FIELD(req, msg, components);
            return req;

            DISCUSY_END_FROM
        }

        decltype(auto) set_content(this auto&& self, opt<explicit_null<std::string>> c) { self.content = std::move(c); return std::forward<decltype(self)>(self); }
        decltype(auto) set_content(this auto&& self, std::string c) { self.content = explicit_null<std::string>{std::move(c)}; return std::forward<decltype(self)>(self); }
        decltype(auto) set_embeds(this auto&& self, opt<explicit_null<std::vector<discusy::message::embed>>> embs) { self.embeds = std::move(embs); return std::forward<decltype(self)>(self); }
        decltype(auto) set_embeds(this auto&& self, std::vector<discusy::message::embed> embs) { self.embeds = explicit_null<std::vector<discusy::message::embed>>{std::move(embs)}; return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_embeds, discusy::message::embed)
        decltype(auto) add_embed(this auto&& self, discusy::message::embed emb) {
            if (!self.embeds || !self.embeds->has_value()) self.embeds.emplace().emplace();
            (**self.embeds).emplace_back(std::move(emb));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_flags(this auto&& self, opt<explicit_null<flags_t<discusy::message::message_flags>>> f) noexcept { self.flags = f; return std::forward<decltype(self)>(self); }
        decltype(auto) set_flags(this auto&& self, flags_t<discusy::message::message_flags> f) noexcept { self.flags = explicit_null<flags_t<discusy::message::message_flags>>{f}; return std::forward<decltype(self)>(self); }
        decltype(auto) add_flags(this auto&& self, discusy::message::message_flags f) noexcept {
            if (!self.flags || !self.flags->has_value()) self.flags.emplace().emplace();
            (**self.flags).add_flags(f);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) add_flag(this auto&& self, discusy::message::message_flags f) noexcept { return std::forward<decltype(self)>(self).add_flags(f); }
        decltype(auto) set_allowed_mentions(this auto&& self, opt<explicit_null<message::allowed_mentions_obj>> am) { self.allowed_mentions = std::move(am); return std::forward<decltype(self)>(self); }
        decltype(auto) set_allowed_mentions(this auto&& self, message::allowed_mentions_obj am) { self.allowed_mentions = explicit_null<message::allowed_mentions_obj>{std::move(am)}; return std::forward<decltype(self)>(self); }
        decltype(auto) set_components(this auto&& self, opt<explicit_null<std::vector<discusy::components::component>>> comps) {
            self.components = std::move(comps);
            if (self.components && self.components->has_value() && !(*self.components)->empty()) {
                self.add_flags(discusy::message::message_flags::IS_COMPONENTS_V2);
            }
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_components(this auto&& self, std::vector<discusy::components::component> comps) {
            const bool not_empty = !comps.empty();
            self.components = explicit_null<std::vector<discusy::components::component>>{std::move(comps)};
            if (not_empty) {
                self.add_flags(discusy::message::message_flags::IS_COMPONENTS_V2);
            }
            return std::forward<decltype(self)>(self);
        }
        DISCUSY_VARIADIC_SETTER(set_components, discusy::components::component)
        decltype(auto) add_component(this auto&& self, discusy::components::component comp) {
            if (!self.components || !self.components->has_value()) self.components.emplace().emplace();
            (**self.components).emplace_back(std::move(comp));
            self.add_flags(discusy::message::message_flags::IS_COMPONENTS_V2);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) add_action_row(this auto&& self, discusy::components::ActionRow row) {
            return std::forward<decltype(self)>(self).add_component(std::move(row));
        }
        decltype(auto) add_container(this auto&& self, discusy::components::Container container) {
            return std::forward<decltype(self)>(self).add_component(std::move(container));
        }
        decltype(auto) add_section(this auto&& self, discusy::components::Section section) {
            return std::forward<decltype(self)>(self).add_component(std::move(section));
        }
        decltype(auto) add_text_display(this auto&& self, discusy::components::TextDisplay td) {
            return std::forward<decltype(self)>(self).add_component(std::move(td));
        }
        decltype(auto) add_text_display(this auto&& self, std::string text) {
            return std::forward<decltype(self)>(self).add_component(discusy::components::TextDisplay::create(std::move(text)));
        }
        decltype(auto) add_media_gallery(this auto&& self, discusy::components::MediaGallery mg) {
            return std::forward<decltype(self)>(self).add_component(std::move(mg));
        }
        decltype(auto) add_file(this auto&& self, discusy::components::File f) {
            return std::forward<decltype(self)>(self).add_component(std::move(f));
        }
        decltype(auto) add_separator(this auto&& self, discusy::components::Separator sep = discusy::components::Separator::create()) {
            return std::forward<decltype(self)>(self).add_component(std::move(sep));
        }
        decltype(auto) set_attachments(this auto&& self, opt<explicit_null<std::vector<message::attachment_request>>> atts) { self.attachments = std::move(atts); return std::forward<decltype(self)>(self); }
        decltype(auto) set_attachments(this auto&& self, std::vector<message::attachment_request> atts) { self.attachments = explicit_null<std::vector<message::attachment_request>>{std::move(atts)}; return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_attachments, message::attachment_request)
        decltype(auto) add_attachment(this auto&& self, message::attachment_request att) {
            if (!self.attachments || !self.attachments->has_value()) self.attachments.emplace().emplace();
            (**self.attachments).emplace_back(std::move(att));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_poll(this auto&& self, opt<explicit_null<discusy::poll::poll_create_request>> p) { self.poll = std::move(p); return std::forward<decltype(self)>(self); }
        decltype(auto) set_poll(this auto&& self, discusy::poll::poll_create_request p) { self.poll = explicit_null<discusy::poll::poll_create_request>{std::move(p)}; return std::forward<decltype(self)>(self); }
    };

    struct execute_webhook_query {
        opt<bool> wait{};
        opt<snowflake> thread_id{};
        opt<bool> with_components{};

        static execute_webhook_query create() noexcept {
            return execute_webhook_query{};
        }
        decltype(auto) set_wait(this auto&& self, opt<bool> w) noexcept { self.wait = w; return std::forward<decltype(self)>(self); }
        decltype(auto) set_thread_id(this auto&& self, opt<snowflake> tid) noexcept { self.thread_id = tid; return std::forward<decltype(self)>(self); }
        decltype(auto) set_with_components(this auto&& self, opt<bool> wc) noexcept { self.with_components = wc; return std::forward<decltype(self)>(self); }
    };

    struct execute_webhook {
        opt<explicit_null<std::string>> content{};
        opt<std::string> username{};
        opt<std::string> avatar_url{};
        opt<bool> tts{};
        opt<explicit_null<std::vector<discusy::message::embed>>> embeds{};
        opt<explicit_null<message::allowed_mentions_obj>> allowed_mentions{};
        opt<explicit_null<std::vector<discusy::components::component>>> components{};
        opt<explicit_null<std::vector<message::attachment_request>>> attachments{};
        opt<explicit_null<flags_t<discusy::message::message_flags>>> flags{};
        opt<std::string> thread_name{};
        opt<std::vector<snowflake>> applied_tags{};
        opt<explicit_null<discusy::poll::poll_create_request>> poll{};

        static execute_webhook create(opt<std::string> content_ = {}) {
            execute_webhook req{};
            if (content_) req.content = explicit_null<std::string>{*content_};
            return req;
        }

        template <typename From>
        static execute_webhook from(From&& msg) {
            DISCUSY_FORWARD_IF_SAME(execute_webhook, msg);

            static_assert(
                DISCUSY_HAS_FIELD(From, content) ||
                DISCUSY_HAS_FIELD(From, tts) ||
                DISCUSY_HAS_FIELD(From, embeds) ||
                DISCUSY_HAS_FIELD(From, flags) ||
                DISCUSY_HAS_FIELD(From, components),
                "execute_webhook::from: Source type contains no compatible message fields."
            );

            execute_webhook req{};
            DISCUSY_TRANSFER_FIELD(req, msg, content);
            DISCUSY_TRANSFER_FIELD(req, msg, tts);
            DISCUSY_TRANSFER_FIELD(req, msg, embeds);
            DISCUSY_TRANSFER_FIELD(req, msg, flags);
            DISCUSY_TRANSFER_FIELD(req, msg, components);
            return req;

            DISCUSY_END_FROM
        }

        decltype(auto) set_content(this auto&& self, opt<explicit_null<std::string>> c) { self.content = std::move(c); return std::forward<decltype(self)>(self); }
        decltype(auto) set_content(this auto&& self, std::string c) { self.content = explicit_null<std::string>{std::move(c)}; return std::forward<decltype(self)>(self); }
        decltype(auto) set_username(this auto&& self, opt<std::string> u) { self.username = std::move(u); return std::forward<decltype(self)>(self); }
        decltype(auto) set_avatar_url(this auto&& self, opt<std::string> a) { self.avatar_url = std::move(a); return std::forward<decltype(self)>(self); }
        decltype(auto) set_tts(this auto&& self, opt<bool> t) noexcept { self.tts = t; return std::forward<decltype(self)>(self); }
        decltype(auto) set_embeds(this auto&& self, opt<explicit_null<std::vector<discusy::message::embed>>> embs) { self.embeds = std::move(embs); return std::forward<decltype(self)>(self); }
        decltype(auto) set_embeds(this auto&& self, std::vector<discusy::message::embed> embs) { self.embeds = explicit_null<std::vector<discusy::message::embed>>{std::move(embs)}; return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_embeds, discusy::message::embed)
        decltype(auto) add_embed(this auto&& self, discusy::message::embed emb) {
            if (!self.embeds || !self.embeds->has_value()) self.embeds.emplace().emplace();
            (**self.embeds).emplace_back(std::move(emb));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_allowed_mentions(this auto&& self, opt<explicit_null<message::allowed_mentions_obj>> am) { self.allowed_mentions = std::move(am); return std::forward<decltype(self)>(self); }
        decltype(auto) set_allowed_mentions(this auto&& self, message::allowed_mentions_obj am) { self.allowed_mentions = explicit_null<message::allowed_mentions_obj>{std::move(am)}; return std::forward<decltype(self)>(self); }
        decltype(auto) set_components(this auto&& self, opt<explicit_null<std::vector<discusy::components::component>>> comps) {
            self.components = std::move(comps);
            if (self.components && self.components->has_value() && !(*self.components)->empty()) {
                self.add_flags(discusy::message::message_flags::IS_COMPONENTS_V2);
            }
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_components(this auto&& self, std::vector<discusy::components::component> comps) {
            const bool not_empty = !comps.empty();
            self.components = explicit_null<std::vector<discusy::components::component>>{std::move(comps)};
            if (not_empty) {
                self.add_flags(discusy::message::message_flags::IS_COMPONENTS_V2);
            }
            return std::forward<decltype(self)>(self);
        }
        DISCUSY_VARIADIC_SETTER(set_components, discusy::components::component)
        decltype(auto) add_component(this auto&& self, discusy::components::component comp) {
            if (!self.components || !self.components->has_value()) self.components.emplace().emplace();
            (**self.components).emplace_back(std::move(comp));
            self.add_flags(discusy::message::message_flags::IS_COMPONENTS_V2);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) add_action_row(this auto&& self, discusy::components::ActionRow row) {
            return std::forward<decltype(self)>(self).add_component(std::move(row));
        }
        decltype(auto) add_container(this auto&& self, discusy::components::Container container) {
            return std::forward<decltype(self)>(self).add_component(std::move(container));
        }
        decltype(auto) add_section(this auto&& self, discusy::components::Section section) {
            return std::forward<decltype(self)>(self).add_component(std::move(section));
        }
        decltype(auto) add_text_display(this auto&& self, discusy::components::TextDisplay td) {
            return std::forward<decltype(self)>(self).add_component(std::move(td));
        }
        decltype(auto) add_text_display(this auto&& self, std::string text) {
            return std::forward<decltype(self)>(self).add_component(discusy::components::TextDisplay::create(std::move(text)));
        }
        decltype(auto) add_media_gallery(this auto&& self, discusy::components::MediaGallery mg) {
            return std::forward<decltype(self)>(self).add_component(std::move(mg));
        }
        decltype(auto) add_file(this auto&& self, discusy::components::File f) {
            return std::forward<decltype(self)>(self).add_component(std::move(f));
        }
        decltype(auto) add_separator(this auto&& self, discusy::components::Separator sep = discusy::components::Separator::create()) {
            return std::forward<decltype(self)>(self).add_component(std::move(sep));
        }
        decltype(auto) set_attachments(this auto&& self, opt<explicit_null<std::vector<message::attachment_request>>> atts) { self.attachments = std::move(atts); return std::forward<decltype(self)>(self); }
        decltype(auto) set_attachments(this auto&& self, std::vector<message::attachment_request> atts) { self.attachments = explicit_null<std::vector<message::attachment_request>>{std::move(atts)}; return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_attachments, message::attachment_request)
        decltype(auto) add_attachment(this auto&& self, message::attachment_request att) {
            if (!self.attachments || !self.attachments->has_value()) self.attachments.emplace().emplace();
            (**self.attachments).emplace_back(std::move(att));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_flags(this auto&& self, opt<explicit_null<flags_t<discusy::message::message_flags>>> f) noexcept { self.flags = f; return std::forward<decltype(self)>(self); }
        decltype(auto) set_flags(this auto&& self, flags_t<discusy::message::message_flags> f) noexcept { self.flags = explicit_null<flags_t<discusy::message::message_flags>>{f}; return std::forward<decltype(self)>(self); }
        decltype(auto) add_flags(this auto&& self, discusy::message::message_flags f) noexcept {
            if (!self.flags || !self.flags->has_value()) self.flags.emplace().emplace();
            (**self.flags).add_flags(f);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) add_flag(this auto&& self, discusy::message::message_flags f) noexcept { return std::forward<decltype(self)>(self).add_flags(f); }
        decltype(auto) set_thread_name(this auto&& self, opt<std::string> tn) { self.thread_name = std::move(tn); return std::forward<decltype(self)>(self); }
        decltype(auto) set_applied_tags(this auto&& self, opt<std::vector<snowflake>> tags) { self.applied_tags = std::move(tags); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_applied_tags, snowflake)
        decltype(auto) add_applied_tag(this auto&& self, snowflake tag) {
            if (!self.applied_tags) self.applied_tags.emplace();
            self.applied_tags->emplace_back(tag);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_poll(this auto&& self, opt<explicit_null<discusy::poll::poll_create_request>> p) { self.poll = std::move(p); return std::forward<decltype(self)>(self); }
        decltype(auto) set_poll(this auto&& self, discusy::poll::poll_create_request p) { self.poll = explicit_null<discusy::poll::poll_create_request>{std::move(p)}; return std::forward<decltype(self)>(self); }
    };

    struct create_webhook {
        std::string name{};
        opt<explicit_null<image_data>> avatar{};

        static create_webhook create(std::string name_ = {}) {
            return create_webhook{.name = std::move(name_)};
        }
        decltype(auto) set_name(this auto&& self, std::string name_) { self.name = std::move(name_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_avatar(this auto&& self, opt<explicit_null<image_data>> a) { self.avatar = std::move(a); return std::forward<decltype(self)>(self); }
    };

    struct modify_webhook {
        opt<std::string> name{};
        opt<explicit_null<image_data>> avatar{};
        opt<snowflake> channel_id{};

        static modify_webhook create() noexcept {
            return modify_webhook{};
        }
        decltype(auto) set_name(this auto&& self, opt<std::string> n) { self.name = std::move(n); return std::forward<decltype(self)>(self); }
        decltype(auto) set_avatar(this auto&& self, opt<explicit_null<image_data>> a) { self.avatar = std::move(a); return std::forward<decltype(self)>(self); }
        decltype(auto) set_channel_id(this auto&& self, opt<snowflake> cid) noexcept { self.channel_id = cid; return std::forward<decltype(self)>(self); }
        decltype(auto) set_channel(this auto&& self, snowflake cid) noexcept { return std::forward<decltype(self)>(self).set_channel_id(cid); }
    };

    struct modify_webhook_with_token {
        opt<std::string> name{};
        opt<explicit_null<image_data>> avatar{};

        static modify_webhook_with_token create() noexcept {
            return modify_webhook_with_token{};
        }
        decltype(auto) set_name(this auto&& self, opt<std::string> n) { self.name = std::move(n); return std::forward<decltype(self)>(self); }
        decltype(auto) set_avatar(this auto&& self, opt<explicit_null<image_data>> a) { self.avatar = std::move(a); return std::forward<decltype(self)>(self); }
    };

    struct execute_compatible_webhook_query {
        opt<snowflake> thread_id{};
        opt<bool> wait{};

        static execute_compatible_webhook_query create() noexcept {
            return execute_compatible_webhook_query{};
        }
        decltype(auto) set_thread_id(this auto&& self, opt<snowflake> tid) noexcept { self.thread_id = tid; return std::forward<decltype(self)>(self); }
        decltype(auto) set_wait(this auto&& self, opt<bool> w) noexcept { self.wait = w; return std::forward<decltype(self)>(self); }
    };

    struct delete_webhook_message_query {
        opt<snowflake> thread_id{};

        static delete_webhook_message_query create(opt<snowflake> tid = {}) noexcept {
            return delete_webhook_message_query{.thread_id = tid};
        }
        decltype(auto) set_thread_id(this auto&& self, opt<snowflake> tid) noexcept { self.thread_id = tid; return std::forward<decltype(self)>(self); }
    };
}

namespace interaction {
    enum class interaction_callback_type : std::uint8_t {
        PONG = 1, // ACK a Ping
        CHANNEL_MESSAGE_WITH_SOURCE = 4, // Respond to an interaction with a message
        DEFERRED_CHANNEL_MESSAGE_WITH_SOURCE = 5, // ACK an interaction and edit a response later, the user sees a loading state
        DEFERRED_UPDATE_MESSAGE = 6, // For components, ACK an interaction and edit the original message later; the user does not see a loading state
        UPDATE_MESSAGE = 7, // For components, edit the message the component was attached to
        APPLICATION_COMMAND_AUTOCOMPLETE_RESULT = 8, // Respond to an autocomplete interaction with suggested choices
        MODAL = 9, // Respond to an interaction with a popup modal
        PREMIUM_REQUIRED = 10, // Deprecated; respond to an interaction with an upgrade button, only available for apps with monetization enabled
        LAUNCH_ACTIVITY = 12, // Launch the Activity associated with the app. Only available for apps with Activities enabled
    };

    struct interaction_callback_data_message {
        opt<bool> tts{};
        opt<std::string> content{};
        opt<std::vector<discusy::message::embed>> embeds{};
        opt<message::allowed_mentions_obj> allowed_mentions{};
        opt<flags_t<discusy::message::message_flags>> flags{};
        opt<std::vector<discusy::components::component>> components{};
        opt<std::vector<message::attachment_request>> attachments{};
        opt<discusy::poll::poll_create_request> poll{};

        decltype(auto) set_tts(this auto&& self, opt<bool> t) noexcept { self.tts = t; return std::forward<decltype(self)>(self); }
        decltype(auto) set_content(this auto&& self, opt<std::string> c) { self.content = std::move(c); return std::forward<decltype(self)>(self); }
        decltype(auto) set_embeds(this auto&& self, opt<std::vector<discusy::message::embed>> embs) { self.embeds = std::move(embs); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_embeds, discusy::message::embed)
        decltype(auto) add_embed(this auto&& self, discusy::message::embed emb) {
            if (!self.embeds) self.embeds.emplace();
            self.embeds->emplace_back(std::move(emb));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_allowed_mentions(this auto&& self, opt<message::allowed_mentions_obj> am) { self.allowed_mentions = std::move(am); return std::forward<decltype(self)>(self); }
        decltype(auto) set_flags(this auto&& self, opt<flags_t<discusy::message::message_flags>> f) noexcept { self.flags = f; return std::forward<decltype(self)>(self); }
        decltype(auto) add_flags(this auto&& self, discusy::message::message_flags f) noexcept {
            if (!self.flags) self.flags.emplace();
            self.flags->add_flags(f);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) add_flag(this auto&& self, discusy::message::message_flags f) noexcept { return std::forward<decltype(self)>(self).add_flags(f); }
        decltype(auto) set_ephemeral(this auto&& self, bool eph = true) noexcept {
            if (eph) self.add_flags(discusy::message::message_flags::EPHEMERAL);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_components(this auto&& self, opt<std::vector<discusy::components::component>> comps) {
            self.components = std::move(comps);
            if (self.components && !self.components->empty()) {
                self.add_flags(discusy::message::message_flags::IS_COMPONENTS_V2);
            }
            return std::forward<decltype(self)>(self);
        }
        DISCUSY_VARIADIC_SETTER(set_components, discusy::components::component)
        decltype(auto) add_component(this auto&& self, discusy::components::component comp) {
            if (!self.components) self.components.emplace();
            self.components->emplace_back(std::move(comp));
            self.add_flags(discusy::message::message_flags::IS_COMPONENTS_V2);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) add_action_row(this auto&& self, discusy::components::ActionRow row) {
            return std::forward<decltype(self)>(self).add_component(std::move(row));
        }
        decltype(auto) add_container(this auto&& self, discusy::components::Container container) {
            return std::forward<decltype(self)>(self).add_component(std::move(container));
        }
        decltype(auto) add_section(this auto&& self, discusy::components::Section section) {
            return std::forward<decltype(self)>(self).add_component(std::move(section));
        }
        decltype(auto) add_text_display(this auto&& self, discusy::components::TextDisplay td) {
            return std::forward<decltype(self)>(self).add_component(std::move(td));
        }
        decltype(auto) add_text_display(this auto&& self, std::string text) {
            return std::forward<decltype(self)>(self).add_component(discusy::components::TextDisplay::create(std::move(text)));
        }
        decltype(auto) add_media_gallery(this auto&& self, discusy::components::MediaGallery mg) {
            return std::forward<decltype(self)>(self).add_component(std::move(mg));
        }
        decltype(auto) add_file(this auto&& self, discusy::components::File f) {
            return std::forward<decltype(self)>(self).add_component(std::move(f));
        }
        decltype(auto) add_separator(this auto&& self, discusy::components::Separator sep = discusy::components::Separator::create()) {
            return std::forward<decltype(self)>(self).add_component(std::move(sep));
        }
        decltype(auto) set_attachments(this auto&& self, opt<std::vector<message::attachment_request>> atts) { self.attachments = std::move(atts); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_attachments, message::attachment_request)
        decltype(auto) add_attachment(this auto&& self, message::attachment_request att) {
            if (!self.attachments) self.attachments.emplace();
            self.attachments->emplace_back(std::move(att));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_poll(this auto&& self, opt<discusy::poll::poll_create_request> p) { self.poll = std::move(p); return std::forward<decltype(self)>(self); }


        static interaction_callback_data_message create(opt<std::string> content_ = {}, bool ephemeral = false) {
            interaction_callback_data_message m{};
            if (content_) m.content = std::move(content_);
            if (ephemeral) m.set_ephemeral(true);
            return m;
        }

        template <typename From>
        static interaction_callback_data_message from(From&& msg, bool ephemeral = false) {
            if constexpr (std::is_same_v<std::remove_cvref_t<From>, interaction_callback_data_message>) {
                auto res = std::forward<From>(msg);
                if (ephemeral) res.set_ephemeral(true);
                return res;
            } else {
                static_assert(
                    DISCUSY_HAS_FIELD(From, content) ||
                    DISCUSY_HAS_FIELD(From, tts) ||
                    DISCUSY_HAS_FIELD(From, embeds) ||
                    DISCUSY_HAS_FIELD(From, flags) ||
                    DISCUSY_HAS_FIELD(From, components),
                    "interaction_callback_data_message::from: Source type contains no compatible message fields."
                );

                interaction_callback_data_message m{};
                DISCUSY_TRANSFER_FIELD(m, msg, content);
                DISCUSY_TRANSFER_FIELD(m, msg, tts);
                DISCUSY_TRANSFER_FIELD(m, msg, embeds);
                DISCUSY_TRANSFER_FIELD(m, msg, flags);
                DISCUSY_TRANSFER_FIELD(m, msg, components);
                if (ephemeral) m.set_ephemeral(true);
                return m;
            }
        }
    };

    struct interaction_callback_data_autocomplete {
        std::vector<discusy::application_commands::application_command_option_choice> choices{};

        static interaction_callback_data_autocomplete create(std::vector<discusy::application_commands::application_command_option_choice> ch = {}) {
            return interaction_callback_data_autocomplete{.choices = std::move(ch)};
        }
        decltype(auto) set_choices(this auto&& self, std::vector<discusy::application_commands::application_command_option_choice> ch) { self.choices = std::move(ch); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_choices, discusy::application_commands::application_command_option_choice)
        decltype(auto) add_choice(this auto&& self, discusy::application_commands::application_command_option_choice ch) { self.choices.emplace_back(std::move(ch)); return std::forward<decltype(self)>(self); }
    };

    struct interaction_callback_data_modal {
        std::string custom_id{};
        std::string title{};
        std::vector<discusy::components::component> components{};

        static interaction_callback_data_modal create(std::string custom_id_ = {}, std::string title_ = {}) {
            return interaction_callback_data_modal{.custom_id = std::move(custom_id_), .title = std::move(title_)};
        }
        decltype(auto) set_custom_id(this auto&& self, std::string cid) { self.custom_id = std::move(cid); return std::forward<decltype(self)>(self); }
        decltype(auto) set_title(this auto&& self, std::string t) { self.title = std::move(t); return std::forward<decltype(self)>(self); }
        decltype(auto) set_components(this auto&& self, std::vector<discusy::components::component> comps) { self.components = std::move(comps); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_components, discusy::components::component)
        decltype(auto) add_component(this auto&& self, discusy::components::component comp) { self.components.emplace_back(std::move(comp)); return std::forward<decltype(self)>(self); }
        decltype(auto) add_action_row(this auto&& self, discusy::components::ActionRow row) {
            return std::forward<decltype(self)>(self).add_component(std::move(row));
        }
        decltype(auto) add_label(this auto&& self, discusy::components::Label lbl) {
            return std::forward<decltype(self)>(self).add_component(std::move(lbl));
        }
        decltype(auto) add_text_display(this auto&& self, discusy::components::TextDisplay td) {
            return std::forward<decltype(self)>(self).add_component(std::move(td));
        }
        decltype(auto) add_text_display(this auto&& self, std::string text) {
            return std::forward<decltype(self)>(self).add_component(discusy::components::TextDisplay::create(std::move(text)));
        }
    };

    using interaction_callback_data = std::variant<
        interaction_callback_data_message,
        interaction_callback_data_autocomplete,
        interaction_callback_data_modal
    >;

    struct interaction_response {
        interaction_callback_type type{};
        opt<interaction_callback_data> data{};

        static interaction_response create(interaction_callback_type type_ = interaction_callback_type::CHANNEL_MESSAGE_WITH_SOURCE) noexcept {
            return interaction_response{.type = type_};
        }
        static  interaction_response message(std::string content, bool ephemeral = false) {
            return interaction_response{
                .type = interaction_callback_type::CHANNEL_MESSAGE_WITH_SOURCE,
                .data = interaction_callback_data_message::create(std::move(content), ephemeral),
            };
        }
        static interaction_response embed(discusy::message::embed e, bool ephemeral = false) {
            interaction_callback_data_message data_msg{};
            data_msg.add_embed(std::move(e));
            if (ephemeral) data_msg.set_ephemeral(true);
            return interaction_response{
                .type = interaction_callback_type::CHANNEL_MESSAGE_WITH_SOURCE,
                .data = std::move(data_msg),
            };
        }
        template <typename From>
        static interaction_response from(From&& msg, bool ephemeral = false) {
            if constexpr (std::is_same_v<std::remove_cvref_t<From>, interaction_response>) {
                return std::forward<From>(msg);
            } else {
                return interaction_response{
                    .type = interaction_callback_type::CHANNEL_MESSAGE_WITH_SOURCE,
                    .data = interaction_callback_data_message::from(std::forward<From>(msg), ephemeral),
                };
            }
        }
        static interaction_response pong() noexcept {
            return interaction_response{.type = interaction_callback_type::PONG};
        }
        static interaction_response defer(bool ephemeral = false) {
            interaction_response res{.type = interaction_callback_type::DEFERRED_CHANNEL_MESSAGE_WITH_SOURCE};
            if (ephemeral) {
                interaction_callback_data_message d{};
                d.set_ephemeral(true);
                res.data = std::move(d);
            }
            return res;
        }
        static interaction_response defer_update() noexcept {
            return interaction_response{.type = interaction_callback_type::DEFERRED_UPDATE_MESSAGE};
        }

        decltype(auto) set_type(this auto&& self, interaction_callback_type t) noexcept { self.type = t; return std::forward<decltype(self)>(self); }
        decltype(auto) set_data(this auto&& self, opt<interaction_callback_data> d) { self.data = std::move(d); return std::forward<decltype(self)>(self); }
    };

    struct interaction_callback_object {
        snowflake id{};
        discusy::interaction::interaction_type type{};
        opt<std::string> activity_instance_id{};
        opt<snowflake> response_message_id{};
        opt<bool> response_message_loading{};
        opt<bool> response_message_ephemeral{};

        static interaction_callback_object create(snowflake id_ = {}) noexcept {
            return interaction_callback_object{.id = id_};
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_type(this auto&& self, discusy::interaction::interaction_type t) noexcept { self.type = t; return std::forward<decltype(self)>(self); }
        decltype(auto) set_activity_instance_id(this auto&& self, opt<std::string> aid) { self.activity_instance_id = std::move(aid); return std::forward<decltype(self)>(self); }
        decltype(auto) set_response_message_id(this auto&& self, opt<snowflake> mid) noexcept { self.response_message_id = mid; return std::forward<decltype(self)>(self); }
        decltype(auto) set_response_message_loading(this auto&& self, opt<bool> rml) noexcept { self.response_message_loading = rml; return std::forward<decltype(self)>(self); }
        decltype(auto) set_response_message_ephemeral(this auto&& self, opt<bool> rme) noexcept { self.response_message_ephemeral = rme; return std::forward<decltype(self)>(self); }
    };

    struct interaction_callback_activity_instance_resource {
        std::string id{};

        static interaction_callback_activity_instance_resource create(std::string id_ = {}) {
            return interaction_callback_activity_instance_resource{.id = std::move(id_)};
        }
        decltype(auto) set_id(this auto&& self, std::string id_) { self.id = std::move(id_); return std::forward<decltype(self)>(self); }
    };

    struct interaction_callback_resource {
        interaction_callback_type type{};
        opt<interaction_callback_activity_instance_resource> activity_instance{};
        opt<discusy::message::message> message{};

        static interaction_callback_resource create(interaction_callback_type t = interaction_callback_type::CHANNEL_MESSAGE_WITH_SOURCE) noexcept {
            return interaction_callback_resource{.type = t};
        }
        decltype(auto) set_type(this auto&& self, interaction_callback_type t) noexcept { self.type = t; return std::forward<decltype(self)>(self); }
        decltype(auto) set_activity_instance(this auto&& self, opt<interaction_callback_activity_instance_resource> ai) { self.activity_instance = std::move(ai); return std::forward<decltype(self)>(self); }
        decltype(auto) set_message(this auto&& self, opt<discusy::message::message> msg) { self.message = std::move(msg); return std::forward<decltype(self)>(self); }
    };

    struct interaction_callback_response {
        interaction_callback_object interaction{};
        opt<interaction_callback_resource> resource{};

        static interaction_callback_response create() noexcept {
            return interaction_callback_response{};
        }
        decltype(auto) set_interaction(this auto&& self, interaction_callback_object inter) { self.interaction = std::move(inter); return std::forward<decltype(self)>(self); }
        decltype(auto) set_resource(this auto&& self, opt<interaction_callback_resource> res) { self.resource = std::move(res); return std::forward<decltype(self)>(self); }
    };

    struct create_interaction_response_query {
        opt<bool> with_response{};

        static create_interaction_response_query create(opt<bool> wr = {}) noexcept {
            return create_interaction_response_query{.with_response = wr};
        }
        decltype(auto) set_with_response(this auto&& self, opt<bool> wr) noexcept { self.with_response = wr; return std::forward<decltype(self)>(self); }
    };
}

namespace application_commands {
    struct application_command {
        opt<snowflake> id{}; // only used by bulk overwrite
        std::string name{};
        opt<std::unordered_map<std::string, std::string>> name_localizations{};
        opt<std::string> description{};
        opt<std::unordered_map<std::string, std::string>> description_localizations{};
        opt<std::vector<discusy::application_commands::application_command_option>> options{};
        opt<permissions_t> default_member_permissions{};
        opt<bool> dm_permission{}; // deprecated
        opt<bool> default_permission{}; // pretty much deprecated
        opt<std::vector<discusy::application::application_integration_type>> integration_types{};
        opt<std::unordered_set<discusy::interaction::interaction_context_type>> contexts{};
        opt<discusy::application_commands::application_command_type> type{};
        opt<bool> nsfw{};
        opt<discusy::application_commands::entry_point_command_handler_types> handler{};

        static application_command create(std::string name_ = {}, opt<std::string> description_ = {}, opt<discusy::application_commands::application_command_type> type_ = discusy::application_commands::application_command_type::CHAT_INPUT) {
            return application_command{.name = std::move(name_), .description = std::move(description_), .type = type_};
        }
        template <typename From>
        static application_command from(From&& cmd) {
            DISCUSY_FORWARD_IF_SAME(application_command, cmd);

            static_assert(
                DISCUSY_HAS_FIELD(From, id) ||
                DISCUSY_HAS_FIELD(From, name) ||
                DISCUSY_HAS_FIELD(From, name_localizations) ||
                DISCUSY_HAS_FIELD(From, description) ||
                DISCUSY_HAS_FIELD(From, description_localizations) ||
                DISCUSY_HAS_FIELD(From, options) ||
                DISCUSY_HAS_FIELD(From, default_member_permissions) ||
                DISCUSY_HAS_FIELD(From, dm_permission) ||
                DISCUSY_HAS_FIELD(From, default_permission) ||
                DISCUSY_HAS_FIELD(From, integration_types) ||
                DISCUSY_HAS_FIELD(From, contexts) ||
                DISCUSY_HAS_FIELD(From, type) ||
                DISCUSY_HAS_FIELD(From, nsfw) ||
                DISCUSY_HAS_FIELD(From, handler),
                "application_command::from: Source type contains no compatible application command fields."
            );

            application_command c{};
            DISCUSY_TRANSFER_FIELD(c, cmd, id);
            DISCUSY_TRANSFER_FIELD(c, cmd, name);
            DISCUSY_TRANSFER_FIELD(c, cmd, name_localizations);
            DISCUSY_TRANSFER_FIELD(c, cmd, description);
            DISCUSY_TRANSFER_FIELD(c, cmd, description_localizations);
            DISCUSY_TRANSFER_FIELD(c, cmd, options);
            DISCUSY_TRANSFER_FIELD(c, cmd, default_member_permissions);
            DISCUSY_TRANSFER_FIELD(c, cmd, dm_permission);
            DISCUSY_TRANSFER_FIELD(c, cmd, default_permission);
            DISCUSY_TRANSFER_FIELD(c, cmd, integration_types);
            DISCUSY_TRANSFER_FIELD(c, cmd, contexts);
            DISCUSY_TRANSFER_FIELD(c, cmd, type);
            DISCUSY_TRANSFER_FIELD(c, cmd, nsfw);
            DISCUSY_TRANSFER_FIELD(c, cmd, handler);
            return c;

            DISCUSY_END_FROM
        }
        decltype(auto) set_id(this auto&& self, opt<snowflake> id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_name(this auto&& self, std::string n) { self.name = std::move(n); return std::forward<decltype(self)>(self); }
        decltype(auto) set_name_localizations(this auto&& self, opt<std::unordered_map<std::string, std::string>> nl) { self.name_localizations = std::move(nl); return std::forward<decltype(self)>(self); }
        decltype(auto) set_description(this auto&& self, opt<std::string> d) { self.description = std::move(d); return std::forward<decltype(self)>(self); }
        decltype(auto) set_description_localizations(this auto&& self, opt<std::unordered_map<std::string, std::string>> dl) { self.description_localizations = std::move(dl); return std::forward<decltype(self)>(self); }
        decltype(auto) set_options(this auto&& self, opt<std::vector<discusy::application_commands::application_command_option>> opts) { self.options = std::move(opts); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_options, discusy::application_commands::application_command_option)
        decltype(auto) add_option(this auto&& self, discusy::application_commands::application_command_option opt_) {
            if (!self.options) self.options.emplace();
            self.options->emplace_back(std::move(opt_));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_default_member_permissions(this auto&& self, opt<permissions_t> dmp) noexcept { self.default_member_permissions = dmp; return std::forward<decltype(self)>(self); }
        decltype(auto) set_dm_permission(this auto&& self, opt<bool> dmp) noexcept { self.dm_permission = dmp; return std::forward<decltype(self)>(self); }
        decltype(auto) set_default_permission(this auto&& self, opt<bool> dp) noexcept { self.default_permission = dp; return std::forward<decltype(self)>(self); }
        decltype(auto) set_integration_types(this auto&& self, opt<std::vector<discusy::application::application_integration_type>> it) { self.integration_types = std::move(it); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_integration_types, discusy::application::application_integration_type)
        decltype(auto) add_integration_type(this auto&& self, discusy::application::application_integration_type it) {
            if (!self.integration_types) self.integration_types.emplace();
            self.integration_types->emplace_back(it);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_contexts(this auto&& self, opt<std::unordered_set<discusy::interaction::interaction_context_type>> ctx) { self.contexts = std::move(ctx); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER_SET(set_contexts, discusy::interaction::interaction_context_type)
        decltype(auto) add_context(this auto&& self, discusy::interaction::interaction_context_type ctx) {
            if (!self.contexts) self.contexts.emplace();
            self.contexts->emplace(ctx);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_type(this auto&& self, opt<discusy::application_commands::application_command_type> t) noexcept { self.type = t; return std::forward<decltype(self)>(self); }
        decltype(auto) set_nsfw(this auto&& self, opt<bool> n) noexcept { self.nsfw = n; return std::forward<decltype(self)>(self); }
        decltype(auto) set_handler(this auto&& self, opt<discusy::application_commands::entry_point_command_handler_types> h) noexcept { self.handler = h; return std::forward<decltype(self)>(self); }
    };

    struct get_application_commands_query {
        opt<bool> with_localizations{};

        static get_application_commands_query create(opt<bool> wl = {}) noexcept {
            return get_application_commands_query{.with_localizations = wl};
        }
        decltype(auto) set_with_localizations(this auto&& self, opt<bool> wl) noexcept { self.with_localizations = wl; return std::forward<decltype(self)>(self); }
    };

    struct edit_application_command_permissions {
        std::vector<discusy::application_commands::application_command_permissions> permissions{};

        static edit_application_command_permissions create(std::vector<discusy::application_commands::application_command_permissions> perms = {}) {
            return edit_application_command_permissions{.permissions = std::move(perms)};
        }
        decltype(auto) set_permissions(this auto&& self, std::vector<discusy::application_commands::application_command_permissions> perms) { self.permissions = std::move(perms); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_permissions, discusy::application_commands::application_command_permissions)
        decltype(auto) add_permission(this auto&& self, discusy::application_commands::application_command_permissions p) { self.permissions.emplace_back(std::move(p)); return std::forward<decltype(self)>(self); }
    };
}

namespace emoji {
    struct list_application_emojis_response {
        std::vector<discusy::emoji::emoji> items{};

        static list_application_emojis_response create(std::vector<discusy::emoji::emoji> itms = {}) {
            return list_application_emojis_response{.items = std::move(itms)};
        }
        decltype(auto) set_items(this auto&& self, std::vector<discusy::emoji::emoji> itms) { self.items = std::move(itms); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_items, discusy::emoji::emoji)
        decltype(auto) add_item(this auto&& self, discusy::emoji::emoji itm) { self.items.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
    };

    struct create_guild_emoji {
        std::string name{};
        image_data image{};
        std::vector<snowflake> roles{};

        static create_guild_emoji create(std::string name_ = {}, image_data img = {}) {
            return create_guild_emoji{.name = std::move(name_), .image = std::move(img)};
        }
        decltype(auto) set_name(this auto&& self, std::string name_) { self.name = std::move(name_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_image(this auto&& self, image_data img) { self.image = std::move(img); return std::forward<decltype(self)>(self); }
        decltype(auto) set_roles(this auto&& self, std::vector<snowflake> r) { self.roles = std::move(r); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_roles, snowflake)
        decltype(auto) add_role(this auto&& self, snowflake rid) { self.roles.emplace_back(rid); return std::forward<decltype(self)>(self); }
    };

    struct modify_guild_emoji {
        opt<std::string> name{};
        opt<explicit_null<std::vector<snowflake>>> roles{};

        static modify_guild_emoji create(opt<std::string> name_ = {}) {
            return modify_guild_emoji{.name = std::move(name_)};
        }
        decltype(auto) set_name(this auto&& self, opt<std::string> n) { self.name = std::move(n); return std::forward<decltype(self)>(self); }
        decltype(auto) set_roles(this auto&& self, opt<explicit_null<std::vector<snowflake>>> r) { self.roles = std::move(r); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_roles, snowflake)
        decltype(auto) add_role(this auto&& self, snowflake rid) {
            if (!self.roles || !self.roles->has_value()) self.roles.emplace().emplace();
            (**self.roles).emplace_back(rid);
            return std::forward<decltype(self)>(self);
        }
    };

    struct create_application_emoji {
        std::string name{};
        image_data image{};

        static create_application_emoji create(std::string name_ = {}, image_data img = {}) {
            return create_application_emoji{.name = std::move(name_), .image = std::move(img)};
        }
        decltype(auto) set_name(this auto&& self, std::string name_) { self.name = std::move(name_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_image(this auto&& self, image_data img) { self.image = std::move(img); return std::forward<decltype(self)>(self); }
    };

    struct modify_application_emoji {
        std::string name{};

        static modify_application_emoji create(std::string name_ = {}) {
            return modify_application_emoji{.name = std::move(name_)};
        }
        decltype(auto) set_name(this auto&& self, std::string name_) { self.name = std::move(name_); return std::forward<decltype(self)>(self); }
    };
}

namespace entitlement {
    enum class entitlement_owner_type : std::uint8_t {
        GUILD_SUBSCRIPTION = 1, // entitlement is owned by a guild
        USER_SUBSCRIPTION = 2, // entitlement is owned by a user
    };

    struct list_entitlements_query {
        opt<snowflake> user_id{};
        opt<std::string> sku_ids{}; // comma-delimited set of snowflakes
        opt<snowflake> before{};
        opt<snowflake> after{};
        opt<integer> limit{};
        opt<snowflake> guild_id{};
        opt<bool> exclude_ended{};
        opt<bool> exclude_deleted{};

        static list_entitlements_query create() noexcept {
            return list_entitlements_query{};
        }
        decltype(auto) set_user_id(this auto&& self, opt<snowflake> uid) noexcept { self.user_id = uid; return std::forward<decltype(self)>(self); }
        decltype(auto) set_sku_ids(this auto&& self, opt<std::string> sids) { self.sku_ids = std::move(sids); return std::forward<decltype(self)>(self); }
        decltype(auto) set_before(this auto&& self, opt<snowflake> b) noexcept { self.before = b; return std::forward<decltype(self)>(self); }
        decltype(auto) set_after(this auto&& self, opt<snowflake> a) noexcept { self.after = a; return std::forward<decltype(self)>(self); }
        decltype(auto) set_limit(this auto&& self, opt<integer> l) noexcept { self.limit = l; return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild_id(this auto&& self, opt<snowflake> gid) noexcept { self.guild_id = gid; return std::forward<decltype(self)>(self); }
        decltype(auto) set_exclude_ended(this auto&& self, opt<bool> ee) noexcept { self.exclude_ended = ee; return std::forward<decltype(self)>(self); }
        decltype(auto) set_exclude_deleted(this auto&& self, opt<bool> ed) noexcept { self.exclude_deleted = ed; return std::forward<decltype(self)>(self); }
    };

    struct create_test_entitlement {
        snowflake sku_id{};
        snowflake owner_id{};
        entitlement_owner_type owner_type{};

        static create_test_entitlement create(snowflake sku = {}, snowflake owner = {}, entitlement_owner_type ot = entitlement_owner_type::USER_SUBSCRIPTION) noexcept {
            return create_test_entitlement{.sku_id = sku, .owner_id = owner, .owner_type = ot};
        }
        decltype(auto) set_sku_id(this auto&& self, snowflake sku) noexcept { self.sku_id = sku; return std::forward<decltype(self)>(self); }
        decltype(auto) set_owner_id(this auto&& self, snowflake owner) noexcept { self.owner_id = owner; return std::forward<decltype(self)>(self); }
        decltype(auto) set_owner_type(this auto&& self, entitlement_owner_type ot) noexcept { self.owner_type = ot; return std::forward<decltype(self)>(self); }
    };
}

namespace guild_scheduled_event {
    struct list_scheduled_events_for_guild_query {
        opt<bool> with_user_count{};

        static list_scheduled_events_for_guild_query create(opt<bool> wuc = {}) noexcept {
            return list_scheduled_events_for_guild_query{.with_user_count = wuc};
        }
        decltype(auto) set_with_user_count(this auto&& self, opt<bool> wuc) noexcept { self.with_user_count = wuc; return std::forward<decltype(self)>(self); }
    };

    struct create_guild_scheduled_event {
        opt<snowflake> channel_id{};
        opt<discusy::guild_scheduled_event::guild_scheduled_event_entity_metadata> entity_metadata{};
        std::string name{};
        discusy::guild_scheduled_event::guild_scheduled_event_privacy_level privacy_level{};
        timestamp scheduled_start_time{};
        opt<timestamp> scheduled_end_time{};
        opt<std::string> description{};
        discusy::guild_scheduled_event::guild_scheduled_event_entity_types entity_type{};
        opt<image_data> image{};
        opt<discusy::guild_scheduled_event::guild_scheduled_event_recurrence_rule> recurrence_rule{};

        static create_guild_scheduled_event create(std::string name_ = {}, timestamp start_time = {}, discusy::guild_scheduled_event::guild_scheduled_event_entity_types entity_type_ = discusy::guild_scheduled_event::guild_scheduled_event_entity_types::STAGE_INSTANCE) {
            return create_guild_scheduled_event{.name = std::move(name_), .privacy_level = discusy::guild_scheduled_event::guild_scheduled_event_privacy_level::GUILD_ONLY, .scheduled_start_time = start_time, .entity_type = entity_type_};
        }
        template <typename From>
        static create_guild_scheduled_event from(From&& e) {
            DISCUSY_FORWARD_IF_SAME(create_guild_scheduled_event, e);

            static_assert(
                DISCUSY_HAS_FIELD(From, channel_id) ||
                DISCUSY_HAS_FIELD(From, entity_metadata) ||
                DISCUSY_HAS_FIELD(From, name) ||
                DISCUSY_HAS_FIELD(From, privacy_level) ||
                DISCUSY_HAS_FIELD(From, scheduled_start_time) ||
                DISCUSY_HAS_FIELD(From, scheduled_end_time) ||
                DISCUSY_HAS_FIELD(From, description) ||
                DISCUSY_HAS_FIELD(From, entity_type) ||
                DISCUSY_HAS_FIELD(From, recurrence_rule),
                "create_guild_scheduled_event::from: Source type contains no compatible scheduled event fields."
            );

            create_guild_scheduled_event res{};
            DISCUSY_TRANSFER_FIELD(res, e, channel_id);
            DISCUSY_TRANSFER_FIELD(res, e, entity_metadata);
            DISCUSY_TRANSFER_FIELD(res, e, name);
            DISCUSY_TRANSFER_FIELD(res, e, privacy_level);
            DISCUSY_TRANSFER_FIELD(res, e, scheduled_start_time);
            DISCUSY_TRANSFER_FIELD(res, e, scheduled_end_time);
            DISCUSY_TRANSFER_FIELD(res, e, description);
            DISCUSY_TRANSFER_FIELD(res, e, entity_type);
            DISCUSY_TRANSFER_FIELD(res, e, recurrence_rule);
            return res;

            DISCUSY_END_FROM
        }
        decltype(auto) set_channel_id(this auto&& self, opt<snowflake> cid) noexcept { self.channel_id = cid; return std::forward<decltype(self)>(self); }
        decltype(auto) set_channel(this auto&& self, snowflake cid) noexcept { return std::forward<decltype(self)>(self).set_channel_id(cid); }
        decltype(auto) set_entity_metadata(this auto&& self, opt<discusy::guild_scheduled_event::guild_scheduled_event_entity_metadata> em) { self.entity_metadata = std::move(em); return std::forward<decltype(self)>(self); }
        decltype(auto) set_name(this auto&& self, std::string n) { self.name = std::move(n); return std::forward<decltype(self)>(self); }
        decltype(auto) set_privacy_level(this auto&& self, discusy::guild_scheduled_event::guild_scheduled_event_privacy_level pl) noexcept { self.privacy_level = pl; return std::forward<decltype(self)>(self); }
        decltype(auto) set_scheduled_start_time(this auto&& self, timestamp st) noexcept { self.scheduled_start_time = st; return std::forward<decltype(self)>(self); }
        decltype(auto) set_scheduled_end_time(this auto&& self, opt<timestamp> et) noexcept { self.scheduled_end_time = et; return std::forward<decltype(self)>(self); }
        decltype(auto) set_description(this auto&& self, opt<std::string> d) { self.description = std::move(d); return std::forward<decltype(self)>(self); }
        decltype(auto) set_entity_type(this auto&& self, discusy::guild_scheduled_event::guild_scheduled_event_entity_types et) noexcept { self.entity_type = et; return std::forward<decltype(self)>(self); }
        decltype(auto) set_image(this auto&& self, opt<image_data> img) { self.image = std::move(img); return std::forward<decltype(self)>(self); }
        decltype(auto) set_recurrence_rule(this auto&& self, opt<discusy::guild_scheduled_event::guild_scheduled_event_recurrence_rule> rr) { self.recurrence_rule = std::move(rr); return std::forward<decltype(self)>(self); }
    };

    struct get_guild_scheduled_event_query {
        opt<bool> with_user_count{};

        static get_guild_scheduled_event_query create(opt<bool> wuc = {}) noexcept {
            return get_guild_scheduled_event_query{.with_user_count = wuc};
        }
        decltype(auto) set_with_user_count(this auto&& self, opt<bool> wuc) noexcept { self.with_user_count = wuc; return std::forward<decltype(self)>(self); }
    };

    struct modify_guild_scheduled_event {
        opt<explicit_null<snowflake>> channel_id{};
        opt<explicit_null<discusy::guild_scheduled_event::guild_scheduled_event_entity_metadata>> entity_metadata{};
        opt<std::string> name{};
        opt<discusy::guild_scheduled_event::guild_scheduled_event_privacy_level> privacy_level{};
        opt<timestamp> scheduled_start_time{};
        opt<timestamp> scheduled_end_time{};
        opt<explicit_null<std::string>> description{};
        opt<discusy::guild_scheduled_event::guild_scheduled_event_entity_types> entity_type{};
        opt<discusy::guild_scheduled_event::guild_scheduled_event_status> status{};
        opt<image_data> image{};
        opt<explicit_null<discusy::guild_scheduled_event::guild_scheduled_event_recurrence_rule>> recurrence_rule{};

        static modify_guild_scheduled_event create() noexcept {
            return modify_guild_scheduled_event{};
        }
        template <typename From>
        static modify_guild_scheduled_event from(From&& e) {
            DISCUSY_FORWARD_IF_SAME(modify_guild_scheduled_event, e);

            static_assert(
                DISCUSY_HAS_FIELD(From, channel_id) ||
                DISCUSY_HAS_FIELD(From, entity_metadata) ||
                DISCUSY_HAS_FIELD(From, name) ||
                DISCUSY_HAS_FIELD(From, privacy_level) ||
                DISCUSY_HAS_FIELD(From, scheduled_start_time) ||
                DISCUSY_HAS_FIELD(From, scheduled_end_time) ||
                DISCUSY_HAS_FIELD(From, description) ||
                DISCUSY_HAS_FIELD(From, entity_type) ||
                DISCUSY_HAS_FIELD(From, status) ||
                DISCUSY_HAS_FIELD(From, recurrence_rule),
                "modify_guild_scheduled_event::from: Source type contains no compatible scheduled event fields."
            );

            modify_guild_scheduled_event m{};
            DISCUSY_TRANSFER_FIELD(m, e, channel_id);
            DISCUSY_TRANSFER_FIELD(m, e, entity_metadata);
            DISCUSY_TRANSFER_FIELD(m, e, name);
            DISCUSY_TRANSFER_FIELD(m, e, privacy_level);
            DISCUSY_TRANSFER_FIELD(m, e, scheduled_start_time);
            DISCUSY_TRANSFER_FIELD(m, e, scheduled_end_time);
            DISCUSY_TRANSFER_FIELD(m, e, description);
            DISCUSY_TRANSFER_FIELD(m, e, entity_type);
            DISCUSY_TRANSFER_FIELD(m, e, status);
            DISCUSY_TRANSFER_FIELD(m, e, recurrence_rule);
            return m;

            DISCUSY_END_FROM
        }
        decltype(auto) set_channel_id(this auto&& self, opt<explicit_null<snowflake>> cid) noexcept { self.channel_id = cid; return std::forward<decltype(self)>(self); }
        decltype(auto) set_channel(this auto&& self, snowflake cid) noexcept { return std::forward<decltype(self)>(self).set_channel_id(explicit_null<snowflake>{cid}); }
        decltype(auto) set_entity_metadata(this auto&& self, opt<explicit_null<discusy::guild_scheduled_event::guild_scheduled_event_entity_metadata>> em) { self.entity_metadata = std::move(em); return std::forward<decltype(self)>(self); }
        decltype(auto) set_name(this auto&& self, opt<std::string> n) { self.name = std::move(n); return std::forward<decltype(self)>(self); }
        decltype(auto) set_privacy_level(this auto&& self, opt<discusy::guild_scheduled_event::guild_scheduled_event_privacy_level> pl) noexcept { self.privacy_level = pl; return std::forward<decltype(self)>(self); }
        decltype(auto) set_scheduled_start_time(this auto&& self, opt<timestamp> st) noexcept { self.scheduled_start_time = st; return std::forward<decltype(self)>(self); }
        decltype(auto) set_scheduled_end_time(this auto&& self, opt<timestamp> et) noexcept { self.scheduled_end_time = et; return std::forward<decltype(self)>(self); }
        decltype(auto) set_description(this auto&& self, opt<explicit_null<std::string>> d) { self.description = std::move(d); return std::forward<decltype(self)>(self); }
        decltype(auto) set_entity_type(this auto&& self, opt<discusy::guild_scheduled_event::guild_scheduled_event_entity_types> et) noexcept { self.entity_type = et; return std::forward<decltype(self)>(self); }
        decltype(auto) set_status(this auto&& self, opt<discusy::guild_scheduled_event::guild_scheduled_event_status> s) noexcept { self.status = s; return std::forward<decltype(self)>(self); }
        decltype(auto) set_image(this auto&& self, opt<image_data> img) { self.image = std::move(img); return std::forward<decltype(self)>(self); }
        decltype(auto) set_recurrence_rule(this auto&& self, opt<explicit_null<discusy::guild_scheduled_event::guild_scheduled_event_recurrence_rule>> rr) { self.recurrence_rule = std::move(rr); return std::forward<decltype(self)>(self); }
    };

    struct get_guild_scheduled_event_users_query {
        opt<integer> limit{};
        opt<bool> with_member{};
        opt<snowflake> before{};
        opt<snowflake> after{};

        static get_guild_scheduled_event_users_query create() noexcept {
            return get_guild_scheduled_event_users_query{};
        }
        decltype(auto) set_limit(this auto&& self, opt<integer> l) noexcept { self.limit = l; return std::forward<decltype(self)>(self); }
        decltype(auto) set_with_member(this auto&& self, opt<bool> wm) noexcept { self.with_member = wm; return std::forward<decltype(self)>(self); }
        decltype(auto) set_before(this auto&& self, opt<snowflake> b) noexcept { self.before = b; return std::forward<decltype(self)>(self); }
        decltype(auto) set_after(this auto&& self, opt<snowflake> a) noexcept { self.after = a; return std::forward<decltype(self)>(self); }
    };
}

namespace guild_template {
    struct create_guild_template {
        std::string name{};
        opt<explicit_null<std::string>> description{};

        static create_guild_template create(std::string name_ = {}) {
            return create_guild_template{.name = std::move(name_)};
        }
        decltype(auto) set_name(this auto&& self, std::string n) { self.name = std::move(n); return std::forward<decltype(self)>(self); }
        decltype(auto) set_description(this auto&& self, opt<explicit_null<std::string>> d) { self.description = std::move(d); return std::forward<decltype(self)>(self); }
        decltype(auto) set_description(this auto&& self, std::string d) { self.description = explicit_null<std::string>{std::move(d)}; return std::forward<decltype(self)>(self); }
    };

    struct modify_guild_template {
        opt<std::string> name{};
        opt<explicit_null<std::string>> description{};

        static modify_guild_template create(opt<std::string> name_ = {}) {
            return modify_guild_template{.name = std::move(name_)};
        }
        decltype(auto) set_name(this auto&& self, opt<std::string> n) { self.name = std::move(n); return std::forward<decltype(self)>(self); }
        decltype(auto) set_description(this auto&& self, opt<explicit_null<std::string>> d) { self.description = std::move(d); return std::forward<decltype(self)>(self); }
        decltype(auto) set_description(this auto&& self, std::string d) { self.description = explicit_null<std::string>{std::move(d)}; return std::forward<decltype(self)>(self); }
    };
}

namespace invite {
    enum class target_users_job_status_code : std::uint8_t {
        UNSPECIFIED = 0, // The default value
        PROCESSING = 1, // The job is still being processed
        COMPLETED = 2, // The job has been completed successfully
        FAILED = 3, // The job has failed, see error_message for more details
    };

    struct get_invite_query {
        opt<bool> with_counts{};
        opt<snowflake> guild_scheduled_event_id{};

        static get_invite_query create(opt<bool> wc = {}) noexcept {
            return get_invite_query{.with_counts = wc};
        }
        decltype(auto) set_with_counts(this auto&& self, opt<bool> wc) noexcept { self.with_counts = wc; return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild_scheduled_event_id(this auto&& self, opt<snowflake> gseid) noexcept { self.guild_scheduled_event_id = gseid; return std::forward<decltype(self)>(self); }
    };

    struct bulk_add_delete_target_users {
        std::vector<snowflake> user_ids{}; // max of 1000
    };

    struct target_users_job_status {
        target_users_job_status_code status{};
        integer total_users{};
        integer processed_users{};
        opt<timestamp> created_at{};
        opt<timestamp> completed_at{};
        opt<std::string> error_message{};

        static target_users_job_status create() noexcept {
            return target_users_job_status{};
        }
        decltype(auto) set_status(this auto&& self, target_users_job_status_code s) noexcept { self.status = s; return std::forward<decltype(self)>(self); }
        decltype(auto) set_total_users(this auto&& self, integer tu) noexcept { self.total_users = tu; return std::forward<decltype(self)>(self); }
        decltype(auto) set_processed_users(this auto&& self, integer pu) noexcept { self.processed_users = pu; return std::forward<decltype(self)>(self); }
        decltype(auto) set_created_at(this auto&& self, opt<timestamp> ca) noexcept { self.created_at = ca; return std::forward<decltype(self)>(self); }
        decltype(auto) set_completed_at(this auto&& self, opt<timestamp> ca) noexcept { self.completed_at = ca; return std::forward<decltype(self)>(self); }
        decltype(auto) set_error_message(this auto&& self, opt<std::string> em) { self.error_message = std::move(em); return std::forward<decltype(self)>(self); }
    };
}

namespace lobby {
    struct lobby_member_request {
        snowflake id{};
        opt<discusy::lobby::lobby_metadata> metadata{};
        opt<flags_t<discusy::lobby::lobby_member_flags>> flags{};

        static lobby_member_request create(snowflake id_ = {}) noexcept {
            return lobby_member_request{.id = id_};
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_metadata(this auto&& self, opt<discusy::lobby::lobby_metadata> m) { self.metadata = std::move(m); return std::forward<decltype(self)>(self); }
        decltype(auto) set_flags(this auto&& self, opt<flags_t<discusy::lobby::lobby_member_flags>> f) noexcept { self.flags = f; return std::forward<decltype(self)>(self); }
    };

    struct bulk_lobby_member_request {
        snowflake id{};
        opt<discusy::lobby::lobby_metadata> metadata{};
        opt<flags_t<discusy::lobby::lobby_member_flags>> flags{};
        opt<bool> remove_member{};

        static bulk_lobby_member_request create(snowflake id_ = {}) noexcept {
            return bulk_lobby_member_request{.id = id_};
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_metadata(this auto&& self, opt<discusy::lobby::lobby_metadata> m) { self.metadata = std::move(m); return std::forward<decltype(self)>(self); }
        decltype(auto) set_flags(this auto&& self, opt<flags_t<discusy::lobby::lobby_member_flags>> f) noexcept { self.flags = f; return std::forward<decltype(self)>(self); }
        decltype(auto) set_remove_member(this auto&& self, opt<bool> rm) noexcept { self.remove_member = rm; return std::forward<decltype(self)>(self); }
    };

    struct create_lobby {
        opt<discusy::lobby::lobby_metadata> metadata{};
        opt<std::vector<lobby_member_request>> members{};
        opt<integer> idle_timeout_seconds{};

        static create_lobby create() noexcept {
            return create_lobby{};
        }
        decltype(auto) set_metadata(this auto&& self, opt<discusy::lobby::lobby_metadata> m) { self.metadata = std::move(m); return std::forward<decltype(self)>(self); }
        decltype(auto) set_members(this auto&& self, opt<std::vector<lobby_member_request>> m) { self.members = std::move(m); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_members, lobby_member_request)
        decltype(auto) add_member(this auto&& self, lobby_member_request m) {
            if (!self.members) self.members.emplace();
            self.members->emplace_back(std::move(m));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_idle_timeout_seconds(this auto&& self, opt<integer> its) noexcept { self.idle_timeout_seconds = its; return std::forward<decltype(self)>(self); }
    };

    struct create_or_join_lobby {
        std::string secret{};
        opt<integer> idle_timeout_seconds{};
        opt<discusy::lobby::lobby_metadata> lobby_metadata{};
        opt<discusy::lobby::lobby_metadata> member_metadata{};

        static create_or_join_lobby create(std::string secret_ = {}) {
            return create_or_join_lobby{.secret = std::move(secret_)};
        }
        decltype(auto) set_secret(this auto&& self, std::string s) { self.secret = std::move(s); return std::forward<decltype(self)>(self); }
        decltype(auto) set_idle_timeout_seconds(this auto&& self, opt<integer> its) noexcept { self.idle_timeout_seconds = its; return std::forward<decltype(self)>(self); }
        decltype(auto) set_lobby_metadata(this auto&& self, opt<discusy::lobby::lobby_metadata> lm) { self.lobby_metadata = std::move(lm); return std::forward<decltype(self)>(self); }
        decltype(auto) set_member_metadata(this auto&& self, opt<discusy::lobby::lobby_metadata> mm) { self.member_metadata = std::move(mm); return std::forward<decltype(self)>(self); }
    };

    struct modify_lobby {
        opt<discusy::lobby::lobby_metadata> metadata{};
        opt<std::vector<lobby_member_request>> members{};
        opt<integer> idle_timeout_seconds{};

        static modify_lobby create() noexcept {
            return modify_lobby{};
        }
        decltype(auto) set_metadata(this auto&& self, opt<discusy::lobby::lobby_metadata> m) { self.metadata = std::move(m); return std::forward<decltype(self)>(self); }
        decltype(auto) set_members(this auto&& self, opt<std::vector<lobby_member_request>> m) { self.members = std::move(m); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_members, lobby_member_request)
        decltype(auto) add_member(this auto&& self, lobby_member_request m) {
            if (!self.members) self.members.emplace();
            self.members->emplace_back(std::move(m));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_idle_timeout_seconds(this auto&& self, opt<integer> its) noexcept { self.idle_timeout_seconds = its; return std::forward<decltype(self)>(self); }
    };

    struct add_lobby_member {
        opt<discusy::lobby::lobby_metadata> metadata{};
        opt<flags_t<discusy::lobby::lobby_member_flags>> flags{};

        static add_lobby_member create() noexcept {
            return add_lobby_member{};
        }
        decltype(auto) set_metadata(this auto&& self, opt<discusy::lobby::lobby_metadata> m) { self.metadata = std::move(m); return std::forward<decltype(self)>(self); }
        decltype(auto) set_flags(this auto&& self, opt<flags_t<discusy::lobby::lobby_member_flags>> f) noexcept { self.flags = f; return std::forward<decltype(self)>(self); }
    };

    using bulk_update_lobby_members = std::span<const bulk_lobby_member_request>;

    struct link_channel_to_lobby {
        opt<snowflake> channel_id{};

        static link_channel_to_lobby create(opt<snowflake> cid = {}) noexcept {
            return link_channel_to_lobby{.channel_id = cid};
        }
        decltype(auto) set_channel_id(this auto&& self, opt<snowflake> cid) noexcept { self.channel_id = cid; return std::forward<decltype(self)>(self); }
        decltype(auto) set_channel(this auto&& self, snowflake cid) noexcept { return std::forward<decltype(self)>(self).set_channel_id(cid); }
    };

    struct send_lobby_message {
        std::string content{};
        opt<discusy::lobby::lobby_metadata> metadata{};
        opt<flags_t<discusy::message::message_flags>> flags{};

        static send_lobby_message create(std::string content_ = {}) {
            return send_lobby_message{.content = std::move(content_)};
        }
        decltype(auto) set_content(this auto&& self, std::string c) { self.content = std::move(c); return std::forward<decltype(self)>(self); }
        decltype(auto) set_metadata(this auto&& self, opt<discusy::lobby::lobby_metadata> m) { self.metadata = std::move(m); return std::forward<decltype(self)>(self); }
        decltype(auto) set_flags(this auto&& self, opt<flags_t<discusy::message::message_flags>> f) noexcept { self.flags = f; return std::forward<decltype(self)>(self); }
    };

    struct get_lobby_messages_query {
        opt<integer> limit{};

        static get_lobby_messages_query create(opt<integer> l = {}) noexcept {
            return get_lobby_messages_query{.limit = l};
        }
        decltype(auto) set_limit(this auto&& self, opt<integer> l) noexcept { self.limit = l; return std::forward<decltype(self)>(self); }
    };

    using update_lobby_message_moderation_metadata = discusy::lobby::lobby_metadata;
}

namespace poll {
    struct get_answer_voters_query {
        opt<snowflake> after{};
        opt<integer> limit{};

        static get_answer_voters_query create() noexcept {
            return get_answer_voters_query{};
        }
        decltype(auto) set_after(this auto&& self, opt<snowflake> a) noexcept { self.after = a; return std::forward<decltype(self)>(self); }
        decltype(auto) set_limit(this auto&& self, opt<integer> l) noexcept { self.limit = l; return std::forward<decltype(self)>(self); }
    };

    struct get_answer_voters_response {
        std::vector<discusy::user::user> users{};

        static get_answer_voters_response create(std::vector<discusy::user::user> us = {}) {
            return get_answer_voters_response{.users = std::move(us)};
        }
        decltype(auto) set_users(this auto&& self, std::vector<discusy::user::user> us) { self.users = std::move(us); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_users, discusy::user::user)
        decltype(auto) add_user(this auto&& self, discusy::user::user u) { self.users.emplace_back(std::move(u)); return std::forward<decltype(self)>(self); }
    };
}

namespace sku {
    struct list_skus_response {
        std::vector<discusy::sku::sku> skus{};

        static list_skus_response create(std::vector<discusy::sku::sku> sks = {}) {
            return list_skus_response{.skus = std::move(sks)};
        }
        decltype(auto) set_skus(this auto&& self, std::vector<discusy::sku::sku> sks) { self.skus = std::move(sks); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_skus, discusy::sku::sku)
        decltype(auto) add_sku(this auto&& self, discusy::sku::sku s) { self.skus.emplace_back(std::move(s)); return std::forward<decltype(self)>(self); }
    };
}

namespace soundboard {
    struct send_soundboard_sound {
        snowflake sound_id{};
        opt<snowflake> source_guild_id{};

        static send_soundboard_sound create(snowflake sid = {}, opt<snowflake> sgid = {}) noexcept {
            return send_soundboard_sound{.sound_id = sid, .source_guild_id = sgid};
        }
        decltype(auto) set_sound_id(this auto&& self, snowflake sid) noexcept { self.sound_id = sid; return std::forward<decltype(self)>(self); }
        decltype(auto) set_source_guild_id(this auto&& self, opt<snowflake> sgid) noexcept { self.source_guild_id = sgid; return std::forward<decltype(self)>(self); }
    };

    struct list_guild_soundboard_sounds_response {
        std::vector<discusy::soundboard::soundboard_sound> items{};

        static list_guild_soundboard_sounds_response create(std::vector<discusy::soundboard::soundboard_sound> itms = {}) {
            return list_guild_soundboard_sounds_response{.items = std::move(itms)};
        }
        decltype(auto) set_items(this auto&& self, std::vector<discusy::soundboard::soundboard_sound> itms) { self.items = std::move(itms); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_items, discusy::soundboard::soundboard_sound)
        decltype(auto) add_item(this auto&& self, discusy::soundboard::soundboard_sound itm) { self.items.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
    };

    struct create_guild_soundboard_sound {
        std::string name{};
        std::string sound{}; // data uri of the mp3/ogg sound data
        opt<explicit_null<double>> volume{};
        opt<explicit_null<snowflake>> emoji_id{};
        opt<explicit_null<std::string>> emoji_name{};

        static create_guild_soundboard_sound create(std::string name_ = {}, std::string sound_ = {}) {
            return create_guild_soundboard_sound{.name = std::move(name_), .sound = std::move(sound_)};
        }
        decltype(auto) set_name(this auto&& self, std::string n) { self.name = std::move(n); return std::forward<decltype(self)>(self); }
        decltype(auto) set_sound(this auto&& self, std::string s) { self.sound = std::move(s); return std::forward<decltype(self)>(self); }
        decltype(auto) set_volume(this auto&& self, opt<explicit_null<double>> v) noexcept { self.volume = v; return std::forward<decltype(self)>(self); }
        decltype(auto) set_emoji_id(this auto&& self, opt<explicit_null<snowflake>> eid) noexcept { self.emoji_id = eid; return std::forward<decltype(self)>(self); }
        decltype(auto) set_emoji_name(this auto&& self, opt<explicit_null<std::string>> en) { self.emoji_name = std::move(en); return std::forward<decltype(self)>(self); }
    };

    struct modify_guild_soundboard_sound {
        opt<std::string> name{};
        opt<explicit_null<double>> volume{};
        opt<explicit_null<snowflake>> emoji_id{};
        opt<explicit_null<std::string>> emoji_name{};

        static modify_guild_soundboard_sound create(opt<std::string> name_ = {}) {
            return modify_guild_soundboard_sound{.name = std::move(name_)};
        }
        decltype(auto) set_name(this auto&& self, opt<std::string> n) { self.name = std::move(n); return std::forward<decltype(self)>(self); }
        decltype(auto) set_volume(this auto&& self, opt<explicit_null<double>> v) noexcept { self.volume = v; return std::forward<decltype(self)>(self); }
        decltype(auto) set_emoji_id(this auto&& self, opt<explicit_null<snowflake>> eid) noexcept { self.emoji_id = eid; return std::forward<decltype(self)>(self); }
        decltype(auto) set_emoji_name(this auto&& self, opt<explicit_null<std::string>> en) { self.emoji_name = std::move(en); return std::forward<decltype(self)>(self); }
    };
}

namespace stage_instance {
    struct create_stage_instance {
        snowflake channel_id{};
        std::string topic{};
        opt<discusy::stage_instance::privacy_level> privacy_level{};
        opt<bool> send_start_notification{};
        opt<snowflake> guild_scheduled_event_id{};

        static create_stage_instance create(snowflake cid = {}, std::string topic_ = {}) {
            return create_stage_instance{.channel_id = cid, .topic = std::move(topic_)};
        }
        template <typename From>
        static create_stage_instance from(From&& s) {
            DISCUSY_FORWARD_IF_SAME(create_stage_instance, s);

            static_assert(
                DISCUSY_HAS_FIELD(From, channel_id) ||
                DISCUSY_HAS_FIELD(From, topic) ||
                DISCUSY_HAS_FIELD(From, privacy_level) ||
                DISCUSY_HAS_FIELD(From, guild_scheduled_event_id),
                "create_stage_instance::from: Source type contains no compatible stage instance fields."
            );

            create_stage_instance res{};
            DISCUSY_TRANSFER_FIELD(res, s, channel_id);
            DISCUSY_TRANSFER_FIELD(res, s, topic);
            DISCUSY_TRANSFER_FIELD(res, s, privacy_level);
            DISCUSY_TRANSFER_FIELD(res, s, guild_scheduled_event_id);
            return res;

            DISCUSY_END_FROM
        }
        decltype(auto) set_channel_id(this auto&& self, snowflake cid) noexcept { self.channel_id = cid; return std::forward<decltype(self)>(self); }
        decltype(auto) set_channel(this auto&& self, snowflake cid) noexcept { return std::forward<decltype(self)>(self).set_channel_id(cid); }
        decltype(auto) set_topic(this auto&& self, std::string t) { self.topic = std::move(t); return std::forward<decltype(self)>(self); }
        decltype(auto) set_privacy_level(this auto&& self, opt<discusy::stage_instance::privacy_level> pl) noexcept { self.privacy_level = pl; return std::forward<decltype(self)>(self); }
        decltype(auto) set_send_start_notification(this auto&& self, opt<bool> ssn) noexcept { self.send_start_notification = ssn; return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild_scheduled_event_id(this auto&& self, opt<snowflake> gseid) noexcept { self.guild_scheduled_event_id = gseid; return std::forward<decltype(self)>(self); }
    };

    struct modify_stage_instance {
        opt<std::string> topic{};
        opt<discusy::stage_instance::privacy_level> privacy_level{};

        static modify_stage_instance create(opt<std::string> topic_ = {}) {
            return modify_stage_instance{.topic = std::move(topic_)};
        }
        template <typename From>
        static modify_stage_instance from(From&& s) {
            DISCUSY_FORWARD_IF_SAME(modify_stage_instance, s);

            static_assert(
                DISCUSY_HAS_FIELD(From, topic) ||
                DISCUSY_HAS_FIELD(From, privacy_level),
                "modify_stage_instance::from: Source type contains no compatible stage instance fields."
            );

            modify_stage_instance res{};
            DISCUSY_TRANSFER_FIELD(res, s, topic);
            DISCUSY_TRANSFER_FIELD(res, s, privacy_level);
            return res;

            DISCUSY_END_FROM
        }
        decltype(auto) set_topic(this auto&& self, opt<std::string> t) { self.topic = std::move(t); return std::forward<decltype(self)>(self); }
        decltype(auto) set_privacy_level(this auto&& self, opt<discusy::stage_instance::privacy_level> pl) noexcept { self.privacy_level = pl; return std::forward<decltype(self)>(self); }
    };
}

namespace sticker {
    struct list_sticker_packs_response {
        std::vector<discusy::sticker::sticker_pack> sticker_packs{};

        static list_sticker_packs_response create(std::vector<discusy::sticker::sticker_pack> sp = {}) {
            return list_sticker_packs_response{.sticker_packs = std::move(sp)};
        }
        decltype(auto) set_sticker_packs(this auto&& self, std::vector<discusy::sticker::sticker_pack> sp) { self.sticker_packs = std::move(sp); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_sticker_packs, discusy::sticker::sticker_pack)
        decltype(auto) add_sticker_pack(this auto&& self, discusy::sticker::sticker_pack sp) { self.sticker_packs.emplace_back(std::move(sp)); return std::forward<decltype(self)>(self); }
    };

    // sent as multipart/form-data alongside the sticker file
    struct create_guild_sticker {
        std::string name{};
        std::string description{};
        std::string tags{};

        static create_guild_sticker create(std::string name_ = {}, std::string desc_ = {}, std::string tags_ = {}) {
            return create_guild_sticker{.name = std::move(name_), .description = std::move(desc_), .tags = std::move(tags_)};
        }
        decltype(auto) set_name(this auto&& self, std::string n) { self.name = std::move(n); return std::forward<decltype(self)>(self); }
        decltype(auto) set_description(this auto&& self, std::string d) { self.description = std::move(d); return std::forward<decltype(self)>(self); }
        decltype(auto) set_tags(this auto&& self, std::string t) { self.tags = std::move(t); return std::forward<decltype(self)>(self); }
    };

    struct modify_guild_sticker {
        opt<std::string> name{};
        opt<explicit_null<std::string>> description{};
        opt<std::string> tags{};

        static modify_guild_sticker create() noexcept {
            return modify_guild_sticker{};
        }
        decltype(auto) set_name(this auto&& self, opt<std::string> n) { self.name = std::move(n); return std::forward<decltype(self)>(self); }
        decltype(auto) set_description(this auto&& self, opt<explicit_null<std::string>> d) { self.description = std::move(d); return std::forward<decltype(self)>(self); }
        decltype(auto) set_description(this auto&& self, std::string d) { self.description = explicit_null<std::string>{std::move(d)}; return std::forward<decltype(self)>(self); }
        decltype(auto) set_tags(this auto&& self, opt<std::string> t) { self.tags = std::move(t); return std::forward<decltype(self)>(self); }
    };
}

namespace subscription {
    struct list_sku_subscriptions_query {
        opt<snowflake> before{};
        opt<snowflake> after{};
        opt<integer> limit{};
        opt<snowflake> user_id{};

        static list_sku_subscriptions_query create() noexcept {
            return list_sku_subscriptions_query{};
        }
        decltype(auto) set_before(this auto&& self, opt<snowflake> b) noexcept { self.before = b; return std::forward<decltype(self)>(self); }
        decltype(auto) set_after(this auto&& self, opt<snowflake> a) noexcept { self.after = a; return std::forward<decltype(self)>(self); }
        decltype(auto) set_limit(this auto&& self, opt<integer> l) noexcept { self.limit = l; return std::forward<decltype(self)>(self); }
        decltype(auto) set_user_id(this auto&& self, opt<snowflake> uid) noexcept { self.user_id = uid; return std::forward<decltype(self)>(self); }
    };
}

namespace user {
    struct modify_current_user {
        opt<std::string> username{};
        opt<explicit_null<image_data>> avatar{};
        opt<explicit_null<image_data>> banner{};

        static modify_current_user create(opt<std::string> name_ = {}) {
            return modify_current_user{.username = std::move(name_)};
        }
        template <typename From>
        static modify_current_user from(From&& u) {
            DISCUSY_FORWARD_IF_SAME(modify_current_user, u);

            static_assert(
                DISCUSY_HAS_FIELD(From, username),
                "modify_current_user::from: Source type contains no compatible user fields."
            );

            modify_current_user res{};
            DISCUSY_TRANSFER_FIELD(res, u, username);
            return res;

            DISCUSY_END_FROM
        }
        decltype(auto) set_username(this auto&& self, opt<std::string> u) { self.username = std::move(u); return std::forward<decltype(self)>(self); }
        decltype(auto) set_avatar(this auto&& self, opt<explicit_null<image_data>> av) { self.avatar = std::move(av); return std::forward<decltype(self)>(self); }
        decltype(auto) set_banner(this auto&& self, opt<explicit_null<image_data>> ban) { self.banner = std::move(ban); return std::forward<decltype(self)>(self); }
    };

    struct get_current_user_guilds_query {
        opt<snowflake> before{};
        opt<snowflake> after{};
        opt<integer> limit{};
        opt<bool> with_counts{};

        static get_current_user_guilds_query create() noexcept {
            return get_current_user_guilds_query{};
        }
        decltype(auto) set_before(this auto&& self, opt<snowflake> b) noexcept { self.before = b; return std::forward<decltype(self)>(self); }
        decltype(auto) set_after(this auto&& self, opt<snowflake> a) noexcept { self.after = a; return std::forward<decltype(self)>(self); }
        decltype(auto) set_limit(this auto&& self, opt<integer> l) noexcept { self.limit = l; return std::forward<decltype(self)>(self); }
        decltype(auto) set_with_counts(this auto&& self, opt<bool> wc) noexcept { self.with_counts = wc; return std::forward<decltype(self)>(self); }
    };

    struct create_dm {
        snowflake recipient_id{};

        static create_dm create(snowflake rid = {}) noexcept {
            return create_dm{.recipient_id = rid};
        }
        decltype(auto) set_recipient_id(this auto&& self, snowflake rid) noexcept { self.recipient_id = rid; return std::forward<decltype(self)>(self); }
        decltype(auto) set_recipient(this auto&& self, snowflake rid) noexcept { return std::forward<decltype(self)>(self).set_recipient_id(rid); }
    };

    struct create_group_dm {
        std::vector<std::string> access_tokens{};
        std::unordered_map<std::string, std::string> nicks{};

        static create_group_dm create(std::vector<std::string> tokens = {}) {
            return create_group_dm{.access_tokens = std::move(tokens)};
        }
        decltype(auto) set_access_tokens(this auto&& self, std::vector<std::string> tokens) { self.access_tokens = std::move(tokens); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_access_tokens, std::string)
        decltype(auto) add_access_token(this auto&& self, std::string token) { self.access_tokens.emplace_back(std::move(token)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_nicks(this auto&& self, std::unordered_map<std::string, std::string> n) { self.nicks = std::move(n); return std::forward<decltype(self)>(self); }
        decltype(auto) add_nick(this auto&& self, std::string user_id, std::string nick) { self.nicks.emplace(std::move(user_id), std::move(nick)); return std::forward<decltype(self)>(self); }
    };

    struct update_current_user_application_role_connection {
        opt<std::string> platform_name{};
        opt<std::string> platform_username{};
        opt<std::unordered_map<std::string, std::string>> metadata{};

        static update_current_user_application_role_connection create(opt<std::string> name_ = {}) {
            return update_current_user_application_role_connection{.platform_name = std::move(name_)};
        }
        decltype(auto) set_platform_name(this auto&& self, opt<std::string> pn) { self.platform_name = std::move(pn); return std::forward<decltype(self)>(self); }
        decltype(auto) set_platform_username(this auto&& self, opt<std::string> pu) { self.platform_username = std::move(pu); return std::forward<decltype(self)>(self); }
        decltype(auto) set_metadata(this auto&& self, opt<std::unordered_map<std::string, std::string>> m) { self.metadata = std::move(m); return std::forward<decltype(self)>(self); }
    };
}

namespace voice {
    struct modify_current_user_voice_state {
        opt<snowflake> channel_id{};
        opt<bool> suppress{};
        opt<explicit_null<timestamp>> request_to_speak_timestamp{};

        static modify_current_user_voice_state create() noexcept {
            return modify_current_user_voice_state{};
        }
        decltype(auto) set_channel_id(this auto&& self, opt<snowflake> cid) noexcept { self.channel_id = cid; return std::forward<decltype(self)>(self); }
        decltype(auto) set_channel(this auto&& self, snowflake cid) noexcept { return std::forward<decltype(self)>(self).set_channel_id(cid); }
        decltype(auto) set_suppress(this auto&& self, opt<bool> s) noexcept { self.suppress = s; return std::forward<decltype(self)>(self); }
        decltype(auto) set_request_to_speak_timestamp(this auto&& self, opt<explicit_null<timestamp>> rts) noexcept { self.request_to_speak_timestamp = rts; return std::forward<decltype(self)>(self); }
    };

    struct modify_user_voice_state {
        opt<snowflake> channel_id{};
        opt<bool> suppress{};

        static modify_user_voice_state create() noexcept {
            return modify_user_voice_state{};
        }
        decltype(auto) set_channel_id(this auto&& self, opt<snowflake> cid) noexcept { self.channel_id = cid; return std::forward<decltype(self)>(self); }
        decltype(auto) set_channel(this auto&& self, snowflake cid) noexcept { return std::forward<decltype(self)>(self).set_channel_id(cid); }
        decltype(auto) set_suppress(this auto&& self, opt<bool> s) noexcept { self.suppress = s; return std::forward<decltype(self)>(self); }
    };
}

namespace oauth2 {
    struct current_authorization_information {
        discusy::application::application application{};
        std::vector<std::string> scopes{};
        timestamp expires{};
        opt<discusy::user::user> user{};

        static current_authorization_information create() noexcept {
            return current_authorization_information{};
        }
        decltype(auto) set_application(this auto&& self, discusy::application::application application_) { self.application = std::move(application_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_scopes(this auto&& self, std::vector<std::string> scopes_) { self.scopes = std::move(scopes_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_scopes, std::string)
        decltype(auto) add_scope(this auto&& self, std::string scope) { self.scopes.emplace_back(std::move(scope)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_expires(this auto&& self, timestamp expires_) noexcept { self.expires = expires_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_user(this auto&& self, opt<discusy::user::user> user_) { self.user = std::move(user_); return std::forward<decltype(self)>(self); }
    };
}
}

template <>
struct glz::meta<discusy::api::application::activity_location_kind> {
    using enum discusy::api::application::activity_location_kind;
    static constexpr auto value = glz::enumerate(
        "gc", guild_channel, 
        "pc", private_channel
    );
};

template <>
struct glz::meta<discusy::api::message::allowed_mentions> {
    using enum discusy::api::message::allowed_mentions;
    static constexpr auto value = glz::enumerate(
        roles, users, everyone
    );
};
