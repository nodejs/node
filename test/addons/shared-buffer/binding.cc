#include <node.h>
#include <v8.h>
#include <uv.h>

using namespace v8;

extern "C" {
  void init(Handle<Object> target);
}


#define BUFSIZE 1024
static uint8_t buf[BUFSIZE];
static uv_mutex_t lock;


Handle<Value> Get(const Arguments& args) {
  HandleScope scope;

  int index = args[0]->Uint32Value();

  if (index < 0 || BUFSIZE <= index) {
    return ThrowException(Exception::Error(String::New("out of bounds")));
  }

  return scope.Close(Integer::New(buf[index]));
}


Handle<Value> Set(const Arguments& args) {
  uv_mutex_lock(&lock);
  HandleScope scope;

  int index = args[0]->Uint32Value();

  if (index < 0 || BUFSIZE <= index) {
    return ThrowException(Exception::Error(String::New("out of bounds")));
  }

  buf[index] = args[1]->Uint32Value();

  Local<Integer> val = Integer::New(buf[index]);

  uv_mutex_unlock(&lock);

  return scope.Close(val);
}


void init(Handle<Object> target) {
  NODE_SET_METHOD(target, "get", Get);
  NODE_SET_METHOD(target, "set", Set);
  target->Set(String::New("length"), Integer::New(BUFSIZE));
  uv_mutex_init(&lock);
}
