
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
using v8::Object;
using v8::String;
using v8::Value;
using v8::FunctionCallbackInfo;

static void NodeMsg(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Local<Object> message = Object::New(env->isolate());

  if (args.Length() < 1)
    return TYPE_ERROR("message key is required");
  if (!args[0]->IsInt32())
    return TYPE_ERROR("message key must be an integer");
  int key = static_cast<int>(args[0]->Int32Value());

  const char * id = node_message_id(key);
  const char * msg = node_message_str(key);

  message->Set(
    OneByteString(env->isolate(), "id"),
    OneByteString(env->isolate(), id));

  message->Set(
    OneByteString(env->isolate(), "msg"),
    String::NewFromUtf8(env->isolate(), msg));

  args.GetReturnValue().Set(message);
}

void DefineMessages(Environment* env, Local<Object> target) {
  // Set the Message ID Constants
#define NODE_MSG_CONSTANT(id, _) DEFINE_MSG_CONSTANT(target, id);
  NODE_MESSAGES(NODE_MSG_CONSTANT);
#undef NODE_MSG_CONSTANT

  env->SetMethod(target, "msg", NodeMsg);
}
}  // namespace node
