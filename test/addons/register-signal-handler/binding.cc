#ifndef _WIN32
#include <node.h>
#include <v8.h>
#include <uv.h>
#include <assert.h>
#include <unistd.h>

using v8::Boolean;
using v8::FunctionCallbackInfo;
using v8::Int32;
using v8::Value;

void Handler(int signo, siginfo_t* siginfo, void* ucontext) {
  char signo_char = signo;
  int written;
  do {
    written = write(1, &signo_char, 1);  // write() is signal-safe.
  } while (written == -1 && errno == EINTR);
  assert(written == 1);
}

void RegisterSignalHandler(const FunctionCallbackInfo<Value>& args) {
  assert(args[0]->IsInt32());
  assert(args[1]->IsBoolean());

  int32_t signo = args[0].As<Int32>()->Value();
  bool reset_handler = args[1].As<Boolean>()->Value();

  node::RegisterSignalHandler(signo, Handler, reset_handler);
}

NODE_MODULE_INIT() {
  NODE_SET_METHOD(exports, "registerSignalHandler", RegisterSignalHandler);
}

#endif  // _WIN32
