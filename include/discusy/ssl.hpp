#pragma once

#if OPENSSL_VERSION_NUMBER < 0x30000000L
#include <vector>
#endif
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#ifdef WIN32
#include <windows.h>
#include <wincrypt.h>
#endif

namespace discusy::ssl {

// this is probably unnecessary now considering the shared ssl context
inline void setup_ssl_context([[maybe_unused]] boost::asio::ssl::context& ctx) {
    #ifdef SSL_MODE_RELEASE_BUFFERS
    SSL_CTX_set_mode(ctx.native_handle(), SSL_MODE_RELEASE_BUFFERS);
    #endif
    #ifdef WIN32
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    SSL_CTX_load_verify_store(ctx.native_handle(), "org.openssl.winstore:");
#else
    static std::vector<std::vector<unsigned char>> global_cert_bytes;
    static std::once_flag init_flag;

    std::call_once(init_flag, []() {
        HCERTSTORE hStore = CertOpenSystemStoreW(0, L"ROOT");
        if (!hStore) return;

        PCCERT_CONTEXT pContext = NULL;
        while ((pContext = CertEnumCertificatesInStore(hStore, pContext)) != NULL) {
            global_cert_bytes.emplace_back(
                pContext->pbCertEncoded, 
                pContext->pbCertEncoded + pContext->cbCertEncoded
            );
        }
        CertCloseStore(hStore, 0);
    });

    X509_STORE* store = SSL_CTX_get_cert_store(ctx.native_handle());
    if (!store) return;

    for (const auto& cert_data : global_cert_bytes) {
        const unsigned char* p = cert_data.data();
        X509* x509 = d2i_X509(NULL, &p, static_cast<long>(cert_data.size()));
        if (x509) {
            X509_STORE_add_cert(store, x509);
            X509_free(x509); 
        }
    }
#endif
    #endif
}

}