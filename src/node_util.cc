#include "base_object-inl.h"
#include "node_dotenv.h"
#include "node_errors.h"
#include "node_external_reference.h"
#include "util-inl.h"
#include "v8-fast-api-calls.h"

namespace node {
namespace util {

using v8::ALL_PROPERTIES;
using v8::Array;
using v8::ArrayBuffer;
using v8::ArrayBufferView;
using v8::BigInt;
using v8::Boolean;
using v8::CFunction;
using v8::Context;
using v8::DictionaryTemplate;
using v8::External;
using v8::FunctionCallbackInfo;
using v8::IndexFilter;
using v8::Integer;
using v8::Isolate;
using v8::KeyCollectionMode;
using v8::Local;
using v8::LocalVector;
using v8::MaybeLocal;
using v8::Name;
using v8::Number;
using v8::Object;
using v8::ObjectTemplate;
using v8::ONLY_CONFIGURABLE;
using v8::ONLY_ENUMERABLE;
using v8::ONLY_WRITABLE;
using v8::Promise;
using v8::PropertyFilter;
using v8::Proxy;
using v8::SharedArrayBuffer;
using v8::SKIP_STRINGS;
using v8::SKIP_SYMBOLS;
using v8::StackFrame;
using v8::StackTrace;
using v8::String;
using v8::Uint32;
using v8::Uint8Array;
using v8::Value;

// If a UTF-16 character is a low/trailing surrogate.
CHAR_TEST(16, IsUnicodeTrail, (ch & 0xFC00) == 0xDC00)

// If a UTF-16 character is a surrogate.
CHAR_TEST(16, IsUnicodeSurrogate, (ch & 0xF800) == 0xD800)

// If a UTF-16 surrogate is a low/trailing one.
CHAR_TEST(16, IsUnicodeSurrogateTrail, (ch & 0x400) != 0)

static void GetOwnNonIndexProperties(
    const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();

  CHECK(args[0]->IsObject());
  CHECK(args[1]->IsUint32());

  Local<Object> object = args[0].As<Object>();

  Local<Array> properties;

  PropertyFilter filter = FromV8Value<PropertyFilter>(args[1]);

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

  void* ptr = external->Value(v8::kExternalPointerTypeTagDefault);
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

static void GetCallerLocation(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  Local<StackTrace> trace = StackTrace::CurrentStackTrace(isolate, 2);

  // This function is frame zero. The caller is frame one. If there aren't two
  // stack frames, return undefined.
  if (trace->GetFrameCount() != 2) {
    return;
  }

  Local<StackFrame> frame = trace->GetFrame(isolate, 1);
  Local<Value> file = frame->GetScriptNameOrSourceURL();

  if (file.IsEmpty()) {
    return;
  }

  Local<Value> ret[] = {Integer::New(isolate, frame->GetLineNumber()),
                        Integer::New(isolate, frame->GetColumn()),
                        file};

  args.GetReturnValue().Set(Array::New(args.GetIsolate(), ret, arraysize(ret)));
}

static void PreviewEntries(const FunctionCallbackInfo<Value>& args) {
  if (!args[0]->IsObject())
    return;

  Isolate* isolate = args.GetIsolate();
  bool is_key_value;
  Local<Array> entries;
  if (!args[0].As<Object>()->PreviewEntries(&is_key_value).ToLocal(&entries))
    return;
  // Fast path for WeakMap and WeakSet.
  if (args.Length() == 1)
    return args.GetReturnValue().Set(entries);

  Local<Value> ret[] = {entries, Boolean::New(isolate, is_key_value)};
  return args.GetReturnValue().Set(Array::New(isolate, ret, arraysize(ret)));
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

// Returns [buffer, byteOffset, byteLength] in a single binding crossing,
// equivalent to reading the three properties via
// Reflect.get(view.constructor.prototype, ..., view). Uses the V8 API
// directly so it is immune to prototype tampering and avoids the JS-side
// overhead of the defensive accessors in lib/internal/.
void GetArrayBufferView(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  CHECK(args[0]->IsArrayBufferView());
  Local<ArrayBufferView> view = args[0].As<ArrayBufferView>();
  Local<Value> values[] = {
      view->Buffer(),
      Number::New(isolate, static_cast<double>(view->ByteOffset())),
      Number::New(isolate, static_cast<double>(view->ByteLength())),
  };
  args.GetReturnValue().Set(Array::New(isolate, values, arraysize(values)));
}

static bool ReadNonNegativeInteger(Local<Value> value, uint64_t* result) {
  constexpr double kMaxSafeInteger = 9007199254740991.0;
  double number = value.As<Number>()->Value();
  if (number < 0 || number > kMaxSafeInteger) {
    return false;
  }
  *result = static_cast<uint64_t>(number);
  return static_cast<double>(*result) == number;
}

// Returns true iff bytes can be safely copied between the buffers given the
// requested offsets and count. Matches lib/internal/webstreams/util.js:
//   toBuffer !== fromBuffer &&
//   !toBuffer.detached &&
//   !fromBuffer.detached &&
//   toIndex + count <= toBuffer.byteLength &&
//   fromIndex + count <= fromBuffer.byteLength
void CanCopyArrayBuffer(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsArrayBuffer() || args[0]->IsSharedArrayBuffer());
  CHECK(args[1]->IsNumber());
  CHECK(args[2]->IsArrayBuffer() || args[2]->IsSharedArrayBuffer());
  CHECK(args[3]->IsNumber());
  CHECK(args[4]->IsNumber());

  // SharedArrayBuffer handles are interoperable with ArrayBuffer handles in
  // V8, so we can use the ArrayBuffer accessors uniformly. WasDetached()
  // always returns false on a SAB.
  Local<ArrayBuffer> to_buffer = args[0].As<ArrayBuffer>();
  Local<ArrayBuffer> from_buffer = args[2].As<ArrayBuffer>();

  if (to_buffer->StrictEquals(from_buffer)) {
    args.GetReturnValue().Set(false);
    return;
  }
  if (to_buffer->WasDetached() || from_buffer->WasDetached()) {
    args.GetReturnValue().Set(false);
    return;
  }

  uint64_t to_index;
  uint64_t from_index;
  uint64_t count;
  if (!ReadNonNegativeInteger(args[1], &to_index) ||
      !ReadNonNegativeInteger(args[3], &from_index) ||
      !ReadNonNegativeInteger(args[4], &count)) {
    args.GetReturnValue().Set(false);
    return;
  }

  const uint64_t to_byte_length = to_buffer->ByteLength();
  const uint64_t from_byte_length = from_buffer->ByteLength();

  bool ok = to_index <= to_byte_length && count <= to_byte_length - to_index &&
            from_index <= from_byte_length &&
            count <= from_byte_length - from_index;
  args.GetReturnValue().Set(ok);
}

// Equivalent to:
//   new Uint8Array(view.buffer.slice(view.byteOffset,
//                                    view.byteOffset + view.byteLength))
// Allocates a fresh ArrayBuffer with the view's bytes copied into it, then
// returns a Uint8Array over the full new buffer. Avoids the JS-side
// Reflect.get + slice round-trip.
void CloneAsUint8Array(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  CHECK(args[0]->IsArrayBufferView());
  Local<ArrayBufferView> view = args[0].As<ArrayBufferView>();
  size_t byte_length = view->ByteLength();
  Local<ArrayBuffer> new_buffer;
  if (!ArrayBuffer::MaybeNew(isolate, byte_length).ToLocal(&new_buffer)) {
    // MaybeNew does not schedule an exception on allocation failure.
    THROW_ERR_MEMORY_ALLOCATION_FAILED(isolate);
    return;
  }
  if (byte_length > 0) {
    size_t copied = view->CopyContents(new_buffer->Data(), byte_length);
    CHECK_EQ(copied, byte_length);
  }
  args.GetReturnValue().Set(Uint8Array::New(new_buffer, 0, byte_length));
}

static uint32_t GetUVHandleTypeCode(const uv_handle_type type) {
  // TODO(anonrig): We can use an enum here and then create the array in the
  // binding, which will remove the hard-coding in C++ and JS land.

  // Currently, the return type of this function corresponds to the index of the
  // array defined in the JS land. This is done as an optimization to reduce the
  // string serialization overhead.
  switch (type) {
    case UV_TCP:
      return 0;
    case UV_TTY:
      return 1;
    case UV_UDP:
      return 2;
    case UV_FILE:
      return 3;
    case UV_NAMED_PIPE:
      return 4;
    case UV_UNKNOWN_HANDLE:
      return 5;
    default:
      ABORT();
  }
}

static void GuessHandleType(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  int fd;
  if (!args[0]->Int32Value(context).To(&fd)) return;
  CHECK_GE(fd, 0);

  uv_handle_type t = uv_guess_handle(fd);
  args.GetReturnValue().Set(GetUVHandleTypeCode(t));
}

static uint32_t FastGuessHandleType(Local<Value> receiver, const uint32_t fd) {
  uv_handle_type t = uv_guess_handle(fd);
  return GetUVHandleTypeCode(t);
}

CFunction fast_guess_handle_type_(CFunction::Make(FastGuessHandleType));

static void ParseEnv(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  Environment* env = Environment::GetCurrent(context);
  CHECK_EQ(args.Length(), 1);  // content
  CHECK(args[0]->IsString());
  Utf8Value content(isolate, args[0]);
  Dotenv dotenv{};
  dotenv.ParseContent(content.ToStringView());
  Local<Object> obj;
  if (dotenv.ToObject(env).ToLocal(&obj)) {
    args.GetReturnValue().Set(obj);
  }
}

static void GetCallSites(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  Environment* env = Environment::GetCurrent(context);

  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsUint32());
  const uint32_t frames = args[0].As<Uint32>()->Value();
  CHECK(frames >= 1 && frames <= 200);

  // +1 for disregarding node:util
  Local<StackTrace> stack = StackTrace::CurrentStackTrace(isolate, frames + 1);
  const int frame_count = stack->GetFrameCount();
  LocalVector<Value> callsite_objects(isolate);

  auto callsite_template = env->callsite_template();
  if (callsite_template.IsEmpty()) {
    static constexpr std::string_view names[] = {
        "functionName",
        "scriptId",
        "scriptName",
        "lineNumber",
        "columnNumber",
        // TODO(legendecas): deprecate CallSite.column.
        "column"};
    callsite_template = DictionaryTemplate::New(isolate, names);
    env->set_callsite_template(callsite_template);
  }

  // Frame 0 is node:util. It should be skipped.
  for (int i = 1; i < frame_count; ++i) {
    Local<StackFrame> stack_frame = stack->GetFrame(isolate, i);

    Local<Value> function_name = stack_frame->GetFunctionName();
    if (function_name.IsEmpty()) {
      function_name = v8::String::Empty(isolate);
    }

    Local<Value> script_name = stack_frame->GetScriptName();
    if (script_name.IsEmpty()) {
      script_name = v8::String::Empty(isolate);
    }

    std::string script_id = std::to_string(stack_frame->GetScriptId());

    MaybeLocal<Value> values[] = {
        function_name,
        OneByteString(isolate, script_id),
        script_name,
        Integer::NewFromUnsigned(isolate, stack_frame->GetLineNumber()),
        Integer::NewFromUnsigned(isolate, stack_frame->GetColumn()),
        // TODO(legendecas): deprecate CallSite.column.
        Integer::NewFromUnsigned(isolate, stack_frame->GetColumn()),
    };

    Local<Object> callsite;
    if (!NewDictionaryInstanceNullProto(context, callsite_template, values)
             .ToLocal(&callsite)) {
      return;
    }
    callsite_objects.push_back(callsite);
  }

  Local<Array> callsites =
      Array::New(isolate, callsite_objects.data(), callsite_objects.size());
  args.GetReturnValue().Set(callsites);
}

/**
 * Checks whether the current call directly initiated from a file inside
 * node_modules. This checks up to `frame_limit` stack frames, until it finds
 * a frame that is not part of node internal modules.
 */
static void IsInsideNodeModules(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();

  int frames_limit = (args.Length() > 0 && args[0]->IsInt32())
                         ? args[0].As<v8::Int32>()->Value()
                         : 10;
  Local<StackTrace> stack =
      StackTrace::CurrentStackTrace(isolate, frames_limit);
  int frame_count = stack->GetFrameCount();

  bool result = false;
  for (int i = 0; i < frame_count; ++i) {
    Local<StackFrame> stack_frame = stack->GetFrame(isolate, i);
    Local<String> script_name = stack_frame->GetScriptName();

    if (script_name.IsEmpty() || script_name->Length() == 0) {
      continue;
    }
    Utf8Value script_name_utf8(isolate, script_name);
    std::string_view script_name_str = script_name_utf8.ToStringView();
    if (script_name_str.starts_with("node:")) {
      continue;
    }
    result = script_name_str.find("/node_modules/") != std::string::npos ||
             script_name_str.find("\\node_modules\\") != std::string::npos ||
             script_name_str.find("/node_modules\\") != std::string::npos ||
             script_name_str.find("\\node_modules/") != std::string::npos;
    break;
  }

  args.GetReturnValue().Set(result);
}

static void DefineLazyPropertiesGetter(
    Local<v8::Name> name, const v8::PropertyCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  // This getter has no JavaScript function representation and is not
  // invoked in the creation context.
  // When this getter is invoked in a vm context, the `Realm::GetCurrent(info)`
  // returns a nullptr and retrieve the creation context via `this` object and
  // get the creation Realm.
  Local<Value> receiver_val = info.HolderV2();
  if (!receiver_val->IsObject()) {
    THROW_ERR_INVALID_INVOCATION(isolate);
    return;
  }
  Local<Object> receiver = receiver_val.As<Object>();
  Local<Context> context;
  if (!receiver->GetCreationContext().ToLocal(&context)) {
    THROW_ERR_INVALID_INVOCATION(isolate);
    return;
  }

  Realm* realm = Realm::GetCurrent(context);
  Local<Value> arg = info.Data();
  Local<Value> require_result;
  if (!realm->builtin_module_require()
           ->Call(context, Null(isolate), 1, &arg)
           .ToLocal(&require_result)) {
    // V8 will have scheduled an error to be thrown.
    return;
  }
  Local<Value> ret;
  if (!require_result.As<v8::Object>()->Get(context, name).ToLocal(&ret)) {
    // V8 will have scheduled an error to be thrown.
    return;
  }
  info.GetReturnValue().Set(ret);
}

static void DefineLazyProperties(const FunctionCallbackInfo<Value>& args) {
  // target: object, id: string, keys: string[][, enumerable = true]
  CHECK_GE(args.Length(), 3);
  // target: Object where to define the lazy properties.
  CHECK(args[0]->IsObject());
  // id: Internal module to lazy-load where the API to expose are implemented.
  CHECK(args[1]->IsString());
  // keys: Keys to map from `require(id)` and `target`.
  CHECK(args[2]->IsArray());
  // enumerable: Whether the property should be enumerable.
  CHECK(args.Length() == 3 || args[3]->IsBoolean());

  auto context = args.GetIsolate()->GetCurrentContext();

  auto target = args[0].As<Object>();
  Local<Value> id = args[1];
  v8::PropertyAttribute attribute =
      args.Length() == 3 || args[3]->IsTrue() ? v8::None : v8::DontEnum;

  const Local<Array> keys = args[2].As<Array>();
  size_t length = keys->Length();
  for (size_t i = 0; i < length; i++) {
    Local<Value> key;
    if (!keys->Get(context, i).ToLocal(&key)) {
      // V8 will have scheduled an error to be thrown.
      return;
    }
    CHECK(key->IsString());
    if (target
            ->SetLazyDataProperty(context,
                                  key.As<String>(),
                                  DefineLazyPropertiesGetter,
                                  id,
                                  attribute)
            .IsNothing()) {
      // V8 will have scheduled an error to be thrown.
      return;
    };
  }
}

void ConstructSharedArrayBuffer(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  int64_t length;
  // Note: IntegerValue() clamps its output, so excessively large input values
  // will not overflow
  if (!args[0]->IntegerValue(env->context()).To(&length)) {
    return;
  }
  if (length < 0 ||
      static_cast<uint64_t>(length) > ArrayBuffer::kMaxByteLength) {
    env->ThrowRangeError("Invalid array buffer length");
    return;
  }
  Local<SharedArrayBuffer> sab;
  if (!SharedArrayBuffer::MaybeNew(env->isolate(), static_cast<size_t>(length))
           .ToLocal(&sab)) {
    // Note: SharedArrayBuffer::MaybeNew doesn't schedule an exception if it
    // fails
    env->ThrowRangeError("Array buffer allocation failed");
    return;
  }
  args.GetReturnValue().Set(sab);
}

// Marks a promise as handled and silent to prevent unhandled rejection
// tracking from triggering.
void MarkPromiseAsHandled(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsPromise());
  Local<Promise> promise = args[0].As<Promise>();
  promise->MarkAsHandled();
  promise->MarkAsSilent();
}

void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(GetPromiseDetails);
  registry->Register(GetProxyDetails);
  registry->Register(GetCallerLocation);
  registry->Register(PreviewEntries);
  registry->Register(GetCallSites);
  registry->Register(GetOwnNonIndexProperties);
  registry->Register(GetConstructorName);
  registry->Register(GetExternalValue);
  registry->Register(Sleep);
  registry->Register(ArrayBufferViewHasBuffer);
  registry->Register(GetArrayBufferView);
  registry->Register(CanCopyArrayBuffer);
  registry->Register(CloneAsUint8Array);
  registry->Register(GuessHandleType);
  registry->Register(fast_guess_handle_type_);
  registry->Register(ParseEnv);
  registry->Register(IsInsideNodeModules);
  registry->Register(DefineLazyProperties);
  registry->Register(DefineLazyPropertiesGetter);
  registry->Register(ConstructSharedArrayBuffer);
  registry->Register(MarkPromiseAsHandled);
}

void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  Environment* env = Environment::GetCurrent(context);
  Isolate* isolate = env->isolate();

  {
    Local<ObjectTemplate> tmpl = ObjectTemplate::New(isolate);
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
            Integer::New(isolate, Environment::ExitInfoField::name))           \
      .Check();

    V(kExiting);
    V(kExitCode);
    V(kHasExitCode);
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

#define V(name)                                                                \
  constants                                                                    \
      ->Set(                                                                   \
          context,                                                             \
          FIXED_ONE_BYTE_STRING(isolate, #name),                               \
          Integer::New(isolate,                                                \
                       static_cast<int32_t>(BaseObject::TransferMode::name)))  \
      .Check();

    V(kDisallowCloneAndTransfer);
    V(kTransferable);
    V(kCloneable);
#undef V

    target->Set(context, env->constants_string(), constants).Check();
  }

  SetMethod(context, target, "isInsideNodeModules", IsInsideNodeModules);
  SetMethod(context, target, "defineLazyProperties", DefineLazyProperties);
  SetMethodNoSideEffect(
      context, target, "getPromiseDetails", GetPromiseDetails);
  SetMethodNoSideEffect(context, target, "getProxyDetails", GetProxyDetails);
  SetMethodNoSideEffect(
      context, target, "getCallerLocation", GetCallerLocation);
  SetMethodNoSideEffect(context, target, "previewEntries", PreviewEntries);
  SetMethodNoSideEffect(
      context, target, "getOwnNonIndexProperties", GetOwnNonIndexProperties);
  SetMethodNoSideEffect(
      context, target, "getConstructorName", GetConstructorName);
  SetMethodNoSideEffect(context, target, "getExternalValue", GetExternalValue);
  SetMethodNoSideEffect(context, target, "getCallSites", GetCallSites);
  SetMethod(context, target, "sleep", Sleep);
  SetMethod(context, target, "parseEnv", ParseEnv);
  SetMethod(
      context, target, "arrayBufferViewHasBuffer", ArrayBufferViewHasBuffer);
  SetMethodNoSideEffect(
      context, target, "getArrayBufferView", GetArrayBufferView);
  SetMethodNoSideEffect(
      context, target, "canCopyArrayBuffer", CanCopyArrayBuffer);
  SetMethod(context, target, "cloneAsUint8Array", CloneAsUint8Array);
  SetMethod(context,
            target,
            "constructSharedArrayBuffer",
            ConstructSharedArrayBuffer);
  SetMethod(context, target, "markPromiseAsHandled", MarkPromiseAsHandled);

  Local<String> should_abort_on_uncaught_toggle =
      FIXED_ONE_BYTE_STRING(env->isolate(), "shouldAbortOnUncaughtToggle");
  CHECK(target
            ->Set(context,
                  should_abort_on_uncaught_toggle,
                  env->should_abort_on_uncaught_toggle().GetJSArray())
            .FromJust());

  SetFastMethodNoSideEffect(context,
                            target,
                            "guessHandleType",
                            GuessHandleType,
                            &fast_guess_handle_type_);
}

}  // namespace util
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(util, node::util::Initialize)
NODE_BINDING_EXTERNAL_REFERENCE(util, node::util::RegisterExternalReferences)
