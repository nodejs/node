#include "node_util.h"
#include "base_object-inl.h"
#include "node_errors.h"
#include "node_external_reference.h"
#include "util-inl.h"

namespace node {
namespace util {

using v8::ALL_PROPERTIES;
using v8::Array;
using v8::ArrayBufferView;
using v8::BigInt;
using v8::Boolean;
using v8::Context;
using v8::External;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::IndexFilter;
using v8::Integer;
using v8::Isolate;
using v8::KeyCollectionMode;
using v8::Local;
using v8::Object;
using v8::ONLY_CONFIGURABLE;
using v8::ONLY_ENUMERABLE;
using v8::ONLY_WRITABLE;
using v8::Promise;
using v8::PropertyFilter;
using v8::Proxy;
using v8::SKIP_STRINGS;
using v8::SKIP_SYMBOLS;
using v8::String;
using v8::Uint32;
using v8::Value;

// Used in ToUSVString().
constexpr char16_t kUnicodeReplacementCharacter = 0xFFFD;

// If a UTF-16 character is a low/trailing surrogate.
CHAR_TEST(16, IsUnicodeTrail, (ch & 0xFC00) == 0xDC00)

// If a UTF-16 character is a surrogate.
CHAR_TEST(16, IsUnicodeSurrogate, (ch & 0xF800) == 0xD800)

// If a UTF-16 surrogate is a low/trailing one.
CHAR_TEST(16, IsUnicodeSurrogateTrail, (ch & 0x400) != 0)

static void GetOwnNonIndexProperties(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Local<Context> context = env->context();

  CHECK(args[0]->IsObject());
  CHECK(args[1]->IsUint32());

  Local<Object> object = args[0].As<Object>();

  Local<Array> properties;

  PropertyFilter filter =
    static_cast<PropertyFilter>(args[1].As<Uint32>()->Value());

  if (!object->GetPropertyNames(
        context, KeyCollectionMode::kOwnOnly,
        filter,
        IndexFilter::kSkipIndices)
          .ToLocal(&properties)) {
    return;
  }
  args.GetReturnValue().Set(properties);
}

static void GetConstructorName(
    const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsObject());

  Local<Object> object = args[0].As<Object>();
  Local<String> name = object->GetConstructorName();

  args.GetReturnValue().Set(name);
}

static void GetExternalValue(
    const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsExternal());
  Isolate* isolate = args.GetIsolate();
  Local<External> external = args[0].As<External>();

  void* ptr = external->Value();
  uint64_t value = reinterpret_cast<uint64_t>(ptr);
  Local<BigInt> ret = BigInt::NewFromUnsigned(isolate, value);
  args.GetReturnValue().Set(ret);
}

static void GetPromiseDetails(const FunctionCallbackInfo<Value>& args) {
  // Return undefined if it's not a Promise.
  if (!args[0]->IsPromise())
    return;

  auto isolate = args.GetIsolate();

  Local<Promise> promise = args[0].As<Promise>();

  int state = promise->State();
  Local<Value> values[2] = { Integer::New(isolate, state) };
  size_t number_of_values = 1;
  if (state != Promise::PromiseState::kPending)
    values[number_of_values++] = promise->Result();
  Local<Array> ret = Array::New(isolate, values, number_of_values);
  args.GetReturnValue().Set(ret);
}

static void GetProxyDetails(const FunctionCallbackInfo<Value>& args) {
  // Return undefined if it's not a proxy.
  if (!args[0]->IsProxy())
    return;

  Local<Proxy> proxy = args[0].As<Proxy>();

  // TODO(BridgeAR): Remove the length check as soon as we prohibit access to
  // the util binding layer. It's accessed in the wild and `esm` would break in
  // case the check is removed.
  if (args.Length() == 1 || args[1]->IsTrue()) {
    Local<Value> ret[] = {
      proxy->GetTarget(),
      proxy->GetHandler()
    };

    args.GetReturnValue().Set(
        Array::New(args.GetIsolate(), ret, arraysize(ret)));
  } else {
    Local<Value> ret = proxy->GetTarget();

    args.GetReturnValue().Set(ret);
  }
}

static void IsArrayBufferDetached(const FunctionCallbackInfo<Value>& args) {
  if (args[0]->IsArrayBuffer()) {
    auto buffer = args[0].As<v8::ArrayBuffer>();
    args.GetReturnValue().Set(buffer->WasDetached());
    return;
  }
  args.GetReturnValue().Set(false);
}

static void PreviewEntries(const FunctionCallbackInfo<Value>& args) {
  if (!args[0]->IsObject())
    return;

  Environment* env = Environment::GetCurrent(args);
  bool is_key_value;
  Local<Array> entries;
  if (!args[0].As<Object>()->PreviewEntries(&is_key_value).ToLocal(&entries))
    return;
  // Fast path for WeakMap and WeakSet.
  if (args.Length() == 1)
    return args.GetReturnValue().Set(entries);

  Local<Value> ret[] = {
    entries,
    Boolean::New(env->isolate(), is_key_value)
  };
  return args.GetReturnValue().Set(
      Array::New(env->isolate(), ret, arraysize(ret)));
}

static void Sleep(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsUint32());
  uint32_t msec = args[0].As<Uint32>()->Value();
  uv_sleep(msec);
}

void ArrayBufferViewHasBuffer(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsArrayBufferView());
  args.GetReturnValue().Set(args[0].As<ArrayBufferView>()->HasBuffer());
}

WeakReference::WeakReference(Realm* realm,
                             Local<Object> object,
                             Local<Object> target)
    : WeakReference(realm, object, target, 0) {}

WeakReference::WeakReference(Realm* realm,
                             Local<Object> object,
                             Local<Object> target,
                             uint64_t reference_count)
    : SnapshotableObject(realm, object, type_int),
      reference_count_(reference_count) {
  MakeWeak();
  if (!target.IsEmpty()) {
    target_.Reset(realm->isolate(), target);
    if (reference_count_ == 0) {
      target_.SetWeak();
    }
  }
}

bool WeakReference::PrepareForSerialization(Local<Context> context,
                                            v8::SnapshotCreator* creator) {
  if (target_.IsEmpty()) {
    target_index_ = 0;
    return true;
  }

  // Users can still hold strong references to target in addition to the
  // reference that we manage here, and they could expect that the referenced
  // object remains the same as long as that external strong reference
  // is alive. Since we have no way to know if there is any other reference
  // keeping the target alive, the best we can do to maintain consistency is to
  // simply save a reference to the target in the snapshot (effectively making
  // it strong) during serialization, and restore it during deserialization.
  // If there's no known counted reference from our side, we'll make the
  // reference here weak upon deserialization so that it can be GC'ed if users
  // do not hold additional references to it.
  Local<Object> target = target_.Get(context->GetIsolate());
  target_index_ = creator->AddData(context, target);
  DCHECK_NE(target_index_, 0);
  target_.Reset();
  return true;
}

InternalFieldInfoBase* WeakReference::Serialize(int index) {
  DCHECK_EQ(index, BaseObject::kEmbedderType);
  InternalFieldInfo* info =
      InternalFieldInfoBase::New<InternalFieldInfo>(type());
  info->target = target_index_;
  info->reference_count = reference_count_;
  return info;
}

void WeakReference::Deserialize(Local<Context> context,
                                Local<Object> holder,
                                int index,
                                InternalFieldInfoBase* info) {
  DCHECK_EQ(index, BaseObject::kEmbedderType);
  HandleScope scope(context->GetIsolate());

  InternalFieldInfo* weak_info = reinterpret_cast<InternalFieldInfo*>(info);
  Local<Object> target;
  if (weak_info->target != 0) {
    target = context->GetDataFromSnapshotOnce<Object>(weak_info->target)
                 .ToLocalChecked();
  }
  new WeakReference(
      Realm::GetCurrent(context), holder, target, weak_info->reference_count);
}

void WeakReference::New(const FunctionCallbackInfo<Value>& args) {
  Realm* realm = Realm::GetCurrent(args);
  CHECK(args.IsConstructCall());
  CHECK(args[0]->IsObject());
  new WeakReference(realm, args.This(), args[0].As<Object>());
}

void WeakReference::Get(const FunctionCallbackInfo<Value>& args) {
  WeakReference* weak_ref = Unwrap<WeakReference>(args.Holder());
  Isolate* isolate = args.GetIsolate();
  if (!weak_ref->target_.IsEmpty())
    args.GetReturnValue().Set(weak_ref->target_.Get(isolate));
}

void WeakReference::IncRef(const FunctionCallbackInfo<Value>& args) {
  WeakReference* weak_ref = Unwrap<WeakReference>(args.Holder());
  weak_ref->reference_count_++;
  if (weak_ref->target_.IsEmpty()) return;
  if (weak_ref->reference_count_ == 1) weak_ref->target_.ClearWeak();
  args.GetReturnValue().Set(
      v8::Number::New(args.GetIsolate(), weak_ref->reference_count_));
}

void WeakReference::DecRef(const FunctionCallbackInfo<Value>& args) {
  WeakReference* weak_ref = Unwrap<WeakReference>(args.Holder());
  CHECK_GE(weak_ref->reference_count_, 1);
  weak_ref->reference_count_--;
  if (weak_ref->target_.IsEmpty()) return;
  if (weak_ref->reference_count_ == 0) weak_ref->target_.SetWeak();
  args.GetReturnValue().Set(
      v8::Number::New(args.GetIsolate(), weak_ref->reference_count_));
}

static void GuessHandleType(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  int fd;
  if (!args[0]->Int32Value(env->context()).To(&fd)) return;
  CHECK_GE(fd, 0);

  uv_handle_type t = uv_guess_handle(fd);
  const char* type = nullptr;

  switch (t) {
    case UV_TCP:
      type = "TCP";
      break;
    case UV_TTY:
      type = "TTY";
      break;
    case UV_UDP:
      type = "UDP";
      break;
    case UV_FILE:
      type = "FILE";
      break;
    case UV_NAMED_PIPE:
      type = "PIPE";
      break;
    case UV_UNKNOWN_HANDLE:
      type = "UNKNOWN";
      break;
    default:
      ABORT();
  }

  args.GetReturnValue().Set(OneByteString(env->isolate(), type));
}

static void ToUSVString(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK_GE(args.Length(), 2);
  CHECK(args[0]->IsString());
  CHECK(args[1]->IsNumber());

  TwoByteValue value(env->isolate(), args[0]);

  int64_t start = args[1]->IntegerValue(env->context()).FromJust();
  CHECK_GE(start, 0);

  for (size_t i = start; i < value.length(); i++) {
    char16_t c = value[i];
    if (!IsUnicodeSurrogate(c)) {
      continue;
    } else if (IsUnicodeSurrogateTrail(c) || i == value.length() - 1) {
      value[i] = kUnicodeReplacementCharacter;
    } else {
      char16_t d = value[i + 1];
      if (IsUnicodeTrail(d)) {
        i++;
      } else {
        value[i] = kUnicodeReplacementCharacter;
      }
    }
  }

  args.GetReturnValue().Set(
      String::NewFromTwoByte(env->isolate(),
                             *value,
                             v8::NewStringType::kNormal,
                             value.length()).ToLocalChecked());
}

void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(GetPromiseDetails);
  registry->Register(GetProxyDetails);
  registry->Register(IsArrayBufferDetached);
  registry->Register(PreviewEntries);
  registry->Register(GetOwnNonIndexProperties);
  registry->Register(GetConstructorName);
  registry->Register(GetExternalValue);
  registry->Register(Sleep);
  registry->Register(ArrayBufferViewHasBuffer);
  registry->Register(WeakReference::New);
  registry->Register(WeakReference::Get);
  registry->Register(WeakReference::IncRef);
  registry->Register(WeakReference::DecRef);
  registry->Register(GuessHandleType);
  registry->Register(ToUSVString);
}

void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  Environment* env = Environment::GetCurrent(context);
  Isolate* isolate = env->isolate();

  {
    Local<v8::ObjectTemplate> tmpl = v8::ObjectTemplate::New(isolate);
#define V(PropertyName, _)                                                     \
  tmpl->Set(FIXED_ONE_BYTE_STRING(env->isolate(), #PropertyName),              \
            env->PropertyName());

    PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(V)
#undef V

    target
        ->Set(context,
              FIXED_ONE_BYTE_STRING(isolate, "privateSymbols"),
              tmpl->NewInstance(context).ToLocalChecked())
        .Check();
  }

  {
    Local<Object> constants = Object::New(isolate);
#define V(name)                                                                \
  constants                                                                    \
      ->Set(context,                                                           \
            FIXED_ONE_BYTE_STRING(isolate, #name),                             \
            Integer::New(isolate, Promise::PromiseState::name))                \
      .Check();

    V(kPending);
    V(kFulfilled);
    V(kRejected);
#undef V

#define V(name)                                                                \
  constants                                                                    \
      ->Set(context,                                                           \
            FIXED_ONE_BYTE_STRING(isolate, #name),                             \
            Integer::New(isolate, PropertyFilter::name))                       \
      .Check();

    V(ALL_PROPERTIES);
    V(ONLY_WRITABLE);
    V(ONLY_ENUMERABLE);
    V(ONLY_CONFIGURABLE);
    V(SKIP_STRINGS);
    V(SKIP_SYMBOLS);
#undef V

    target->Set(context, env->constants_string(), constants).Check();
  }

  SetMethodNoSideEffect(
      context, target, "getPromiseDetails", GetPromiseDetails);
  SetMethodNoSideEffect(context, target, "getProxyDetails", GetProxyDetails);
  SetMethodNoSideEffect(
      context, target, "isArrayBufferDetached", IsArrayBufferDetached);
  SetMethodNoSideEffect(context, target, "previewEntries", PreviewEntries);
  SetMethodNoSideEffect(
      context, target, "getOwnNonIndexProperties", GetOwnNonIndexProperties);
  SetMethodNoSideEffect(
      context, target, "getConstructorName", GetConstructorName);
  SetMethodNoSideEffect(context, target, "getExternalValue", GetExternalValue);
  SetMethod(context, target, "sleep", Sleep);

  SetMethod(
      context, target, "arrayBufferViewHasBuffer", ArrayBufferViewHasBuffer);

  Local<String> should_abort_on_uncaught_toggle =
      FIXED_ONE_BYTE_STRING(env->isolate(), "shouldAbortOnUncaughtToggle");
  CHECK(target
            ->Set(context,
                  should_abort_on_uncaught_toggle,
                  env->should_abort_on_uncaught_toggle().GetJSArray())
            .FromJust());

  Local<FunctionTemplate> weak_ref =
      NewFunctionTemplate(isolate, WeakReference::New);
  weak_ref->InstanceTemplate()->SetInternalFieldCount(
      WeakReference::kInternalFieldCount);
  weak_ref->Inherit(BaseObject::GetConstructorTemplate(env));
  SetProtoMethod(isolate, weak_ref, "get", WeakReference::Get);
  SetProtoMethod(isolate, weak_ref, "incRef", WeakReference::IncRef);
  SetProtoMethod(isolate, weak_ref, "decRef", WeakReference::DecRef);
  SetConstructorFunction(context, target, "WeakReference", weak_ref);

  SetMethod(context, target, "guessHandleType", GuessHandleType);

  SetMethodNoSideEffect(context, target, "toUSVString", ToUSVString);
}

}  // namespace util
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(util, node::util::Initialize)
NODE_BINDING_EXTERNAL_REFERENCE(util, node::util::RegisterExternalReferences)
