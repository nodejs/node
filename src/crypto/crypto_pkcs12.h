#ifndef SRC_CRYPTO_CRYPTO_PKCS12_H_
#define SRC_CRYPTO_CRYPTO_PKCS12_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "env.h"
#include "ncrypto.h"
#include "v8.h"

namespace node {
namespace crypto {

// Parsed contents of a PKCS#12 bundle. Any field may be empty: a bundle can
// legitimately contain only certificates, or only a key.
struct PKCS12Contents {
  ncrypto::EVPKeyPointer key;
  ncrypto::X509Pointer cert;
  ncrypto::StackOfX509 ca;
};

enum class PKCS12ParseError {
  // Input is not a well-formed PKCS#12 structure.
  NOT_RECOGNIZED,
  // Structure is valid but uses an algorithm the active providers do not
  // offer -- typically RC2 or PBE-SHA1 without the legacy provider.
  UNSUPPORTED_ALGORITHM,
  FAILED,
};

using PKCS12ParseResult = ncrypto::Result<PKCS12Contents, PKCS12ParseError>;

// Parses a PKCS#12 bundle from `bio`. `pass` is nullptr for no passphrase, or
// a NUL-terminated string. PKCS12_parse() verifies the MAC when one is present,
// and for nullptr or "" it tries both PKCS#12 password encodings, so those two
// arguments cannot give different results.
//
// Callers apply their own requirements to the result: crypto.parsePKCS12()
// accepts any combination of key and certificates, while the TLS `pfx` option
// requires both a key and a certificate.
PKCS12ParseResult ParsePKCS12Bundle(const ncrypto::BIOPointer& bio,
                                    const char* pass);

class PKCS12Parser final {
 public:
  static void Initialize(Environment* env, v8::Local<v8::Object> target);
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);
};

}  // namespace crypto
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_CRYPTO_CRYPTO_PKCS12_H_
