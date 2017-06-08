#include "../openssl.h"
#include "../../../node_crypto_factory.h"
#include "node_crypto.h"
#include <string.h>

namespace node {
namespace crypto {

constexpr char version_[] = "1.0.2e";
constexpr char typeversion_[] = "openssl_1_0_2e";

const std::string OpenSSL::Version() {
  return version_;
}

void Crypto::RegisterCrypto() {
  CryptoFactory::Register(typeversion_, []() -> Crypto* {
     return new OpenSSL();
  });
}

void Crypto::UnregisterCrypto() {
  CryptoFactory::Unregister(typeversion_);
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
