
#include "node_messages.h"
#include "node.h"
#include "v8.h"
#include "util.h"
#include "env.h"
#include "env-inl.h"

#define TYPE_ERROR(msg) env->ThrowTypeError(msg)

// Used to be a macro, hence the uppercase name.
#define DEFINE_MSG_CONSTANT(target, constant)                                 \
  do {                                                                        \
    v8::Isolate* isolate = target->GetIsolate();                              \
    v8::Local<v8::String> constant_name =                                     \
        v8::String::NewFromUtf8(isolate, #constant);                          \
    v8::Local<v8::Number> constant_value =                                    \
        v8::Number::New(isolate, static_cast<double>(MSG_ ## constant));      \
    v8::PropertyAttribute constant_attributes =                               \
        static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontDelete);    \
    (target)->ForceSet(constant_name, constant_value, constant_attributes);   \
  }                                                                           \
  while (0)

namespace node {

using v8::Local;
using v8::Array;
using v8::Object;
using v8::String;
using v8::Value;
using v8::FunctionCallbackInfo;
using v8::PropertyAttribute;

void DefineMessages(Environment* env, Local<Object> target) {
  // Set the Message ID Constants
  v8::Isolate* isolate = target->GetIsolate();
  Local<Array> keys = Array::New(isolate);
  Local<Array> messages = Array::New(isolate);

#define NODE_MSG_CONSTANT(id, msg)                                            \
  DEFINE_MSG_CONSTANT(target, id);                                            \
  keys->Set(MSG_ ## id, OneByteString(isolate, #id));                         \
  messages->Set(MSG_ ## id, v8::String::NewFromUtf8(isolate, msg));
  NODE_MESSAGES(NODE_MSG_CONSTANT);
#undef NODE_MSG_CONSTANT

  PropertyAttribute constant_attributes =
      static_cast<PropertyAttribute>(v8::ReadOnly | v8::DontDelete);
  (target)->ForceSet(String::NewFromUtf8(isolate, "keys"),
                    keys, constant_attributes);
  (target)->ForceSet(String::NewFromUtf8(isolate, "messages"),
                    messages, constant_attributes);
}
}  // namespace node
