#ifndef NODE_BUFFER
#define NODE_BUFFER

#include <v8.h>

namespace node {

void InitBuffer(v8::Handle<v8::Object> target);

}

#endif  // NODE_BUFFER
