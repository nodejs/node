#ifndef node_constants_h
#define node_constants_h

#include <v8.h>

namespace node {

void DefineConstants(v8::Handle<v8::Object> target);

} // namespace node
#endif // node_constants_h
