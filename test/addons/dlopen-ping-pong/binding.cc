#include <node.h>
#include <v8.h>

#ifndef _WIN32

#include <dlfcn.h>

extern "C"
__attribute__((visibility("default")))
const char* dlopen_pong(void) {
  return "pong";
}

namespace {

using v8::Exception;
using v8::FunctionCallbackInfo;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;

typedef const char* (*ping)(void);

static ping ping_func;

void ThrowError(Isolate* isolate, const char* message) {
  isolate->ThrowException(
      Exception::Error(String::NewFromUtf8(isolate, message).ToLocalChecked()));
}

void LoadLibrary(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  const String::Utf8Value filename(isolate, args[0]);
  void* handle = dlopen(*filename, RTLD_LAZY);
  if (handle == nullptr) {
    ThrowError(isolate, dlerror() != nullptr ? dlerror() : "dlopen failed");
    return;
  }
  ping_func = reinterpret_cast<ping>(dlsym(handle, "dlopen_ping"));
  if (ping_func == nullptr) {
    dlclose(handle);
    ThrowError(isolate,
               dlerror() != nullptr ? dlerror() : "dlsym dlopen_ping failed");
    return;
  }
}

void Ping(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  if (ping_func == nullptr) {
    ThrowError(isolate, "ping library was not loaded");
    return;
  }
  const char* answer = ping_func();
  if (answer == nullptr) {
    ThrowError(isolate, "dlopen_pong could not be resolved (RTLD_GLOBAL)");
    return;
  }
  args.GetReturnValue().Set(
      String::NewFromUtf8(isolate, answer).ToLocalChecked());
}

void init(Local<Object> exports) {
  NODE_SET_METHOD(exports, "load", LoadLibrary);
  NODE_SET_METHOD(exports, "ping", Ping);
}

NODE_MODULE(NODE_GYP_MODULE_NAME, init)

}  // anonymous namespace

#endif  // _WIN32
