#include "node.h"
#include "v8.h"

#include <assert.h>
#include <vector>

namespace {

using v8::FunctionCallbackInfo;
using v8::Function;
using v8::Object;
using v8::String;
using v8::Local;
using v8::Value;

void MakeCallback(const FunctionCallbackInfo<Value>& args) {
  assert(args[0]->IsObject());
  assert(args[1]->IsFunction() || args[1]->IsString());
  auto isolate = args.GetIsolate();
  auto recv = args[0].As<Object>();
  std::vector<Local<Value>> argv;
  for (size_t n = 2; n < static_cast<size_t>(args.Length()); n += 1) {
    argv.push_back(args[n]);
  }
  Local<Value> result;
  if (args[1]->IsFunction()) {
    auto method = args[1].As<Function>();
    result =
        node::MakeCallback(isolate, recv, method, argv.size(), argv.data());
  } else if (args[1]->IsString()) {
    auto method = args[1].As<String>();
    result =
        node::MakeCallback(isolate, recv, method, argv.size(), argv.data());
  } else {
    assert(0 && "unreachable");
  }
  args.GetReturnValue().Set(result);
}

void Initialize(Local<Object> exports) {
  NODE_SET_METHOD(exports, "makeCallback", MakeCallback);
}

}  // namespace

NODE_MODULE(NODE_GYP_MODULE_NAME, Initialize)
