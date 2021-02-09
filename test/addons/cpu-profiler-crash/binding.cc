#include "node.h"
#include "v8.h"
#include "v8-profiler.h"

static void StartCpuProfiler(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::CpuProfiler* profiler = v8::CpuProfiler::New(args.GetIsolate());
  v8::Local<v8::String> profile_title = v8::String::NewFromUtf8(
    args.GetIsolate(),
    "testing",
    v8::NewStringType::kInternalized).ToLocalChecked();
  profiler->StartProfiling(profile_title, true);
}

NODE_MODULE_INIT(/* exports, module, context */) {
  NODE_SET_METHOD(exports, "startCpuProfiler", StartCpuProfiler);
}
