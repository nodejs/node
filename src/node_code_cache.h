#ifndef SRC_NODE_CODE_CACHE_H_
#define SRC_NODE_CODE_CACHE_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node_internals.h"

namespace node {

void DefineCodeCache(Environment* env, v8::Local<v8::Object> target);
void DefineCodeCacheHash(Environment* env, v8::Local<v8::Object> target);

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_CODE_CACHE_H_
