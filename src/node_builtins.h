#ifndef SRC_NODE_BUILTINS_H_
#define SRC_NODE_BUILTINS_H_

#include "v8.h"
#include "env.h"

namespace node {

void DefineBuiltins(Environment* env,
                    v8::Handle<v8::Object> target,
                    node_module* builtins);

}  // namespace node

#endif  // SRC_NODE_BUILTINS_H_
