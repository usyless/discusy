#pragma once

#include <string>
#include <string_view>

#include <usylibpp/strings.hpp>
#include <usylibpp/types.hpp>

#include "intents.hpp"
#include "types.hpp"
#include "gateway_events.hpp"
#include "multipart.hpp"
#include <discusy/version.hpp>

namespace discusy {

struct config {
    std::string token;
    intent intents{discusy::DEFAULT_INTENTS};
    std::uint32_t threads{1};
    opt<std::uint32_t> audio_threads{1};
    opt<send_event::update_presence> initial_presence{};
    opt<std::size_t> max_pool_size{};

    [[nodiscard]] http::headers make_headers(std::string_view content_type, std::string reason = {}) const {
        const std::size_t count = reason.empty() ? 3 : 4;

        std::vector<std::string> keys;
        std::vector<std::string> values;
        keys.reserve(count);
        values.reserve(count);

        keys.emplace_back("authorization");
        values.emplace_back(ulp::str::concat_strings("Bot ", token));

        keys.emplace_back("content-type");
        values.emplace_back(content_type);

        keys.emplace_back("User-Agent");
        values.emplace_back(discusy::version::discusy_header);

        if (!reason.empty()) {
            keys.emplace_back("X-Audit-Log-Reason");
            values.emplace_back(std::move(reason));
        }

        return http::headers{std::sorted_unique, std::move(keys), std::move(values)};
    }

    [[nodiscard]] http::headers bot_headers_json() const {
        return make_headers("application/json");
    }

    // does it need to be escaped? idk
    [[nodiscard]] http::headers bot_headers_json(std::string r) const {
        return make_headers("application/json", std::move(r));
    }

    [[nodiscard]] http::headers bot_headers_multipart() const {
        return make_headers(multipart::BOUNDARY_HEADER);
    }

    [[nodiscard]] http::headers bot_headers_multipart(std::string r) const {
        return make_headers(multipart::BOUNDARY_HEADER, std::move(r));
    }

    [[nodiscard]] http::headers bot_headers_form_urlencoded() const {
        return make_headers("application/x-www-form-urlencoded");
    }

    [[nodiscard]] http::headers bot_headers_form_urlencoded(std::string r) const {
        return make_headers("application/x-www-form-urlencoded", std::move(r));
    }
};

}