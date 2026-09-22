#pragma once

#include <boost/asio.hpp>
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <utility>
#include <stdexcept>
#include <array>

#include <cctype>

#include "types.hpp"
#include "events.hpp"
#include "api_types.hpp"
#include "api_constants.hpp"
#include "helpers.hpp"
#include "discusy.hpp"

namespace discusy {

namespace detail {

[[noreturn]] inline void throw_unbound_bot() {
    throw std::logic_error("discusy: object is not bound to a bot instance (bot_ptr_ is null)");
}

inline void check_bot(const bot* b) {
    if (b == nullptr) [[unlikely]] {
        throw_unbound_bot();
    }
}

[[nodiscard]] inline bool is_media_filename(const std::string_view filename, const std::string_view content_type) noexcept {
    if (content_type.starts_with("image/") || content_type.starts_with("video/")) {
        return true;
    }
    auto ends_with_ci = [](const std::string_view str, const std::string_view suffix) noexcept -> bool {
        if (str.size() < suffix.size()) return false;
        const auto str_end = str.substr(str.size() - suffix.size());
        for (std::size_t i = 0; i < suffix.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(str_end[i])) != std::tolower(static_cast<unsigned char>(suffix[i]))) {
                return false;
            }
        }
        return true;
    };
    return ends_with_ci(filename, ".png") || ends_with_ci(filename, ".jpg") ||
           ends_with_ci(filename, ".jpeg") || ends_with_ci(filename, ".webp") ||
           ends_with_ci(filename, ".gif") || ends_with_ci(filename, ".mp4") ||
           ends_with_ci(filename, ".mov") || ends_with_ci(filename, ".webm");
}

[[nodiscard]] inline std::vector<components::component> make_v2_file_components(const upload_files_param& files) {
    std::vector<components::component> result;
    std::vector<components::component> non_media;
    components::MediaGallery current_gallery{};

    auto process_file = [&](const auto& file) {
        if (file.filename.empty()) return;
        std::string attachment_url = ulp::str::concat_strings("attachment://", file.filename);
        const bool spoiler = file.filename.starts_with("SPOILER_");

        if (is_media_filename(file.filename, file.content_type)) {
            if (current_gallery.items.size() >= 10) {
                result.emplace_back(std::move(current_gallery));
                current_gallery = components::MediaGallery{};
            }
            current_gallery.add_item(std::move(attachment_url), std::nullopt, spoiler);
        } else {
            non_media.emplace_back(components::File::create(std::move(attachment_url), std::string{file.filename}, spoiler));
        }
    };

    std::visit([&process_file](const auto& span_files) {
        for (const auto& f : span_files) {
            process_file(f);
        }
    }, files.files);

    if (!current_gallery.items.empty()) {
        result.emplace_back(std::move(current_gallery));
    }

    for (auto&& item : non_media) {
        result.emplace_back(std::move(item));
    }

    return result;
}

}

// user::user

template <bool ReturnResult, typename CompletionToken>
auto discusy::user::user::create_dm(this auto&& self, CompletionToken&& token) {
    detail::check_bot(self.bot_ptr_);
    return self.bot_ptr_->api.template create_dm<ReturnResult>(
        api::user::create_dm{.recipient_id{self.id}}
    )(std::forward<CompletionToken>(token));
}

// guild::guild_member

template <bool ReturnResult, typename CompletionToken>
auto discusy::guild::guild_member::add_role_api(this auto&& self, const snowflake role_id, CompletionToken&& token) {
    detail::check_bot(self.bot_ptr_);
    if (!self.guild_id_) throw std::logic_error("discusy: guild_member has no guild_id context");
    if (!self.user) throw std::logic_error("discusy: guild_member has no user object");
    return self.bot_ptr_->api.template add_guild_member_role<ReturnResult>(*self.guild_id_, self.user->id, role_id)(std::forward<CompletionToken>(token));
}

template <bool ReturnResult, typename CompletionToken>
auto discusy::guild::guild_member::remove_role_api(this auto&& self, const snowflake role_id, CompletionToken&& token) {
    detail::check_bot(self.bot_ptr_);
    if (!self.guild_id_) throw std::logic_error("discusy: guild_member has no guild_id context");
    if (!self.user) throw std::logic_error("discusy: guild_member has no user object");
    return self.bot_ptr_->api.template remove_guild_member_role<ReturnResult>(*self.guild_id_, self.user->id, role_id)(std::forward<CompletionToken>(token));
}

template <bool ReturnResult, typename CompletionToken>
auto discusy::guild::guild_member::kick(this auto&& self, CompletionToken&& token) {
    detail::check_bot(self.bot_ptr_);
    if (!self.guild_id_) throw std::logic_error("discusy: guild_member has no guild_id context");
    if (!self.user) throw std::logic_error("discusy: guild_member has no user object");
    return self.bot_ptr_->api.template remove_guild_member<ReturnResult>(*self.guild_id_, self.user->id)(std::forward<CompletionToken>(token));
}

template <bool ReturnResult, typename CompletionToken>
auto discusy::guild::guild_member::ban(this auto&& self, const integer delete_message_days, CompletionToken&& token) {
    detail::check_bot(self.bot_ptr_);
    if (!self.guild_id_) throw std::logic_error("discusy: guild_member has no guild_id context");
    if (!self.user) throw std::logic_error("discusy: guild_member has no user object");
    return self.bot_ptr_->api.template create_guild_ban<ReturnResult>(*self.guild_id_, self.user->id, api::guild::create_guild_ban{.delete_message_days{delete_message_days}})(std::forward<CompletionToken>(token));
}

// guild::guild

template <bool ReturnResult, typename CompletionToken>
auto discusy::guild::guild::fetch_member(this auto&& self, const snowflake user_id, CompletionToken&& token) {
    detail::check_bot(self.bot_ptr_);
    return self.bot_ptr_->api.template get_guild_member<ReturnResult>(self.id, user_id)(std::forward<CompletionToken>(token));
}

template <bool ReturnResult, typename CompletionToken>
auto discusy::guild::guild::kick(this auto&& self, const snowflake user_id, CompletionToken&& token) {
    detail::check_bot(self.bot_ptr_);
    return self.bot_ptr_->api.template remove_guild_member<ReturnResult>(self.id, user_id)(std::forward<CompletionToken>(token));
}

template <bool ReturnResult, typename CompletionToken>
auto discusy::guild::guild::ban(this auto&& self, const snowflake user_id, const integer delete_message_days, CompletionToken&& token) {
    detail::check_bot(self.bot_ptr_);
    return self.bot_ptr_->api.template create_guild_ban<ReturnResult>(self.id, user_id, api::guild::create_guild_ban{.delete_message_days{delete_message_days}})(std::forward<CompletionToken>(token));
}

template <bool ReturnResult, typename CompletionToken>
auto discusy::guild::guild::unban(this auto&& self, const snowflake user_id, CompletionToken&& token) {
    detail::check_bot(self.bot_ptr_);
    return self.bot_ptr_->api.template remove_guild_ban<ReturnResult>(self.id, user_id)(std::forward<CompletionToken>(token));
}

template <bool ReturnResult, typename CompletionToken>
auto discusy::guild::guild::add_member_role(this auto&& self, const snowflake user_id, const snowflake role_id, CompletionToken&& token) {
    detail::check_bot(self.bot_ptr_);
    return self.bot_ptr_->api.template add_guild_member_role<ReturnResult>(self.id, user_id, role_id)(std::forward<CompletionToken>(token));
}

template <bool ReturnResult, typename CompletionToken>
auto discusy::guild::guild::remove_member_role(this auto&& self, const snowflake user_id, const snowflake role_id, CompletionToken&& token) {
    detail::check_bot(self.bot_ptr_);
    return self.bot_ptr_->api.template remove_guild_member_role<ReturnResult>(self.id, user_id, role_id)(std::forward<CompletionToken>(token));
}

template <bool ReturnResult, typename CompletionToken>
auto discusy::guild::guild::create_channel(this auto&& self, std::string name, const discusy::channel::channel_type type, CompletionToken&& token) {
    detail::check_bot(self.bot_ptr_);
    return self.bot_ptr_->api.template create_guild_channel<ReturnResult>(self.id, api::guild::create_guild_channel{.name{std::move(name)}, .type{type}})(std::forward<CompletionToken>(token));
}

// channel::channel

template <bool ReturnResult, typename CompletionToken>
auto discusy::channel::channel::send(this auto&& self, std::string content, CompletionToken&& token) {
    detail::check_bot(self.bot_ptr_);
    return self.bot_ptr_->api.template create_message<ReturnResult>(self.id, api::message::create_message{.content{std::move(content)}})(std::forward<CompletionToken>(token));
}

template <bool ReturnResult, typename CompletionToken>
auto discusy::channel::channel::send_embed(this auto&& self, discusy::message::embed e, CompletionToken&& token) {
    detail::check_bot(self.bot_ptr_);
    return self.bot_ptr_->api.template create_message<ReturnResult>(self.id, api::message::create_message{.embeds{make_vector(std::move(e))}})(std::forward<CompletionToken>(token));
}


template <bool ReturnResult, typename CompletionToken>
auto discusy::channel::channel::send_file(this auto&& self, http_api::upload_file_view f, std::string content, CompletionToken&& token) {
    const std::array<http_api::upload_file_view, 1> arr{f};
    return self.template send_files<ReturnResult>(arr, std::move(content), std::forward<CompletionToken>(token));
}

template <bool ReturnResult, typename CompletionToken>
auto discusy::channel::channel::send_files(this auto&& self, http_api::upload_files_param files, std::string content, CompletionToken&& token) {
    detail::check_bot(self.bot_ptr_);
    api::message::create_message msg{};
    if (!content.empty()) msg.content = std::move(content);
    return self.bot_ptr_->api.template create_message<ReturnResult>(self.id, msg, files)(std::forward<CompletionToken>(token));
}

template <bool ReturnResult, typename CompletionToken>
auto discusy::channel::channel::send_with(this auto&& self, api::message::create_message msg, CompletionToken&& token) {
    detail::check_bot(self.bot_ptr_);
    return self.bot_ptr_->api.template create_message<ReturnResult>(self.id, std::move(msg))(std::forward<CompletionToken>(token));
}

template <bool ReturnResult, typename CompletionToken>
auto discusy::channel::channel::delete_channel(this auto&& self, CompletionToken&& token) {
    detail::check_bot(self.bot_ptr_);
    return self.bot_ptr_->api.template delete_channel<ReturnResult>(self.id)(std::forward<CompletionToken>(token));
}

template <bool ReturnResult, typename CompletionToken>
auto discusy::channel::channel::create_thread(this auto&& self, std::string name, CompletionToken&& token) {
    detail::check_bot(self.bot_ptr_);
    return self.bot_ptr_->api.template start_thread_without_message<ReturnResult>(self.id, api::channels::start_thread_without_message{.name{std::move(name)}})(std::forward<CompletionToken>(token));
}

// message::message

template <bool ReturnResult, typename CompletionToken>
auto discusy::message::message::send(this auto&& self, CompletionToken&& token) {
    detail::check_bot(self.bot_ptr_);
    return self.bot_ptr_->api.template create_message<ReturnResult>(
        self.channel_id, 
        api::message::create_message::from(std::forward<decltype(self)>(self))
    )(std::forward<CompletionToken>(token));
}

template <bool ReturnResult, typename CompletionToken>
auto discusy::message::message::reply(this auto&& self, std::string content, const bool ping, CompletionToken&& token) {
    detail::check_bot(self.bot_ptr_);
    api::message::create_message msg{.content{std::move(content)}};
    msg.message_reference = discusy::message::message_reference{.message_id{self.id}, .channel_id{self.channel_id}};
    if (!ping) {
        msg.allowed_mentions = api::message::allowed_mentions_obj{.replied_user{false}};
    }
    return self.bot_ptr_->api.template create_message<ReturnResult>(self.channel_id, msg)(std::forward<CompletionToken>(token));
}

template <bool ReturnResult, typename CompletionToken>
auto discusy::message::message::reply_embed(this auto&& self, discusy::message::embed e, const bool ping, CompletionToken&& token) {
    detail::check_bot(self.bot_ptr_);
    api::message::create_message msg{.embeds{make_vector(std::move(e))}};
    msg.message_reference = discusy::message::message_reference{.message_id{self.id}, .channel_id{self.channel_id}};
    if (!ping) {
        msg.allowed_mentions = api::message::allowed_mentions_obj{.replied_user{false}};
    }
    return self.bot_ptr_->api.template create_message<ReturnResult>(self.channel_id, msg)(std::forward<CompletionToken>(token));
}


template <bool ReturnResult, typename CompletionToken>
auto discusy::message::message::reply_file(this auto&& self, http_api::upload_file_view f, std::string content, const bool ping, CompletionToken&& token) {
    const std::array<http_api::upload_file_view, 1> arr{f};
    return self.template reply_files<ReturnResult>(arr, std::move(content), ping, std::forward<CompletionToken>(token));
}

template <bool ReturnResult, typename CompletionToken>
auto discusy::message::message::reply_files(this auto&& self, http_api::upload_files_param files, std::string content, const bool ping, CompletionToken&& token) {
    detail::check_bot(self.bot_ptr_);
    api::message::create_message msg{};
    if (!content.empty()) msg.content = std::move(content);
    msg.message_reference = discusy::message::message_reference{.message_id{self.id}, .channel_id{self.channel_id}};
    if (!ping) {
        msg.allowed_mentions = api::message::allowed_mentions_obj{.replied_user{false}};
    }
    return self.bot_ptr_->api.template create_message<ReturnResult>(self.channel_id, msg, files)(std::forward<CompletionToken>(token));
}

template <bool ReturnResult, typename CompletionToken>
auto discusy::message::message::reply_with(this auto&& self, api::message::create_message msg, CompletionToken&& token) {
    detail::check_bot(self.bot_ptr_);
    auto m = std::move(msg);
    if (!m.message_reference) {
        m.message_reference = discusy::message::message_reference{.message_id{self.id}, .channel_id{self.channel_id}};
    }
    return self.bot_ptr_->api.template create_message<ReturnResult>(self.channel_id, std::move(m))(std::forward<CompletionToken>(token));
}

template <bool ReturnResult, typename CompletionToken>
auto discusy::message::message::edit(this auto&& self, std::string content, CompletionToken&& token) {
    detail::check_bot(self.bot_ptr_);
    return self.bot_ptr_->api.template edit_message<ReturnResult>(self.channel_id, self.id, api::message::edit_message{.content{std::move(content)}})(std::forward<CompletionToken>(token));
}

template <bool ReturnResult, typename CompletionToken>
auto discusy::message::message::edit_embed(this auto&& self, discusy::message::embed e, CompletionToken&& token) {
    detail::check_bot(self.bot_ptr_);
    return self.bot_ptr_->api.template edit_message<ReturnResult>(self.channel_id, self.id, api::message::edit_message{.embeds{make_vector(std::move(e))}})(std::forward<CompletionToken>(token));
}

template <bool ReturnResult, typename CompletionToken>
auto discusy::message::message::edit_with(this auto&& self, api::message::edit_message msg, CompletionToken&& token) {
    detail::check_bot(self.bot_ptr_);
    return self.bot_ptr_->api.template edit_message<ReturnResult>(self.channel_id, self.id, std::move(msg))(std::forward<CompletionToken>(token));
}

template <bool ReturnResult, typename CompletionToken>
auto discusy::message::message::delete_message(this auto&& self, CompletionToken&& token) {
    detail::check_bot(self.bot_ptr_);
    return self.bot_ptr_->api.template delete_message<ReturnResult>(self.channel_id, self.id)(std::forward<CompletionToken>(token));
}

template <bool ReturnResult, typename CompletionToken>
auto discusy::message::message::add_reaction(this auto&& self, std::string emoji, CompletionToken&& token) {
    detail::check_bot(self.bot_ptr_);
    return self.bot_ptr_->api.template create_reaction<ReturnResult>(self.channel_id, self.id, std::move(emoji))(std::forward<CompletionToken>(token));
}

template <bool ReturnResult, typename CompletionToken>
auto discusy::message::message::remove_reaction(this auto&& self, std::string emoji, const snowflake user_id, CompletionToken&& token) {
    detail::check_bot(self.bot_ptr_);
    if (user_id.value == 0) {
        return self.bot_ptr_->api.template delete_own_reaction<ReturnResult>(self.channel_id, self.id, std::move(emoji))(std::forward<CompletionToken>(token));
    }
    return self.bot_ptr_->api.template delete_user_reaction<ReturnResult>(self.channel_id, self.id, std::move(emoji), user_id)(std::forward<CompletionToken>(token));
    
}

template <bool ReturnResult, typename CompletionToken>
auto discusy::message::message::pin(this auto&& self, CompletionToken&& token) {
    detail::check_bot(self.bot_ptr_);
    return self.bot_ptr_->api.template pin_message<ReturnResult>(self.channel_id, self.id)(std::forward<CompletionToken>(token));
}

template <bool ReturnResult, typename CompletionToken>
auto discusy::message::message::unpin(this auto&& self, CompletionToken&& token) {
    detail::check_bot(self.bot_ptr_);
    return self.bot_ptr_->api.template unpin_message<ReturnResult>(self.channel_id, self.id)(std::forward<CompletionToken>(token));
}

template <bool ReturnResult, typename CompletionToken>
auto discusy::message::message::create_thread(this auto&& self, std::string name, CompletionToken&& token) {
    detail::check_bot(self.bot_ptr_);
    return self.bot_ptr_->api.template start_thread_from_message<ReturnResult>(self.channel_id, self.id, api::channels::start_thread_from_message{.name{std::move(name)}})(std::forward<CompletionToken>(token));
}

// interaction::interaction

template <bool ReturnResult, typename CompletionToken>
auto interaction::interaction::reply(this auto&& self, std::string content, const bool ephemeral, CompletionToken&& token) {
    detail::check_bot(self.bot_ptr_);
    return self.bot_ptr_->api.template create_interaction_response<ReturnResult>(self.id, self.token,
        api::interaction::interaction_response{
            .type = api::interaction::interaction_callback_type::CHANNEL_MESSAGE_WITH_SOURCE,
            .data{helpers::text_data(std::move(content), ephemeral)},
        })(std::forward<CompletionToken>(token));
}

template <bool ReturnResult, typename CompletionToken>
auto interaction::interaction::reply_ephemeral(this auto&& self, std::string content, CompletionToken&& token) {
    return self.template reply<ReturnResult>(std::move(content), true, std::forward<CompletionToken>(token));
}

template <bool ReturnResult, typename CompletionToken>
auto interaction::interaction::reply_with(this auto&& self, api::interaction::interaction_callback_data_message data, CompletionToken&& token) {
    detail::check_bot(self.bot_ptr_);
    return self.bot_ptr_->api.template create_interaction_response<ReturnResult>(self.id, self.token,
        api::interaction::interaction_response{
            .type = api::interaction::interaction_callback_type::CHANNEL_MESSAGE_WITH_SOURCE,
            .data{std::move(data)},
        })(std::forward<CompletionToken>(token));
}

template <bool ReturnResult, typename CompletionToken>
auto interaction::interaction::reply_embed(this auto&& self, discusy::message::embed e, const bool ephemeral, CompletionToken&& token) {
    return self.template reply_with<ReturnResult>(helpers::embed_data(std::move(e), ephemeral), std::forward<CompletionToken>(token));
}

template <bool ReturnResult, typename CompletionToken>
auto interaction::interaction::reply_components(this auto&& self, std::vector<components::component> components_, const bool ephemeral, CompletionToken&& token) {
    api::interaction::interaction_callback_data_message data{};
    if (ephemeral) data.add_flag(discusy::message::message_flags::EPHEMERAL);
    data.set_components(std::move(components_));
    return self.template reply_with<ReturnResult>(std::move(data), std::forward<CompletionToken>(token));
}


template <bool ReturnResult, typename CompletionToken>
auto interaction::interaction::reply_file(this auto&& self, http_api::upload_file_view f, std::string content, const bool ephemeral, CompletionToken&& token) {
    const std::array<http_api::upload_file_view, 1> arr{f};
    return self.template reply_files<ReturnResult>(arr, std::move(content), ephemeral, std::forward<CompletionToken>(token));
}

template <bool ReturnResult, typename CompletionToken>
auto interaction::interaction::reply_files(this auto&& self, http_api::upload_files_param files, std::string content, const bool ephemeral, CompletionToken&& token) {
    detail::check_bot(self.bot_ptr_);
    api::interaction::interaction_callback_data_message data{};
    if (!content.empty()) data.content = std::move(content);
    if (ephemeral) data.flags = helpers::ephemeral_flag();

    return self.bot_ptr_->api.template create_interaction_response<ReturnResult>(self.id, self.token,
        api::interaction::interaction_response{
            .type = api::interaction::interaction_callback_type::CHANNEL_MESSAGE_WITH_SOURCE,
            .data{std::move(data)},
        },
        files)(std::forward<CompletionToken>(token));
}

template <bool ReturnResult, typename CompletionToken>
auto interaction::interaction::defer(this auto&& self, const bool ephemeral, CompletionToken&& token) {
    detail::check_bot(self.bot_ptr_);
    api::interaction::interaction_response resp{
        .type = api::interaction::interaction_callback_type::DEFERRED_CHANNEL_MESSAGE_WITH_SOURCE,
    };
    if (ephemeral) {
        resp.data = api::interaction::interaction_callback_data_message{.flags{helpers::ephemeral_flag()}};
    }
    return self.bot_ptr_->api.template create_interaction_response<ReturnResult>(self.id, self.token, resp)(std::forward<CompletionToken>(token));
}

template <bool ReturnResult, typename CompletionToken>
auto interaction::interaction::thinking(this auto&& self, const bool ephemeral, CompletionToken&& token) {
    return self.template defer<ReturnResult>(ephemeral, std::forward<CompletionToken>(token));
}

template <bool ReturnResult, typename CompletionToken>
auto interaction::interaction::defer_ephemeral(this auto&& self, CompletionToken&& token) {
    return self.template defer<ReturnResult>(true, std::forward<CompletionToken>(token));
}

template <bool ReturnResult, typename CompletionToken>
auto interaction::interaction::edit_reply(this auto&& self, std::string content, CompletionToken&& token) {
    detail::check_bot(self.bot_ptr_);
    return self.bot_ptr_->api.template edit_original_interaction_response<ReturnResult>(self.token,
        api::webhook::edit_webhook_message{.content{std::move(content)}})(std::forward<CompletionToken>(token));
}

template <bool ReturnResult, typename CompletionToken>
auto interaction::interaction::edit_reply_embed(this auto&& self, discusy::message::embed e, CompletionToken&& token) {
    detail::check_bot(self.bot_ptr_);
    return self.bot_ptr_->api.template edit_original_interaction_response<ReturnResult>(self.token,
        api::webhook::edit_webhook_message{
            .embeds{make_vector(std::move(e))},
        })(std::forward<CompletionToken>(token));
}

template <bool ReturnResult, typename CompletionToken>
auto interaction::interaction::edit_reply_with(this auto&& self, api::webhook::edit_webhook_message msg, CompletionToken&& token) {
    detail::check_bot(self.bot_ptr_);
    return self.bot_ptr_->api.template edit_original_interaction_response<ReturnResult>(self.token, std::move(msg))(std::forward<CompletionToken>(token));
}

template <bool ReturnResult, typename CompletionToken>
auto interaction::interaction::clear_components(this auto&& self, CompletionToken&& token) {
    detail::check_bot(self.bot_ptr_);
    return self.bot_ptr_->api.template edit_original_interaction_response<ReturnResult>(self.token,
        api::webhook::edit_webhook_message{
            .components{std::vector<components::component>{}},
        })(std::forward<CompletionToken>(token));
}

template <bool ReturnResult, typename CompletionToken>
auto interaction::interaction::followup(this auto&& self, std::string content, const bool ephemeral, CompletionToken&& token) {
    detail::check_bot(self.bot_ptr_);
    api::webhook::execute_webhook msg{.content{std::move(content)}};
    if (ephemeral) msg.flags = helpers::ephemeral_flag();
    return self.bot_ptr_->api.template create_followup_message<ReturnResult>(self.token, msg)(std::forward<CompletionToken>(token));
}

template <bool ReturnResult, typename CompletionToken>
auto interaction::interaction::followup_with(this auto&& self, api::webhook::execute_webhook msg, CompletionToken&& token) {
    detail::check_bot(self.bot_ptr_);
    return self.bot_ptr_->api.template create_followup_message<ReturnResult>(self.token, msg)(std::forward<CompletionToken>(token));
}


template <bool ReturnResult, typename CompletionToken>
auto interaction::interaction::followup_file(this auto&& self, http_api::upload_file_view f, std::string content, CompletionToken&& token) {
    const std::array<http_api::upload_file_view, 1> arr{f};
    return self.template followup_files<ReturnResult>(arr, std::move(content), std::forward<CompletionToken>(token));
}

template <bool ReturnResult, typename CompletionToken>
auto interaction::interaction::followup_files(this auto&& self, http_api::upload_files_param files, std::string content, CompletionToken&& token) {
    detail::check_bot(self.bot_ptr_);
    api::webhook::execute_webhook msg{};
    if (!content.empty()) msg.content = std::move(content);
    return self.bot_ptr_->api.template create_followup_message<ReturnResult>(self.token, msg, files)(std::forward<CompletionToken>(token));
}

template <bool ReturnResult, typename CompletionToken>
auto interaction::interaction::autocomplete(this auto&& self, std::vector<discusy::application_commands::application_command_option_choice> choices, CompletionToken&& token_) {
    detail::check_bot(self.bot_ptr_);
    constexpr auto limit = static_cast<std::size_t>(discusy::constants::MAX_AUTOCOMPLETE_CHOICES);
    if (choices.size() > limit) choices.resize(limit);
    return self.bot_ptr_->api.template create_interaction_response<ReturnResult>(self.id, self.token,
        api::interaction::interaction_response{
            .type = api::interaction::interaction_callback_type::APPLICATION_COMMAND_AUTOCOMPLETE_RESULT,
            .data{api::interaction::interaction_callback_data_autocomplete{.choices{std::move(choices)}}},
        })(std::forward<CompletionToken>(token_));
}

template <bool ReturnResult, typename CompletionToken>
auto interaction::interaction::modal(this auto&& self, std::string custom_id, std::string title, std::vector<components::component> components_, CompletionToken&& token_) {
    detail::check_bot(self.bot_ptr_);
    return self.bot_ptr_->api.template create_interaction_response<ReturnResult>(self.id, self.token,
        api::interaction::interaction_response{
            .type = api::interaction::interaction_callback_type::MODAL,
            .data{api::interaction::interaction_callback_data_modal{
                .custom_id{std::move(custom_id)},
                .title{std::move(title)},
                .components{std::move(components_)},
            },},
        })(std::forward<CompletionToken>(token_));
}

template <bool ReturnResult, typename CompletionToken>
auto interaction::interaction::update_message(this auto&& self, api::interaction::interaction_callback_data_message data, CompletionToken&& token_) {
    detail::check_bot(self.bot_ptr_);
    return self.bot_ptr_->api.template create_interaction_response<ReturnResult>(self.id, self.token,
        api::interaction::interaction_response{
            .type = api::interaction::interaction_callback_type::UPDATE_MESSAGE,
            .data{std::move(data)},
        })(std::forward<CompletionToken>(token_));
}

template <bool ReturnResult, typename CompletionToken>
auto interaction::interaction::defer_update(this auto&& self, CompletionToken&& token_) {
    detail::check_bot(self.bot_ptr_);
    return self.bot_ptr_->api.template create_interaction_response<ReturnResult>(self.id, self.token,
        api::interaction::interaction_response{
            .type = api::interaction::interaction_callback_type::DEFERRED_UPDATE_MESSAGE,
        })(std::forward<CompletionToken>(token_));
}

template <bool ReturnResult, typename CompletionToken>
auto interaction::interaction::acknowledge_component_interaction(this auto&& self, CompletionToken&& token_) {
    return self.template defer_update<ReturnResult>(std::forward<CompletionToken>(token_));
}

template <bool ReturnResult, typename CompletionToken>
auto interaction::interaction::acknowledge_component(this auto&& self, CompletionToken&& token_) {
    return self.template defer_update<ReturnResult>(std::forward<CompletionToken>(token_));
}

template <bool ReturnResult, typename CompletionToken>
auto interaction::interaction::acknowledge(this auto&& self, CompletionToken&& token_) {
    return self.template defer_update<ReturnResult>(std::forward<CompletionToken>(token_));
}

template <bool ReturnResult, typename CompletionToken>
auto interaction::interaction::reply_to_component_interaction(this auto&& self, std::vector<components::component> components_, CompletionToken&& token_) {
    return self.template reply_to_component<ReturnResult>(std::move(components_), std::forward<CompletionToken>(token_));
}

template <bool ReturnResult, typename CompletionToken>
auto interaction::interaction::reply_to_component_interaction(this auto&& self, api::interaction::interaction_callback_data_message data, CompletionToken&& token_) {
    return self.template update_message<ReturnResult>(std::move(data), std::forward<CompletionToken>(token_));
}

template <bool ReturnResult, typename CompletionToken>
auto interaction::interaction::reply_to_component(this auto&& self, std::vector<components::component> components_, CompletionToken&& token_) {
    return self.template reply_to_component_components<ReturnResult>(std::move(components_), std::forward<CompletionToken>(token_));
}

template <bool ReturnResult, typename CompletionToken>
auto interaction::interaction::reply_to_component(this auto&& self, api::interaction::interaction_callback_data_message data, CompletionToken&& token_) {
    return self.template update_message<ReturnResult>(std::move(data), std::forward<CompletionToken>(token_));
}

template <bool ReturnResult, typename CompletionToken>
auto interaction::interaction::reply_to_component_embed(this auto&& self, discusy::message::embed e, CompletionToken&& token_) {
    api::interaction::interaction_callback_data_message data{};
    data.embeds = make_vector(std::move(e));
    return self.template update_message<ReturnResult>(std::move(data), std::forward<CompletionToken>(token_));
}

template <bool ReturnResult, typename CompletionToken>
auto interaction::interaction::reply_to_component_components(this auto&& self, std::vector<components::component> components_, CompletionToken&& token_) {
    api::interaction::interaction_callback_data_message data{};
    data.set_components(std::move(components_));
    return self.template update_message<ReturnResult>(std::move(data), std::forward<CompletionToken>(token_));
}

template <bool ReturnResult, typename CompletionToken>
auto interaction::interaction::reply_to_component_file(this auto&& self, http_api::upload_file_view f, CompletionToken&& token_) {
    const std::array<http_api::upload_file_view, 1> arr{f};
    return self.template reply_to_component_files<ReturnResult>(arr, std::forward<CompletionToken>(token_));
}

template <bool ReturnResult, typename CompletionToken>
auto interaction::interaction::reply_to_component_file(this auto&& self, http_api::upload_file_view f, std::vector<components::component> components_, CompletionToken&& token_) {
    const std::array<http_api::upload_file_view, 1> arr{f};
    return self.template reply_to_component_files<ReturnResult>(arr, std::move(components_), std::forward<CompletionToken>(token_));
}

template <bool ReturnResult, typename CompletionToken>
auto interaction::interaction::reply_to_component_files(this auto&& self, http_api::upload_files_param files, CompletionToken&& token_) {
    return self.template reply_to_component_files<ReturnResult>(files, detail::make_v2_file_components(files), std::forward<CompletionToken>(token_));
}

template <bool ReturnResult, typename CompletionToken>
auto interaction::interaction::reply_to_component_files(this auto&& self, http_api::upload_files_param files, std::vector<components::component> components_, CompletionToken&& token_) {
    detail::check_bot(self.bot_ptr_);
    api::interaction::interaction_callback_data_message data{};
    if (components_.empty()) {
        data.set_components(detail::make_v2_file_components(files));
    } else {
        data.set_components(std::move(components_));
    }
    return self.bot_ptr_->api.template create_interaction_response<ReturnResult>(self.id, self.token,
        api::interaction::interaction_response{
            .type = api::interaction::interaction_callback_type::UPDATE_MESSAGE,
            .data{std::move(data)},
        },
        files)(std::forward<CompletionToken>(token_));
}

template <bool ReturnResult, typename CompletionToken>
auto interaction::interaction::reply_component_file(this auto&& self, http_api::upload_file_view f, CompletionToken&& token_) {
    return self.template reply_to_component_file<ReturnResult>(f, std::forward<CompletionToken>(token_));
}

template <bool ReturnResult, typename CompletionToken>
auto interaction::interaction::reply_component_file(this auto&& self, http_api::upload_file_view f, std::vector<components::component> components_, CompletionToken&& token_) {
    return self.template reply_to_component_file<ReturnResult>(f, std::move(components_), std::forward<CompletionToken>(token_));
}

template <bool ReturnResult, typename CompletionToken>
auto interaction::interaction::reply_component_files(this auto&& self, http_api::upload_files_param files, CompletionToken&& token_) {
    return self.template reply_to_component_files<ReturnResult>(files, std::forward<CompletionToken>(token_));
}

template <bool ReturnResult, typename CompletionToken>
auto interaction::interaction::reply_component_files(this auto&& self, http_api::upload_files_param files, std::vector<components::component> components_, CompletionToken&& token_) {
    return self.template reply_to_component_files<ReturnResult>(files, std::move(components_), std::forward<CompletionToken>(token_));
}

template <bool ReturnResult, typename CompletionToken>
auto interaction::interaction::reply_component_interaction(this auto&& self, std::vector<components::component> components_, CompletionToken&& token_) {
    return self.template reply_to_component<ReturnResult>(std::move(components_), std::forward<CompletionToken>(token_));
}

template <bool ReturnResult, typename CompletionToken>
auto interaction::interaction::reply_component_interaction(this auto&& self, api::interaction::interaction_callback_data_message data, CompletionToken&& token_) {
    return self.template update_message<ReturnResult>(std::move(data), std::forward<CompletionToken>(token_));
}

template <bool ReturnResult, typename CompletionToken>
auto interaction::interaction::reply_component(this auto&& self, std::vector<components::component> components_, CompletionToken&& token_) {
    return self.template reply_to_component<ReturnResult>(std::move(components_), std::forward<CompletionToken>(token_));
}

template <bool ReturnResult, typename CompletionToken>
auto interaction::interaction::reply_component(this auto&& self, api::interaction::interaction_callback_data_message data, CompletionToken&& token_) {
    return self.template update_message<ReturnResult>(std::move(data), std::forward<CompletionToken>(token_));
}

// interaction::modal_component_data

inline std::string_view interaction::modal_component_data::text_value(this auto&& self, const std::string_view target_custom_id) noexcept {
    for (const auto& wrapper : self.components) {
        if (const auto* ti = wrapper.template get_if<components::TextInputInteractionResponse>()) {
            if (ti->custom_id == target_custom_id) return ti->value;
            continue;
        }

        if (const auto* label = wrapper.template get_if<components::LabelInteractionResponse>()) {
            if (const auto* inner = label->component.template get_if<components::TextInput>()) {
                if (inner->custom_id == target_custom_id && inner->value) return *inner->value;
            }
        }
    }
    return {};
}

inline const std::vector<std::string>* interaction::modal_component_data::select_values(this auto&& self, const std::string_view target_custom_id) noexcept {
    for (const auto& wrapper : self.components) {
        if (const auto* ss = wrapper.template get_if<components::StringSelectInteractionResponse>()) {
            if (ss->custom_id == target_custom_id) return &ss->values;
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// interaction::application_command_data
// ---------------------------------------------------------------------------

inline const interaction::application_command_interaction_data_option* interaction::application_command_data::find_subcommand(this auto&& self) noexcept {
    if (!self.options) return nullptr;
    for (const auto& o : *self.options) {
        if (o.type == discusy::application_commands::application_command_option_type::SUB_COMMAND ||
            o.type == discusy::application_commands::application_command_option_type::SUB_COMMAND_GROUP) {
            return &o;
        }
    }
    return nullptr;
}

inline const std::vector<interaction::application_command_interaction_data_option>* interaction::application_command_data::option_scope(this auto&& self) noexcept {
    if (!self.options) return nullptr;

    const std::vector<application_command_interaction_data_option>* scope = &*self.options;

    // at most two levels: group -> sub -> args
    for (int depth = 0; depth < 2; ++depth) {
        if (!scope) break;
        const auto* nested = [&]() -> const application_command_interaction_data_option* {
            for (const auto& o : *scope) {
                if (o.type == discusy::application_commands::application_command_option_type::SUB_COMMAND ||
                    o.type == discusy::application_commands::application_command_option_type::SUB_COMMAND_GROUP) {
                    return &o;
                }
            }
            return nullptr;
        }();
        if (!nested || !nested->options) break;
        scope = &*nested->options;
    }

    return scope;
}

inline command_path interaction::application_command_data::subcommand_path(this auto&& self) noexcept {
    command_path path{};
    if (!self.options) return path;

    const auto* first = self.find_subcommand();
    if (!first) return path;

    if (first->type == discusy::application_commands::application_command_option_type::SUB_COMMAND_GROUP) {
        path.group = first->name;
        if (first->options) {
            for (const auto& o : *first->options) {
                if (o.type == discusy::application_commands::application_command_option_type::SUB_COMMAND ||
                    o.type == discusy::application_commands::application_command_option_type::SUB_COMMAND_GROUP) {
                    path.sub = o.name;
                    break;
                }
            }
        }
    } else {
        path.sub = first->name;
    }

    return path;
}

inline const interaction::application_command_interaction_data_option* interaction::application_command_data::find_option(this auto&& self, const std::string_view opt_name) noexcept {
    const auto* scope = self.option_scope();
    if (!scope) return nullptr;
    for (const auto& o : *scope) {
        if (o.name == opt_name) return &o;
    }
    return nullptr;
}

inline std::optional<std::string> interaction::application_command_data::get_string(this auto&& self, const std::string_view opt_name) {
    const auto* o = self.find_option(opt_name);
    if (!o) return std::nullopt;
    auto v = o->template value_as<std::string>();
    if (v && v->empty()) return std::nullopt;
    return v;
}

inline std::optional<integer> interaction::application_command_data::get_integer(this auto&& self, const std::string_view opt_name) noexcept {
    const auto* o = self.find_option(opt_name);
    return o ? o->template value_as<integer>() : std::nullopt;
}

inline std::optional<double> interaction::application_command_data::get_double(this auto&& self, const std::string_view opt_name) noexcept {
    const auto* o = self.find_option(opt_name);
    return o ? o->template value_as<double>() : std::nullopt;
}

inline std::optional<bool> interaction::application_command_data::get_bool(this auto&& self, const std::string_view opt_name) noexcept {
    const auto* o = self.find_option(opt_name);
    return o ? o->template value_as<bool>() : std::nullopt;
}

inline std::optional<snowflake> interaction::application_command_data::get_snowflake(this auto&& self, const std::string_view opt_name) {
    const auto s = self.get_string(opt_name);
    if (!s || s->empty()) return std::nullopt;

    const auto v = ulp::str::to_number<std::uint64_t>(*s);
    if (!v) return std::nullopt;

    return snowflake{*v};
}

inline const interaction::application_command_interaction_data_option* interaction::application_command_data::focused_option(this auto&& self) noexcept {
    const auto* scope = self.option_scope();
    if (!scope) return nullptr;

    for (const auto& o : *scope) {
        if (o.focused.value_or(false)) return &o;
    }
    return nullptr;
}

inline focused_input interaction::application_command_data::focused(this auto&& self) noexcept {
    const auto* o = self.focused_option();
    if (!o) return {};

    focused_input out{.name = o->name};
    if (o->value) {
        if (const auto* v = std::get_if<std::string>(&*o->value)) out.value = *v;
    }
    return out;
}

#define DISCUSY_CMD_RESOLVED_GETTER(NAME, FIELD, TYPE)                                      \
    inline const TYPE* interaction::application_command_data::NAME(this auto&& self, const snowflake id) noexcept { \
        if (!self.resolved || !self.resolved->FIELD) return nullptr;                        \
        const auto it = self.resolved->FIELD->find(id.value);                               \
        return (it == self.resolved->FIELD->end()) ? nullptr : &it->second;                 \
    }                                                                                       \
    inline const TYPE* interaction::application_command_data::NAME(this auto&& self, const std::string_view opt_name) { \
        const auto id = self.get_snowflake(opt_name);                                       \
        return id ? self.NAME(*id) : nullptr;                                               \
    }

DISCUSY_CMD_RESOLVED_GETTER(resolved_attachment, attachments, discusy::message::attachment)
DISCUSY_CMD_RESOLVED_GETTER(resolved_user,       users,       discusy::user::user)
DISCUSY_CMD_RESOLVED_GETTER(resolved_channel,    channels,    discusy::channel::channel)
DISCUSY_CMD_RESOLVED_GETTER(resolved_role,       roles,       discusy::permissions::role)
DISCUSY_CMD_RESOLVED_GETTER(resolved_message,    messages,    discusy::message::message)

#undef DISCUSY_CMD_RESOLVED_GETTER

inline const discusy::guild::guild_member* interaction::application_command_data::resolved_member(this auto&& self, const snowflake id) noexcept {
    if (!self.resolved || !self.resolved->members) return nullptr;
    const auto it = self.resolved->members->find(id.value);
    return (it == self.resolved->members->end()) ? nullptr : &it->second;
}

inline const discusy::guild::guild_member* interaction::application_command_data::resolved_member(this auto&& self, const std::string_view opt_name) {
    const auto id = self.get_snowflake(opt_name);
    return id ? self.resolved_member(*id) : nullptr;
}

// interaction::interaction

inline std::string_view interaction::interaction::command_name(this auto&& self) noexcept {
    const auto* cmd = self.command_data();
    return cmd ? std::string_view{cmd->name} : std::string_view{};
}

inline const interaction::application_command_data* interaction::interaction::command_data(this auto&& self) noexcept {
    if (!self.data) return nullptr;
    if (self.type != interaction_type::APPLICATION_COMMAND &&
        self.type != interaction_type::APPLICATION_COMMAND_AUTOCOMPLETE) {
        return nullptr;
    }
    return std::get_if<application_command_data>(&*self.data);
}

inline const interaction::message_component_data* interaction::interaction::component_data(this auto&& self) noexcept {
    if (!self.data || self.type != interaction_type::MESSAGE_COMPONENT) return nullptr;
    return std::get_if<message_component_data>(&*self.data);
}

inline const interaction::modal_component_data* interaction::interaction::modal_data(this auto&& self) noexcept {
    if (!self.data || self.type != interaction_type::MODAL_SUBMIT) return nullptr;
    return std::get_if<modal_component_data>(&*self.data);
}

inline bool interaction::interaction::is_command(this auto&& self) noexcept {
    return self.type == interaction_type::APPLICATION_COMMAND;
}

inline bool interaction::interaction::is_autocomplete(this auto&& self) noexcept {
    return self.type == interaction_type::APPLICATION_COMMAND_AUTOCOMPLETE;
}

inline bool interaction::interaction::is_component(this auto&& self) noexcept {
    return self.type == interaction_type::MESSAGE_COMPONENT;
}

inline bool interaction::interaction::is_modal_submit(this auto&& self) noexcept {
    return self.type == interaction_type::MODAL_SUBMIT;
}

inline command_path interaction::interaction::subcommand_path(this auto&& self) noexcept {
    const auto* cmd = self.command_data();
    return cmd ? cmd->subcommand_path() : command_path{};
}

inline const interaction::application_command_interaction_data_option* interaction::interaction::find_option(this auto&& self, const std::string_view opt_name) noexcept {
    const auto* cmd = self.command_data();
    return cmd ? cmd->find_option(opt_name) : nullptr;
}

inline const interaction::application_command_interaction_data_option* interaction::interaction::find_subcommand(this auto&& self) noexcept {
    const auto* cmd = self.command_data();
    return cmd ? cmd->find_subcommand() : nullptr;
}

inline const std::vector<interaction::application_command_interaction_data_option>* interaction::interaction::option_scope(this auto&& self) noexcept {
    const auto* cmd = self.command_data();
    return cmd ? cmd->option_scope() : nullptr;
}

inline std::optional<std::string> interaction::interaction::get_string(this auto&& self, const std::string_view opt_name) {
    const auto* cmd = self.command_data();
    return cmd ? cmd->get_string(opt_name) : std::nullopt;
}

inline std::optional<integer> interaction::interaction::get_integer(this auto&& self, const std::string_view opt_name) noexcept {
    const auto* cmd = self.command_data();
    return cmd ? cmd->get_integer(opt_name) : std::nullopt;
}

inline std::optional<double> interaction::interaction::get_double(this auto&& self, const std::string_view opt_name) noexcept {
    const auto* cmd = self.command_data();
    return cmd ? cmd->get_double(opt_name) : std::nullopt;
}

inline std::optional<bool> interaction::interaction::get_bool(this auto&& self, const std::string_view opt_name) noexcept {
    const auto* cmd = self.command_data();
    return cmd ? cmd->get_bool(opt_name) : std::nullopt;
}

inline std::optional<snowflake> interaction::interaction::get_snowflake(this auto&& self, const std::string_view opt_name) {
    const auto* cmd = self.command_data();
    return cmd ? cmd->get_snowflake(opt_name) : std::nullopt;
}

inline const interaction::application_command_interaction_data_option* interaction::interaction::focused_option(this auto&& self) noexcept {
    const auto* cmd = self.command_data();
    return cmd ? cmd->focused_option() : nullptr;
}

inline focused_input interaction::interaction::focused(this auto&& self) noexcept {
    const auto* cmd = self.command_data();
    return cmd ? cmd->focused() : focused_input{};
}

inline const discusy::message::attachment* interaction::interaction::resolved_attachment(this auto&& self, const snowflake id) noexcept {
    const auto* cmd = self.command_data();
    return cmd ? cmd->resolved_attachment(id) : nullptr;
}

inline const discusy::message::attachment* interaction::interaction::resolved_attachment(this auto&& self, const std::string_view opt_name) {
    const auto* cmd = self.command_data();
    return cmd ? cmd->resolved_attachment(opt_name) : nullptr;
}

inline const discusy::user::user* interaction::interaction::resolved_user(this auto&& self, const snowflake id) noexcept {
    const auto* cmd = self.command_data();
    return cmd ? cmd->resolved_user(id) : nullptr;
}

inline const discusy::user::user* interaction::interaction::resolved_user(this auto&& self, const std::string_view opt_name) {
    const auto* cmd = self.command_data();
    return cmd ? cmd->resolved_user(opt_name) : nullptr;
}

inline const discusy::channel::channel* interaction::interaction::resolved_channel(this auto&& self, const snowflake id) noexcept {
    const auto* cmd = self.command_data();
    return cmd ? cmd->resolved_channel(id) : nullptr;
}

inline const discusy::channel::channel* interaction::interaction::resolved_channel(this auto&& self, const std::string_view opt_name) {
    const auto* cmd = self.command_data();
    return cmd ? cmd->resolved_channel(opt_name) : nullptr;
}

inline const discusy::permissions::role* interaction::interaction::resolved_role(this auto&& self, const snowflake id) noexcept {
    const auto* cmd = self.command_data();
    return cmd ? cmd->resolved_role(id) : nullptr;
}

inline const discusy::permissions::role* interaction::interaction::resolved_role(this auto&& self, const std::string_view opt_name) {
    const auto* cmd = self.command_data();
    return cmd ? cmd->resolved_role(opt_name) : nullptr;
}

inline const discusy::message::message* interaction::interaction::resolved_message(this auto&& self, const snowflake id) noexcept {
    const auto* cmd = self.command_data();
    return cmd ? cmd->resolved_message(id) : nullptr;
}

inline const discusy::message::message* interaction::interaction::resolved_message(this auto&& self, const std::string_view opt_name) {
    const auto* cmd = self.command_data();
    return cmd ? cmd->resolved_message(opt_name) : nullptr;
}

inline const discusy::guild::guild_member* interaction::interaction::resolved_member(this auto&& self, const snowflake id) noexcept {
    const auto* cmd = self.command_data();
    return cmd ? cmd->resolved_member(id) : nullptr;
}

inline const discusy::guild::guild_member* interaction::interaction::resolved_member(this auto&& self, const std::string_view opt_name) {
    const auto* cmd = self.command_data();
    return cmd ? cmd->resolved_member(opt_name) : nullptr;
}

inline std::string_view interaction::interaction::component_custom_id(this auto&& self) noexcept {
    const auto* d = self.component_data();
    return d ? std::string_view{d->custom_id} : std::string_view{};
}

inline const std::vector<std::string>* interaction::interaction::component_values(this auto&& self) noexcept {
    const auto* d = self.component_data();
    return d ? d->values_ptr() : nullptr;
}

inline std::string_view interaction::interaction::component_first_value(this auto&& self) noexcept {
    const auto* d = self.component_data();
    return d ? d->first_value() : std::string_view{};
}

inline std::string_view interaction::interaction::modal_custom_id(this auto&& self) noexcept {
    const auto* d = self.modal_data();
    return d ? std::string_view{d->custom_id} : std::string_view{};
}

inline std::string_view interaction::interaction::modal_text_value(this auto&& self, const std::string_view custom_id) noexcept {
    const auto* d = self.modal_data();
    return d ? d->text_value(custom_id) : std::string_view{};
}

inline const std::vector<std::string>* interaction::interaction::modal_select_values(this auto&& self, const std::string_view custom_id) noexcept {
    const auto* d = self.modal_data();
    return d ? d->select_values(custom_id) : nullptr;
}

inline bool interaction::interaction::bot_can(this auto&& self, const permissions::permissions perm) noexcept {
    return self.app_permissions.has_flags(perm) ||
           self.app_permissions.has_flags(permissions::permissions::ADMINISTRATOR);
}

inline bool interaction::interaction::user_can(this auto&& self, const permissions::permissions perm) noexcept {
    if (!self.member || !self.member->permissions) return false;
    return self.member->permissions->has_flags(perm) ||
           self.member->permissions->has_flags(permissions::permissions::ADMINISTRATOR);
}

inline const discusy::user::user* interaction::interaction::user_ptr(this auto&& self) noexcept {
    if (self.member && self.member->user) return &*self.member->user;
    if (self.user) return &*self.user;
    return nullptr;
}

inline const discusy::user::user* interaction::interaction::user_of(this auto&& self) noexcept {
    return self.user_ptr();
}

inline snowflake interaction::interaction::user_id(this auto&& self) noexcept {
    const auto* u = self.user_ptr();
    return u ? u->id : snowflake{};
}

inline snowflake interaction::interaction::user_id_of(this auto&& self) noexcept {
    return self.user_id();
}

inline std::string_view interaction::interaction::display_name(this auto&& self) noexcept {
    if (self.member && self.member->nick && !self.member->nick->empty()) return *self.member->nick;

    const auto* u = self.user_ptr();
    if (!u) return {};

    if (u->global_name && !u->global_name->empty()) return *u->global_name;
    return u->username;
}

inline std::string_view interaction::interaction::display_name_of(this auto&& self) noexcept {
    return self.display_name();
}

inline bool interaction::interaction::in_guild(this auto&& self) noexcept {
    return self.guild_id.has_value();
}

inline snowflake interaction::interaction::guild_id_of(this auto&& self) noexcept {
    return self.guild_id.value_or(snowflake{});
}

inline snowflake interaction::interaction::guild_id_or_default(this auto&& self) noexcept {
    return self.guild_id.value_or(snowflake{});
}

inline snowflake interaction::interaction::channel_id_of(this auto&& self) noexcept {
    return self.channel_id.value_or(snowflake{});
}

}
