#include <node.h>
#include <v8.h>

#ifndef _WIN32

#include <dlfcn.h>

extern "C" const char* dlopen_pong(void) {
  return "pong";
}

namespace {

using v8::FunctionCallbackInfo;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;

typedef const char* (*ping)(void);

static ping ping_func;

void LoadLibrary(const FunctionCallbackInfo<Value>& args) {
  const String::Utf8Value filename(args[0]);
  void* handle = dlopen(*filename, RTLD_LAZY);
  assert(handle != nullptr);
  ping_func = reinterpret_cast<ping>(dlsym(handle, "dlopen_ping"));
  assert(ping_func != nullptr);
}

void Ping(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  assert(ping_func != nullptr);
  args.GetReturnValue().Set(String::NewFromUtf8(isolate, ping_func()));
}

void init(Local<Object> exports) {
  NODE_SET_METHOD(exports, "load", LoadLibrary);
  NODE_SET_METHOD(exports, "ping", Ping);
}

NODE_MODULE(NODE_GYP_MODULE_NAME, init)

}  // anonymous namespace

#endif  // _WIN32
