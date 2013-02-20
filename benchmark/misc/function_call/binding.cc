#include <v8.h>
#include <node.h>

using namespace v8;

static int c = 0;

static Handle<Value> Hello(const Arguments& args) {
  HandleScope scope;
  return scope.Close(Integer::New(c++));
}

extern "C" void init (Handle<Object> target) {
  HandleScope scope;
  NODE_SET_METHOD(target, "hello", Hello);
}

NODE_MODULE(binding, init);
