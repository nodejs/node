#include "node.h"
#include "v8.h"

#include "../../../src/util.h"

#include <vector>

namespace {

void MakeCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args[0]->IsObject());
  CHECK(args[1]->IsFunction() || args[1]->IsString());
  auto isolate = args.GetIsolate();
  auto recv = args[0].As<v8::Object>();
  std::vector<v8::Local<v8::Value>> argv;
  for (size_t n = 2; n < static_cast<size_t>(args.Length()); n += 1) {
    argv.push_back(args[n]);
  }
  v8::Local<v8::Value> result;
  if (args[1]->IsFunction()) {
    auto method = args[1].As<v8::Function>();
    result =
        node::MakeCallback(isolate, recv, method, argv.size(), argv.data());
  } else if (args[1]->IsString()) {
    auto method = args[1].As<v8::String>();
    result =
        node::MakeCallback(isolate, recv, method, argv.size(), argv.data());
  } else {
    UNREACHABLE();
  }
  args.GetReturnValue().Set(result);
}

void Initialize(v8::Local<v8::Object> target) {
  NODE_SET_METHOD(target, "makeCallback", MakeCallback);
}

}  // namespace

NODE_MODULE(binding, Initialize)
