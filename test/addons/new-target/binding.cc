#include <node.h>
#include <v8.h>

using namespace v8;
namespace {

  inline void NewClass(const FunctionCallbackInfo<Value>&) {}
  
  inline void Initialize(Local<Object> binding) {
    auto isolate = binding->GetIsolate();
    binding->Set(String::NewFromUtf8(isolate, "Class"),
                 FunctionTemplate::New(isolate, NewClass)->GetFunction());
  }
  
  NODE_MODULE(NODE_GYP_MODULE_NAME, Initialize)

}  // anonymous namespac
