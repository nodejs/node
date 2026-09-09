#ifndef SRC_CRYPTO_CRYPTO_PQC_H_
#define SRC_CRYPTO_CRYPTO_PQC_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "v8.h"

namespace node {
namespace crypto {
void GetPqcKeyTypes(const v8::FunctionCallbackInfo<v8::Value>& args);
}  // namespace crypto
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_CRYPTO_CRYPTO_PQC_H_
