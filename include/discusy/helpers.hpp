#pragma once

#include <cstdint>
#include <concepts>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>
#include <array>
#include <span>

#include <usylibpp/strings.hpp>

#include "api_types.hpp"
#include "events.hpp"
#include "gateway_events.hpp"
#include "http_api.hpp"
#include "types.hpp"

namespace discusy::helpers {

// identity

[[nodiscard]] inline const discusy::user::user* user_of(const interaction::interaction& i) noexcept {
    return i.user_ptr();
}

[[nodiscard]] inline snowflake user_id_of(const interaction::interaction& i) noexcept {
    return i.user_id();
}

[[nodiscard]] inline const discusy::user::user* user_of(const discusy::message::message& m) noexcept {
    return m.user_ptr();
}

[[nodiscard]] inline snowflake user_id_of(const discusy::message::message& m) noexcept {
    return m.user_id();
}

[[nodiscard]] inline const discusy::user::user* user_of(const discusy::guild::guild_member& m) noexcept {
    return m.user_ptr();
}

[[nodiscard]] inline snowflake user_id_of(const discusy::guild::guild_member& m) noexcept {
    return m.user_id();
}

[[nodiscard]] inline std::string_view display_name_of(const interaction::interaction& i) noexcept {
    return i.display_name();
}

[[nodiscard]] inline std::string mention_user(const snowflake id) {
    return id.mention_user();
}

[[nodiscard]] inline std::string mention_channel(const snowflake id) {
    return id.mention_channel();
}

[[nodiscard]] inline std::string mention_role(const snowflake id) {
    return id.mention_role();
}

// command options

namespace detail {
    using option = interaction::application_command_interaction_data_option;
}

[[nodiscard]] inline const detail::option* find_option(
    std::span<const detail::option> options, const std::string_view name) noexcept
{
    for (const auto& o : options) {
        if (o.name == name) return &o;
    }
    return nullptr;
}

// Subcommands and subcommand groups nest their arguments one level down.
[[nodiscard]] inline const detail::option* find_subcommand(
    const std::vector<detail::option>& options) noexcept
{
    for (const auto& o : options) {
        if (o.type == discusy::application_commands::application_command_option_type::SUB_COMMAND ||
            o.type == discusy::application_commands::application_command_option_type::SUB_COMMAND_GROUP) {
            return &o;
        }
    }
    return nullptr;
}

[[nodiscard]] inline const detail::option* find_subcommand(
    const interaction::application_command_data& cmd) noexcept
{
    return cmd.find_subcommand();
}

// The path Discord took to reach the leaf: "group" and/or "sub" may be empty.
using discusy::command_path;

// The options vector the user's actual arguments live in. For a flat command
// that's cmd.options; for `/a b c x:1` it's the options of `c`. Everything
// below goes through this, so get_string(cmd, "x") does the right thing
// whether or not the command has subcommands.
[[nodiscard]] inline const std::vector<detail::option>* option_scope(
    const interaction::application_command_data& cmd) noexcept
{
    return cmd.option_scope();
}

[[nodiscard]] inline command_path subcommand_path(
    const interaction::application_command_data& cmd) noexcept
{
    return cmd.subcommand_path();
}

[[nodiscard]] inline const detail::option* find_option(
    const interaction::application_command_data& cmd, const std::string_view name) noexcept
{
    return cmd.find_option(name);
}

template <typename T>
[[nodiscard]] inline std::optional<T> option_value(const detail::option* o) noexcept {
    return o ? o->template value_as<T>() : std::nullopt;
}

[[nodiscard]] inline std::optional<std::string> get_string(
    const interaction::application_command_data& cmd, const std::string_view name)
{
    return cmd.get_string(name);
}

[[nodiscard]] inline std::optional<integer> get_integer(
    const interaction::application_command_data& cmd, const std::string_view name) noexcept
{
    return cmd.get_integer(name);
}

[[nodiscard]] inline std::optional<double> get_double(
    const interaction::application_command_data& cmd, const std::string_view name) noexcept
{
    return cmd.get_double(name);
}

[[nodiscard]] inline std::optional<bool> get_bool(
    const interaction::application_command_data& cmd, const std::string_view name) noexcept
{
    return cmd.get_bool(name);
}

// USER, CHANNEL, ROLE, MENTIONABLE and ATTACHMENT options all arrive as the id
// in string form. This parses it back without the caller reaching for
// std::from_chars every time.
[[nodiscard]] inline std::optional<snowflake> get_snowflake(
    const interaction::application_command_data& cmd, const std::string_view name)
{
    return cmd.get_snowflake(name);
}

// ---------------------------------------------------------------------------
// resolved lookups
//
// Each returns nullptr rather than throwing or default-constructing, so a
// missing entry is distinguishable from an empty one.
// ---------------------------------------------------------------------------

#define DISCUSY_RESOLVED_GETTER(NAME, FIELD, TYPE)                                          \
    [[nodiscard]] inline const TYPE* NAME(                                                  \
        const interaction::application_command_data& cmd, const snowflake id) noexcept      \
    {                                                                                       \
        return cmd.NAME(id);                                                                \
    }                                                                                       \
                                                                                            \
    [[nodiscard]] inline const TYPE* NAME(                                                  \
        const interaction::application_command_data& cmd, const std::string_view opt_name)  \
    {                                                                                       \
        return cmd.NAME(opt_name);                                                          \
    }

DISCUSY_RESOLVED_GETTER(resolved_attachment, attachments, discusy::message::attachment)
DISCUSY_RESOLVED_GETTER(resolved_user,       users,       discusy::user::user)
DISCUSY_RESOLVED_GETTER(resolved_channel,    channels,    discusy::channel::channel)
DISCUSY_RESOLVED_GETTER(resolved_role,       roles,       discusy::permissions::role)
DISCUSY_RESOLVED_GETTER(resolved_message,    messages,    discusy::message::message)

#undef DISCUSY_RESOLVED_GETTER

// message building

[[nodiscard]] constexpr flags_t<discusy::message::message_flags> ephemeral_flag() noexcept {
    return flags_t<discusy::message::message_flags>{discusy::message::message_flags::EPHEMERAL};
}

[[nodiscard]] constexpr flags_t<discusy::message::message_flags> components_v2_flag() noexcept {
    return flags_t<discusy::message::message_flags>{discusy::message::message_flags::IS_COMPONENTS_V2};
}

[[nodiscard]] inline api::interaction::interaction_callback_data_message text_data(
    std::string content, const bool ephemeral = false)
{
    api::interaction::interaction_callback_data_message data{.content{std::move(content)}};
    if (ephemeral) data.flags = ephemeral_flag();
    return data;
}

[[nodiscard]] inline api::interaction::interaction_callback_data_message embed_data(
    discusy::message::embed e, const bool ephemeral = false)
{
    api::interaction::interaction_callback_data_message data{};
    data.embeds = make_vector(std::move(e));
    if (ephemeral) data.flags = ephemeral_flag();
    return data;
}

// Single-file shorthand. `id` is the attachment index Discord uses to tie the
// multipart part to the message; "0" is right unless you're sending several.
[[nodiscard]] inline http_api::upload_file file(
    std::string filename, std::string data, std::string content_type = "application/octet-stream",
    std::string id = "0")
{
    return http_api::upload_file{
        .filename{std::move(filename)},
        .data{std::move(data)},
        .content_type{std::move(content_type)},
        .id{std::move(id)},
    };
}

[[nodiscard]] inline http_api::upload_file text_file(std::string filename, std::string data) {
    return file(std::move(filename), std::move(data), "text/plain");
}

// interaction dispatch

[[nodiscard]] inline const interaction::application_command_data* command_data(
    const recieve_event::interaction_create& e) noexcept
{
    return e.command_data();
}

[[nodiscard]] inline const interaction::message_component_data* component_data(
    const recieve_event::interaction_create& e) noexcept
{
    return e.component_data();
}

[[nodiscard]] inline const interaction::modal_component_data* modal_data(
    const recieve_event::interaction_create& e) noexcept
{
    return e.modal_data();
}

// autocomplete

[[nodiscard]] inline const detail::option* focused_option(
    const interaction::application_command_data& cmd) noexcept
{
    return cmd.focused_option();
}

// Name + current partial input of the focused option, empty if there isn't one.
using discusy::focused_input;

[[nodiscard]] inline focused_input focused(
    const interaction::application_command_data& cmd) noexcept
{
    return cmd.focused();
}

// One template rather than three overloads, because `choice("x", 5)` against
// separate integer/double overloads is ambiguous.
template <typename V>
[[nodiscard]] inline discusy::application_commands::application_command_option_choice choice(
    std::string name, V&& value)
{
    using bare = std::decay_t<V>;

    discusy::application_commands::application_command_option_choice c{.name{std::move(name)}};

    if constexpr (std::is_floating_point_v<bare>) {
        c.value = static_cast<double>(value);
    } else if constexpr (std::is_integral_v<bare> && !std::is_same_v<bare, bool>) {
        c.value = static_cast<integer>(value);
    } else {
        c.value = std::string{std::forward<V>(value)};
    }

    return c;
}

// name == value, which is what most string choice lists actually want.
[[nodiscard]] inline discusy::application_commands::application_command_option_choice choice(std::string name) {
    auto value = name;
    return choice(std::move(name), std::move(value));
}

// resolved lookups, keyed off an option

[[nodiscard]] inline const discusy::guild::guild_member* resolved_member(
    const interaction::application_command_data& cmd, const snowflake id) noexcept
{
    return cmd.resolved_member(id);
}

[[nodiscard]] inline const discusy::guild::guild_member* resolved_member(
    const interaction::application_command_data& cmd, const std::string_view opt_name)
{
    return cmd.resolved_member(opt_name);
}

// component and modal interactions

[[nodiscard]] inline std::string_view component_custom_id(
    const recieve_event::interaction_create& e) noexcept
{
    return e.component_custom_id();
}

[[nodiscard]] inline const std::vector<std::string>* component_values(
    const recieve_event::interaction_create& e) noexcept
{
    return e.component_values();
}

// Convenience for the overwhelmingly common single-select case.
[[nodiscard]] inline std::string_view component_first_value(
    const recieve_event::interaction_create& e) noexcept
{
    return e.component_first_value();
}

[[nodiscard]] inline std::string_view modal_custom_id(
    const recieve_event::interaction_create& e) noexcept
{
    return e.modal_custom_id();
}

// Pulls a text input's value out of a modal submission by custom_id. Handles
// both the bare TextInput response and the Label-wrapped form Discord uses for
// newer modals, so callers don't have to care which they got.
[[nodiscard]] inline std::string_view modal_text_value(
    const recieve_event::interaction_create& e, const std::string_view custom_id) noexcept
{
    return e.modal_text_value(custom_id);
}

// Same, but for a string select living inside a modal.
[[nodiscard]] inline const std::vector<std::string>* modal_select_values(
    const recieve_event::interaction_create& e, const std::string_view custom_id) noexcept
{
    return e.modal_select_values(custom_id);
}

// permissions

[[nodiscard]] inline bool bot_can(
    const interaction::interaction& i, const permissions::permissions perm) noexcept
{
    return i.bot_can(perm);
}

[[nodiscard]] inline bool user_can(
    const interaction::interaction& i, const permissions::permissions perm) noexcept
{
    return i.user_can(perm);
}

// embeds

namespace colour {
    inline constexpr integer none    = 0;
    inline constexpr integer red     = 0xED4245;
    inline constexpr integer green   = 0x57F287;
    inline constexpr integer yellow  = 0xFEE75C;
    inline constexpr integer blurple = 0x5865F2;
    inline constexpr integer fuchsia = 0xEB459E;
    inline constexpr integer white   = 0xFFFFFF;
    inline constexpr integer grey    = 0x99AAB5;
}

[[nodiscard]] inline discusy::message::embed embed(
    std::string title, std::string description, const integer colour_value = colour::blurple)
{
    return discusy::message::embed{
        .title{std::move(title)},
        .description{std::move(description)},
        .color{colour_value},
    };
}

[[nodiscard]] inline discusy::message::embed_field field(
    std::string name, std::string value, const bool inline_field = false)
{
    return discusy::message::embed_field{
        .name{std::move(name)},
        .value{std::move(value)},
        .inline_{inline_field},
    };
}

inline discusy::message::embed& add_field(
    discusy::message::embed& e, std::string name, std::string value, const bool inline_field = false)
{
    return e.add_field(std::move(name), std::move(value), inline_field);
}

inline discusy::message::embed& set_footer(discusy::message::embed& e, std::string text) {
    return e.set_footer(std::move(text));
}

inline discusy::message::embed& set_author(discusy::message::embed& e, std::string name, std::string icon_url = {}) {
    return e.set_author(std::move(name), std::move(icon_url));
}

inline discusy::message::embed& set_thumbnail(discusy::message::embed& e, std::string url) {
    return e.set_thumbnail(std::move(url));
}

inline discusy::message::embed& set_image(discusy::message::embed& e, std::string url) {
    return e.set_image(std::move(url));
}

// components

[[nodiscard]] inline components::Button button(
    const components::button_style style, std::string label, std::string custom_id)
{
    return components::Button{
        .style = style,
        .label{std::move(label)},
        .custom_id{std::move(custom_id)},
    };
}

[[nodiscard]] inline components::Button link_button(std::string label, std::string url) {
    return components::Button{
        .style = components::button_style::Link,
        .label{std::move(label)},
        .url{std::move(url)},
    };
}

[[nodiscard]] inline components::select_option select_option(
    std::string label, std::string value, std::string description = {})
{
    components::select_option o{.label{std::move(label)}, .value{std::move(value)}};
    if (!description.empty()) o.description = std::move(description);
    return o;
}

[[nodiscard]] inline components::StringSelect string_select(
    std::string custom_id, std::vector<components::select_option> options, std::string placeholder = {})
{
    components::StringSelect s{
        .custom_id{std::move(custom_id)},
        .options{std::move(options)},
    };
    if (!placeholder.empty()) s.placeholder = std::move(placeholder);
    return s;
}

[[nodiscard]] inline components::TextInput text_input(
    std::string custom_id, const components::text_input_style style = components::text_input_style::Short,
    const bool required = true)
{
    return components::TextInput{
        .custom_id{std::move(custom_id)},
        .style = style,
        .required{required},
    };
}

[[nodiscard]] inline components::TextDisplay text_display(std::string content) {
    return components::TextDisplay{.content{std::move(content)}};
}

[[nodiscard]] constexpr components::Separator separator(const bool divider = true) noexcept {
    return components::Separator{.divider{divider}};
}

// Any of Button / StringSelect / UserSelect / RoleSelect / MentionableSelect /
// ChannelSelect, in one row.
template <typename... Cs>
[[nodiscard]] inline components::ActionRow action_row(Cs&&... cs) {
    components::ActionRow row{};
    row.components.reserve(sizeof...(Cs));
    (row.components.emplace_back(components::action_row_component{std::forward<Cs>(cs)}), ...);
    return row;
}

// Wrap any top-level component for a message's `components` vector.
template <typename C>
[[nodiscard]] inline components::component as_component(C&& c) {
    return components::component{std::forward<C>(c)};
}

// The common case: a single row of buttons, ready to hand to a message.
template <typename... Cs>
[[nodiscard]] inline std::vector<components::component> button_row(Cs&&... cs) {
    std::vector<components::component> out;
    out.emplace_back(as_component(action_row(std::forward<Cs>(cs)...)));
    return out;
}

// application command definitions

namespace cmd {

using option_type = discusy::application_commands::application_command_option_type;
using option_t = discusy::application_commands::application_command_option;

[[nodiscard]] inline option_t option(
    const option_type type, std::string name, std::string description, const bool required = false)
{
    option_t o{
        .type = type,
        .name{std::move(name)},
        .description{std::move(description)},
    };
    if (required) o.required = true;
    return o;
}

#define DISCUSY_CMD_OPTION(FN, TYPE)                                                    \
    [[nodiscard]] inline option_t FN(                                                   \
        std::string name, std::string description, const bool required = false)         \
    {                                                                                   \
        return option(option_type::TYPE, std::move(name), std::move(description), required); \
    }

DISCUSY_CMD_OPTION(string_opt,      STRING)
DISCUSY_CMD_OPTION(integer_opt,     INTEGER)
DISCUSY_CMD_OPTION(bool_opt,        BOOLEAN)
DISCUSY_CMD_OPTION(user_opt,        USER)
DISCUSY_CMD_OPTION(channel_opt,     CHANNEL)
DISCUSY_CMD_OPTION(role_opt,        ROLE)
DISCUSY_CMD_OPTION(mentionable_opt, MENTIONABLE)
DISCUSY_CMD_OPTION(number_opt,      NUMBER)
DISCUSY_CMD_OPTION(attachment_opt,  ATTACHMENT)

#undef DISCUSY_CMD_OPTION

[[nodiscard]] inline option_t autocompleted(option_t o) {
    return std::move(o).autocompleted();
}

[[nodiscard]] inline option_t with_range(option_t o, const integer min, const integer max) {
    return std::move(o).with_range(min, max);
}

[[nodiscard]] inline option_t with_min(option_t o, const integer min) {
    return std::move(o).with_min(min);
}

[[nodiscard]] inline option_t with_number_min(option_t o, const double min) {
    return std::move(o).with_number_min(min);
}

[[nodiscard]] inline option_t with_number_range(option_t o, const double min, const double max) {
    return std::move(o).with_number_range(min, max);
}

[[nodiscard]] inline option_t with_length(option_t o, const std::uint16_t min, const std::uint16_t max) {
    return std::move(o).with_length(min, max);
}

[[nodiscard]] inline option_t with_choices(
    option_t o, std::vector<discusy::application_commands::application_command_option_choice> choices)
{
    return std::move(o).with_choices(std::move(choices));
}

template <typename... Cs>
    requires (sizeof...(Cs) > 0 && (std::convertible_to<Cs, discusy::application_commands::application_command_option_choice> && ...))
[[nodiscard]] inline option_t with_choices(option_t o, Cs&&... choices) {
    return std::move(o).with_choices(make_vector<discusy::application_commands::application_command_option_choice>(std::forward<Cs>(choices)...));
}

[[nodiscard]] inline option_t with_channel_types(
    option_t o, std::vector<discusy::channel::channel_type> types)
{
    return std::move(o).with_channel_types(std::move(types));
}

[[nodiscard]] inline option_t sub_command(
    std::string name, std::string description, std::vector<option_t> options = {})
{
    option_t o{
        .type = option_type::SUB_COMMAND,
        .name{std::move(name)},
        .description{std::move(description)},
    };
    if (!options.empty()) o.options = std::move(options);
    return o;
}

template <typename... Options>
    requires (sizeof...(Options) > 0 && (std::convertible_to<Options, option_t> && ...))
[[nodiscard]] inline option_t sub_command(
    std::string name, std::string description, Options&&... options)
{
    return sub_command(std::move(name), std::move(description), make_vector<option_t>(std::forward<Options>(options)...));
}

[[nodiscard]] inline option_t sub_command_group(
    std::string name, std::string description, std::vector<option_t> subcommands)
{
    return option_t{
        .type = option_type::SUB_COMMAND_GROUP,
        .name{std::move(name)},
        .description{std::move(description)},
        .options{std::move(subcommands)},
    };
}

template <typename... Subcommands>
    requires (sizeof...(Subcommands) > 0 && (std::convertible_to<Subcommands, option_t> && ...))
[[nodiscard]] inline option_t sub_command_group(
    std::string name, std::string description, Subcommands&&... subcommands)
{
    return sub_command_group(std::move(name), std::move(description), make_vector<option_t>(std::forward<Subcommands>(subcommands)...));
}

[[nodiscard]] inline const std::unordered_set<interaction::interaction_context_type>& guild_only() {
    static const std::unordered_set<interaction::interaction_context_type> set{
        interaction::interaction_context_type::GUILD,
    };
    return set;
}

[[nodiscard]] inline api::application_commands::application_command slash(
    std::string name, std::string description, std::vector<option_t> options = {})
{
    api::application_commands::application_command c{
        .name{std::move(name)},
        .description{std::move(description)},
        .default_member_permissions{permissions_t{}},
        .contexts{guild_only()},
    };
    if (!options.empty()) c.options = std::move(options);
    return c;
}

template <typename... Options>
    requires (sizeof...(Options) > 0 && (std::convertible_to<Options, option_t> && ...))
[[nodiscard]] inline api::application_commands::application_command slash(
    std::string name, std::string description, Options&&... options)
{
    return slash(std::move(name), std::move(description), make_vector<option_t>(std::forward<Options>(options)...));
}

template <typename... Cmds>
    requires (sizeof...(Cmds) > 0 && (std::convertible_to<Cmds, api::application_commands::application_command> && ...))
[[nodiscard]] inline std::array<api::application_commands::application_command, sizeof...(Cmds)> commands(Cmds&&... cmds) {
    return make_array<api::application_commands::application_command>(std::forward<Cmds>(cmds)...);
}

}

// interaction dispatch

[[nodiscard]] inline std::string_view command_name(
    const recieve_event::interaction_create& e) noexcept
{
    return e.command_name();
}

[[nodiscard]] inline bool is_command(
    const recieve_event::interaction_create& e) noexcept
{
    return e.is_command();
}

[[nodiscard]] inline bool is_autocomplete(
    const recieve_event::interaction_create& e) noexcept
{
    return e.is_autocomplete();
}

[[nodiscard]] inline bool is_component(
    const recieve_event::interaction_create& e) noexcept
{
    return e.is_component();
}

[[nodiscard]] inline bool is_modal_submit(
    const recieve_event::interaction_create& e) noexcept
{
    return e.is_modal_submit();
}

[[nodiscard]] inline snowflake guild_id_of(const interaction::interaction& i) noexcept {
    return i.guild_id_of();
}

[[nodiscard]] inline snowflake channel_id_of(const interaction::interaction& i) noexcept {
    return i.channel_id_of();
}

[[nodiscard]] inline bool in_guild(const interaction::interaction& i) noexcept {
    return i.in_guild();
}

}
