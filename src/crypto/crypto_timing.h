#ifndef SRC_CRYPTO_CRYPTO_TIMING_H_
#define SRC_CRYPTO_CRYPTO_TIMING_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "env.h"
#include "v8.h"

namespace node {
namespace crypto {
namespace Timing {
void Initialize(Environment* env, v8::Local<v8::Object> target);

}  // namespace Timing
}  // namespace crypto
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_CRYPTO_CRYPTO_TIMING_H_
