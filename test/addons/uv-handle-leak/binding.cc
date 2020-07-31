#include <node.h>
#include <v8.h>
#include <uv.h>

using v8::Context;
using v8::FunctionCallbackInfo;
using v8::Isolate;
using v8::Local;
using v8::Value;

// Give these things names in the public namespace so that we can see
// them show up in symbol dumps.
void CloseCallback(uv_handle_t* handle) {}

class ExampleOwnerClass {
 public:
  virtual ~ExampleOwnerClass();
};

// Do not inline this into the class, because that may remove the virtual
// table when LTO is used, and with it the symbol for which we grep the process
// output in test/abort/test-addon-uv-handle-leak.
// When the destructor is not inlined, the compiler will have to assume that it,
// and the vtable, is part of what this compilation unit exports, and keep them.
ExampleOwnerClass::~ExampleOwnerClass() {}

ExampleOwnerClass example_instance;

void LeakHandle(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  uv_loop_t* loop = node::GetCurrentEventLoop(isolate);
  assert(loop != nullptr);

  uv_timer_t* leaked_timer = new uv_timer_t;
  leaked_timer->close_cb = CloseCallback;

  if (args[0]->IsNumber()) {
    leaked_timer->data =
        reinterpret_cast<void*>(args[0]->IntegerValue(context).FromJust());
  } else {
    leaked_timer->data = &example_instance;
  }

  uv_timer_init(loop, leaked_timer);
  uv_timer_start(leaked_timer, [](uv_timer_t*){}, 1000, 1000);
  uv_unref(reinterpret_cast<uv_handle_t*>(leaked_timer));
}

// This module gets loaded multiple times in some tests so it must support that.
NODE_MODULE_INIT(/*exports, module, context*/) {
  NODE_SET_METHOD(exports, "leakHandle", LeakHandle);
}
