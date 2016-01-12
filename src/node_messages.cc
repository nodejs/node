
#include "node_messages.h"
#include "node.h"
#include "v8.h"
#include "util.h"
#include "env.h"
#include "env-inl.h"

namespace node {

using v8::Isolate;
using v8::Local;
using v8::Array;
using v8::Object;
using v8::String;
using v8::Number;
using v8::Value;
using v8::FunctionCallbackInfo;
using v8::PropertyAttribute;
using v8::ReadOnly;
using v8::DontDelete;

#define DEFINE_MSG_CONSTANT(target, constant)                                 \
  do {                                                                        \
    Isolate* isolate = target->GetIsolate();                                  \
    Local<String> constant_name = OneByteString(isolate, #constant);          \
    Local<Number> constant_value =                                            \
        Number::New(isolate, static_cast<double>(MSG_ ## constant));          \
    PropertyAttribute constant_attributes =                                   \
        static_cast<PropertyAttribute>(ReadOnly | DontDelete);                \
    (target)->ForceSet(constant_name, constant_value, constant_attributes);   \
  }                                                                           \
  while (0)

void DefineMessages(Environment* env, Local<Object> target) {
  // Set the Message ID Constants
  Isolate* isolate = target->GetIsolate();
  Local<Array> keys = Array::New(isolate, MSG_UNKNOWN);
  Local<Array> messages = Array::New(isolate, MSG_UNKNOWN);

#define NODE_MSG_CONSTANT(id, msg)                                            \
  DEFINE_MSG_CONSTANT(target, id);                                            \
  keys->Set(MSG_ ## id, OneByteString(isolate, #id));                         \
  messages->Set(MSG_ ## id, String::NewFromUtf8(isolate, msg));
  NODE_MESSAGES(NODE_MSG_CONSTANT);
#undef NODE_MSG_CONSTANT

  PropertyAttribute constant_attributes =
      static_cast<PropertyAttribute>(ReadOnly | DontDelete);
  (target)->ForceSet(OneByteString(isolate, "keys"),
                    keys, constant_attributes);
  (target)->ForceSet(OneByteString(isolate, "messages"),
                    messages, constant_attributes);
}
}  // namespace node
