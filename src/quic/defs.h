#pragma once

#include <aliased_struct.h>
#include <env.h>
#include <node_errors.h>
#include <uv.h>
#include <v8.h>

namespace node {
namespace quic {

#define NGTCP2_SUCCESS 0
#define NGTCP2_ERR(V) (V != NGTCP2_SUCCESS)
#define NGTCP2_OK(V) (V == NGTCP2_SUCCESS)

template <typename Opt, std::string Opt::*member>
bool SetOption(Environment* env,
               Opt* options,
               const v8::Local<v8::Object>& object,
               const v8::Local<v8::String>& name) {
  v8::Local<v8::Value> value;
  if (!object->Get(env->context(), name).ToLocal(&value)) return false;
  if (!value->IsUndefined()) {
    Utf8Value utf8(env->isolate(), value);
    options->*member = *utf8;
  }
  return true;
}

template <typename Opt, bool Opt::*member>
bool SetOption(Environment* env,
               Opt* options,
               const v8::Local<v8::Object>& object,
               const v8::Local<v8::String>& name) {
  v8::Local<v8::Value> value;
  if (!object->Get(env->context(), name).ToLocal(&value)) return false;
  options->*member = value->BooleanValue(env->isolate());
  return true;
}

template <typename Opt, uint32_t Opt::*member>
bool SetOption(Environment* env,
               Opt* options,
               const v8::Local<v8::Object>& object,
               const v8::Local<v8::String>& name) {
  v8::Local<v8::Value> value;
  if (!object->Get(env->context(), name).ToLocal(&value)) return false;
  if (!value->IsUndefined()) {
    if (!value->IsUint32()) {
      Utf8Value nameStr(env->isolate(), name);
      THROW_ERR_INVALID_ARG_VALUE(
          env, "The %s option must be an uint32", *nameStr);
      return false;
    }
    v8::Local<v8::Uint32> num;
    if (!value->ToUint32(env->context()).ToLocal(&num)) {
      Utf8Value nameStr(env->isolate(), name);
      THROW_ERR_INVALID_ARG_VALUE(
          env, "The %s option must be an uint32", *nameStr);
      return false;
    }
    options->*member = num->Value();
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
    if (!value->IsBigInt() && !value->IsNumber()) {
      Utf8Value nameStr(env->isolate(), name);
      THROW_ERR_INVALID_ARG_VALUE(
          env, "option %s must be a bigint or number", *nameStr);
      return false;
    }
    DCHECK_IMPLIES(!value->IsBigInt(), value->IsNumber());

    uint64_t val = 0;
    if (value->IsBigInt()) {
      bool lossless = true;
      val = value.As<v8::BigInt>()->Uint64Value(&lossless);
      if (!lossless) {
        Utf8Value label(env->isolate(), name);
        THROW_ERR_INVALID_ARG_VALUE(env, "option %s is out of range", *label);
        return false;
      }
    } else {
      double dbl = value.As<v8::Number>()->Value();
      if (dbl < 0) {
        Utf8Value label(env->isolate(), name);
        THROW_ERR_INVALID_ARG_VALUE(env, "option %s is out of range", *label);
        return false;
      }
      val = static_cast<uint64_t>(dbl);
    }
    options->*member = val;
  }
  return true;
}

// Utilities used to update the stats for Endpoint, Session, and Stream
// objects. The stats themselves are maintained in an AliasedStruct within
// each of the relevant classes.

template <typename Stats, uint64_t Stats::*member>
void IncrementStat(Stats* stats, uint64_t amt = 1) {
  stats->*member += amt;
}

template <typename Stats, uint64_t Stats::*member>
void RecordTimestampStat(Stats* stats) {
  stats->*member = uv_hrtime();
}

template <typename Stats, uint64_t Stats::*member>
void SetStat(Stats* stats, uint64_t val) {
  stats->*member = val;
}

template <typename Stats, uint64_t Stats::*member>
uint64_t GetStat(Stats* stats) {
  return stats->*member;
}

#define STAT_INCREMENT(Type, name)                                             \
  IncrementStat<Type, &Type::name>(stats_.Data());
#define STAT_INCREMENT_N(Type, name, amt)                                      \
  IncrementStat<Type, &Type::name>(stats_.Data(), amt);
#define STAT_RECORD_TIMESTAMP(Type, name)                                      \
  RecordTimestampStat<Type, &Type::name>(stats_.Data());
#define STAT_SET(Type, name, val) SetStat<Type, &Type::name>(stats_.Data(), val)
#define STAT_GET(Type, name) GetStat<Type, &Type::name>(stats_.Data())
#define STAT_FIELD(_, name) uint64_t name;
#define STAT_STRUCT(klass, name)                                               \
  struct klass::Stats final {                                                  \
    name##_STATS(STAT_FIELD)                                                   \
  };

#define JS_METHOD(name)                                                        \
  static void name(const v8::FunctionCallbackInfo<v8::Value>& args)

}  // namespace quic
}  // namespace node
