#ifndef SRC_CRYPTO_CRYPTO_SPKAC_H_
#define SRC_CRYPTO_CRYPTO_SPKAC_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "env.h"
#include "v8.h"

#include <openssl/evp.h>

namespace node {
namespace crypto {
namespace SPKAC {
void Initialize(Environment* env, v8::Local<v8::Object>);
void RegisterExternalReferences(ExternalReferenceRegistry* registry);
}  // namespace SPKAC
}  // namespace crypto
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_CRYPTO_CRYPTO_SPKAC_H_
