#ifndef SRC_NODE_JAVASCRIPT_H_
#define SRC_NODE_JAVASCRIPT_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "v8.h"
#include "env.h"

namespace node {

void DefineJavaScript(Environment* env, v8::Local<v8::Object> target);
v8::Local<v8::String> MainSource(Environment* env);

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_JAVASCRIPT_H_
