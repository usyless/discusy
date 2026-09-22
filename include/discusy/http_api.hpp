#pragma once

#include <expected>
#include <vector>
#include <utility>
#include <optional>
#include <type_traits>
#include <ranges>
#include <span>

#include <glaze/glaze.hpp>

#include <usylibpp/strings.hpp>

#include "state.hpp"
#include "events.hpp" // technically defines macros but this file will only ever be included correctly in discusy.hpp anyway
#include "http_client.hpp"
#include "urls.hpp"
#include "config.hpp"
#include "json.hpp"
#include "api_types.hpp"
#include "multipart.hpp"
#include "asio_helpers.hpp"

namespace discusy {

template <typename T>
struct is_optional : std::false_type {};

template <typename U>
struct is_optional<std::optional<U>> : std::true_type {};

template <typename T>
inline constexpr bool is_optional_v = is_optional<std::remove_cvref_t<T>>::value;

template <typename T>
inline constexpr bool is_iterable_v = 
    std::ranges::range<T> && 
    !std::is_same_v<T, std::string> && 
    !std::is_same_v<T, std::string_view>;

// struct names must not need to be url encoded
// does this work with glz meta? idk
template <typename T>
requires ( glz::has_reflect<T> )
[[nodiscard]] constexpr std::string make_query_string(const T& obj) {
    std::string q{"?"};

    static constexpr auto append_value = [](std::string& q, const auto& val) constexpr -> void {
        using ActualValType = std::remove_cvref_t<decltype(val)>;
        
        if constexpr (std::is_same_v<ActualValType, bool>) {
            q.push_back(val ? '1' : '0');
        }
        else if constexpr (std::is_convertible_v<ActualValType, std::string_view>) {
            q += ulp::str::url_encode(val);
        }
        else {
            std::string temp;
            if (glz::write_json(val, temp)) return;
            
            if (temp.size() >= 2 && temp.front() == '"' && temp.back() == '"') {
                q += ulp::str::url_encode(std::string_view{temp.data() + 1, temp.size() - 2});
            } else {
                q += ulp::str::url_encode(temp);
            }
        }
    };

    static constexpr auto process_actual = [](std::string& q, const auto& val, std::string_view name) constexpr -> void {
        using ActualType = std::remove_cvref_t<decltype(val)>;
        
        if constexpr (is_iterable_v<ActualType>) {
            for (const auto& item : val) {
                q += name;
                q.push_back('=');
                append_value(q, item);
                q.push_back('&');
            }
        } else {
            q += name;
            q.push_back('=');
            append_value(q, val);
            q.push_back('&');
        }
    };

    static constexpr auto N = glz::reflect<T>::size;
    if constexpr (N > 0) {
        [&]<std::size_t... I>(std::index_sequence<I...>) constexpr {
            static constexpr auto process_member = [](std::string& q, const T& obj, auto index_const) constexpr -> void {
                constexpr std::size_t idx = decltype(index_const)::value;
                constexpr std::string_view name = glz::get<idx>(glz::member_names<T>);
                const auto& value = glz::get<idx>(glz::to_tie(obj));
                using ValueType = std::remove_cvref_t<decltype(value)>;

                if constexpr (is_optional_v<ValueType>) {
                    if (value) {
                        process_actual(q, *value, name);
                    }
                } else {
                    process_actual(q, value, name);
                }
            };
            (process_member(q, obj, std::integral_constant<std::size_t, I>{}), ...);
        }(std::make_index_sequence<N>{});

        if (q.size() == 1) return {};
        if (q.back() == '&') q.pop_back();

        return q;
    } else {
        return {};
    }
}

// only valid to call once the shards have been initialised at least once
class http_api {
public:
    template <typename T>
    using callback_t = discusy::api::result<T>&&;

    template <typename T>
    using co_callback_t = discusy::api::result<T>;

    template <typename T>
    using result_t = discusy::api::result<T>;

    using upload_file = discusy::upload_file;
    using upload_file_view = discusy::upload_file_view;

    // id to data
    using upload_files_param = discusy::upload_files_param;

    struct raw_data { // has glz meta
        std::string data{};
    };
private:
    discusy::state& state_;
    const discusy::config& cfg_;

    // resp has to be non-const to allow glaze optimisations
    template <typename T>
    decltype(boost::asio::deferred_t::values(boost::system::error_code{}, discusy::api::result<T>{})) value_from_api_response(const boost::system::error_code& ec, auto&& resp) {
        if (ec) {
            return boost::asio::deferred_t::values(ec, discusy::api::result<T>{
                .error_info_{{.message = ec.message(), .code = 0, .http_status = resp.status_code}},
            });
        }

        if (resp.status_code >= 200 && resp.status_code < 300) {
            if constexpr (std::is_void_v<T>) {
                return boost::asio::deferred_t::values(boost::system::error_code{}, discusy::api::result<void>{
                    .success = true,
                });
            } else {
                T val{};
                if (discusy::json::parse_json(val, resp.body)) {
                    boost::system::error_code err = boost::asio::error::fault;
                    return boost::asio::deferred_t::values(err, discusy::api::result<T>{
                        .error_info_{{.message = "JSON Parsing error", .code = 0, .http_status = resp.status_code}},
                    });
                }
                discusy::attach_bot(val, state_.bot_ptr);
                return boost::asio::deferred_t::values(boost::system::error_code{}, discusy::api::result<T>{
                    .data{std::move(val)},
                });
            }
        } else {
            discusy::api::error err{};
            std::string err_msg;
            if (discusy::json::parse_json<discusy::json::glz_opts_non_minified>(err, resp.body)) {
                err_msg = resp.body.empty() ? ("HTTP error " + std::string{ulp::str::to_string_view(resp.status_code)}) : "JSON Parsing error while parsing error";
            } else {
                err_msg = std::move(err.message);
            }

            boost::system::error_code http_ec;
            if (resp.status_code == 401 || resp.status_code == 403) {
                http_ec = boost::asio::error::no_permission;
            } else if (resp.status_code == 404) {
                http_ec = boost::asio::error::not_found;
            } else if (resp.status_code == 429) {
                http_ec = boost::asio::error::try_again;
            } else if (resp.status_code >= 500) {
                http_ec = boost::asio::error::connection_refused;
            } else {
                http_ec = boost::asio::error::invalid_argument;
            }

            return boost::asio::deferred_t::values(http_ec, discusy::api::result<T>{
                .error_info_{{
                    .message = std::move(err_msg),
                    .code = err.code,
                    .http_status = resp.status_code,
                    .errors = std::move(err.errors),
                },},
            });
        }
    }

    template <typename RetType, discusy::asio::ctf<discusy::api::result<RetType>> CompletionToken = discusy::ctx::io_context::dct_t>
    auto api_request_impl_(discusy::http::method m, auto&& url, auto&& body, auto&& req_headers, CompletionToken&& token = discusy::ctx::io_context::dct_t()) {
        return state_.client.api_request(
            m, std::forward<decltype(url)>(url), std::forward<decltype(body)>(body), std::forward<decltype(req_headers)>(req_headers), {},
            boost::asio::deferred([this](const boost::system::error_code& ec, discusy::http::response&& resp) {
                return value_from_api_response<RetType>(ec, std::move(resp));
            })
        )(std::forward<CompletionToken>(token));
    }

    template <typename RetType, discusy::asio::ctf<discusy::api::result<RetType>> CompletionToken = discusy::ctx::io_context::dct_t>
    auto api_request_with_body_impl_(discusy::http::method m, std::string&& url, std::expected<std::string, std::string>&& body_res, discusy::http_client::headers&& req_headers, CompletionToken&& token = discusy::ctx::io_context::dct_t()) {
        return boost::asio::async_initiate<CompletionToken, void(boost::system::error_code, discusy::api::result<RetType>)>(
            [](discusy::asio::chf<discusy::api::result<RetType>> auto&& handler, http_api* self, discusy::http::method m, auto&& url, auto&& body_res, auto&& req_headers) {
                if (!body_res) {
                    auto exec = boost::asio::get_associated_immediate_executor(handler, self->state_.client.io_ctx_.executor_);
                    auto alloc = boost::asio::get_associated_allocator(
                        handler,
                        boost::asio::recycling_allocator<void>{}
                    );

                    boost::asio::dispatch(exec,
                        boost::asio::bind_allocator(alloc, [h = std::forward<decltype(handler)>(handler), err = std::forward<decltype(body_res)>(body_res).error()]() mutable {
                            std::move(h)(boost::asio::error::invalid_argument, discusy::api::result<RetType>{
                                .error_info_{{.message = std::move(err)}},
                            });
                        })
                    );
                    return;
                }

                self->template api_request_impl_<RetType>(
                    m, std::forward<decltype(url)>(url), std::forward<decltype(body_res)>(body_res).value(), std::forward<decltype(req_headers)>(req_headers), std::forward<decltype(handler)>(handler)
                );
            },
            token,
            this,
            m,
            std::move(url),
            std::move(body_res),
            std::move(req_headers)
        );
    }

    template <typename T>
    [[nodiscard]] static std::expected<std::string, std::string> handle_json_body(const T& obj) {
        std::string json;
        if (discusy::json::write_json(obj, json)) {
            return std::unexpected("Failure to write json");
        }
        return json;
    }

    template <typename T>
    [[nodiscard]] static std::expected<std::string, std::string> handle_multipart_files_body(const T& obj, upload_files_param files) {
        std::string json;
        if (discusy::json::write_json(obj, json)) {
            return std::unexpected("Failure to write json");
        }

        std::string body;
        discusy::multipart::add_multipart_part(body, "payload_json", json, "", "application/json");
        
        std::visit([&body](const auto& span_files) {
            for (const auto& file : span_files) {
                auto name = ulp::str::concat_strings("files[", file.id, "]");
                discusy::multipart::add_multipart_part(body, name, file.data, file.filename, file.content_type);
            }
        }, files.files);

        discusy::multipart::finish_multipart(body);
        return body;
    }

    template <typename T>
    [[nodiscard]] static std::expected<std::string, std::string> handle_multipart_body(const T& obj, std::string_view name, std::string_view content, std::string_view content_type) {
        std::string json;
        if (discusy::json::write_json(obj, json)) {
            return std::unexpected("Failure to write json");
        }

        std::string body;
        discusy::multipart::add_multipart_part(body, "payload_json", json, "", "application/json");

        if (!content.empty()) {
            discusy::multipart::add_multipart_part(body, name, content, "", content_type);
        }

        discusy::multipart::finish_multipart(body);
        return body;
    }

    // formats an emoji object into the url path segment used by the reaction endpoints
    [[nodiscard]] static std::string format_emoji(const discusy::emoji::emoji& e) {
        return ulp::str::url_encode(
            ulp::str::concat_strings(
                e.name ? *e.name : "",
                e.id ? ulp::str::concat_strings(":", e.id->str()) : ""
            )
        );
    }

    // for endpoints that take an already serialised body (slack/github webhooks, empty objects)
    [[nodiscard]] static std::expected<std::string, std::string> handle_raw_body(std::string_view raw) {
        return std::string{raw};
    }

    // a json payload plus a single named file part (eg. sticker uploads)
    template <typename T>
    [[nodiscard]] static std::expected<std::string, std::string> handle_multipart_named_file_body(const T& obj, std::string_view name, const discusy::upload_file& file) {
        std::string json;
        if (discusy::json::write_json(obj, json)) {
            return std::unexpected("Failure to write json");
        }

        std::string body;
        discusy::multipart::add_multipart_part(body, "payload_json", json, "", "application/json");
        discusy::multipart::add_multipart_part(body, name, file.data, file.filename, file.content_type);

        discusy::multipart::finish_multipart(body);
        return body;
    }

    // a single named file part with no json payload alongside it
    [[nodiscard]] static std::expected<std::string, std::string> handle_multipart_only_file_body(std::string_view name, const discusy::upload_file& file) {
        std::string body;
        discusy::multipart::add_multipart_part(body, name, file.data, file.filename, file.content_type);
        discusy::multipart::finish_multipart(body);
        return body;
    }

    template <bool ReturnResult, typename RetType>
    using ret_t = std::conditional_t<ReturnResult, RetType, void>;

    template <bool ReturnResult, typename RetType>
    using api_ret_t = discusy::api::result<ret_t<ReturnResult, RetType>>;

public:
    http_api(discusy::state& s, const discusy::config& cfg) noexcept : state_{s}, cfg_{cfg} {}

// For endpoints that only require a URL (Simple GETs, Query Params, URL Params)
#pragma push_macro("DISCUSY_API_EMPTY_BODY")
#undef DISCUSY_API_EMPTY_BODY
#define DISCUSY_API_EMPTY_BODY(NAME, METHOD, RET_TYPE, URL_EXPR, ...) \
    template <bool ReturnResult = true, discusy::asio::ctf<api_ret_t<ReturnResult, RET_TYPE>> CompletionToken = discusy::ctx::io_context::dct_t> \
    auto NAME(__VA_ARGS__ __VA_OPT__(,) CompletionToken&& token = discusy::ctx::io_context::dct_t()) { \
        return api_request_impl_<ret_t<ReturnResult, RET_TYPE>>( \
            discusy::http::method::METHOD, URL_EXPR, std::string{}, cfg_.bot_headers_json(), std::forward<CompletionToken>(token) \
        ); \
    }

// For endpoints that only require a URL + Audit Log Reason
#pragma push_macro("DISCUSY_API_EMPTY_BODY_REASON")
#undef DISCUSY_API_EMPTY_BODY_REASON
#define DISCUSY_API_EMPTY_BODY_REASON(NAME, METHOD, RET_TYPE, URL_EXPR, ...) \
    template <bool ReturnResult = true, discusy::asio::ctf<api_ret_t<ReturnResult, RET_TYPE>> CompletionToken = discusy::ctx::io_context::dct_t> \
    auto NAME(__VA_ARGS__ __VA_OPT__(,) std::string reason = {}, CompletionToken&& token = discusy::ctx::io_context::dct_t()) { \
        return api_request_impl_<ret_t<ReturnResult, RET_TYPE>>( \
            discusy::http::method::METHOD, URL_EXPR, std::string{}, (reason.empty() ? cfg_.bot_headers_json() : cfg_.bot_headers_json(std::move(reason))), \
            std::forward<CompletionToken>(token) \
        ); \
    }

// For endpoints that require parsing a body (put, patch, POST)
#pragma push_macro("DISCUSY_API_JSON_BODY")
#undef DISCUSY_API_JSON_BODY
#define DISCUSY_API_JSON_BODY(NAME, METHOD, RET_TYPE, BODY_EXPR, URL_EXPR, ...) \
    template <bool ReturnResult = true, discusy::asio::ctf<api_ret_t<ReturnResult, RET_TYPE>> CompletionToken = discusy::ctx::io_context::dct_t> \
    auto NAME(__VA_ARGS__ __VA_OPT__(,) CompletionToken&& token = discusy::ctx::io_context::dct_t()) { \
        return api_request_with_body_impl_<ret_t<ReturnResult, RET_TYPE>>( \
            discusy::http::method::METHOD, URL_EXPR, BODY_EXPR, cfg_.bot_headers_json(), std::forward<CompletionToken>(token) \
        ); \
    }

// For endpoints that require a JSON body + Audit Log Reason
#pragma push_macro("DISCUSY_API_JSON_BODY_REASON")
#undef DISCUSY_API_JSON_BODY_REASON
#define DISCUSY_API_JSON_BODY_REASON(NAME, METHOD, RET_TYPE, BODY_EXPR, URL_EXPR, ...) \
    template <bool ReturnResult = true, discusy::asio::ctf<api_ret_t<ReturnResult, RET_TYPE>> CompletionToken = discusy::ctx::io_context::dct_t> \
    auto NAME(__VA_ARGS__ __VA_OPT__(,) std::string reason = {}, CompletionToken&& token = discusy::ctx::io_context::dct_t()) { \
        return api_request_with_body_impl_<ret_t<ReturnResult, RET_TYPE>>( \
            discusy::http::method::METHOD, URL_EXPR, BODY_EXPR, (reason.empty() ? cfg_.bot_headers_json() : cfg_.bot_headers_json(std::move(reason))), \
            std::forward<CompletionToken>(token) \
        ); \
    }

#pragma push_macro("DISCUSY_API_MULTIPART_JSON_BODY")
#undef DISCUSY_API_MULTIPART_JSON_BODY
#define DISCUSY_API_MULTIPART_JSON_BODY(NAME, METHOD, RET_TYPE, BODY_EXPR, URL_EXPR, ...) \
    template <bool ReturnResult = true, discusy::asio::ctf<api_ret_t<ReturnResult, RET_TYPE>> CompletionToken = discusy::ctx::io_context::dct_t> \
    auto NAME(__VA_ARGS__ __VA_OPT__(,) CompletionToken&& token = discusy::ctx::io_context::dct_t()) { \
        return api_request_with_body_impl_<ret_t<ReturnResult, RET_TYPE>>( \
            discusy::http::method::METHOD, URL_EXPR, BODY_EXPR, cfg_.bot_headers_multipart(), std::forward<CompletionToken>(token) \
        ); \
    }

#pragma push_macro("DISCUSY_API_MULTIPART_JSON_BODY_REASON")
#undef DISCUSY_API_MULTIPART_JSON_BODY_REASON
#define DISCUSY_API_MULTIPART_JSON_BODY_REASON(NAME, METHOD, RET_TYPE, BODY_EXPR, URL_EXPR, ...) \
    template <bool ReturnResult = true, discusy::asio::ctf<api_ret_t<ReturnResult, RET_TYPE>> CompletionToken = discusy::ctx::io_context::dct_t> \
    auto NAME(__VA_ARGS__ __VA_OPT__(,) std::string reason = {}, CompletionToken&& token = discusy::ctx::io_context::dct_t()) { \
        return api_request_with_body_impl_<ret_t<ReturnResult, RET_TYPE>>( \
            discusy::http::method::METHOD, URL_EXPR, BODY_EXPR, (reason.empty() ? cfg_.bot_headers_multipart() : cfg_.bot_headers_multipart(std::move(reason))), \
            std::forward<CompletionToken>(token) \
        ); \
    }



    DISCUSY_API_EMPTY_BODY(
        get_gateway_bot,
        get,
        discusy::api::get_gateway_bot,
        discusy::urls::GET_BOT_PARAMS
    )

    using application_role_connection_metadata_records_ret_t = std::vector<discusy::application_role_connection_metadata::application_role_connection_metadata>;
    using application_role_connection_metadata_records_t = std::span<const discusy::application_role_connection_metadata::application_role_connection_metadata>;
    // https://docs.discord.com/developers/resources/application-role-connection-metadata#get-application-role-connection-metadata-records
    DISCUSY_API_EMPTY_BODY(
        get_application_role_connection_metadata_records,
        get,
        application_role_connection_metadata_records_ret_t,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/applications/", state_.application_id, "/role-connections/metadata")
    )

    // https://docs.discord.com/developers/resources/application-role-connection-metadata#update-application-role-connection-metadata-records
    DISCUSY_API_JSON_BODY(
        update_application_role_connection_metadata_records,
        put,
        application_role_connection_metadata_records_ret_t,
        handle_json_body(update),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/applications/", state_.application_id, "/role-connections/metadata"),
        application_role_connection_metadata_records_t update
    )

    // https://docs.discord.com/developers/resources/application#get-current-application
    DISCUSY_API_EMPTY_BODY(
        get_current_application,
        get,
        discusy::application::application,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/applications/@me")
    )

    // https://docs.discord.com/developers/resources/application#edit-current-application
    DISCUSY_API_JSON_BODY(
        edit_current_application,
        patch,
        discusy::application::application,
        handle_json_body(update),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/applications/@me"),
        const discusy::api::application::edit_current_application& update
    )

    // https://docs.discord.com/developers/resources/application#get-application-activity-instance
    DISCUSY_API_EMPTY_BODY(
        get_application_activity_instance,
        get,
        discusy::api::application::activity_instance,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/applications/", state_.application_id, "/activity-instances/", instance_id),
        std::string_view instance_id
    )

    // https://docs.discord.com/developers/topics/oauth2#get-current-authorization-information
    DISCUSY_API_EMPTY_BODY(
        get_current_authorization_information,
        get,
        discusy::api::oauth2::current_authorization_information,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/oauth2/@me")
    )

    // https://docs.discord.com/developers/topics/oauth2#get-current-bot-application-information
    DISCUSY_API_EMPTY_BODY(
        get_current_bot_application_information,
        get,
        discusy::application::application,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/oauth2/applications/@me")
    )

    // https://docs.discord.com/developers/resources/audit-log#get-guild-audit-log
    DISCUSY_API_EMPTY_BODY(
        get_guild_audit_log,
        get,
        discusy::audit_log::audit_log,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/audit-logs", make_query_string(params)),
        const discusy::snowflake guild_id, const discusy::api::audit_log::get_guild_audit_log_query_params& params = {}
    )

    // https://docs.discord.com/developers/resources/auto-moderation#list-auto-moderation-rules-for-guild
    DISCUSY_API_EMPTY_BODY(
        list_auto_moderation_rules_for_guild,
        get,
        std::vector<discusy::auto_moderation::auto_moderation_rule>,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/auto-moderation/rules"),
        const discusy::snowflake guild_id
    )

    // https://docs.discord.com/developers/resources/auto-moderation#get-auto-moderation-rule
    DISCUSY_API_EMPTY_BODY(
        get_auto_moderation_rule,
        get,
        discusy::auto_moderation::auto_moderation_rule,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/auto-moderation/rules/", rule_id.to_snowflake_str()),
        const discusy::snowflake guild_id, const discusy::snowflake rule_id
    )

    // https://docs.discord.com/developers/resources/auto-moderation#create-auto-moderation-rule
    DISCUSY_API_JSON_BODY_REASON(
        create_auto_moderation_rule,
        post,
        discusy::auto_moderation::auto_moderation_rule,
        handle_json_body(rule),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/auto-moderation/rules"),
        const discusy::snowflake guild_id, const discusy::api::auto_moderation::create_auto_moderation_rule& rule
    )

    // https://docs.discord.com/developers/resources/auto-moderation#modify-auto-moderation-rule
    DISCUSY_API_JSON_BODY_REASON(
        modify_auto_moderation_rule,
        patch,
        discusy::auto_moderation::auto_moderation_rule,
        handle_json_body(update),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/auto-moderation/rules/", rule_id.to_snowflake_str()),
        const discusy::snowflake guild_id, const discusy::snowflake rule_id, const discusy::api::auto_moderation::modify_auto_moderation_rule& update
    )

    // https://docs.discord.com/developers/resources/auto-moderation#delete-auto-moderation-rule
    DISCUSY_API_EMPTY_BODY_REASON(
        delete_auto_moderation_rule,
        delete_,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/auto-moderation/rules/", rule_id.to_snowflake_str()),
        const discusy::snowflake guild_id, const discusy::snowflake rule_id
    )

    // https://docs.discord.com/developers/resources/channel#get-channel
    DISCUSY_API_EMPTY_BODY(
        get_channel,
        get,
        discusy::channel::channel,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str()),
        const discusy::snowflake channel_id
    )

    // https://docs.discord.com/developers/resources/channel#modify-channel
    DISCUSY_API_JSON_BODY_REASON(
        modify_channel,
        patch,
        discusy::channel::channel,
        handle_json_body(update),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str()),
        const discusy::snowflake channel_id, const discusy::api::channels::modify_channel& update
    )

    // https://docs.discord.com/developers/resources/channel#set-voice-channel-status
    DISCUSY_API_JSON_BODY_REASON(
        set_voice_channel_status,
        put,
        void,
        handle_json_body(status),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/voice-status"),
        const discusy::snowflake channel_id, const discusy::api::channels::set_voice_channel_status& status
    )

    // https://docs.discord.com/developers/resources/channel#delete/close-channel
    DISCUSY_API_EMPTY_BODY_REASON(
        delete_channel,
        delete_,
        discusy::channel::channel,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str()),
        const discusy::snowflake channel_id
    )

    // https://docs.discord.com/developers/resources/channel#delete/close-channel
    DISCUSY_API_EMPTY_BODY_REASON(
        close_channel,
        delete_,
        discusy::channel::channel,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str()),
        const discusy::snowflake channel_id
    )

    // https://docs.discord.com/developers/resources/channel#edit-channel-permissions
    DISCUSY_API_JSON_BODY_REASON(
        edit_channel_permissions,
        put,
        void,
        handle_json_body(edit),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/permissions/", overwrite_id.to_snowflake_str()),
        const discusy::snowflake channel_id, const discusy::snowflake overwrite_id, const discusy::api::channels::edit_channel_permissions& edit
    )

    // https://docs.discord.com/developers/resources/channel#get-channel-invites
    DISCUSY_API_EMPTY_BODY(
        get_channel_invites,
        get,
        std::vector<discusy::invite::invite>,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/invites"),
        const discusy::snowflake channel_id
    )

    // https://docs.discord.com/developers/resources/channel#create-channel-invite
    DISCUSY_API_MULTIPART_JSON_BODY_REASON(
        create_channel_invite,
        post,
        discusy::invite::invite,
        handle_multipart_body(create, "target_users_file", target_users_file, "application/csv"),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/invites"),
        const discusy::snowflake channel_id, const discusy::api::channels::create_channel_invite& create, std::string_view target_users_file = {}
    )

    // https://docs.discord.com/developers/resources/channel#delete-channel-permission
    DISCUSY_API_EMPTY_BODY_REASON(
        delete_channel_permission,
        delete_,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/permissions/", overwrite_id.to_snowflake_str()),
        const discusy::snowflake channel_id, const discusy::snowflake overwrite_id
    )

    // https://docs.discord.com/developers/resources/channel#follow-announcement-channel
    DISCUSY_API_JSON_BODY_REASON(
        follow_announcement_channel,
        post,
        discusy::channel::followed_channel,
        handle_json_body(follow),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/followers"),
        const discusy::snowflake channel_id, const discusy::api::channels::follow_announcement_channel& follow
    )

    // https://docs.discord.com/developers/resources/channel#trigger-typing-indicator
    DISCUSY_API_EMPTY_BODY(
        trigger_typing_indicator,
        post,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/typing"),
        const discusy::snowflake channel_id
    )

    // https://docs.discord.com/developers/resources/channel#group-dm-add-recipient
    DISCUSY_API_JSON_BODY(
        group_dm_add_recipient,
        put,
        void,
        handle_json_body(add),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/recipients/", user_id.to_snowflake_str()),
        const discusy::snowflake channel_id, const discusy::snowflake user_id, const discusy::api::channels::group_dm_add_recipient& add
    )

    // https://docs.discord.com/developers/resources/channel#group-dm-remove-recipient
    DISCUSY_API_EMPTY_BODY(
        group_dm_remove_recipient,
        delete_,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/recipients/", user_id.to_snowflake_str()),
        const discusy::snowflake channel_id, const discusy::snowflake user_id
    )

    // https://docs.discord.com/developers/resources/channel#start-thread-from-message
    DISCUSY_API_JSON_BODY_REASON(
        start_thread_from_message,
        post,
        discusy::channel::channel,
        handle_json_body(thr),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/messages/", message_id.to_snowflake_str(), "/threads"),
        const discusy::snowflake channel_id, const discusy::snowflake message_id, const discusy::api::channels::start_thread_from_message& thr
    )

    // https://docs.discord.com/developers/resources/channel#start-thread-without-message
    DISCUSY_API_JSON_BODY_REASON(
        start_thread_without_message,
        post,
        discusy::channel::channel,
        handle_json_body(thr),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/threads"),
        const discusy::snowflake channel_id, const discusy::api::channels::start_thread_without_message& thr
    )

    // https://docs.discord.com/developers/resources/channel#start-thread-in-forum-or-media-channel
    DISCUSY_API_MULTIPART_JSON_BODY_REASON(
        start_thread_in_forum_or_media_channel,
        post,
        discusy::channel::channel,
        handle_multipart_files_body(thr, files),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/threads"),
        const discusy::snowflake channel_id, const discusy::api::channels::start_thread_in_forum_or_media_channel& thr, upload_files_param files = {}
    )

    // https://docs.discord.com/developers/resources/channel#join-thread
    DISCUSY_API_EMPTY_BODY(
        join_thread,
        put,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/thread-members/@me"),
        const discusy::snowflake channel_id
    )

    // https://docs.discord.com/developers/resources/channel#add-thread-member
    DISCUSY_API_EMPTY_BODY(
        add_thread_member,
        put,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/thread-members/", user_id.to_snowflake_str()),
        const discusy::snowflake channel_id, const discusy::snowflake user_id
    )

    // https://docs.discord.com/developers/resources/channel#leave-thread
    DISCUSY_API_EMPTY_BODY(
        leave_thread,
        delete_,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/thread-members/@me"),
        const discusy::snowflake channel_id
    )

    // https://docs.discord.com/developers/resources/channel#remove-thread-member
    DISCUSY_API_EMPTY_BODY(
        remove_thread_member,
        delete_,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/thread-members/", user_id.to_snowflake_str()),
        const discusy::snowflake channel_id, const discusy::snowflake user_id
    )

    // https://docs.discord.com/developers/resources/channel#get-thread-member
    DISCUSY_API_EMPTY_BODY(
        get_thread_member,
        get,
        discusy::channel::thread_member_with_guild,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/thread-members/", user_id.to_snowflake_str(), make_query_string(get)),
        const discusy::snowflake channel_id, const discusy::snowflake user_id, const discusy::api::channels::get_thread_member& get = {}
    )

    // https://docs.discord.com/developers/resources/channel#list-thread-members
    DISCUSY_API_EMPTY_BODY(
        list_thread_members,
        get,
        std::vector<discusy::channel::thread_member_with_guild>,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/thread-members", make_query_string(list)),
        const discusy::snowflake channel_id, const discusy::api::channels::list_thread_members& list = {}
    )

    // https://docs.discord.com/developers/resources/channel#list-public-archived-threads
    DISCUSY_API_EMPTY_BODY(
        list_public_archived_threads,
        get,
        discusy::api::channels::list_archived_threads_response,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/threads/archived/public", make_query_string(list)),
        const discusy::snowflake channel_id, const discusy::api::channels::list_archived_threads_query& list = {}
    )

    // https://docs.discord.com/developers/resources/channel#list-private-archived-threads
    DISCUSY_API_EMPTY_BODY(
        list_private_archived_threads,
        get,
        discusy::api::channels::list_archived_threads_response,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/threads/archived/private", make_query_string(list)),
        const discusy::snowflake channel_id, const discusy::api::channels::list_archived_threads_query& list = {}
    )

    // https://docs.discord.com/developers/resources/channel#list-joined-private-archived-threads
    DISCUSY_API_EMPTY_BODY(
        list_joined_private_archived_threads,
        get,
        discusy::api::channels::list_archived_threads_response,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/users/@me/threads/archived/private", make_query_string(list)),
        const discusy::snowflake channel_id, const discusy::api::channels::list_joined_private_archived_threads_query& list = {}
    )

    // https://docs.discord.com/developers/resources/channel#get-channel-webhooks
    DISCUSY_API_EMPTY_BODY(
        get_channel_webhooks,
        get,
        std::vector<discusy::webhook::webhook>,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/webhooks"),
        const discusy::snowflake channel_id
    )

    // https://docs.discord.com/developers/resources/webhook#create-webhook
    DISCUSY_API_JSON_BODY_REASON(
        create_webhook,
        post,
        discusy::webhook::webhook,
        handle_json_body(create),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/webhooks"),
        const discusy::snowflake channel_id, const discusy::api::webhook::create_webhook& create
    )

    // https://docs.discord.com/developers/resources/guild#get-guild-channels
    DISCUSY_API_EMPTY_BODY(
        get_guild_channels,
        get,
        std::vector<discusy::channel::channel>,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/channels"),
        const discusy::snowflake guild_id
    )

    // https://docs.discord.com/developers/resources/guild#modify-guild-member
    DISCUSY_API_JSON_BODY_REASON(
        modify_guild_member,
        patch,
        discusy::guild::guild_member,
        handle_json_body(modify),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/members/", user_id.to_snowflake_str()),
        const discusy::snowflake guild_id, const discusy::snowflake user_id, const discusy::api::guild::modify_guild_member& modify
    )

    // https://docs.discord.com/developers/resources/guild#get-guild
    DISCUSY_API_EMPTY_BODY(
        get_guild,
        get,
        discusy::guild::guild,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), make_query_string(query)),
        const discusy::snowflake guild_id, const discusy::api::guild::get_guild_query& query = {}
    )

    // https://docs.discord.com/developers/resources/guild#get-guild-preview
    DISCUSY_API_EMPTY_BODY(
        get_guild_preview,
        get,
        discusy::guild::guild_preview,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/preview"),
        const discusy::snowflake guild_id
    )

    // https://docs.discord.com/developers/resources/guild#modify-guild
    DISCUSY_API_JSON_BODY_REASON(
        modify_guild,
        patch,
        discusy::guild::guild,
        handle_json_body(update),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str()),
        const discusy::snowflake guild_id, const discusy::api::guild::modify_guild& update
    )

    // https://docs.discord.com/developers/resources/guild#create-guild-channel
    DISCUSY_API_JSON_BODY_REASON(
        create_guild_channel,
        post,
        discusy::channel::channel,
        handle_json_body(create),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/channels"),
        const discusy::snowflake guild_id, const discusy::api::guild::create_guild_channel& create
    )

    // https://docs.discord.com/developers/resources/guild#modify-guild-channel-positions
    DISCUSY_API_JSON_BODY(
        modify_guild_channel_positions,
        patch,
        void,
        handle_json_body(positions),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/channels"),
        const discusy::snowflake guild_id, discusy::api::guild::modify_guild_channel_positions positions
    )

    // https://docs.discord.com/developers/resources/guild#list-active-guild-threads
    DISCUSY_API_EMPTY_BODY(
        list_active_guild_threads,
        get,
        discusy::api::guild::list_active_guild_threads_response,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/threads/active"),
        const discusy::snowflake guild_id
    )

    // https://docs.discord.com/developers/resources/guild#get-guild-member
    DISCUSY_API_EMPTY_BODY(
        get_guild_member,
        get,
        discusy::guild::guild_member,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/members/", user_id.to_snowflake_str()),
        const discusy::snowflake guild_id, const discusy::snowflake user_id
    )

    // https://docs.discord.com/developers/resources/guild#list-guild-members
    DISCUSY_API_EMPTY_BODY(
        list_guild_members,
        get,
        std::vector<discusy::guild::guild_member>,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/members", make_query_string(query)),
        const discusy::snowflake guild_id, const discusy::api::guild::list_guild_members_query& query = {}
    )

    // https://docs.discord.com/developers/resources/guild#search-guild-members
    DISCUSY_API_EMPTY_BODY(
        search_guild_members,
        get,
        std::vector<discusy::guild::guild_member>,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/members/search", make_query_string(query)),
        const discusy::snowflake guild_id, const discusy::api::guild::search_guild_members_query& query
    )

    // https://docs.discord.com/developers/resources/guild#add-guild-member
    DISCUSY_API_JSON_BODY(
        add_guild_member,
        put,
        discusy::guild::guild_member,
        handle_json_body(add),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/members/", user_id.to_snowflake_str()),
        const discusy::snowflake guild_id, const discusy::snowflake user_id, const discusy::api::guild::add_guild_member& add
    )

    // https://docs.discord.com/developers/resources/guild#modify-current-member
    DISCUSY_API_JSON_BODY_REASON(
        modify_current_member,
        patch,
        discusy::guild::guild_member,
        handle_json_body(modify),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/members/@me"),
        const discusy::snowflake guild_id, const discusy::api::guild::modify_current_member& modify
    )

    // deprecated, use modify_current_member
    // https://docs.discord.com/developers/resources/guild#modify-current-user-nick
    DISCUSY_API_JSON_BODY_REASON(
        modify_current_user_nick,
        patch,
        discusy::guild::guild_member,
        handle_json_body(modify),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/members/@me/nick"),
        const discusy::snowflake guild_id, const discusy::api::guild::modify_current_user_nick& modify
    )

    // https://docs.discord.com/developers/resources/guild#add-guild-member-role
    DISCUSY_API_EMPTY_BODY_REASON(
        add_guild_member_role,
        put,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/members/", user_id.to_snowflake_str(), "/roles/", role_id.to_snowflake_str()),
        const discusy::snowflake guild_id, const discusy::snowflake user_id, const discusy::snowflake role_id
    )

    // https://docs.discord.com/developers/resources/guild#remove-guild-member-role
    DISCUSY_API_EMPTY_BODY_REASON(
        remove_guild_member_role,
        delete_,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/members/", user_id.to_snowflake_str(), "/roles/", role_id.to_snowflake_str()),
        const discusy::snowflake guild_id, const discusy::snowflake user_id, const discusy::snowflake role_id
    )

    // https://docs.discord.com/developers/resources/guild#remove-guild-member
    DISCUSY_API_EMPTY_BODY_REASON(
        remove_guild_member,
        delete_,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/members/", user_id.to_snowflake_str()),
        const discusy::snowflake guild_id, const discusy::snowflake user_id
    )

    // https://docs.discord.com/developers/resources/guild#get-guild-bans
    DISCUSY_API_EMPTY_BODY(
        get_guild_bans,
        get,
        std::vector<discusy::guild::ban>,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/bans", make_query_string(query)),
        const discusy::snowflake guild_id, const discusy::api::guild::get_guild_bans_query& query = {}
    )

    // https://docs.discord.com/developers/resources/guild#get-guild-ban
    DISCUSY_API_EMPTY_BODY(
        get_guild_ban,
        get,
        discusy::guild::ban,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/bans/", user_id.to_snowflake_str()),
        const discusy::snowflake guild_id, const discusy::snowflake user_id
    )

    // https://docs.discord.com/developers/resources/guild#create-guild-ban
    DISCUSY_API_JSON_BODY_REASON(
        create_guild_ban,
        put,
        void,
        handle_json_body(ban),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/bans/", user_id.to_snowflake_str()),
        const discusy::snowflake guild_id, const discusy::snowflake user_id, const discusy::api::guild::create_guild_ban& ban = {}
    )

    // https://docs.discord.com/developers/resources/guild#remove-guild-ban
    DISCUSY_API_EMPTY_BODY_REASON(
        remove_guild_ban,
        delete_,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/bans/", user_id.to_snowflake_str()),
        const discusy::snowflake guild_id, const discusy::snowflake user_id
    )

    // https://docs.discord.com/developers/resources/guild#bulk-guild-ban
    DISCUSY_API_JSON_BODY_REASON(
        bulk_guild_ban,
        post,
        discusy::api::guild::bulk_guild_ban_response,
        handle_json_body(ban),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/bulk-ban"),
        const discusy::snowflake guild_id, const discusy::api::guild::bulk_guild_ban& ban
    )

    // https://docs.discord.com/developers/resources/guild#get-guild-roles
    DISCUSY_API_EMPTY_BODY(
        get_guild_roles,
        get,
        std::vector<discusy::permissions::role>,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/roles"),
        const discusy::snowflake guild_id
    )

    // https://docs.discord.com/developers/resources/guild#get-guild-role
    DISCUSY_API_EMPTY_BODY(
        get_guild_role,
        get,
        discusy::permissions::role,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/roles/", role_id.to_snowflake_str()),
        const discusy::snowflake guild_id, const discusy::snowflake role_id
    )

    // https://docs.discord.com/developers/resources/guild#get-guild-role-member-counts
    DISCUSY_API_EMPTY_BODY(
        get_guild_role_member_counts,
        get,
        discusy::api::guild::role_member_counts,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/roles/member-counts"),
        const discusy::snowflake guild_id
    )

    // https://docs.discord.com/developers/resources/guild#create-guild-role
    DISCUSY_API_JSON_BODY_REASON(
        create_guild_role,
        post,
        discusy::permissions::role,
        handle_json_body(create),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/roles"),
        const discusy::snowflake guild_id, const discusy::api::guild::create_guild_role& create = {}
    )

    // https://docs.discord.com/developers/resources/guild#modify-guild-role-positions
    DISCUSY_API_JSON_BODY_REASON(
        modify_guild_role_positions,
        patch,
        std::vector<discusy::permissions::role>,
        handle_json_body(positions),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/roles"),
        const discusy::snowflake guild_id, discusy::api::guild::modify_guild_role_positions positions
    )

    // https://docs.discord.com/developers/resources/guild#modify-guild-role
    DISCUSY_API_JSON_BODY_REASON(
        modify_guild_role,
        patch,
        discusy::permissions::role,
        handle_json_body(update),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/roles/", role_id.to_snowflake_str()),
        const discusy::snowflake guild_id, const discusy::snowflake role_id, const discusy::api::guild::modify_guild_role& update
    )

    // https://docs.discord.com/developers/resources/guild#delete-guild-role
    DISCUSY_API_EMPTY_BODY_REASON(
        delete_guild_role,
        delete_,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/roles/", role_id.to_snowflake_str()),
        const discusy::snowflake guild_id, const discusy::snowflake role_id
    )

    // https://docs.discord.com/developers/resources/guild#get-guild-prune-count
    DISCUSY_API_EMPTY_BODY(
        get_guild_prune_count,
        get,
        discusy::api::guild::guild_prune_count_response,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/prune", make_query_string(query)),
        const discusy::snowflake guild_id, const discusy::api::guild::get_guild_prune_count_query& query = {}
    )

    // https://docs.discord.com/developers/resources/guild#begin-guild-prune
    DISCUSY_API_JSON_BODY_REASON(
        begin_guild_prune,
        post,
        discusy::api::guild::guild_prune_count_response,
        handle_json_body(prune),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/prune"),
        const discusy::snowflake guild_id, const discusy::api::guild::begin_guild_prune& prune = {}
    )

    // https://docs.discord.com/developers/resources/guild#get-guild-voice-regions
    DISCUSY_API_EMPTY_BODY(
        get_guild_voice_regions,
        get,
        std::vector<discusy::voice::voice_region>,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/regions"),
        const discusy::snowflake guild_id
    )

    // https://docs.discord.com/developers/resources/guild#get-guild-invites
    DISCUSY_API_EMPTY_BODY(
        get_guild_invites,
        get,
        std::vector<discusy::invite::invite>,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/invites"),
        const discusy::snowflake guild_id
    )

    // https://docs.discord.com/developers/resources/guild#get-guild-integrations
    DISCUSY_API_EMPTY_BODY(
        get_guild_integrations,
        get,
        std::vector<discusy::guild::integration>,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/integrations"),
        const discusy::snowflake guild_id
    )

    // https://docs.discord.com/developers/resources/guild#delete-guild-integration
    DISCUSY_API_EMPTY_BODY_REASON(
        delete_guild_integration,
        delete_,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/integrations/", integration_id.to_snowflake_str()),
        const discusy::snowflake guild_id, const discusy::snowflake integration_id
    )

    // https://docs.discord.com/developers/resources/guild#get-guild-widget-settings
    DISCUSY_API_EMPTY_BODY(
        get_guild_widget_settings,
        get,
        discusy::guild::guild_widget_settings,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/widget"),
        const discusy::snowflake guild_id
    )

    // https://docs.discord.com/developers/resources/guild#modify-guild-widget
    DISCUSY_API_JSON_BODY_REASON(
        modify_guild_widget,
        patch,
        discusy::guild::guild_widget_settings,
        handle_json_body(update),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/widget"),
        const discusy::snowflake guild_id, const discusy::api::guild::modify_guild_widget& update
    )

    // https://docs.discord.com/developers/resources/guild#get-guild-widget
    DISCUSY_API_EMPTY_BODY(
        get_guild_widget,
        get,
        discusy::guild::guild_widget,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/widget.json"),
        const discusy::snowflake guild_id
    )

    // https://docs.discord.com/developers/resources/guild#get-guild-vanity-url
    DISCUSY_API_EMPTY_BODY(
        get_guild_vanity_url,
        get,
        discusy::invite::invite,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/vanity-url"),
        const discusy::snowflake guild_id
    )

    // returns a PNG image
    // https://docs.discord.com/developers/resources/guild#get-guild-widget-image
    DISCUSY_API_EMPTY_BODY(
        get_guild_widget_image,
        get,
        http_api::raw_data,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/widget.png", make_query_string(query)),
        const discusy::snowflake guild_id, const discusy::api::guild::get_guild_widget_image_query& query = {}
    )

    // https://docs.discord.com/developers/resources/guild#get-guild-welcome-screen
    DISCUSY_API_EMPTY_BODY(
        get_guild_welcome_screen,
        get,
        discusy::guild::welcome_screen,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/welcome-screen"),
        const discusy::snowflake guild_id
    )

    // https://docs.discord.com/developers/resources/guild#modify-guild-welcome-screen
    DISCUSY_API_JSON_BODY_REASON(
        modify_guild_welcome_screen,
        patch,
        discusy::guild::welcome_screen,
        handle_json_body(update),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/welcome-screen"),
        const discusy::snowflake guild_id, const discusy::api::guild::modify_guild_welcome_screen& update
    )

    // https://docs.discord.com/developers/resources/guild#get-guild-onboarding
    DISCUSY_API_EMPTY_BODY(
        get_guild_onboarding,
        get,
        discusy::guild::guild_onboarding,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/onboarding"),
        const discusy::snowflake guild_id
    )

    // https://docs.discord.com/developers/resources/guild#modify-guild-onboarding
    DISCUSY_API_JSON_BODY_REASON(
        modify_guild_onboarding,
        put,
        discusy::guild::guild_onboarding,
        handle_json_body(update),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/onboarding"),
        const discusy::snowflake guild_id, const discusy::api::guild::modify_guild_onboarding& update
    )

    // https://docs.discord.com/developers/resources/guild#modify-guild-incident-actions
    DISCUSY_API_JSON_BODY(
        modify_guild_incident_actions,
        put,
        discusy::guild::incidents_data,
        handle_json_body(update),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/incident-actions"),
        const discusy::snowflake guild_id, const discusy::api::guild::modify_guild_incident_actions& update
    )

    // https://docs.discord.com/developers/resources/guild#get-guild-webhooks
    DISCUSY_API_EMPTY_BODY(
        get_guild_webhooks,
        get,
        std::vector<discusy::webhook::webhook>,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/webhooks"),
        const discusy::snowflake guild_id
    )

    // https://docs.discord.com/developers/resources/message#get-channel-message
    DISCUSY_API_EMPTY_BODY(
        get_channel_message,
        get,
        discusy::message::message,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/messages/", message_id.to_snowflake_str()),
        const discusy::snowflake channel_id, const discusy::snowflake message_id
    )

    // https://docs.discord.com/developers/resources/message#create-message
    DISCUSY_API_MULTIPART_JSON_BODY(
        create_message,
        post,
        discusy::message::message,
        handle_multipart_files_body(msg, files),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/messages"),
        const discusy::snowflake channel_id, const discusy::api::message::create_message& msg, upload_files_param files = {}
    )

    // https://docs.discord.com/developers/resources/message#create-reaction
    DISCUSY_API_EMPTY_BODY(
        create_reaction,
        put,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/messages/", message_id.to_snowflake_str(), "/reactions/", ulp::str::url_encode(emoji), "/@me"),
        const discusy::snowflake channel_id, const discusy::snowflake message_id, const std::string_view emoji
    )

    // https://docs.discord.com/developers/resources/message#create-reaction
    DISCUSY_API_EMPTY_BODY(
        create_reaction,
        put,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/messages/", message_id.to_snowflake_str(), "/reactions/", format_emoji(emoji), "/@me"),
        const discusy::snowflake channel_id, const discusy::snowflake message_id, const discusy::emoji::emoji& emoji
    )

    // https://docs.discord.com/developers/resources/message#delete-message
    DISCUSY_API_EMPTY_BODY_REASON(
        delete_message,
        delete_,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/messages/", message_id.to_snowflake_str()),
        const discusy::snowflake channel_id, const discusy::snowflake message_id
    )

    // https://docs.discord.com/developers/resources/message#get-channel-messages
    DISCUSY_API_EMPTY_BODY(
        get_channel_messages,
        get,
        std::vector<discusy::message::message>,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/messages", make_query_string(query)),
        const discusy::snowflake channel_id, const discusy::api::message::get_channel_messages_query& query = {}
    )

    // https://docs.discord.com/developers/resources/message#search-guild-messages
    DISCUSY_API_EMPTY_BODY(
        search_guild_messages,
        get,
        discusy::api::message::search_guild_messages_response,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/messages/search", make_query_string(query)),
        const discusy::snowflake guild_id, const discusy::api::message::search_guild_messages_query& query = {}
    )

    // https://docs.discord.com/developers/resources/message#crosspost-message
    DISCUSY_API_EMPTY_BODY(
        crosspost_message,
        post,
        discusy::message::message,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/messages/", message_id.to_snowflake_str(), "/crosspost"),
        const discusy::snowflake channel_id, const discusy::snowflake message_id
    )

    // https://docs.discord.com/developers/resources/message#delete-own-reaction
    DISCUSY_API_EMPTY_BODY(
        delete_own_reaction,
        delete_,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/messages/", message_id.to_snowflake_str(), "/reactions/", ulp::str::url_encode(emoji), "/@me"),
        const discusy::snowflake channel_id, const discusy::snowflake message_id, const std::string_view emoji
    )

    // https://docs.discord.com/developers/resources/message#delete-own-reaction
    DISCUSY_API_EMPTY_BODY(
        delete_own_reaction,
        delete_,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/messages/", message_id.to_snowflake_str(), "/reactions/", format_emoji(emoji), "/@me"),
        const discusy::snowflake channel_id, const discusy::snowflake message_id, const discusy::emoji::emoji& emoji
    )

    // https://docs.discord.com/developers/resources/message#delete-user-reaction
    DISCUSY_API_EMPTY_BODY(
        delete_user_reaction,
        delete_,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/messages/", message_id.to_snowflake_str(), "/reactions/", ulp::str::url_encode(emoji), "/", user_id.to_snowflake_str()),
        const discusy::snowflake channel_id, const discusy::snowflake message_id, const std::string_view emoji, const discusy::snowflake user_id
    )

    // https://docs.discord.com/developers/resources/message#delete-user-reaction
    DISCUSY_API_EMPTY_BODY(
        delete_user_reaction,
        delete_,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/messages/", message_id.to_snowflake_str(), "/reactions/", format_emoji(emoji), "/", user_id.to_snowflake_str()),
        const discusy::snowflake channel_id, const discusy::snowflake message_id, const discusy::emoji::emoji& emoji, const discusy::snowflake user_id
    )

    // https://docs.discord.com/developers/resources/message#get-reactions
    DISCUSY_API_EMPTY_BODY(
        get_reactions,
        get,
        std::vector<discusy::user::user>,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/messages/", message_id.to_snowflake_str(), "/reactions/", ulp::str::url_encode(emoji), make_query_string(query)),
        const discusy::snowflake channel_id, const discusy::snowflake message_id, const std::string_view emoji, const discusy::api::message::get_reactions_query& query = {}
    )

    // https://docs.discord.com/developers/resources/message#get-reactions
    DISCUSY_API_EMPTY_BODY(
        get_reactions,
        get,
        std::vector<discusy::user::user>,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/messages/", message_id.to_snowflake_str(), "/reactions/", format_emoji(emoji), make_query_string(query)),
        const discusy::snowflake channel_id, const discusy::snowflake message_id, const discusy::emoji::emoji& emoji, const discusy::api::message::get_reactions_query& query = {}
    )

    // https://docs.discord.com/developers/resources/message#delete-all-reactions
    DISCUSY_API_EMPTY_BODY(
        delete_all_reactions,
        delete_,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/messages/", message_id.to_snowflake_str(), "/reactions"),
        const discusy::snowflake channel_id, const discusy::snowflake message_id
    )

    // https://docs.discord.com/developers/resources/message#delete-all-reactions-for-emoji
    DISCUSY_API_EMPTY_BODY(
        delete_all_reactions_for_emoji,
        delete_,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/messages/", message_id.to_snowflake_str(), "/reactions/", ulp::str::url_encode(emoji)),
        const discusy::snowflake channel_id, const discusy::snowflake message_id, const std::string_view emoji
    )

    // https://docs.discord.com/developers/resources/message#delete-all-reactions-for-emoji
    DISCUSY_API_EMPTY_BODY(
        delete_all_reactions_for_emoji,
        delete_,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/messages/", message_id.to_snowflake_str(), "/reactions/", format_emoji(emoji)),
        const discusy::snowflake channel_id, const discusy::snowflake message_id, const discusy::emoji::emoji& emoji
    )

    // https://docs.discord.com/developers/resources/message#edit-message
    DISCUSY_API_MULTIPART_JSON_BODY(
        edit_message,
        patch,
        discusy::message::message,
        handle_multipart_files_body(edit, files),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/messages/", message_id.to_snowflake_str()),
        const discusy::snowflake channel_id, const discusy::snowflake message_id, const discusy::api::message::edit_message& edit, upload_files_param files = {}
    )

    // https://docs.discord.com/developers/resources/message#bulk-delete-messages
    DISCUSY_API_JSON_BODY_REASON(
        bulk_delete_messages,
        post,
        void,
        handle_json_body(del),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/messages/bulk-delete"),
        const discusy::snowflake channel_id, const discusy::api::message::bulk_delete_messages& del
    )

    // https://docs.discord.com/developers/resources/message#get-channel-pins
    DISCUSY_API_EMPTY_BODY(
        get_channel_pins,
        get,
        discusy::api::message::get_channel_pins_response,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/messages/pins", make_query_string(query)),
        const discusy::snowflake channel_id, const discusy::api::message::get_channel_pins_query& query = {}
    )

    // https://docs.discord.com/developers/resources/message#pin-message
    DISCUSY_API_EMPTY_BODY_REASON(
        pin_message,
        put,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/messages/pins/", message_id.to_snowflake_str()),
        const discusy::snowflake channel_id, const discusy::snowflake message_id
    )

    // https://docs.discord.com/developers/resources/message#unpin-message
    DISCUSY_API_EMPTY_BODY_REASON(
        unpin_message,
        delete_,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/messages/pins/", message_id.to_snowflake_str()),
        const discusy::snowflake channel_id, const discusy::snowflake message_id
    )

    // deprecated, use get_channel_pins
    // https://docs.discord.com/developers/resources/message#get-pinned-messages
    DISCUSY_API_EMPTY_BODY(
        get_pinned_messages,
        get,
        std::vector<discusy::message::message>,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/pins"),
        const discusy::snowflake channel_id
    )

    // deprecated, use pin_message
    // https://docs.discord.com/developers/resources/message#pin-message-deprecated
    DISCUSY_API_EMPTY_BODY_REASON(
        pin_message_deprecated,
        put,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/pins/", message_id.to_snowflake_str()),
        const discusy::snowflake channel_id, const discusy::snowflake message_id
    )

    // deprecated, use unpin_message
    // https://docs.discord.com/developers/resources/message#unpin-message-deprecated
    DISCUSY_API_EMPTY_BODY_REASON(
        unpin_message_deprecated,
        delete_,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/pins/", message_id.to_snowflake_str()),
        const discusy::snowflake channel_id, const discusy::snowflake message_id
    )

    // https://docs.discord.com/developers/resources/poll#get-answer-voters
    DISCUSY_API_EMPTY_BODY(
        get_answer_voters,
        get,
        discusy::api::poll::get_answer_voters_response,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/polls/", message_id.to_snowflake_str(), "/answers/", std::to_string(answer_id), make_query_string(query)),
        const discusy::snowflake channel_id, const discusy::snowflake message_id, const discusy::integer answer_id, const discusy::api::poll::get_answer_voters_query& query = {}
    )

    // https://docs.discord.com/developers/resources/poll#end-poll
    DISCUSY_API_EMPTY_BODY(
        end_poll,
        post,
        discusy::message::message,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/polls/", message_id.to_snowflake_str(), "/expire"),
        const discusy::snowflake channel_id, const discusy::snowflake message_id
    )

    // https://docs.discord.com/developers/interactions/receiving-and-responding#create-interaction-response
    DISCUSY_API_MULTIPART_JSON_BODY(
        create_interaction_response,
        post,
        discusy::api::interaction::interaction_callback_response,
        handle_multipart_files_body(resp, files),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/interactions/", interaction_id.to_snowflake_str(), "/", interaction_token, "/callback", make_query_string(query)),
        const discusy::snowflake interaction_id, const std::string_view interaction_token, const discusy::api::interaction::interaction_response& resp, upload_files_param files = {}, const discusy::api::interaction::create_interaction_response_query& query = {}
    )

    // https://docs.discord.com/developers/interactions/receiving-and-responding#get-original-interaction-response
    DISCUSY_API_EMPTY_BODY(
        get_original_interaction_response,
        get,
        discusy::message::message,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/webhooks/", state_.application_id, "/", interaction_token, "/messages/@original", make_query_string(query)),
        const std::string_view interaction_token, const discusy::api::webhook::get_webhook_message_query& query = {}
    )

    // https://docs.discord.com/developers/interactions/receiving-and-responding#edit-original-interaction-response
    DISCUSY_API_MULTIPART_JSON_BODY(
        edit_original_interaction_response,
        patch,
        discusy::message::message,
        handle_multipart_files_body(edit, files),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/webhooks/", state_.application_id, "/", interaction_token, "/messages/@original", make_query_string(query)),
        const std::string_view interaction_token, const discusy::api::webhook::edit_webhook_message& edit, upload_files_param files = {}, const discusy::api::webhook::edit_webhook_message_query& query = {}
    )

    // https://docs.discord.com/developers/interactions/receiving-and-responding#delete-original-interaction-response
    DISCUSY_API_EMPTY_BODY(
        delete_original_interaction_response,
        delete_,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/webhooks/", state_.application_id, "/", interaction_token, "/messages/@original"),
        const std::string_view interaction_token
    )

    // wait is always true
    // https://docs.discord.com/developers/interactions/receiving-and-responding#create-followup-message
    DISCUSY_API_MULTIPART_JSON_BODY(
        create_followup_message,
        post,
        discusy::message::message,
        handle_multipart_files_body(create, files),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/webhooks/", state_.application_id, "/", interaction_token, make_query_string(query)),
        const std::string_view interaction_token, const discusy::api::webhook::execute_webhook& create, upload_files_param files = {}, const discusy::api::webhook::execute_webhook_query& query = {}
    )

    // https://docs.discord.com/developers/interactions/receiving-and-responding#get-followup-message
    DISCUSY_API_EMPTY_BODY(
        get_followup_message,
        get,
        discusy::message::message,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/webhooks/", state_.application_id, "/", interaction_token, "/messages/", message_id.to_snowflake_str(), make_query_string(query)),
        const std::string_view interaction_token, const discusy::snowflake message_id, const discusy::api::webhook::get_webhook_message_query& query = {}
    )

    // https://docs.discord.com/developers/interactions/receiving-and-responding#edit-followup-message
    DISCUSY_API_MULTIPART_JSON_BODY(
        edit_followup_message,
        patch,
        discusy::message::message,
        handle_multipart_files_body(edit, files),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/webhooks/", state_.application_id, "/", interaction_token, "/messages/", message_id.to_snowflake_str(), make_query_string(query)),
        const std::string_view interaction_token, const discusy::snowflake message_id, const discusy::api::webhook::edit_webhook_message& edit, upload_files_param files = {}, const discusy::api::webhook::edit_webhook_message_query& query = {}
    )

    // https://docs.discord.com/developers/interactions/receiving-and-responding#delete-followup-message
    DISCUSY_API_EMPTY_BODY(
        delete_followup_message,
        delete_,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/webhooks/", state_.application_id, "/", interaction_token, "/messages/", message_id.to_snowflake_str()),
        const std::string_view interaction_token, const discusy::snowflake message_id
    )

    // https://docs.discord.com/developers/interactions/application-commands#create-global-application-command
    DISCUSY_API_JSON_BODY(
        create_global_application_command,
        post,
        discusy::application_commands::application_command,
        handle_json_body(cmd),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/applications/", state_.application_id, "/commands"),
        const discusy::api::application_commands::application_command& cmd
    )

    // https://docs.discord.com/developers/interactions/application-commands#delete-global-application-command
    DISCUSY_API_EMPTY_BODY(
        delete_global_application_command,
        delete_,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/applications/", state_.application_id, "/commands/", command_id.to_snowflake_str()),
        const discusy::snowflake command_id
    )

    // https://docs.discord.com/developers/interactions/application-commands#bulk-overwrite-global-application-commands
    DISCUSY_API_JSON_BODY(
        bulk_overwrite_global_application_commands,
        put,
        std::vector<discusy::application_commands::application_command>,
        handle_json_body(cmds),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/applications/", state_.application_id, "/commands"),
        std::span<const discusy::api::application_commands::application_command> cmds
    )

    // https://docs.discord.com/developers/resources/emoji#list-guild-emojis
    DISCUSY_API_EMPTY_BODY(
        list_guild_emojis,
        get,
        std::vector<discusy::emoji::emoji>,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/emojis"),
        const discusy::snowflake guild_id
    )

    // https://docs.discord.com/developers/resources/emoji#get-guild-emoji
    DISCUSY_API_EMPTY_BODY(
        get_guild_emoji,
        get,
        discusy::emoji::emoji,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/emojis/", emoji_id.to_snowflake_str()),
        const discusy::snowflake guild_id, const discusy::snowflake emoji_id
    )

    // https://docs.discord.com/developers/resources/emoji#create-guild-emoji
    DISCUSY_API_JSON_BODY_REASON(
        create_guild_emoji,
        post,
        discusy::emoji::emoji,
        handle_json_body(create),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/emojis"),
        const discusy::snowflake guild_id, const discusy::api::emoji::create_guild_emoji& create
    )

    // https://docs.discord.com/developers/resources/emoji#modify-guild-emoji
    DISCUSY_API_JSON_BODY_REASON(
        modify_guild_emoji,
        patch,
        discusy::emoji::emoji,
        handle_json_body(update),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/emojis/", emoji_id.to_snowflake_str()),
        const discusy::snowflake guild_id, const discusy::snowflake emoji_id, const discusy::api::emoji::modify_guild_emoji& update
    )

    // https://docs.discord.com/developers/resources/emoji#delete-guild-emoji
    DISCUSY_API_EMPTY_BODY_REASON(
        delete_guild_emoji,
        delete_,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/emojis/", emoji_id.to_snowflake_str()),
        const discusy::snowflake guild_id, const discusy::snowflake emoji_id
    )

    using application_emojis_t = discusy::api::emoji::list_application_emojis_response;
    // https://docs.discord.com/developers/resources/emoji#list-application-emojis
    DISCUSY_API_EMPTY_BODY(
        list_application_emojis,
        get,
        application_emojis_t,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/applications/", state_.application_id, "/emojis")
    )

    // https://docs.discord.com/developers/resources/emoji#get-application-emoji
    DISCUSY_API_EMPTY_BODY(
        get_application_emoji,
        get,
        discusy::emoji::emoji,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/applications/", state_.application_id, "/emojis/", emoji_id.to_snowflake_str()),
        const discusy::snowflake emoji_id
    )

    // https://docs.discord.com/developers/resources/emoji#create-application-emoji
    DISCUSY_API_JSON_BODY(
        create_application_emoji,
        post,
        discusy::emoji::emoji,
        handle_json_body(create),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/applications/", state_.application_id, "/emojis"),
        const discusy::api::emoji::create_application_emoji& create
    )

    // https://docs.discord.com/developers/resources/emoji#modify-application-emoji
    DISCUSY_API_JSON_BODY(
        modify_application_emoji,
        patch,
        discusy::emoji::emoji,
        handle_json_body(update),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/applications/", state_.application_id, "/emojis/", emoji_id.to_snowflake_str()),
        const discusy::snowflake emoji_id, const discusy::api::emoji::modify_application_emoji& update
    )

    // https://docs.discord.com/developers/resources/emoji#delete-application-emoji
    DISCUSY_API_EMPTY_BODY(
        delete_application_emoji,
        delete_,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/applications/", state_.application_id, "/emojis/", emoji_id.to_snowflake_str()),
        const discusy::snowflake emoji_id
    )

    // https://docs.discord.com/developers/resources/sticker#get-sticker
    DISCUSY_API_EMPTY_BODY(
        get_sticker,
        get,
        discusy::sticker::sticker,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/stickers/", sticker_id.to_snowflake_str()),
        const discusy::snowflake sticker_id
    )

    // https://docs.discord.com/developers/resources/sticker#list-sticker-packs
    DISCUSY_API_EMPTY_BODY(
        list_sticker_packs,
        get,
        discusy::api::sticker::list_sticker_packs_response,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/sticker-packs")
    )

    // https://docs.discord.com/developers/resources/sticker#get-sticker-pack
    DISCUSY_API_EMPTY_BODY(
        get_sticker_pack,
        get,
        discusy::sticker::sticker_pack,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/sticker-packs/", pack_id.to_snowflake_str()),
        const discusy::snowflake pack_id
    )

    // https://docs.discord.com/developers/resources/sticker#list-guild-stickers
    DISCUSY_API_EMPTY_BODY(
        list_guild_stickers,
        get,
        std::vector<discusy::sticker::sticker>,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/stickers"),
        const discusy::snowflake guild_id
    )

    // https://docs.discord.com/developers/resources/sticker#get-guild-sticker
    DISCUSY_API_EMPTY_BODY(
        get_guild_sticker,
        get,
        discusy::sticker::sticker,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/stickers/", sticker_id.to_snowflake_str()),
        const discusy::snowflake guild_id, const discusy::snowflake sticker_id
    )

    // the sticker file itself is sent as its own multipart part named "file"
    // https://docs.discord.com/developers/resources/sticker#create-guild-sticker
    DISCUSY_API_MULTIPART_JSON_BODY_REASON(
        create_guild_sticker,
        post,
        discusy::sticker::sticker,
        handle_multipart_named_file_body(create, "file", file),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/stickers"),
        const discusy::snowflake guild_id, const discusy::api::sticker::create_guild_sticker& create, const discusy::upload_file& file
    )

    // https://docs.discord.com/developers/resources/sticker#modify-guild-sticker
    DISCUSY_API_JSON_BODY_REASON(
        modify_guild_sticker,
        patch,
        discusy::sticker::sticker,
        handle_json_body(update),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/stickers/", sticker_id.to_snowflake_str()),
        const discusy::snowflake guild_id, const discusy::snowflake sticker_id, const discusy::api::sticker::modify_guild_sticker& update
    )

    // https://docs.discord.com/developers/resources/sticker#delete-guild-sticker
    DISCUSY_API_EMPTY_BODY_REASON(
        delete_guild_sticker,
        delete_,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/stickers/", sticker_id.to_snowflake_str()),
        const discusy::snowflake guild_id, const discusy::snowflake sticker_id
    )

    // https://docs.discord.com/developers/resources/soundboard#send-soundboard-sound
    DISCUSY_API_JSON_BODY(
        send_soundboard_sound,
        post,
        void,
        handle_json_body(send),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/channels/", channel_id.to_snowflake_str(), "/send-soundboard-sound"),
        const discusy::snowflake channel_id, const discusy::api::soundboard::send_soundboard_sound& send
    )

    // https://docs.discord.com/developers/resources/soundboard#list-default-soundboard-sounds
    DISCUSY_API_EMPTY_BODY(
        list_default_soundboard_sounds,
        get,
        std::vector<discusy::soundboard::soundboard_sound>,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/soundboard-default-sounds")
    )

    // https://docs.discord.com/developers/resources/soundboard#list-guild-soundboard-sounds
    DISCUSY_API_EMPTY_BODY(
        list_guild_soundboard_sounds,
        get,
        discusy::api::soundboard::list_guild_soundboard_sounds_response,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/soundboard-sounds"),
        const discusy::snowflake guild_id
    )

    // https://docs.discord.com/developers/resources/soundboard#get-guild-soundboard-sound
    DISCUSY_API_EMPTY_BODY(
        get_guild_soundboard_sound,
        get,
        discusy::soundboard::soundboard_sound,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/soundboard-sounds/", sound_id.to_snowflake_str()),
        const discusy::snowflake guild_id, const discusy::snowflake sound_id
    )

    // https://docs.discord.com/developers/resources/soundboard#create-guild-soundboard-sound
    DISCUSY_API_JSON_BODY_REASON(
        create_guild_soundboard_sound,
        post,
        discusy::soundboard::soundboard_sound,
        handle_json_body(create),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/soundboard-sounds"),
        const discusy::snowflake guild_id, const discusy::api::soundboard::create_guild_soundboard_sound& create
    )

    // https://docs.discord.com/developers/resources/soundboard#modify-guild-soundboard-sound
    DISCUSY_API_JSON_BODY_REASON(
        modify_guild_soundboard_sound,
        patch,
        discusy::soundboard::soundboard_sound,
        handle_json_body(update),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/soundboard-sounds/", sound_id.to_snowflake_str()),
        const discusy::snowflake guild_id, const discusy::snowflake sound_id, const discusy::api::soundboard::modify_guild_soundboard_sound& update
    )

    // https://docs.discord.com/developers/resources/soundboard#delete-guild-soundboard-sound
    DISCUSY_API_EMPTY_BODY_REASON(
        delete_guild_soundboard_sound,
        delete_,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/soundboard-sounds/", sound_id.to_snowflake_str()),
        const discusy::snowflake guild_id, const discusy::snowflake sound_id
    )

    // https://docs.discord.com/developers/resources/stage-instance#create-stage-instance
    DISCUSY_API_JSON_BODY_REASON(
        create_stage_instance,
        post,
        discusy::stage_instance::stage_instance,
        handle_json_body(create),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/stage-instances"),
        const discusy::api::stage_instance::create_stage_instance& create
    )

    // https://docs.discord.com/developers/resources/stage-instance#get-stage-instance
    DISCUSY_API_EMPTY_BODY(
        get_stage_instance,
        get,
        discusy::stage_instance::stage_instance,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/stage-instances/", channel_id.to_snowflake_str()),
        const discusy::snowflake channel_id
    )

    // https://docs.discord.com/developers/resources/stage-instance#modify-stage-instance
    DISCUSY_API_JSON_BODY_REASON(
        modify_stage_instance,
        patch,
        discusy::stage_instance::stage_instance,
        handle_json_body(update),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/stage-instances/", channel_id.to_snowflake_str()),
        const discusy::snowflake channel_id, const discusy::api::stage_instance::modify_stage_instance& update
    )

    // https://docs.discord.com/developers/resources/stage-instance#delete-stage-instance
    DISCUSY_API_EMPTY_BODY_REASON(
        delete_stage_instance,
        delete_,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/stage-instances/", channel_id.to_snowflake_str()),
        const discusy::snowflake channel_id
    )

    // https://docs.discord.com/developers/resources/voice#list-voice-regions
    DISCUSY_API_EMPTY_BODY(
        list_voice_regions,
        get,
        std::vector<discusy::voice::voice_region>,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/voice/regions")
    )

    // https://docs.discord.com/developers/resources/voice#get-current-user-voice-state
    DISCUSY_API_EMPTY_BODY(
        get_current_user_voice_state,
        get,
        discusy::voice::voice_state,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/voice-states/@me"),
        const discusy::snowflake guild_id
    )

    // https://docs.discord.com/developers/resources/voice#get-user-voice-state
    DISCUSY_API_EMPTY_BODY(
        get_user_voice_state,
        get,
        discusy::voice::voice_state,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/voice-states/", user_id.to_snowflake_str()),
        const discusy::snowflake guild_id, const discusy::snowflake user_id
    )

    // https://docs.discord.com/developers/resources/voice#modify-current-user-voice-state
    DISCUSY_API_JSON_BODY(
        modify_current_user_voice_state,
        patch,
        void,
        handle_json_body(update),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/voice-states/@me"),
        const discusy::snowflake guild_id, const discusy::api::voice::modify_current_user_voice_state& update
    )

    // https://docs.discord.com/developers/resources/voice#modify-user-voice-state
    DISCUSY_API_JSON_BODY(
        modify_user_voice_state,
        patch,
        void,
        handle_json_body(update),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/voice-states/", user_id.to_snowflake_str()),
        const discusy::snowflake guild_id, const discusy::snowflake user_id, const discusy::api::voice::modify_user_voice_state& update
    )

    // https://docs.discord.com/developers/resources/invite#get-invite
    DISCUSY_API_EMPTY_BODY(
        get_invite,
        get,
        discusy::invite::invite,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/invites/", ulp::str::url_encode(invite_code), make_query_string(query)),
        const std::string_view invite_code, const discusy::api::invite::get_invite_query& query = {}
    )

    // https://docs.discord.com/developers/resources/invite#delete-invite
    DISCUSY_API_EMPTY_BODY_REASON(
        delete_invite,
        delete_,
        discusy::invite::invite,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/invites/", ulp::str::url_encode(invite_code)),
        const std::string_view invite_code
    )

    // returns the raw CSV of target users
    // https://docs.discord.com/developers/resources/invite#get-target-users
    DISCUSY_API_EMPTY_BODY(
        get_target_users,
        get,
        http_api::raw_data,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/invites/", ulp::str::url_encode(invite_code), "/target-users"),
        const std::string_view invite_code
    )

    // https://docs.discord.com/developers/resources/invite#add-target-user
    DISCUSY_API_EMPTY_BODY(
        add_target_users,
        put,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/invites/", ulp::str::url_encode(invite_code), "/target-users/", user_id.to_snowflake_str()),
        const std::string_view invite_code, const discusy::snowflake user_id
    )

    // https://docs.discord.com/developers/resources/invite#remove-target-user
    DISCUSY_API_EMPTY_BODY(
        remove_target_users,
        delete_,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/invites/", ulp::str::url_encode(invite_code), "/target-users/", user_id.to_snowflake_str()),
        const std::string_view invite_code, const discusy::snowflake user_id
    )

    // https://docs.discord.com/developers/resources/invite#update-target-users
    DISCUSY_API_MULTIPART_JSON_BODY(
        update_target_users,
        put,
        void,
        handle_multipart_only_file_body("target_users_file", file),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/invites/", ulp::str::url_encode(invite_code), "/target-users"),
        const std::string_view invite_code, const discusy::upload_file& file
    )

    // https://docs.discord.com/developers/resources/invite#bulk-add-target-users
    DISCUSY_API_JSON_BODY(
        bulk_add_target_users,
        post,
        void,
        handle_json_body(bulk_add),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/invites/", ulp::str::url_encode(invite_code), "/target-users/bulk-add"),
        const std::string_view invite_code, const discusy::api::invite::bulk_add_delete_target_users& bulk_add
    )

    // https://docs.discord.com/developers/resources/invite#bulk-delete-target-users
    DISCUSY_API_JSON_BODY(
        bulk_delete_target_users,
        post,
        void,
        handle_json_body(bulk_delete),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/invites/", ulp::str::url_encode(invite_code), "/target-users/bulk-delete"),
        const std::string_view invite_code, const discusy::api::invite::bulk_add_delete_target_users& bulk_delete
    )

    // https://docs.discord.com/developers/resources/invite#get-target-users-job-status
    DISCUSY_API_EMPTY_BODY(
        get_target_users_job_status,
        get,
        discusy::api::invite::target_users_job_status,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/invites/", ulp::str::url_encode(invite_code), "/target-users/job-status"),
        const std::string_view invite_code
    )

    // https://docs.discord.com/developers/resources/guild-template#get-guild-template
    DISCUSY_API_EMPTY_BODY(
        get_guild_template,
        get,
        discusy::guild_template::guild_template,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/templates/", ulp::str::url_encode(template_code)),
        const std::string_view template_code
    )

    // https://docs.discord.com/developers/resources/guild-template#get-guild-templates
    DISCUSY_API_EMPTY_BODY(
        get_guild_templates,
        get,
        std::vector<discusy::guild_template::guild_template>,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/templates"),
        const discusy::snowflake guild_id
    )

    // https://docs.discord.com/developers/resources/guild-template#create-guild-template
    DISCUSY_API_JSON_BODY(
        create_guild_template,
        post,
        discusy::guild_template::guild_template,
        handle_json_body(create),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/templates"),
        const discusy::snowflake guild_id, const discusy::api::guild_template::create_guild_template& create
    )

    // https://docs.discord.com/developers/resources/guild-template#sync-guild-template
    DISCUSY_API_EMPTY_BODY(
        sync_guild_template,
        put,
        discusy::guild_template::guild_template,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/templates/", ulp::str::url_encode(template_code)),
        const discusy::snowflake guild_id, const std::string_view template_code
    )

    // https://docs.discord.com/developers/resources/guild-template#modify-guild-template
    DISCUSY_API_JSON_BODY(
        modify_guild_template,
        patch,
        discusy::guild_template::guild_template,
        handle_json_body(update),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/templates/", ulp::str::url_encode(template_code)),
        const discusy::snowflake guild_id, const std::string_view template_code, const discusy::api::guild_template::modify_guild_template& update
    )

    // https://docs.discord.com/developers/resources/guild-template#delete-guild-template
    DISCUSY_API_EMPTY_BODY(
        delete_guild_template,
        delete_,
        discusy::guild_template::guild_template,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/templates/", ulp::str::url_encode(template_code)),
        const discusy::snowflake guild_id, const std::string_view template_code
    )

    // https://docs.discord.com/developers/resources/guild-scheduled-event#list-scheduled-events-for-guild
    DISCUSY_API_EMPTY_BODY(
        list_scheduled_events_for_guild,
        get,
        std::vector<discusy::guild_scheduled_event::guild_scheduled_event>,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/scheduled-events", make_query_string(query)),
        const discusy::snowflake guild_id, const discusy::api::guild_scheduled_event::list_scheduled_events_for_guild_query& query = {}
    )

    // https://docs.discord.com/developers/resources/guild-scheduled-event#create-guild-scheduled-event
    DISCUSY_API_JSON_BODY_REASON(
        create_guild_scheduled_event,
        post,
        discusy::guild_scheduled_event::guild_scheduled_event,
        handle_json_body(create),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/scheduled-events"),
        const discusy::snowflake guild_id, const discusy::api::guild_scheduled_event::create_guild_scheduled_event& create
    )

    // https://docs.discord.com/developers/resources/guild-scheduled-event#get-guild-scheduled-event
    DISCUSY_API_EMPTY_BODY(
        get_guild_scheduled_event,
        get,
        discusy::guild_scheduled_event::guild_scheduled_event,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/scheduled-events/", event_id.to_snowflake_str(), make_query_string(query)),
        const discusy::snowflake guild_id, const discusy::snowflake event_id, const discusy::api::guild_scheduled_event::get_guild_scheduled_event_query& query = {}
    )

    // https://docs.discord.com/developers/resources/guild-scheduled-event#modify-guild-scheduled-event
    DISCUSY_API_JSON_BODY_REASON(
        modify_guild_scheduled_event,
        patch,
        discusy::guild_scheduled_event::guild_scheduled_event,
        handle_json_body(update),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/scheduled-events/", event_id.to_snowflake_str()),
        const discusy::snowflake guild_id, const discusy::snowflake event_id, const discusy::api::guild_scheduled_event::modify_guild_scheduled_event& update
    )

    // https://docs.discord.com/developers/resources/guild-scheduled-event#delete-guild-scheduled-event
    DISCUSY_API_EMPTY_BODY(
        delete_guild_scheduled_event,
        delete_,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/scheduled-events/", event_id.to_snowflake_str()),
        const discusy::snowflake guild_id, const discusy::snowflake event_id
    )

    // https://docs.discord.com/developers/resources/guild-scheduled-event#get-guild-scheduled-event-users
    DISCUSY_API_EMPTY_BODY(
        get_guild_scheduled_event_users,
        get,
        std::vector<discusy::guild_scheduled_event::guild_scheduled_event_user>,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/guilds/", guild_id.to_snowflake_str(), "/scheduled-events/", event_id.to_snowflake_str(), "/users", make_query_string(query)),
        const discusy::snowflake guild_id, const discusy::snowflake event_id, const discusy::api::guild_scheduled_event::get_guild_scheduled_event_users_query& query = {}
    )

    // https://docs.discord.com/developers/resources/user#get-current-user
    DISCUSY_API_EMPTY_BODY(
        get_current_user,
        get,
        discusy::user::user,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/users/@me")
    )

    // https://docs.discord.com/developers/resources/user#get-user
    DISCUSY_API_EMPTY_BODY(
        get_user,
        get,
        discusy::user::user,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/users/", user_id.to_snowflake_str()),
        const discusy::snowflake user_id
    )

    // TODO: could also update the me inside the state_ with the return value
    // https://docs.discord.com/developers/resources/user#modify-current-user
    DISCUSY_API_JSON_BODY(
        modify_current_user,
        patch,
        discusy::user::user,
        handle_json_body(update),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/users/@me"),
        const discusy::api::user::modify_current_user& update
    )

    // https://docs.discord.com/developers/resources/user#get-current-user-guilds
    DISCUSY_API_EMPTY_BODY(
        get_current_user_guilds,
        get,
        std::vector<discusy::guild::guild>,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/users/@me/guilds", make_query_string(query)),
        const discusy::api::user::get_current_user_guilds_query& query = {}
    )

    // https://docs.discord.com/developers/resources/user#get-current-user-guild-member
    DISCUSY_API_EMPTY_BODY(
        get_current_user_guild_member,
        get,
        discusy::guild::guild_member,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/users/@me/guilds/", guild_id.to_snowflake_str(), "/member"),
        const discusy::snowflake guild_id
    )

    // https://docs.discord.com/developers/resources/user#leave-guild
    DISCUSY_API_EMPTY_BODY(
        leave_guild,
        delete_,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/users/@me/guilds/", guild_id.to_snowflake_str()),
        const discusy::snowflake guild_id
    )

    // https://docs.discord.com/developers/resources/user#create-dm
    DISCUSY_API_JSON_BODY(
        create_dm,
        post,
        discusy::channel::channel,
        handle_json_body(create),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/users/@me/channels"),
        const discusy::api::user::create_dm& create
    )

    // https://docs.discord.com/developers/resources/user#create-group-dm
    DISCUSY_API_JSON_BODY(
        create_group_dm,
        post,
        discusy::channel::channel,
        handle_json_body(create),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/users/@me/channels"),
        const discusy::api::user::create_group_dm& create
    )

    // https://docs.discord.com/developers/resources/user#get-current-user-connections
    DISCUSY_API_EMPTY_BODY(
        get_current_user_connections,
        get,
        std::vector<discusy::user::connection>,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/users/@me/connections")
    )

    // https://docs.discord.com/developers/resources/user#get-current-user-application-role-connection
    DISCUSY_API_EMPTY_BODY(
        get_current_user_application_role_connection,
        get,
        discusy::user::application_role_connection,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/users/@me/applications/", state_.application_id, "/role-connection")
    )

    // https://docs.discord.com/developers/resources/user#update-current-user-application-role-connection
    DISCUSY_API_JSON_BODY(
        update_current_user_application_role_connection,
        put,
        discusy::user::application_role_connection,
        handle_json_body(update),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/users/@me/applications/", state_.application_id, "/role-connection"),
        const discusy::api::user::update_current_user_application_role_connection& update
    )

    // https://docs.discord.com/developers/resources/user#delete-current-user-application-role-connection
    DISCUSY_API_EMPTY_BODY(
        delete_current_user_application_role_connection,
        delete_,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/users/@me/applications/", state_.application_id, "/role-connection")
    )

    // https://docs.discord.com/developers/resources/webhook#get-webhook
    DISCUSY_API_EMPTY_BODY(
        get_webhook,
        get,
        discusy::webhook::webhook,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/webhooks/", webhook_id.to_snowflake_str()),
        const discusy::snowflake webhook_id
    )

    // https://docs.discord.com/developers/resources/webhook#get-webhook-with-token
    DISCUSY_API_EMPTY_BODY(
        get_webhook_with_token,
        get,
        discusy::webhook::webhook,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/webhooks/", webhook_id.to_snowflake_str(), "/", webhook_token),
        const discusy::snowflake webhook_id, const std::string_view webhook_token
    )

    // https://docs.discord.com/developers/resources/webhook#modify-webhook
    DISCUSY_API_JSON_BODY_REASON(
        modify_webhook,
        patch,
        discusy::webhook::webhook,
        handle_json_body(update),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/webhooks/", webhook_id.to_snowflake_str()),
        const discusy::snowflake webhook_id, const discusy::api::webhook::modify_webhook& update
    )

    // https://docs.discord.com/developers/resources/webhook#modify-webhook-with-token
    DISCUSY_API_JSON_BODY_REASON(
        modify_webhook_with_token,
        patch,
        discusy::webhook::webhook,
        handle_json_body(update),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/webhooks/", webhook_id.to_snowflake_str(), "/", webhook_token),
        const discusy::snowflake webhook_id, const std::string_view webhook_token, const discusy::api::webhook::modify_webhook_with_token& update
    )

    // https://docs.discord.com/developers/resources/webhook#delete-webhook
    DISCUSY_API_EMPTY_BODY_REASON(
        delete_webhook,
        delete_,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/webhooks/", webhook_id.to_snowflake_str()),
        const discusy::snowflake webhook_id
    )

    // https://docs.discord.com/developers/resources/webhook#delete-webhook-with-token
    DISCUSY_API_EMPTY_BODY_REASON(
        delete_webhook_with_token,
        delete_,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/webhooks/", webhook_id.to_snowflake_str(), "/", webhook_token),
        const discusy::snowflake webhook_id, const std::string_view webhook_token
    )

    // only returns a message when wait is set to true in the query
    // https://docs.discord.com/developers/resources/webhook#execute-webhook
    DISCUSY_API_MULTIPART_JSON_BODY(
        execute_webhook,
        post,
        discusy::message::message,
        handle_multipart_files_body(execute, files),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/webhooks/", webhook_id.to_snowflake_str(), "/", webhook_token, make_query_string(query)),
        const discusy::snowflake webhook_id, const std::string_view webhook_token, const discusy::api::webhook::execute_webhook& execute, upload_files_param files = {}, const discusy::api::webhook::execute_webhook_query& query = {}
    )

    // takes a raw slack formatted payload as the body
    // https://docs.discord.com/developers/resources/webhook#execute-slack-compatible-webhook
    DISCUSY_API_JSON_BODY(
        execute_slack_compatible_webhook,
        post,
        void,
        handle_raw_body(payload),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/webhooks/", webhook_id.to_snowflake_str(), "/", webhook_token, "/slack", make_query_string(query)),
        const discusy::snowflake webhook_id, const std::string_view webhook_token, const std::string_view payload, const discusy::api::webhook::execute_compatible_webhook_query& query = {}
    )

    // takes a raw github formatted payload as the body
    // https://docs.discord.com/developers/resources/webhook#execute-github-compatible-webhook
    DISCUSY_API_JSON_BODY(
        execute_github_compatible_webhook,
        post,
        void,
        handle_raw_body(payload),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/webhooks/", webhook_id.to_snowflake_str(), "/", webhook_token, "/github", make_query_string(query)),
        const discusy::snowflake webhook_id, const std::string_view webhook_token, const std::string_view payload, const discusy::api::webhook::execute_compatible_webhook_query& query = {}
    )

    // https://docs.discord.com/developers/resources/webhook#get-webhook-message
    DISCUSY_API_EMPTY_BODY(
        get_webhook_message,
        get,
        discusy::message::message,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/webhooks/", webhook_id.to_snowflake_str(), "/", webhook_token, "/messages/", message_id.to_snowflake_str(), make_query_string(query)),
        const discusy::snowflake webhook_id, const std::string_view webhook_token, const discusy::snowflake message_id, const discusy::api::webhook::get_webhook_message_query& query = {}
    )

    // https://docs.discord.com/developers/resources/webhook#edit-webhook-message
    DISCUSY_API_MULTIPART_JSON_BODY(
        edit_webhook_message,
        patch,
        discusy::message::message,
        handle_multipart_files_body(edit, files),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/webhooks/", webhook_id.to_snowflake_str(), "/", webhook_token, "/messages/", message_id.to_snowflake_str(), make_query_string(query)),
        const discusy::snowflake webhook_id, const std::string_view webhook_token, const discusy::snowflake message_id, const discusy::api::webhook::edit_webhook_message& edit, upload_files_param files = {}, const discusy::api::webhook::edit_webhook_message_query& query = {}
    )

    // https://docs.discord.com/developers/resources/webhook#delete-webhook-message
    DISCUSY_API_EMPTY_BODY(
        delete_webhook_message,
        delete_,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/webhooks/", webhook_id.to_snowflake_str(), "/", webhook_token, "/messages/", message_id.to_snowflake_str(), make_query_string(query)),
        const discusy::snowflake webhook_id, const std::string_view webhook_token, const discusy::snowflake message_id, const discusy::api::webhook::delete_webhook_message_query& query = {}
    )

    // https://docs.discord.com/developers/resources/entitlement#list-entitlements
    DISCUSY_API_EMPTY_BODY(
        list_entitlements,
        get,
        std::vector<discusy::entitlement::entitlement>,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/applications/", state_.application_id, "/entitlements", make_query_string(query)),
        const discusy::api::entitlement::list_entitlements_query& query = {}
    )

    // https://docs.discord.com/developers/resources/entitlement#get-entitlement
    DISCUSY_API_EMPTY_BODY(
        get_entitlement,
        get,
        discusy::entitlement::entitlement,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/applications/", state_.application_id, "/entitlements/", entitlement_id.to_snowflake_str()),
        const discusy::snowflake entitlement_id
    )

    // https://docs.discord.com/developers/resources/entitlement#consume-an-entitlement
    DISCUSY_API_EMPTY_BODY(
        consume_entitlement,
        post,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/applications/", state_.application_id, "/entitlements/", entitlement_id.to_snowflake_str(), "/consume"),
        const discusy::snowflake entitlement_id
    )

    // https://docs.discord.com/developers/resources/entitlement#create-test-entitlement
    DISCUSY_API_JSON_BODY(
        create_test_entitlement,
        post,
        discusy::entitlement::entitlement,
        handle_json_body(create),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/applications/", state_.application_id, "/entitlements"),
        const discusy::api::entitlement::create_test_entitlement& create
    )

    // https://docs.discord.com/developers/resources/entitlement#delete-test-entitlement
    DISCUSY_API_EMPTY_BODY(
        delete_test_entitlement,
        delete_,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/applications/", state_.application_id, "/entitlements/", entitlement_id.to_snowflake_str()),
        const discusy::snowflake entitlement_id
    )

    // https://docs.discord.com/developers/resources/sku#list-skus
    DISCUSY_API_EMPTY_BODY(
        list_skus,
        get,
        std::vector<discusy::sku::sku>,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/applications/", state_.application_id, "/skus")
    )

    // https://docs.discord.com/developers/resources/subscription#list-sku-subscriptions
    DISCUSY_API_EMPTY_BODY(
        list_sku_subscriptions,
        get,
        std::vector<discusy::subscription::subscription>,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/skus/", sku_id.to_snowflake_str(), "/subscriptions", make_query_string(query)),
        const discusy::snowflake sku_id, const discusy::api::subscription::list_sku_subscriptions_query& query = {}
    )

    // https://docs.discord.com/developers/resources/subscription#get-sku-subscription
    DISCUSY_API_EMPTY_BODY(
        get_sku_subscription,
        get,
        discusy::subscription::subscription,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/skus/", sku_id.to_snowflake_str(), "/subscriptions/", subscription_id.to_snowflake_str()),
        const discusy::snowflake sku_id, const discusy::snowflake subscription_id
    )

    // https://docs.discord.com/developers/resources/lobby#create-lobby
    DISCUSY_API_JSON_BODY(
        create_lobby,
        post,
        discusy::lobby::lobby,
        handle_json_body(create),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/lobbies"),
        const discusy::api::lobby::create_lobby& create = {}
    )

    // https://docs.discord.com/developers/resources/lobby#create-or-join-lobby
    DISCUSY_API_JSON_BODY(
        create_or_join_lobby,
        put,
        discusy::lobby::lobby,
        handle_json_body(create),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/lobbies"),
        const discusy::api::lobby::create_or_join_lobby& create
    )

    // https://docs.discord.com/developers/resources/lobby#get-lobby
    DISCUSY_API_EMPTY_BODY(
        get_lobby,
        get,
        discusy::lobby::lobby,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/lobbies/", lobby_id.to_snowflake_str()),
        const discusy::snowflake lobby_id
    )

    // https://docs.discord.com/developers/resources/lobby#modify-lobby
    DISCUSY_API_JSON_BODY(
        modify_lobby,
        patch,
        discusy::lobby::lobby,
        handle_json_body(update),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/lobbies/", lobby_id.to_snowflake_str()),
        const discusy::snowflake lobby_id, const discusy::api::lobby::modify_lobby& update
    )

    // https://docs.discord.com/developers/resources/lobby#delete-lobby
    DISCUSY_API_EMPTY_BODY(
        delete_lobby,
        delete_,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/lobbies/", lobby_id.to_snowflake_str()),
        const discusy::snowflake lobby_id
    )

    // https://docs.discord.com/developers/resources/lobby#add-a-member-to-a-lobby
    DISCUSY_API_JSON_BODY(
        add_lobby_member,
        put,
        discusy::lobby::lobby_member,
        handle_json_body(add),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/lobbies/", lobby_id.to_snowflake_str(), "/members/", user_id.to_snowflake_str()),
        const discusy::snowflake lobby_id, const discusy::snowflake user_id, const discusy::api::lobby::add_lobby_member& add = {}
    )

    // https://docs.discord.com/developers/resources/lobby#bulk-update-lobby-members
    DISCUSY_API_JSON_BODY(
        bulk_update_lobby_members,
        post,
        std::vector<discusy::lobby::lobby_member>,
        handle_json_body(members),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/lobbies/", lobby_id.to_snowflake_str(), "/members/bulk"),
        const discusy::snowflake lobby_id, discusy::api::lobby::bulk_update_lobby_members members
    )

    // https://docs.discord.com/developers/resources/lobby#remove-a-member-from-a-lobby
    DISCUSY_API_EMPTY_BODY(
        remove_lobby_member,
        delete_,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/lobbies/", lobby_id.to_snowflake_str(), "/members/", user_id.to_snowflake_str()),
        const discusy::snowflake lobby_id, const discusy::snowflake user_id
    )

    // https://docs.discord.com/developers/resources/lobby#leave-lobby
    DISCUSY_API_EMPTY_BODY(
        leave_lobby,
        delete_,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/lobbies/", lobby_id.to_snowflake_str(), "/members/@me"),
        const discusy::snowflake lobby_id
    )

    // https://docs.discord.com/developers/resources/lobby#link-channel-to-lobby
    DISCUSY_API_JSON_BODY(
        link_channel_to_lobby,
        patch,
        discusy::lobby::lobby,
        handle_json_body(link),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/lobbies/", lobby_id.to_snowflake_str(), "/channel-linking"),
        const discusy::snowflake lobby_id, const discusy::api::lobby::link_channel_to_lobby& link
    )

    // https://docs.discord.com/developers/resources/lobby#unlink-channel-from-lobby
    DISCUSY_API_JSON_BODY(
        unlink_channel_from_lobby,
        patch,
        discusy::lobby::lobby,
        handle_raw_body("{}"),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/lobbies/", lobby_id.to_snowflake_str(), "/channel-linking"),
        const discusy::snowflake lobby_id
    )

    // https://docs.discord.com/developers/resources/lobby#send-lobby-message
    DISCUSY_API_JSON_BODY(
        send_lobby_message,
        post,
        discusy::lobby::lobby_message,
        handle_json_body(msg),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/lobbies/", lobby_id.to_snowflake_str(), "/messages"),
        const discusy::snowflake lobby_id, const discusy::api::lobby::send_lobby_message& msg
    )

    // https://docs.discord.com/developers/resources/lobby#get-lobby-messages
    DISCUSY_API_EMPTY_BODY(
        get_lobby_messages,
        get,
        std::vector<discusy::lobby::lobby_message>,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/lobbies/", lobby_id.to_snowflake_str(), "/messages", make_query_string(query)),
        const discusy::snowflake lobby_id, const discusy::api::lobby::get_lobby_messages_query& query = {}
    )

    // https://docs.discord.com/developers/resources/lobby#update-lobby-message-moderation-metadata
    DISCUSY_API_JSON_BODY(
        update_lobby_message_moderation_metadata,
        put,
        void,
        handle_json_body(metadata),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/lobbies/", lobby_id.to_snowflake_str(), "/messages/", message_id.to_snowflake_str(), "/moderation-metadata"),
        const discusy::snowflake lobby_id, const discusy::snowflake message_id, const discusy::api::lobby::update_lobby_message_moderation_metadata& metadata
    )

    // https://docs.discord.com/developers/resources/lobby#create-lobby-channel-invite-for-self
    DISCUSY_API_EMPTY_BODY(
        create_lobby_channel_invite_for_self,
        post,
        discusy::lobby::lobby_invite,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/lobbies/", lobby_id.to_snowflake_str(), "/members/@me/invites"),
        const discusy::snowflake lobby_id
    )

    // https://docs.discord.com/developers/resources/lobby#create-lobby-channel-invite-for-user
    DISCUSY_API_EMPTY_BODY(
        create_lobby_channel_invite_for_user,
        post,
        discusy::lobby::lobby_invite,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/lobbies/", lobby_id.to_snowflake_str(), "/members/", user_id.to_snowflake_str(), "/invites"),
        const discusy::snowflake lobby_id, const discusy::snowflake user_id
    )

    // https://docs.discord.com/developers/interactions/application-commands#get-global-application-commands
    DISCUSY_API_EMPTY_BODY(
        get_global_application_commands,
        get,
        std::vector<discusy::application_commands::application_command>,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/applications/", state_.application_id, "/commands", make_query_string(query)),
        const discusy::api::application_commands::get_application_commands_query& query = {}
    )

    // https://docs.discord.com/developers/interactions/application-commands#get-global-application-command
    DISCUSY_API_EMPTY_BODY(
        get_global_application_command,
        get,
        discusy::application_commands::application_command,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/applications/", state_.application_id, "/commands/", command_id.to_snowflake_str()),
        const discusy::snowflake command_id
    )

    // https://docs.discord.com/developers/interactions/application-commands#edit-global-application-command
    DISCUSY_API_JSON_BODY(
        edit_global_application_command,
        patch,
        discusy::application_commands::application_command,
        handle_json_body(cmd),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/applications/", state_.application_id, "/commands/", command_id.to_snowflake_str()),
        const discusy::snowflake command_id, const discusy::api::application_commands::application_command& cmd
    )

    // https://docs.discord.com/developers/interactions/application-commands#get-guild-application-commands
    DISCUSY_API_EMPTY_BODY(
        get_guild_application_commands,
        get,
        std::vector<discusy::application_commands::application_command>,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/applications/", state_.application_id, "/guilds/", guild_id.to_snowflake_str(), "/commands", make_query_string(query)),
        const discusy::snowflake guild_id, const discusy::api::application_commands::get_application_commands_query& query = {}
    )

    // https://docs.discord.com/developers/interactions/application-commands#create-guild-application-command
    DISCUSY_API_JSON_BODY(
        create_guild_application_command,
        post,
        discusy::application_commands::application_command,
        handle_json_body(cmd),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/applications/", state_.application_id, "/guilds/", guild_id.to_snowflake_str(), "/commands"),
        const discusy::snowflake guild_id, const discusy::api::application_commands::application_command& cmd
    )

    // https://docs.discord.com/developers/interactions/application-commands#get-guild-application-command
    DISCUSY_API_EMPTY_BODY(
        get_guild_application_command,
        get,
        discusy::application_commands::application_command,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/applications/", state_.application_id, "/guilds/", guild_id.to_snowflake_str(), "/commands/", command_id.to_snowflake_str()),
        const discusy::snowflake guild_id, const discusy::snowflake command_id
    )

    // https://docs.discord.com/developers/interactions/application-commands#edit-guild-application-command
    DISCUSY_API_JSON_BODY(
        edit_guild_application_command,
        patch,
        discusy::application_commands::application_command,
        handle_json_body(cmd),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/applications/", state_.application_id, "/guilds/", guild_id.to_snowflake_str(), "/commands/", command_id.to_snowflake_str()),
        const discusy::snowflake guild_id, const discusy::snowflake command_id, const discusy::api::application_commands::application_command& cmd
    )

    // https://docs.discord.com/developers/interactions/application-commands#delete-guild-application-command
    DISCUSY_API_EMPTY_BODY(
        delete_guild_application_command,
        delete_,
        void,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/applications/", state_.application_id, "/guilds/", guild_id.to_snowflake_str(), "/commands/", command_id.to_snowflake_str()),
        const discusy::snowflake guild_id, const discusy::snowflake command_id
    )

    // https://docs.discord.com/developers/interactions/application-commands#bulk-overwrite-guild-application-commands
    DISCUSY_API_JSON_BODY(
        bulk_overwrite_guild_application_commands,
        put,
        std::vector<discusy::application_commands::application_command>,
        handle_json_body(cmds),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/applications/", state_.application_id, "/guilds/", guild_id.to_snowflake_str(), "/commands"),
        const discusy::snowflake guild_id, std::span<const discusy::api::application_commands::application_command> cmds
    )

    // https://docs.discord.com/developers/interactions/application-commands#get-guild-application-command-permissions
    DISCUSY_API_EMPTY_BODY(
        get_guild_application_command_permissions,
        get,
        std::vector<discusy::application_commands::guild_application_command_permissions>,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/applications/", state_.application_id, "/guilds/", guild_id.to_snowflake_str(), "/commands/permissions"),
        const discusy::snowflake guild_id
    )

    // https://docs.discord.com/developers/interactions/application-commands#get-application-command-permissions
    DISCUSY_API_EMPTY_BODY(
        get_application_command_permissions,
        get,
        discusy::application_commands::guild_application_command_permissions,
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/applications/", state_.application_id, "/guilds/", guild_id.to_snowflake_str(), "/commands/", command_id.to_snowflake_str(), "/permissions"),
        const discusy::snowflake guild_id, const discusy::snowflake command_id
    )

    // requires a bearer token, not the bot token
    // https://docs.discord.com/developers/interactions/application-commands#edit-application-command-permissions
    DISCUSY_API_JSON_BODY(
        edit_application_command_permissions,
        put,
        discusy::application_commands::guild_application_command_permissions,
        handle_json_body(perms),
        ulp::str::concat_strings(discusy::urls::REST_BASE, "/applications/", state_.application_id, "/guilds/", guild_id.to_snowflake_str(), "/commands/", command_id.to_snowflake_str(), "/permissions"),
        const discusy::snowflake guild_id, const discusy::snowflake command_id, const discusy::api::application_commands::edit_application_command_permissions& perms
    )
};

}

#undef DISCUSY_API_EMPTY_BODY
#pragma pop_macro("DISCUSY_API_EMPTY_BODY")
#undef DISCUSY_API_JSON_BODY
#pragma pop_macro("DISCUSY_API_JSON_BODY")
#undef DISCUSY_API_EMPTY_BODY_REASON
#pragma pop_macro("DISCUSY_API_EMPTY_BODY_REASON")
#undef DISCUSY_API_JSON_BODY_REASON
#pragma pop_macro("DISCUSY_API_JSON_BODY_REASON")
#undef DISCUSY_API_MULTIPART_JSON_BODY
#pragma pop_macro("DISCUSY_API_MULTIPART_JSON_BODY")
#undef DISCUSY_API_MULTIPART_JSON_BODY_REASON
#pragma pop_macro("DISCUSY_API_MULTIPART_JSON_BODY_REASON")

template <>
struct glz::meta<discusy::http_api::raw_data> {
    using T = discusy::http_api::raw_data;

    static constexpr auto read_fn = [](T& obj, std::string&& input) {
        obj.data = std::move(input);
    };

    static constexpr auto write_fn = [](const T& obj) -> const std::string& {
        return obj.data;
    };

    static constexpr auto value = glz::custom<read_fn, write_fn>;
};
