#ifndef SRC_NODE_JAVASCRIPT_H_
#define SRC_NODE_JAVASCRIPT_H_

#include "v8.h"
#include "env.h"

namespace node {

void DefineJavaScript(Environment* env, v8::Local<v8::Object> target);
v8::Local<v8::String> MainSource(Environment* env);

}  // namespace node

#endif  // SRC_NODE_JAVASCRIPT_H_
