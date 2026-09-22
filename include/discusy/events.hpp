#pragma once

#include <string>
#include <vector>
#include <variant>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <type_traits>
#include <utility>
#include <concepts>

#include <glaze/glaze.hpp>

#include "types.hpp"
#include "io_context.hpp"

static_assert(true, "Clangd bug fix");

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-attributes"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-braces"
#endif

namespace discusy {

class bot;
class http_api;

namespace message {
    struct embed;
    struct message;
}

namespace channel {
    struct channel;
    enum class channel_type : std::uint8_t;
}

namespace guild {
    struct guild;
    struct guild_member;
}

namespace user {
    struct user;
}

namespace interaction {
    struct interaction;
}

namespace api {
    namespace message {
        struct create_message;
        struct edit_message;
    }
    namespace interaction {
        struct interaction_callback_data_message;
    }
    namespace webhook {
        struct edit_webhook_message;
        struct execute_webhook;
    }
}

namespace application_role_connection_metadata {
    enum class application_role_connection_metadata_type : std::uint8_t {
        INTEGER_LESS_THAN_OR_EQUAL = 1, // the metadata value (integer) is less than or equal to the guild’s configured value (integer)
        INTEGER_GREATER_THAN_OR_EQUAL = 2, // the metadata value (integer) is greater than or equal to the guild’s configured value (integer)
        INTEGER_EQUAL = 3, // the metadata value (integer) is equal to the guild’s configured value (integer)
        INTEGER_NOT_EQUAL = 4, // the metadata value (integer) is not equal to the guild’s configured value (integer)
        DATETIME_LESS_THAN_OR_EQUAL = 5, // the metadata value (ISO8601 string) is less than or equal to the guild’s configured value (integer; days before current date)
        DATETIME_GREATER_THAN_OR_EQUAL = 6, // the metadata value (ISO8601 string) is greater than or equal to the guild’s configured value (integer; days before current date)
        BOOLEAN_EQUAL = 7, // the metadata value (integer) is equal to the guild’s configured value (integer; 1)
        BOOLEAN_NOT_EQUAL = 8, // the metadata value (integer) is not equal to the guild’s configured value (integer; 1)
    };

    struct application_role_connection_metadata {
        application_role_connection_metadata_type type{};
        std::string key{};
        std::string name{};
        opt<std::unordered_map<std::string, std::string>> name_localizations{};
        std::string description{};
        opt<std::unordered_map<std::string, std::string>> description_localizations{};

        static application_role_connection_metadata create(application_role_connection_metadata_type type_ = application_role_connection_metadata_type::INTEGER_LESS_THAN_OR_EQUAL, std::string key_ = {}, std::string name_ = {}, std::string description_ = {}) {
            return application_role_connection_metadata{
                .type = type_,
                .key = std::move(key_),
                .name = std::move(name_),
                .description = std::move(description_),
            };
        }
        decltype(auto) set_type(this auto&& self, application_role_connection_metadata_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_key(this auto&& self, std::string key_) { self.key = std::move(key_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_name(this auto&& self, std::string name_) { self.name = std::move(name_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_name_localizations(this auto&& self, opt<std::unordered_map<std::string, std::string>> name_localizations_) { self.name_localizations = std::move(name_localizations_); return std::forward<decltype(self)>(self); }
        decltype(auto) add_name_localization(this auto&& self, std::string k, std::string v) {
            if (!self.name_localizations) self.name_localizations.emplace();
            (*self.name_localizations)[std::move(k)] = std::move(v);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_description(this auto&& self, std::string description_) { self.description = std::move(description_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_description_localizations(this auto&& self, opt<std::unordered_map<std::string, std::string>> description_localizations_) { self.description_localizations = std::move(description_localizations_); return std::forward<decltype(self)>(self); }
        decltype(auto) add_description_localization(this auto&& self, std::string k, std::string v) {
            if (!self.description_localizations) self.description_localizations.emplace();
            (*self.description_localizations)[std::move(k)] = std::move(v);
            return std::forward<decltype(self)>(self);
        }
    };
};

namespace guild {
    struct guild_member;
};

namespace user {

    // User's primary guild
    struct user_primary_guild {
        // ID of the user's primary guild
        opt<snowflake> identity_guild_id{};
        // Whether the user is displaying the primary guild's server tag
        opt<bool> identity_enabled{};
        // Text of the user's server tag. Limited to 4 characters
        opt<std::string> tag{};
        // Server tag badge hash
        opt<guild_tag_badge_hash> badge{};

        static user_primary_guild create(opt<snowflake> id_ = {}, opt<bool> enabled_ = {}, opt<std::string> tag_ = {}) {
            return user_primary_guild{
                .identity_guild_id = id_,
                .identity_enabled = enabled_,
                .tag = std::move(tag_),
            };
        }
        decltype(auto) set_identity_guild_id(this auto&& self, opt<snowflake> identity_guild_id_) noexcept { self.identity_guild_id = identity_guild_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_identity_guild(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_identity_guild_id(id_); }
        decltype(auto) set_identity_enabled(this auto&& self, opt<bool> identity_enabled_) noexcept { self.identity_enabled = identity_enabled_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_tag(this auto&& self, opt<std::string> tag_) { self.tag = std::move(tag_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_badge(this auto&& self, opt<guild_tag_badge_hash> badge_) { self.badge = std::move(badge_); return std::forward<decltype(self)>(self); }
    };

    // User Resource Structures
    // Nameplate data for the user
    struct nameplate {
        // ID of the nameplate SKU
        snowflake sku_id{};
        // Path to the nameplate asset
        std::string asset{};
        // The label of this nameplate
        std::string label{};
        // Background color of the nameplate
        std::string palette{}; // COULD BE ENUM

        static nameplate create(snowflake sku_id_ = {}, std::string asset_ = {}, std::string label_ = {}, std::string palette_ = {}) {
            return nameplate{
                .sku_id = sku_id_,
                .asset = std::move(asset_),
                .label = std::move(label_),
                .palette = std::move(palette_),
            };
        }
        decltype(auto) set_sku_id(this auto&& self, snowflake sku_id_) noexcept { self.sku_id = sku_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_sku(this auto&& self, snowflake id_) noexcept { return std::forward<decltype(self)>(self).set_sku_id(id_); }
        decltype(auto) set_asset(this auto&& self, std::string asset_) { self.asset = std::move(asset_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_label(this auto&& self, std::string label_) { self.label = std::move(label_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_palette(this auto&& self, std::string palette_) { self.palette = std::move(palette_); return std::forward<decltype(self)>(self); }
    };

    // Avatar decoration data for the user
    struct avatar_decoration_data {
        // Avatar decoration hash
        avatar_decoration_hash asset{};
        // ID of the avatar decoration's SKU
        snowflake sku_id{};

        static avatar_decoration_data create(avatar_decoration_hash asset_ = {}, snowflake sku_id_ = {}) {
            return avatar_decoration_data{.asset = std::move(asset_), .sku_id = sku_id_};
        }
        decltype(auto) set_asset(this auto&& self, avatar_decoration_hash asset_) { self.asset = std::move(asset_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_sku_id(this auto&& self, snowflake sku_id_) noexcept { self.sku_id = sku_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_sku(this auto&& self, snowflake id_) noexcept { return std::forward<decltype(self)>(self).set_sku_id(id_); }
    };

    // Collectibles the user has
    struct collectibles {
        // Nameplate data if the user has one
        opt<discusy::user::nameplate> nameplate{};

        static collectibles create(opt<discusy::user::nameplate> nameplate_ = {}) {
            return collectibles{.nameplate = std::move(nameplate_)};
        }
        decltype(auto) set_nameplate(this auto&& self, opt<discusy::user::nameplate> nameplate_) { self.nameplate = std::move(nameplate_); return std::forward<decltype(self)>(self); }
    };

    enum class user_flags : std::uint32_t {
        STAFF = 1ULL << 0, // Discord Employee
        PARTNER = 1ULL << 1, // Partnered Server Owner
        HYPESQUAD = 1ULL << 2, // HypeSquad Events Member
        BUG_HUNTER_LEVEL_1 = 1ULL << 3, // Bug Hunter Level 1
        HYPESQUAD_ONLINE_HOUSE_1 = 1ULL << 6, // House Bravery Member
        HYPESQUAD_ONLINE_HOUSE_2 = 1ULL << 7, // House Brilliance Member
        HYPESQUAD_ONLINE_HOUSE_3 = 1ULL << 8, // House Balance Member
        PREMIUM_EARLY_SUPPORTER = 1ULL << 9, // Early Nitro Supporter
        TEAM_PSEUDO_USER = 1ULL << 10, // User is a team
        BUG_HUNTER_LEVEL_2 = 1ULL << 14, // Bug Hunter Level 2
        VERIFIED_BOT = 1ULL << 16, // Verified Bot
        VERIFIED_DEVELOPER = 1ULL << 17, // Early Verified Bot Developer
        CERTIFIED_MODERATOR = 1ULL << 18, // Moderator Programs Alumni
        BOT_HTTP_INTERACTIONS = 1ULL << 19, // Bot uses only HTTP interactions and is shown in the online member list
    };

    enum class premium_type : std::uint8_t {
        None = 0,
        NitroClassic = 1,
        Nitro = 2,
        NitroBasic = 3,
    };

    // User object
    struct user {
        // The user's id
        snowflake id{};
        // The user's username, not unique across the platform
        std::string username{};
        // The user's Discord-tag
        std::string discriminator{};
        // The user's display name, if it is set
        opt<std::string> global_name{};
        // The user's avatar hash
        opt<user_avatar_hash> avatar{};
        // Whether the user belongs to an OAuth2 application
        opt<bool> bot{};
        // Whether the user is an Official Discord System user
        opt<bool> system{};
        // Whether the user has two factor enabled on their account
        opt<bool> mfa_enabled{};
        // The user's banner hash
        opt<user_banner_hash> banner{};
        // The user's banner color encoded as an integer
        opt<integer> accent_color{};
        // The user's chosen language option
        opt<std::string> locale{};
        // Whether the email on this account has been verified
        opt<bool> verified{};
        // The user's email
        opt<std::string> email{};
        // The flags on a user's account
        opt<flags_t<user_flags>> flags{};
        // The type of Nitro subscription on a user's account
        opt<discusy::user::premium_type> premium_type{};
        // The public flags on a user's account
        opt<flags_t<user_flags>> public_flags{};
        // Data for the user's avatar decoration
        opt<discusy::user::avatar_decoration_data> avatar_decoration_data{};
        // Collectibles the user has
        opt<discusy::user::collectibles> collectibles{};
        // The user's primary guild
        opt<user_primary_guild> primary_guild{};

        // for message create
        opt<std::shared_ptr<guild::guild_member>> member{};

        static user create(snowflake id_ = {}, std::string username_ = {}, std::string discriminator_ = {}) {
            return user{
                .id = id_,
                .username = std::move(username_),
                .discriminator = std::move(discriminator_),
            };
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_username(this auto&& self, std::string username_) { self.username = std::move(username_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_discriminator(this auto&& self, std::string discriminator_) { self.discriminator = std::move(discriminator_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_global_name(this auto&& self, opt<std::string> global_name_) { self.global_name = std::move(global_name_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_avatar(this auto&& self, opt<user_avatar_hash> avatar_) { self.avatar = std::move(avatar_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_bot(this auto&& self, opt<bool> bot_) noexcept { self.bot = bot_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_system(this auto&& self, opt<bool> system_) noexcept { self.system = system_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_mfa_enabled(this auto&& self, opt<bool> mfa_enabled_) noexcept { self.mfa_enabled = mfa_enabled_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_banner(this auto&& self, opt<user_banner_hash> banner_) { self.banner = std::move(banner_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_accent_color(this auto&& self, opt<integer> accent_color_) noexcept { self.accent_color = accent_color_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_locale(this auto&& self, opt<std::string> locale_) { self.locale = std::move(locale_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_verified(this auto&& self, opt<bool> verified_) noexcept { self.verified = verified_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_email(this auto&& self, opt<std::string> email_) { self.email = std::move(email_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_flags(this auto&& self, opt<flags_t<user_flags>> flags_) noexcept { self.flags = flags_; return std::forward<decltype(self)>(self); }
        decltype(auto) add_flags(this auto&& self, user_flags f) noexcept {
            if (!self.flags) self.flags.emplace();
            self.flags->add_flags(f);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) add_flag(this auto&& self, user_flags f) noexcept { return std::forward<decltype(self)>(self).add_flags(f); }
        decltype(auto) set_premium_type(this auto&& self, opt<discusy::user::premium_type> premium_type_) noexcept { self.premium_type = premium_type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_public_flags(this auto&& self, opt<flags_t<user_flags>> public_flags_) noexcept { self.public_flags = public_flags_; return std::forward<decltype(self)>(self); }
        decltype(auto) add_public_flags(this auto&& self, user_flags f) noexcept {
            if (!self.public_flags) self.public_flags.emplace();
            self.public_flags->add_flags(f);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) add_public_flag(this auto&& self, user_flags f) noexcept { return std::forward<decltype(self)>(self).add_public_flags(f); }
        decltype(auto) set_avatar_decoration_data(this auto&& self, opt<discusy::user::avatar_decoration_data> avatar_decoration_data_) { self.avatar_decoration_data = std::move(avatar_decoration_data_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_collectibles(this auto&& self, opt<discusy::user::collectibles> collectibles_) { self.collectibles = std::move(collectibles_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_primary_guild(this auto&& self, opt<user_primary_guild> primary_guild_) { self.primary_guild = std::move(primary_guild_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_member(this auto&& self, opt<std::shared_ptr<guild::guild_member>> member_) { self.member = std::move(member_); return std::forward<decltype(self)>(self); }

        discusy::bot* bot_ptr_{nullptr};
        void set_bot_void(this auto&& self, discusy::bot* b) noexcept { self.bot_ptr_ = b; }
        decltype(auto) set_bot(this auto&& self, discusy::bot* b) noexcept { self.set_bot_void(b); return std::forward<decltype(self)>(self); }

        [[nodiscard]] std::string mention(this const auto& self) {
            return self.id.mention_user();
        }
        [[nodiscard]] std::string_view display_name(this const auto& self) noexcept {
            if (self.global_name && !self.global_name->empty()) return *self.global_name;
            return self.username;
        }

        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto create_dm(this auto&& self, CompletionToken&& token = ctx::io_context::dct_t());
    };

    enum class connection_visibility : std::uint8_t {
        None = 0, // invisible to everyone except the user themselves
        Everyone = 1, // visible to everyone
    };

    struct application_role_connection {
        opt<std::string> platform_name{};
        std::unordered_map<std::string, application_role_connection_metadata::application_role_connection_metadata> metadata{};

        static application_role_connection create(opt<std::string> platform_name_ = {}) {
            return application_role_connection{.platform_name = std::move(platform_name_)};
        }
        decltype(auto) set_platform_name(this auto&& self, opt<std::string> platform_name_) { self.platform_name = std::move(platform_name_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_metadata(this auto&& self, std::unordered_map<std::string, application_role_connection_metadata::application_role_connection_metadata> metadata_) { self.metadata = std::move(metadata_); return std::forward<decltype(self)>(self); }
        decltype(auto) add_metadata(this auto&& self, std::string k, application_role_connection_metadata::application_role_connection_metadata v) {
            self.metadata[std::move(k)] = std::move(v);
            return std::forward<decltype(self)>(self);
        }
    };
}

namespace sticker {
    enum class sticker_type : std::uint8_t {
        STANDARD = 1, // an official sticker in a pack
        GUILD = 2, // a sticker uploaded to a guild for the guild’s members
    };

    enum class sticker_format_type : std::uint8_t {
        PNG = 1,
        APNG = 2,
        LOTTIE = 3,
        GIF = 4,
    };

    struct sticker {
        snowflake id{};
        opt<snowflake> pack_id{};
        std::string name{};
        opt<std::string> description{};
        std::string tags{}; // comma separated
        sticker_type type{};
        sticker_format_type format_type{};
        opt<bool> available{};
        opt<snowflake> guild_id{};
        opt<user::user> user{};
        opt<integer> sort_value{};

        static sticker create(snowflake id_ = {}, std::string name_ = {}, sticker_type type_ = sticker_type::STANDARD, sticker_format_type format_type_ = sticker_format_type::PNG) {
            return sticker{
                .id = id_,
                .name = std::move(name_),
                .type = type_,
                .format_type = format_type_,
            };
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_pack_id(this auto&& self, opt<snowflake> pack_id_) noexcept { self.pack_id = pack_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_pack(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_pack_id(id_); }
        decltype(auto) set_name(this auto&& self, std::string name_) { self.name = std::move(name_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_description(this auto&& self, opt<std::string> description_) { self.description = std::move(description_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_tags(this auto&& self, std::string tags_) { self.tags = std::move(tags_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_type(this auto&& self, sticker_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_format_type(this auto&& self, sticker_format_type format_type_) noexcept { self.format_type = format_type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_available(this auto&& self, opt<bool> available_) noexcept { self.available = available_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild_id(this auto&& self, opt<snowflake> guild_id_) noexcept { self.guild_id = guild_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_guild_id(id_); }
        decltype(auto) set_user(this auto&& self, opt<user::user> user_) { self.user = std::move(user_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_sort_value(this auto&& self, opt<integer> sort_value_) noexcept { self.sort_value = sort_value_; return std::forward<decltype(self)>(self); }
    };

    struct sticker_item {
        snowflake id{};
        std::string name{};
        sticker_format_type format_type{};

        static sticker_item create(snowflake id_ = {}, std::string name_ = {}, sticker_format_type format_type_ = sticker_format_type::PNG) {
            return sticker_item{
                .id = id_,
                .name = std::move(name_),
                .format_type = format_type_,
            };
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_name(this auto&& self, std::string name_) { self.name = std::move(name_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_format_type(this auto&& self, sticker_format_type format_type_) noexcept { self.format_type = format_type_; return std::forward<decltype(self)>(self); }
    };

    struct sticker_pack {
        snowflake id{};
        std::vector<sticker> stickers{};
        std::string name{};
        snowflake sku_id{};
        opt<snowflake> cover_sticker_id{};
        std::string description{};
        opt<snowflake> banner_asset_id{};

        static sticker_pack create(snowflake id_ = {}, std::string name_ = {}, snowflake sku_id_ = {}) {
            return sticker_pack{.id = id_, .name = std::move(name_), .sku_id = sku_id_};
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_stickers(this auto&& self, std::vector<sticker> stickers_) { self.stickers = std::move(stickers_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_stickers, sticker)
        decltype(auto) add_sticker(this auto&& self, sticker itm) { self.stickers.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_name(this auto&& self, std::string name_) { self.name = std::move(name_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_sku_id(this auto&& self, snowflake sku_id_) noexcept { self.sku_id = sku_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_sku(this auto&& self, snowflake id_) noexcept { return std::forward<decltype(self)>(self).set_sku_id(id_); }
        decltype(auto) set_cover_sticker_id(this auto&& self, opt<snowflake> cover_sticker_id_) noexcept { self.cover_sticker_id = cover_sticker_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_cover_sticker(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_cover_sticker_id(id_); }
        decltype(auto) set_description(this auto&& self, std::string description_) { self.description = std::move(description_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_banner_asset_id(this auto&& self, opt<snowflake> banner_asset_id_) noexcept { self.banner_asset_id = banner_asset_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_banner_asset(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_banner_asset_id(id_); }
    };
}

namespace emoji {
    struct emoji {
        opt<snowflake> id{};
        opt<std::string> name{};
        opt<std::vector<snowflake>> roles{};
        opt<user::user> user{};
        opt<bool> require_colons{};
        opt<bool> managed{};
        opt<bool> animated{};
        opt<bool> available{};

        static emoji create(opt<snowflake> id_ = {}, opt<std::string> name_ = {}) {
            return emoji{.id = id_, .name = std::move(name_)};
        }
        decltype(auto) set_id(this auto&& self, opt<snowflake> id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_name(this auto&& self, opt<std::string> name_) { self.name = std::move(name_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_roles(this auto&& self, opt<std::vector<snowflake>> roles_) { self.roles = std::move(roles_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_roles, snowflake)
        decltype(auto) add_role(this auto&& self, snowflake itm) {
            if (!self.roles) self.roles.emplace();
            self.roles->emplace_back(itm);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_user(this auto&& self, opt<user::user> user_) { self.user = std::move(user_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_require_colons(this auto&& self, opt<bool> require_colons_) noexcept { self.require_colons = require_colons_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_managed(this auto&& self, opt<bool> managed_) noexcept { self.managed = managed_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_animated(this auto&& self, opt<bool> animated_) noexcept { self.animated = animated_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_available(this auto&& self, opt<bool> available_) noexcept { self.available = available_; return std::forward<decltype(self)>(self); }
    };
}

namespace permissions {
    struct role_colors {
        integer primary_color{};
        opt<integer> secondary_color{};
        opt<integer> tertiary_color{};

        static role_colors create(integer primary_color_ = 0, opt<integer> secondary_color_ = {}, opt<integer> tertiary_color_ = {}) noexcept {
            return role_colors{
                .primary_color = primary_color_,
                .secondary_color = secondary_color_,
                .tertiary_color = tertiary_color_,
            };
        }
        decltype(auto) set_primary_color(this auto&& self, integer primary_color_) noexcept { self.primary_color = primary_color_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_secondary_color(this auto&& self, opt<integer> secondary_color_) noexcept { self.secondary_color = secondary_color_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_tertiary_color(this auto&& self, opt<integer> tertiary_color_) noexcept { self.tertiary_color = tertiary_color_; return std::forward<decltype(self)>(self); }
    };

    enum class role_flags : std::uint8_t {
        IN_PROMPT = 1ULL << 0, // role can be selected by members in an onboarding prompt
    };

    struct role_tags {
        opt<snowflake> bot_id{};
        opt<snowflake> integration_id{};
        null_bool premium_subscriber{};
        opt<snowflake> subscription_listing_id{};
        null_bool available_for_purchase{};
        null_bool guild_connections{};

        static role_tags create() noexcept {
            return role_tags{};
        }
        decltype(auto) set_bot_id(this auto&& self, opt<snowflake> bot_id_) noexcept { self.bot_id = bot_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_bot(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_bot_id(id_); }
        decltype(auto) set_integration_id(this auto&& self, opt<snowflake> integration_id_) noexcept { self.integration_id = integration_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_integration(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_integration_id(id_); }
        decltype(auto) set_premium_subscriber(this auto&& self, null_bool premium_subscriber_) noexcept { self.premium_subscriber = premium_subscriber_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_subscription_listing_id(this auto&& self, opt<snowflake> subscription_listing_id_) noexcept { self.subscription_listing_id = subscription_listing_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_subscription_listing(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_subscription_listing_id(id_); }
        decltype(auto) set_available_for_purchase(this auto&& self, null_bool available_for_purchase_) noexcept { self.available_for_purchase = available_for_purchase_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild_connections(this auto&& self, null_bool guild_connections_) noexcept { self.guild_connections = guild_connections_; return std::forward<decltype(self)>(self); }
    };

    struct role {
        snowflake id{};
        std::string name{};
        integer color{};
        role_colors colors{};
        bool hoist{};
        opt<role_icon_hash> icon{};
        opt<std::string> unicode_emoji{};
        integer position{};
        permissions_t permissions{};
        bool managed{};
        bool mentionable{};
        opt<role_tags> tags{};
        flags_t<role_flags> flags{};

        static role create(snowflake id_ = {}, std::string name_ = {}, integer color_ = 0, permissions_t permissions_ = {}) {
            return role{
                .id = id_,
                .name = std::move(name_),
                .color = color_,
                .permissions = permissions_,
            };
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_name(this auto&& self, std::string name_) { self.name = std::move(name_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_color(this auto&& self, integer color_) noexcept { self.color = color_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_colors(this auto&& self, role_colors colors_) noexcept { self.colors = colors_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_hoist(this auto&& self, bool hoist_) noexcept { self.hoist = hoist_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_icon(this auto&& self, opt<role_icon_hash> icon_) { self.icon = std::move(icon_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_unicode_emoji(this auto&& self, opt<std::string> unicode_emoji_) { self.unicode_emoji = std::move(unicode_emoji_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_position(this auto&& self, integer position_) noexcept { self.position = position_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_permissions(this auto&& self, permissions_t permissions_) noexcept { self.permissions = permissions_; return std::forward<decltype(self)>(self); }
        decltype(auto) add_permission(this auto&& self, discusy::permissions::permissions p) noexcept { self.permissions.add_flags(p); return std::forward<decltype(self)>(self); }
        decltype(auto) add_permissions(this auto&& self, discusy::permissions::permissions p) noexcept { return std::forward<decltype(self)>(self).add_permission(p); }
        decltype(auto) set_managed(this auto&& self, bool managed_) noexcept { self.managed = managed_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_mentionable(this auto&& self, bool mentionable_) noexcept { self.mentionable = mentionable_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_tags(this auto&& self, opt<role_tags> tags_) { self.tags = std::move(tags_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_flags(this auto&& self, flags_t<role_flags> flags_) noexcept { self.flags = flags_; return std::forward<decltype(self)>(self); }
        decltype(auto) add_flags(this auto&& self, role_flags f) noexcept { self.flags.add_flags(f); return std::forward<decltype(self)>(self); }
        decltype(auto) add_flag(this auto&& self, role_flags f) noexcept { return std::forward<decltype(self)>(self).add_flags(f); }

        [[nodiscard]] std::string mention(this const auto& self) {
            return self.id.mention_role();
        }
    };
}

namespace guild {
    struct integration_account {
        std::string id{};
        std::string name{};

        static integration_account create(std::string id_ = {}, std::string name_ = {}) {
            return integration_account{.id = std::move(id_), .name = std::move(name_)};
        }
        decltype(auto) set_id(this auto&& self, std::string id_) { self.id = std::move(id_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_name(this auto&& self, std::string name_) { self.name = std::move(name_); return std::forward<decltype(self)>(self); }
    };

    struct integration_application {
        snowflake id{};
        std::string name{};
        opt<application_icon_hash> icon{};
        std::string description{};
        opt<user::user> bot{};

        static integration_application create(snowflake id_ = {}, std::string name_ = {}, std::string description_ = {}) {
            return integration_application{
                .id = id_,
                .name = std::move(name_),
                .description = std::move(description_),
            };
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_name(this auto&& self, std::string name_) { self.name = std::move(name_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_icon(this auto&& self, opt<application_icon_hash> icon_) { self.icon = std::move(icon_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_description(this auto&& self, std::string description_) { self.description = std::move(description_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_bot(this auto&& self, opt<user::user> bot_) { self.bot = std::move(bot_); return std::forward<decltype(self)>(self); }
    };

    enum class integration_expire_behaviours : std::uint8_t {
        RemoveRole = 0,
        Kick = 1,
    };

    struct integration {
        snowflake id{};
        std::string name{};
        std::string type{};
        bool enabled{};
        opt<bool> syncing{};
        opt<snowflake> role_id{};
        opt<bool> enable_emoticons{};
        opt<integration_expire_behaviours> expire_behavior{};
        opt<integer> expire_grace_period{};
        opt<user::user> user{};
        integration_account account{};
        opt<timestamp> synced_at{};
        opt<integer> subscriber_count{};
        opt<bool> revoked{};
        opt<integration_application> application{};
        opt<std::vector<std::string>> scopes{};

        static integration create(snowflake id_ = {}, std::string name_ = {}, std::string type_ = {}) {
            return integration{
                .id = id_,
                .name = std::move(name_),
                .type = std::move(type_),
            };
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_name(this auto&& self, std::string name_) { self.name = std::move(name_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_type(this auto&& self, std::string type_) { self.type = std::move(type_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_enabled(this auto&& self, bool enabled_) noexcept { self.enabled = enabled_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_syncing(this auto&& self, opt<bool> syncing_) noexcept { self.syncing = syncing_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_role_id(this auto&& self, opt<snowflake> role_id_) noexcept { self.role_id = role_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_role(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_role_id(id_); }
        decltype(auto) set_enable_emoticons(this auto&& self, opt<bool> enable_emoticons_) noexcept { self.enable_emoticons = enable_emoticons_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_expire_behavior(this auto&& self, opt<integration_expire_behaviours> expire_behavior_) noexcept { self.expire_behavior = expire_behavior_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_expire_grace_period(this auto&& self, opt<integer> expire_grace_period_) noexcept { self.expire_grace_period = expire_grace_period_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_user(this auto&& self, opt<user::user> user_) { self.user = std::move(user_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_account(this auto&& self, integration_account account_) { self.account = std::move(account_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_synced_at(this auto&& self, opt<timestamp> synced_at_) noexcept { self.synced_at = synced_at_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_subscriber_count(this auto&& self, opt<integer> subscriber_count_) noexcept { self.subscriber_count = subscriber_count_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_revoked(this auto&& self, opt<bool> revoked_) noexcept { self.revoked = revoked_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_application(this auto&& self, opt<integration_application> application_) { self.application = std::move(application_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_scopes(this auto&& self, opt<std::vector<std::string>> scopes_) { self.scopes = std::move(scopes_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_scopes, std::string)
        decltype(auto) add_scope(this auto&& self, std::string itm) {
            if (!self.scopes) self.scopes.emplace();
            self.scopes->emplace_back(std::move(itm));
            return std::forward<decltype(self)>(self);
        }
    };

    struct integration_with_guild {
        discusy::guild::integration integration{};

        static integration_with_guild create(discusy::guild::integration integ = {}, snowflake gid = {}) {
            return integration_with_guild{.integration = std::move(integ), .other = {.guild_id = gid}};
        }
        decltype(auto) set_integration(this auto&& self, discusy::guild::integration integ) { self.integration = std::move(integ); return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild_id(this auto&& self, snowflake gid) noexcept { self.other.guild_id = gid; return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild(this auto&& self, snowflake gid) noexcept { return std::forward<decltype(self)>(self).set_guild_id(gid); }

        struct Other {
            snowflake guild_id{};
        } other;

        constexpr auto* operator->(this auto&& self) noexcept { return std::addressof(self.integration); }
        constexpr decltype(auto) operator*(this auto&& self) noexcept { return (std::forward<decltype(self)>(self).integration); }
        constexpr decltype(auto) operator()(this auto&& self) noexcept { return (std::forward<decltype(self)>(self).other); }

        struct glaze {
            using T = integration_with_guild;
            static constexpr auto value = glz::merge{
                &T::integration,
                &T::other
            };
        };
    };

    struct ban {
        opt<std::string> reason{};
        user::user user{};

        static ban create(user::user user_ = {}, opt<std::string> reason_ = {}) {
            return ban{.reason = std::move(reason_), .user = std::move(user_)};
        }
        decltype(auto) set_reason(this auto&& self, opt<std::string> reason_) { self.reason = std::move(reason_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_user(this auto&& self, user::user user_) { self.user = std::move(user_); return std::forward<decltype(self)>(self); }
    };

    struct onboarding_prompt_option {
        snowflake id{};
        std::vector<snowflake> channel_ids{};
        std::vector<snowflake> role_ids{};
        opt<emoji::emoji> emoji{};
        opt<snowflake> emoji_id{};
        opt<std::string> emoji_name{};
        opt<bool> emoji_animated{};
        std::string title{};
        opt<std::string> description{};

        static onboarding_prompt_option create(snowflake id_ = {}, std::string title_ = {}) {
            return onboarding_prompt_option{.id = id_, .title = std::move(title_)};
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_channel_ids(this auto&& self, std::vector<snowflake> channel_ids_) { self.channel_ids = std::move(channel_ids_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_channel_ids, snowflake)
        decltype(auto) add_channel_id(this auto&& self, snowflake itm) noexcept { self.channel_ids.emplace_back(itm); return std::forward<decltype(self)>(self); }
        decltype(auto) set_role_ids(this auto&& self, std::vector<snowflake> role_ids_) { self.role_ids = std::move(role_ids_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_role_ids, snowflake)
        decltype(auto) add_role_id(this auto&& self, snowflake itm) noexcept { self.role_ids.emplace_back(itm); return std::forward<decltype(self)>(self); }
        decltype(auto) set_emoji(this auto&& self, opt<emoji::emoji> emoji_) { self.emoji = std::move(emoji_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_emoji_id(this auto&& self, opt<snowflake> emoji_id_) noexcept { self.emoji_id = emoji_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_emoji_name(this auto&& self, opt<std::string> emoji_name_) { self.emoji_name = std::move(emoji_name_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_emoji_animated(this auto&& self, opt<bool> emoji_animated_) noexcept { self.emoji_animated = emoji_animated_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_title(this auto&& self, std::string title_) { self.title = std::move(title_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_description(this auto&& self, opt<std::string> description_) { self.description = std::move(description_); return std::forward<decltype(self)>(self); }
    };

    enum class onboarding_prompt_type : std::uint8_t {
        MULTIPLE_CHOICE = 0,
        DROPDOWN = 1,
    };

    enum class onboarding_mode : std::uint8_t {
        ONBOARDING_DEFAULT = 0, // Counts only Default Channels towards constraints
        ONBOARDING_ADVANCED = 1, // Counts Default Channels and Questions towards constraints
    };

    struct onboarding_prompt {
        snowflake id{};
        onboarding_prompt_type type{};
        std::vector<onboarding_prompt_option> options{};
        std::string title{};
        bool single_select{};
        bool required{};
        bool in_onboarding{};

        static onboarding_prompt create(snowflake id_ = {}, onboarding_prompt_type type_ = onboarding_prompt_type::MULTIPLE_CHOICE, std::string title_ = {}) {
            return onboarding_prompt{.id = id_, .type = type_, .title = std::move(title_)};
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_type(this auto&& self, onboarding_prompt_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_options(this auto&& self, std::vector<onboarding_prompt_option> options_) { self.options = std::move(options_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_options, onboarding_prompt_option)
        decltype(auto) add_option(this auto&& self, onboarding_prompt_option itm) { self.options.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_title(this auto&& self, std::string title_) { self.title = std::move(title_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_single_select(this auto&& self, bool single_select_) noexcept { self.single_select = single_select_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_required(this auto&& self, bool required_) noexcept { self.required = required_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_in_onboarding(this auto&& self, bool in_onboarding_) noexcept { self.in_onboarding = in_onboarding_; return std::forward<decltype(self)>(self); }
    };

    struct guild_onboarding {
        snowflake guild_id{};
        std::vector<onboarding_prompt> prompts{};
        std::vector<snowflake> default_channel_ids{};
        bool enabled{};
        onboarding_mode mode{};

        static guild_onboarding create(snowflake guild_id_ = {}, bool enabled_ = false, onboarding_mode mode_ = onboarding_mode::ONBOARDING_DEFAULT) noexcept {
            return guild_onboarding{
                .guild_id = guild_id_,
                .enabled = enabled_,
                .mode = mode_,
            };
        }
        decltype(auto) set_guild_id(this auto&& self, snowflake guild_id_) noexcept { self.guild_id = guild_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild(this auto&& self, snowflake id_) noexcept { return std::forward<decltype(self)>(self).set_guild_id(id_); }
        decltype(auto) set_prompts(this auto&& self, std::vector<onboarding_prompt> prompts_) { self.prompts = std::move(prompts_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_prompts, onboarding_prompt)
        decltype(auto) add_prompt(this auto&& self, onboarding_prompt itm) { self.prompts.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_default_channel_ids(this auto&& self, std::vector<snowflake> default_channel_ids_) { self.default_channel_ids = std::move(default_channel_ids_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_default_channel_ids, snowflake)
        decltype(auto) add_default_channel_id(this auto&& self, snowflake itm) noexcept { self.default_channel_ids.emplace_back(itm); return std::forward<decltype(self)>(self); }
        decltype(auto) set_enabled(this auto&& self, bool enabled_) noexcept { self.enabled = enabled_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_mode(this auto&& self, onboarding_mode mode_) noexcept { self.mode = mode_; return std::forward<decltype(self)>(self); }
    };

    enum class guild_member_flags : std::uint16_t {
        DID_REJOIN = 1ULL << 0, // Member has left and rejoined the guild	false
        COMPLETED_ONBOARDING = 1ULL << 1, // Member has completed onboarding	false
        BYPASSES_VERIFICATION = 1ULL << 2, // Member is exempt from guild verification requirements	true
        STARTED_ONBOARDING = 1ULL << 3, // Member has started onboarding	false
        IS_GUEST = 1ULL << 4, // Member is a guest and can only access the voice channel they were invited to	false
        STARTED_HOME_ACTIONS = 1ULL << 5, // Member has started Server Guide new member actions	false
        COMPLETED_HOME_ACTIONS = 1ULL << 6, // Member has completed Server Guide new member actions	false
        AUTOMOD_QUARANTINED_USERNAME = 1ULL << 7, // Member’s username, display name, or nickname is blocked by AutoMod	false
        DM_SETTINGS_UPSELL_ACKNOWLEDGED = 1ULL << 9, // Member has dismissed the DM settings upsell	false
        AUTOMOD_QUARANTINED_GUILD_TAG = 1ULL << 10, // Member’s guild tag is blocked by AutoMod	false
    };

    struct guild_member {
        opt<user::user> user{};
        opt<std::string> nick{};
        opt<guild_member_avatar_hash> avatar{};
        opt<guild_member_banner_hash> banner{};
        std::vector<snowflake> roles{};
        opt<timestamp> joined_at{};
        opt<timestamp> premium_since{};
        opt<bool> deaf{}; // sometimes might be missing
        opt<bool> mute{};
        flags_t<guild_member_flags> flags{};
        opt<bool> pending{};
        opt<permissions_t> permissions{};
        opt<timestamp> communication_disabled_until{};
        opt<user::avatar_decoration_data> avatar_decoration_data{};
        opt<user::collectibles> collectibles{};

        static guild_member create(opt<user::user> user_ = {}, std::vector<snowflake> roles_ = {}) {
            return guild_member{.user = std::move(user_), .roles = std::move(roles_)};
        }
        decltype(auto) set_user(this auto&& self, opt<user::user> user_) { self.user = std::move(user_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_nick(this auto&& self, opt<std::string> nick_) { self.nick = std::move(nick_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_avatar(this auto&& self, opt<guild_member_avatar_hash> avatar_) { self.avatar = std::move(avatar_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_banner(this auto&& self, opt<guild_member_banner_hash> banner_) { self.banner = std::move(banner_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_roles(this auto&& self, std::vector<snowflake> roles_) { self.roles = std::move(roles_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_roles, snowflake)
        decltype(auto) add_role(this auto&& self, snowflake itm) noexcept { self.roles.emplace_back(itm); return std::forward<decltype(self)>(self); }
        decltype(auto) set_joined_at(this auto&& self, opt<timestamp> joined_at_) noexcept { self.joined_at = joined_at_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_premium_since(this auto&& self, opt<timestamp> premium_since_) noexcept { self.premium_since = premium_since_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_deaf(this auto&& self, opt<bool> deaf_) noexcept { self.deaf = deaf_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_mute(this auto&& self, opt<bool> mute_) noexcept { self.mute = mute_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_flags(this auto&& self, flags_t<guild_member_flags> flags_) noexcept { self.flags = flags_; return std::forward<decltype(self)>(self); }
        decltype(auto) add_flags(this auto&& self, guild_member_flags f) noexcept { self.flags.add_flags(f); return std::forward<decltype(self)>(self); }
        decltype(auto) add_flag(this auto&& self, guild_member_flags f) noexcept { return std::forward<decltype(self)>(self).add_flags(f); }
        decltype(auto) set_pending(this auto&& self, opt<bool> pending_) noexcept { self.pending = pending_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_permissions(this auto&& self, opt<permissions_t> permissions_) noexcept { self.permissions = permissions_; return std::forward<decltype(self)>(self); }
        decltype(auto) add_permission(this auto&& self, permissions::permissions p) noexcept {
            if (!self.permissions) self.permissions.emplace();
            self.permissions->add_flags(p);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) add_permissions(this auto&& self, permissions::permissions p) noexcept { return std::forward<decltype(self)>(self).add_permission(p); }
        decltype(auto) set_communication_disabled_until(this auto&& self, opt<timestamp> communication_disabled_until_) noexcept { self.communication_disabled_until = communication_disabled_until_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_avatar_decoration_data(this auto&& self, opt<user::avatar_decoration_data> avatar_decoration_data_) { self.avatar_decoration_data = std::move(avatar_decoration_data_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_collectibles(this auto&& self, opt<user::collectibles> collectibles_) { self.collectibles = std::move(collectibles_); return std::forward<decltype(self)>(self); }

        discusy::bot* bot_ptr_{nullptr};
        opt<snowflake> guild_id_{std::nullopt};

        void set_bot_void(this auto&& self, discusy::bot* b) noexcept {
            self.bot_ptr_ = b;
            if (self.user) self.user->set_bot_void(b);
        }
        decltype(auto) set_bot(this auto&& self, discusy::bot* b) noexcept { self.set_bot_void(b); return std::forward<decltype(self)>(self); }
        void set_guild_id_context_void(this auto&& self, const opt<snowflake> gid) noexcept {
            self.guild_id_ = gid;
        }
        decltype(auto) set_guild_id_context(this auto&& self, const opt<snowflake> gid) noexcept { self.set_guild_id_context_void(gid); return std::forward<decltype(self)>(self); }

        [[nodiscard]] snowflake user_id(this const auto& self) noexcept {
            return self.user ? self.user->id : snowflake{};
        }
        [[nodiscard]] const user::user* user_ptr(this const auto& self) noexcept {
            return self.user ? &*self.user : nullptr;
        }
        [[nodiscard]] const user::user* user_of(this const auto& self) noexcept {
            return self.user_ptr();
        }
        [[nodiscard]] snowflake user_id_of(this const auto& self) noexcept {
            return self.user_id();
        }
        [[nodiscard]] std::string_view display_name(this const auto& self) noexcept {
            if (self.nick && !self.nick->empty()) return *self.nick;
            if (!self.user) return {};
            if (self.user->global_name && !self.user->global_name->empty()) return *self.user->global_name;
            return self.user->username;
        }
        [[nodiscard]] std::string mention(this const auto& self) {
            return self.user ? self.user->mention() : std::string{};
        }

        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto add_role_api(this auto&& self, snowflake role_id, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto remove_role_api(this auto&& self, snowflake role_id, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto kick(this auto&& self, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto ban(this auto&& self, integer delete_message_days = 0, CompletionToken&& token = ctx::io_context::dct_t());
    };

    struct guild_member_with_guild {
        discusy::guild::guild_member guild_member{};

        static guild_member_with_guild create(discusy::guild::guild_member gm = {}, snowflake gid = {}) {
            return guild_member_with_guild{.guild_member = std::move(gm), .other = {.guild_id = gid}};
        }
        decltype(auto) set_guild_member(this auto&& self, discusy::guild::guild_member gm) { self.guild_member = std::move(gm); return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild_id(this auto&& self, snowflake gid) noexcept { self.other.guild_id = gid; return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild(this auto&& self, snowflake gid) noexcept { return std::forward<decltype(self)>(self).set_guild_id(gid); }

        struct Other {
            snowflake guild_id{};
        } other;

        constexpr auto* operator->(this auto&& self) noexcept { return std::addressof(self.guild_member); }
        constexpr decltype(auto) operator*(this auto&& self) noexcept { return (std::forward<decltype(self)>(self).guild_member); }
        constexpr decltype(auto) operator()(this auto&& self) noexcept { return (std::forward<decltype(self)>(self).other); }

        struct glaze {
            using T = guild_member_with_guild;
            static constexpr auto value = glz::merge{
                &T::guild_member,
                &T::other,
            };
        };
    };

    struct welcome_screen_channel {
        snowflake channel_id{};
        std::string description{};
        opt<snowflake> emoji_id{};
        opt<std::string> emoji_name{};

        static welcome_screen_channel create(snowflake channel_id_ = {}, std::string description_ = {}) {
            return welcome_screen_channel{
                .channel_id = channel_id_,
                .description = std::move(description_),
            };
        }
        decltype(auto) set_channel_id(this auto&& self, snowflake channel_id_) noexcept { self.channel_id = channel_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_channel(this auto&& self, snowflake id_) noexcept { return std::forward<decltype(self)>(self).set_channel_id(id_); }
        decltype(auto) set_description(this auto&& self, std::string description_) { self.description = std::move(description_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_emoji_id(this auto&& self, opt<snowflake> emoji_id_) noexcept { self.emoji_id = emoji_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_emoji(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_emoji_id(id_); }
        decltype(auto) set_emoji_name(this auto&& self, opt<std::string> emoji_name_) { self.emoji_name = std::move(emoji_name_); return std::forward<decltype(self)>(self); }
    };

    struct welcome_screen {
        opt<std::string> description{};
        std::vector<welcome_screen_channel> welcome_channels{};

        static welcome_screen create(opt<std::string> description_ = {}, std::vector<welcome_screen_channel> welcome_channels_ = {}) {
            return welcome_screen{
                .description = std::move(description_),
                .welcome_channels = std::move(welcome_channels_),
            };
        }
        decltype(auto) set_description(this auto&& self, opt<std::string> description_) { self.description = std::move(description_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_welcome_channels(this auto&& self, std::vector<welcome_screen_channel> welcome_channels_) { self.welcome_channels = std::move(welcome_channels_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_welcome_channels, welcome_screen_channel)
        decltype(auto) add_welcome_channel(this auto&& self, welcome_screen_channel itm) { self.welcome_channels.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
    };

    struct incidents_data {
        opt<timestamp> invites_disabled_until{};
        opt<timestamp> dms_disabled_until{};
        opt<timestamp> dm_spam_detected_at{};
        opt<timestamp> raid_detected_at{};

        static incidents_data create() noexcept {
            return incidents_data{};
        }
        decltype(auto) set_invites_disabled_until(this auto&& self, opt<timestamp> invites_disabled_until_) noexcept { self.invites_disabled_until = invites_disabled_until_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_dms_disabled_until(this auto&& self, opt<timestamp> dms_disabled_until_) noexcept { self.dms_disabled_until = dms_disabled_until_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_dm_spam_detected_at(this auto&& self, opt<timestamp> dm_spam_detected_at_) noexcept { self.dm_spam_detected_at = dm_spam_detected_at_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_raid_detected_at(this auto&& self, opt<timestamp> raid_detected_at_) noexcept { self.raid_detected_at = raid_detected_at_; return std::forward<decltype(self)>(self); }
    };

    struct unavailable_guild {
        snowflake id{};
        bool unavailable{};

        static unavailable_guild create(snowflake id_ = {}, bool unavailable_ = false) noexcept {
            return unavailable_guild{.id = id_, .unavailable = unavailable_};
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_unavailable(this auto&& self, bool unavailable_) noexcept { self.unavailable = unavailable_; return std::forward<decltype(self)>(self); }
    };

    enum class verification_level : std::uint8_t {
        NONE = 0, // unrestricted
        LOW = 1, // must have verified email on account
        MEDIUM = 2, // must be registered on Discord for longer than 5 minutes
        HIGH = 3, // must be a member of the server for longer than 10 minutes
        VERY_HIGH = 4, // must have a verified phone number
    };

    enum class default_message_notification_level : std::uint8_t {
        ALL_MESSAGES = 0, // members will receive notifications for all messages by default
        ONLY_MENTIONS = 1, // members will receive notifications only for messages that @mention them by default
    };

    enum class explicit_content_filter_level : std::uint8_t {
        DISABLED = 0, // media content will not be scanned
        MEMBERS_WITHOUT_ROLES = 1, // media content sent by members without roles will be scanned
        ALL_MEMBERS = 2, // media content sent by all members will be scanned
    };

    enum class mfa_level : std::uint8_t {
        NONE = 0, // guild has no MFA/2FA requirement for moderation actions
        ELEVATED = 1, // guild has a 2FA requirement for moderation actions
    };

    enum class system_channel_flags : std::uint8_t {
        SUPPRESS_JOIN_NOTIFICATIONS = 1ULL << 0, // Suppress member join notifications
        SUPPRESS_PREMIUM_SUBSCRIPTIONS = 1ULL << 1, // Suppress server boost notifications
        SUPPRESS_GUILD_REMINDER_NOTIFICATIONS = 1ULL << 2, // Suppress server setup tips
        SUPPRESS_JOIN_NOTIFICATION_REPLIES = 1ULL << 3, // Hide member join sticker reply buttons
        SUPPRESS_ROLE_SUBSCRIPTION_PURCHASE_NOTIFICATIONS = 1ULL << 4, // Suppress role subscription purchase and renewal notifications
        SUPPRESS_ROLE_SUBSCRIPTION_PURCHASE_NOTIFICATION_REPLIES = 1ULL << 5, // Hide role subscription sticker reply buttons
    };

    enum class premium_tier : std::uint8_t {
        NONE = 0, // guild has not unlocked any Server Boost perks
        TIER_1 = 1, // guild has unlocked Server Boost level 1 perks
        TIER_2 = 2, // guild has unlocked Server Boost level 2 perks
        TIER_3 = 3, // guild has unlocked Server Boost level 3 perks
    };

    enum class guild_age_restriction_level : std::uint8_t {
        DEFAULT = 0,
        EXPLICIT = 1,
        SAFE = 2,
        AGE_RESTRICTED = 3,
    };

    struct guild_features {
        static constexpr std::string_view Guild = "Guild";
        static constexpr std::string_view Feature = "Feature";
        static constexpr std::string_view ANIMATED_BANNER = "ANIMATED_BANNER";
        static constexpr std::string_view ANIMATED_ICON = "ANIMATED_ICON";
        static constexpr std::string_view APPLICATION_COMMAND_PERMISSIONS_V2 = "APPLICATION_COMMAND_PERMISSIONS_V2";
        static constexpr std::string_view AUTO_MODERATION = "AUTO_MODERATION";
        static constexpr std::string_view BANNER = "BANNER";
        static constexpr std::string_view COMMUNITY = "COMMUNITY";
        static constexpr std::string_view CREATOR_MONETIZABLE_PROVISIONAL = "CREATOR_MONETIZABLE_PROVISIONAL";
        static constexpr std::string_view CREATOR_STORE_PAGE = "CREATOR_STORE_PAGE";
        static constexpr std::string_view DEVELOPER_SUPPORT_SERVER = "DEVELOPER_SUPPORT_SERVER";
        static constexpr std::string_view DISCOVERABLE = "DISCOVERABLE";
        static constexpr std::string_view FEATURABLE = "FEATURABLE";
        static constexpr std::string_view INVITES_DISABLED = "INVITES_DISABLED";
        static constexpr std::string_view INVITE_SPLASH = "INVITE_SPLASH";
        static constexpr std::string_view MEMBER_VERIFICATION_GATE_ENABLED = "MEMBER_VERIFICATION_GATE_ENABLED";
        static constexpr std::string_view MORE_SOUNDBOARD = "MORE_SOUNDBOARD";
        static constexpr std::string_view MORE_STICKERS = "MORE_STICKERS";
        static constexpr std::string_view NEWS = "NEWS";
        static constexpr std::string_view PARTNERED = "PARTNERED";
        static constexpr std::string_view PREVIEW_ENABLED = "PREVIEW_ENABLED";
        static constexpr std::string_view RAID_ALERTS_DISABLED = "RAID_ALERTS_DISABLED";
        static constexpr std::string_view ROLE_ICONS = "ROLE_ICONS";
        static constexpr std::string_view ROLE_SUBSCRIPTIONS_AVAILABLE_FOR_PURCHASE = "ROLE_SUBSCRIPTIONS_AVAILABLE_FOR_PURCHASE";
        static constexpr std::string_view ROLE_SUBSCRIPTIONS_ENABLED = "ROLE_SUBSCRIPTIONS_ENABLED";
        static constexpr std::string_view SOUNDBOARD = "SOUNDBOARD";
        static constexpr std::string_view TICKETED_EVENTS_ENABLED = "TICKETED_EVENTS_ENABLED";
        static constexpr std::string_view VANITY_URL = "VANITY_URL";
        static constexpr std::string_view VERIFIED = "VERIFIED";
        static constexpr std::string_view VIP_REGIONS = "VIP_REGIONS";
        static constexpr std::string_view WELCOME_SCREEN_ENABLED = "WELCOME_SCREEN_ENABLED";
        static constexpr std::string_view GUESTS_ENABLED = "GUESTS_ENABLED";
        static constexpr std::string_view GUILD_TAGS = "GUILD_TAGS";
        static constexpr std::string_view ENHANCED_ROLE_COLORS = "ENHANCED_ROLE_COLORS";
    };

    struct guild {
        snowflake id{};
        std::string name{};
        opt<discusy::guild_icon_hash> icon{};
        opt<discusy::guild_icon_hash> icon_hash{};
        opt<discusy::guild_splash_hash> splash{};
        opt<discusy::guild_discovery_splash_hash> discovery_splash{};
        opt<bool> owner{};
        snowflake owner_id{};
        opt<permissions_t> permissions{};
        opt<std::string> region{};
        opt<snowflake> afk_channel_id{};
        integer afk_timeout{};
        opt<bool> widget_enabled{};
        opt<snowflake> widget_channel_id{};
        discusy::guild::verification_level verification_level{};
        discusy::guild::default_message_notification_level default_message_notifications{};
        discusy::guild::explicit_content_filter_level explicit_content_filter{};
        std::vector<discusy::permissions::role> roles{};
        std::vector<discusy::emoji::emoji> emojis{};
        std::vector<std::string> features{};
        discusy::guild::mfa_level mfa_level{};
        opt<snowflake> application_id{};
        opt<snowflake> system_channel_id{};
        discusy::flags_t<discusy::guild::system_channel_flags> system_channel_flags{};
        opt<snowflake> rules_channel_id{};
        opt<integer> max_presences{};
        opt<integer> max_members{};
        opt<std::string> vanity_url_code{};
        opt<std::string> description{};
        opt<discusy::guild_banner_hash> banner{};
        discusy::guild::premium_tier premium_tier{};
        opt<integer> premium_subscription_count{};
        std::string preferred_locale{};
        opt<snowflake> public_updates_channel_id{};
        opt<integer> max_video_channel_users{};
        opt<integer> max_stage_video_channel_users{};
        opt<integer> approximate_member_count{};
        opt<integer> approximate_presence_count{};
        opt<discusy::guild::welcome_screen> welcome_screen{};
        discusy::guild::guild_age_restriction_level nsfw_level{};
        opt<std::vector<sticker::sticker>> stickers{};
        bool premium_progress_bar_enabled{};
        opt<snowflake> safety_alerts_channel_id{};
        opt<discusy::guild::incidents_data> incidents_data{};

        static guild create(snowflake id_ = {}, std::string name_ = {}) {
            return guild{.id = id_, .name = std::move(name_)};
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_name(this auto&& self, std::string name_) { self.name = std::move(name_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_icon(this auto&& self, opt<discusy::guild_icon_hash> icon_) { self.icon = std::move(icon_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_icon_hash(this auto&& self, opt<discusy::guild_icon_hash> icon_hash_) { self.icon_hash = std::move(icon_hash_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_splash(this auto&& self, opt<discusy::guild_splash_hash> splash_) { self.splash = std::move(splash_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_discovery_splash(this auto&& self, opt<discusy::guild_discovery_splash_hash> discovery_splash_) { self.discovery_splash = std::move(discovery_splash_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_owner(this auto&& self, opt<bool> owner_) noexcept { self.owner = owner_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_owner_id(this auto&& self, snowflake owner_id_) noexcept { self.owner_id = owner_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_permissions(this auto&& self, opt<permissions_t> permissions_) noexcept { self.permissions = permissions_; return std::forward<decltype(self)>(self); }
        decltype(auto) add_permission(this auto&& self, permissions::permissions p) noexcept {
            if (!self.permissions) self.permissions.emplace();
            self.permissions->add_flags(p);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) add_permissions(this auto&& self, permissions::permissions p) noexcept { return std::forward<decltype(self)>(self).add_permission(p); }
        decltype(auto) set_region(this auto&& self, opt<std::string> region_) { self.region = std::move(region_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_afk_channel_id(this auto&& self, opt<snowflake> afk_channel_id_) noexcept { self.afk_channel_id = afk_channel_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_afk_channel(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_afk_channel_id(id_); }
        decltype(auto) set_afk_timeout(this auto&& self, integer afk_timeout_) noexcept { self.afk_timeout = afk_timeout_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_widget_enabled(this auto&& self, opt<bool> widget_enabled_) noexcept { self.widget_enabled = widget_enabled_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_widget_channel_id(this auto&& self, opt<snowflake> widget_channel_id_) noexcept { self.widget_channel_id = widget_channel_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_widget_channel(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_widget_channel_id(id_); }
        decltype(auto) set_verification_level(this auto&& self, discusy::guild::verification_level verification_level_) noexcept { self.verification_level = verification_level_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_default_message_notifications(this auto&& self, discusy::guild::default_message_notification_level default_message_notifications_) noexcept { self.default_message_notifications = default_message_notifications_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_explicit_content_filter(this auto&& self, discusy::guild::explicit_content_filter_level explicit_content_filter_) noexcept { self.explicit_content_filter = explicit_content_filter_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_roles(this auto&& self, std::vector<discusy::permissions::role> roles_) { self.roles = std::move(roles_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_roles, discusy::permissions::role)
        decltype(auto) add_role(this auto&& self, discusy::permissions::role itm) { self.roles.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_emojis(this auto&& self, std::vector<discusy::emoji::emoji> emojis_) { self.emojis = std::move(emojis_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_emojis, discusy::emoji::emoji)
        decltype(auto) add_emoji(this auto&& self, discusy::emoji::emoji itm) { self.emojis.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_features(this auto&& self, std::vector<std::string> features_) { self.features = std::move(features_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_features, std::string)
        decltype(auto) add_feature(this auto&& self, std::string itm) { self.features.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_mfa_level(this auto&& self, discusy::guild::mfa_level mfa_level_) noexcept { self.mfa_level = mfa_level_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_application_id(this auto&& self, opt<snowflake> application_id_) noexcept { self.application_id = application_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_application(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_application_id(id_); }
        decltype(auto) set_system_channel_id(this auto&& self, opt<snowflake> system_channel_id_) noexcept { self.system_channel_id = system_channel_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_system_channel(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_system_channel_id(id_); }
        decltype(auto) set_system_channel_flags(this auto&& self, discusy::flags_t<discusy::guild::system_channel_flags> system_channel_flags_) noexcept { self.system_channel_flags = system_channel_flags_; return std::forward<decltype(self)>(self); }
        decltype(auto) add_system_channel_flags(this auto&& self, discusy::guild::system_channel_flags f) noexcept { self.system_channel_flags.add_flags(f); return std::forward<decltype(self)>(self); }
        decltype(auto) add_system_channel_flag(this auto&& self, discusy::guild::system_channel_flags f) noexcept { return std::forward<decltype(self)>(self).add_system_channel_flags(f); }
        decltype(auto) set_rules_channel_id(this auto&& self, opt<snowflake> rules_channel_id_) noexcept { self.rules_channel_id = rules_channel_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_rules_channel(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_rules_channel_id(id_); }
        decltype(auto) set_max_presences(this auto&& self, opt<integer> max_presences_) noexcept { self.max_presences = max_presences_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_max_members(this auto&& self, opt<integer> max_members_) noexcept { self.max_members = max_members_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_vanity_url_code(this auto&& self, opt<std::string> vanity_url_code_) { self.vanity_url_code = std::move(vanity_url_code_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_description(this auto&& self, opt<std::string> description_) { self.description = std::move(description_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_banner(this auto&& self, opt<discusy::guild_banner_hash> banner_) { self.banner = std::move(banner_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_premium_tier(this auto&& self, discusy::guild::premium_tier premium_tier_) noexcept { self.premium_tier = premium_tier_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_premium_subscription_count(this auto&& self, opt<integer> premium_subscription_count_) noexcept { self.premium_subscription_count = premium_subscription_count_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_preferred_locale(this auto&& self, std::string preferred_locale_) { self.preferred_locale = std::move(preferred_locale_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_public_updates_channel_id(this auto&& self, opt<snowflake> public_updates_channel_id_) noexcept { self.public_updates_channel_id = public_updates_channel_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_public_updates_channel(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_public_updates_channel_id(id_); }
        decltype(auto) set_max_video_channel_users(this auto&& self, opt<integer> max_video_channel_users_) noexcept { self.max_video_channel_users = max_video_channel_users_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_max_stage_video_channel_users(this auto&& self, opt<integer> max_stage_video_channel_users_) noexcept { self.max_stage_video_channel_users = max_stage_video_channel_users_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_approximate_member_count(this auto&& self, opt<integer> approximate_member_count_) noexcept { self.approximate_member_count = approximate_member_count_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_approximate_presence_count(this auto&& self, opt<integer> approximate_presence_count_) noexcept { self.approximate_presence_count = approximate_presence_count_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_welcome_screen(this auto&& self, opt<discusy::guild::welcome_screen> welcome_screen_) { self.welcome_screen = std::move(welcome_screen_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_nsfw_level(this auto&& self, discusy::guild::guild_age_restriction_level nsfw_level_) noexcept { self.nsfw_level = nsfw_level_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_stickers(this auto&& self, opt<std::vector<sticker::sticker>> stickers_) { self.stickers = std::move(stickers_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_stickers, sticker::sticker)
        decltype(auto) add_sticker(this auto&& self, sticker::sticker itm) {
            if (!self.stickers) self.stickers.emplace();
            self.stickers->emplace_back(std::move(itm));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_premium_progress_bar_enabled(this auto&& self, bool premium_progress_bar_enabled_) noexcept { self.premium_progress_bar_enabled = premium_progress_bar_enabled_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_safety_alerts_channel_id(this auto&& self, opt<snowflake> safety_alerts_channel_id_) noexcept { self.safety_alerts_channel_id = safety_alerts_channel_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_safety_alerts_channel(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_safety_alerts_channel_id(id_); }
        decltype(auto) set_incidents_data(this auto&& self, opt<discusy::guild::incidents_data> incidents_data_) { self.incidents_data = std::move(incidents_data_); return std::forward<decltype(self)>(self); }

        discusy::bot* bot_ptr_{nullptr};
        void set_bot_void(this auto&& self, discusy::bot* b) noexcept { self.bot_ptr_ = b; }
        decltype(auto) set_bot(this auto&& self, discusy::bot* b) noexcept { self.set_bot_void(b); return std::forward<decltype(self)>(self); }

        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto fetch_member(this auto&& self, snowflake user_id, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto kick(this auto&& self, snowflake user_id, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto ban(this auto&& self, snowflake user_id, integer delete_message_days = 0, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto unban(this auto&& self, snowflake user_id, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto add_member_role(this auto&& self, snowflake user_id, snowflake role_id, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto remove_member_role(this auto&& self, snowflake user_id, snowflake role_id, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto create_channel(this auto&& self, std::string name, channel::channel_type type, CompletionToken&& token = ctx::io_context::dct_t());
    };

    struct guild_preview {
        snowflake id{};
        std::string name{};
        opt<guild_icon_hash> icon{};
        opt<guild_splash_hash> splash{};
        opt<guild_discovery_splash_hash> discovery_splash{};
        std::vector<emoji::emoji> emojis{};
        std::vector<std::string> features{};
        integer approximate_member_count{};
        integer approximate_presence_count{};
        opt<std::string> description{};
        std::vector<sticker::sticker> stickers{};

        static guild_preview create(snowflake id_ = {}, std::string name_ = {}) {
            return guild_preview{.id = id_, .name = std::move(name_)};
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_name(this auto&& self, std::string name_) { self.name = std::move(name_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_icon(this auto&& self, opt<guild_icon_hash> icon_) { self.icon = std::move(icon_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_splash(this auto&& self, opt<guild_splash_hash> splash_) { self.splash = std::move(splash_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_discovery_splash(this auto&& self, opt<guild_discovery_splash_hash> discovery_splash_) { self.discovery_splash = std::move(discovery_splash_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_emojis(this auto&& self, std::vector<emoji::emoji> emojis_) { self.emojis = std::move(emojis_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_emojis, emoji::emoji)
        decltype(auto) add_emoji(this auto&& self, emoji::emoji itm) { self.emojis.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_features(this auto&& self, std::vector<std::string> features_) { self.features = std::move(features_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_features, std::string)
        decltype(auto) add_feature(this auto&& self, std::string itm) { self.features.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_approximate_member_count(this auto&& self, integer approximate_member_count_) noexcept { self.approximate_member_count = approximate_member_count_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_approximate_presence_count(this auto&& self, integer approximate_presence_count_) noexcept { self.approximate_presence_count = approximate_presence_count_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_description(this auto&& self, opt<std::string> description_) { self.description = std::move(description_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_stickers(this auto&& self, std::vector<sticker::sticker> stickers_) { self.stickers = std::move(stickers_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_stickers, sticker::sticker)
        decltype(auto) add_sticker(this auto&& self, sticker::sticker itm) { self.stickers.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
    };
}

namespace user {
    struct connection {
        std::string id{};
        std::string name{};
        std::string type{};
        opt<bool> revoked{};
        opt<std::vector<guild::integration>> integrations{};
        bool verified{};
        bool friend_sync{};
        bool show_activity{};
        bool two_way_link{};
        connection_visibility visibility{};

        static connection create(std::string id_ = {}, std::string name_ = {}, std::string type_ = {}) {
            return connection{
                .id = std::move(id_),
                .name = std::move(name_),
                .type = std::move(type_),
            };
        }
        decltype(auto) set_id(this auto&& self, std::string id_) { self.id = std::move(id_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_name(this auto&& self, std::string name_) { self.name = std::move(name_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_type(this auto&& self, std::string type_) { self.type = std::move(type_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_revoked(this auto&& self, opt<bool> revoked_) noexcept { self.revoked = revoked_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_integrations(this auto&& self, opt<std::vector<guild::integration>> integrations_) { self.integrations = std::move(integrations_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_integrations, guild::integration)
        decltype(auto) add_integration(this auto&& self, guild::integration integration) {
            if (!self.integrations) self.integrations.emplace();
            self.integrations->emplace_back(std::move(integration));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_verified(this auto&& self, bool verified_) noexcept { self.verified = verified_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_friend_sync(this auto&& self, bool friend_sync_) noexcept { self.friend_sync = friend_sync_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_show_activity(this auto&& self, bool show_activity_) noexcept { self.show_activity = show_activity_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_two_way_link(this auto&& self, bool two_way_link_) noexcept { self.two_way_link = two_way_link_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_visibility(this auto&& self, connection_visibility visibility_) noexcept { self.visibility = visibility_; return std::forward<decltype(self)>(self); }
    };
}

namespace channel {

    enum class overwrite_type : std::uint8_t {
        role = 0,
        member = 1,
    };

    struct overwrite {
        snowflake id{};
        overwrite_type type{};
        std::string allow{};
        std::string deny{};

        static overwrite create(snowflake id_ = {}, overwrite_type type_ = overwrite_type::role, std::string allow_ = {}, std::string deny_ = {}) {
            return overwrite{
                .id = id_,
                .type = type_,
                .allow = std::move(allow_),
                .deny = std::move(deny_),
            };
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_type(this auto&& self, overwrite_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_allow(this auto&& self, std::string allow_) { self.allow = std::move(allow_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_deny(this auto&& self, std::string deny_) { self.deny = std::move(deny_); return std::forward<decltype(self)>(self); }
    };

    struct thread_metadata {
        bool archived{};
        integer auto_archive_duration{};
        timestamp archive_timestamp{};
        bool locked{};
        opt<bool> invitable{};
        opt<timestamp> create_timestamp{};

        static thread_metadata create(bool archived_ = false, integer auto_archive_duration_ = 0, timestamp archive_timestamp_ = {}, bool locked_ = false) noexcept {
            return thread_metadata{
                .archived = archived_,
                .auto_archive_duration = auto_archive_duration_,
                .archive_timestamp = archive_timestamp_,
                .locked = locked_,
            };
        }
        decltype(auto) set_archived(this auto&& self, bool archived_) noexcept { self.archived = archived_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_auto_archive_duration(this auto&& self, integer auto_archive_duration_) noexcept { self.auto_archive_duration = auto_archive_duration_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_archive_timestamp(this auto&& self, timestamp archive_timestamp_) noexcept { self.archive_timestamp = archive_timestamp_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_locked(this auto&& self, bool locked_) noexcept { self.locked = locked_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_invitable(this auto&& self, opt<bool> invitable_) noexcept { self.invitable = invitable_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_create_timestamp(this auto&& self, opt<timestamp> create_timestamp_) noexcept { self.create_timestamp = create_timestamp_; return std::forward<decltype(self)>(self); }
    };

    struct thread_member {
        opt<snowflake> id{};
        opt<snowflake> user_id{};
        timestamp join_timestamp{};
        integer flags{};
        opt<guild::guild_member> member{};

        static thread_member create(opt<snowflake> id_ = {}, opt<snowflake> user_id_ = {}, timestamp join_timestamp_ = {}, integer flags_ = 0) noexcept {
            return thread_member{
                .id = id_,
                .user_id = user_id_,
                .join_timestamp = join_timestamp_,
                .flags = flags_,
            };
        }
        decltype(auto) set_id(this auto&& self, opt<snowflake> id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_user_id(this auto&& self, opt<snowflake> user_id_) noexcept { self.user_id = user_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_user(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_user_id(id_); }
        decltype(auto) set_join_timestamp(this auto&& self, timestamp join_timestamp_) noexcept { self.join_timestamp = join_timestamp_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_flags(this auto&& self, integer flags_) noexcept { self.flags = flags_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_member(this auto&& self, opt<guild::guild_member> member_) { self.member = std::move(member_); return std::forward<decltype(self)>(self); }
    };

    struct thread_member_with_guild {
        discusy::channel::thread_member thread_member{};

        static thread_member_with_guild create(discusy::channel::thread_member tm = {}, snowflake gid = {}) {
            return thread_member_with_guild{.thread_member = std::move(tm), .other = {.guild_id = gid}};
        }
        decltype(auto) set_thread_member(this auto&& self, discusy::channel::thread_member tm) { self.thread_member = std::move(tm); return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild_id(this auto&& self, snowflake gid) noexcept { self.other.guild_id = gid; return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild(this auto&& self, snowflake gid) noexcept { return std::forward<decltype(self)>(self).set_guild_id(gid); }

        struct Other {
            snowflake guild_id{};
        } other;

        constexpr auto* operator->(this auto&& self) noexcept { return std::addressof(self.thread_member); }
        constexpr decltype(auto) operator*(this auto&& self) noexcept { return (std::forward<decltype(self)>(self).thread_member); }
        constexpr decltype(auto) operator()(this auto&& self) noexcept { return (std::forward<decltype(self)>(self).other); }

        struct glaze {
            using T = thread_member_with_guild;
            static constexpr auto value = glz::merge{
                &T::thread_member,
                &T::other,
            };
        };
    };

    enum class channel_type : std::uint8_t {
        GUILD_TEXT = 0, // a text channel within a server
        DM = 1, // a direct message between users
        GUILD_VOICE = 2, // a voice channel within a server
        GROUP_DM = 3, // a direct message between multiple users
        GUILD_CATEGORY = 4, // an organizational category that contains up to 50 channels
        GUILD_ANNOUNCEMENT = 5, // a channel that users can follow and crosspost into their own server (formerly news channels)
        ANNOUNCEMENT_THREAD = 10, // a temporary sub-channel within a GUILD_ANNOUNCEMENT channel
        PUBLIC_THREAD = 11, // a temporary sub-channel within a GUILD_TEXT or GUILD_FORUM channel
        PRIVATE_THREAD = 12, // a temporary sub-channel within a GUILD_TEXT channel that is only viewable by those invited and those with the MANAGE_THREADS permission
        GUILD_STAGE_VOICE = 13, // a voice channel for hosting events with an audience
        GUILD_DIRECTORY = 14, // the channel in a hub containing the listed servers
        GUILD_FORUM = 15, // Channel that can only contain threads
        GUILD_MEDIA = 16, // Channel that can only contain threads, similar to GUILD_FORUM channels
    };

    enum class channel_flags : std::uint32_t {
        PINNED = 1ULL << 1, // this thread is pinned to the top of its parent GUILD_FORUM or GUILD_MEDIA channel
        REQUIRE_TAG = 1ULL << 4, // whether a tag is required to be specified when creating a thread in a GUILD_FORUM or a GUILD_MEDIA channel. Tags are specified in the applied_tags field.
        HIDE_MEDIA_DOWNLOAD_OPTIONS = 1ULL << 15, // when set hides the embedded media download options. Available only for media channels
        CHANNEL_OBFUSCATED = 1ULL << 17, // this channel’s metadata has been obfuscated because the current user cannot view it. Only ever set on channels received over the Gateway; the HTTP API never sets this flag. See Obfuscated Channels.
        IS_SPOILER_CHANNEL = 1ULL << 21, // this channel is a Spoiler Channel i.e. users must opt in to view its contents. Can be set on all textual guild channels and voice channels (not GUILD_STAGE). Can only be set if channel's nsfw is false
    };

    struct forum_tag {
        snowflake id{};
        std::string name{};
        bool moderated{};
        opt<snowflake> emoji_id{};
        opt<std::string> emoji_name{};

        static forum_tag create(snowflake id_ = {}, std::string name_ = {}, bool moderated_ = false) {
            return forum_tag{.id = id_, .name = std::move(name_), .moderated = moderated_};
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_name(this auto&& self, std::string name_) { self.name = std::move(name_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_moderated(this auto&& self, bool moderated_) noexcept { self.moderated = moderated_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_emoji_id(this auto&& self, opt<snowflake> emoji_id_) noexcept { self.emoji_id = emoji_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_emoji(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_emoji_id(id_); }
        decltype(auto) set_emoji_name(this auto&& self, opt<std::string> emoji_name_) { self.emoji_name = std::move(emoji_name_); return std::forward<decltype(self)>(self); }
    };

    struct default_reaction {
        opt<snowflake> emoji_id{};
        opt<std::string> emoji_name{};

        static default_reaction create(opt<snowflake> emoji_id_ = {}, opt<std::string> emoji_name_ = {}) {
            return default_reaction{
                .emoji_id = emoji_id_,
                .emoji_name = std::move(emoji_name_),
            };
        }
        decltype(auto) set_emoji_id(this auto&& self, opt<snowflake> emoji_id_) noexcept { self.emoji_id = emoji_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_emoji(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_emoji_id(id_); }
        decltype(auto) set_emoji_name(this auto&& self, opt<std::string> emoji_name_) { self.emoji_name = std::move(emoji_name_); return std::forward<decltype(self)>(self); }
    };

    enum class sort_order_type : std::uint8_t {
        LATEST_ACTIVITY = 0, // Sort forum posts by activity
        CREATION_DATE = 1, // Sort forum posts by creation time (from most recent to oldest)
    };

    enum class forum_layout_type : std::uint8_t {
        NOT_SET = 0, // No default has been set for forum channel
        LIST_VIEW = 1, // Display posts as a list
        GALLERY_VIEW = 2, // Display posts as a collection of tiles
    };

    enum class video_quality_mode : std::uint8_t {
        AUTO = 1, // Discord chooses the quality for optimal performance
        FULL = 2, // 720p
    };

    struct channel {
        snowflake id{};
        channel_type type{};
        opt<snowflake> guild_id{};
        opt<integer> position{};
        opt<std::vector<overwrite>> permission_overwrites{};
        opt<std::string> name{};
        opt<std::string> topic{};
        opt<bool> nsfw{};
        opt<snowflake> last_message_id{};
        opt<integer> bitrate{};
        opt<integer> user_limit{};
        opt<integer> rate_limit_per_user{};
        opt<std::vector<user::user>> recipients{};
        opt<std::string> icon{};
        opt<snowflake> owner_id{};
        opt<snowflake> application_id{};
        opt<bool> managed{};
        opt<snowflake> parent_id{};
        opt<timestamp> last_pin_timestamp{};
        opt<std::string> rtc_region{};
        opt<discusy::channel::video_quality_mode> video_quality_mode{};
        opt<integer> message_count{};
        opt<integer> member_count{};
        opt<discusy::channel::thread_metadata> thread_metadata{};
        opt<thread_member> member{};
        opt<integer> default_auto_archive_duration{};
        opt<permissions_t> permissions{};
        opt<flags_t<channel_flags>> flags{};
        opt<integer> total_message_sent{};
        opt<std::vector<forum_tag>> available_tags{};
        opt<std::vector<snowflake>> applied_tags{};
        opt<default_reaction> default_reaction_emoji{};
        opt<integer> default_thread_rate_limit_per_user{};
        opt<sort_order_type> default_sort_order{};
        opt<forum_layout_type> default_forum_layout{};

        static channel create(snowflake id_ = {}, channel_type type_ = channel_type::GUILD_TEXT) noexcept {
            return channel{.id = id_, .type = type_};
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_type(this auto&& self, channel_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild_id(this auto&& self, opt<snowflake> guild_id_) noexcept { self.guild_id = guild_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_guild_id(id_); }
        decltype(auto) set_position(this auto&& self, opt<integer> position_) noexcept { self.position = position_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_permission_overwrites(this auto&& self, opt<std::vector<overwrite>> permission_overwrites_) { self.permission_overwrites = std::move(permission_overwrites_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_permission_overwrites, overwrite)
        decltype(auto) add_permission_overwrite(this auto&& self, overwrite itm) {
            if (!self.permission_overwrites) self.permission_overwrites.emplace();
            self.permission_overwrites->emplace_back(std::move(itm));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_name(this auto&& self, opt<std::string> name_) { self.name = std::move(name_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_topic(this auto&& self, opt<std::string> topic_) { self.topic = std::move(topic_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_nsfw(this auto&& self, opt<bool> nsfw_) noexcept { self.nsfw = nsfw_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_last_message_id(this auto&& self, opt<snowflake> last_message_id_) noexcept { self.last_message_id = last_message_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_last_message(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_last_message_id(id_); }
        decltype(auto) set_bitrate(this auto&& self, opt<integer> bitrate_) noexcept { self.bitrate = bitrate_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_user_limit(this auto&& self, opt<integer> user_limit_) noexcept { self.user_limit = user_limit_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_rate_limit_per_user(this auto&& self, opt<integer> rate_limit_per_user_) noexcept { self.rate_limit_per_user = rate_limit_per_user_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_recipients(this auto&& self, opt<std::vector<user::user>> recipients_) { self.recipients = std::move(recipients_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_recipients, user::user)
        decltype(auto) add_recipient(this auto&& self, user::user itm) {
            if (!self.recipients) self.recipients.emplace();
            self.recipients->emplace_back(std::move(itm));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_icon(this auto&& self, opt<std::string> icon_) { self.icon = std::move(icon_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_owner_id(this auto&& self, opt<snowflake> owner_id_) noexcept { self.owner_id = owner_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_owner(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_owner_id(id_); }
        decltype(auto) set_application_id(this auto&& self, opt<snowflake> application_id_) noexcept { self.application_id = application_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_application(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_application_id(id_); }
        decltype(auto) set_managed(this auto&& self, opt<bool> managed_) noexcept { self.managed = managed_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_parent_id(this auto&& self, opt<snowflake> parent_id_) noexcept { self.parent_id = parent_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_parent(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_parent_id(id_); }
        decltype(auto) set_last_pin_timestamp(this auto&& self, opt<timestamp> last_pin_timestamp_) noexcept { self.last_pin_timestamp = last_pin_timestamp_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_rtc_region(this auto&& self, opt<std::string> rtc_region_) { self.rtc_region = std::move(rtc_region_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_video_quality_mode(this auto&& self, opt<discusy::channel::video_quality_mode> video_quality_mode_) noexcept { self.video_quality_mode = video_quality_mode_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_message_count(this auto&& self, opt<integer> message_count_) noexcept { self.message_count = message_count_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_member_count(this auto&& self, opt<integer> member_count_) noexcept { self.member_count = member_count_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_thread_metadata(this auto&& self, opt<discusy::channel::thread_metadata> thread_metadata_) { self.thread_metadata = std::move(thread_metadata_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_member(this auto&& self, opt<thread_member> member_) { self.member = std::move(member_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_default_auto_archive_duration(this auto&& self, opt<integer> default_auto_archive_duration_) noexcept { self.default_auto_archive_duration = default_auto_archive_duration_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_permissions(this auto&& self, opt<permissions_t> permissions_) noexcept { self.permissions = permissions_; return std::forward<decltype(self)>(self); }
        decltype(auto) add_permission(this auto&& self, permissions::permissions p) noexcept {
            if (!self.permissions) self.permissions.emplace();
            self.permissions->add_flags(p);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) add_permissions(this auto&& self, permissions::permissions p) noexcept { return std::forward<decltype(self)>(self).add_permission(p); }
        decltype(auto) set_flags(this auto&& self, opt<flags_t<channel_flags>> flags_) noexcept { self.flags = flags_; return std::forward<decltype(self)>(self); }
        decltype(auto) add_flags(this auto&& self, channel_flags f) noexcept {
            if (!self.flags) self.flags.emplace();
            self.flags->add_flags(f);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) add_flag(this auto&& self, channel_flags f) noexcept { return std::forward<decltype(self)>(self).add_flags(f); }
        decltype(auto) set_total_message_sent(this auto&& self, opt<integer> total_message_sent_) noexcept { self.total_message_sent = total_message_sent_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_available_tags(this auto&& self, opt<std::vector<forum_tag>> available_tags_) { self.available_tags = std::move(available_tags_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_available_tags, forum_tag)
        decltype(auto) add_available_tag(this auto&& self, forum_tag itm) {
            if (!self.available_tags) self.available_tags.emplace();
            self.available_tags->emplace_back(std::move(itm));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_applied_tags(this auto&& self, opt<std::vector<snowflake>> applied_tags_) { self.applied_tags = std::move(applied_tags_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_applied_tags, snowflake)
        decltype(auto) add_applied_tag(this auto&& self, snowflake itm) {
            if (!self.applied_tags) self.applied_tags.emplace();
            self.applied_tags->emplace_back(itm);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_default_reaction_emoji(this auto&& self, opt<default_reaction> default_reaction_emoji_) noexcept { self.default_reaction_emoji = default_reaction_emoji_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_default_thread_rate_limit_per_user(this auto&& self, opt<integer> default_thread_rate_limit_per_user_) noexcept { self.default_thread_rate_limit_per_user = default_thread_rate_limit_per_user_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_default_sort_order(this auto&& self, opt<sort_order_type> default_sort_order_) noexcept { self.default_sort_order = default_sort_order_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_default_forum_layout(this auto&& self, opt<forum_layout_type> default_forum_layout_) noexcept { self.default_forum_layout = default_forum_layout_; return std::forward<decltype(self)>(self); }
        decltype(auto) add_tag(this auto&& self, forum_tag t) { return std::forward<decltype(self)>(self).add_available_tag(std::move(t)); }
        decltype(auto) add_tag(this auto&& self, snowflake t) { return std::forward<decltype(self)>(self).add_applied_tag(t); }

        discusy::bot* bot_ptr_{nullptr};
        void set_bot_void(this auto&& self, discusy::bot* b) noexcept { self.bot_ptr_ = b; }
        decltype(auto) set_bot(this auto&& self, discusy::bot* b) noexcept { self.set_bot_void(b); return std::forward<decltype(self)>(self); }

        [[nodiscard]] std::string mention(this const auto& self) {
            return self.id.mention_channel();
        }

        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto send(this auto&& self, std::string content, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto send_embed(this auto&& self, message::embed e, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto send_file(this auto&& self, discusy::upload_file_view f, std::string content = {}, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto send_files(this auto&& self, discusy::upload_files_param files, std::string content = {}, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto send_with(this auto&& self, api::message::create_message msg, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto delete_channel(this auto&& self, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto create_thread(this auto&& self, std::string name, CompletionToken&& token = ctx::io_context::dct_t());
    };

    struct followed_channel {
        snowflake channel_id{};
        snowflake webhook_id{};

        static followed_channel create(snowflake channel_id_ = {}, snowflake webhook_id_ = {}) noexcept {
            return followed_channel{.channel_id = channel_id_, .webhook_id = webhook_id_};
        }
        decltype(auto) set_channel_id(this auto&& self, snowflake channel_id_) noexcept { self.channel_id = channel_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_channel(this auto&& self, snowflake id_) noexcept { return std::forward<decltype(self)>(self).set_channel_id(id_); }
        decltype(auto) set_webhook_id(this auto&& self, snowflake webhook_id_) noexcept { self.webhook_id = webhook_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_webhook(this auto&& self, snowflake id_) noexcept { return std::forward<decltype(self)>(self).set_webhook_id(id_); }
    };

    struct channel_mention {
        snowflake id{};
        snowflake guild_id{};
        channel_type type{};
        std::string name{};

        static channel_mention create(snowflake id_ = {}, snowflake guild_id_ = {}, channel_type type_ = channel_type::GUILD_TEXT, std::string name_ = {}) {
            return channel_mention{
                .id = id_,
                .guild_id = guild_id_,
                .type = type_,
                .name = std::move(name_),
            };
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild_id(this auto&& self, snowflake guild_id_) noexcept { self.guild_id = guild_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild(this auto&& self, snowflake id_) noexcept { return std::forward<decltype(self)>(self).set_guild_id(id_); }
        decltype(auto) set_type(this auto&& self, channel_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_name(this auto&& self, std::string name_) { self.name = std::move(name_); return std::forward<decltype(self)>(self); }
    };
}

namespace guild {
    struct guild_widget_settings {
        bool enabled{};
        opt<snowflake> channel_id{};

        static guild_widget_settings create(bool enabled_ = false, opt<snowflake> channel_id_ = {}) noexcept {
            return guild_widget_settings{.enabled = enabled_, .channel_id = channel_id_};
        }
        decltype(auto) set_enabled(this auto&& self, bool enabled_) noexcept { self.enabled = enabled_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_channel_id(this auto&& self, opt<snowflake> channel_id_) noexcept { self.channel_id = channel_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_channel(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_channel_id(id_); }
    };

    struct guild_widget {
        snowflake id{};
        std::string name{};
        opt<std::string> instant_invite{};
        std::vector<channel::channel> channels{}; // partial, double check
        std::vector<user::user> members{}; // partial, double check
        integer presence_count{};

        static guild_widget create(snowflake id_ = {}, std::string name_ = {}) {
            return guild_widget{.id = id_, .name = std::move(name_)};
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_name(this auto&& self, std::string name_) { self.name = std::move(name_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_instant_invite(this auto&& self, opt<std::string> instant_invite_) { self.instant_invite = std::move(instant_invite_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_channels(this auto&& self, std::vector<channel::channel> channels_) { self.channels = std::move(channels_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_channels, channel::channel)
        decltype(auto) add_channel(this auto&& self, channel::channel itm) { self.channels.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_members(this auto&& self, std::vector<user::user> members_) { self.members = std::move(members_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_members, user::user)
        decltype(auto) add_member(this auto&& self, user::user itm) { self.members.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_presence_count(this auto&& self, integer presence_count_) noexcept { self.presence_count = presence_count_; return std::forward<decltype(self)>(self); }
    };
}

namespace team {
    enum class membership_state : std::uint8_t {
        INVITED = 1,
        ACCEPTED = 2,
    };

    enum class team_member_role : std::uint8_t { // make sure to update glz meta
        owner, // not actually represented
        admin,
        developer,
        read_only,
    };

    struct team_member {
        discusy::team::membership_state membership_state{};
        snowflake team_id{};
        opt<user::user> user{}; // partial, double check
        team_member_role role{};

        static team_member create(discusy::team::membership_state membership_state_ = discusy::team::membership_state::INVITED, snowflake team_id_ = {}, team_member_role role_ = team_member_role::admin) noexcept {
            return team_member{
                .membership_state = membership_state_,
                .team_id = team_id_,
                .role = role_,
            };
        }
        decltype(auto) set_membership_state(this auto&& self, discusy::team::membership_state membership_state_) noexcept { self.membership_state = membership_state_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_team_id(this auto&& self, snowflake team_id_) noexcept { self.team_id = team_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_team(this auto&& self, snowflake id_) noexcept { return std::forward<decltype(self)>(self).set_team_id(id_); }
        decltype(auto) set_user(this auto&& self, opt<user::user> user_) { self.user = std::move(user_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_role(this auto&& self, team_member_role role_) noexcept { self.role = role_; return std::forward<decltype(self)>(self); }
    };


    struct team {
        opt<team_icon_hash> icon{};
        snowflake id{};
        std::vector<team_member> members{};
        std::string name{};
        snowflake owner_user_id{};

        static team create(snowflake id_ = {}, std::string name_ = {}, snowflake owner_user_id_ = {}) {
            return team{
                .id = id_,
                .name = std::move(name_),
                .owner_user_id = owner_user_id_,
            };
        }
        decltype(auto) set_icon(this auto&& self, opt<team_icon_hash> icon_) { self.icon = std::move(icon_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_members(this auto&& self, std::vector<team_member> members_) { self.members = std::move(members_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_members, team_member)
        decltype(auto) add_member(this auto&& self, team_member itm) { self.members.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_name(this auto&& self, std::string name_) { self.name = std::move(name_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_owner_user_id(this auto&& self, snowflake owner_user_id_) noexcept { self.owner_user_id = owner_user_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_owner_user(this auto&& self, snowflake id_) noexcept { return std::forward<decltype(self)>(self).set_owner_user_id(id_); }
    };
};

namespace application {
    struct install_params {
        std::vector<std::string> scopes{}; // could be enum
        permissions_t permissions{};

        static install_params create(std::vector<std::string> scopes_ = {}, permissions_t permissions_ = {}) {
            return install_params{
                .scopes = std::move(scopes_),
                .permissions = permissions_,
            };
        }
        decltype(auto) set_scopes(this auto&& self, std::vector<std::string> scopes_) { self.scopes = std::move(scopes_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_scopes, std::string)
        decltype(auto) add_scope(this auto&& self, std::string itm) { self.scopes.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_permissions(this auto&& self, permissions_t permissions_) noexcept { self.permissions = permissions_; return std::forward<decltype(self)>(self); }
        decltype(auto) add_permission(this auto&& self, permissions::permissions p) noexcept { self.permissions.add_flags(p); return std::forward<decltype(self)>(self); }
        decltype(auto) add_permissions(this auto&& self, permissions::permissions p) noexcept { return std::forward<decltype(self)>(self).add_permission(p); }
    };

    struct integration_type_config {
        opt<install_params> oauth2_install_params{};

        static integration_type_config create(opt<install_params> oauth2_install_params_ = {}) {
            return integration_type_config{
                .oauth2_install_params = std::move(oauth2_install_params_),
            };
        }
        decltype(auto) set_oauth2_install_params(this auto&& self, opt<install_params> oauth2_install_params_) { self.oauth2_install_params = std::move(oauth2_install_params_); return std::forward<decltype(self)>(self); }
    };

    enum class application_flags : std::uint32_t {
        APPLICATION_AUTO_MODERATION_RULE_CREATE_BADGE = 1ULL << 6,
        GATEWAY_PRESENCE = 1ULL << 12,
        GATEWAY_PRESENCE_LIMITED = 1ULL << 13,
        GATEWAY_GUILD_MEMBERS = 1ULL << 14,
        GATEWAY_GUILD_MEMBERS_LIMITED = 1ULL << 15,
        VERIFICATION_PENDING_GUILD_LIMIT = 1ULL << 16,
        EMBEDDED = 1ULL << 17,
        GATEWAY_MESSAGE_CONTENT = 1ULL << 18,
        GATEWAY_MESSAGE_CONTENT_LIMITED = 1ULL << 19,
        APPLICATION_COMMAND_BADGE = 1ULL << 23,
    };

    enum class application_event_webhook_status : std::uint8_t {
        DISABLED = 1, // Webhook events are disabled by developer
        ENABLED = 2, // Webhook events are enabled by developer
        DISABLED_BY_DISCORD = 3, // Webhook events are disabled by Discord, usually due to inactivity
    };

    enum class application_integration_type : std::uint8_t {
        GUILD_INSTALL = 0, // App is installable to servers
        USER_INSTALL = 1 // App is installable to users
    };

    struct application {
        snowflake id{};
        std::string name{};
        opt<application_icon_hash> icon{};
        std::string description{};
        opt<std::vector<std::string>> rpc_origins{};
        bool bot_public{};
        bool bot_require_code_grant{};
        opt<user::user> bot{}; // partial, double check
        opt<std::string> terms_of_service_url{};
        opt<std::string> privacy_policy_url{};
        opt<user::user> owner{}; // partial, double check
        std::string verify_key{};
        opt<team::team> team{};
        opt<snowflake> guild_id{};
        opt<guild::guild> guild{}; // partial, double check
        opt<snowflake> primary_sku_id{};
        opt<std::string> slug{};
        opt<application_cover_hash> cover_image{};
        opt<flags_t<application_flags>> flags{};
        opt<flags_t<application_flags, flags_type::string>> flags_new{};
        opt<integer> approximate_guild_count{};
        opt<integer> approximate_user_install_count{};
        opt<integer> approximate_user_authorization_count{};
        opt<std::vector<std::string>> redirect_uris{};
        opt<std::string> interactions_endpoint_url{};
        opt<std::string> role_connections_verification_url{};
        opt<std::string> event_webhooks_url{};
        opt<application_event_webhook_status> event_webhooks_status{};
        opt<std::vector<std::string>> event_webhooks_types{}; // could be enum
        opt<std::vector<std::string>> tags{};
        opt<discusy::application::install_params> install_params{};
        opt<std::unordered_map<application_integration_type, integration_type_config>> integration_types_config{};
        opt<std::string> custom_install_url{};

        static application create(snowflake id_ = {}, std::string name_ = {}, std::string description_ = {}) {
            return application{
                .id = id_,
                .name = std::move(name_),
                .description = std::move(description_),
            };
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_name(this auto&& self, std::string name_) { self.name = std::move(name_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_icon(this auto&& self, opt<application_icon_hash> icon_) { self.icon = std::move(icon_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_description(this auto&& self, std::string description_) { self.description = std::move(description_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_rpc_origins(this auto&& self, opt<std::vector<std::string>> rpc_origins_) { self.rpc_origins = std::move(rpc_origins_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_rpc_origins, std::string)
        decltype(auto) add_rpc_origin(this auto&& self, std::string itm) {
            if (!self.rpc_origins) self.rpc_origins.emplace();
            self.rpc_origins->emplace_back(std::move(itm));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_bot_public(this auto&& self, bool bot_public_) noexcept { self.bot_public = bot_public_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_bot_require_code_grant(this auto&& self, bool bot_require_code_grant_) noexcept { self.bot_require_code_grant = bot_require_code_grant_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_bot(this auto&& self, opt<user::user> bot_) { self.bot = std::move(bot_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_terms_of_service_url(this auto&& self, opt<std::string> terms_of_service_url_) { self.terms_of_service_url = std::move(terms_of_service_url_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_privacy_policy_url(this auto&& self, opt<std::string> privacy_policy_url_) { self.privacy_policy_url = std::move(privacy_policy_url_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_owner(this auto&& self, opt<user::user> owner_) { self.owner = std::move(owner_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_verify_key(this auto&& self, std::string verify_key_) { self.verify_key = std::move(verify_key_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_team(this auto&& self, opt<team::team> team_) { self.team = std::move(team_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild_id(this auto&& self, opt<snowflake> guild_id_) noexcept { self.guild_id = guild_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild(this auto&& self, opt<guild::guild> guild_) { self.guild = std::move(guild_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_primary_sku_id(this auto&& self, opt<snowflake> primary_sku_id_) noexcept { self.primary_sku_id = primary_sku_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_primary_sku(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_primary_sku_id(id_); }
        decltype(auto) set_slug(this auto&& self, opt<std::string> slug_) { self.slug = std::move(slug_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_cover_image(this auto&& self, opt<application_cover_hash> cover_image_) { self.cover_image = std::move(cover_image_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_flags(this auto&& self, opt<flags_t<application_flags>> flags_) noexcept { self.flags = flags_; return std::forward<decltype(self)>(self); }
        decltype(auto) add_flags(this auto&& self, application_flags f) noexcept {
            if (!self.flags) self.flags.emplace();
            self.flags->add_flags(f);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) add_flag(this auto&& self, application_flags f) noexcept { return std::forward<decltype(self)>(self).add_flags(f); }
        decltype(auto) set_approximate_guild_count(this auto&& self, opt<integer> approximate_guild_count_) noexcept { self.approximate_guild_count = approximate_guild_count_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_approximate_user_install_count(this auto&& self, opt<integer> approximate_user_install_count_) noexcept { self.approximate_user_install_count = approximate_user_install_count_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_approximate_user_authorization_count(this auto&& self, opt<integer> approximate_user_authorization_count_) noexcept { self.approximate_user_authorization_count = approximate_user_authorization_count_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_redirect_uris(this auto&& self, opt<std::vector<std::string>> redirect_uris_) { self.redirect_uris = std::move(redirect_uris_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_redirect_uris, std::string)
        decltype(auto) add_redirect_uri(this auto&& self, std::string itm) {
            if (!self.redirect_uris) self.redirect_uris.emplace();
            self.redirect_uris->emplace_back(std::move(itm));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_interactions_endpoint_url(this auto&& self, opt<std::string> interactions_endpoint_url_) { self.interactions_endpoint_url = std::move(interactions_endpoint_url_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_role_connections_verification_url(this auto&& self, opt<std::string> role_connections_verification_url_) { self.role_connections_verification_url = std::move(role_connections_verification_url_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_event_webhooks_url(this auto&& self, opt<std::string> event_webhooks_url_) { self.event_webhooks_url = std::move(event_webhooks_url_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_event_webhooks_status(this auto&& self, opt<application_event_webhook_status> event_webhooks_status_) noexcept { self.event_webhooks_status = event_webhooks_status_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_event_webhooks_types(this auto&& self, opt<std::vector<std::string>> event_webhooks_types_) { self.event_webhooks_types = std::move(event_webhooks_types_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_event_webhooks_types, std::string)
        decltype(auto) add_event_webhooks_type(this auto&& self, std::string itm) {
            if (!self.event_webhooks_types) self.event_webhooks_types.emplace();
            self.event_webhooks_types->emplace_back(std::move(itm));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_tags(this auto&& self, opt<std::vector<std::string>> tags_) { self.tags = std::move(tags_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_tags, std::string)
        decltype(auto) add_tag(this auto&& self, std::string itm) {
            if (!self.tags) self.tags.emplace();
            self.tags->emplace_back(std::move(itm));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_install_params(this auto&& self, opt<discusy::application::install_params> install_params_) { self.install_params = std::move(install_params_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_integration_types_config(this auto&& self, opt<std::unordered_map<application_integration_type, integration_type_config>> integration_types_config_) { self.integration_types_config = std::move(integration_types_config_); return std::forward<decltype(self)>(self); }
        decltype(auto) add_integration_types_config(this auto&& self, application_integration_type k, integration_type_config v) {
            if (!self.integration_types_config) self.integration_types_config.emplace();
            (*self.integration_types_config)[k] = std::move(v);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_custom_install_url(this auto&& self, opt<std::string> custom_install_url_) { self.custom_install_url = std::move(custom_install_url_); return std::forward<decltype(self)>(self); }
        decltype(auto) add_event_webhook_type(this auto&& self, std::string t) { return std::forward<decltype(self)>(self).add_event_webhooks_type(std::move(t)); }
    };
}

namespace auto_moderation {

    // Auto Moderation Resource Structures
    // Auto Moderation action metadata
    struct auto_moderation_action_metadata {
        // Channel to which user content should be logged (SEND_ALERT_MESSAGE)
        snowflake channel_id{};
        // Timeout duration in seconds (TIMEOUT)
        integer duration_seconds{};
        // Additional explanation shown when message is blocked (BLOCK_MESSAGE)
        opt<std::string> custom_message{}; // nullable

        static auto_moderation_action_metadata create(snowflake channel_id_ = {}, integer duration_seconds_ = 0, opt<std::string> custom_message_ = {}) {
            return auto_moderation_action_metadata{
                .channel_id = channel_id_,
                .duration_seconds = duration_seconds_,
                .custom_message = std::move(custom_message_),
            };
        }
        decltype(auto) set_channel_id(this auto&& self, snowflake channel_id_) noexcept { self.channel_id = channel_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_channel(this auto&& self, snowflake id_) noexcept { return std::forward<decltype(self)>(self).set_channel_id(id_); }
        decltype(auto) set_duration_seconds(this auto&& self, integer duration_seconds_) noexcept { self.duration_seconds = duration_seconds_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_custom_message(this auto&& self, opt<std::string> custom_message_) { self.custom_message = std::move(custom_message_); return std::forward<decltype(self)>(self); }
    };

    enum class auto_moderation_action_type : std::uint8_t {
        BLOCK_MESSAGE = 1,
        SEND_ALERT_MESSAGE = 2,
        TIMEOUT = 3,
        BLOCK_MEMBER_INTERACTION = 4,
    };

    // Auto Moderation action
    struct auto_moderation_action {
        // The type of action (1=BLOCK_MESSAGE, 2=SEND_ALERT_MESSAGE, 3=TIMEOUT, 4=BLOCK_MEMBER_INTERACTION)
        auto_moderation_action_type type{};
        // Additional metadata needed during execution for this specific action type
        opt<auto_moderation_action_metadata> metadata{};

        static auto_moderation_action create(auto_moderation_action_type type_ = auto_moderation_action_type::BLOCK_MESSAGE, opt<auto_moderation_action_metadata> metadata_ = {}) {
            return auto_moderation_action{.type = type_, .metadata = std::move(metadata_)};
        }
        decltype(auto) set_type(this auto&& self, auto_moderation_action_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_metadata(this auto&& self, opt<auto_moderation_action_metadata> metadata_) { self.metadata = std::move(metadata_); return std::forward<decltype(self)>(self); }
    };

    enum class auto_moderation_rule_keyword_preset_type : std::uint8_t {
        PROFANITY = 1,
        SEXUAL_CONTENT = 2,
        SLURS = 3,
    };

    // Auto Moderation trigger metadata
    struct auto_moderation_trigger_metadata {
        // Substrings which will be searched for in content (KEYWORD, MEMBER_PROFILE)
        std::vector<std::string> keyword_filter{};
        // Regular expression patterns which will be matched against content (KEYWORD, MEMBER_PROFILE)
        std::vector<std::string> regex_patterns{};
        // Internally pre-defined wordsets (KEYWORD_PRESET). Values: 1=PROFANITY, 2=SEXUAL_CONTENT, 3=SLURS
        std::vector<auto_moderation_rule_keyword_preset_type> presets{};
        // Substrings which should not trigger the rule (KEYWORD, KEYWORD_PRESET, MEMBER_PROFILE)
        std::vector<std::string> allow_list{};
        // Total number of unique role and user mentions allowed per message (MENTION_SPAM)
        integer mention_total_limit{};
        // Whether to automatically detect mention raids (MENTION_SPAM)
        bool mention_raid_protection_enabled{};

        static auto_moderation_trigger_metadata create() noexcept {
            return auto_moderation_trigger_metadata{};
        }
        decltype(auto) set_keyword_filter(this auto&& self, std::vector<std::string> keyword_filter_) { self.keyword_filter = std::move(keyword_filter_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_keyword_filter, std::string)
        decltype(auto) add_keyword_filter(this auto&& self, std::string itm) { self.keyword_filter.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_regex_patterns(this auto&& self, std::vector<std::string> regex_patterns_) { self.regex_patterns = std::move(regex_patterns_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_regex_patterns, std::string)
        decltype(auto) add_regex_pattern(this auto&& self, std::string itm) { self.regex_patterns.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_presets(this auto&& self, std::vector<auto_moderation_rule_keyword_preset_type> presets_) { self.presets = std::move(presets_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_presets, auto_moderation_rule_keyword_preset_type)
        decltype(auto) add_preset(this auto&& self, auto_moderation_rule_keyword_preset_type itm) noexcept { self.presets.emplace_back(itm); return std::forward<decltype(self)>(self); }
        decltype(auto) set_allow_list(this auto&& self, std::vector<std::string> allow_list_) { self.allow_list = std::move(allow_list_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_allow_list, std::string)
        decltype(auto) add_allow_list(this auto&& self, std::string itm) { self.allow_list.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_mention_total_limit(this auto&& self, integer mention_total_limit_) noexcept { self.mention_total_limit = mention_total_limit_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_mention_raid_protection_enabled(this auto&& self, bool mention_raid_protection_enabled_) noexcept { self.mention_raid_protection_enabled = mention_raid_protection_enabled_; return std::forward<decltype(self)>(self); }
        decltype(auto) add_keyword(this auto&& self, std::string k) { return std::forward<decltype(self)>(self).add_keyword_filter(std::move(k)); }
        decltype(auto) add_pattern(this auto&& self, std::string r) { return std::forward<decltype(self)>(self).add_regex_pattern(std::move(r)); }
        decltype(auto) add_allowed(this auto&& self, std::string a) { return std::forward<decltype(self)>(self).add_allow_list(std::move(a)); }
    };

    enum class auto_moderation_rule_event_type : std::uint8_t {
        MESSAGE_SEND = 1,
        MEMBER_UPDATE = 2,
    };

    enum class auto_moderation_rule_trigger_type : std::uint8_t {
        KEYWORD	= 1,
        SPAM = 3,
        KEYWORD_PRESET = 4,
        MENTION_SPAM = 5,
        MEMBER_PROFILE = 6,
    };

    // Auto Moderation rule
    struct auto_moderation_rule {
        // The id of this rule
        snowflake id{};
        // The id of the guild which this rule belongs to
        snowflake guild_id{};
        // The rule name
        std::string name{};
        // The user which first created this rule
        snowflake creator_id{};
        // The rule event type (1=MESSAGE_SEND, 2=MEMBER_UPDATE)
        auto_moderation_rule_event_type event_type{};
        // The rule trigger type (1=KEYWORD, 3=SPAM, 4=KEYWORD_PRESET, 5=MENTION_SPAM, 6=MEMBER_PROFILE)
        auto_moderation_rule_trigger_type trigger_type{};
        // The rule trigger metadata
        auto_moderation_trigger_metadata trigger_metadata{};
        // The actions which will execute when the rule is triggered
        std::vector<auto_moderation_action> actions{};
        // Whether the rule is enabled
        bool enabled{};
        // The role ids that should not be affected by the rule (Maximum of 20)
        std::vector<snowflake> exempt_roles{};
        // The channel ids that should not be affected by the rule (Maximum of 50)
        std::vector<snowflake> exempt_channels{};

        static auto_moderation_rule create(snowflake id_ = {}, snowflake guild_id_ = {}, std::string name_ = {}, auto_moderation_rule_event_type event_type_ = auto_moderation_rule_event_type::MESSAGE_SEND, auto_moderation_rule_trigger_type trigger_type_ = auto_moderation_rule_trigger_type::KEYWORD) {
            return auto_moderation_rule{
                .id = id_,
                .guild_id = guild_id_,
                .name = std::move(name_),
                .event_type = event_type_,
                .trigger_type = trigger_type_,
            };
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild_id(this auto&& self, snowflake guild_id_) noexcept { self.guild_id = guild_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild(this auto&& self, snowflake id_) noexcept { return std::forward<decltype(self)>(self).set_guild_id(id_); }
        decltype(auto) set_name(this auto&& self, std::string name_) { self.name = std::move(name_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_creator_id(this auto&& self, snowflake creator_id_) noexcept { self.creator_id = creator_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_creator(this auto&& self, snowflake id_) noexcept { return std::forward<decltype(self)>(self).set_creator_id(id_); }
        decltype(auto) set_event_type(this auto&& self, auto_moderation_rule_event_type event_type_) noexcept { self.event_type = event_type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_trigger_type(this auto&& self, auto_moderation_rule_trigger_type trigger_type_) noexcept { self.trigger_type = trigger_type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_trigger_metadata(this auto&& self, auto_moderation_trigger_metadata trigger_metadata_) { self.trigger_metadata = std::move(trigger_metadata_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_actions(this auto&& self, std::vector<auto_moderation_action> actions_) { self.actions = std::move(actions_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_actions, auto_moderation_action)
        decltype(auto) add_action(this auto&& self, auto_moderation_action itm) { self.actions.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_enabled(this auto&& self, bool enabled_) noexcept { self.enabled = enabled_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_exempt_roles(this auto&& self, std::vector<snowflake> exempt_roles_) { self.exempt_roles = std::move(exempt_roles_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_exempt_roles, snowflake)
        decltype(auto) add_exempt_role(this auto&& self, snowflake itm) noexcept { self.exempt_roles.emplace_back(itm); return std::forward<decltype(self)>(self); }
        decltype(auto) set_exempt_channels(this auto&& self, std::vector<snowflake> exempt_channels_) { self.exempt_channels = std::move(exempt_channels_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_exempt_channels, snowflake)
        decltype(auto) add_exempt_channel(this auto&& self, snowflake itm) noexcept { self.exempt_channels.emplace_back(itm); return std::forward<decltype(self)>(self); }
    };
}

namespace interaction {
    enum class interaction_context_type : std::uint8_t {
        GUILD = 0, // Interaction can be used within servers
        BOT_DM = 1, // Interaction can be used within DMs with the app’s bot user
        PRIVATE_CHANNEL = 2, // Interaction can be used within Group DMs and DMs other than the app’s bot user
    };
}

namespace application_commands {

    enum class application_command_permission_type : std::uint8_t {
        ROLE = 1,
        USER = 2,
        CHANNEL = 3,
    };

    enum class application_command_option_type : std::uint8_t {
        SUB_COMMAND = 1, // 
        SUB_COMMAND_GROUP = 2, // 
        STRING = 3, // 
        INTEGER = 4, // Any integer between -2^53+1 and 2^53-1
        BOOLEAN = 5, // 
        USER = 6, // 
        CHANNEL = 7, // Includes all channel types + categories
        ROLE = 8, // 
        MENTIONABLE = 9, // Includes users and roles
        NUMBER = 10, // Any double between -2^53 and 2^53
        ATTACHMENT = 11, // attachment object
    };

    enum class application_command_type : std::uint8_t {
        CHAT_INPUT = 1, // Slash commands; a text-based command that shows up when a user types /
        USER = 2, // A UI-based command that shows up when you right click or tap on a user
        MESSAGE = 3, // A UI-based command that shows up when you right click or tap on a message
        PRIMARY_ENTRY_POINT = 4, // A UI-based command that represents the primary way to invoke an app’s Activity
    };

    enum class entry_point_command_handler_types : std::uint8_t {
        APP_HANDLER = 1, // The app handles the interaction using an interaction token
        DISCORD_LAUNCH_ACTIVITY = 2, // Discord handles the interaction by launching an Activity and sending a follow-up message without coordinating with the app
    };

    // Represents an individual permission overwrite for a command
    struct application_command_permissions {
        snowflake id{};
        application_command_permission_type type{};
        bool permission{};

        static application_command_permissions create(snowflake id_ = {}, application_command_permission_type type_ = application_command_permission_type::ROLE, bool permission_ = false) noexcept {
            return application_command_permissions{
                .id = id_,
                .type = type_,
                .permission = permission_,
            };
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_type(this auto&& self, application_command_permission_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_permission(this auto&& self, bool permission_) noexcept { self.permission = permission_; return std::forward<decltype(self)>(self); }
    };

    // This is the direct payload for APPLICATION_COMMAND_PERMISSIONS_UPDATE
    struct guild_application_command_permissions {
        snowflake id{}; // ID of the command or the application ID
        snowflake application_id{};
        snowflake guild_id{};
        std::vector<application_command_permissions> permissions{};

        static guild_application_command_permissions create(snowflake id_ = {}, snowflake application_id_ = {}, snowflake guild_id_ = {}, std::vector<application_command_permissions> permissions_ = {}) {
            return guild_application_command_permissions{
                .id = id_,
                .application_id = application_id_,
                .guild_id = guild_id_,
                .permissions = std::move(permissions_),
            };
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_application_id(this auto&& self, snowflake application_id_) noexcept { self.application_id = application_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_application(this auto&& self, snowflake id_) noexcept { return std::forward<decltype(self)>(self).set_application_id(id_); }
        decltype(auto) set_guild_id(this auto&& self, snowflake guild_id_) noexcept { self.guild_id = guild_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild(this auto&& self, snowflake id_) noexcept { return std::forward<decltype(self)>(self).set_guild_id(id_); }
        decltype(auto) set_permissions(this auto&& self, std::vector<application_command_permissions> permissions_) { self.permissions = std::move(permissions_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_permissions, application_command_permissions)
        decltype(auto) add_permission(this auto&& self, application_command_permissions itm) { self.permissions.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
    };

    struct application_command_option_choice {
        std::string name{};
        opt<std::unordered_map<std::string, std::string>> name_localizations{};
        std::variant<std::string, integer, double> value{};

        static application_command_option_choice create(std::string name_ = {}, std::variant<std::string, integer, double> value_ = {}) {
            return application_command_option_choice{
                .name = std::move(name_),
                .value = std::move(value_),
            };
        }
        decltype(auto) set_name(this auto&& self, std::string name_) { self.name = std::move(name_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_name_localizations(this auto&& self, opt<std::unordered_map<std::string, std::string>> name_localizations_) { self.name_localizations = std::move(name_localizations_); return std::forward<decltype(self)>(self); }
        decltype(auto) add_name_localization(this auto&& self, std::string k, std::string v) {
            if (!self.name_localizations) self.name_localizations.emplace();
            (*self.name_localizations)[std::move(k)] = std::move(v);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_value(this auto&& self, std::variant<std::string, integer, double> value_) { self.value = std::move(value_); return std::forward<decltype(self)>(self); }
    };

    struct application_command_option {
        application_command_option_type type{};
        std::string name{};
        opt<std::unordered_map<std::string, std::string>> name_localizations{};
        std::string description{};
        opt<std::unordered_map<std::string, std::string>> description_localizations{};
        opt<bool> required{};
        opt<std::vector<application_command_option_choice>> choices{};
        opt<std::vector<application_command_option>> options{};
        opt<std::vector<channel::channel_type>> channel_types{};
        opt<std::variant<integer, double>> min_value{};
        opt<std::variant<integer, double>> max_value{};
        opt<std::uint16_t> min_length{};
        opt<std::uint16_t> max_length{};
        opt<bool> autocomplete{};
        opt<std::vector<std::string>> file_types{};

        static application_command_option create(application_command_option_type type_ = application_command_option_type::STRING, std::string name_ = {}, std::string description_ = {}) {
            return application_command_option{
                .type = type_,
                .name = std::move(name_),
                .description = std::move(description_),
            };
        }
        decltype(auto) set_type(this auto&& self, application_command_option_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_name(this auto&& self, std::string name_) { self.name = std::move(name_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_name_localizations(this auto&& self, opt<std::unordered_map<std::string, std::string>> name_localizations_) { self.name_localizations = std::move(name_localizations_); return std::forward<decltype(self)>(self); }
        decltype(auto) add_name_localization(this auto&& self, std::string k, std::string v) {
            if (!self.name_localizations) self.name_localizations.emplace();
            (*self.name_localizations)[std::move(k)] = std::move(v);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_description(this auto&& self, std::string description_) { self.description = std::move(description_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_description_localizations(this auto&& self, opt<std::unordered_map<std::string, std::string>> description_localizations_) { self.description_localizations = std::move(description_localizations_); return std::forward<decltype(self)>(self); }
        decltype(auto) add_description_localization(this auto&& self, std::string k, std::string v) {
            if (!self.description_localizations) self.description_localizations.emplace();
            (*self.description_localizations)[std::move(k)] = std::move(v);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_required(this auto&& self, opt<bool> required_) noexcept { self.required = required_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_choices(this auto&& self, opt<std::vector<application_command_option_choice>> choices_) { self.choices = std::move(choices_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_choices, application_command_option_choice)
        decltype(auto) add_choice(this auto&& self, application_command_option_choice itm) {
            if (!self.choices) self.choices.emplace();
            self.choices->emplace_back(std::move(itm));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_options(this auto&& self, opt<std::vector<application_command_option>> options_) { self.options = std::move(options_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_options, application_command_option)
        decltype(auto) add_option(this auto&& self, application_command_option itm) {
            if (!self.options) self.options.emplace();
            self.options->emplace_back(std::move(itm));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_channel_types(this auto&& self, opt<std::vector<channel::channel_type>> channel_types_) { self.channel_types = std::move(channel_types_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_channel_types, channel::channel_type)
        decltype(auto) add_channel_type(this auto&& self, channel::channel_type itm) {
            if (!self.channel_types) self.channel_types.emplace();
            self.channel_types->emplace_back(itm);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_min_value(this auto&& self, opt<std::variant<integer, double>> min_value_) { self.min_value = std::move(min_value_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_max_value(this auto&& self, opt<std::variant<integer, double>> max_value_) { self.max_value = std::move(max_value_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_min_length(this auto&& self, opt<std::uint16_t> min_length_) noexcept { self.min_length = min_length_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_max_length(this auto&& self, opt<std::uint16_t> max_length_) noexcept { self.max_length = max_length_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_autocomplete(this auto&& self, opt<bool> autocomplete_) noexcept { self.autocomplete = autocomplete_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_file_types(this auto&& self, opt<std::vector<std::string>> file_types_) { self.file_types = std::move(file_types_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_file_types, std::string)
        decltype(auto) add_file_type(this auto&& self, std::string itm) {
            if (!self.file_types) self.file_types.emplace();
            self.file_types->emplace_back(std::move(itm));
            return std::forward<decltype(self)>(self);
        }

        decltype(auto) autocompleted(this auto&& self, const bool ac = true) noexcept {
            self.autocomplete = ac;
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) with_range(this auto&& self, const integer min, const integer max) noexcept {
            self.min_value = min;
            self.max_value = max;
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) with_min(this auto&& self, const integer min) noexcept {
            self.min_value = min;
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) with_number_min(this auto&& self, const double min) noexcept {
            self.min_value = min;
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) with_number_range(this auto&& self, const double min, const double max) noexcept {
            self.min_value = min;
            self.max_value = max;
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) with_length(this auto&& self, const std::uint16_t min, const std::uint16_t max) noexcept {
            self.min_length = min;
            self.max_length = max;
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) with_choices(this auto&& self, std::vector<application_command_option_choice> choices_) {
            self.choices = std::move(choices_);
            return std::forward<decltype(self)>(self);
        }
        DISCUSY_VARIADIC_SETTER(with_choices, application_command_option_choice)
        decltype(auto) with_channel_types(this auto&& self, std::vector<channel::channel_type> types) {
            self.channel_types = std::move(types);
            return std::forward<decltype(self)>(self);
        }
        DISCUSY_VARIADIC_SETTER(with_channel_types, channel::channel_type)
    };

    struct application_command {
        snowflake id{};
        opt<application_command_type> type{};
        snowflake application_id{};
        opt<snowflake> guild_id{};
        std::string name{};
        opt<std::unordered_map<std::string, std::string>> name_localizations{};
        std::string description{};
        opt<std::unordered_map<std::string, std::string>> description_localizations{};
        opt<std::vector<application_command_option>> options{};
        opt<permissions_t> default_member_permissions{};
        opt<bool> dm_permission{}; // deprecated
        opt<bool> default_permission{}; // pretty much deprecated
        opt<bool> nsfw{};
        opt<std::vector<application::application_integration_type>> integration_types{};
        opt<std::vector<interaction::interaction_context_type>> contexts{};
        snowflake version{};
        opt<entry_point_command_handler_types> handler{};

        static application_command create(snowflake id_ = {}, snowflake application_id_ = {}, std::string name_ = {}, std::string description_ = {}) {
            return application_command{
                .id = id_,
                .application_id = application_id_,
                .name = std::move(name_),
                .description = std::move(description_),
            };
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_type(this auto&& self, opt<application_command_type> type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_application_id(this auto&& self, snowflake application_id_) noexcept { self.application_id = application_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_application(this auto&& self, snowflake id_) noexcept { return std::forward<decltype(self)>(self).set_application_id(id_); }
        decltype(auto) set_guild_id(this auto&& self, opt<snowflake> guild_id_) noexcept { self.guild_id = guild_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_guild_id(id_); }
        decltype(auto) set_name(this auto&& self, std::string name_) { self.name = std::move(name_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_name_localizations(this auto&& self, opt<std::unordered_map<std::string, std::string>> name_localizations_) { self.name_localizations = std::move(name_localizations_); return std::forward<decltype(self)>(self); }
        decltype(auto) add_name_localization(this auto&& self, std::string k, std::string v) {
            if (!self.name_localizations) self.name_localizations.emplace();
            (*self.name_localizations)[std::move(k)] = std::move(v);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_description(this auto&& self, std::string description_) { self.description = std::move(description_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_description_localizations(this auto&& self, opt<std::unordered_map<std::string, std::string>> description_localizations_) { self.description_localizations = std::move(description_localizations_); return std::forward<decltype(self)>(self); }
        decltype(auto) add_description_localization(this auto&& self, std::string k, std::string v) {
            if (!self.description_localizations) self.description_localizations.emplace();
            (*self.description_localizations)[std::move(k)] = std::move(v);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_options(this auto&& self, opt<std::vector<application_command_option>> options_) { self.options = std::move(options_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_options, application_command_option)
        decltype(auto) add_option(this auto&& self, application_command_option itm) {
            if (!self.options) self.options.emplace();
            self.options->emplace_back(std::move(itm));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_default_member_permissions(this auto&& self, opt<permissions_t> default_member_permissions_) noexcept { self.default_member_permissions = default_member_permissions_; return std::forward<decltype(self)>(self); }
        decltype(auto) add_default_member_permission(this auto&& self, permissions::permissions p) noexcept {
            if (!self.default_member_permissions) self.default_member_permissions.emplace();
            self.default_member_permissions->add_flags(p);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) add_default_member_permissions(this auto&& self, permissions::permissions p) noexcept { return std::forward<decltype(self)>(self).add_default_member_permission(p); }
        decltype(auto) set_dm_permission(this auto&& self, opt<bool> dm_permission_) noexcept { self.dm_permission = dm_permission_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_default_permission(this auto&& self, opt<bool> default_permission_) noexcept { self.default_permission = default_permission_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_nsfw(this auto&& self, opt<bool> nsfw_) noexcept { self.nsfw = nsfw_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_integration_types(this auto&& self, opt<std::vector<application::application_integration_type>> integration_types_) { self.integration_types = std::move(integration_types_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_integration_types, application::application_integration_type)
        decltype(auto) add_integration_type(this auto&& self, application::application_integration_type itm) {
            if (!self.integration_types) self.integration_types.emplace();
            self.integration_types->emplace_back(itm);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_contexts(this auto&& self, opt<std::vector<interaction::interaction_context_type>> contexts_) { self.contexts = std::move(contexts_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_contexts, interaction::interaction_context_type)
        decltype(auto) add_context(this auto&& self, interaction::interaction_context_type itm) {
            if (!self.contexts) self.contexts.emplace();
            self.contexts->emplace_back(itm);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_version(this auto&& self, snowflake version_) noexcept { self.version = version_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_handler(this auto&& self, opt<entry_point_command_handler_types> handler_) noexcept { self.handler = handler_; return std::forward<decltype(self)>(self); }
    };
}

namespace entitlement {

    enum class entitlement_type : std::uint8_t {
        PURCHASE = 1, // Entitlement was purchased by user
        PREMIUM_SUBSCRIPTION = 2, // Entitlement for Discord Nitro subscription
        DEVELOPER_GIFT = 3, // Entitlement was gifted by developer
        TEST_MODE_PURCHASE = 4, // Entitlement was purchased by a dev in application test mode
        FREE_PURCHASE = 5, // Entitlement was granted when the SKU was free
        USER_GIFT = 6, // Entitlement was gifted by another user
        PREMIUM_PURCHASE = 7, // Entitlement was claimed by user for free as a Nitro Subscriber
        APPLICATION_SUBSCRIPTION = 8, // Entitlement was purchased as an app subscription
    };

    struct entitlement {
        snowflake id{};
        snowflake sku_id{};
        snowflake application_id{};
        opt<snowflake> user_id{};
        entitlement_type type{};
        bool deleted{};
        opt<timestamp> starts_at{};
        opt<timestamp> ends_at{};
        opt<snowflake> guild_id{};
        opt<bool> consumed{};

        static entitlement create(snowflake id_ = {}, snowflake sku_id_ = {}, snowflake application_id_ = {}, entitlement_type type_ = entitlement_type::PURCHASE) noexcept {
            return entitlement{
                .id = id_,
                .sku_id = sku_id_,
                .application_id = application_id_,
                .type = type_,
            };
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_sku_id(this auto&& self, snowflake sku_id_) noexcept { self.sku_id = sku_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_sku(this auto&& self, snowflake id_) noexcept { return std::forward<decltype(self)>(self).set_sku_id(id_); }
        decltype(auto) set_application_id(this auto&& self, snowflake application_id_) noexcept { self.application_id = application_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_application(this auto&& self, snowflake id_) noexcept { return std::forward<decltype(self)>(self).set_application_id(id_); }
        decltype(auto) set_user_id(this auto&& self, opt<snowflake> user_id_) noexcept { self.user_id = user_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_user(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_user_id(id_); }
        decltype(auto) set_type(this auto&& self, entitlement_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_deleted(this auto&& self, bool deleted_) noexcept { self.deleted = deleted_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_starts_at(this auto&& self, opt<timestamp> starts_at_) noexcept { self.starts_at = starts_at_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_ends_at(this auto&& self, opt<timestamp> ends_at_) noexcept { self.ends_at = ends_at_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild_id(this auto&& self, opt<snowflake> guild_id_) noexcept { self.guild_id = guild_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_guild_id(id_); }
        decltype(auto) set_consumed(this auto&& self, opt<bool> consumed_) noexcept { self.consumed = consumed_; return std::forward<decltype(self)>(self); }
    };
}

namespace audit_log {
    enum class audit_log_event : std::uint8_t {
        GUILD_UPDATE = 1, // Server settings were updated	Guild
        CHANNEL_CREATE = 10, // Channel was created	Channel
        CHANNEL_UPDATE = 11, // Channel settings were updated	Channel
        CHANNEL_DELETE = 12, // Channel was deleted	Channel
        CHANNEL_OVERWRITE_CREATE = 13, // Permission overwrite was added to a channel	Channel Overwrite
        CHANNEL_OVERWRITE_UPDATE = 14, // Permission overwrite was updated for a channel	Channel Overwrite
        CHANNEL_OVERWRITE_DELETE = 15, // Permission overwrite was deleted from a channel	Channel Overwrite
        MEMBER_KICK = 20, // Member was removed from server	
        MEMBER_PRUNE = 21, // Members were pruned from server	
        MEMBER_BAN_ADD = 22, // Member was banned from server	
        MEMBER_BAN_REMOVE = 23, // Server ban was lifted for a member	
        MEMBER_UPDATE = 24, // Member was updated in server	Member
        MEMBER_ROLE_UPDATE = 25, // Member was added or removed from a role	Partial Role*
        MEMBER_MOVE = 26, // Member was moved to a different voice channel	
        MEMBER_DISCONNECT = 27, // Member was disconnected from a voice channel	
        BOT_ADD = 28, // Bot user was added to server	
        ROLE_CREATE = 30, // Role was created	Role
        ROLE_UPDATE = 31, // Role was edited	Role
        ROLE_DELETE = 32, // Role was deleted	Role
        INVITE_CREATE = 40, // Server invite was created	Invite and Invite Metadata*
        INVITE_UPDATE = 41, // Server invite was updated	Invite and Invite Metadata*
        INVITE_DELETE = 42, // Server invite was deleted	Invite and Invite Metadata*
        WEBHOOK_CREATE = 50, // Webhook was created	Webhook*
        WEBHOOK_UPDATE = 51, // Webhook properties or channel were updated	Webhook*
        WEBHOOK_DELETE = 52, // Webhook was deleted	Webhook*
        EMOJI_CREATE = 60, // Emoji was created	Emoji
        EMOJI_UPDATE = 61, // Emoji name was updated	Emoji
        EMOJI_DELETE = 62, // Emoji was deleted	Emoji
        MESSAGE_DELETE = 72, // Single message was deleted	
        MESSAGE_BULK_DELETE = 73, // Multiple messages were deleted	
        MESSAGE_PIN = 74, // Message was pinned to a channel	
        MESSAGE_UNPIN = 75, // Message was unpinned from a channel	
        INTEGRATION_CREATE = 80, // App was added to server	Integration
        INTEGRATION_UPDATE = 81, // App was updated (as an example, its scopes were updated)	Integration
        INTEGRATION_DELETE = 82, // App was removed from server	Integration
        STAGE_INSTANCE_CREATE = 83, // Stage instance was created (stage channel becomes live)	Stage Instance
        STAGE_INSTANCE_UPDATE = 84, // Stage instance details were updated	Stage Instance
        STAGE_INSTANCE_DELETE = 85, // Stage instance was deleted (stage channel no longer live)	Stage Instance
        STICKER_CREATE = 90, // Sticker was created	Sticker
        STICKER_UPDATE = 91, // Sticker details were updated	Sticker
        STICKER_DELETE = 92, // Sticker was deleted	Sticker
        GUILD_SCHEDULED_EVENT_CREATE = 100, // Event was created	Guild Scheduled Event
        GUILD_SCHEDULED_EVENT_UPDATE = 101, // Event was updated	Guild Scheduled Event
        GUILD_SCHEDULED_EVENT_DELETE = 102, // Event was cancelled	Guild Scheduled Event
        THREAD_CREATE = 110, // Thread was created in a channel	Thread
        THREAD_UPDATE = 111, // Thread was updated	Thread
        THREAD_DELETE = 112, // Thread was deleted	Thread
        APPLICATION_COMMAND_PERMISSION_UPDATE = 121, // Permissions were updated for a command	Command Permission*
        SOUNDBOARD_SOUND_CREATE = 130, // Soundboard sound was created	Soundboard Sound
        SOUNDBOARD_SOUND_UPDATE = 131, // Soundboard sound was updated	Soundboard Sound
        SOUNDBOARD_SOUND_DELETE = 132, // Soundboard sound was deleted	Soundboard Sound
        AUTO_MODERATION_RULE_CREATE = 140, // Auto Moderation rule was created	Auto Moderation Rule
        AUTO_MODERATION_RULE_UPDATE = 141, // Auto Moderation rule was updated	Auto Moderation Rule
        AUTO_MODERATION_RULE_DELETE = 142, // Auto Moderation rule was deleted	Auto Moderation Rule
        AUTO_MODERATION_BLOCK_MESSAGE = 143, // Message was blocked by Auto Moderation	
        AUTO_MODERATION_FLAG_TO_CHANNEL = 144, // Message was flagged by Auto Moderation	
        AUTO_MODERATION_USER_COMMUNICATION_DISABLED = 145, // Member was timed out by Auto Moderation	
        AUTO_MODERATION_QUARANTINE_USER = 146, // Member was quarantined by Auto Moderation	
        CREATOR_MONETIZATION_REQUEST_CREATED = 150, // Creator monetization request was created	
        CREATOR_MONETIZATION_TERMS_ACCEPTED = 151, // Creator monetization terms were accepted	
        ONBOARDING_PROMPT_CREATE = 163, // Guild Onboarding Question was created	Onboarding Prompt Structure
        ONBOARDING_PROMPT_UPDATE = 164, // Guild Onboarding Question was updated	Onboarding Prompt Structure
        ONBOARDING_PROMPT_DELETE = 165, // Guild Onboarding Question was deleted	Onboarding Prompt Structure
        ONBOARDING_CREATE = 166, // Guild Onboarding was created	Guild Onboarding
        ONBOARDING_UPDATE = 167, // Guild Onboarding was updated	Guild Onboarding
        HOME_SETTINGS_CREATE = 190, // Guild Server Guide was created	
        HOME_SETTINGS_UPDATE = 191, // Guild Server Guide was updated	
        VOICE_CHANNEL_STATUS_CREATE = 192, // A voice channel status was set by a user
        VOICE_CHANNEL_STATUS_DELETE = 193, // A voice channel status was deleted by a user	
    };


    struct audit_log_change {
        opt<glz::raw_json_view> new_value{}; // Can represent strings, ints, bools dynamically
        opt<glz::raw_json_view> old_value{};
        std::string key{};

        static audit_log_change create(std::string key_ = {}, opt<glz::raw_json_view> new_val = {}, opt<glz::raw_json_view> old_val = {}) {
            return audit_log_change{
                .new_value = std::move(new_val),
                .old_value = std::move(old_val),
                .key = std::move(key_),
            };
        }
        decltype(auto) set_new_value(this auto&& self, opt<glz::raw_json_view> new_value_) { self.new_value = std::move(new_value_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_old_value(this auto&& self, opt<glz::raw_json_view> old_value_) { self.old_value = std::move(old_value_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_key(this auto&& self, std::string key_) { self.key = std::move(key_); return std::forward<decltype(self)>(self); }
    };

    struct optional_audit_entry_info {
        opt<snowflake> application_id{};
        opt<std::string> auto_moderation_rule_name{};
        opt<std::string> auto_moderation_rule_trigger_type{};
        opt<snowflake> channel_id{};
        opt<std::string> count{};
        opt<std::string> delete_member_days{};
        opt<snowflake> id{};
        opt<std::string> members_removed{};
        opt<snowflake> message_id{};
        opt<std::string> role_name{};
        opt<std::string> type{};
        opt<std::string> integration_type{};
        opt<std::string> status{};

        static optional_audit_entry_info create() noexcept {
            return optional_audit_entry_info{};
        }
        decltype(auto) set_application_id(this auto&& self, opt<snowflake> application_id_) noexcept { self.application_id = application_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_application(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_application_id(id_); }
        decltype(auto) set_auto_moderation_rule_name(this auto&& self, opt<std::string> auto_moderation_rule_name_) { self.auto_moderation_rule_name = std::move(auto_moderation_rule_name_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_auto_moderation_rule_trigger_type(this auto&& self, opt<std::string> auto_moderation_rule_trigger_type_) { self.auto_moderation_rule_trigger_type = std::move(auto_moderation_rule_trigger_type_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_channel_id(this auto&& self, opt<snowflake> channel_id_) noexcept { self.channel_id = channel_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_channel(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_channel_id(id_); }
        decltype(auto) set_count(this auto&& self, opt<std::string> count_) { self.count = std::move(count_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_delete_member_days(this auto&& self, opt<std::string> delete_member_days_) { self.delete_member_days = std::move(delete_member_days_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_id(this auto&& self, opt<snowflake> id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_members_removed(this auto&& self, opt<std::string> members_removed_) { self.members_removed = std::move(members_removed_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_message_id(this auto&& self, opt<snowflake> message_id_) noexcept { self.message_id = message_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_message(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_message_id(id_); }
        decltype(auto) set_role_name(this auto&& self, opt<std::string> role_name_) { self.role_name = std::move(role_name_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_type(this auto&& self, opt<std::string> type_) { self.type = std::move(type_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_integration_type(this auto&& self, opt<std::string> integration_type_) { self.integration_type = std::move(integration_type_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_status(this auto&& self, opt<std::string> status_) { self.status = std::move(status_); return std::forward<decltype(self)>(self); }
    };

    struct audit_log_entry {
        opt<std::string> target_id{};
        opt<std::vector<audit_log_change>> changes{};
        opt<snowflake> user_id{};
        snowflake id{};
        audit_log_event action_type{};
        opt<optional_audit_entry_info> options{};
        opt<std::string> reason{};

        static audit_log_entry create(snowflake id_ = {}, audit_log_event action_type_ = audit_log_event::GUILD_UPDATE) noexcept {
            return audit_log_entry{.id = id_, .action_type = action_type_};
        }
        decltype(auto) set_target_id(this auto&& self, opt<std::string> target_id_) { self.target_id = std::move(target_id_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_changes(this auto&& self, opt<std::vector<audit_log_change>> changes_) { self.changes = std::move(changes_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_changes, audit_log_change)
        decltype(auto) add_change(this auto&& self, audit_log_change itm) {
            if (!self.changes) self.changes.emplace();
            self.changes->emplace_back(std::move(itm));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_user_id(this auto&& self, opt<snowflake> user_id_) noexcept { self.user_id = user_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_user(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_user_id(id_); }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_action_type(this auto&& self, audit_log_event action_type_) noexcept { self.action_type = action_type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_options(this auto&& self, opt<optional_audit_entry_info> options_) { self.options = std::move(options_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_reason(this auto&& self, opt<std::string> reason_) { self.reason = std::move(reason_); return std::forward<decltype(self)>(self); }
        decltype(auto) add_change(this auto&& self, std::string key_, opt<glz::raw_json_view> new_val = {}, opt<glz::raw_json_view> old_val = {}) {
            return std::forward<decltype(self)>(self).add_change(audit_log_change{.new_value = new_val, .old_value = old_val, .key = std::move(key_)});
        }
    };

    struct audit_log_entry_with_guild {
        discusy::audit_log::audit_log_entry audit_log_entry{};

        static audit_log_entry_with_guild create(discusy::audit_log::audit_log_entry ale = {}, snowflake gid = {}) {
            return audit_log_entry_with_guild{.audit_log_entry = std::move(ale), .other = {.guild_id = gid}};
        }
        decltype(auto) set_audit_log_entry(this auto&& self, discusy::audit_log::audit_log_entry ale) { self.audit_log_entry = std::move(ale); return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild_id(this auto&& self, snowflake gid) noexcept { self.other.guild_id = gid; return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild(this auto&& self, snowflake gid) noexcept { return std::forward<decltype(self)>(self).set_guild_id(gid); }

        struct Other {
            snowflake guild_id{};
        } other;

        constexpr auto* operator->(this auto&& self) noexcept { return std::addressof(self.audit_log_entry); }
        constexpr decltype(auto) operator*(this auto&& self) noexcept { return (std::forward<decltype(self)>(self).audit_log_entry); }
        constexpr decltype(auto) operator()(this auto&& self) noexcept { return (std::forward<decltype(self)>(self).other); }

        struct glaze {
            using T = audit_log_entry_with_guild;
            static constexpr auto value = glz::merge{
                &T::audit_log_entry,
                &T::other,
            };
        };
    };
}

namespace guild_scheduled_event {
    struct guild_scheduled_event_entity_metadata {
        opt<std::string> location{};

        static guild_scheduled_event_entity_metadata create(opt<std::string> location_ = {}) {
            return guild_scheduled_event_entity_metadata{.location = std::move(location_)};
        }
        decltype(auto) set_location(this auto&& self, opt<std::string> location_) { self.location = std::move(location_); return std::forward<decltype(self)>(self); }
    };

    enum class guild_scheduled_event_privacy_level : std::uint8_t {
        GUILD_ONLY = 2, // the scheduled event is only accessible to guild members
    };

    enum class guild_scheduled_event_status : std::uint8_t {
        SCHEDULED = 1,
        ACTIVE = 2,
        COMPLETED = 3,
        CANCELED = 4,
    };

    enum class guild_scheduled_event_entity_types : std::uint8_t {
        STAGE_INSTANCE = 1,
        VOICE = 2,
        EXTERNAL = 3,
    };

    enum class guild_scheduled_event_recurrence_rule_frequency : std::uint8_t {
        YEARLY = 0,
        MONTHLY = 1,
        WEEKLY = 2,
        DAILY = 3,
    };

    enum class guild_scheduled_event_recurrence_rule_weekday : std::uint8_t {
        MONDAY = 0,
        TUESDAY = 1,
        WEDNESDAY = 2,
        THURSDAY = 3,
        FRIDAY = 4,
        SATURDAY = 5,
        SUNDAY = 6,
    };

    enum class guild_scheduled_event_recurrence_rule_month : std::uint8_t {
        JANUARY = 1,
        FEBRUARY = 2,
        MARCH = 3,
        APRIL = 4,
        MAY = 5,
        JUNE = 6,
        JULY = 7,
        AUGUST = 8,
        SEPTEMBER = 9,
        OCTOBER = 10,
        NOVEMBER = 11,
        DECEMBER = 12,
    };

    struct guild_scheduled_event_recurrence_rule_n_weekday {
        std::uint8_t n{}; // week to reoccur on, 1-5
        guild_scheduled_event_recurrence_rule_weekday day{};

        static guild_scheduled_event_recurrence_rule_n_weekday create(std::uint8_t n_ = 1, guild_scheduled_event_recurrence_rule_weekday day_ = guild_scheduled_event_recurrence_rule_weekday::MONDAY) noexcept {
            return guild_scheduled_event_recurrence_rule_n_weekday{.n = n_, .day = day_};
        }
        decltype(auto) set_n(this auto&& self, std::uint8_t n_) noexcept { self.n = n_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_day(this auto&& self, guild_scheduled_event_recurrence_rule_weekday day_) noexcept { self.day = day_; return std::forward<decltype(self)>(self); }
    };

    struct guild_scheduled_event_recurrence_rule {
        timestamp start{};
        opt<timestamp> end{};
        guild_scheduled_event_recurrence_rule_frequency frequency{};
        integer interval{};
        opt<std::vector<guild_scheduled_event_recurrence_rule_weekday>> by_weekday{};
        opt<std::vector<guild_scheduled_event_recurrence_rule_n_weekday>> by_n_weekday{};
        opt<std::vector<guild_scheduled_event_recurrence_rule_month>> by_month{};
        opt<std::vector<std::uint8_t>> by_month_day{};
        opt<std::vector<std::uint16_t>> by_year_day{};
        opt<integer> count{};

        static guild_scheduled_event_recurrence_rule create(timestamp start_ = {}, guild_scheduled_event_recurrence_rule_frequency frequency_ = guild_scheduled_event_recurrence_rule_frequency::WEEKLY, integer interval_ = 1) noexcept {
            return guild_scheduled_event_recurrence_rule{
                .start = start_,
                .frequency = frequency_,
                .interval = interval_,
            };
        }
        decltype(auto) set_start(this auto&& self, timestamp start_) noexcept { self.start = start_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_end(this auto&& self, opt<timestamp> end_) noexcept { self.end = end_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_frequency(this auto&& self, guild_scheduled_event_recurrence_rule_frequency frequency_) noexcept { self.frequency = frequency_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_interval(this auto&& self, integer interval_) noexcept { self.interval = interval_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_by_weekday(this auto&& self, opt<std::vector<guild_scheduled_event_recurrence_rule_weekday>> by_weekday_) { self.by_weekday = std::move(by_weekday_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_by_weekday, guild_scheduled_event_recurrence_rule_weekday)
        decltype(auto) add_by_weekday(this auto&& self, guild_scheduled_event_recurrence_rule_weekday itm) {
            if (!self.by_weekday) self.by_weekday.emplace();
            self.by_weekday->emplace_back(itm);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_by_n_weekday(this auto&& self, opt<std::vector<guild_scheduled_event_recurrence_rule_n_weekday>> by_n_weekday_) { self.by_n_weekday = std::move(by_n_weekday_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_by_n_weekday, guild_scheduled_event_recurrence_rule_n_weekday)
        decltype(auto) add_by_n_weekday(this auto&& self, guild_scheduled_event_recurrence_rule_n_weekday itm) {
            if (!self.by_n_weekday) self.by_n_weekday.emplace();
            self.by_n_weekday->emplace_back(itm);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_by_month(this auto&& self, opt<std::vector<guild_scheduled_event_recurrence_rule_month>> by_month_) { self.by_month = std::move(by_month_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_by_month, guild_scheduled_event_recurrence_rule_month)
        decltype(auto) add_by_month(this auto&& self, guild_scheduled_event_recurrence_rule_month itm) {
            if (!self.by_month) self.by_month.emplace();
            self.by_month->emplace_back(itm);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_by_month_day(this auto&& self, opt<std::vector<std::uint8_t>> by_month_day_) { self.by_month_day = std::move(by_month_day_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_by_month_day, std::uint8_t)
        decltype(auto) add_by_month_day(this auto&& self, std::uint8_t itm) {
            if (!self.by_month_day) self.by_month_day.emplace();
            self.by_month_day->emplace_back(itm);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_by_year_day(this auto&& self, opt<std::vector<std::uint16_t>> by_year_day_) { self.by_year_day = std::move(by_year_day_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_by_year_day, std::uint16_t)
        decltype(auto) add_by_year_day(this auto&& self, std::uint16_t itm) {
            if (!self.by_year_day) self.by_year_day.emplace();
            self.by_year_day->emplace_back(itm);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_count(this auto&& self, opt<integer> count_) noexcept { self.count = count_; return std::forward<decltype(self)>(self); }
    };

    struct guild_scheduled_event {
        snowflake id{};
        snowflake guild_id{};
        opt<snowflake> channel_id{};
        opt<snowflake> creator_id{};
        std::string name{};
        opt<std::string> description{};
        timestamp scheduled_start_time{};
        opt<timestamp> scheduled_end_time{};
        guild_scheduled_event_privacy_level privacy_level{};
        guild_scheduled_event_status status{};
        guild_scheduled_event_entity_types entity_type{};
        opt<snowflake> entity_id{};
        opt<guild_scheduled_event_entity_metadata> entity_metadata{};
        opt<integer> user_count{};
        opt<guild_scheduled_event_cover_hash> image{};
        opt<guild_scheduled_event_recurrence_rule> recurrence_rule{};

        static guild_scheduled_event create(snowflake id_ = {}, snowflake guild_id_ = {}, std::string name_ = {}, timestamp scheduled_start_time_ = {}, guild_scheduled_event_privacy_level privacy_level_ = guild_scheduled_event_privacy_level::GUILD_ONLY, guild_scheduled_event_status status_ = guild_scheduled_event_status::SCHEDULED, guild_scheduled_event_entity_types entity_type_ = guild_scheduled_event_entity_types::VOICE) {
            return guild_scheduled_event{
                .id = id_,
                .guild_id = guild_id_,
                .name = std::move(name_),
                .scheduled_start_time = scheduled_start_time_,
                .privacy_level = privacy_level_,
                .status = status_,
                .entity_type = entity_type_,
            };
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild_id(this auto&& self, snowflake guild_id_) noexcept { self.guild_id = guild_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild(this auto&& self, snowflake id_) noexcept { return std::forward<decltype(self)>(self).set_guild_id(id_); }
        decltype(auto) set_channel_id(this auto&& self, opt<snowflake> channel_id_) noexcept { self.channel_id = channel_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_channel(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_channel_id(id_); }
        decltype(auto) set_creator_id(this auto&& self, opt<snowflake> creator_id_) noexcept { self.creator_id = creator_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_creator(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_creator_id(id_); }
        decltype(auto) set_name(this auto&& self, std::string name_) { self.name = std::move(name_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_description(this auto&& self, opt<std::string> description_) { self.description = std::move(description_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_scheduled_start_time(this auto&& self, timestamp scheduled_start_time_) noexcept { self.scheduled_start_time = scheduled_start_time_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_scheduled_end_time(this auto&& self, opt<timestamp> scheduled_end_time_) noexcept { self.scheduled_end_time = scheduled_end_time_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_privacy_level(this auto&& self, guild_scheduled_event_privacy_level privacy_level_) noexcept { self.privacy_level = privacy_level_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_status(this auto&& self, guild_scheduled_event_status status_) noexcept { self.status = status_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_entity_type(this auto&& self, guild_scheduled_event_entity_types entity_type_) noexcept { self.entity_type = entity_type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_entity_id(this auto&& self, opt<snowflake> entity_id_) noexcept { self.entity_id = entity_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_entity(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_entity_id(id_); }
        decltype(auto) set_entity_metadata(this auto&& self, opt<guild_scheduled_event_entity_metadata> entity_metadata_) { self.entity_metadata = std::move(entity_metadata_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_user_count(this auto&& self, opt<integer> user_count_) noexcept { self.user_count = user_count_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_image(this auto&& self, opt<guild_scheduled_event_cover_hash> image_) { self.image = std::move(image_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_recurrence_rule(this auto&& self, opt<guild_scheduled_event_recurrence_rule> recurrence_rule_) { self.recurrence_rule = std::move(recurrence_rule_); return std::forward<decltype(self)>(self); }
    };

    struct guild_scheduled_event_user {
        snowflake guild_scheduled_event_id{};
        user::user user{};
        opt<guild::guild_member> member{};

        static guild_scheduled_event_user create(snowflake event_id = {}, user::user user_ = {}) {
            return guild_scheduled_event_user{
                .guild_scheduled_event_id = event_id,
                .user = std::move(user_),
            };
        }
        decltype(auto) set_guild_scheduled_event_id(this auto&& self, snowflake guild_scheduled_event_id_) noexcept { self.guild_scheduled_event_id = guild_scheduled_event_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild_scheduled_event(this auto&& self, snowflake id_) noexcept { return std::forward<decltype(self)>(self).set_guild_scheduled_event_id(id_); }
        decltype(auto) set_user(this auto&& self, user::user user_) { self.user = std::move(user_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_member(this auto&& self, opt<guild::guild_member> member_) { self.member = std::move(member_); return std::forward<decltype(self)>(self); }
    };
}

namespace soundboard {
    struct soundboard_sound {
        std::string name{};
        snowflake sound_id{};
        double volume{};
        opt<snowflake> emoji_id{};
        opt<std::string> emoji_name{};
        opt<snowflake> guild_id{};
        bool available{};
        opt<user::user> user{};

        static soundboard_sound create(std::string name_ = {}, snowflake sound_id_ = {}, double volume_ = 1.0) {
            return soundboard_sound{
                .name = std::move(name_),
                .sound_id = sound_id_,
                .volume = volume_,
            };
        }
        decltype(auto) set_name(this auto&& self, std::string name_) { self.name = std::move(name_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_sound_id(this auto&& self, snowflake sound_id_) noexcept { self.sound_id = sound_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_sound(this auto&& self, snowflake id_) noexcept { return std::forward<decltype(self)>(self).set_sound_id(id_); }
        decltype(auto) set_volume(this auto&& self, double volume_) noexcept { self.volume = volume_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_emoji_id(this auto&& self, opt<snowflake> emoji_id_) noexcept { self.emoji_id = emoji_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_emoji(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_emoji_id(id_); }
        decltype(auto) set_emoji_name(this auto&& self, opt<std::string> emoji_name_) { self.emoji_name = std::move(emoji_name_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild_id(this auto&& self, opt<snowflake> guild_id_) noexcept { self.guild_id = guild_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_guild_id(id_); }
        decltype(auto) set_available(this auto&& self, bool available_) noexcept { self.available = available_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_user(this auto&& self, opt<user::user> user_) { self.user = std::move(user_); return std::forward<decltype(self)>(self); }
    };
};

namespace interaction {
    enum class interaction_type : std::uint8_t {
        PING = 1,
        APPLICATION_COMMAND = 2,
        MESSAGE_COMPONENT = 3,
        APPLICATION_COMMAND_AUTOCOMPLETE = 4,
        MODAL_SUBMIT = 5,
    };

    struct message_interaction {
        snowflake id{};
        interaction_type type{};
        std::string name{};
        user::user user{};
        opt<guild::guild_member> member{};

        static message_interaction create(snowflake id_ = {}, interaction_type type_ = interaction_type::APPLICATION_COMMAND, std::string name_ = {}, user::user user_ = {}) {
            return message_interaction{
                .id = id_,
                .type = type_,
                .name = std::move(name_),
                .user = std::move(user_),
            };
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_type(this auto&& self, interaction_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_name(this auto&& self, std::string name_) { self.name = std::move(name_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_user(this auto&& self, user::user user_) { self.user = std::move(user_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_member(this auto&& self, opt<guild::guild_member> member_) { self.member = std::move(member_); return std::forward<decltype(self)>(self); }
    };
};

namespace invite {
    enum class invite_type : std::uint8_t {
        GUILD = 0,
        GROUP_DM = 1,
        FRIEND = 2,
    };
    
    enum class invite_target_type : std::uint8_t {
        STREAM = 1,
        EMBEDDED_APPLICATION = 2,
    };

    enum class guild_invite_flags : std::uint8_t {
        IS_GUEST_INVITE = 1ULL << 0, // this invite is a guest invite for a voice channel
    };

    struct invite {
        invite_type type{};
        std::string code{};
        opt<guild::guild> guild{}; // partial, double check
        opt<channel::channel> channel{}; // partial, double check
        opt<user::user> inviter{};
        opt<invite_target_type> target_type{};
        opt<user::user> target_user{};
        opt<application::application> target_application{}; // partial, double check
        opt<integer> approximate_presence_count{};
        opt<integer> approximate_member_count{};
        opt<timestamp> expires_at{};
        opt<guild_scheduled_event::guild_scheduled_event> guild_scheduled_event{};
        opt<flags_t<guild_invite_flags>> flags{};
        opt<std::vector<permissions::role>> roles{}; // partial, double check

        static invite create(invite_type type_ = invite_type::GUILD, std::string code_ = {}) {
            return invite{.type = type_, .code = std::move(code_)};
        }
        decltype(auto) set_type(this auto&& self, invite_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_code(this auto&& self, std::string code_) { self.code = std::move(code_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild(this auto&& self, opt<guild::guild> guild_) { self.guild = std::move(guild_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_channel(this auto&& self, opt<channel::channel> channel_) { self.channel = std::move(channel_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_inviter(this auto&& self, opt<user::user> inviter_) { self.inviter = std::move(inviter_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_target_type(this auto&& self, opt<invite_target_type> target_type_) noexcept { self.target_type = target_type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_target_user(this auto&& self, opt<user::user> target_user_) { self.target_user = std::move(target_user_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_target_application(this auto&& self, opt<application::application> target_application_) { self.target_application = std::move(target_application_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_approximate_presence_count(this auto&& self, opt<integer> approximate_presence_count_) noexcept { self.approximate_presence_count = approximate_presence_count_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_approximate_member_count(this auto&& self, opt<integer> approximate_member_count_) noexcept { self.approximate_member_count = approximate_member_count_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_expires_at(this auto&& self, opt<timestamp> expires_at_) noexcept { self.expires_at = expires_at_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild_scheduled_event(this auto&& self, opt<guild_scheduled_event::guild_scheduled_event> guild_scheduled_event_) { self.guild_scheduled_event = std::move(guild_scheduled_event_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_flags(this auto&& self, opt<flags_t<guild_invite_flags>> flags_) noexcept { self.flags = flags_; return std::forward<decltype(self)>(self); }
        decltype(auto) add_flags(this auto&& self, guild_invite_flags f) noexcept {
            if (!self.flags) self.flags.emplace();
            self.flags->add_flags(f);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) add_flag(this auto&& self, guild_invite_flags f) noexcept { return std::forward<decltype(self)>(self).add_flags(f); }
        decltype(auto) set_roles(this auto&& self, opt<std::vector<permissions::role>> roles_) { self.roles = std::move(roles_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_roles, permissions::role)
        decltype(auto) add_role(this auto&& self, permissions::role itm) {
            if (!self.roles) self.roles.emplace();
            self.roles->emplace_back(std::move(itm));
            return std::forward<decltype(self)>(self);
        }
    };

    struct invite_metadata {
        integer uses{};
        integer max_uses{};
        integer max_age{};
        bool temporary{};
        timestamp created_at{};

        static invite_metadata create(integer uses_ = 0, integer max_uses_ = 0, integer max_age_ = 0, bool temporary_ = false, timestamp created_at_ = {}) noexcept {
            return invite_metadata{
                .uses = uses_,
                .max_uses = max_uses_,
                .max_age = max_age_,
                .temporary = temporary_,
                .created_at = created_at_,
            };
        }
        decltype(auto) set_uses(this auto&& self, integer uses_) noexcept { self.uses = uses_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_max_uses(this auto&& self, integer max_uses_) noexcept { self.max_uses = max_uses_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_max_age(this auto&& self, integer max_age_) noexcept { self.max_age = max_age_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_temporary(this auto&& self, bool temporary_) noexcept { self.temporary = temporary_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_created_at(this auto&& self, timestamp created_at_) noexcept { self.created_at = created_at_; return std::forward<decltype(self)>(self); }
    };
}

namespace voice {
    struct voice_state {
        opt<snowflake> guild_id{};
        opt<snowflake> channel_id{};
        snowflake user_id{};
        opt<guild::guild_member> member{};
        std::string session_id{};
        bool deaf{};
        bool mute{};
        bool self_deaf{};
        bool self_mute{};
        opt<bool> self_stream{};
        bool self_video{};
        bool suppress{};
        opt<timestamp> request_to_speak_timestamp{};

        static voice_state create(snowflake user_id_ = {}, std::string session_id_ = {}) {
            return voice_state{.user_id = user_id_, .session_id = std::move(session_id_)};
        }
        decltype(auto) set_guild_id(this auto&& self, opt<snowflake> guild_id_) noexcept { self.guild_id = guild_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_guild_id(id_); }
        decltype(auto) set_channel_id(this auto&& self, opt<snowflake> channel_id_) noexcept { self.channel_id = channel_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_channel(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_channel_id(id_); }
        decltype(auto) set_user_id(this auto&& self, snowflake user_id_) noexcept { self.user_id = user_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_user(this auto&& self, snowflake id_) noexcept { return std::forward<decltype(self)>(self).set_user_id(id_); }
        decltype(auto) set_member(this auto&& self, opt<guild::guild_member> member_) { self.member = std::move(member_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_session_id(this auto&& self, std::string session_id_) { self.session_id = std::move(session_id_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_deaf(this auto&& self, bool deaf_) noexcept { self.deaf = deaf_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_mute(this auto&& self, bool mute_) noexcept { self.mute = mute_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_self_deaf(this auto&& self, bool self_deaf_) noexcept { self.self_deaf = self_deaf_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_self_mute(this auto&& self, bool self_mute_) noexcept { self.self_mute = self_mute_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_self_stream(this auto&& self, opt<bool> self_stream_) noexcept { self.self_stream = self_stream_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_self_video(this auto&& self, bool self_video_) noexcept { self.self_video = self_video_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_suppress(this auto&& self, bool suppress_) noexcept { self.suppress = suppress_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_request_to_speak_timestamp(this auto&& self, opt<timestamp> request_to_speak_timestamp_) noexcept { self.request_to_speak_timestamp = request_to_speak_timestamp_; return std::forward<decltype(self)>(self); }
    };

    struct voice_region {
        std::string id{};
        std::string name{};
        bool optimal{};
        bool deprecated{};
        bool custom{};

        static voice_region create(std::string id_ = {}, std::string name_ = {}) {
            return voice_region{.id = std::move(id_), .name = std::move(name_)};
        }
        decltype(auto) set_id(this auto&& self, std::string id_) { self.id = std::move(id_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_name(this auto&& self, std::string name_) { self.name = std::move(name_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_optimal(this auto&& self, bool optimal_) noexcept { self.optimal = optimal_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_deprecated(this auto&& self, bool deprecated_) noexcept { self.deprecated = deprecated_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_custom(this auto&& self, bool custom_) noexcept { self.custom = custom_; return std::forward<decltype(self)>(self); }
    };
};

namespace stage_instance {
    enum class privacy_level : std::uint8_t {
        PUBLIC = 1, // The Stage instance is visible publicly. (deprecated)
        GUILD_ONLY = 2, // The Stage instance is visible to only guild members.
    };

    struct stage_instance {
        snowflake id{};
        snowflake guild_id{};
        snowflake channel_id{};
        std::string topic{};
        discusy::stage_instance::privacy_level privacy_level{};
        bool discoverable_disabled{};
        opt<snowflake> guild_scheduled_event_id{};

        static stage_instance create(snowflake id_ = {}, snowflake guild_id_ = {}, snowflake channel_id_ = {}, std::string topic_ = {}) {
            return stage_instance{
                .id = id_,
                .guild_id = guild_id_,
                .channel_id = channel_id_,
                .topic = std::move(topic_),
            };
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild_id(this auto&& self, snowflake guild_id_) noexcept { self.guild_id = guild_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild(this auto&& self, snowflake id_) noexcept { return std::forward<decltype(self)>(self).set_guild_id(id_); }
        decltype(auto) set_channel_id(this auto&& self, snowflake channel_id_) noexcept { self.channel_id = channel_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_channel(this auto&& self, snowflake id_) noexcept { return std::forward<decltype(self)>(self).set_channel_id(id_); }
        decltype(auto) set_topic(this auto&& self, std::string topic_) { self.topic = std::move(topic_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_privacy_level(this auto&& self, discusy::stage_instance::privacy_level privacy_level_) noexcept { self.privacy_level = privacy_level_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_discoverable_disabled(this auto&& self, bool discoverable_disabled_) noexcept { self.discoverable_disabled = discoverable_disabled_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild_scheduled_event_id(this auto&& self, opt<snowflake> guild_scheduled_event_id_) noexcept { self.guild_scheduled_event_id = guild_scheduled_event_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild_scheduled_event(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_guild_scheduled_event_id(id_); }
    };
};

namespace subscription {
    enum class subscription_status : std::uint8_t {
        ACTIVE = 0, // Subscription is active and scheduled to renew.
        INACTIVE = 1, // Subscription is inactive and not being charged.
        ENDING = 2, // Subscription is active but will not renew.
    };

    struct subscription {
        snowflake id{};
        snowflake user_id{};
        std::vector<snowflake> sku_ids{};
        std::vector<snowflake> entitlement_ids{};
        opt<std::vector<snowflake>> renewal_sku_ids{};
        timestamp current_period_start{};
        timestamp current_period_end{};
        subscription_status status{};
        opt<timestamp> canceled_at{};
        opt<std::string> country{};

        static subscription create(snowflake id_ = {}, snowflake user_id_ = {}, subscription_status status_ = subscription_status::ACTIVE) noexcept {
            return subscription{.id = id_, .user_id = user_id_, .status = status_};
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_user_id(this auto&& self, snowflake user_id_) noexcept { self.user_id = user_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_user(this auto&& self, snowflake id_) noexcept { return std::forward<decltype(self)>(self).set_user_id(id_); }
        decltype(auto) set_sku_ids(this auto&& self, std::vector<snowflake> sku_ids_) { self.sku_ids = std::move(sku_ids_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_sku_ids, snowflake)
        decltype(auto) add_sku_id(this auto&& self, snowflake itm) noexcept { self.sku_ids.emplace_back(itm); return std::forward<decltype(self)>(self); }
        decltype(auto) set_entitlement_ids(this auto&& self, std::vector<snowflake> entitlement_ids_) { self.entitlement_ids = std::move(entitlement_ids_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_entitlement_ids, snowflake)
        decltype(auto) add_entitlement_id(this auto&& self, snowflake itm) noexcept { self.entitlement_ids.emplace_back(itm); return std::forward<decltype(self)>(self); }
        decltype(auto) set_renewal_sku_ids(this auto&& self, opt<std::vector<snowflake>> renewal_sku_ids_) { self.renewal_sku_ids = std::move(renewal_sku_ids_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_renewal_sku_ids, snowflake)
        decltype(auto) add_renewal_sku_id(this auto&& self, snowflake itm) {
            if (!self.renewal_sku_ids) self.renewal_sku_ids.emplace();
            self.renewal_sku_ids->emplace_back(itm);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_current_period_start(this auto&& self, timestamp current_period_start_) noexcept { self.current_period_start = current_period_start_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_current_period_end(this auto&& self, timestamp current_period_end_) noexcept { self.current_period_end = current_period_end_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_status(this auto&& self, subscription_status status_) noexcept { self.status = status_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_canceled_at(this auto&& self, opt<timestamp> canceled_at_) noexcept { self.canceled_at = canceled_at_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_country(this auto&& self, opt<std::string> country_) { self.country = std::move(country_); return std::forward<decltype(self)>(self); }
    };
};

namespace poll {
    struct poll_media {
        opt<std::string> text{}; // ????? unsure
        opt<emoji::emoji> emoji{}; // ?? are the docs wrong?

        static poll_media create(opt<std::string> text_ = {}, opt<emoji::emoji> emoji_ = {}) {
            return poll_media{.text = std::move(text_), .emoji = std::move(emoji_)};
        }
        decltype(auto) set_text(this auto&& self, opt<std::string> text_) { self.text = std::move(text_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_emoji(this auto&& self, opt<emoji::emoji> emoji_) { self.emoji = std::move(emoji_); return std::forward<decltype(self)>(self); }
    };

    struct poll_answer {
        opt<integer> answer_id{}; // only sent in responses from the API/Gateway, never in create requests
        discusy::poll::poll_media poll_media{};

        static poll_answer create(discusy::poll::poll_media poll_media_ = {}, opt<integer> answer_id_ = {}) {
            return poll_answer{
                .answer_id = answer_id_,
                .poll_media = std::move(poll_media_),
            };
        }
        decltype(auto) set_answer_id(this auto&& self, opt<integer> answer_id_) noexcept { self.answer_id = answer_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_poll_media(this auto&& self, discusy::poll::poll_media poll_media_) { self.poll_media = std::move(poll_media_); return std::forward<decltype(self)>(self); }
    };

    enum class layout_type : std::uint8_t {
        DEFAULT = 1, // The, uhm, default layout type.  
    };

    struct poll_answer_count {
        integer id{};
        integer count{};
        bool me_voted{};

        static poll_answer_count create(integer id_ = 0, integer count_ = 0, bool me_voted_ = false) noexcept {
            return poll_answer_count{.id = id_, .count = count_, .me_voted = me_voted_};
        }
        decltype(auto) set_id(this auto&& self, integer id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_count(this auto&& self, integer count_) noexcept { self.count = count_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_me_voted(this auto&& self, bool me_voted_) noexcept { self.me_voted = me_voted_; return std::forward<decltype(self)>(self); }
    };

    struct poll_results {
        bool is_finalized{};
        std::vector<poll_answer_count> answer_counts{};

        static poll_results create(bool is_finalized_ = false, std::vector<poll_answer_count> answer_counts_ = {}) {
            return poll_results{
                .is_finalized = is_finalized_,
                .answer_counts = std::move(answer_counts_),
            };
        }
        decltype(auto) set_is_finalized(this auto&& self, bool is_finalized_) noexcept { self.is_finalized = is_finalized_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_answer_counts(this auto&& self, std::vector<poll_answer_count> answer_counts_) { self.answer_counts = std::move(answer_counts_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_answer_counts, poll_answer_count)
        decltype(auto) add_answer_count(this auto&& self, poll_answer_count itm) { self.answer_counts.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
    };

    struct poll {
        poll_media question{};
        std::vector<poll_answer> answers{};
        opt<timestamp> expiry{};
        bool allow_multiselect{};
        discusy::poll::layout_type layout_type{};
        opt<poll_results> results{};

        static poll create(poll_media question_ = {}, std::vector<poll_answer> answers_ = {}) {
            return poll{.question = std::move(question_), .answers = std::move(answers_)};
        }
        decltype(auto) set_question(this auto&& self, poll_media question_) { self.question = std::move(question_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_answers(this auto&& self, std::vector<poll_answer> answers_) { self.answers = std::move(answers_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_answers, poll_answer)
        decltype(auto) add_answer(this auto&& self, poll_answer itm) { self.answers.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_expiry(this auto&& self, opt<timestamp> expiry_) noexcept { self.expiry = expiry_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_allow_multiselect(this auto&& self, bool allow_multiselect_) noexcept { self.allow_multiselect = allow_multiselect_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_layout_type(this auto&& self, discusy::poll::layout_type layout_type_) noexcept { self.layout_type = layout_type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_results(this auto&& self, opt<poll_results> results_) { self.results = std::move(results_); return std::forward<decltype(self)>(self); }
    };

    struct poll_create_request {
        poll_media question{};
        std::vector<poll_answer> answers{};
        opt<integer> duration{};
        opt<bool> allow_multiselect{};
        opt<discusy::poll::layout_type> layout_type{};

        static poll_create_request create(poll_media question_ = {}, std::vector<poll_answer> answers_ = {}) {
            return poll_create_request{
                .question = std::move(question_),
                .answers = std::move(answers_),
            };
        }
        decltype(auto) set_question(this auto&& self, poll_media question_) { self.question = std::move(question_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_answers(this auto&& self, std::vector<poll_answer> answers_) { self.answers = std::move(answers_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_answers, poll_answer)
        decltype(auto) add_answer(this auto&& self, poll_answer itm) { self.answers.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_duration(this auto&& self, opt<integer> duration_) noexcept { self.duration = duration_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_allow_multiselect(this auto&& self, opt<bool> allow_multiselect_) noexcept { self.allow_multiselect = allow_multiselect_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_layout_type(this auto&& self, opt<discusy::poll::layout_type> layout_type_) noexcept { self.layout_type = layout_type_; return std::forward<decltype(self)>(self); }
    };
};

namespace message {
    enum class attachment_flags : std::uint8_t {
        IS_CLIP = 1ULL << 0, // this attachment is a Clip from a stream
        IS_THUMBNAIL = 1ULL << 1, // this attachment is the thumbnail of a thread in a media channel, displayed in the grid but not on the message
        IS_REMIX = 1ULL << 2, // this attachment has been edited using the remix feature on mobile (deprecated)
        IS_SPOILER = 1ULL << 3, // this attachment was marked as a spoiler and is blurred until clicked
        IS_ANIMATED = 1ULL << 5, // this attachment is an animated image
    };


    struct attachment {
        snowflake id{};
        std::string filename{};
        opt<std::string> title{};
        opt<std::string> content_type{};
        integer size{};
        std::string url{};
        std::string proxy_url{};
        opt<integer> height{};
        opt<integer> width{};
        opt<std::string> placeholder{};
        opt<integer> placeholder_version{};
        opt<bool> ephemeral{};
        opt<float> duration_secs{};
        opt<std::string> waveform{}; // base64 encoded bytearray representing a sampled waveform (currently for voice messages)
        opt<flags_t<attachment_flags>> flags{};
        opt<std::vector<user::user>> clip_participants{};
        opt<timestamp> clip_created_at{};
        opt<application::application> application{};

        static attachment create(snowflake id_ = {}, std::string filename_ = {}) {
            return attachment{.id = id_, .filename = std::move(filename_)};
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_filename(this auto&& self, std::string filename_) { self.filename = std::move(filename_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_title(this auto&& self, opt<std::string> title_) { self.title = std::move(title_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_content_type(this auto&& self, opt<std::string> content_type_) { self.content_type = std::move(content_type_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_size(this auto&& self, integer size_) noexcept { self.size = size_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_url(this auto&& self, std::string url_) { self.url = std::move(url_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_proxy_url(this auto&& self, std::string proxy_url_) { self.proxy_url = std::move(proxy_url_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_height(this auto&& self, opt<integer> height_) noexcept { self.height = height_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_width(this auto&& self, opt<integer> width_) noexcept { self.width = width_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_placeholder(this auto&& self, opt<std::string> placeholder_) { self.placeholder = std::move(placeholder_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_placeholder_version(this auto&& self, opt<integer> placeholder_version_) noexcept { self.placeholder_version = placeholder_version_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_ephemeral(this auto&& self, opt<bool> ephemeral_) noexcept { self.ephemeral = ephemeral_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_duration_secs(this auto&& self, opt<float> duration_secs_) noexcept { self.duration_secs = duration_secs_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_waveform(this auto&& self, opt<std::string> waveform_) { self.waveform = std::move(waveform_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_flags(this auto&& self, opt<flags_t<attachment_flags>> flags_) noexcept { self.flags = flags_; return std::forward<decltype(self)>(self); }
        decltype(auto) add_flags(this auto&& self, attachment_flags f) noexcept {
            if (!self.flags) self.flags.emplace();
            self.flags->add_flags(f);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) add_flag(this auto&& self, attachment_flags f) noexcept { return std::forward<decltype(self)>(self).add_flags(f); }
        decltype(auto) set_clip_participants(this auto&& self, opt<std::vector<user::user>> clip_participants_) { self.clip_participants = std::move(clip_participants_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_clip_participants, user::user)
        decltype(auto) add_clip_participant(this auto&& self, user::user itm) {
            if (!self.clip_participants) self.clip_participants.emplace();
            self.clip_participants->emplace_back(std::move(itm));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_clip_created_at(this auto&& self, opt<timestamp> clip_created_at_) noexcept { self.clip_created_at = clip_created_at_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_application(this auto&& self, opt<application::application> application_) { self.application = std::move(application_); return std::forward<decltype(self)>(self); }
    };

    enum class embed_type : std::uint8_t { // make sure to update glaze meta
        rich, // generic embed rendered from embed attributes
        image, // image embed
        video, // video embed
        gifv, // animated gif image embed rendered as a video embed
        article, // article embed
        link, // link embed
        poll_result, // poll result embed
    };

    enum class embed_media_flags : std::uint8_t {
        IS_ANIMATED = 1ULL << 5, // this image is animated
    };

    struct embed_footer {
        std::string text{};
        opt<std::string> icon_url{};
        opt<std::string> proxy_icon_url{};

        static embed_footer create(std::string text_ = {}, opt<std::string> icon_url_ = {}) {
            return embed_footer{
                .text = std::move(text_),
                .icon_url = std::move(icon_url_),
            };
        }
        decltype(auto) set_text(this auto&& self, std::string text_) { self.text = std::move(text_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_icon_url(this auto&& self, opt<std::string> icon_url_) { self.icon_url = std::move(icon_url_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_proxy_icon_url(this auto&& self, opt<std::string> proxy_icon_url_) { self.proxy_icon_url = std::move(proxy_icon_url_); return std::forward<decltype(self)>(self); }
    };

    struct embed_image {
        std::string url{};
        opt<std::string> proxy_url{};
        opt<integer> height{};
        opt<integer> width{};
        opt<std::string> content_type{};
        opt<std::string> placeholder{};
        opt<integer> placeholder_version{};
        opt<std::string> description{};
        opt<flags_t<embed_media_flags>> flags{};

        static embed_image create(std::string url_ = {}) {
            return embed_image{.url = std::move(url_)};
        }
        decltype(auto) set_url(this auto&& self, std::string url_) { self.url = std::move(url_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_proxy_url(this auto&& self, opt<std::string> proxy_url_) { self.proxy_url = std::move(proxy_url_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_height(this auto&& self, opt<integer> height_) noexcept { self.height = height_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_width(this auto&& self, opt<integer> width_) noexcept { self.width = width_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_content_type(this auto&& self, opt<std::string> content_type_) { self.content_type = std::move(content_type_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_placeholder(this auto&& self, opt<std::string> placeholder_) { self.placeholder = std::move(placeholder_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_placeholder_version(this auto&& self, opt<integer> placeholder_version_) noexcept { self.placeholder_version = placeholder_version_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_description(this auto&& self, opt<std::string> description_) { self.description = std::move(description_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_flags(this auto&& self, opt<flags_t<embed_media_flags>> flags_) noexcept { self.flags = flags_; return std::forward<decltype(self)>(self); }
        decltype(auto) add_flags(this auto&& self, embed_media_flags f) noexcept {
            if (!self.flags) self.flags.emplace();
            self.flags->add_flags(f);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) add_flag(this auto&& self, embed_media_flags f) noexcept { return std::forward<decltype(self)>(self).add_flags(f); }
    };

    struct embed_video {
        opt<std::string> url{};
        opt<std::string> proxy_url{};
        opt<integer> height{};
        opt<integer> width{};
        opt<std::string> content_type{};
        opt<std::string> placeholder{};
        opt<integer> placeholder_version{};
        opt<std::string> description{};
        opt<flags_t<embed_media_flags>> flags{};

        static embed_video create(opt<std::string> url_ = {}) {
            return embed_video{.url = std::move(url_)};
        }
        decltype(auto) set_url(this auto&& self, opt<std::string> url_) { self.url = std::move(url_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_proxy_url(this auto&& self, opt<std::string> proxy_url_) { self.proxy_url = std::move(proxy_url_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_height(this auto&& self, opt<integer> height_) noexcept { self.height = height_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_width(this auto&& self, opt<integer> width_) noexcept { self.width = width_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_content_type(this auto&& self, opt<std::string> content_type_) { self.content_type = std::move(content_type_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_placeholder(this auto&& self, opt<std::string> placeholder_) { self.placeholder = std::move(placeholder_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_placeholder_version(this auto&& self, opt<integer> placeholder_version_) noexcept { self.placeholder_version = placeholder_version_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_description(this auto&& self, opt<std::string> description_) { self.description = std::move(description_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_flags(this auto&& self, opt<flags_t<embed_media_flags>> flags_) noexcept { self.flags = flags_; return std::forward<decltype(self)>(self); }
        decltype(auto) add_flags(this auto&& self, embed_media_flags f) noexcept {
            if (!self.flags) self.flags.emplace();
            self.flags->add_flags(f);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) add_flag(this auto&& self, embed_media_flags f) noexcept { return std::forward<decltype(self)>(self).add_flags(f); }
    };

    struct embed_provider {
        opt<std::string> name{};
        opt<std::string> url{};

        static embed_provider create(opt<std::string> name_ = {}, opt<std::string> url_ = {}) {
            return embed_provider{.name = std::move(name_), .url = std::move(url_)};
        }
        decltype(auto) set_name(this auto&& self, opt<std::string> name_) { self.name = std::move(name_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_url(this auto&& self, opt<std::string> url_) { self.url = std::move(url_); return std::forward<decltype(self)>(self); }
    };

    struct embed_author {
        std::string name{};
        opt<std::string> url{};
        opt<std::string> icon_url{};
        opt<std::string> proxy_icon_url{};

        static embed_author create(std::string name_ = {}, opt<std::string> url_ = {}, opt<std::string> icon_url_ = {}) {
            return embed_author{
                .name = std::move(name_),
                .url = std::move(url_),
                .icon_url = std::move(icon_url_)
            };
        }
        decltype(auto) set_name(this auto&& self, std::string name_) { self.name = std::move(name_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_url(this auto&& self, opt<std::string> url_) { self.url = std::move(url_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_icon_url(this auto&& self, opt<std::string> icon_url_) { self.icon_url = std::move(icon_url_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_proxy_icon_url(this auto&& self, opt<std::string> proxy_icon_url_) { self.proxy_icon_url = std::move(proxy_icon_url_); return std::forward<decltype(self)>(self); }
    };

    struct embed_field {
        std::string name{};
        std::string value{};
        opt<bool> inline_{};

        static embed_field create(std::string name_ = {}, std::string value_ = {}, opt<bool> inline_l = {}) {
            return embed_field{
                .name = std::move(name_),
                .value = std::move(value_),
                .inline_ = inline_l,
            };
        }
        decltype(auto) set_name(this auto&& self, std::string name_) { self.name = std::move(name_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_value(this auto&& self, std::string value_) { self.value = std::move(value_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_inline_(this auto&& self, opt<bool> inline_l) noexcept { self.inline_ = inline_l; return std::forward<decltype(self)>(self); }
        decltype(auto) set_inline(this auto&& self, opt<bool> inline_l) noexcept { self.inline_ = inline_l; return std::forward<decltype(self)>(self); }

        struct glaze {
            static consteval std::string_view rename_key(const std::string_view key) {
                if (key == "inline_") return "inline";
                return key;
            };
        };
    };

    enum class embed_flags : std::uint8_t {
        IS_CONTENT_INVENTORY_ENTRY = 1ULL << 5, // this embed is a fallback for a reply to an activity card
    };

    struct embed {
        opt<std::string> title{};
        opt<embed_type> type{};
        opt<std::string> description{};
        opt<std::string> url{};
        opt<discusy::timestamp> timestamp{};
        opt<integer> color{};
        opt<embed_footer> footer{};
        opt<embed_image> image{};
        opt<embed_image> thumbnail{};
        opt<embed_video> video{};
        opt<embed_provider> provider{};
        opt<embed_author> author{};
        opt<std::vector<embed_field>> fields{};
        opt<flags_t<embed_flags>> flags{};

        static embed create(opt<std::string> title_ = {}, opt<std::string> description_ = {}) {
            return embed{
                .title = std::move(title_),
                .description = std::move(description_),
            };
        }
        decltype(auto) set_title(this auto&& self, opt<std::string> title_) { self.title = std::move(title_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_type(this auto&& self, opt<embed_type> type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_description(this auto&& self, opt<std::string> description_) { self.description = std::move(description_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_url(this auto&& self, opt<std::string> url_) { self.url = std::move(url_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_timestamp(this auto&& self, opt<discusy::timestamp> timestamp_) noexcept { self.timestamp = timestamp_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_color(this auto&& self, opt<integer> color_) noexcept { self.color = color_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_footer(this auto&& self, opt<embed_footer> footer_) { self.footer = std::move(footer_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_image(this auto&& self, opt<embed_image> image_) { self.image = std::move(image_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_thumbnail(this auto&& self, opt<embed_image> thumbnail_) { self.thumbnail = std::move(thumbnail_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_video(this auto&& self, opt<embed_video> video_) { self.video = std::move(video_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_provider(this auto&& self, opt<embed_provider> provider_) { self.provider = std::move(provider_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_author(this auto&& self, opt<embed_author> author_) { self.author = std::move(author_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_fields(this auto&& self, opt<std::vector<embed_field>> fields_) { self.fields = std::move(fields_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_fields, embed_field)
        decltype(auto) add_field(this auto&& self, embed_field itm) {
            if (!self.fields) self.fields.emplace();
            self.fields->emplace_back(std::move(itm));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_flags(this auto&& self, opt<flags_t<embed_flags>> flags_) noexcept { self.flags = flags_; return std::forward<decltype(self)>(self); }
        decltype(auto) add_flags(this auto&& self, embed_flags f) noexcept {
            if (!self.flags) self.flags.emplace();
            self.flags->add_flags(f);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) add_flag(this auto&& self, embed_flags f) noexcept { return std::forward<decltype(self)>(self).add_flags(f); }
        decltype(auto) add_field(this auto&& self, std::string name_, std::string value_, opt<bool> inline__ = {}) {
            return std::forward<decltype(self)>(self).add_field(embed_field{.name = std::move(name_), .value = std::move(value_), .inline_ = inline__});
        }
        decltype(auto) set_footer(this auto&& self, std::string text) {
            self.footer = embed_footer{.text = std::move(text)};
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_author(this auto&& self, std::string name, std::string icon_url = {}) {
            embed_author a{.name = std::move(name)};
            if (!icon_url.empty()) a.icon_url = std::move(icon_url);
            self.author = std::move(a);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_thumbnail(this auto&& self, std::string url) {
            self.thumbnail = embed_image{.url = std::move(url)};
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_image(this auto&& self, std::string url) {
            self.image = embed_image{.url = std::move(url)};
            return std::forward<decltype(self)>(self);
        }
    };

    struct reaction_count_details {
        integer burst{};
        integer normal{};

        static reaction_count_details create(integer burst_ = 0, integer normal_ = 0) noexcept {
            return reaction_count_details{.burst = burst_, .normal = normal_};
        }
        decltype(auto) set_burst(this auto&& self, integer burst_) noexcept { self.burst = burst_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_normal(this auto&& self, integer normal_) noexcept { self.normal = normal_; return std::forward<decltype(self)>(self); }
    };

    struct reaction {
        integer count{};
        reaction_count_details count_details{};
        bool me{};
        bool me_burst{};
        emoji::emoji emoji{};
        std::vector<std::string> burst_colors{}; // i assume string if they say hex

        static reaction create(integer count_ = 0, emoji::emoji emoji_ = {}, bool me_ = false) {
            return reaction{.count = count_, .me = me_, .emoji = std::move(emoji_)};
        }
        decltype(auto) set_count(this auto&& self, integer count_) noexcept { self.count = count_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_count_details(this auto&& self, reaction_count_details count_details_) noexcept { self.count_details = count_details_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_me(this auto&& self, bool me_) noexcept { self.me = me_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_me_burst(this auto&& self, bool me_burst_) noexcept { self.me_burst = me_burst_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_emoji(this auto&& self, emoji::emoji emoji_) { self.emoji = std::move(emoji_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_burst_colors(this auto&& self, std::vector<std::string> burst_colors_) { self.burst_colors = std::move(burst_colors_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_burst_colors, std::string)
        decltype(auto) add_burst_color(this auto&& self, std::string itm) { self.burst_colors.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
    };

    enum class message_type : std::uint8_t { // last bool is deletable
        DEFAULT = 0, // true
        RECIPIENT_ADD = 1, // false
        RECIPIENT_REMOVE = 2, // false
        CALL = 3, // false
        CHANNEL_NAME_CHANGE = 4, // false
        CHANNEL_ICON_CHANGE = 5, // false
        CHANNEL_PINNED_MESSAGE = 6, // true
        USER_JOIN = 7, // true
        GUILD_BOOST = 8, // true
        GUILD_BOOST_TIER_1 = 9, // true
        GUILD_BOOST_TIER_2 = 10, // true
        GUILD_BOOST_TIER_3 = 11, // true
        CHANNEL_FOLLOW_ADD = 12, // true
        GUILD_DISCOVERY_DISQUALIFIED = 14, // true
        GUILD_DISCOVERY_REQUALIFIED = 15, // true
        GUILD_DISCOVERY_GRACE_PERIOD_INITIAL_WARNING = 16, // true
        GUILD_DISCOVERY_GRACE_PERIOD_FINAL_WARNING = 17, // true
        THREAD_CREATED = 18, // true
        REPLY = 19, // true
        CHAT_INPUT_COMMAND = 20, // true
        THREAD_STARTER_MESSAGE = 21, // false
        GUILD_INVITE_REMINDER = 22, // true
        CONTEXT_MENU_COMMAND = 23, // true
        AUTO_MODERATION_ACTION = 24, // true*
        ROLE_SUBSCRIPTION_PURCHASE = 25, // true
        INTERACTION_PREMIUM_UPSELL = 26, // true
        STAGE_START = 27, // true
        STAGE_END = 28, // true
        STAGE_SPEAKER = 29, // true
        STAGE_TOPIC = 31, // true
        GUILD_APPLICATION_PREMIUM_SUBSCRIPTION = 32, // true
        GUILD_INCIDENT_ALERT_MODE_ENABLED = 36, // true
        GUILD_INCIDENT_ALERT_MODE_DISABLED = 37, // true
        GUILD_INCIDENT_REPORT_RAID = 38, // true
        GUILD_INCIDENT_REPORT_FALSE_ALARM = 39, // true
        PURCHASE_NOTIFICATION = 44, // true
        POLL_RESULT = 46, // true
    };

    enum class message_activity_type : std::uint8_t {
        JOIN = 1,
        SPECTATE = 2,
        LISTEN = 3,
        JOIN_REQUEST = 5,
        STREAM_REQUEST = 6,
    };

    struct message_activity {
        message_activity_type type{};
        opt<std::string> party_id{};

        static message_activity create(message_activity_type type_ = message_activity_type::JOIN, opt<std::string> party_id_ = {}) {
            return message_activity{.type = type_, .party_id = std::move(party_id_)};
        }
        decltype(auto) set_type(this auto&& self, message_activity_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_party_id(this auto&& self, opt<std::string> party_id_) { self.party_id = std::move(party_id_); return std::forward<decltype(self)>(self); }
    };

    enum class message_flags : std::uint16_t {
        CROSSPOSTED = 1ULL << 0, // this message has been published to subscribed channels (via Channel Following)
        IS_CROSSPOST = 1ULL << 1, // this message originated from a message in another channel (via Channel Following)
        SUPPRESS_EMBEDS = 1ULL << 2, // do not include any embeds when serializing this message
        SOURCE_MESSAGE_DELETED = 1ULL << 3, // the source message for this crosspost has been deleted (via Channel Following)
        URGENT = 1ULL << 4, // this message came from the urgent message system
        HAS_THREAD = 1ULL << 5, // this message has an associated thread, with the same id as the message
        EPHEMERAL = 1ULL << 6, // this message is only visible to the user who invoked the Interaction
        LOADING = 1ULL << 7, // this message is an Interaction Response and the bot is “thinking”
        FAILED_TO_MENTION_SOME_ROLES_IN_THREAD = 1ULL << 8, // this message failed to mention some roles and add their members to the thread
        SUPPRESS_NOTIFICATIONS = 1ULL << 12, // this message will not trigger push and desktop notifications
        IS_VOICE_MESSAGE = 1ULL << 13, // this message is a voice message
        HAS_SNAPSHOT = 1ULL << 14, // this message has a snapshot (via Message Forwarding)
        IS_COMPONENTS_V2 = 1ULL << 15, // allows you to create fully component-driven messages
    };

    enum class message_reference_type : std::uint8_t {
        DEFAULT = 0, // referenced_message	A standard reference used by replies.
        FORWARD = 1, // message_snapshot	Reference used to point to a message at a point in time.
    };

    struct message_reference {
        message_reference_type type{message_reference_type::DEFAULT}; // optional but given the default i think its fine?
        opt<snowflake> message_id{};
        opt<snowflake> channel_id{};
        opt<snowflake> guild_id{};
        opt<bool> fail_if_not_exists{};

        static message_reference create(opt<snowflake> message_id_ = {}, opt<snowflake> channel_id_ = {}, opt<snowflake> guild_id_ = {}) noexcept {
            return message_reference{
                .message_id = message_id_,
                .channel_id = channel_id_,
                .guild_id = guild_id_,
            };
        }
        decltype(auto) set_type(this auto&& self, message_reference_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_message_id(this auto&& self, opt<snowflake> message_id_) noexcept { self.message_id = message_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_message(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_message_id(id_); }
        decltype(auto) set_channel_id(this auto&& self, opt<snowflake> channel_id_) noexcept { self.channel_id = channel_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_channel(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_channel_id(id_); }
        decltype(auto) set_guild_id(this auto&& self, opt<snowflake> guild_id_) noexcept { self.guild_id = guild_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_guild_id(id_); }
        decltype(auto) set_fail_if_not_exists(this auto&& self, opt<bool> fail_if_not_exists_) noexcept { self.fail_if_not_exists = fail_if_not_exists_; return std::forward<decltype(self)>(self); }
    };

    struct message;
}

namespace interaction {
    struct resolved {
        opt<std::unordered_map<decltype(snowflake::value), user::user>> users{};
        opt<std::unordered_map<decltype(snowflake::value), guild::guild_member>> members{};
        opt<std::unordered_map<decltype(snowflake::value), permissions::role>> roles{};
        opt<std::unordered_map<decltype(snowflake::value), channel::channel>> channels{};
        opt<std::unordered_map<decltype(snowflake::value), message::message>> messages{};
        opt<std::unordered_map<decltype(snowflake::value), message::attachment>> attachments{};
    };
}

namespace components {
    #pragma push_macro("COMPONENTS")
    #undef COMPONENTS
    #pragma push_macro("DISCUSY_X")
    #undef DISCUSY_X
    #pragma push_macro("DISCUSY_D")
    #undef DISCUSY_D
    #define DISCUSY_D ,
    
    #define COMPONENTS \
        DISCUSY_X(ActionRow, 1) DISCUSY_D \
        DISCUSY_X(Button, 2) DISCUSY_D \
        DISCUSY_X(StringSelect, 3) DISCUSY_D \
        DISCUSY_X(TextInput, 4) DISCUSY_D \
        DISCUSY_X(UserSelect, 5) DISCUSY_D \
        DISCUSY_X(RoleSelect, 6) DISCUSY_D \
        DISCUSY_X(MentionableSelect, 7) DISCUSY_D \
        DISCUSY_X(ChannelSelect, 8) DISCUSY_D \
        DISCUSY_X(Section, 9) DISCUSY_D \
        DISCUSY_X(TextDisplay, 10) DISCUSY_D \
        DISCUSY_X(Thumbnail, 11) DISCUSY_D \
        DISCUSY_X(MediaGallery, 12) DISCUSY_D \
        DISCUSY_X(File, 13) DISCUSY_D \
        DISCUSY_X(Separator, 14) DISCUSY_D \
        DISCUSY_X(Container, 17) DISCUSY_D \
        DISCUSY_X(Label, 18) DISCUSY_D \
        DISCUSY_X(FileUpload, 19) DISCUSY_D \
        DISCUSY_X(RadioGroup, 21) DISCUSY_D \
        DISCUSY_X(CheckboxGroup, 22) DISCUSY_D \
        DISCUSY_X(Checkbox, 23)

    /**
        // ActionRow = 1, // Container to display a row of interactive components	Layout	Message
        // Button = 2, // Button object	Interactive	Message
        // StringSelect = 3, // Select menu for picking from defined text options	Interactive	Message, Modal
        // TextInput = 4, // Text input object	Interactive	Modal
        // UserSelect = 5, // Select menu for users	Interactive	Message, Modal
        // RoleSelect = 6, // Select menu for roles	Interactive	Message, Modal
        // MentionableSelect = 7, // Select menu for mentionables (users and roles)	Interactive	Message, Modal
        // ChannelSelect = 8, // elect menu for channels	Interactive	Message, Modal
        // Section = 9, // Container to display text alongside an accessory component	Layout	Message
        // TextDisplay = 10, // Markdown text	Content	Message, Modal
        // Thumbnail = 11, // Small image that can be used as an accessory	Content	Message
        // MediaGallery = 12, // Display images and other media	Content	Message
        // File = 13, // Displays an attached file	Content	Message
        // Separator = 14, // Component to add vertical padding between other components	Layout	Message
        // Container = 17, // Container that visually groups a set of components	Layout	Message
        // Label = 18, // Container associating a label and description with a component	Layout	Modal
        // FileUpload = 19, // Component for uploading files	Interactive	Modal
        // RadioGroup = 21, // Single-choice set of options	Interactive	Modal
        // CheckboxGroup = 22, // Multi-selectable group of checkboxes	Interactive	Modal
        // Checkbox = 23, // Single checkbox for yes/no choice	Interactive	Modal
    */
    enum class component_type : std::uint8_t {
        #define DISCUSY_X(name, num) name = num
        COMPONENTS
        #undef DISCUSY_X
        ,
    };

    struct type_extractor {
        component_type type{};
    };

    struct ir_type_extractor {
        opt<components::component_type> type{};
        opt<components::component_type> component_type{};
    };

    enum class button_style : std::uint8_t {
        Primary = 1, // The most important or recommended action in a group of options	custom_id
        Secondary = 2, // Alternative or supporting actions	custom_id
        Success = 3, // Positive confirmation or completion actions	custom_id
        Danger = 4, // An action with irreversible consequences	custom_id
        Link = 5, // Navigates to a URL	url
        Premium = 6, // Purchase	sku_id
    };

    enum class text_input_style : std::uint8_t {
        Short = 1, // Single-line input
        Paragraph = 2, // Multi-line input
    };

    enum class select_default_value_type : std::uint8_t { // HAS GLAZE META
        user,
        role,
        channel,
    };

    enum class unfurled_media_item_flags : std::uint8_t {
        IS_ANIMATED = 1ULL << 0, // This image is animated
    };

    enum class separator_spacing : std::uint8_t {
        small = 1,
        large = 2,
    };

    template <typename Derived, typename... Ts>
    struct component_variant : std::variant<Ts...> {
        using variant_base = std::variant<Ts...>;
        using variant_base::variant_base;
        using variant_base::operator=;

        template <typename T>
        [[nodiscard]] constexpr bool is(this const auto& self) noexcept {
            return std::holds_alternative<T>(self);
        }

        template <typename T>
        [[nodiscard]] constexpr auto* get_if(this auto&& self) noexcept {
            return std::get_if<T>(&self);
        }

        template <typename T>
        [[nodiscard]] constexpr decltype(auto) get(this auto&& self) {
            return std::get<T>(std::forward<decltype(self)>(self));
        }

        template <typename Visitor>
        constexpr decltype(auto) visit(this auto&& self, Visitor&& visitor) {
            return std::visit(std::forward<Visitor>(visitor), std::forward<decltype(self)>(self));
        }

        [[nodiscard]] constexpr component_type get_type(this const auto& self) noexcept {
            return self.visit([](const auto& elem) noexcept -> component_type {
                return elem.type;
            });
        }

        template <typename OtherDerived, typename... OtherTs>
            requires (!std::is_same_v<Derived, OtherDerived> && (std::is_constructible_v<variant_base, OtherTs> && ...))
        constexpr component_variant(const component_variant<OtherDerived, OtherTs...>& other) {
            other.visit([this](const auto& val) {
                this->template emplace<std::decay_t<decltype(val)>>(val);
            });
        }

        template <typename OtherDerived, typename... OtherTs>
            requires (!std::is_same_v<Derived, OtherDerived> && (std::is_constructible_v<variant_base, OtherTs> && ...))
        constexpr component_variant(component_variant<OtherDerived, OtherTs...>&& other) {
            std::move(other).visit([this](auto&& val) {
                this->template emplace<std::decay_t<decltype(val)>>(std::forward<decltype(val)>(val));
            });
        }

        template <typename OtherDerived, typename... OtherTs>
            requires (!std::is_same_v<Derived, OtherDerived> && (std::is_constructible_v<variant_base, OtherTs> && ...))
        constexpr decltype(auto) operator=(this auto&& self, const component_variant<OtherDerived, OtherTs...>& other) {
            other.visit([&self](const auto& val) {
                self.template emplace<std::decay_t<decltype(val)>>(val);
            });
            return std::forward<decltype(self)>(self);
        }

        template <typename OtherDerived, typename... OtherTs>
            requires (!std::is_same_v<Derived, OtherDerived> && (std::is_constructible_v<variant_base, OtherTs> && ...))
        constexpr decltype(auto) operator=(this auto&& self, component_variant<OtherDerived, OtherTs...>&& other) {
            std::move(other).visit([&self](auto&& val) {
                self.template emplace<std::decay_t<decltype(val)>>(std::forward<decltype(val)>(val));
            });
            return std::forward<decltype(self)>(self);
        }

        [[nodiscard]] constexpr bool operator==(this const auto& self, const component_variant& other)
            requires (std::equality_comparable<Ts> && ...)
        {
            return static_cast<const variant_base&>(self) == static_cast<const variant_base&>(other);
        }

        [[nodiscard]] constexpr auto operator<=>(this const auto& self, const component_variant& other)
            requires (std::three_way_comparable<Ts> && ...)
        {
            return static_cast<const variant_base&>(self) <=> static_cast<const variant_base&>(other);
        }

        bool operator<(this const auto&, const component_variant&) = delete;
        bool operator<=(this const auto&, const component_variant&) = delete;
        bool operator>(this const auto&, const component_variant&) = delete;
        bool operator>=(this const auto&, const component_variant&) = delete;

        friend bool operator<(const component_variant&, const component_variant&) = delete;
        friend bool operator<=(const component_variant&, const component_variant&) = delete;
        friend bool operator>(const component_variant&, const component_variant&) = delete;
        friend bool operator>=(const component_variant&, const component_variant&) = delete;
    };

    struct CheckboxInteractionResponse {
        component_type type{component_type::Checkbox};
        std::int32_t id{};
        std::string custom_id{};
        bool value{};

        static CheckboxInteractionResponse create(std::int32_t id_ = 0, std::string custom_id_ = {}, bool value_ = false) {
            return CheckboxInteractionResponse{
                .id = id_,
                .custom_id = std::move(custom_id_),
                .value = value_,
            };
        }
        decltype(auto) set_type(this auto&& self, component_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_id(this auto&& self, std::int32_t id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_custom_id(this auto&& self, std::string custom_id_) { self.custom_id = std::move(custom_id_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_value(this auto&& self, bool value_) noexcept { self.value = value_; return std::forward<decltype(self)>(self); }
    };

    struct Checkbox {
        component_type type{component_type::Checkbox};
        opt<std::int32_t> id{};
        std::string custom_id{};
        opt<bool> default_{};

        static Checkbox create(std::string custom_id_ = {}, opt<bool> default__ = {}) {
            return Checkbox{.custom_id = std::move(custom_id_), .default_ = default__};
        }
        decltype(auto) set_type(this auto&& self, component_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_id(this auto&& self, opt<std::int32_t> id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_custom_id(this auto&& self, std::string custom_id_) { self.custom_id = std::move(custom_id_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_default_(this auto&& self, opt<bool> default__) noexcept { self.default_ = default__; return std::forward<decltype(self)>(self); }
        decltype(auto) set_default(this auto&& self, opt<bool> default__) noexcept { self.default_ = default__; return std::forward<decltype(self)>(self); }

        struct glaze {
            static consteval std::string_view rename_key(const std::string_view key) {
                if (key == "default_") return "default";
                return key;
            };
        };
    };

    struct CheckboxGroupInteractionResponse {
        component_type type{component_type::CheckboxGroup};
        std::int32_t id{};
        std::string custom_id{};
        std::vector<std::string> values{};

        static CheckboxGroupInteractionResponse create(std::int32_t id_ = 0, std::string custom_id_ = {}, std::vector<std::string> values_ = {}) {
            return CheckboxGroupInteractionResponse{
                .id = id_,
                .custom_id = std::move(custom_id_),
                .values = std::move(values_),
            };
        }
        decltype(auto) set_type(this auto&& self, component_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_id(this auto&& self, std::int32_t id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_custom_id(this auto&& self, std::string custom_id_) { self.custom_id = std::move(custom_id_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_values(this auto&& self, std::vector<std::string> values_) { self.values = std::move(values_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_values, std::string)
        decltype(auto) add_value(this auto&& self, std::string itm) { self.values.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
    };

    struct checkbox_group_option {
        std::string value{};
        std::string label{};
        opt<std::string> description{};
        opt<bool> default_{};

        static checkbox_group_option create(std::string value_ = {}, std::string label_ = {}, opt<std::string> description_ = {}, opt<bool> default_l = {}) {
            return checkbox_group_option{
                .value = std::move(value_),
                .label = std::move(label_),
                .description = std::move(description_),
                .default_ = default_l,
            };
        }
        decltype(auto) set_value(this auto&& self, std::string value_) { self.value = std::move(value_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_label(this auto&& self, std::string label_) { self.label = std::move(label_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_description(this auto&& self, opt<std::string> description_) { self.description = std::move(description_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_default_(this auto&& self, opt<bool> default_l) noexcept { self.default_ = default_l; return std::forward<decltype(self)>(self); }
        decltype(auto) set_default(this auto&& self, opt<bool> default_l) noexcept { self.default_ = default_l; return std::forward<decltype(self)>(self); }

        struct glaze {
            static consteval std::string_view rename_key(const std::string_view key) {
                if (key == "default_") return "default";
                return key;
            };
        };
    };

    struct CheckboxGroup {
        component_type type{component_type::CheckboxGroup};
        opt<std::int32_t> id{};
        std::string custom_id{};
        std::vector<checkbox_group_option> options{};
        opt<std::uint8_t> min_values{}; // defaults to 1?
        opt<std::uint8_t> max_values{};
        opt<bool> required{};

        static CheckboxGroup create(std::string custom_id_ = {}, std::vector<checkbox_group_option> options_ = {}) {
            return CheckboxGroup{
                .custom_id = std::move(custom_id_),
                .options = std::move(options_),
            };
        }
        decltype(auto) set_type(this auto&& self, component_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_id(this auto&& self, opt<std::int32_t> id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_custom_id(this auto&& self, std::string custom_id_) { self.custom_id = std::move(custom_id_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_options(this auto&& self, std::vector<checkbox_group_option> options_) { self.options = std::move(options_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_options, checkbox_group_option)
        decltype(auto) add_option(this auto&& self, checkbox_group_option itm) { self.options.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
        decltype(auto) add_option(this auto&& self, std::string value, std::string label, opt<std::string> description = {}, opt<bool> default_ = {}) {
            self.options.emplace_back(checkbox_group_option::create(std::move(value), std::move(label), std::move(description), default_));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_min_values(this auto&& self, opt<std::uint8_t> min_values_) noexcept { self.min_values = min_values_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_max_values(this auto&& self, opt<std::uint8_t> max_values_) noexcept { self.max_values = max_values_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_required(this auto&& self, opt<bool> required_) noexcept { self.required = required_; return std::forward<decltype(self)>(self); }
    };

    struct RadioGroupInteractionResponse {
        component_type type{component_type::RadioGroup};
        std::int32_t id{};
        std::string custom_id{};
        opt<std::string> value{};

        static RadioGroupInteractionResponse create(std::int32_t id_ = 0, std::string custom_id_ = {}, opt<std::string> value_ = {}) {
            return RadioGroupInteractionResponse{
                .id = id_,
                .custom_id = std::move(custom_id_),
                .value = std::move(value_),
            };
        }
        decltype(auto) set_type(this auto&& self, component_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_id(this auto&& self, std::int32_t id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_custom_id(this auto&& self, std::string custom_id_) { self.custom_id = std::move(custom_id_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_value(this auto&& self, opt<std::string> value_) { self.value = std::move(value_); return std::forward<decltype(self)>(self); }
    };
    
    struct radio_group_options {
        std::string value{};
        std::string label{};
        opt<std::string> description{};
        opt<bool> default_{};

        static radio_group_options create(std::string value_ = {}, std::string label_ = {}, opt<std::string> description_ = {}, opt<bool> default_l = {}) {
            return radio_group_options{
                .value = std::move(value_),
                .label = std::move(label_),
                .description = std::move(description_),
                .default_ = default_l,
            };
        }
        decltype(auto) set_value(this auto&& self, std::string value_) { self.value = std::move(value_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_label(this auto&& self, std::string label_) { self.label = std::move(label_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_description(this auto&& self, opt<std::string> description_) { self.description = std::move(description_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_default_(this auto&& self, opt<bool> default_l) noexcept { self.default_ = default_l; return std::forward<decltype(self)>(self); }
        decltype(auto) set_default(this auto&& self, opt<bool> default_l) noexcept { self.default_ = default_l; return std::forward<decltype(self)>(self); }

        struct glaze {
            static consteval std::string_view rename_key(const std::string_view key) {
                if (key == "default_") return "default";
                return key;
            };
        };
    };

    struct RadioGroup {
        component_type type{component_type::RadioGroup};
        opt<std::int32_t> id{};
        std::string custom_id{};
        std::vector<radio_group_options> options{};
        opt<bool> required{true};

        static RadioGroup create(std::string custom_id_ = {}, std::vector<radio_group_options> options_ = {}) {
            return RadioGroup{
                .custom_id = std::move(custom_id_),
                .options = std::move(options_),
            };
        }
        decltype(auto) set_type(this auto&& self, component_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_id(this auto&& self, opt<std::int32_t> id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_custom_id(this auto&& self, std::string custom_id_) { self.custom_id = std::move(custom_id_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_options(this auto&& self, std::vector<radio_group_options> options_) { self.options = std::move(options_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_options, radio_group_options)
        decltype(auto) add_option(this auto&& self, radio_group_options itm) { self.options.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
        decltype(auto) add_option(this auto&& self, std::string value, std::string label, opt<std::string> description = {}, opt<bool> default_ = {}) {
            self.options.emplace_back(radio_group_options::create(std::move(value), std::move(label), std::move(description), default_));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_required(this auto&& self, opt<bool> required_) noexcept { self.required = required_; return std::forward<decltype(self)>(self); }
    };

    struct FileUploadInteractionResponse {
        component_type type{component_type::FileUpload};
        std::int32_t id{};
        std::string custom_id{};
        std::vector<snowflake> values{};

        static FileUploadInteractionResponse create(std::int32_t id_ = 0, std::string custom_id_ = {}, std::vector<snowflake> values_ = {}) {
            return FileUploadInteractionResponse{
                .id = id_,
                .custom_id = std::move(custom_id_),
                .values = std::move(values_),
            };
        }
        decltype(auto) set_type(this auto&& self, component_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_id(this auto&& self, std::int32_t id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_custom_id(this auto&& self, std::string custom_id_) { self.custom_id = std::move(custom_id_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_values(this auto&& self, std::vector<snowflake> values_) { self.values = std::move(values_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_values, snowflake)
        decltype(auto) add_value(this auto&& self, snowflake itm) noexcept { self.values.emplace_back(itm); return std::forward<decltype(self)>(self); }
    };

    struct FileUpload {
        component_type type{component_type::FileUpload};
        opt<std::int32_t> id{};
        std::string custom_id{};
        opt<std::uint8_t> min_values{}; // defaults to 1?
        opt<std::uint8_t> max_values{};
        opt<bool> required{};
        opt<std::vector<std::string>> file_types{};

        static FileUpload create(std::string custom_id_ = {}) {
            return FileUpload{.custom_id = std::move(custom_id_)};
        }
        decltype(auto) set_type(this auto&& self, component_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_id(this auto&& self, opt<std::int32_t> id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_custom_id(this auto&& self, std::string custom_id_) { self.custom_id = std::move(custom_id_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_min_values(this auto&& self, opt<std::uint8_t> min_values_) noexcept { self.min_values = min_values_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_max_values(this auto&& self, opt<std::uint8_t> max_values_) noexcept { self.max_values = max_values_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_required(this auto&& self, opt<bool> required_) noexcept { self.required = required_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_file_types(this auto&& self, opt<std::vector<std::string>> file_types_) { self.file_types = std::move(file_types_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_file_types, std::string)
        decltype(auto) add_file_type(this auto&& self, std::string itm) {
            if (!self.file_types) self.file_types.emplace();
            self.file_types->emplace_back(std::move(itm));
            return std::forward<decltype(self)>(self);
        }
    };

    struct Separator {
        component_type type{component_type::Separator};
        opt<std::int32_t> id{};
        opt<bool> divider{true};
        opt<separator_spacing> spacing{separator_spacing::small};

        static Separator create(opt<bool> divider_ = true, opt<separator_spacing> spacing_ = separator_spacing::small) noexcept {
            return Separator{.divider = divider_, .spacing = spacing_};
        }
        decltype(auto) set_type(this auto&& self, component_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_id(this auto&& self, opt<std::int32_t> id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_divider(this auto&& self, opt<bool> divider_) noexcept { self.divider = divider_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_spacing(this auto&& self, opt<separator_spacing> spacing_) noexcept { self.spacing = spacing_; return std::forward<decltype(self)>(self); }
    };

    struct unfurled_media_item {
        std::string url{};
        opt<std::string> proxy_url{};
        opt<integer> height{};
        opt<integer> width{};
        opt<std::string> placeholder{};
        opt<integer> placeholder_version{};
        opt<std::string> content_type{};
        opt<flags_t<unfurled_media_item_flags>> flags{};
        opt<snowflake> attachment_id{};

        static unfurled_media_item create(std::string url_ = {}) {
            return unfurled_media_item{.url = std::move(url_)};
        }
        decltype(auto) set_url(this auto&& self, std::string url_) { self.url = std::move(url_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_proxy_url(this auto&& self, opt<std::string> proxy_url_) { self.proxy_url = std::move(proxy_url_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_height(this auto&& self, opt<integer> height_) noexcept { self.height = height_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_width(this auto&& self, opt<integer> width_) noexcept { self.width = width_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_placeholder(this auto&& self, opt<std::string> placeholder_) { self.placeholder = std::move(placeholder_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_placeholder_version(this auto&& self, opt<integer> placeholder_version_) noexcept { self.placeholder_version = placeholder_version_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_content_type(this auto&& self, opt<std::string> content_type_) { self.content_type = std::move(content_type_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_flags(this auto&& self, opt<flags_t<unfurled_media_item_flags>> flags_) noexcept { self.flags = flags_; return std::forward<decltype(self)>(self); }
        decltype(auto) add_flags(this auto&& self, unfurled_media_item_flags f) noexcept {
            if (!self.flags) self.flags.emplace();
            self.flags->add_flags(f);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) add_flag(this auto&& self, unfurled_media_item_flags f) noexcept { return std::forward<decltype(self)>(self).add_flags(f); }
        decltype(auto) set_attachment_id(this auto&& self, opt<snowflake> attachment_id_) noexcept { self.attachment_id = attachment_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_attachment(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_attachment_id(id_); }
    };

    struct File {
        component_type type{component_type::File};
        opt<std::int32_t> id{};
        unfurled_media_item file{};
        opt<bool> spoiler{false};
        opt<std::string> name{};
        opt<integer> size{};

        static File create(unfurled_media_item file_ = {}, opt<std::string> name_ = {}) {
            return File{.file = std::move(file_), .name = std::move(name_)};
        }
        static File create(std::string attachment_url, opt<std::string> name_ = {}, opt<bool> spoiler_ = false) {
            return File{
                .file = unfurled_media_item::create(std::move(attachment_url)),
                .spoiler = spoiler_,
                .name = std::move(name_),
            };
        }
        decltype(auto) set_type(this auto&& self, component_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_id(this auto&& self, opt<std::int32_t> id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_file(this auto&& self, unfurled_media_item file_) { self.file = std::move(file_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_file(this auto&& self, std::string url_) { self.file = unfurled_media_item::create(std::move(url_)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_spoiler(this auto&& self, opt<bool> spoiler_) noexcept { self.spoiler = spoiler_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_name(this auto&& self, opt<std::string> name_) { self.name = std::move(name_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_size(this auto&& self, opt<integer> size_) noexcept { self.size = size_; return std::forward<decltype(self)>(self); }
    };

    struct media_gallery_item {
        unfurled_media_item media{};
        opt<std::string> description{};
        opt<bool> spoiler{false};

        static media_gallery_item create(unfurled_media_item media_ = {}, opt<std::string> description_ = {}, opt<bool> spoiler_ = false) {
            return media_gallery_item{
                .media = std::move(media_),
                .description = std::move(description_),
                .spoiler = spoiler_,
            };
        }
        static media_gallery_item create(std::string url, opt<std::string> description_ = {}, opt<bool> spoiler_ = false) {
            return media_gallery_item{
                .media = unfurled_media_item::create(std::move(url)),
                .description = std::move(description_),
                .spoiler = spoiler_,
            };
        }
        decltype(auto) set_media(this auto&& self, unfurled_media_item media_) { self.media = std::move(media_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_media(this auto&& self, std::string url_) { self.media = unfurled_media_item::create(std::move(url_)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_description(this auto&& self, opt<std::string> description_) { self.description = std::move(description_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_spoiler(this auto&& self, opt<bool> spoiler_) noexcept { self.spoiler = spoiler_; return std::forward<decltype(self)>(self); }
    };

    struct MediaGallery {
        component_type type{component_type::MediaGallery};
        opt<std::int32_t> id{};
        std::vector<media_gallery_item> items{};

        static MediaGallery create(std::vector<media_gallery_item> items_ = {}) {
            return MediaGallery{.items = std::move(items_)};
        }
        decltype(auto) set_type(this auto&& self, component_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_id(this auto&& self, opt<std::int32_t> id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_items(this auto&& self, std::vector<media_gallery_item> items_) { self.items = std::move(items_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_items, media_gallery_item)
        decltype(auto) add_item(this auto&& self, media_gallery_item itm) { self.items.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
        decltype(auto) add_item(this auto&& self, unfurled_media_item media, opt<std::string> description = {}, opt<bool> spoiler = false) {
            self.items.emplace_back(media_gallery_item::create(std::move(media), std::move(description), spoiler));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) add_item(this auto&& self, std::string url, opt<std::string> description = {}, opt<bool> spoiler = false) {
            self.items.emplace_back(media_gallery_item::create(std::move(url), std::move(description), spoiler));
            return std::forward<decltype(self)>(self);
        }
    };

    struct Thumbnail {
        component_type type{component_type::Thumbnail};
        opt<std::int32_t> id{};
        unfurled_media_item media{};
        opt<std::string> description{};
        opt<bool> spoiler{};

        static Thumbnail create(unfurled_media_item media_ = {}, opt<std::string> description_ = {}) {
            return Thumbnail{
                .media = std::move(media_),
                .description = std::move(description_),
            };
        }
        static Thumbnail create(std::string url, opt<std::string> description_ = {}) {
            return Thumbnail{
                .media = unfurled_media_item::create(std::move(url)),
                .description = std::move(description_),
            };
        }
        decltype(auto) set_type(this auto&& self, component_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_id(this auto&& self, opt<std::int32_t> id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_media(this auto&& self, unfurled_media_item media_) { self.media = std::move(media_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_media(this auto&& self, std::string url_) { self.media = unfurled_media_item::create(std::move(url_)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_description(this auto&& self, opt<std::string> description_) { self.description = std::move(description_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_spoiler(this auto&& self, opt<bool> spoiler_) noexcept { self.spoiler = spoiler_; return std::forward<decltype(self)>(self); }
    };

    struct TextDisplayInteractionResponse {
        component_type type{component_type::TextDisplay};
        std::int32_t id{};

        static TextDisplayInteractionResponse create(std::int32_t id_ = 0) noexcept {
            return TextDisplayInteractionResponse{.id = id_};
        }
        decltype(auto) set_type(this auto&& self, component_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_id(this auto&& self, std::int32_t id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
    };

    struct TextDisplay {
        component_type type{component_type::TextDisplay};
        opt<std::int32_t> id{};
        std::string content{};

        static TextDisplay create(std::string content_ = {}) {
            return TextDisplay{.content = std::move(content_)};
        }
        decltype(auto) set_type(this auto&& self, component_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_id(this auto&& self, opt<std::int32_t> id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_content(this auto&& self, std::string content_) { self.content = std::move(content_); return std::forward<decltype(self)>(self); }
    };

    struct select_default_value {
        snowflake id{};
        select_default_value_type type{};

        static select_default_value create(snowflake id_ = {}, select_default_value_type type_ = select_default_value_type::user) noexcept {
            return select_default_value{.id = id_, .type = type_};
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_type(this auto&& self, select_default_value_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
    };

    template <component_type t>
    struct GenericSelect {
        component_type type{t};
        std::int32_t id{};
        std::string custom_id{};
        opt<std::string> placeholder{};
        opt<std::vector<select_default_value>> default_values{};
        opt<std::uint8_t> min_values{}; // defaults to 1?
        opt<std::uint8_t> max_values{};
        opt<bool> required{};
        opt<bool> disabled{};
        opt<std::vector<channel::channel_type>> channel_types{}; // for ChannelSelect

        static GenericSelect create(std::string custom_id_ = {}, opt<std::string> placeholder_ = {}) {
            return GenericSelect{
                .custom_id = std::move(custom_id_),
                .placeholder = std::move(placeholder_),
            };
        }
        decltype(auto) set_type(this auto&& self, component_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_id(this auto&& self, std::int32_t id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_custom_id(this auto&& self, std::string custom_id_) { self.custom_id = std::move(custom_id_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_placeholder(this auto&& self, opt<std::string> placeholder_) { self.placeholder = std::move(placeholder_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_default_values(this auto&& self, opt<std::vector<select_default_value>> default_values_) { self.default_values = std::move(default_values_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_default_values, select_default_value)
        decltype(auto) add_default_value(this auto&& self, select_default_value itm) {
            if (!self.default_values) self.default_values.emplace();
            self.default_values->emplace_back(itm);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) add_default_value(this auto&& self, snowflake id, select_default_value_type type_val = select_default_value_type::user) {
            return std::forward<decltype(self)>(self).add_default_value(select_default_value::create(id, type_val));
        }
        decltype(auto) set_min_values(this auto&& self, opt<std::uint8_t> min_values_) noexcept { self.min_values = min_values_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_max_values(this auto&& self, opt<std::uint8_t> max_values_) noexcept { self.max_values = max_values_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_required(this auto&& self, opt<bool> required_) noexcept { self.required = required_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_disabled(this auto&& self, opt<bool> disabled_) noexcept { self.disabled = disabled_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_channel_types(this auto&& self, opt<std::vector<channel::channel_type>> channel_types_) { self.channel_types = std::move(channel_types_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_channel_types, channel::channel_type)
        decltype(auto) add_channel_type(this auto&& self, channel::channel_type ct) {
            if (!self.channel_types) self.channel_types.emplace();
            self.channel_types->emplace_back(ct);
            return std::forward<decltype(self)>(self);
        }
    };

    template <components::component_type t>
    struct generic_select_interaction_response {
        components::component_type type{t};
        components::component_type component_type{t};
        std::int32_t id{};
        std::string custom_id{};
        interaction::resolved resolved{};
        std::vector<snowflake> values{};

        static generic_select_interaction_response create(std::int32_t id_ = 0, std::string custom_id_ = {}, std::vector<snowflake> values_ = {}) {
            return generic_select_interaction_response{
                .id = id_,
                .custom_id = std::move(custom_id_),
                .values = std::move(values_),
            };
        }
        decltype(auto) set_type(this auto&& self, components::component_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_component_type(this auto&& self, components::component_type component_type_) noexcept { self.component_type = component_type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_id(this auto&& self, std::int32_t id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_custom_id(this auto&& self, std::string custom_id_) { self.custom_id = std::move(custom_id_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_resolved(this auto&& self, interaction::resolved resolved_) { self.resolved = std::move(resolved_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_values(this auto&& self, std::vector<snowflake> values_) { self.values = std::move(values_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_values, snowflake)
        decltype(auto) add_value(this auto&& self, snowflake itm) noexcept { self.values.emplace_back(itm); return std::forward<decltype(self)>(self); }
    };

    using ChannelSelectInteractionResponse = generic_select_interaction_response<component_type::ChannelSelect>;
    using ChannelSelect = GenericSelect<component_type::ChannelSelect>;

    using MentionableSelectInteractionResponse = generic_select_interaction_response<component_type::MentionableSelect>;
    using MentionableSelect = GenericSelect<component_type::MentionableSelect>;

    using RoleSelectInteractionResponse = generic_select_interaction_response<component_type::RoleSelect>;
    using RoleSelect = GenericSelect<component_type::RoleSelect>;

    using UserSelectInteractionResponse = generic_select_interaction_response<component_type::UserSelect>;
    using UserSelect = GenericSelect<component_type::UserSelect>;

    struct TextInputInteractionResponse {
        component_type type{component_type::TextInput};
        std::int32_t id{};
        std::string custom_id{};
        std::string value{};

        static TextInputInteractionResponse create(std::int32_t id_ = 0, std::string custom_id_ = {}, std::string value_ = {}) {
            return TextInputInteractionResponse{
                .id = id_,
                .custom_id = std::move(custom_id_),
                .value = std::move(value_),
            };
        }
        decltype(auto) set_type(this auto&& self, component_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_id(this auto&& self, std::int32_t id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_custom_id(this auto&& self, std::string custom_id_) { self.custom_id = std::move(custom_id_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_value(this auto&& self, std::string value_) { self.value = std::move(value_); return std::forward<decltype(self)>(self); }
    };

    struct TextInput {
        component_type type{component_type::TextInput};
        opt<std::int32_t> id{};
        std::string custom_id{};
        text_input_style style{};
        opt<std::uint16_t> min_length{};
        opt<std::uint16_t> max_length{};
        opt<bool> required{};
        opt<std::string> value{};
        opt<std::string> placeholder{};

        static TextInput create(std::string custom_id_ = {}, text_input_style style_ = text_input_style::Short) {
            return TextInput{.custom_id = std::move(custom_id_), .style = style_};
        }
        decltype(auto) set_type(this auto&& self, component_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_id(this auto&& self, opt<std::int32_t> id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_custom_id(this auto&& self, std::string custom_id_) { self.custom_id = std::move(custom_id_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_style(this auto&& self, text_input_style style_) noexcept { self.style = style_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_min_length(this auto&& self, opt<std::uint16_t> min_length_) noexcept { self.min_length = min_length_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_max_length(this auto&& self, opt<std::uint16_t> max_length_) noexcept { self.max_length = max_length_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_required(this auto&& self, opt<bool> required_) noexcept { self.required = required_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_value(this auto&& self, opt<std::string> value_) { self.value = std::move(value_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_placeholder(this auto&& self, opt<std::string> placeholder_) { self.placeholder = std::move(placeholder_); return std::forward<decltype(self)>(self); }
    };

    struct Button {
        component_type type{component_type::Button};
        opt<std::int32_t> id{};
        button_style style{};
        opt<std::string> label{};
        opt<emoji::emoji> emoji{};
        opt<std::string> custom_id{};
        opt<snowflake> sku_id{};
        opt<std::string> url{};
        opt<bool> disabled{};

        static Button create(button_style style_ = button_style::Primary, opt<std::string> label_ = {}, opt<std::string> custom_id_ = {}) {
            return Button{
                .style = style_,
                .label = std::move(label_),
                .custom_id = std::move(custom_id_),
            };
        }
        decltype(auto) set_type(this auto&& self, component_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_id(this auto&& self, opt<std::int32_t> id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_style(this auto&& self, button_style style_) noexcept { self.style = style_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_label(this auto&& self, opt<std::string> label_) { self.label = std::move(label_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_emoji(this auto&& self, opt<emoji::emoji> emoji_) { self.emoji = std::move(emoji_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_custom_id(this auto&& self, opt<std::string> custom_id_) { self.custom_id = std::move(custom_id_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_sku_id(this auto&& self, opt<snowflake> sku_id_) noexcept { self.sku_id = sku_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_sku(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_sku_id(id_); }
        decltype(auto) set_url(this auto&& self, opt<std::string> url_) { self.url = std::move(url_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_disabled(this auto&& self, opt<bool> disabled_) noexcept { self.disabled = disabled_; return std::forward<decltype(self)>(self); }
    };

    #pragma push_macro("SECTION_ACCESSORY")
    #undef SECTION_ACCESSORY
    #define SECTION_ACCESSORY \
        DISCUSY_X(Button) DISCUSY_D \
        DISCUSY_X(Thumbnail)

    struct section_accessory : component_variant<section_accessory,
        #define DISCUSY_X(name) name
        SECTION_ACCESSORY
        #undef DISCUSY_X
    > {
        using component_variant::component_variant;

        static section_accessory create() noexcept {
            return section_accessory{};
        }
        template <typename T>
        static section_accessory create(T&& c) {
            return section_accessory{std::forward<T>(c)};
        }
        template <typename T>
        decltype(auto) set_component(this auto&& self, T&& c) {
            *self = std::forward<T>(c);
            return std::forward<decltype(self)>(self);
        }
    };

    struct Section {
        component_type type{component_type::Section};
        opt<std::int32_t> id{};
        std::vector<TextDisplay> components{}; // will be variant eventually
        section_accessory accessory{};

        static Section create(std::vector<TextDisplay> components_ = {}, section_accessory accessory_ = {}) {
            return Section{
                .components = std::move(components_),
                .accessory = std::move(accessory_),
            };
        }
        decltype(auto) set_type(this auto&& self, component_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_id(this auto&& self, opt<std::int32_t> id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_components(this auto&& self, std::vector<TextDisplay> components_) { self.components = std::move(components_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_components, TextDisplay)
        decltype(auto) add_component(this auto&& self, TextDisplay itm) { self.components.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
        decltype(auto) add_text_display(this auto&& self, TextDisplay itm) { self.components.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
        decltype(auto) add_text(this auto&& self, std::string content) { self.components.emplace_back(TextDisplay::create(std::move(content))); return std::forward<decltype(self)>(self); }
        decltype(auto) set_accessory(this auto&& self, section_accessory accessory_) { self.accessory = std::move(accessory_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_accessory(this auto&& self, Button btn) { self.accessory = std::move(btn); return std::forward<decltype(self)>(self); }
        decltype(auto) set_accessory(this auto&& self, Thumbnail thumb) { self.accessory = std::move(thumb); return std::forward<decltype(self)>(self); }
        decltype(auto) set_button_accessory(this auto&& self, Button btn) { self.accessory = std::move(btn); return std::forward<decltype(self)>(self); }
        decltype(auto) set_thumbnail_accessory(this auto&& self, Thumbnail thumb) { self.accessory = std::move(thumb); return std::forward<decltype(self)>(self); }
        decltype(auto) set_thumbnail_accessory(this auto&& self, std::string url, opt<std::string> description = {}, opt<bool> spoiler = {}) {
            self.accessory = Thumbnail::create(std::move(url), std::move(description)).set_spoiler(spoiler);
            return std::forward<decltype(self)>(self);
        }
    };

    struct select_option {
        std::string label{};
        std::string value{};
        opt<std::string> description{};
        opt<emoji::emoji> emoji{};
        opt<bool> default_{};

        static select_option create(std::string label_ = {}, std::string value_ = {}, opt<std::string> description_ = {}, opt<emoji::emoji> emoji_ = {}, opt<bool> default_l = {}) {
            return select_option{
                .label = std::move(label_),
                .value = std::move(value_),
                .description = std::move(description_),
                .emoji = std::move(emoji_),
                .default_ = default_l,
            };
        }
        decltype(auto) set_label(this auto&& self, std::string label_) { self.label = std::move(label_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_value(this auto&& self, std::string value_) { self.value = std::move(value_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_description(this auto&& self, opt<std::string> description_) { self.description = std::move(description_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_emoji(this auto&& self, opt<emoji::emoji> emoji_) { self.emoji = std::move(emoji_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_default_(this auto&& self, opt<bool> default_l) noexcept { self.default_ = default_l; return std::forward<decltype(self)>(self); }
        decltype(auto) set_default(this auto&& self, opt<bool> default_l) noexcept { self.default_ = default_l; return std::forward<decltype(self)>(self); }

        struct glaze {
            static consteval std::string_view rename_key(const std::string_view key) {
                if (key == "default_") return "default";
                return key;
            };
        };
    };

    struct StringSelectInteractionResponse {
        components::component_type type{components::component_type::StringSelect};
        components::component_type component_type{components::component_type::StringSelect};
        std::int32_t id{};
        std::string custom_id{};
        std::vector<std::string> values{};

        static StringSelectInteractionResponse create(std::int32_t id_ = 0, std::string custom_id_ = {}, std::vector<std::string> values_ = {}) {
            return StringSelectInteractionResponse{
                .id = id_,
                .custom_id = std::move(custom_id_),
                .values = std::move(values_),
            };
        }
        decltype(auto) set_type(this auto&& self, components::component_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_component_type(this auto&& self, components::component_type component_type_) noexcept { self.component_type = component_type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_id(this auto&& self, std::int32_t id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_custom_id(this auto&& self, std::string custom_id_) { self.custom_id = std::move(custom_id_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_values(this auto&& self, std::vector<std::string> values_) { self.values = std::move(values_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_values, std::string)
        decltype(auto) add_value(this auto&& self, std::string itm) { self.values.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
    };

    struct StringSelect {
        component_type type{component_type::StringSelect};
        opt<std::int32_t> id{};
        std::string custom_id{};
        std::vector<select_option> options{};
        opt<std::string> placeholder{};
        opt<std::uint8_t> min_values{}; // defaults to 1?
        opt<std::uint8_t> max_values{};
        opt<bool> required{};
        opt<bool> disabled{};

        static StringSelect create(std::string custom_id_ = {}, std::vector<select_option> options_ = {}) {
            return StringSelect{
                .custom_id = std::move(custom_id_),
                .options = std::move(options_),
            };
        }
        decltype(auto) set_type(this auto&& self, component_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_id(this auto&& self, opt<std::int32_t> id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_custom_id(this auto&& self, std::string custom_id_) { self.custom_id = std::move(custom_id_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_options(this auto&& self, std::vector<select_option> options_) { self.options = std::move(options_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_options, select_option)
        decltype(auto) add_option(this auto&& self, select_option itm) { self.options.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
        decltype(auto) add_option(this auto&& self, std::string label, std::string value, opt<std::string> description = {}, opt<emoji::emoji> emoji = {}, opt<bool> default_ = {}) {
            self.options.emplace_back(select_option::create(std::move(label), std::move(value), std::move(description), std::move(emoji), default_));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_placeholder(this auto&& self, opt<std::string> placeholder_) { self.placeholder = std::move(placeholder_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_min_values(this auto&& self, opt<std::uint8_t> min_values_) noexcept { self.min_values = min_values_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_max_values(this auto&& self, opt<std::uint8_t> max_values_) noexcept { self.max_values = max_values_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_required(this auto&& self, opt<bool> required_) noexcept { self.required = required_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_disabled(this auto&& self, opt<bool> disabled_) noexcept { self.disabled = disabled_; return std::forward<decltype(self)>(self); }
    };

    #pragma push_macro("LABEL_CHILD_COMPONENTS")
    #undef LABEL_CHILD_COMPONENTS
    #define LABEL_CHILD_COMPONENTS \
        DISCUSY_X(TextInput) DISCUSY_D \
        DISCUSY_X(StringSelect) DISCUSY_D \
        DISCUSY_X(UserSelect) DISCUSY_D \
        DISCUSY_X(RoleSelect) DISCUSY_D \
        DISCUSY_X(MentionableSelect) DISCUSY_D \
        DISCUSY_X(ChannelSelect) DISCUSY_D \
        DISCUSY_X(FileUpload) DISCUSY_D \
        DISCUSY_X(RadioGroup) DISCUSY_D \
        DISCUSY_X(CheckboxGroup) DISCUSY_D \
        DISCUSY_X(Checkbox)

    struct label_child_component : component_variant<label_child_component,
        #define DISCUSY_X(name) name
        LABEL_CHILD_COMPONENTS
        #undef DISCUSY_X
    > {
        using component_variant::component_variant;

        static label_child_component create() noexcept {
            return label_child_component{};
        }
        template <typename T>
        static label_child_component create(T&& c) {
            return label_child_component{std::forward<T>(c)};
        }
        template <typename T>
        decltype(auto) set_component(this auto&& self, T&& c) {
            *self = std::forward<T>(c);
            return std::forward<decltype(self)>(self);
        }
    };

    struct LabelInteractionResponse {
        component_type type{component_type::Label};
        std::int32_t id{};
        label_child_component component{};

        static LabelInteractionResponse create(std::int32_t id_ = 0, label_child_component component_ = {}) {
            return LabelInteractionResponse{.id = id_, .component = std::move(component_)};
        }
        decltype(auto) set_type(this auto&& self, component_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_id(this auto&& self, std::int32_t id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_component(this auto&& self, label_child_component component_) { self.component = std::move(component_); return std::forward<decltype(self)>(self); }
    };

    struct Label {
        component_type type{component_type::Label};
        opt<std::int32_t> id{};
        std::string label{};
        opt<std::string> description{};
        label_child_component component{};

        static Label create(std::string label_ = {}, label_child_component component_ = {}) {
            return Label{.label = std::move(label_), .component = std::move(component_)};
        }
        decltype(auto) set_type(this auto&& self, component_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_id(this auto&& self, opt<std::int32_t> id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_label(this auto&& self, std::string label_) { self.label = std::move(label_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_description(this auto&& self, opt<std::string> description_) { self.description = std::move(description_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_component(this auto&& self, label_child_component component_) { self.component = std::move(component_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_text_input(this auto&& self, TextInput input) { self.component = std::move(input); return std::forward<decltype(self)>(self); }
        decltype(auto) set_string_select(this auto&& self, StringSelect select) { self.component = std::move(select); return std::forward<decltype(self)>(self); }
        decltype(auto) set_user_select(this auto&& self, UserSelect select) { self.component = std::move(select); return std::forward<decltype(self)>(self); }
        decltype(auto) set_role_select(this auto&& self, RoleSelect select) { self.component = std::move(select); return std::forward<decltype(self)>(self); }
        decltype(auto) set_mentionable_select(this auto&& self, MentionableSelect select) { self.component = std::move(select); return std::forward<decltype(self)>(self); }
        decltype(auto) set_channel_select(this auto&& self, ChannelSelect select) { self.component = std::move(select); return std::forward<decltype(self)>(self); }
        decltype(auto) set_file_upload(this auto&& self, FileUpload upload) { self.component = std::move(upload); return std::forward<decltype(self)>(self); }
        decltype(auto) set_radio_group(this auto&& self, RadioGroup rg) { self.component = std::move(rg); return std::forward<decltype(self)>(self); }
        decltype(auto) set_checkbox_group(this auto&& self, CheckboxGroup cg) { self.component = std::move(cg); return std::forward<decltype(self)>(self); }
        decltype(auto) set_checkbox(this auto&& self, Checkbox cb) { self.component = std::move(cb); return std::forward<decltype(self)>(self); }
    };

    #pragma push_macro("ACTION_ROW_COMPONENTS")
    #undef ACTION_ROW_COMPONENTS
    #define ACTION_ROW_COMPONENTS \
        DISCUSY_X(Button) DISCUSY_D \
        DISCUSY_X(StringSelect) DISCUSY_D \
        DISCUSY_X(UserSelect) DISCUSY_D \
        DISCUSY_X(RoleSelect) DISCUSY_D \
        DISCUSY_X(MentionableSelect) DISCUSY_D \
        DISCUSY_X(ChannelSelect) 

    struct action_row_component : component_variant<action_row_component,
        #define DISCUSY_X(name) name
        ACTION_ROW_COMPONENTS
        #undef DISCUSY_X
    > {
        using component_variant::component_variant;

        static action_row_component create() noexcept {
            return action_row_component{};
        }
        template <typename T>
        static action_row_component create(T&& c) {
            return action_row_component{std::forward<T>(c)};
        }
        template <typename T>
        decltype(auto) set_component(this auto&& self, T&& c) {
            *self = std::forward<T>(c);
            return std::forward<decltype(self)>(self);
        }
    };

    struct ActionRow {
        component_type type{component_type::ActionRow};
        opt<std::int32_t> id{};
        std::vector<action_row_component> components{};

        static ActionRow create(std::vector<action_row_component> components_ = {}) {
            return ActionRow{.components = std::move(components_)};
        }
        decltype(auto) set_type(this auto&& self, component_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_id(this auto&& self, opt<std::int32_t> id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_components(this auto&& self, std::vector<action_row_component> components_) { self.components = std::move(components_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_components, action_row_component)
        decltype(auto) add_component(this auto&& self, action_row_component itm) { self.components.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
        decltype(auto) add_button(this auto&& self, Button btn) { self.components.emplace_back(std::move(btn)); return std::forward<decltype(self)>(self); }
        decltype(auto) add_string_select(this auto&& self, StringSelect sel) { self.components.emplace_back(std::move(sel)); return std::forward<decltype(self)>(self); }
        decltype(auto) add_user_select(this auto&& self, UserSelect sel) { self.components.emplace_back(std::move(sel)); return std::forward<decltype(self)>(self); }
        decltype(auto) add_role_select(this auto&& self, RoleSelect sel) { self.components.emplace_back(std::move(sel)); return std::forward<decltype(self)>(self); }
        decltype(auto) add_mentionable_select(this auto&& self, MentionableSelect sel) { self.components.emplace_back(std::move(sel)); return std::forward<decltype(self)>(self); }
        decltype(auto) add_channel_select(this auto&& self, ChannelSelect sel) { self.components.emplace_back(std::move(sel)); return std::forward<decltype(self)>(self); }
    };

    #pragma push_macro("CONTAINER_CHILD_COMPONENTS")
    #undef CONTAINER_CHILD_COMPONENTS
    #define CONTAINER_CHILD_COMPONENTS \
        DISCUSY_X(ActionRow) DISCUSY_D \
        DISCUSY_X(TextDisplay) DISCUSY_D \
        DISCUSY_X(Section) DISCUSY_D \
        DISCUSY_X(MediaGallery) DISCUSY_D \
        DISCUSY_X(Separator) DISCUSY_D \
        DISCUSY_X(File)
    
    struct container_child_component : component_variant<container_child_component,
        #define DISCUSY_X(name) name
        CONTAINER_CHILD_COMPONENTS
        #undef DISCUSY_X
    > {
        using component_variant::component_variant;

        static container_child_component create() noexcept {
            return container_child_component{};
        }
        template <typename T>
        static container_child_component create(T&& c) {
            return container_child_component{std::forward<T>(c)};
        }
        template <typename T>
        decltype(auto) set_component(this auto&& self, T&& c) {
            *self = std::forward<T>(c);
            return std::forward<decltype(self)>(self);
        }
    };

    struct Container {
        component_type type{component_type::Container};
        opt<std::int32_t> id{};
        std::vector<container_child_component> components{};
        opt<integer> accent_color{};
        opt<bool> spoiler{false};

        static Container create(std::vector<container_child_component> components_ = {}) {
            return Container{.components = std::move(components_)};
        }
        decltype(auto) set_type(this auto&& self, component_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_id(this auto&& self, opt<std::int32_t> id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_components(this auto&& self, std::vector<container_child_component> components_) { self.components = std::move(components_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_components, container_child_component)
        decltype(auto) add_component(this auto&& self, container_child_component itm) { self.components.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
        decltype(auto) add_action_row(this auto&& self, ActionRow row) { self.components.emplace_back(std::move(row)); return std::forward<decltype(self)>(self); }
        decltype(auto) add_text_display(this auto&& self, TextDisplay td) { self.components.emplace_back(std::move(td)); return std::forward<decltype(self)>(self); }
        decltype(auto) add_text(this auto&& self, std::string content) { self.components.emplace_back(TextDisplay::create(std::move(content))); return std::forward<decltype(self)>(self); }
        decltype(auto) add_section(this auto&& self, Section sec) { self.components.emplace_back(std::move(sec)); return std::forward<decltype(self)>(self); }
        decltype(auto) add_media_gallery(this auto&& self, MediaGallery mg) { self.components.emplace_back(std::move(mg)); return std::forward<decltype(self)>(self); }
        decltype(auto) add_separator(this auto&& self, Separator sep = Separator::create()) { self.components.emplace_back(std::move(sep)); return std::forward<decltype(self)>(self); }
        decltype(auto) add_file(this auto&& self, File f) { self.components.emplace_back(std::move(f)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_accent_color(this auto&& self, opt<integer> accent_color_) noexcept { self.accent_color = accent_color_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_spoiler(this auto&& self, opt<bool> spoiler_) noexcept { self.spoiler = spoiler_; return std::forward<decltype(self)>(self); }
    };

    struct component : component_variant<component,
        #define DISCUSY_X(name, _) name
        COMPONENTS
        #undef DISCUSY_X
    > {
        using component_variant::component_variant;

        static discusy::components::component create() noexcept {
            return discusy::components::component{};
        }
        template <typename T>
        static discusy::components::component create(T&& c) {
            return discusy::components::component{std::forward<T>(c)};
        }
        template <typename T>
        decltype(auto) set_component(this auto&& self, T&& c) {
            *self = std::forward<T>(c);
            return std::forward<decltype(self)>(self);
        }
    };
    
};

namespace message {

    struct message_snapshot {
        std::shared_ptr<discusy::message::message> message{}; // technically can just use the reduced subset but it might change

        static message_snapshot create(std::shared_ptr<discusy::message::message> msg = {}) {
            return message_snapshot{.message = std::move(msg)};
        }
        decltype(auto) set_message(this auto&& self, std::shared_ptr<discusy::message::message> message_) { self.message = std::move(message_); return std::forward<decltype(self)>(self); }
    };

    struct application_command_interaction_metadata {
        snowflake id{};
        interaction::interaction_type type{};
        user::user user{};
        std::unordered_map<application::application_integration_type, snowflake> authorizing_integration_owners{};
        opt<snowflake> original_response_message_id{};
        opt<user::user> target_user{};
        opt<snowflake> target_message_id{};

        static application_command_interaction_metadata create(snowflake id_ = {}, interaction::interaction_type type_ = interaction::interaction_type::APPLICATION_COMMAND, user::user user_ = {}) {
            return application_command_interaction_metadata{
                .id = id_,
                .type = type_,
                .user = std::move(user_),
            };
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_type(this auto&& self, interaction::interaction_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_user(this auto&& self, user::user user_) { self.user = std::move(user_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_authorizing_integration_owners(this auto&& self, std::unordered_map<application::application_integration_type, snowflake> authorizing_integration_owners_) { self.authorizing_integration_owners = std::move(authorizing_integration_owners_); return std::forward<decltype(self)>(self); }
        decltype(auto) add_authorizing_integration_owner(this auto&& self, application::application_integration_type k, snowflake v) {
            self.authorizing_integration_owners[k] = v;
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_original_response_message_id(this auto&& self, opt<snowflake> original_response_message_id_) noexcept { self.original_response_message_id = original_response_message_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_original_response_message(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_original_response_message_id(id_); }
        decltype(auto) set_target_user(this auto&& self, opt<user::user> target_user_) { self.target_user = std::move(target_user_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_target_message_id(this auto&& self, opt<snowflake> target_message_id_) noexcept { self.target_message_id = target_message_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_target_message(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_target_message_id(id_); }
    };

    struct message_component_interaction_metadata {
        snowflake id{};
        interaction::interaction_type type{};
        user::user user{};
        std::unordered_map<application::application_integration_type, snowflake> authorizing_integration_owners{};
        opt<snowflake> original_response_message_id{};
        snowflake interacted_message_id{};

        static message_component_interaction_metadata create(snowflake id_ = {}, interaction::interaction_type type_ = interaction::interaction_type::MESSAGE_COMPONENT, user::user user_ = {}, snowflake interacted_message_id_ = {}) {
            return message_component_interaction_metadata{
                .id = id_,
                .type = type_,
                .user = std::move(user_),
                .interacted_message_id = interacted_message_id_,
            };
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_type(this auto&& self, interaction::interaction_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_user(this auto&& self, user::user user_) { self.user = std::move(user_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_authorizing_integration_owners(this auto&& self, std::unordered_map<application::application_integration_type, snowflake> authorizing_integration_owners_) { self.authorizing_integration_owners = std::move(authorizing_integration_owners_); return std::forward<decltype(self)>(self); }
        decltype(auto) add_authorizing_integration_owner(this auto&& self, application::application_integration_type k, snowflake v) {
            self.authorizing_integration_owners[k] = v;
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_original_response_message_id(this auto&& self, opt<snowflake> original_response_message_id_) noexcept { self.original_response_message_id = original_response_message_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_original_response_message(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_original_response_message_id(id_); }
        decltype(auto) set_interacted_message_id(this auto&& self, snowflake interacted_message_id_) noexcept { self.interacted_message_id = interacted_message_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_interacted_message(this auto&& self, snowflake id_) noexcept { return std::forward<decltype(self)>(self).set_interacted_message_id(id_); }
    };

    struct modal_submit_interaction_metadata {
        snowflake id{};
        interaction::interaction_type type{};
        user::user user{};
        std::unordered_map<application::application_integration_type, snowflake> authorizing_integration_owners{};
        opt<snowflake> original_response_message_id{};
        std::variant<application_command_interaction_metadata, message_component_interaction_metadata> triggering_interaction_metadata{};

        static modal_submit_interaction_metadata create(snowflake id_ = {}, interaction::interaction_type type_ = interaction::interaction_type::MODAL_SUBMIT, user::user user_ = {}) {
            return modal_submit_interaction_metadata{
                .id = id_,
                .type = type_,
                .user = std::move(user_),
            };
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_type(this auto&& self, interaction::interaction_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_user(this auto&& self, user::user user_) { self.user = std::move(user_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_authorizing_integration_owners(this auto&& self, std::unordered_map<application::application_integration_type, snowflake> authorizing_integration_owners_) { self.authorizing_integration_owners = std::move(authorizing_integration_owners_); return std::forward<decltype(self)>(self); }
        decltype(auto) add_authorizing_integration_owner(this auto&& self, application::application_integration_type k, snowflake v) {
            self.authorizing_integration_owners[k] = v;
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_original_response_message_id(this auto&& self, opt<snowflake> original_response_message_id_) noexcept { self.original_response_message_id = original_response_message_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_original_response_message(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_original_response_message_id(id_); }
        decltype(auto) set_triggering_interaction_metadata(this auto&& self, std::variant<application_command_interaction_metadata, message_component_interaction_metadata> triggering_interaction_metadata_) { self.triggering_interaction_metadata = std::move(triggering_interaction_metadata_); return std::forward<decltype(self)>(self); }
    };

    using message_interaction_metadata = std::variant<
        application_command_interaction_metadata,
        message_component_interaction_metadata,
        modal_submit_interaction_metadata
    >;

    struct role_subscription_data {
        snowflake role_subscription_listing_id{};
        std::string tier_name{};
        integer total_months_subscribed{};
        bool is_renewal{};

        static role_subscription_data create(snowflake role_subscription_listing_id_ = {}, std::string tier_name_ = {}, integer total_months_subscribed_ = 0, bool is_renewal_ = false) {
            return role_subscription_data{
                .role_subscription_listing_id = role_subscription_listing_id_,
                .tier_name = std::move(tier_name_),
                .total_months_subscribed = total_months_subscribed_,
                .is_renewal = is_renewal_,
            };
        }
        decltype(auto) set_role_subscription_listing_id(this auto&& self, snowflake role_subscription_listing_id_) noexcept { self.role_subscription_listing_id = role_subscription_listing_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_role_subscription_listing(this auto&& self, snowflake id_) noexcept { return std::forward<decltype(self)>(self).set_role_subscription_listing_id(id_); }
        decltype(auto) set_tier_name(this auto&& self, std::string tier_name_) { self.tier_name = std::move(tier_name_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_total_months_subscribed(this auto&& self, integer total_months_subscribed_) noexcept { self.total_months_subscribed = total_months_subscribed_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_is_renewal(this auto&& self, bool is_renewal_) noexcept { self.is_renewal = is_renewal_; return std::forward<decltype(self)>(self); }
    };

    struct message_call {
        std::vector<snowflake> participants{};
        opt<timestamp> ended_timestamp{};

        static message_call create(std::vector<snowflake> participants_ = {}, opt<timestamp> ended_timestamp_ = {}) {
            return message_call{
                .participants = std::move(participants_),
                .ended_timestamp = ended_timestamp_,
            };
        }
        decltype(auto) set_participants(this auto&& self, std::vector<snowflake> participants_) { self.participants = std::move(participants_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_participants, snowflake)
        decltype(auto) add_participant(this auto&& self, snowflake itm) noexcept { self.participants.emplace_back(itm); return std::forward<decltype(self)>(self); }
        decltype(auto) set_ended_timestamp(this auto&& self, opt<timestamp> ended_timestamp_) noexcept { self.ended_timestamp = ended_timestamp_; return std::forward<decltype(self)>(self); }
    };

    enum class base_theme : std::uint8_t {
        UNSET = 0,
        DARK = 1,
        LIGHT = 2,
        DARKER = 3,
        MIDNIGHT = 4,
    };

    struct shared_client_theme {
        std::vector<std::string> colors{};
        std::uint16_t gradient_angle{};
        std::uint8_t base_mix{};
        opt<discusy::message::base_theme> base_theme{};

        static shared_client_theme create(std::vector<std::string> colors_ = {}, std::uint16_t gradient_angle_ = 0, std::uint8_t base_mix_ = 0) {
            return shared_client_theme{
                .colors = std::move(colors_),
                .gradient_angle = gradient_angle_,
                .base_mix = base_mix_,
            };
        }
        decltype(auto) set_colors(this auto&& self, std::vector<std::string> colors_) { self.colors = std::move(colors_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_colors, std::string)
        decltype(auto) add_color(this auto&& self, std::string itm) { self.colors.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_gradient_angle(this auto&& self, std::uint16_t gradient_angle_) noexcept { self.gradient_angle = gradient_angle_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_base_mix(this auto&& self, std::uint8_t base_mix_) noexcept { self.base_mix = base_mix_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_base_theme(this auto&& self, opt<discusy::message::base_theme> base_theme_) noexcept { self.base_theme = base_theme_; return std::forward<decltype(self)>(self); }
    };

    struct message {
        discusy::snowflake id{};
        discusy::snowflake channel_id{};
        discusy::user::user author{};
        std::string content{};
        discusy::timestamp timestamp{};
        opt<discusy::timestamp> edited_timestamp{};
        bool tts{};
        bool mention_everyone{};
        std::vector<discusy::user::user> mentions{};
        std::vector<discusy::snowflake> mention_roles{};
        opt<std::vector<discusy::channel::channel_mention>> mention_channels{};
        std::vector<discusy::message::attachment> attachments{};
        std::vector<discusy::message::embed> embeds{};
        opt<std::vector<discusy::message::reaction>> reactions{};
        opt<std::variant<discusy::integer, std::string>> nonce{};
        bool pinned{};
        opt<discusy::snowflake> webhook_id{};
        discusy::message::message_type type{};
        opt<discusy::message::message_activity> activity{};
        opt<discusy::application::application> application{};
        opt<discusy::snowflake> application_id{};
        opt<discusy::flags_t<discusy::message::message_flags>> flags{};
        opt<discusy::message::message_reference> message_reference{};
        opt<std::vector<discusy::message::message_snapshot>> message_snapshots{};
        opt<std::shared_ptr<discusy::message::message>> referenced_message{};
        opt<discusy::message::message_interaction_metadata> interaction_metadata{};
        opt<discusy::interaction::message_interaction> interaction{};
        opt<discusy::channel::channel> thread{};
        opt<std::vector<components::component>> components{};
        opt<std::vector<sticker::sticker_item>> sticker_items{};
        opt<std::vector<sticker::sticker>> stickers{};
        opt<integer> position{};
        opt<discusy::message::role_subscription_data> role_subscription_data{};
        opt<interaction::resolved> resolved{};
        opt<discusy::poll::poll> poll{};
        opt<discusy::message::message_call> call{};
        opt<discusy::message::shared_client_theme> shared_client_theme{};

        static message create(discusy::snowflake id_ = {}, discusy::snowflake channel_id_ = {}, discusy::user::user author_ = {}, std::string content_ = {}) {
            return message{
                .id = id_,
                .channel_id = channel_id_,
                .author = std::move(author_),
                .content = std::move(content_),
            };
        }
        decltype(auto) set_id(this auto&& self, discusy::snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_channel_id(this auto&& self, discusy::snowflake channel_id_) noexcept { self.channel_id = channel_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_channel(this auto&& self, discusy::snowflake id_) noexcept { return std::forward<decltype(self)>(self).set_channel_id(id_); }
        decltype(auto) set_author(this auto&& self, discusy::user::user author_) { self.author = std::move(author_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_content(this auto&& self, std::string content_) { self.content = std::move(content_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_timestamp(this auto&& self, discusy::timestamp timestamp_) noexcept { self.timestamp = timestamp_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_edited_timestamp(this auto&& self, opt<discusy::timestamp> edited_timestamp_) noexcept { self.edited_timestamp = edited_timestamp_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_tts(this auto&& self, bool tts_) noexcept { self.tts = tts_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_mention_everyone(this auto&& self, bool mention_everyone_) noexcept { self.mention_everyone = mention_everyone_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_mentions(this auto&& self, std::vector<discusy::user::user> mentions_) { self.mentions = std::move(mentions_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_mentions, discusy::user::user)
        decltype(auto) add_mention(this auto&& self, discusy::user::user itm) { self.mentions.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_mention_roles(this auto&& self, std::vector<discusy::snowflake> mention_roles_) { self.mention_roles = std::move(mention_roles_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_mention_roles, discusy::snowflake)
        decltype(auto) add_mention_role(this auto&& self, discusy::snowflake itm) noexcept { self.mention_roles.emplace_back(itm); return std::forward<decltype(self)>(self); }
        decltype(auto) set_mention_channels(this auto&& self, opt<std::vector<discusy::channel::channel_mention>> mention_channels_) { self.mention_channels = std::move(mention_channels_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_mention_channels, discusy::channel::channel_mention)
        decltype(auto) add_mention_channel(this auto&& self, discusy::channel::channel_mention itm) {
            if (!self.mention_channels) self.mention_channels.emplace();
            self.mention_channels->emplace_back(std::move(itm));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_attachments(this auto&& self, std::vector<discusy::message::attachment> attachments_) { self.attachments = std::move(attachments_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_attachments, discusy::message::attachment)
        decltype(auto) add_attachment(this auto&& self, discusy::message::attachment itm) { self.attachments.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_embeds(this auto&& self, std::vector<discusy::message::embed> embeds_) { self.embeds = std::move(embeds_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_embeds, discusy::message::embed)
        decltype(auto) add_embed(this auto&& self, discusy::message::embed itm) { self.embeds.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_reactions(this auto&& self, opt<std::vector<discusy::message::reaction>> reactions_) { self.reactions = std::move(reactions_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_reactions, discusy::message::reaction)
        decltype(auto) add_reaction(this auto&& self, discusy::message::reaction itm) {
            if (!self.reactions) self.reactions.emplace();
            self.reactions->emplace_back(std::move(itm));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_nonce(this auto&& self, opt<std::variant<discusy::integer, std::string>> nonce_) { self.nonce = std::move(nonce_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_pinned(this auto&& self, bool pinned_) noexcept { self.pinned = pinned_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_webhook_id(this auto&& self, opt<discusy::snowflake> webhook_id_) noexcept { self.webhook_id = webhook_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_webhook(this auto&& self, opt<discusy::snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_webhook_id(id_); }
        decltype(auto) set_type(this auto&& self, discusy::message::message_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_activity(this auto&& self, opt<discusy::message::message_activity> activity_) { self.activity = std::move(activity_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_application(this auto&& self, opt<discusy::application::application> application_) { self.application = std::move(application_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_application_id(this auto&& self, opt<discusy::snowflake> application_id_) noexcept { self.application_id = application_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_flags(this auto&& self, opt<discusy::flags_t<discusy::message::message_flags>> flags_) noexcept { self.flags = flags_; return std::forward<decltype(self)>(self); }
        decltype(auto) add_flags(this auto&& self, discusy::message::message_flags f) noexcept {
            if (!self.flags) self.flags.emplace();
            self.flags->add_flags(f);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) add_flag(this auto&& self, discusy::message::message_flags f) noexcept { return std::forward<decltype(self)>(self).add_flags(f); }
        decltype(auto) set_message_reference(this auto&& self, opt<discusy::message::message_reference> message_reference_) { self.message_reference = std::move(message_reference_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_message_snapshots(this auto&& self, opt<std::vector<discusy::message::message_snapshot>> message_snapshots_) { self.message_snapshots = std::move(message_snapshots_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_message_snapshots, discusy::message::message_snapshot)
        decltype(auto) add_message_snapshot(this auto&& self, discusy::message::message_snapshot itm) {
            if (!self.message_snapshots) self.message_snapshots.emplace();
            self.message_snapshots->emplace_back(std::move(itm));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_referenced_message(this auto&& self, opt<std::shared_ptr<discusy::message::message>> referenced_message_) { self.referenced_message = std::move(referenced_message_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_interaction_metadata(this auto&& self, opt<discusy::message::message_interaction_metadata> interaction_metadata_) noexcept { self.interaction_metadata = interaction_metadata_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_interaction(this auto&& self, opt<discusy::interaction::message_interaction> interaction_) { self.interaction = std::move(interaction_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_thread(this auto&& self, opt<discusy::channel::channel> thread_) { self.thread = std::move(thread_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_components(this auto&& self, opt<std::vector<components::component>> components_) {
            self.components = std::move(components_);
            if (self.components && !self.components->empty()) {
                self.add_flag(discusy::message::message_flags::IS_COMPONENTS_V2);
            }
            return std::forward<decltype(self)>(self);
        }
        DISCUSY_VARIADIC_SETTER(set_components, components::component)
        decltype(auto) add_component(this auto&& self, components::component itm) {
            if (!self.components) self.components.emplace();
            self.components->emplace_back(std::move(itm));
            self.add_flag(discusy::message::message_flags::IS_COMPONENTS_V2);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) add_action_row(this auto&& self, components::ActionRow row) {
            return std::forward<decltype(self)>(self).add_component(std::move(row));
        }
        decltype(auto) add_container(this auto&& self, components::Container container) {
            return std::forward<decltype(self)>(self).add_component(std::move(container));
        }
        decltype(auto) add_section(this auto&& self, components::Section section) {
            return std::forward<decltype(self)>(self).add_component(std::move(section));
        }
        decltype(auto) add_text_display(this auto&& self, components::TextDisplay td) {
            return std::forward<decltype(self)>(self).add_component(std::move(td));
        }
        decltype(auto) add_text_display(this auto&& self, std::string text) {
            return std::forward<decltype(self)>(self).add_component(self.components::TextDisplay::create(std::move(text)));
        }
        decltype(auto) add_media_gallery(this auto&& self, components::MediaGallery mg) {
            return std::forward<decltype(self)>(self).add_component(std::move(mg));
        }
        decltype(auto) add_file(this auto&& self, components::File f) {
            return std::forward<decltype(self)>(self).add_component(std::move(f));
        }
        decltype(auto) add_separator(this auto&& self, components::Separator sep = components::Separator::create()) {
            return std::forward<decltype(self)>(self).add_component(std::move(sep));
        }
        decltype(auto) set_sticker_items(this auto&& self, opt<std::vector<sticker::sticker_item>> sticker_items_) { self.sticker_items = std::move(sticker_items_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_sticker_items, sticker::sticker_item)
        decltype(auto) add_sticker_item(this auto&& self, sticker::sticker_item itm) {
            if (!self.sticker_items) self.sticker_items.emplace();
            self.sticker_items->emplace_back(std::move(itm));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_stickers(this auto&& self, opt<std::vector<sticker::sticker>> stickers_) { self.stickers = std::move(stickers_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_stickers, sticker::sticker)
        decltype(auto) add_sticker(this auto&& self, sticker::sticker itm) {
            if (!self.stickers) self.stickers.emplace();
            self.stickers->emplace_back(std::move(itm));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_position(this auto&& self, opt<integer> position_) noexcept { self.position = position_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_role_subscription_data(this auto&& self, opt<discusy::message::role_subscription_data> role_subscription_data_) { self.role_subscription_data = std::move(role_subscription_data_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_resolved(this auto&& self, opt<interaction::resolved> resolved_) { self.resolved = std::move(resolved_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_poll(this auto&& self, opt<discusy::poll::poll> poll_) { self.poll = std::move(poll_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_call(this auto&& self, opt<discusy::message::message_call> call_) { self.call = std::move(call_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_shared_client_theme(this auto&& self, opt<discusy::message::shared_client_theme> shared_client_theme_) { self.shared_client_theme = std::move(shared_client_theme_); return std::forward<decltype(self)>(self); }

        discusy::bot* bot_ptr_{nullptr};
        void set_bot_void(this auto&& self, discusy::bot* b) noexcept {
            self.bot_ptr_ = b;
            self.author.set_bot_void(b);
            for (auto& m : self.mentions) m.set_bot_void(b);
            if (self.thread) self.thread->set_bot_void(b);
            if (self.referenced_message && *self.referenced_message) (*self.referenced_message)->set_bot_void(b);
        }
        decltype(auto) set_bot(this auto&& self, discusy::bot* b) noexcept { self.set_bot_void(b); return std::forward<decltype(self)>(self); }

        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto send(this auto&& self, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto reply(this auto&& self, std::string content, bool ping = false, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto reply_embed(this auto&& self, embed e, bool ping = false, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto reply_file(this auto&& self, discusy::upload_file_view f, std::string content = {}, bool ping = false, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto reply_files(this auto&& self, discusy::upload_files_param files, std::string content = {}, bool ping = false, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto reply_with(this auto&& self, api::message::create_message msg, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto edit(this auto&& self, std::string content, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto edit_embed(this auto&& self, embed e, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto edit_with(this auto&& self, api::message::edit_message msg, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto delete_message(this auto&& self, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto add_reaction(this auto&& self, std::string emoji, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto remove_reaction(this auto&& self, std::string emoji, snowflake user_id = {}, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto pin(this auto&& self, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto unpin(this auto&& self, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto create_thread(this auto&& self, std::string name, CompletionToken&& token = ctx::io_context::dct_t());

        [[nodiscard]] const user::user* user_ptr(this const auto& self) noexcept { return &self.author; }
        [[nodiscard]] const user::user* user_of(this const auto& self) noexcept { return &self.author; }
        [[nodiscard]] snowflake user_id(this const auto& self) noexcept { return self.author.id; }
        [[nodiscard]] snowflake user_id_of(this const auto& self) noexcept { return self.author.id; }
    };
};

namespace interaction {

    struct application_command_interaction_data_option {
        std::string name{};
        application_commands::application_command_option_type type{};
        opt<std::variant<std::string, integer, double, bool>> value{};
        opt<std::vector<application_command_interaction_data_option>> options{};
        opt<bool> focused{};

        static application_command_interaction_data_option create(std::string name_ = {}, application_commands::application_command_option_type type_ = application_commands::application_command_option_type::STRING) {
            return application_command_interaction_data_option{
                .name = std::move(name_),
                .type = type_,
            };
        }
        decltype(auto) set_name(this auto&& self, std::string name_) { self.name = std::move(name_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_type(this auto&& self, application_commands::application_command_option_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_value(this auto&& self, opt<std::variant<std::string, integer, double, bool>> value_) { self.value = std::move(value_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_options(this auto&& self, opt<std::vector<application_command_interaction_data_option>> options_) { self.options = std::move(options_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_options, application_command_interaction_data_option)
        decltype(auto) add_option(this auto&& self, application_command_interaction_data_option itm) {
            if (!self.options) self.options.emplace();
            self.options->emplace_back(std::move(itm));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_focused(this auto&& self, opt<bool> focused_) noexcept { self.focused = focused_; return std::forward<decltype(self)>(self); }

        template <typename T>
        [[nodiscard]] std::optional<T> value_as(this const auto& self) noexcept {
            if (!self.value) return std::nullopt;
            if constexpr (std::is_same_v<T, std::string>) {
                if (const auto* v = std::get_if<std::string>(&*self.value)) return *v;
            } else if constexpr (std::is_same_v<T, bool>) {
                if (const auto* v = std::get_if<bool>(&*self.value)) return *v;
            } else if constexpr (std::is_same_v<T, double>) {
                if (const auto* v = std::get_if<double>(&*self.value)) return *v;
                if (const auto* v = std::get_if<integer>(&*self.value)) return static_cast<double>(*v);
            } else if constexpr (std::is_integral_v<T>) {
                if (const auto* v = std::get_if<integer>(&*self.value)) return static_cast<T>(*v);
                if (const auto* v = std::get_if<double>(&*self.value)) return static_cast<T>(*v);
            }
            return std::nullopt;
        }
    };

    struct application_command_data {
        snowflake id{};
        std::string name{};
        application_commands::application_command_type type{};
        opt<discusy::interaction::resolved> resolved{};
        opt<std::vector<application_command_interaction_data_option>> options{};
        opt<snowflake> guild_id{};
        opt<snowflake> target_id{};

        static application_command_data create(snowflake id_ = {}, std::string name_ = {}, application_commands::application_command_type type_ = application_commands::application_command_type::CHAT_INPUT) {
            return application_command_data{
                .id = id_,
                .name = std::move(name_),
                .type = type_,
            };
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_name(this auto&& self, std::string name_) { self.name = std::move(name_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_type(this auto&& self, application_commands::application_command_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_resolved(this auto&& self, opt<discusy::interaction::resolved> resolved_) { self.resolved = std::move(resolved_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_options(this auto&& self, opt<std::vector<application_command_interaction_data_option>> options_) { self.options = std::move(options_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_options, application_command_interaction_data_option)
        decltype(auto) add_option(this auto&& self, application_command_interaction_data_option itm) {
            if (!self.options) self.options.emplace();
            self.options->emplace_back(std::move(itm));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_guild_id(this auto&& self, opt<snowflake> guild_id_) noexcept { self.guild_id = guild_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_guild_id(id_); }
        decltype(auto) set_target_id(this auto&& self, opt<snowflake> target_id_) noexcept { self.target_id = target_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_target(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_target_id(id_); }

        [[nodiscard]] const application_command_interaction_data_option* find_option(this auto&& self, std::string_view opt_name) noexcept;
        [[nodiscard]] const application_command_interaction_data_option* find_subcommand(this auto&& self) noexcept;
        [[nodiscard]] const std::vector<application_command_interaction_data_option>* option_scope(this auto&& self) noexcept;
        [[nodiscard]] command_path subcommand_path(this auto&& self) noexcept;
        [[nodiscard]] std::optional<std::string> get_string(this auto&& self, std::string_view opt_name);
        [[nodiscard]] std::optional<integer> get_integer(this auto&& self, std::string_view opt_name) noexcept;
        [[nodiscard]] std::optional<double> get_double(this auto&& self, std::string_view opt_name) noexcept;
        [[nodiscard]] std::optional<bool> get_bool(this auto&& self, std::string_view opt_name) noexcept;
        [[nodiscard]] std::optional<snowflake> get_snowflake(this auto&& self, std::string_view opt_name);
        [[nodiscard]] const application_command_interaction_data_option* focused_option(this auto&& self) noexcept;
        [[nodiscard]] focused_input focused(this auto&& self) noexcept;
        [[nodiscard]] const discusy::message::attachment* resolved_attachment(this auto&& self, snowflake id) noexcept;
        [[nodiscard]] const discusy::message::attachment* resolved_attachment(this auto&& self, std::string_view opt_name);
        [[nodiscard]] const discusy::user::user* resolved_user(this auto&& self, snowflake id) noexcept;
        [[nodiscard]] const discusy::user::user* resolved_user(this auto&& self, std::string_view opt_name);
        [[nodiscard]] const discusy::channel::channel* resolved_channel(this auto&& self, snowflake id) noexcept;
        [[nodiscard]] const discusy::channel::channel* resolved_channel(this auto&& self, std::string_view opt_name);
        [[nodiscard]] const discusy::permissions::role* resolved_role(this auto&& self, snowflake id) noexcept;
        [[nodiscard]] const discusy::permissions::role* resolved_role(this auto&& self, std::string_view opt_name);
        [[nodiscard]] const discusy::message::message* resolved_message(this auto&& self, snowflake id) noexcept;
        [[nodiscard]] const discusy::message::message* resolved_message(this auto&& self, std::string_view opt_name);
        [[nodiscard]] const discusy::guild::guild_member* resolved_member(this auto&& self, snowflake id) noexcept;
        [[nodiscard]] const discusy::guild::guild_member* resolved_member(this auto&& self, std::string_view opt_name);
    };

    struct message_component_data {
        opt<std::uint32_t> id{}; // ignore
        std::string custom_id{};
        components::component_type component_type{};
        opt<std::vector<std::string>> values{};
        opt<discusy::interaction::resolved> resolved{};

        static message_component_data create(std::string custom_id_ = {}, components::component_type component_type_ = components::component_type::Button) {
            return message_component_data{
                .custom_id = std::move(custom_id_),
                .component_type = component_type_,
            };
        }
        decltype(auto) set_id(this auto&& self, opt<std::uint32_t> id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_custom_id(this auto&& self, std::string custom_id_) { self.custom_id = std::move(custom_id_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_component_type(this auto&& self, components::component_type component_type_) noexcept { self.component_type = component_type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_values(this auto&& self, opt<std::vector<std::string>> values_) { self.values = std::move(values_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_values, std::string)
        decltype(auto) add_value(this auto&& self, std::string itm) {
            if (!self.values) self.values.emplace();
            self.values->emplace_back(std::move(itm));
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_resolved(this auto&& self, opt<discusy::interaction::resolved> resolved_) { self.resolved = std::move(resolved_); return std::forward<decltype(self)>(self); }

        [[nodiscard]] std::string_view first_value(this const auto& self) noexcept {
            return (self.values && !self.values->empty()) ? std::string_view{self.values->front()} : std::string_view{};
        }
        [[nodiscard]] const std::vector<std::string>* values_ptr(this const auto& self) noexcept {
            return self.values ? &*self.values : nullptr;
        }
    };

    #pragma push_macro("COMMAND_INTERACTION_RESPONSE")
    #undef COMMAND_INTERACTION_RESPONSE
    #define COMMAND_INTERACTION_RESPONSE \
        DISCUSY_X(StringSelect) DISCUSY_D \
        DISCUSY_X(TextInput) DISCUSY_D \
        DISCUSY_X(UserSelect) DISCUSY_D \
        DISCUSY_X(RoleSelect) DISCUSY_D \
        DISCUSY_X(MentionableSelect) DISCUSY_D \
        DISCUSY_X(ChannelSelect) DISCUSY_D \
        DISCUSY_X(TextDisplay) DISCUSY_D \
        DISCUSY_X(Label) DISCUSY_D \
        DISCUSY_X(FileUpload) DISCUSY_D \
        DISCUSY_X(RadioGroup) DISCUSY_D \
        DISCUSY_X(CheckboxGroup) DISCUSY_D \
        DISCUSY_X(Checkbox)

    struct component_interaction_response : components::component_variant<component_interaction_response,
        #define DISCUSY_X(name) components::name##InteractionResponse
        COMMAND_INTERACTION_RESPONSE
        #undef DISCUSY_X
    > {
        using component_variant::component_variant;

        static component_interaction_response create() noexcept {
            return component_interaction_response{};
        }
        template <typename T>
        static component_interaction_response create(T&& c) {
            return component_interaction_response{std::forward<T>(c)};
        }
        template <typename T>
        decltype(auto) set_component(this auto&& self, T&& c) {
            *self = std::forward<T>(c);
            return std::forward<decltype(self)>(self);
        }
    };

    struct modal_component_data {
        opt<std::uint32_t> id{}; // ignore
        std::string custom_id{};
        std::vector<component_interaction_response> components{};
        opt<discusy::interaction::resolved> resolved{};

        static modal_component_data create(std::string custom_id_ = {}, std::vector<component_interaction_response> components_ = {}) {
            return modal_component_data{
                .custom_id = std::move(custom_id_),
                .components = std::move(components_),
            };
        }
        decltype(auto) set_id(this auto&& self, opt<std::uint32_t> id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_custom_id(this auto&& self, std::string custom_id_) { self.custom_id = std::move(custom_id_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_components(this auto&& self, std::vector<component_interaction_response> components_) { self.components = std::move(components_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_components, component_interaction_response)
        decltype(auto) add_component(this auto&& self, component_interaction_response itm) { self.components.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_resolved(this auto&& self, opt<discusy::interaction::resolved> resolved_) { self.resolved = std::move(resolved_); return std::forward<decltype(self)>(self); }

        [[nodiscard]] std::string_view text_value(this auto&& self, std::string_view target_custom_id) noexcept;
        [[nodiscard]] const std::vector<std::string>* select_values(this auto&& self, std::string_view target_custom_id) noexcept;
    };

    using interaction_data = std::variant<
        glz::skip, 
        application_command_data, 
        message_component_data,
        modal_component_data
    >;

    struct interaction {
        snowflake id{};
        snowflake application_id{};
        interaction_type type{};
        opt<interaction_data> data{};
        opt<guild::guild> guild{};
        opt<snowflake> guild_id{};
        opt<channel::channel> channel{};
        opt<snowflake> channel_id{};
        opt<guild::guild_member> member{};
        opt<user::user> user{};
        std::string token{};
        std::uint8_t version{1};
        opt<message::message> message{};
        permissions_t app_permissions{};
        opt<std::string> locale{};
        opt<std::string> guild_locale{};
        std::vector<entitlement::entitlement> entitlements{};
        std::unordered_map<application::application_integration_type, snowflake> authorizing_integration_owners{};
        opt<interaction_context_type> context{};
        integer attachment_size_limit{};

        static interaction create(snowflake id_ = {}, snowflake application_id_ = {}, interaction_type type_ = interaction_type::PING) noexcept {
            return interaction{
                .id = id_,
                .application_id = application_id_,
                .type = type_,
            };
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_application_id(this auto&& self, snowflake application_id_) noexcept { self.application_id = application_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_application(this auto&& self, snowflake id_) noexcept { return std::forward<decltype(self)>(self).set_application_id(id_); }
        decltype(auto) set_type(this auto&& self, interaction_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_data(this auto&& self, opt<interaction_data> data_) noexcept { self.data = data_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild(this auto&& self, opt<guild::guild> guild_) { self.guild = std::move(guild_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild_id(this auto&& self, opt<snowflake> guild_id_) noexcept { self.guild_id = guild_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_channel(this auto&& self, opt<channel::channel> channel_) { self.channel = std::move(channel_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_channel_id(this auto&& self, opt<snowflake> channel_id_) noexcept { self.channel_id = channel_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_member(this auto&& self, opt<guild::guild_member> member_) { self.member = std::move(member_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_user(this auto&& self, opt<user::user> user_) { self.user = std::move(user_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_token(this auto&& self, std::string token_) { self.token = std::move(token_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_version(this auto&& self, std::uint8_t version_) noexcept { self.version = version_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_message(this auto&& self, opt<message::message> message_) { self.message = std::move(message_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_app_permissions(this auto&& self, permissions_t app_permissions_) noexcept { self.app_permissions = app_permissions_; return std::forward<decltype(self)>(self); }
        decltype(auto) add_app_permission(this auto&& self, permissions::permissions p) noexcept { self.app_permissions.add_flags(p); return std::forward<decltype(self)>(self); }
        decltype(auto) add_app_permissions(this auto&& self, permissions::permissions p) noexcept { return std::forward<decltype(self)>(self).add_app_permission(p); }
        decltype(auto) set_locale(this auto&& self, opt<std::string> locale_) { self.locale = std::move(locale_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild_locale(this auto&& self, opt<std::string> guild_locale_) { self.guild_locale = std::move(guild_locale_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_entitlements(this auto&& self, std::vector<entitlement::entitlement> entitlements_) { self.entitlements = std::move(entitlements_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_entitlements, entitlement::entitlement)
        decltype(auto) add_entitlement(this auto&& self, entitlement::entitlement itm) { self.entitlements.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_authorizing_integration_owners(this auto&& self, std::unordered_map<application::application_integration_type, snowflake> authorizing_integration_owners_) { self.authorizing_integration_owners = std::move(authorizing_integration_owners_); return std::forward<decltype(self)>(self); }
        decltype(auto) add_authorizing_integration_owner(this auto&& self, application::application_integration_type k, snowflake v) {
            self.authorizing_integration_owners[k] = v;
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) set_context(this auto&& self, opt<interaction_context_type> context_) noexcept { self.context = context_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_attachment_size_limit(this auto&& self, integer attachment_size_limit_) noexcept { self.attachment_size_limit = attachment_size_limit_; return std::forward<decltype(self)>(self); }

        discusy::bot* bot_ptr_{nullptr};
        void set_bot_void(this auto&& self, discusy::bot* b) noexcept {
            self.bot_ptr_ = b;
            if (self.member) {
                self.member->set_bot_void(b);
                self.member->set_guild_id_context_void(self.guild_id);
            }
            if (self.message) self.message->set_bot_void(b);
            if (self.user) self.user->set_bot_void(b);
            if (self.channel) self.channel->set_bot_void(b);
            if (self.guild) self.guild->set_bot_void(b);
        }
        decltype(auto) set_bot(this auto&& self, discusy::bot* b) noexcept { self.set_bot_void(b); return std::forward<decltype(self)>(self); }

        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto reply(this auto&& self, std::string content, bool ephemeral = false, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto reply_ephemeral(this auto&& self, std::string content, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto reply_embed(this auto&& self, message::embed e, bool ephemeral = false, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto reply_components(this auto&& self, std::vector<components::component> components_, bool ephemeral = false, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto reply_file(this auto&& self, discusy::upload_file_view f, std::string content = {}, bool ephemeral = false, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto reply_files(this auto&& self, discusy::upload_files_param files, std::string content = {}, bool ephemeral = false, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto reply_with(this auto&& self, api::interaction::interaction_callback_data_message data, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto defer(this auto&& self, bool ephemeral = false, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto thinking(this auto&& self, bool ephemeral = true, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto defer_ephemeral(this auto&& self, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto edit_reply(this auto&& self, std::string content, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto edit_reply_embed(this auto&& self, message::embed e, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto edit_reply_with(this auto&& self, api::webhook::edit_webhook_message msg, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto clear_components(this auto&& self, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto followup(this auto&& self, std::string content, bool ephemeral = false, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto followup_with(this auto&& self, api::webhook::execute_webhook msg, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto followup_file(this auto&& self, discusy::upload_file_view f, std::string content = {}, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto followup_files(this auto&& self, discusy::upload_files_param files, std::string content = {}, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto autocomplete(this auto&& self, std::vector<application_commands::application_command_option_choice> choices, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto modal(this auto&& self, std::string custom_id, std::string title, std::vector<components::component> components_, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto update_message(this auto&& self, api::interaction::interaction_callback_data_message data, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto defer_update(this auto&& self, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto acknowledge_component_interaction(this auto&& self, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto acknowledge_component(this auto&& self, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto acknowledge(this auto&& self, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto reply_to_component_interaction(this auto&& self, std::vector<components::component> components_, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto reply_to_component_interaction(this auto&& self, api::interaction::interaction_callback_data_message data, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto reply_to_component(this auto&& self, std::vector<components::component> components_, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto reply_to_component(this auto&& self, api::interaction::interaction_callback_data_message data, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto reply_to_component_embed(this auto&& self, message::embed e, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto reply_to_component_components(this auto&& self, std::vector<components::component> components_, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto reply_to_component_file(this auto&& self, discusy::upload_file_view f, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto reply_to_component_file(this auto&& self, discusy::upload_file_view f, std::vector<components::component> components_, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto reply_to_component_files(this auto&& self, discusy::upload_files_param files, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto reply_to_component_files(this auto&& self, discusy::upload_files_param files, std::vector<components::component> components_, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto reply_component_file(this auto&& self, discusy::upload_file_view f, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto reply_component_file(this auto&& self, discusy::upload_file_view f, std::vector<components::component> components_, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto reply_component_files(this auto&& self, discusy::upload_files_param files, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto reply_component_files(this auto&& self, discusy::upload_files_param files, std::vector<components::component> components_, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto reply_component_interaction(this auto&& self, std::vector<components::component> components_, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto reply_component_interaction(this auto&& self, api::interaction::interaction_callback_data_message data, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto reply_component(this auto&& self, std::vector<components::component> components_, CompletionToken&& token = ctx::io_context::dct_t());
        template <bool ReturnResult = false, typename CompletionToken = ctx::io_context::dct_t>
        auto reply_component(this auto&& self, api::interaction::interaction_callback_data_message data, CompletionToken&& token = ctx::io_context::dct_t());


        [[nodiscard]] std::string_view command_name(this auto&& self) noexcept;
        [[nodiscard]] const application_command_data* command_data(this auto&& self) noexcept;
        [[nodiscard]] const message_component_data* component_data(this auto&& self) noexcept;
        [[nodiscard]] const modal_component_data* modal_data(this auto&& self) noexcept;
        [[nodiscard]] bool is_command(this auto&& self) noexcept;
        [[nodiscard]] bool is_autocomplete(this auto&& self) noexcept;
        [[nodiscard]] bool is_component(this auto&& self) noexcept;
        [[nodiscard]] bool is_modal_submit(this auto&& self) noexcept;
        [[nodiscard]] command_path subcommand_path(this auto&& self) noexcept;
        [[nodiscard]] std::optional<std::string> get_string(this auto&& self, std::string_view opt_name);
        [[nodiscard]] std::optional<integer> get_integer(this auto&& self, std::string_view opt_name) noexcept;
        [[nodiscard]] std::optional<double> get_double(this auto&& self, std::string_view opt_name) noexcept;
        [[nodiscard]] std::optional<bool> get_bool(this auto&& self, std::string_view opt_name) noexcept;
        [[nodiscard]] std::optional<snowflake> get_snowflake(this auto&& self, std::string_view opt_name);
        [[nodiscard]] focused_input focused(this auto&& self) noexcept;
        [[nodiscard]] std::string_view component_custom_id(this auto&& self) noexcept;
        [[nodiscard]] const std::vector<std::string>* component_values(this auto&& self) noexcept;
        [[nodiscard]] std::string_view component_first_value(this auto&& self) noexcept;
        [[nodiscard]] std::string_view modal_custom_id(this auto&& self) noexcept;
        [[nodiscard]] std::string_view modal_text_value(this auto&& self, std::string_view custom_id) noexcept;
        [[nodiscard]] const std::vector<std::string>* modal_select_values(this auto&& self, std::string_view custom_id) noexcept;
        [[nodiscard]] bool bot_can(this auto&& self, permissions::permissions perm) noexcept;
        [[nodiscard]] bool user_can(this auto&& self, permissions::permissions perm) noexcept;
        [[nodiscard]] const user::user* user_ptr(this auto&& self) noexcept;
        [[nodiscard]] const user::user* user_of(this auto&& self) noexcept;
        [[nodiscard]] snowflake user_id(this auto&& self) noexcept;
        [[nodiscard]] snowflake user_id_of(this auto&& self) noexcept;
        [[nodiscard]] std::string_view display_name(this auto&& self) noexcept;
        [[nodiscard]] std::string_view display_name_of(this auto&& self) noexcept;
        [[nodiscard]] bool in_guild(this auto&& self) noexcept;
        [[nodiscard]] snowflake guild_id_of(this auto&& self) noexcept;
        [[nodiscard]] snowflake guild_id_or_default(this auto&& self) noexcept;
        [[nodiscard]] snowflake channel_id_of(this auto&& self) noexcept;
        [[nodiscard]] const application_command_interaction_data_option* find_option(this auto&& self, std::string_view opt_name) noexcept;
        [[nodiscard]] const application_command_interaction_data_option* find_subcommand(this auto&& self) noexcept;
        [[nodiscard]] const std::vector<application_command_interaction_data_option>* option_scope(this auto&& self) noexcept;
        [[nodiscard]] const application_command_interaction_data_option* focused_option(this auto&& self) noexcept;
        [[nodiscard]] const message::attachment* resolved_attachment(this auto&& self, snowflake id) noexcept;
        [[nodiscard]] const message::attachment* resolved_attachment(this auto&& self, std::string_view opt_name);
        [[nodiscard]] const user::user* resolved_user(this auto&& self, snowflake id) noexcept;
        [[nodiscard]] const user::user* resolved_user(this auto&& self, std::string_view opt_name);
        [[nodiscard]] const channel::channel* resolved_channel(this auto&& self, snowflake id) noexcept;
        [[nodiscard]] const channel::channel* resolved_channel(this auto&& self, std::string_view opt_name);
        [[nodiscard]] const permissions::role* resolved_role(this auto&& self, snowflake id) noexcept;
        [[nodiscard]] const permissions::role* resolved_role(this auto&& self, std::string_view opt_name);
        [[nodiscard]] const message::message* resolved_message(this auto&& self, snowflake id) noexcept;
        [[nodiscard]] const message::message* resolved_message(this auto&& self, std::string_view opt_name);
        [[nodiscard]] const guild::guild_member* resolved_member(this auto&& self, snowflake id) noexcept;
        [[nodiscard]] const guild::guild_member* resolved_member(this auto&& self, std::string_view opt_name);
    };
}

namespace webhook {
    enum class webhook_type : std::uint8_t {
        Incoming = 1, // Incoming Webhooks can post messages to channels with a generated token
        ChannelFollower = 2, // Channel Follower Webhooks are internal webhooks used with Channel Following to post new messages into channels
        Application = 3, // Application webhooks are webhooks used with Interactions
    };

    struct webhook {
        snowflake id{};
        webhook_type type{};
        opt<snowflake> guild_id{}; // nullable
        opt<snowflake> channel_id{}; // nullable
        opt<user::user> user{};
        opt<std::string> name{}; // nullable
        opt<user_avatar_hash> avatar{}; // nullable; served from the avatars/ path keyed by webhook id
        opt<std::string> token{};
        opt<snowflake> application_id{}; // nullable
        opt<guild::guild> source_guild{};
        opt<channel::channel> source_channel{};
        opt<std::string> url{};

        static webhook create(snowflake id_ = {}, webhook_type type_ = webhook_type::Incoming) noexcept {
            return webhook{.id = id_, .type = type_};
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_type(this auto&& self, webhook_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild_id(this auto&& self, opt<snowflake> guild_id_) noexcept { self.guild_id = guild_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_guild_id(id_); }
        decltype(auto) set_channel_id(this auto&& self, opt<snowflake> channel_id_) noexcept { self.channel_id = channel_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_channel(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_channel_id(id_); }
        decltype(auto) set_user(this auto&& self, opt<user::user> user_) { self.user = std::move(user_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_name(this auto&& self, opt<std::string> name_) { self.name = std::move(name_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_avatar(this auto&& self, opt<user_avatar_hash> avatar_) { self.avatar = std::move(avatar_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_token(this auto&& self, opt<std::string> token_) { self.token = std::move(token_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_application_id(this auto&& self, opt<snowflake> application_id_) noexcept { self.application_id = application_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_application(this auto&& self, opt<snowflake> id_) noexcept { return std::forward<decltype(self)>(self).set_application_id(id_); }
        decltype(auto) set_source_guild(this auto&& self, opt<guild::guild> source_guild_) { self.source_guild = std::move(source_guild_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_source_channel(this auto&& self, opt<channel::channel> source_channel_) { self.source_channel = std::move(source_channel_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_url(this auto&& self, opt<std::string> url_) { self.url = std::move(url_); return std::forward<decltype(self)>(self); }
    };
}

namespace sku {
    enum class sku_type : std::uint8_t {
        DURABLE = 2, // Durable one-time purchase
        CONSUMABLE = 3, // Consumable one-time purchase
        SUBSCRIPTION = 5, // Represents a recurring subscription
        SUBSCRIPTION_GROUP = 6, // System-generated group for each SUBSCRIPTION SKU created
    };

    enum class sku_flags : std::uint16_t {
        AVAILABLE = 1ULL << 2, // SKU is available for purchase
        GUILD_SUBSCRIPTION = 1ULL << 7, // Recurring SKU that can be purchased by a user and applied to a single server
        USER_SUBSCRIPTION = 1ULL << 8, // Recurring SKU purchased by a user for themselves
    };

    struct sku {
        snowflake id{};
        sku_type type{};
        snowflake application_id{};
        std::string name{};
        std::string slug{};
        flags_t<sku_flags> flags{};

        static sku create(snowflake id_ = {}, sku_type type_ = sku_type::DURABLE, snowflake application_id_ = {}, std::string name_ = {}) {
            return sku{
                .id = id_,
                .type = type_,
                .application_id = application_id_,
                .name = std::move(name_),
            };
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_type(this auto&& self, sku_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_application_id(this auto&& self, snowflake application_id_) noexcept { self.application_id = application_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_application(this auto&& self, snowflake id_) noexcept { return std::forward<decltype(self)>(self).set_application_id(id_); }
        decltype(auto) set_name(this auto&& self, std::string name_) { self.name = std::move(name_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_slug(this auto&& self, std::string slug_) { self.slug = std::move(slug_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_flags(this auto&& self, flags_t<sku_flags> flags_) noexcept { self.flags = flags_; return std::forward<decltype(self)>(self); }
        decltype(auto) add_flags(this auto&& self, sku_flags f) noexcept { self.flags.add_flags(f); return std::forward<decltype(self)>(self); }
        decltype(auto) add_flag(this auto&& self, sku_flags f) noexcept { return std::forward<decltype(self)>(self).add_flags(f); }
    };
}

namespace guild_template {
    struct guild_template {
        std::string code{};
        std::string name{};
        opt<std::string> description{}; // nullable
        integer usage_count{};
        snowflake creator_id{};
        user::user creator{};
        timestamp created_at{};
        timestamp updated_at{};
        snowflake source_guild_id{};
        guild::guild serialized_source_guild{}; // partial
        opt<bool> is_dirty{}; // nullable

        static guild_template create(std::string code_ = {}, std::string name_ = {}) {
            return guild_template{.code = std::move(code_), .name = std::move(name_)};
        }
        decltype(auto) set_code(this auto&& self, std::string code_) { self.code = std::move(code_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_name(this auto&& self, std::string name_) { self.name = std::move(name_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_description(this auto&& self, opt<std::string> description_) { self.description = std::move(description_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_usage_count(this auto&& self, integer usage_count_) noexcept { self.usage_count = usage_count_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_creator_id(this auto&& self, snowflake creator_id_) noexcept { self.creator_id = creator_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_creator(this auto&& self, user::user creator_) { self.creator = std::move(creator_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_created_at(this auto&& self, timestamp created_at_) noexcept { self.created_at = created_at_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_updated_at(this auto&& self, timestamp updated_at_) noexcept { self.updated_at = updated_at_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_source_guild_id(this auto&& self, snowflake source_guild_id_) noexcept { self.source_guild_id = source_guild_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_source_guild(this auto&& self, snowflake id_) noexcept { return std::forward<decltype(self)>(self).set_source_guild_id(id_); }
        decltype(auto) set_serialized_source_guild(this auto&& self, guild::guild serialized_source_guild_) { self.serialized_source_guild = std::move(serialized_source_guild_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_is_dirty(this auto&& self, opt<bool> is_dirty_) noexcept { self.is_dirty = is_dirty_; return std::forward<decltype(self)>(self); }
    };
}

namespace lobby {
    enum class lobby_member_flags : std::uint8_t {
        CanLinkLobby = 1ULL << 0, // user can link a text channel to a lobby
    };

    using lobby_metadata = std::unordered_map<std::string, std::string>;

    struct lobby_member {
        snowflake id{};
        opt<lobby_metadata> metadata{}; // nullable
        opt<flags_t<lobby_member_flags>> flags{};

        static lobby_member create(snowflake id_ = {}) noexcept {
            return lobby_member{.id = id_};
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_metadata(this auto&& self, opt<lobby_metadata> metadata_) noexcept { self.metadata = metadata_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_flags(this auto&& self, opt<flags_t<lobby_member_flags>> flags_) noexcept { self.flags = flags_; return std::forward<decltype(self)>(self); }
        decltype(auto) add_flags(this auto&& self, lobby_member_flags f) noexcept {
            if (!self.flags) self.flags.emplace();
            self.flags->add_flags(f);
            return std::forward<decltype(self)>(self);
        }
        decltype(auto) add_flag(this auto&& self, lobby_member_flags f) noexcept { return std::forward<decltype(self)>(self).add_flags(f); }
    };

    struct lobby {
        snowflake id{};
        snowflake application_id{};
        opt<lobby_metadata> metadata{}; // nullable
        std::vector<lobby_member> members{};
        opt<channel::channel> linked_channel{};

        static lobby create(snowflake id_ = {}, snowflake application_id_ = {}) noexcept {
            return lobby{.id = id_, .application_id = application_id_};
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_application_id(this auto&& self, snowflake application_id_) noexcept { self.application_id = application_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_application(this auto&& self, snowflake id_) noexcept { return std::forward<decltype(self)>(self).set_application_id(id_); }
        decltype(auto) set_metadata(this auto&& self, opt<lobby_metadata> metadata_) noexcept { self.metadata = metadata_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_members(this auto&& self, std::vector<lobby_member> members_) { self.members = std::move(members_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_members, lobby_member)
        decltype(auto) add_member(this auto&& self, lobby_member itm) { self.members.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_linked_channel(this auto&& self, opt<channel::channel> linked_channel_) { self.linked_channel = std::move(linked_channel_); return std::forward<decltype(self)>(self); }
    };

    struct lobby_message {
        snowflake id{};
        message::message_type type{};
        std::string content{};
        snowflake lobby_id{};
        snowflake channel_id{}; // equal to lobby_id
        user::user author{};
        opt<lobby_metadata> metadata{}; // nullable
        opt<lobby_metadata> moderation_metadata{}; // nullable
        flags_t<message::message_flags> flags{};
        snowflake application_id{};

        static lobby_message create(snowflake id_ = {}, std::string content_ = {}, snowflake lobby_id_ = {}, user::user author_ = {}) {
            return lobby_message{
                .id = id_,
                .content = std::move(content_),
                .lobby_id = lobby_id_,
                .author = std::move(author_),
            };
        }
        decltype(auto) set_id(this auto&& self, snowflake id_) noexcept { self.id = id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_type(this auto&& self, message::message_type type_) noexcept { self.type = type_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_content(this auto&& self, std::string content_) { self.content = std::move(content_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_lobby_id(this auto&& self, snowflake lobby_id_) noexcept { self.lobby_id = lobby_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_lobby(this auto&& self, snowflake id_) noexcept { return std::forward<decltype(self)>(self).set_lobby_id(id_); }
        decltype(auto) set_channel_id(this auto&& self, snowflake channel_id_) noexcept { self.channel_id = channel_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_channel(this auto&& self, snowflake id_) noexcept { return std::forward<decltype(self)>(self).set_channel_id(id_); }
        decltype(auto) set_author(this auto&& self, user::user author_) { self.author = std::move(author_); return std::forward<decltype(self)>(self); }
        decltype(auto) set_metadata(this auto&& self, opt<lobby_metadata> metadata_) noexcept { self.metadata = metadata_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_moderation_metadata(this auto&& self, opt<lobby_metadata> moderation_metadata_) noexcept { self.moderation_metadata = moderation_metadata_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_flags(this auto&& self, flags_t<message::message_flags> flags_) noexcept { self.flags = flags_; return std::forward<decltype(self)>(self); }
        decltype(auto) add_flags(this auto&& self, message::message_flags f) noexcept { self.flags.add_flags(f); return std::forward<decltype(self)>(self); }
        decltype(auto) add_flag(this auto&& self, message::message_flags f) noexcept { return std::forward<decltype(self)>(self).add_flags(f); }
        decltype(auto) set_application_id(this auto&& self, snowflake application_id_) noexcept { self.application_id = application_id_; return std::forward<decltype(self)>(self); }
        decltype(auto) set_application(this auto&& self, snowflake id_) noexcept { return std::forward<decltype(self)>(self).set_application_id(id_); }
    };

    struct lobby_invite {
        std::string code{};

        static lobby_invite create(std::string code_ = {}) {
            return lobby_invite{.code = std::move(code_)};
        }
        decltype(auto) set_code(this auto&& self, std::string code_) { self.code = std::move(code_); return std::forward<decltype(self)>(self); }
    };
}

namespace audit_log {
    struct audit_log {
        std::vector<application_commands::application_command> application_commands{};
        std::vector<audit_log_entry> audit_log_entries{};
        std::vector<auto_moderation::auto_moderation_rule> auto_moderation_rules{};
        std::vector<guild_scheduled_event::guild_scheduled_event> guild_scheduled_events{};
        std::vector<guild::integration> integrations{};
        std::vector<channel::channel> threads{};
        std::vector<user::user> users{};
        std::vector<webhook::webhook> webhooks{};

        static audit_log create() noexcept {
            return audit_log{};
        }
        decltype(auto) set_application_commands(this auto&& self, std::vector<application_commands::application_command> application_commands_) { self.application_commands = std::move(application_commands_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_application_commands, application_commands::application_command)
        decltype(auto) add_application_command(this auto&& self, application_commands::application_command itm) { self.application_commands.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_audit_log_entries(this auto&& self, std::vector<audit_log_entry> audit_log_entries_) { self.audit_log_entries = std::move(audit_log_entries_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_audit_log_entries, audit_log_entry)
        decltype(auto) add_audit_log_entry(this auto&& self, audit_log_entry itm) { self.audit_log_entries.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_auto_moderation_rules(this auto&& self, std::vector<auto_moderation::auto_moderation_rule> auto_moderation_rules_) { self.auto_moderation_rules = std::move(auto_moderation_rules_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_auto_moderation_rules, auto_moderation::auto_moderation_rule)
        decltype(auto) add_auto_moderation_rule(this auto&& self, auto_moderation::auto_moderation_rule itm) { self.auto_moderation_rules.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_guild_scheduled_events(this auto&& self, std::vector<guild_scheduled_event::guild_scheduled_event> guild_scheduled_events_) { self.guild_scheduled_events = std::move(guild_scheduled_events_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_guild_scheduled_events, guild_scheduled_event::guild_scheduled_event)
        decltype(auto) add_guild_scheduled_event(this auto&& self, guild_scheduled_event::guild_scheduled_event itm) { self.guild_scheduled_events.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_integrations(this auto&& self, std::vector<guild::integration> integrations_) { self.integrations = std::move(integrations_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_integrations, guild::integration)
        decltype(auto) add_integration(this auto&& self, guild::integration itm) { self.integrations.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_threads(this auto&& self, std::vector<channel::channel> threads_) { self.threads = std::move(threads_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_threads, channel::channel)
        decltype(auto) add_thread(this auto&& self, channel::channel itm) { self.threads.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_users(this auto&& self, std::vector<user::user> users_) { self.users = std::move(users_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_users, user::user)
        decltype(auto) add_user(this auto&& self, user::user itm) { self.users.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
        decltype(auto) set_webhooks(this auto&& self, std::vector<webhook::webhook> webhooks_) { self.webhooks = std::move(webhooks_); return std::forward<decltype(self)>(self); }
        DISCUSY_VARIADIC_SETTER(set_webhooks, webhook::webhook)
        decltype(auto) add_webhook(this auto&& self, webhook::webhook itm) { self.webhooks.emplace_back(std::move(itm)); return std::forward<decltype(self)>(self); }
    };
}
}

#ifdef __clang__
#pragma clang diagnostic pop
#pragma clang diagnostic pop
#endif

template <>
struct glz::meta<discusy::team::team_member_role> {
    using enum discusy::team::team_member_role;
    static constexpr auto value = glz::enumerate(
        "", owner,
        "admin", admin,
        "developer", developer,
        "read_only", read_only
    );
};

template <>
struct glz::meta<discusy::message::embed_type> {
    using enum discusy::message::embed_type;
    static constexpr auto value = glz::enumerate(
        rich, image, video, gifv, article, link, poll_result
    );
};

template <>
struct glz::meta<discusy::components::select_default_value_type> {
    using enum discusy::components::select_default_value_type;
    static constexpr auto value = glz::enumerate(
        user, role, channel
    );
};

#define DISCUSY_X(name, ...) \
case discusy::components::component_type::name: { \
    auto& elem = wrapper.template emplace<discusy::components::name>(); \
    if(glz::read<opts>(elem, raw.str)) { \
        ctx.error = glz::error_code::parse_error; \
        ctx.custom_error_message = "Failed to parse JSON for " #name " component"; \
        return; \
    } \
    break; \
}

#pragma push_macro("GEN_COMPONENT_META")
#undef GEN_COMPONENT_META

#define GEN_COMPONENT_META(name, macro) \
template <> \
struct glz::meta<discusy::components::name> { \
    using T = discusy::components::name; \
    static constexpr auto read_fn = [](T& wrapper, const glz::raw_json& raw, glz::context& ctx) { \
        discusy::components::type_extractor ext; \
        \
        static constexpr glz::opts opts{ \
            .null_terminated = false, \
            .error_on_unknown_keys = false, \
            .minified = true, \
        }; \
        \
        if (glz::read<opts>(ext, raw.str)) { \
            ctx.error = glz::error_code::parse_error; \
            ctx.custom_error_message = "Failed to parse JSON for type extraction"; \
            return; \
        } \
        \
        switch (ext.type) { \
            macro \
            default: { \
                ctx.error = glz::error_code::parse_error; \
                ctx.custom_error_message = "Unknown component type"; \
                return; \
            } \
        } \
    }; \
    static constexpr auto write_fn = [](const T& wrapper) -> const typename T::variant_base& { return static_cast<const typename T::variant_base&>(wrapper); }; \
    static constexpr auto value = glz::custom<read_fn, write_fn>; \
};

#pragma push_macro("DISCUSY_D")
#undef DISCUSY_D
#define DISCUSY_D

GEN_COMPONENT_META(action_row_component, ACTION_ROW_COMPONENTS)
GEN_COMPONENT_META(component, COMPONENTS)
GEN_COMPONENT_META(section_accessory, SECTION_ACCESSORY)
GEN_COMPONENT_META(container_child_component, CONTAINER_CHILD_COMPONENTS)
GEN_COMPONENT_META(label_child_component, LABEL_CHILD_COMPONENTS)

#undef DISCUSY_D
#pragma pop_macro("DISCUSY_D")

#undef ACTION_ROW_COMPONENTS
#pragma pop_macro("ACTION_ROW_COMPONENTS")
#undef COMPONENTS
#pragma pop_macro("COMPONENTS")
#undef SECTION_ACCESSORY
#pragma pop_macro("SECTION_ACCESSORY")
#undef GEN_COMPONENT_META
#pragma pop_macro("GEN_COMPONENT_META")
#undef CONTAINER_CHILD_COMPONENTS
#pragma pop_macro("CONTAINER_CHILD_COMPONENTS")
#undef LABEL_CHILD_COMPONENTS
#pragma pop_macro("LABEL_CHILD_COMPONENTS")
#undef DISCUSY_X

template <>
struct glz::meta<discusy::interaction::component_interaction_response> {
    using T = discusy::interaction::component_interaction_response;
    static constexpr auto read_fn = [](T& wrapper, const glz::raw_json& raw, glz::context& ctx) {
        discusy::components::ir_type_extractor ext;

        static constexpr glz::opts opts{
            .null_terminated = false,
            .error_on_unknown_keys = false,
            .minified = true,
        };
        
        if (glz::read<opts>(ext, raw.str)) {
            ctx.error = glz::error_code::parse_error;
            ctx.custom_error_message = "Failed to parse JSON for type extraction";
            return;
        }

        const auto val = (ext.type && (std::to_underlying(*ext.type) > 0)) ? *ext.type : (ext.component_type) ? *ext.component_type : static_cast<discusy::components::component_type>(0);

        if (std::to_underlying(val) == 0) {
            ctx.error = glz::error_code::parse_error;
            ctx.custom_error_message = "Failed to get type for component interaction response";
            return;
        }

        switch (val) {
            #pragma push_macro("DISCUSY_D")
            #undef DISCUSY_D
            #define DISCUSY_D
            #define DISCUSY_X(name) \
            case discusy::components::component_type::name: { \
                auto& elem = wrapper.template emplace<discusy::components::name##InteractionResponse>(); \
                if(glz::read<opts>(elem, raw.str)) { \
                    ctx.error = glz::error_code::parse_error; \
                    ctx.custom_error_message = "Failed to parse JSON for " #name " component interaction response"; \
                    return; \
                } \
                break; \
            }
            COMMAND_INTERACTION_RESPONSE
            #undef DISCUSY_X
            #undef DISCUSY_D
            #pragma pop_macro("DISCUSY_D")
            default: {
                ctx.error = glz::error_code::parse_error;
                ctx.custom_error_message = "Unknown component interaction response type";
                return;
            }
        }
    };
    static constexpr auto write_fn = [](const T& wrapper) -> const typename T::variant_base& { return static_cast<const T::variant_base&>(wrapper); };
    static constexpr auto value = glz::custom<read_fn, write_fn>;
};

#undef COMMAND_INTERACTION_RESPONSE
#pragma pop_macro("COMMAND_INTERACTION_RESPONSE")
#undef DISCUSY_X
#pragma pop_macro("DISCUSY_X")
#undef DISCUSY_D
#pragma pop_macro("DISCUSY_D")
