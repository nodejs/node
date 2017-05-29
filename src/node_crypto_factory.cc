#include "node_crypto_factory.h"
#include "node_crypto.h"
#include "env.h"
#include "env-inl.h"
#include <string>
#include <iostream>

namespace node {
namespace crypto {

CryptoFactory::CallbackMap CryptoFactory::crypto_libs;

typedef std::pair<const std::string, std::shared_ptr<Crypto>> CryptoPair;

void CryptoFactory::Register(const std::string& version,
                                   CreateCallback cb) {
  if (!crypto_libs.count(version)) {
    std::shared_ptr<Crypto> crypto{cb()};
    crypto_libs.insert(CryptoPair(version, crypto));
  }
}

void CryptoFactory::Unregister(const std::string& version) {
  crypto_libs.erase(version);
}

std::shared_ptr<Crypto> CryptoFactory::Get(const std::string& version,
                                           Environment* env) {
  CallbackMap::iterator it = crypto_libs.find(version);
  if (it != crypto_libs.end()) {
    return it->second;
  }

  if (env == nullptr) {
    fprintf(stderr,
            "node: could not find a crypto implementation for %s",
            version.c_str());
    exit(9);
  }
  env->ThrowError(
      ("Could not find a crypto implementation for " + version).c_str());
  return nullptr;
}

}  //  namespace crypto
}  //  namespace node
