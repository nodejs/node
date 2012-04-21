#undef NDEBUG
#include <assert.h>
#include <stdlib.h>
#include <node.h>
#include <v8.h>

using node::AtExit;
using v8::Handle;
using v8::HandleScope;
using v8::Local;
using v8::Object;

static char cookie[] = "yum yum";
static int at_exit_cb1_called = 0;
static int at_exit_cb2_called = 0;

static void at_exit_cb1(void* arg) {
  HandleScope scope;
  assert(arg == 0);
  Local<Object> obj = Object::New();
  assert(!obj.IsEmpty()); // assert VM is still alive
  assert(obj->IsObject());
  at_exit_cb1_called++;
}

static void at_exit_cb2(void* arg) {
  assert(arg == static_cast<void*>(cookie));
  at_exit_cb2_called++;
}

static void sanity_check(void) {
  assert(at_exit_cb1_called == 1);
  assert(at_exit_cb2_called == 2);
}

void init(Handle<Object> target) {
  AtExit(at_exit_cb1);
  AtExit(at_exit_cb2, cookie);
  AtExit(at_exit_cb2, cookie);
  atexit(sanity_check);
}

NODE_MODULE(binding, init);
