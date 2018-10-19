#ifndef SRC_JS_NATIVE_API_V8_INTERNALS_H_
#define SRC_JS_NATIVE_API_V8_INTERNALS_H_

#include "node_version.h"
#include "env.h"
#include "node_internals.h"

#define NAPI_ARRAYSIZE(array) \
  node::arraysize((array))

#define NAPI_FIXED_ONE_BYTE_STRING(isolate, string) \
  node::FIXED_ONE_BYTE_STRING((isolate), (string))

#define NAPI_PRIVATE_KEY(context, suffix) \
  (node::Environment::GetCurrent((context))->napi_ ## suffix())

namespace v8impl {

template <typename T>
using Persistent = node::Persistent<T>;

using PersistentToLocal = node::PersistentToLocal;

}  // end of namespace v8impl

#endif  // SRC_JS_NATIVE_API_V8_INTERNALS_H_
