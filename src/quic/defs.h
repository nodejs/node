#pragma once

#include <aliased_struct.h>
#include <env.h>
#include <nghttp3/nghttp3.h>
#include <ngtcp2/ngtcp2.h>
#include <node_errors.h>
#include <uv.h>
#include <v8.h>
#include <limits>

namespace node::quic {

#define NGTCP2_SUCCESS 0
#define NGTCP2_ERR(V) (V != NGTCP2_SUCCESS)
#define NGTCP2_OK(V) (V == NGTCP2_SUCCESS)

#define IF_QUIC_DEBUG(env)                                                     \
  if (env->enabled_debug_list()->enabled(DebugCategory::QUIC)) [[unlikely]]

#define DISALLOW_COPY(Name)                                                    \
  Name(const Name&) = delete;                                                  \
  Name& operator=(const Name&) = delete;

#define DISALLOW_MOVE(Name)                                                    \
  Name(Name&&) = delete;                                                       \
  Name& operator=(Name&&) = delete;

#define DISALLOW_COPY_AND_MOVE(Name)                                           \
  DISALLOW_COPY(Name)                                                          \
  DISALLOW_MOVE(Name)

template <typename Opt, std::string Opt::*member>
bool SetOption(Environment* env,
               Opt* options,
               const v8::Local<v8::Object>& object,
               const v8::Local<v8::String>& name) {
  v8::Local<v8::Value> value;
  if (!object->Get(env->context(), name).ToLocal(&value)) return false;
  if (!value->IsUndefined()) {
    Utf8Value utf8(env->isolate(), value);
    options->*member = utf8.ToString();
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
    options->*member = value->BooleanValue(env->isolate());
  }
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
          env, "The %s option must be an uint32", nameStr);
      return false;
    }
    v8::Local<v8::Uint32> num;
    if (!value->ToUint32(env->context()).ToLocal(&num)) {
      Utf8Value nameStr(env->isolate(), name);
      THROW_ERR_INVALID_ARG_VALUE(
          env, "The %s option must be an uint32", nameStr);
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
          env, "option %s must be a bigint or number", nameStr);
      return false;
    }
    DCHECK_IMPLIES(!value->IsBigInt(), value->IsNumber());

    uint64_t val = 0;
    if (value->IsBigInt()) {
      bool lossless = true;
      val = value.As<v8::BigInt>()->Uint64Value(&lossless);
      if (!lossless) {
        Utf8Value label(env->isolate(), name);
        THROW_ERR_INVALID_ARG_VALUE(env, "option %s is out of range", label);
        return false;
      }
    } else {
      double dbl = value.As<v8::Number>()->Value();
      if (dbl < 0) {
        Utf8Value label(env->isolate(), name);
        THROW_ERR_INVALID_ARG_VALUE(env, "option %s is out of range", label);
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

#define JS_METHOD_IMPL(name)                                                   \
  void name(const v8::FunctionCallbackInfo<v8::Value>& args)

#define JS_METHOD(name) static JS_METHOD_IMPL(name)

#define JS_CONSTRUCTOR(name)                                                   \
  inline static bool HasInstance(Environment* env,                             \
                                 v8::Local<v8::Value> value) {                 \
    return GetConstructorTemplate(env)->HasInstance(value);                    \
  }                                                                            \
  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(               \
      Environment* env)

#define JS_CONSTRUCTOR_IMPL(name, template, body)                              \
  v8::Local<v8::FunctionTemplate> name::GetConstructorTemplate(                \
      Environment* env) {                                                      \
    auto& state = BindingData::Get(env);                                       \
    auto tmpl = state.template();                                              \
    if (tmpl.IsEmpty()) {                                                      \
      body state.set_##template(tmpl);                                         \
    }                                                                          \
    return tmpl;                                                               \
  }

#define JS_ILLEGAL_CONSTRUCTOR()                                               \
  tmpl = NewFunctionTemplate(env->isolate(), IllegalConstructor)

#define JS_NEW_CONSTRUCTOR() tmpl = NewFunctionTemplate(env->isolate(), New)

#define JS_INHERIT(name) tmpl->Inherit(name::GetConstructorTemplate(env));

#define JS_CLASS(name)                                                         \
  tmpl->InstanceTemplate()->SetInternalFieldCount(kInternalFieldCount);        \
  tmpl->SetClassName(state.name##_string())

#define JS_CLASS_FIELDS(name, fields)                                          \
  tmpl->InstanceTemplate()->SetInternalFieldCount(fields);                     \
  tmpl->SetClassName(state.name##_string())

#define JS_NEW_INSTANCE_OR_RETURN(env, name, ret)                              \
  v8::Local<v8::Object> name;                                                  \
  if (!GetConstructorTemplate(env)                                             \
           ->InstanceTemplate()                                                \
           ->NewInstance(env->context())                                       \
           .ToLocal(&obj)) {                                                   \
    return ret;                                                                \
  }

#define JS_NEW_INSTANCE(env, name)                                             \
  v8::Local<v8::Object> name;                                                  \
  if (!GetConstructorTemplate(env)                                             \
           ->InstanceTemplate()                                                \
           ->NewInstance(env->context())                                       \
           .ToLocal(&obj)) {                                                   \
    return;                                                                    \
  }

#define JS_BINDING_INIT_BOILERPLATE()                                          \
  static void InitPerIsolate(IsolateData* isolate_data,                        \
                             v8::Local<v8::ObjectTemplate> target);            \
  static void InitPerContext(Realm* realm, v8::Local<v8::Object> target);      \
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry)

#define JS_TRY_ALLOCATE_BACKING(env, name, len)                                \
  auto name = v8::ArrayBuffer::NewBackingStore(                                \
      env->isolate(),                                                          \
      len,                                                                     \
      v8::BackingStoreInitializationMode::kUninitialized,                      \
      v8::BackingStoreOnFailureMode::kReturnNull);                             \
  if (!name) {                                                                 \
    THROW_ERR_MEMORY_ALLOCATION_FAILED(env);                                   \
    return;                                                                    \
  }

#define JS_TRY_ALLOCATE_BACKING_OR_RETURN(env, name, len, ret)                 \
  auto name = v8::ArrayBuffer::NewBackingStore(                                \
      env->isolate(),                                                          \
      len,                                                                     \
      v8::BackingStoreInitializationMode::kUninitialized,                      \
      v8::BackingStoreOnFailureMode::kReturnNull);                             \
  if (!name) {                                                                 \
    THROW_ERR_MEMORY_ALLOCATION_FAILED(env);                                   \
    return ret;                                                                \
  }

#define JS_DEFINE_READONLY_PROPERTY(env, target, name, value)                  \
  target                                                                       \
      ->DefineOwnProperty(                                                     \
          env->context(), name, value, v8::PropertyAttribute::ReadOnly)        \
      .Check();

enum class Side : uint8_t {
  CLIENT,
  SERVER,
};

enum class EndpointLabel : uint8_t {
  LOCAL,
  REMOTE,
};

enum class Direction : uint8_t {
  BIDIRECTIONAL,
  UNIDIRECTIONAL,
};

enum class HeadersKind : uint8_t {
  HINTS,
  INITIAL,
  TRAILING,
};

enum class HeadersFlags : uint8_t {
  NONE,
  TERMINAL,
};

enum class StreamPriority : uint8_t {
  DEFAULT = NGHTTP3_DEFAULT_URGENCY,
  LOW = NGHTTP3_URGENCY_LOW,
  HIGH = NGHTTP3_URGENCY_HIGH,
};

enum class StreamPriorityFlags : uint8_t {
  NONE,
  NON_INCREMENTAL,
};

enum class PathValidationResult : uint8_t {
  SUCCESS = NGTCP2_PATH_VALIDATION_RESULT_SUCCESS,
  FAILURE = NGTCP2_PATH_VALIDATION_RESULT_FAILURE,
  ABORTED = NGTCP2_PATH_VALIDATION_RESULT_ABORTED,
};

enum class DatagramStatus : uint8_t {
  ACKNOWLEDGED,
  LOST,
};

#define CC_ALGOS(V)                                                            \
  V(RENO, reno)                                                                \
  V(CUBIC, cubic)                                                              \
  V(BBR, bbr)

#define V(name, _) static constexpr auto CC_ALGO_##name = NGTCP2_CC_ALGO_##name;
CC_ALGOS(V)
#undef V

using error_code = uint64_t;
using stream_id = int64_t;
using datagram_id = uint64_t;

constexpr size_t kDefaultMaxPacketLength = NGTCP2_MAX_UDP_PAYLOAD_SIZE;
constexpr uint64_t kMaxSizeT = std::numeric_limits<size_t>::max();
constexpr uint64_t kMaxSafeJsInteger = 9007199254740991;
constexpr auto kSocketAddressInfoTimeout = 60 * NGTCP2_SECONDS;
constexpr size_t kMaxVectorCount = 16;
constexpr stream_id kMaxStreamId = std::numeric_limits<stream_id>::max();

class DebugIndentScope final {
 public:
  inline DebugIndentScope() { ++indent_; }
  DISALLOW_COPY_AND_MOVE(DebugIndentScope)
  inline ~DebugIndentScope() { --indent_; }
  inline std::string Prefix() const {
    std::string res("\n");
    res.append(indent_, '\t');
    return res;
  }
  inline std::string Close() const {
    std::string res("\n");
    res.append(indent_ - 1, '\t');
    res += "}";
    return res;
  }

 private:
  static int indent_;
};

}  // namespace node::quic
