#ifndef SRC_NODE_CONSTANTS_H_
#define SRC_NODE_CONSTANTS_H_

#include "node.h"
#include "v8.h"

namespace node {
void DefineConstants(v8::Handle<v8::Object> target);
}  // namespace node

#endif  // SRC_NODE_CONSTANTS_H_
