#pragma once

#include <env.h>
#include <node_errors.h>
#include <v8.h>

namespace node {
namespace quic {

template <typename Opt, bool Opt::*member>
bool SetOption(Environment* env,
               Opt* options,
               const v8::Local<v8::Object>& object,
               const v8::Local<v8::String>& name) {
  v8::Local<v8::Value> value;
  if (!object->Get(env->context(), name).ToLocal(&value)) return false;
  if (!value->IsUndefined()) {
    CHECK(value->IsBoolean());
    options->*member = value->IsTrue();
  }
  return true;
}

template <typename Opt, uint64_t Opt::*member>
bool SetOption(Environment* env,
               Opt* options,
               const v8::Local<v8::Object>& object,
               const v8::Local<v8::String>& name) {
  v8::Local<v8::Value> value;
  if (!object->Get(env->context(), name).ToLocal(&value)) return false;

  if (!value->IsUndefined()) {
    CHECK_IMPLIES(!value->IsBigInt(), value->IsNumber());

    uint64_t val = 0;
    if (value->IsBigInt()) {
      bool lossless = true;
      val = value.As<v8::BigInt>()->Uint64Value(&lossless);
      if (!lossless) {
        Utf8Value label(env->isolate(), name);
        THROW_ERR_OUT_OF_RANGE(
            env, ("options." + label.ToString() + " is out of range").c_str());
        return false;
      }
    } else {
      val = static_cast<int64_t>(value.As<v8::Number>()->Value());
    }
    options->*member = val;
  }
  return true;
}

}  // namespace quic
}  // namespace node
