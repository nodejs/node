#include <v8.h>
#include "node.h"
#include "node_natives.h"
#include <string.h>
#include <strings.h>

using namespace v8;

namespace node {

const char* MainSource() {
  return node_native;
}

void DefineJavaScript(v8::Handle<v8::Object> target) {
  HandleScope scope;

  for (int i = 0; natives[i].name; i++) {
    if (natives[i].source != node_native) {
      Local<String> name = String::New(natives[i].name);
      // TODO: Use ExternalAsciiStringResource for source
      // Might need to do some assertions in js2c about chars > 128
      Local<String> source = String::New(natives[i].source);
      target->Set(name, source);
    }
  }
}



}  // namespace node
