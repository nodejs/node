#include <v8.h>
#include "node.h"
#include "node_natives.h"
#include "node_string.h"
#include <string.h>
#include <strings.h>

using namespace v8;

namespace node {

Handle<String> MainSource() {
  return BUILTIN_ASCII_ARRAY(node_native, sizeof(node_native)-1);
}

void DefineJavaScript(v8::Handle<v8::Object> target) {
  HandleScope scope;

  for (int i = 0; natives[i].name; i++) {
    if (natives[i].source != node_native) {
      Local<String> name = String::New(natives[i].name);
      Handle<String> source = BUILTIN_ASCII_ARRAY(natives[i].source, natives[i].source_len);
      target->Set(name, source);
    }
  }
}

}  // namespace node
