#include "node.h"
#include "node_internals.h"

using v8::Array;
using v8::Context;
using v8::Local;
using v8::Object;
using v8::Value;

namespace node {
namespace extras {

static void Init(
    Local<Object> target, Local<Value> unused, Local<Context> context) {
  Local<Object> ex = context->GetExtrasBindingObject();
  Local<Array> keys = ex->GetPropertyNames(context)
    .ToLocalChecked().As<Array>();

  for (uint32_t i = 0; i < keys->Length(); i++) {
    Local<Value> key = keys->Get(context, i).ToLocalChecked();
    CHECK(target->Set(context, key, ex->Get(context, key)
          .ToLocalChecked()).FromMaybe(false));
  }
}

}  // namespace extras
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(extras, node::extras::Init);
