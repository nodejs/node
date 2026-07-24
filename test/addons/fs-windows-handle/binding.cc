#include <node.h>
#include <v8.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

using v8::BigInt;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;

// Creates an anonymous pipe and returns its raw Win32 read/write HANDLE values
// as JS bigints. These are NOT CRT file descriptors, so passing them as the
// `windowsHandle` stream option exercises the HANDLE -> fd conversion path on
// Windows. Returns undefined on other platforms.
void CreatePipeHandles(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
#ifdef _WIN32
  Local<Context> context = isolate->GetCurrentContext();

  HANDLE read_handle = nullptr;
  HANDLE write_handle = nullptr;
  if (!CreatePipe(&read_handle, &write_handle, nullptr, 0)) {
    isolate->ThrowException(v8::Exception::Error(
        String::NewFromUtf8(isolate, "CreatePipe failed").ToLocalChecked()));
    return;
  }

  Local<Object> result = Object::New(isolate);
  result
      ->Set(context,
            String::NewFromUtf8(isolate, "readHandle").ToLocalChecked(),
            BigInt::New(isolate,
                        static_cast<int64_t>(
                            reinterpret_cast<intptr_t>(read_handle))))
      .Check();
  result
      ->Set(context,
            String::NewFromUtf8(isolate, "writeHandle").ToLocalChecked(),
            BigInt::New(isolate,
                        static_cast<int64_t>(
                            reinterpret_cast<intptr_t>(write_handle))))
      .Check();
  args.GetReturnValue().Set(result);
#else
  args.GetReturnValue().SetUndefined();
#endif
}

}  // anonymous namespace

extern "C" NODE_MODULE_EXPORT void
NODE_MODULE_INITIALIZER(Local<Object> exports,
                        Local<Value> module,
                        Local<Context> context) {
  NODE_SET_METHOD(exports, "createPipeHandles", CreatePipeHandles);
}
