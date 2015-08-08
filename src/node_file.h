#ifndef SRC_NODE_FILE_H_
#define SRC_NODE_FILE_H_

#include "node.h"
#include "v8.h"

namespace node {

void InitFs(v8::Local<v8::Object> target, v8::Local<v8::Context> context);

}  // namespace node

#endif  // SRC_NODE_FILE_H_
