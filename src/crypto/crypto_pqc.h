#ifndef SRC_CRYPTO_CRYPTO_PQC_H_
#define SRC_CRYPTO_CRYPTO_PQC_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "crypto/crypto_keys.h"
#include "env.h"
#include "v8.h"

namespace node {
namespace crypto {
#if OPENSSL_WITH_PQC
bool ExportJwkPqcKey(Environment* env,
                     const KeyObjectData& key,
                     v8::Local<v8::Object> target);

KeyObjectData ImportJWKPqcKey(Environment* env, v8::Local<v8::Object> jwk);

// Returns true for PQC algorithms that support raw private key export/import.
bool IsPqcRawPrivateKeyId(int id);
// Returns true for PQC algorithms that carry the private key as a seed
// (ML-DSA, ML-KEM). Returns false for algorithms that use the expanded
// private key (SLH-DSA), or for non-PQC ids.
bool IsPqcSeedKeyId(int id);
#endif
}  // namespace crypto
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_CRYPTO_CRYPTO_PQC_H_
