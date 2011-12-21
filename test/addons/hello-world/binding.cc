#include <node.h>
#include <v8.h>

using namespace v8;

extern "C" {
  void init(Handle<Object> target);
}

Handle<Value> Method(const Arguments& args) {
  HandleScope scope;
  return scope.Close(String::New("world"));
}

void init(Handle<Object> target) {
  NODE_SET_METHOD(target, "hello", Method);
}
