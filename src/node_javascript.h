#include <v8.h>

namespace node {

void DefineJavaScript(v8::Handle<v8::Object> target);
v8::Handle<v8::String> MainSource();

}  // namespace node
