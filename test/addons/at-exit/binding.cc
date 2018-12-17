#include <assert.h>
#include <stdlib.h>
#include <node.h>
#include <v8.h>

using node::AtExit;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Object;

#if defined(_MSC_VER)
#pragma section(".CRT$XPU", read)
#define NODE_C_DTOR(fn)                                               \
  NODE_CTOR_PREFIX void __cdecl fn(void);                             \
  __declspec(dllexport, allocate(".CRT$XPU"))                         \
      void (__cdecl*fn ## _)(void) = fn;                              \
  NODE_CTOR_PREFIX void __cdecl fn(void)
#else
#define NODE_C_DTOR(fn)                                               \
  NODE_CTOR_PREFIX void fn(void) __attribute__((destructor));         \
  NODE_CTOR_PREFIX void fn(void)
#endif

static char cookie[] = "yum yum";
static int at_exit_cb1_called = 0;
static int at_exit_cb2_called = 0;

static void at_exit_cb1(void* arg) {
  Isolate* isolate = static_cast<Isolate*>(arg);
  HandleScope handle_scope(isolate);
  Local<Object> obj = Object::New(isolate);
  assert(!obj.IsEmpty());  // Assert VM is still alive.
  assert(obj->IsObject());
  at_exit_cb1_called++;
}

static void at_exit_cb2(void* arg) {
  assert(arg == static_cast<void*>(cookie));
  at_exit_cb2_called++;
}

NODE_C_DTOR(sanity_check) {
  assert(at_exit_cb1_called == 1);
  assert(at_exit_cb2_called == 2);
}

void init(Local<Object> exports) {
  AtExit(at_exit_cb1, exports->GetIsolate());
  AtExit(at_exit_cb2, cookie);
  AtExit(at_exit_cb2, cookie);
}

NODE_MODULE(NODE_GYP_MODULE_NAME, init)
