#include <node.h>
#include <v8.h>
#include <cstdio>

namespace {
void TestAbortHandler() {
  fputs("CUSTOM_ABORT_HANDLER_RAN\n", stderr);
  fflush(stderr);
}

void InstallAbortHandler(const v8::FunctionCallbackInfo<v8::Value>&) {
  node::SetAbortHandler(TestAbortHandler);
}
}  // namespace

NODE_MODULE_INIT() {
  NODE_SET_METHOD(exports, "installAbortHandler", InstallAbortHandler);
}
