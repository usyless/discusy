#include <algorithm>
#include <chrono>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <optional>
#include <random>
#include <ranges>
#include <set>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <discusy/discusy.hpp>
#include <discusy/helpers.hpp>

#include <usylibpp/strings.hpp>

// This bot is used for sending random images via slash commands
// These slash commands are linked to channels in a specific discord server
// Sending a message to one of these channels adds to the commands random selection
// Cross reacting to a message in the channel removes it from the selection (A delete breaks the state)
// The specific find channel is used for commands with keywords, these should be comma separated and in the text alongside the image
// The server should ideally have no other channels

// Coroutines are not used to maximise efficiency

constexpr inline auto COMMAND_PREFIX = "cmd-";
constexpr inline auto FIND_CHANNEL_NAME = "cmd-f";
constexpr inline auto FIND_COMMAND_NAME = "f";

constexpr inline auto RESTART_COMMAND_NAME = "restart";

constexpr inline auto DATA_FILE = "data.json";
constexpr inline auto CONFIG_FILE = "config.json";

constexpr inline auto DELETE_REACTION = "\xE2\x9D\x8C";

constexpr inline auto VIDEO_ENDING = "V";
constexpr inline auto OTHER_ENDING = "I";

constexpr inline size_t MAX_COMMAND_CHANNELS = 99;

using namespace discusy;

namespace h = discusy::helpers;

struct transparent_string_hash {
    using is_transparent = void;

    size_t operator()(std::string_view sv) const noexcept {
        return std::hash<std::string_view>{}(sv);
    }
};

struct cache_t {
    std::unordered_map<
        std::string, 
        std::vector<std::string>,
        transparent_string_hash,
        std::equal_to<>
    > command_images;
    std::unordered_map<
        std::string,
        std::string,
        transparent_string_hash,
        std::equal_to<>
    > f_command_images;
};

static cache_t cache{};

struct prevent_duplicates_t {
    std::set<size_t> indices{};
    std::chrono::time_point<std::chrono::steady_clock> last_use{};
};

struct command_dup_tracker_t {
    std::unordered_map<snowflake, prevent_duplicates_t> user_map{};
    std::deque<std::pair<snowflake, std::chrono::steady_clock::time_point>> expiry_queue{};
};

static std::unordered_map<
    const std::string*, command_dup_tracker_t
> prevent_duplicates{};

static std::mutex dup_mtx{};

struct config_t {
    std::string TOKEN;
    snowflake SERVER_ID;
    snowflake ADMIN_USER;
    bool FORCE_REMAKE_COMMANDS;
};

static config_t config{};

std::string_view command_name_from_channel_name(const std::string_view channel_name) {
    return channel_name.substr(usylibpp::str::strlen(COMMAND_PREFIX));
}

std::string fix_attachment_url(const message::attachment& attachment) {
    const auto pos = attachment.url.find('?');
    if (pos == std::string::npos) return attachment.url;

    return attachment.url.substr(0, pos);
}

api::interaction::interaction_callback_data_message get_send_message(std::string_view url) {
    const auto actual_url = (url.ends_with(VIDEO_ENDING) || url.ends_with(OTHER_ENDING))
        ? url.substr(0, url.size() - 1)
        : url;

    return api::interaction::interaction_callback_data_message{}
        .set_flags(message::message_flags::IS_COMPONENTS_V2)
        .add_component(
            components::component::create(
                components::MediaGallery{}
                    .add_item(components::media_gallery_item::create(
                        components::unfurled_media_item::create(std::string{actual_url})
                    )
                )
            )
        );
}

bool is_command_channel(const channel::channel& channel) {
    return channel.type == channel::channel_type::GUILD_TEXT && channel.name && channel.name->starts_with(COMMAND_PREFIX);
}

static constexpr glz::opts write_opts{
    .prettify = true,
};

static constexpr glz::opts read_opts{
    .minified = false
};

bool save_cache(const std::unique_lock<std::shared_mutex>&) {
    std::ofstream data_file(DATA_FILE);
    if (!data_file.is_open()) return false;
    glz::basic_ostream_buffer<std::ofstream> buffer(data_file);
    return !glz::write<write_opts>(cache, buffer);
};

std::string preprocess_f_keyword(const std::string_view keyword) {
    return ulp::str::to_lowercase(ulp::str::trim(keyword, ' '));
}

static std::shared_mutex mtx{};
static std::vector<channel::channel> COMMAND_CHANNELS{};

static const std::unordered_set<interaction::interaction_context_type>& anywhere() {
    static const std::unordered_set<interaction::interaction_context_type> set{
        interaction::interaction_context_type::GUILD,
        interaction::interaction_context_type::BOT_DM,
        interaction::interaction_context_type::PRIVATE_CHANNEL,
    };
    return set;
}

static api::application_commands::application_command anywhere_slash(
    std::string name, std::string description, std::vector<h::cmd::option_t> options = {})
{
    auto c = h::cmd::slash(std::move(name), std::move(description), std::move(options));
    c.contexts = anywhere();
    return c;
}

static api::application_commands::application_command find_command() {
    return anywhere_slash(FIND_COMMAND_NAME, "Send an image from a keyword", {
        h::cmd::string_opt("keyword", "Keyword for image", true).autocompleted(),
    });
}

template <typename Token>
auto force_remake_commands(bot& bot, Token&& token) {
    std::vector<api::application_commands::application_command> commands;
    commands.reserve(COMMAND_CHANNELS.size() + 1);

    for (const auto& channel : COMMAND_CHANNELS) {
        if (!channel.name || *channel.name == FIND_CHANNEL_NAME) continue;
        if (!channel.topic) continue;

        commands.emplace_back(anywhere_slash(
            std::string{command_name_from_channel_name(*channel.name)}, *channel.topic));
    }

    commands.emplace_back(find_command());

    return bot.api.bulk_overwrite_global_application_commands<false>(commands)(std::forward<Token>(token));
};

static constexpr auto DUP_TIMEOUT = std::chrono::seconds{60};

static size_t pick_image_index(const std::string* cmd_ptr, const snowflake user_id, const size_t total_images) {
    const auto now = std::chrono::steady_clock::now();

    std::scoped_lock dup_lock{dup_mtx};

    auto& tracker = prevent_duplicates[cmd_ptr];
    auto& user_map = tracker.user_map;
    auto& expiry_queue = tracker.expiry_queue;

    while (!expiry_queue.empty()) {
        const auto& [q_user_id, q_time] = expiry_queue.front();

        if (now - q_time <= DUP_TIMEOUT) break;

        if (const auto it = user_map.find(q_user_id); it != user_map.end()) {
            if (now - it->second.last_use > DUP_TIMEOUT) user_map.erase(it);
        }

        expiry_queue.pop_front();
    }

    auto& user_data = user_map[user_id];

    if (!user_data.indices.empty() && (now - user_data.last_use > DUP_TIMEOUT)) {
        user_data.indices.clear();
    }
    user_data.last_use = now;

    expiry_queue.emplace_back(user_id, now);

    if (user_data.indices.size() >= total_images) user_data.indices.clear();

    static thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<size_t> dist{0, total_images - 1};

    auto chosen_index = dist(gen);
    while (user_data.indices.contains(chosen_index)) {
        chosen_index = (chosen_index + 1) % total_images;
    }

    user_data.indices.emplace(chosen_index);
    return chosen_index;
}

int main(int, char* argv[]) {
    std::filesystem::current_path(std::filesystem::path(argv[0]).parent_path());

    // config stuff
    {
        std::ifstream config_file(CONFIG_FILE);

        if (!config_file.is_open()) {
            std::ofstream config_file(CONFIG_FILE);
            if (!config_file.is_open()) {
                std::cerr << "Failed to open config file to write!" << '\n';
                return 1;
            }
            glz::basic_ostream_buffer<std::ofstream> buffer(config_file);
            if (glz::write<write_opts>(::config, buffer)) {
                std::cerr << "Failed to write config file!" << '\n';
                return 1;
            }

            std::cerr << "Please setup config file!" << '\n';
            return 1;
        }

        glz::basic_istream_buffer<std::ifstream> buffer(config_file);
        if (glz::read<read_opts>(::config, buffer)) {
            std::cerr << "Delete config file in case of errors and re-launch" << '\n';
            return 1;
        }

        if (!::config.SERVER_ID || ::config.TOKEN.empty() || !::config.ADMIN_USER) {
            std::cerr << "Please setup config file!" << '\n';
            return 1;
        }
    }

    if (std::filesystem::exists(DATA_FILE)) {
        std::ifstream file(DATA_FILE);

        if (file.is_open()) {
            glz::basic_istream_buffer<std::ifstream> buffer(file);
            if (glz::read<read_opts>(::cache, buffer)) {
                ::cache = cache_t{};
            }
        }
    }

    {
    std::scoped_lock dup_lock{dup_mtx};
    prevent_duplicates.reserve(::cache.command_images.size());
    }

    bot bot(discusy::config{
        .token = ::config.TOKEN,
        .intents = intent::guilds | intent::guild_messages | intent::guild_message_reactions | intent::message_content,
        .threads = 1,
    });

    const auto make_find_channel = [&bot]<typename Token>(Token&& t) {
        return bot.api.create_guild_channel<false>(::config.SERVER_ID, api::guild::create_guild_channel{
            .name{FIND_CHANNEL_NAME},
        })(std::forward<Token>(t));
    };
    
    bot.on_shards_ready([&bot, &make_find_channel](auto&&) -> coro::awaitable<void> {
        static std::atomic_bool run = false;
        if (!run.exchange(true, std::memory_order_acquire)) {
            const auto [channels_ec, channels_result] = co_await bot.api.get_guild_channels(::config.SERVER_ID)(token::t_deferred);

            if (channels_ec || !channels_result) {
                std::cerr << "Error fetching channels: " << (channels_result.error().empty() ? channels_ec.message() : channels_result.error()) << "\n";
                exit(1);
            }

            bool find_channel_exists = false;

            const auto& channels = channels_result.value();
            for (const auto& channel : channels) {
                if (channel.type == channel::channel_type::GUILD_TEXT && channel.name) {
                    const auto& channel_name = *channel.name;

                    if (channel_name == FIND_CHANNEL_NAME) {
                        find_channel_exists = true;
                    }
                    
                    if (is_command_channel(channel)) {
                        COMMAND_CHANNELS.emplace_back(channel);
                    }
                }
            }

            if (!find_channel_exists) {
                co_await make_find_channel(token::t_deferred);
            }

            if (::config.FORCE_REMAKE_COMMANDS) {
                co_await force_remake_commands(bot, token::t_deferred);
            }

            bot.gateway_callbacks.on_channel_create([&bot](const recieve_event::channel_create& e) -> void {
                if (!e.guild_id || !is_command_channel(e) || (*e.guild_id != ::config.SERVER_ID)) return;

                bool is_full{false};
                {
                std::shared_lock lock(mtx);
                if (COMMAND_CHANNELS.size() >= MAX_COMMAND_CHANNELS) is_full = true;
                }

                e.send(is_full ? "Max command limit reached!"
                            : "Please set a channel description to activate the command!")(token::detached);
            });

            bot.gateway_callbacks.on_channel_update([&bot](const recieve_event::channel_update& e) -> void {
                if (!e.guild_id || (*e.guild_id != ::config.SERVER_ID)) return;

                if (!is_command_channel(e)) return;

                if (!e.topic) {
                    e.send("No description provided...")(token::detached);
                    return;
                }

                const auto command_name = command_name_from_channel_name(*e.name);
                if (command_name == FIND_COMMAND_NAME) return;

                bool is_full{false};
                {
                std::unique_lock lock(mtx);
                if (COMMAND_CHANNELS.size() >= MAX_COMMAND_CHANNELS) { is_full = true; }
                else if (std::ranges::find(COMMAND_CHANNELS, e.id, &channel::channel::id) == COMMAND_CHANNELS.end()) { COMMAND_CHANNELS.emplace_back(e); }
                }

                if (!is_full) {
                    bot.api.create_global_application_command<false>(
                        anywhere_slash(std::string{command_name}, e.topic.value_or("How"))
                    )([&bot, id = e.id, command_name = std::string{command_name}](const auto& resp_ec, auto resp) {
                        const channel::channel channel{
                            .id = id,
                            .bot_ptr_ = &bot,
                        };

                        if (resp_ec || !resp) {
                            {
                            std::unique_lock lock(mtx);
                            auto channel_it = std::ranges::find(COMMAND_CHANNELS, id, &channel::channel::id);
                            if (channel_it != COMMAND_CHANNELS.end()) COMMAND_CHANNELS.erase(channel_it);
                            if (auto it = ::cache.command_images.find(command_name); it != ::cache.command_images.end()) {
                                {
                                std::scoped_lock dup_lock{dup_mtx};
                                prevent_duplicates.erase(&it->first);
                                }
                                ::cache.command_images.erase(it);
                                save_cache(lock);
                            }
                            }

                            channel.send("Failed to create command... Try editing the description again")(token::detached);
                            return;
                        }

                        channel.send("Updating command!")(token::detached);
                    });
                    return;
                }

                e.send("Unable to create command, max commands reached!")(token::detached);
            });

            bot.gateway_callbacks.on_channel_delete([&bot, &make_find_channel](const recieve_event::channel_delete& e) -> void {
                if (!e.guild_id || (e.guild_id != ::config.SERVER_ID) || !e.name) return;

                if (e.type != channel::channel_type::GUILD_TEXT) return;

                const auto command_name = command_name_from_channel_name(*e.name);
                {
                std::unique_lock lock(mtx);
                auto channel_it = std::ranges::find(COMMAND_CHANNELS, e.id, &channel::channel::id);

                if (channel_it == COMMAND_CHANNELS.end()) return;

                COMMAND_CHANNELS.erase(channel_it);

                if (command_name == FIND_COMMAND_NAME) {
                    cache.f_command_images.clear();
                    save_cache(lock);

                    lock.unlock();
                    make_find_channel(token::detached);
                    return;
                }

                if (auto it = cache.command_images.find(command_name); it != cache.command_images.end()) {
                    {
                    std::scoped_lock dup_lock{dup_mtx};
                    prevent_duplicates.erase(&it->first);
                    }
                    cache.command_images.erase(it);
                    save_cache(lock);
                }
                }

                force_remake_commands(bot, token::detached);
                return;
            });

            bot.gateway_callbacks.on_message_create([&bot](const recieve_event::message_create& e) -> void {
                if (!e.other.guild_id || (*e.other.guild_id != ::config.SERVER_ID) || e.message.attachments.empty()) return;

                {
                std::unique_lock lock(mtx);
                auto channel_it = std::ranges::find(COMMAND_CHANNELS, e.message.channel_id, &channel::channel::id);

                if (channel_it == COMMAND_CHANNELS.end() || !channel_it->name) return;

                const auto command_name = command_name_from_channel_name(*channel_it->name);

                if (command_name == FIND_COMMAND_NAME) {
                    if (e.message.content.empty() || (e.message.attachments.size() > 1)) {
                        lock.unlock();

                        e.reply(
                            "Message needs at least 1 keyword (comma separated) and ONE attachment", false, token::detached);
                        return;
                    }
                    
                    std::vector<std::string> items{};
                    bool no_duplicate_name = true;
                    ulp::str::split_by_for_each(e.message.content, ',', [&no_duplicate_name, &items](const std::string_view _item) {
                        auto item = preprocess_f_keyword(_item);
                        if (!(no_duplicate_name &= (!cache.f_command_images.contains(item)))) return false;
                        items.emplace_back(std::move(item));
                        return true;
                    });

                    if (!no_duplicate_name) {
                        lock.unlock();
                        e.reply("Duplicate keyword! Not adding to bot.", false, token::detached);
                        return;
                    }

                    for (const auto& item : items) {
                        cache.f_command_images.emplace(item, fix_attachment_url(e.message.attachments[0]));
                    }
                } else {
                    if (!e.message.content.empty()) {
                        lock.unlock();
                        e.reply("Message content should be empty!", false, token::detached);
                        return;
                    }

                    auto& cache_entry = cache.command_images[std::string{command_name}];
                    for (const auto& attachment : e.message.attachments) {
                        cache_entry.emplace_back(fix_attachment_url(attachment));
                    }
                }

                save_cache(lock);
                }

                e->add_reaction(DELETE_REACTION)(token::detached);
            });

            bot.gateway_callbacks.on_message_reaction_add([&bot](const recieve_event::message_reaction_add& e) -> void {
                if (!e.guild_id || !e.emoji.name || (e.user_id == bot.get_user_id()) 
                ||  (*e.guild_id != ::config.SERVER_ID)
                ||  (*e.emoji.name != DELETE_REACTION)) return;

                std::string channel_name;
                {
                std::shared_lock lock{mtx};
                auto channel_it = std::ranges::find(COMMAND_CHANNELS, e.channel_id, &channel::channel::id);

                if (channel_it == COMMAND_CHANNELS.end()) return;
                
                if (!channel_it->name) return;
                channel_name = *channel_it->name;
                }

                bot.api.get_channel_message(e.channel_id, e.message_id)([&bot, channel_name = std::move(channel_name)](const auto& msg_ec, auto message_result) -> void {
                    if (msg_ec || !message_result) return;

                    const message::message& msg = message_result.value();

                    if (msg.attachments.empty()) return;

                    bool found_reaction = false;
                    if (!msg.reactions) return;

                    for (const auto& reaction : *msg.reactions) {
                        if (!reaction.emoji.name) continue;

                        if (*reaction.emoji.name == DELETE_REACTION) {
                            found_reaction = true;
                            if (reaction.count_details.normal != 2) return;
                            break;
                        }
                    }

                    if (!found_reaction) return;

                    const auto command_name = command_name_from_channel_name(channel_name);

                    {
                    std::unique_lock lock{mtx};
                    if (command_name == FIND_COMMAND_NAME) {
                        if (msg.content.empty() || (msg.attachments.size() > 1)) return;

                        ulp::str::split_by_for_each(msg.content, ',', [](const std::string_view item) {
                            cache.f_command_images.erase(preprocess_f_keyword(item));
                        });

                    } else {
                        auto vec = cache.command_images.find(command_name);

                        if (vec == cache.command_images.end()) {
                            goto cleanup;
                        }

                        {
                        std::scoped_lock dup_lock{dup_mtx};
                        prevent_duplicates.erase(&vec->first);
                        }

                        for (const auto& attachment : msg.attachments) {
                            auto img_it = std::ranges::find(vec->second, fix_attachment_url(attachment));
                            if (img_it == vec->second.end()) continue;
                            vec->second.erase(img_it);
                        }
                    }
                    
                    save_cache(lock);
                    }

                    cleanup:
                    msg.delete_message()(token::detached);
                });
            });

            bot.gateway_callbacks.on_interaction_create([&bot](const recieve_event::interaction_create& e) -> void {
                const auto* cmd = e.command_data();
                if (!cmd) return;

                if (e.is_autocomplete()) {
                    const auto focused = e.focused();
                    if (focused.name != "keyword") return;

                    const auto userinput = preprocess_f_keyword(focused.value);

                    std::vector<application_commands::application_command_option_choice> choices;

                    {
                    std::shared_lock lock{mtx};
                    for (const auto& [keyword, _] : cache.f_command_images) {
                        if ((!userinput.empty()) && keyword.contains(userinput)) continue;

                        choices.emplace_back(h::choice(keyword));

                        if (choices.size() >= static_cast<size_t>(discusy::constants::MAX_AUTOCOMPLETE_CHOICES)) break;
                    }
                    }

                    e.autocomplete(std::move(choices), token::detached);
                    return;
                }

                if (!e.is_command()) return;

                const auto& command_name = cmd->name;

                if (command_name == RESTART_COMMAND_NAME) {
                    if (e.user_id() != ::config.ADMIN_USER) {
                        e.reply_ephemeral("Nuh uh...", token::detached);
                        return;
                    }

                    e.reply("Restarting...", false, [](auto&&...) {
                        exit(0);
                    });
                    return;
                }

                if (command_name == FIND_COMMAND_NAME) {
                    const auto keyword = cmd->get_string("keyword");
                    if (!keyword) return;

                    std::optional<api::interaction::interaction_callback_data_message> msg;

                    {
                    std::shared_lock lock(mtx);
                    if (const auto it = cache.f_command_images.find(*keyword); it != cache.f_command_images.end()) {
                        msg = get_send_message(it->second);
                    }
                    }

                    if (!msg) {
                        e.reply_ephemeral("Image does not exist for given keyword!", token::detached);
                        return;
                    }

                    e.reply_with(*std::move(msg), token::detached);
                    return;
                }

                std::optional<api::interaction::interaction_callback_data_message> msg;

                {
                std::shared_lock lock(mtx);
                auto command_it = cache.command_images.find(command_name);

                if (command_it == cache.command_images.end()) {
                    lock.unlock();
                    e.reply_ephemeral("Invalid command!", token::detached);
                    return;
                }

                if (command_it->second.empty()) {
                    lock.unlock();
                    e.reply_ephemeral("Command has no media!", token::detached);
                    return;
                }

                const auto chosen_index = pick_image_index(
                    &command_it->first, e.user_id(), command_it->second.size());

                msg = get_send_message(command_it->second[chosen_index]);
                }

                e.reply_with(*std::move(msg), token::detached);
            });
        }

        co_return;
    });

    bot.run();
}
