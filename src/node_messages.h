#ifndef SRC_NODE_MESSAGES_H_
#define SRC_NODE_MESSAGES_H_

#include "node.h"
#include "env.h"
#include "v8.h"

namespace node {

void DefineMessages(Environment* env, v8::Local<v8::Object> target);

}  // namespace node

#endif  // SRC_NODE_MESSAGES_H_
