#include "node.h"
#include "v8.h"

#include <assert.h>
#include <vector>

namespace {

void MakeCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
  assert(args[0]->IsObject());
  assert(args[1]->IsFunction() || args[1]->IsString());
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
        node::MakeCallback(isolate, recv, method, argv.size(), argv.data(),
                           node::async_context{0, 0}).ToLocalChecked();
  } else if (args[1]->IsString()) {
    auto method = args[1].As<v8::String>();
    result =
        node::MakeCallback(isolate, recv, method, argv.size(), argv.data(),
                           node::async_context{0, 0}).ToLocalChecked();
  } else {
    assert(0 && "unreachable");
  }
  args.GetReturnValue().Set(result);
}

void Initialize(v8::Local<v8::Object> exports) {
  NODE_SET_METHOD(exports, "makeCallback", MakeCallback);
}

}  // anonymous namespace

NODE_MODULE(NODE_GYP_MODULE_NAME, Initialize)
