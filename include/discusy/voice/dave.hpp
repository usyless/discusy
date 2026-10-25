#pragma once

#include <cstring>
#include <memory>
#include <string>

#include <dave/dave_interfaces.h>
#include <dave/logger.h>
#include <mls/crypto.h>

#include "../log.hpp"

namespace discusy {

inline void init_dave_logging() noexcept {
    static const bool initialized = []() noexcept {
#ifdef DISCUSY_LOGGING
        discord::dave::SetLogSink([](discord::dave::LoggingSeverity severity,
                                     const char* file,
                                     int line,
                                     const std::string& message) {
            const char* file_name = file;
            if (!file_name) {
                file_name = "unknown";
            } else if (const char* sep = std::strrchr(file_name, '/')) {
                file_name = sep + 1;
            } else if (const char* sep_win = std::strrchr(file_name, '\\')) {
                file_name = sep_win + 1;
            }
            const char* sev_str = "INFO";
            switch (severity) {
                case discord::dave::LS_VERBOSE: sev_str = "VERBOSE"; break;
                case discord::dave::LS_INFO:    sev_str = "INFO"; break;
                case discord::dave::LS_WARNING: sev_str = "WARN"; break;
                case discord::dave::LS_ERROR:   sev_str = "ERROR"; break;
                case discord::dave::LS_NONE:    return;
                default:                        sev_str = "UNKNOWN"; break;
            }
            log::Logger{}("[libdave] [{}] ({}:{}) {}", sev_str, file_name, line, message);
        });
#else
        discord::dave::SetLogSink([](discord::dave::LoggingSeverity, const char*, int, const std::string&) {});
#endif
        return true;
    }();
    (void)initialized;
}

struct dave {
    std::unique_ptr<discord::dave::mls::ISession> session_;
    std::unique_ptr<discord::dave::IEncryptor> encryptor_;
    std::vector<uint8_t> cached_external_sender_;

    dave(const dave& other) = delete;
    dave& operator=(const dave& other) = delete;
    dave(dave&& other) = delete;
    dave& operator=(dave&& other) = delete;

    dave() {
        init_dave_logging();
        create_session();

        encryptor_ = discord::dave::CreateEncryptor();
    }

    void create_session() noexcept {
        session_ = discord::dave::mls::CreateSession(
            nullptr, 
            "",
            []([[maybe_unused]] const std::string& source, [[maybe_unused]] const std::string& reason) {
                #ifdef DISCUSY_LOGGING
                log::Logger{}("[dave session] MLS error callback: source {}, reason {}", source, reason);
                #endif
            }
        );
        if (!cached_external_sender_.empty()) {
            session_->SetExternalSender(cached_external_sender_);
        }
    }

    // group_id is the DAVE/MLS group ID, which Discord derives from the *voice channel* ID.
    // NOLINTNEXTLINE(readability-make-member-function-const)
    void init_session(discord::dave::ProtocolVersion protocol_version, std::uint64_t group_id, const std::string& user_id_str) {
        mlspp::CipherSuite suite{mlspp::CipherSuite::ID::P256_AES128GCM_SHA256_P256};
        mlspp::SignaturePrivateKey sig_priv = mlspp::SignaturePrivateKey::generate(suite);
        auto transient_key = std::make_shared<mlspp::SignaturePrivateKey>(std::move(sig_priv));
        
        session_->Init(
            protocol_version,
            group_id,
            user_id_str,
            transient_key
        );
    }

    constexpr ~dave() noexcept = default;
};

}