#include "../openssl.h"
#include "../../../node_crypto_factory.h"
#include "node_crypto.h"

namespace node {
namespace crypto {

const std::string id_ = CryptoFactory::Register("openssl_1_1_0f",
    []() -> Crypto* { return new OpenSSL(); });

const std::string OpenSSL::Version() {
  return "1.1.0f";
}

bool OpenSSL::HasSNI() {
#ifdef SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
    return true;
#else
    return false;
#endif
}

bool OpenSSL::HasNPN() {
#ifndef OPENSSL_NO_NEXTPROTONEG
    return true;
#else
    return false;
#endif
}

bool OpenSSL::HasALPN() {
#ifndef TLSEXT_TYPE_application_layer_protocol_negotiation
    return true;
#else
    return false;
#endif
}

bool OpenSSL::HasOCSP() {
#if !defined(OPENSSL_NO_TLSEXT) && defined(SSL_CTX_set_tlsext_status_cb)
    return true;
#else
    return false;
#endif
}

OpenSSL::OpenSSL() {
}

OpenSSL::~OpenSSL() {
}

void OpenSSL::UseExtraCaCerts(const std::string& file) {
  crypto::UseExtraCaCerts(file);
}

v8::EntropySource OpenSSL::GetEntropySource() {
  return crypto::EntropySource;
}

}  //  namespace crypto
}  //  namespace node
