#ifndef node_script_h
#define node_script_h

#include <node.h>
#include <node_object_wrap.h>
#include <v8.h>
#include <ev.h>

namespace node {


void InitEvals(v8::Handle<v8::Object> target);

} // namespace node
#endif //  node_script_h
