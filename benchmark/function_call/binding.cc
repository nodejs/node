#include <v8.h>
#include <node.h>
#include <time.h>

using namespace v8;

static int c = 0;

static Handle<Value> Hello(const Arguments& args) {
  HandleScope scope;
  //time_t tv = time(NULL);
  return scope.Close(Integer::New(c++));
}

extern "C" void init (Handle<Object> target) {
  HandleScope scope;
  //target->Set(String::New("hello"), String::New("World"));
  NODE_SET_METHOD(target, "hello", Hello);
}
