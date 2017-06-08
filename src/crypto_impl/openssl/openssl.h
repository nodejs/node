#ifndef SRC_CRYPTO_IMPL_OPENSSL_H_
#define SRC_CRYPTO_IMPL_OPENSSL_H_

#include "../../node_crypto.h"

namespace node {
namespace crypto {

class OpenSSL : public Crypto {
 public:
  OpenSSL();
  ~OpenSSL();
  const std::string Version();
  const std::string Name() { return "openssl"; }
  void UseExtraCaCerts(const std::string& file);
  v8::EntropySource GetEntropySource();
  bool HasSNI();
  bool HasNPN();
  bool HasALPN();
  bool HasOCSP();
};

}  //  namespace crypto
}  //  namespace node

#endif  //  SRC_CRYPTO_IMPL_OPENSSL_H_
