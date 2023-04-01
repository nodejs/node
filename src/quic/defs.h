#pragma once

#include <aliased_struct.h>
#include <env.h>
#include <node_errors.h>
#include <uv.h>
#include <v8.h>

namespace node {
namespace quic {

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

#define STAT_INCREMENT(Type, name) IncrementStat<Type, &Type::name>(&stats_);
#define STAT_INCREMENT_N(Type, name, amt)                                      \
  IncrementStat<Type, &Type::name>(&stats_, amt);
#define STAT_RECORD_TIMESTAMP(Type, name)                                      \
  RecordTimestampStat<Type, &Type::name>(&stats_);
#define STAT_SET(Type, name, val) SetStat<Type, &Type::name>(&stats_, val);
#define STAT_GET(Type, name) GetStat<Type, &Type::name>(&stats_);

}  // namespace quic
}  // namespace node
