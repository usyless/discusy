#pragma once

#include <memory>
#include <string>

#include <dave/dave_interfaces.h>
#include <mls/crypto.h>

#include "../log.hpp"

namespace discusy {

struct dave {
    std::unique_ptr<discord::dave::mls::ISession> session_;
    std::unique_ptr<discord::dave::IEncryptor> encryptor_;
    std::vector<uint8_t> cached_external_sender_;

    dave(const dave& other) = delete;
    dave& operator=(const dave& other) = delete;
    dave(dave&& other) = delete;
    dave& operator=(dave&& other) = delete;

    dave() {
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