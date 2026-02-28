#ifndef SRC_CRYPTO_CRYPTO_ML_DSA_H_
#define SRC_CRYPTO_CRYPTO_ML_DSA_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "crypto/crypto_keys.h"
#include "env.h"
#include "v8.h"

namespace node {
namespace crypto {
#if OPENSSL_WITH_PQC
bool ExportJwkMlDsaKey(Environment* env,
                       const KeyObjectData& key,
                       v8::Local<v8::Object> target);
#endif
}  // namespace crypto
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_CRYPTO_CRYPTO_ML_DSA_H_
