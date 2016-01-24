#ifndef SRC_NODE_JAVASCRIPT_H_
#define SRC_NODE_JAVASCRIPT_H_

#include "v8.h"
#include "env.h"

namespace node {

void DefineJavaScript(Environment* env, v8::Local<v8::Object> target);
v8::ScriptCompiler::Source* MainSource(Environment* env,
                                       v8::Local<v8::String> filename);

}  // namespace node

#endif  // SRC_NODE_JAVASCRIPT_H_
