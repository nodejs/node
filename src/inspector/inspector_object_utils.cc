#include "inspector_object_utils.h"
#include "inspector/protocol_helper.h"
#include "util-inl.h"

using v8::Boolean;
using v8::Context;
using v8::EscapableHandleScope;
using v8::HandleScope;
using v8::Int32;
using v8::Isolate;
using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Nothing;
using v8::Number;
using v8::Object;
using v8::String;
using v8::Value;

namespace node {
namespace inspector {
// Get a protocol string property from the object.
Maybe<protocol::String> ObjectGetProtocolString(Local<Context> context,
                                                Local<Object> object,
                                                Local<String> property) {
  HandleScope handle_scope(Isolate::GetCurrent());
  Local<Value> value;
  if (!object->Get(context, property).ToLocal(&value) || !value->IsString()) {
    return Nothing<protocol::String>();
  }
  Local<String> str = value.As<String>();
  return Just(ToProtocolString(Isolate::GetCurrent(), str));
}

// Get a protocol string property from the object.
Maybe<protocol::String> ObjectGetProtocolString(Local<Context> context,
                                                Local<Object> object,
                                                const char* property) {
  HandleScope handle_scope(Isolate::GetCurrent());
  return ObjectGetProtocolString(
      context, object, OneByteString(Isolate::GetCurrent(), property));
}

// Get a protocol double property from the object.
Maybe<double> ObjectGetDouble(Local<Context> context,
                              Local<Object> object,
                              const char* property) {
  HandleScope handle_scope(Isolate::GetCurrent());
  Local<Value> value;
  if (!object->Get(context, OneByteString(Isolate::GetCurrent(), property))
           .ToLocal(&value) ||
      !value->IsNumber()) {
    return Nothing<double>();
  }
  return Just(value.As<Number>()->Value());
}

// Get a protocol int property from the object.
Maybe<int> ObjectGetInt(Local<Context> context,
                        Local<Object> object,
                        const char* property) {
  HandleScope handle_scope(Isolate::GetCurrent());
  Local<Value> value;
  if (!object->Get(context, OneByteString(Isolate::GetCurrent(), property))
           .ToLocal(&value) ||
      !value->IsInt32()) {
    return Nothing<int>();
  }
  return Just(value.As<Int32>()->Value());
}

// Get a protocol bool property from the object.
Maybe<bool> ObjectGetBool(Local<Context> context,
                          Local<Object> object,
                          const char* property) {
  HandleScope handle_scope(Isolate::GetCurrent());
  Local<Value> value;
  if (!object->Get(context, OneByteString(Isolate::GetCurrent(), property))
           .ToLocal(&value) ||
      !value->IsBoolean()) {
    return Nothing<bool>();
  }
  return Just(value.As<Boolean>()->Value());
}

// Get an object property from the object.
MaybeLocal<Object> ObjectGetObject(Local<Context> context,
                                   Local<Object> object,
                                   const char* property) {
  EscapableHandleScope handle_scope(Isolate::GetCurrent());
  Local<Value> value;
  if (!object->Get(context, OneByteString(Isolate::GetCurrent(), property))
           .ToLocal(&value) ||
      !value->IsObject()) {
    return {};
  }
  return handle_scope.Escape(value.As<Object>());
}

}  // namespace inspector
}  // namespace node
