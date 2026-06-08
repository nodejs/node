#include "streams/readable_stream.h"
#include "streams/streams_binding.h"
#include "base_object-inl.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "node_buffer.h"
#include "node_errors.h"
#include "node_external_reference.h"
#include "node_realm-inl.h"
#include "util-inl.h"
#include "util.h"
#include "v8.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <utility>

namespace node {
namespace webstreams {

using v8::Array;
using v8::ArrayBuffer;
using v8::ArrayBufferView;
using v8::BackingStore;
using v8::BigInt64Array;
using v8::BigUint64Array;
using v8::Boolean;
using v8::Context;
using v8::DataView;
using v8::Exception;
using v8::Float16Array;
using v8::Float32Array;
using v8::Float64Array;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Int16Array;
using v8::Int32Array;
using v8::Int8Array;
using v8::Isolate;
using v8::Just;
using v8::JustVoid;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Nothing;
using v8::Number;
using v8::Object;
using v8::ObjectTemplate;
using v8::Promise;
using v8::Signature;
using v8::String;
using v8::TryCatch;
using v8::Uint16Array;
using v8::Uint32Array;
using v8::Uint8Array;
using v8::Uint8ClampedArray;
using v8::Undefined;
using v8::Value;

namespace {

// Recovers a related StreamBaseObject stored in a wrapper's internal field,
// checking the kind tag before downcasting (BaseObject::FromJSObject performs an
// unchecked static_cast and RTTI is unavailable). Returns nullptr if the field
// is unset or holds a different kind.
template <typename T>
T* GetStreamField(Local<Object> obj, int index, StreamBaseObject::Kind kind) {
  Local<Value> v = obj->GetInternalField(index).template As<Value>();
  if (!v->IsObject()) return nullptr;
  BaseObject* base = BaseObject::FromJSObject(v.As<Object>());
  if (base == nullptr) return nullptr;
  auto* s = static_cast<StreamBaseObject*>(base);
  if (s->stream_kind() != kind) return nullptr;
  return static_cast<T*>(s);
}

Local<Object> MakeReadResult(Isolate* isolate,
                             Local<Context> context,
                             Local<Value> value,
                             bool done) {
  Local<Object> obj = Object::New(isolate);
  obj->Set(context, FIXED_ONE_BYTE_STRING(isolate, "value"), value).Check();
  obj->Set(context,
           FIXED_ONE_BYTE_STRING(isolate, "done"),
           Boolean::New(isolate, done))
      .Check();
  return obj;
}

// Builds an Error/RangeError carrying a Node error `code` property, matching the
// shape of the JS `ERR_*` errors.
Local<Value> MakeCodedError(Isolate* isolate,
                            Local<Context> context,
                            bool range,
                            const char* code,
                            const char* message) {
  Local<String> msg = OneByteString(isolate, message);
  Local<Value> err = range ? Exception::RangeError(msg) : Exception::Error(msg);
  err.As<Object>()
      ->Set(context,
            FIXED_ONE_BYTE_STRING(isolate, "code"),
            OneByteString(isolate, code))
      .Check();
  return err;
}

Local<Value> InvalidStateError(Isolate* isolate,
                               Local<Context> context,
                               const char* message) {
  // ERR_INVALID_STATE.TypeError
  Local<String> msg = OneByteString(isolate, message);
  Local<Value> err = Exception::TypeError(msg);
  err.As<Object>()
      ->Set(context,
            FIXED_ONE_BYTE_STRING(isolate, "code"),
            FIXED_ONE_BYTE_STRING(isolate, "ERR_INVALID_STATE"))
      .Check();
  return err;
}

// Reaction callbacks for user algorithm promises. args.Data() is the controller
// wrapper object (default or byte); the controller is recovered and dispatched
// by its kind tag.
void ReactStartFulfilled(const FunctionCallbackInfo<Value>& args) {
  if (!args.Data()->IsObject()) return;
  BaseObject* base = BaseObject::FromJSObject(args.Data().As<Object>());
  if (base == nullptr) return;
  auto* s = static_cast<StreamBaseObject*>(base);
  if (s->stream_kind() == StreamBaseObject::Kind::kDefaultController)
    static_cast<ReadableStreamDefaultController*>(s)->OnStartFulfilled();
  else
    static_cast<ReadableByteStreamController*>(s)->OnStartFulfilled();
}

void ReactPullFulfilled(const FunctionCallbackInfo<Value>& args) {
  if (!args.Data()->IsObject()) return;
  BaseObject* base = BaseObject::FromJSObject(args.Data().As<Object>());
  if (base == nullptr) return;
  auto* s = static_cast<StreamBaseObject*>(base);
  if (s->stream_kind() == StreamBaseObject::Kind::kDefaultController)
    static_cast<ReadableStreamDefaultController*>(s)->FinishPull();
  else
    static_cast<ReadableByteStreamController*>(s)->FinishPull();
}

void ReactRejected(const FunctionCallbackInfo<Value>& args) {
  if (!args.Data()->IsObject()) return;
  BaseObject* base = BaseObject::FromJSObject(args.Data().As<Object>());
  if (base == nullptr) return;
  auto* s = static_cast<StreamBaseObject*>(base);
  if (s->stream_kind() == StreamBaseObject::Kind::kDefaultController)
    static_cast<ReadableStreamDefaultController*>(s)->ErrorInternal(args[0]);
  else
    static_cast<ReadableByteStreamController*>(s)->ErrorInternal(args[0]);
}

// Attaches fulfill/reject reactions that re-enter C++. The reaction functions
// carry the controller wrapper as their Data.
void ThenReact(Environment* env,
               Local<Promise> promise,
               Local<Object> controller_obj,
               v8::FunctionCallback on_fulfilled) {
  Local<Context> context = env->context();
  Local<Function> ff;
  Local<Function> rj;
  if (!Function::New(context, on_fulfilled, controller_obj).ToLocal(&ff) ||
      !Function::New(context, ReactRejected, controller_obj)
           .ToLocal(&rj)) {
    return;
  }
  USE(promise->Then(context, ff, rj));
}

// A function that returns undefined; used both to map a promise's fulfilment
// value to undefined and as a no-op rejection handler.
void Noop(const FunctionCallbackInfo<Value>& args) {}

// Marks a promise as handled to avoid spurious unhandled-rejection warnings on
// internal promises, mirroring `PromisePrototypeThen(p, undefined, () => {})`.
void MarkHandled(Environment* env, Local<Promise> promise) {
  Local<Context> context = env->context();
  Local<Function> noop;
  if (Function::New(context, Noop).ToLocal(&noop))
    USE(promise->Catch(context, noop));
}

Local<Value> RangeCodedError(Isolate* isolate,
                             Local<Context> context,
                             const char* message) {
  return MakeCodedError(
      isolate, context, /* range */ true, "ERR_INVALID_STATE", message);
}

// --- byte-stream helpers ---

// Transfers an ArrayBuffer: takes ownership of its BackingStore and detaches the
// JS ArrayBuffer (zeroing its views), mirroring ArrayBuffer.prototype.transfer.
// Returns false (with a pending exception) if the buffer cannot be detached.
bool TransferArrayBuffer(Isolate* isolate,
                         Local<Context> context,
                         Local<ArrayBuffer> ab,
                         std::shared_ptr<BackingStore>* out) {
  if (ab->WasDetached()) {
    isolate->ThrowException(MakeCodedError(
        isolate, context, /* range */ false, "ERR_INVALID_STATE",
        "Cannot transfer a detached ArrayBuffer"));
    return false;
  }
  if (!ab->IsDetachable()) {
    isolate->ThrowException(MakeCodedError(
        isolate, context, /* range */ false, "ERR_INVALID_STATE",
        "Cannot transfer a non-detachable ArrayBuffer"));
    return false;
  }
  *out = ab->GetBackingStore();
  Maybe<bool> detached = ab->Detach(Local<Value>());
  if (detached.IsNothing()) {
    out->reset();
    return false;
  }
  return true;
}

// Allocates a fresh BackingStore of `byte_length` bytes (zero-initialized).
std::shared_ptr<BackingStore> NewBackingStore(Isolate* isolate,
                                              size_t byte_length) {
  std::unique_ptr<BackingStore> bs =
      ArrayBuffer::NewBackingStore(isolate, byte_length);
  return std::shared_ptr<BackingStore>(std::move(bs));
}

// Classifies an ArrayBufferView for later reconstruction and reports its
// element size.
ViewType ClassifyView(Local<ArrayBufferView> view, uint32_t* element_size) {
  if (view->IsDataView()) {
    *element_size = 1;
    return ViewType::kDataView;
  }
  if (view->IsUint8Array()) {
    *element_size = 1;
    return node::Buffer::HasInstance(view.As<Value>()) ? ViewType::kBuffer
                                                       : ViewType::kUint8Array;
  }
  if (view->IsInt8Array()) { *element_size = 1; return ViewType::kInt8Array; }
  if (view->IsUint8ClampedArray()) {
    *element_size = 1;
    return ViewType::kUint8ClampedArray;
  }
  if (view->IsInt16Array()) { *element_size = 2; return ViewType::kInt16Array; }
  if (view->IsUint16Array()) {
    *element_size = 2;
    return ViewType::kUint16Array;
  }
  if (view->IsInt32Array()) { *element_size = 4; return ViewType::kInt32Array; }
  if (view->IsUint32Array()) {
    *element_size = 4;
    return ViewType::kUint32Array;
  }
  if (view->IsFloat16Array()) {
    *element_size = 2;
    return ViewType::kFloat16Array;
  }
  if (view->IsFloat32Array()) {
    *element_size = 4;
    return ViewType::kFloat32Array;
  }
  if (view->IsFloat64Array()) {
    *element_size = 8;
    return ViewType::kFloat64Array;
  }
  if (view->IsBigInt64Array()) {
    *element_size = 8;
    return ViewType::kBigInt64Array;
  }
  if (view->IsBigUint64Array()) {
    *element_size = 8;
    return ViewType::kBigUint64Array;
  }
  *element_size = 1;
  return ViewType::kUint8Array;
}

// Builds a typed array / DataView / Buffer of `type` over `ab` at the given
// byte offset and element length.
MaybeLocal<Object> MakeView(Environment* env,
                            ViewType type,
                            Local<ArrayBuffer> ab,
                            size_t byte_offset,
                            size_t length) {
  Isolate* isolate = env->isolate();
  switch (type) {
    case ViewType::kDataView:
      return DataView::New(ab, byte_offset, length).As<Object>();
    case ViewType::kBuffer:
      return node::Buffer::New(isolate, ab, byte_offset, length)
          .FromMaybe(Local<Uint8Array>())
          .As<Object>();
    case ViewType::kUint8Array:
      return Uint8Array::New(ab, byte_offset, length).As<Object>();
    case ViewType::kInt8Array:
      return Int8Array::New(ab, byte_offset, length).As<Object>();
    case ViewType::kUint8ClampedArray:
      return Uint8ClampedArray::New(ab, byte_offset, length).As<Object>();
    case ViewType::kInt16Array:
      return Int16Array::New(ab, byte_offset, length).As<Object>();
    case ViewType::kUint16Array:
      return Uint16Array::New(ab, byte_offset, length).As<Object>();
    case ViewType::kInt32Array:
      return Int32Array::New(ab, byte_offset, length).As<Object>();
    case ViewType::kUint32Array:
      return Uint32Array::New(ab, byte_offset, length).As<Object>();
    case ViewType::kFloat16Array:
      return Float16Array::New(ab, byte_offset, length).As<Object>();
    case ViewType::kFloat32Array:
      return Float32Array::New(ab, byte_offset, length).As<Object>();
    case ViewType::kFloat64Array:
      return Float64Array::New(ab, byte_offset, length).As<Object>();
    case ViewType::kBigInt64Array:
      return BigInt64Array::New(ab, byte_offset, length).As<Object>();
    case ViewType::kBigUint64Array:
      return BigUint64Array::New(ab, byte_offset, length).As<Object>();
  }
  UNREACHABLE();
}

}  // namespace

// ===========================================================================
// ReadableStreamDefaultController
// ===========================================================================

ReadableStreamDefaultController::ReadableStreamDefaultController(
    Environment* env, Local<Object> object)
    : StreamBaseObject(env, object, Kind::kDefaultController) {}

void ReadableStreamDefaultController::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("queue", queue_);
  tracker->TrackField("pull_algorithm", pull_algorithm_);
  tracker->TrackField("cancel_algorithm", cancel_algorithm_);
  tracker->TrackField("size_algorithm", size_algorithm_);
}

ReadableStream* ReadableStreamDefaultController::stream() const {
  return GetStreamField<ReadableStream>(
      object(), kStream, Kind::kStream);
}

Local<FunctionTemplate>
ReadableStreamDefaultController::GetConstructorTemplate(Environment* env) {
  BindingData* bd = BindingData::Get(env);
  Isolate* isolate = env->isolate();
  Local<FunctionTemplate> tmpl =
      bd->readable_stream_default_controller_ctor.Get(isolate);
  if (tmpl.IsEmpty()) {
    tmpl = NewFunctionTemplate(isolate, nullptr);
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        ReadableStreamDefaultController::kInternalFieldCount);
    tmpl->SetClassName(
        FIXED_ONE_BYTE_STRING(isolate, "ReadableStreamDefaultController"));
    Local<Signature> sig = Signature::New(isolate, tmpl);
    tmpl->PrototypeTemplate()->SetAccessorProperty(
        FIXED_ONE_BYTE_STRING(isolate, "desiredSize"),
        FunctionTemplate::New(
            isolate, GetDesiredSize, Local<Value>(), sig, 0,
            v8::ConstructorBehavior::kThrow, v8::SideEffectType::kHasNoSideEffect));
    SetProtoMethod(isolate, tmpl, "close", Close);
    SetProtoMethod(isolate, tmpl, "enqueue", Enqueue);
    SetProtoMethod(isolate, tmpl, "error", Error);
    bd->readable_stream_default_controller_ctor.Reset(isolate, tmpl);
  }
  return tmpl;
}

void ReadableStreamDefaultController::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(GetDesiredSize);
  registry->Register(Close);
  registry->Register(Enqueue);
  registry->Register(Error);
}

double ReadableStreamDefaultController::GetDesiredSizeValue(
    bool* is_null) const {
  ReadableStream* stream = this->stream();
  *is_null = false;
  switch (stream->state()) {
    case StreamState::kErrored:
      *is_null = true;
      return 0;
    case StreamState::kClosed:
      return 0;
    default:
      return high_water_mark_ - queue_total_size_;
  }
}

bool ReadableStreamDefaultController::CanCloseOrEnqueue() const {
  return !close_requested_ &&
         stream()->state() == StreamState::kReadable;
}

bool ReadableStreamDefaultController::ShouldCallPull() const {
  if (!CanCloseOrEnqueue() || !started_) return false;
  ReadableStream* stream = this->stream();
  if (stream->locked() && stream->has_default_reader()) {
    if (stream->default_reader()->num_read_requests() > 0) return true;
  }
  bool is_null = false;
  double desired = GetDesiredSizeValue(&is_null);
  CHECK(!is_null);
  return desired > 0;
}

void ReadableStreamDefaultController::OnStartFulfilled() {
  started_ = true;
  CHECK(!pulling_);
  CHECK(!pull_again_);
  CallPullIfNeeded();
}

void ReadableStreamDefaultController::FinishPull() {
  pulling_ = false;
  if (pull_again_) {
    pull_again_ = false;
    CallPullIfNeeded();
  }
}

void ReadableStreamDefaultController::CallPullIfNeeded() {
  if (!ShouldCallPull()) return;
  if (pulling_) {
    pull_again_ = true;
    return;
  }
  CHECK(!pull_again_);
  pulling_ = true;
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  Local<Object> controller_obj = object();
  Local<Function> pull = pull_algorithm_.Get(isolate);
  Local<Value> argv[] = {controller_obj};
  Local<Value> result;
  if (!pull->Call(context, Undefined(isolate), 1, argv).ToLocal(&result))
    return;
  // The pull algorithm is wrapped as an async function, so it returns a Promise.
  if (!result->IsPromise()) return;
  ThenReact(env, result.As<Promise>(), controller_obj, ReactPullFulfilled);
}

void ReadableStreamDefaultController::ClearAlgorithms() {
  pull_algorithm_.Reset();
  cancel_algorithm_.Reset();
  size_algorithm_.Reset();
}

void ReadableStreamDefaultController::CloseInternal() {
  if (!CanCloseOrEnqueue()) return;
  close_requested_ = true;
  if (queue_size_ == 0) {
    ClearAlgorithms();
    stream()->Close();
  }
}

void ReadableStreamDefaultController::ResetQueue() {
  Environment* env = this->env();
  HandleScope scope(env->isolate());
  queue_.Reset();
  sizes_.clear();
  queue_head_ = 0;
  queue_size_ = 0;
  queue_total_size_ = 0;
}

Maybe<void> ReadableStreamDefaultController::EnqueueValueWithSize(
    Local<Value> value, double size) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  if (std::isnan(size) || size < 0 || size == std::numeric_limits<double>::infinity()) {
    isolate->ThrowException(
        MakeCodedError(isolate, context, /* range */ true,
                       "ERR_INVALID_ARG_VALUE",
                       "The argument 'size' is invalid"));
    return Nothing<void>();
  }
  Local<Array> queue = queue_.Get(isolate);
  if (queue.IsEmpty()) {
    queue = Array::New(isolate);
    queue_.Reset(isolate, queue);
  }
  uint32_t tail = queue_head_ + queue_size_;
  if (queue->Set(context, tail, value).IsNothing()) return Nothing<void>();
  sizes_.push_back(size);
  queue_size_++;
  queue_total_size_ += size;
  return JustVoid();
}

MaybeLocal<Value> ReadableStreamDefaultController::DequeueValue() {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  CHECK_GT(queue_size_, 0u);
  Local<Array> queue = queue_.Get(isolate);
  Local<Value> value;
  if (!queue->Get(context, queue_head_).ToLocal(&value)) return MaybeLocal<Value>();
  // Release the consumed slot for GC.
  USE(queue->Set(context, queue_head_, Undefined(isolate)));
  queue_head_++;
  double size = sizes_.front();
  sizes_.pop_front();
  queue_size_--;
  queue_total_size_ -= size;
  if (queue_total_size_ < 0) queue_total_size_ = 0;
  if (queue_size_ == 0) {
    // Compact: drop the backing array so it does not grow without bound.
    queue_.Reset();
    queue_head_ = 0;
  }
  return value;
}

Maybe<void> ReadableStreamDefaultController::EnqueueInternal(
    Local<Value> chunk) {
  if (!CanCloseOrEnqueue()) return JustVoid();
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  ReadableStream* stream = this->stream();

  if (stream->locked() && stream->has_default_reader() &&
      stream->default_reader()->num_read_requests() > 0) {
    stream->default_reader()->FulfillFront(chunk, false);
  } else {
    TryCatch try_catch(isolate);
    // Compute the chunk size.
    double size = 1;
    bool ok = true;
    if (size_mode_ == SizeMode::kUserFn) {
      Local<Function> size_fn = size_algorithm_.Get(isolate);
      Local<Value> argv[] = {chunk};
      Local<Value> size_val;
      if (!size_fn->Call(context, Undefined(isolate), 1, argv)
               .ToLocal(&size_val)) {
        ok = false;
      } else if (!size_val->NumberValue(context).To(&size)) {
        ok = false;
      }
    } else if (size_mode_ == SizeMode::kByteLength) {
      Local<Object> obj;
      Local<Value> bl;
      if (!chunk->ToObject(context).ToLocal(&obj) ||
          !obj->Get(context, FIXED_ONE_BYTE_STRING(isolate, "byteLength"))
               .ToLocal(&bl) ||
          !bl->NumberValue(context).To(&size)) {
        ok = false;
      }
    }
    if (ok) {
      if (EnqueueValueWithSize(chunk, size).IsNothing()) ok = false;
    }
    if (!ok) {
      Local<Value> exception = try_catch.Exception();
      try_catch.Reset();
      ErrorInternal(exception);
      isolate->ThrowException(exception);
      return Nothing<void>();
    }
  }
  CallPullIfNeeded();
  return JustVoid();
}

void ReadableStreamDefaultController::ErrorInternal(Local<Value> error) {
  ReadableStream* stream = this->stream();
  if (stream->state() != StreamState::kReadable) return;
  ResetQueue();
  ClearAlgorithms();
  stream->ErrorStream(error);
}

Local<Promise> ReadableStreamDefaultController::CancelSteps(
    Local<Value> reason) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  ResetQueue();
  Local<Function> cancel = cancel_algorithm_.Get(isolate);
  Local<Value> argv[] = {reason};
  Local<Value> result;
  Local<Promise> promise;
  if (cancel->Call(context, Undefined(isolate), 1, argv).ToLocal(&result) &&
      result->IsPromise()) {
    promise = result.As<Promise>();
  } else {
    // Fallback: a resolved promise.
    Local<Promise::Resolver> resolver;
    if (Promise::Resolver::New(context).ToLocal(&resolver)) {
      USE(resolver->Resolve(context, Undefined(isolate)));
      promise = resolver->GetPromise();
    }
  }
  ClearAlgorithms();
  return promise;
}

void ReadableStreamDefaultController::PullSteps(
    Local<Promise::Resolver> resolver) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  ReadableStream* stream = this->stream();
  if (queue_size_ > 0) {
    Local<Value> chunk;
    if (!DequeueValue().ToLocal(&chunk)) return;
    if (close_requested_ && queue_size_ == 0) {
      ClearAlgorithms();
      stream->Close();
    } else {
      CallPullIfNeeded();
    }
    USE(resolver->Resolve(context, MakeReadResult(isolate, context, chunk, false)));
    return;
  }
  // No buffered chunk: park the read request and ask for more.
  stream->default_reader()->AddReadRequest(resolver);
  CallPullIfNeeded();
}

bool ReadableStreamDefaultController::Setup(Local<Function> start_algorithm,
                                            Local<Function> pull_algorithm,
                                            Local<Function> cancel_algorithm,
                                            double high_water_mark,
                                            SizeMode size_mode,
                                            Local<Function> size_algorithm) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  high_water_mark_ = high_water_mark;
  size_mode_ = size_mode;
  pull_algorithm_.Reset(isolate, pull_algorithm);
  cancel_algorithm_.Reset(isolate, cancel_algorithm);
  if (size_mode == SizeMode::kUserFn)
    size_algorithm_.Reset(isolate, size_algorithm);

  // Run the start algorithm; on fulfillment mark started and pull. A synchronous
  // throw propagates to the caller (i.e. the ReadableStream constructor).
  Local<Object> controller_obj = object();
  Local<Value> start_result;
  Local<Value> argv[] = {controller_obj};
  if (!start_algorithm->Call(context, Undefined(isolate), 1, argv)
           .ToLocal(&start_result)) {
    return false;  // exception pending
  }
  Local<Promise::Resolver> resolver;
  if (!Promise::Resolver::New(context).ToLocal(&resolver)) return false;
  USE(resolver->Resolve(context, start_result));
  ThenReact(env, resolver->GetPromise(), controller_obj, ReactStartFulfilled);
  return true;
}

void ReadableStreamDefaultController::GetDesiredSize(
    const FunctionCallbackInfo<Value>& args) {
  auto* c = BaseObject::FromJSObject<ReadableStreamDefaultController>(
      args.This());
  Environment* env = Environment::GetCurrent(args);
  if (c == nullptr) return;
  bool is_null = false;
  double desired = c->GetDesiredSizeValue(&is_null);
  if (is_null) {
    args.GetReturnValue().SetNull();
  } else {
    args.GetReturnValue().Set(desired);
  }
}

void ReadableStreamDefaultController::Close(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  auto* c = BaseObject::FromJSObject<ReadableStreamDefaultController>(
      args.This());
  if (c == nullptr) return;
  if (!c->CanCloseOrEnqueue()) {
    args.GetIsolate()->ThrowException(InvalidStateError(
        env->isolate(), env->context(), "Controller is already closed"));
    return;
  }
  c->CloseInternal();
}

void ReadableStreamDefaultController::Enqueue(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  auto* c = BaseObject::FromJSObject<ReadableStreamDefaultController>(
      args.This());
  if (c == nullptr) return;
  if (!c->CanCloseOrEnqueue()) {
    args.GetIsolate()->ThrowException(InvalidStateError(
        env->isolate(), env->context(), "Controller is already closed"));
    return;
  }
  Local<Value> chunk = args[0];
  USE(c->EnqueueInternal(chunk));  // leaves exception pending on failure
}

void ReadableStreamDefaultController::Error(
    const FunctionCallbackInfo<Value>& args) {
  auto* c = BaseObject::FromJSObject<ReadableStreamDefaultController>(
      args.This());
  if (c == nullptr) return;
  c->ErrorInternal(args[0]);
}

// ===========================================================================
// ReadableStreamDefaultReader
// ===========================================================================

ReadableStreamDefaultReader::ReadableStreamDefaultReader(Environment* env,
                                                         Local<Object> object)
    : StreamBaseObject(env, object, Kind::kDefaultReader) {}

void ReadableStreamDefaultReader::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("closed", closed_);
}

ReadableStream* ReadableStreamDefaultReader::stream() const {
  return GetStreamField<ReadableStream>(
      object(), kStream, Kind::kStream);
}

bool ReadableStreamDefaultReader::has_stream() const {
  return stream() != nullptr;
}

Local<FunctionTemplate> ReadableStreamDefaultReader::GetConstructorTemplate(
    Environment* env) {
  BindingData* bd = BindingData::Get(env);
  Isolate* isolate = env->isolate();
  Local<FunctionTemplate> tmpl =
      bd->readable_stream_default_reader_ctor.Get(isolate);
  if (tmpl.IsEmpty()) {
    tmpl = NewFunctionTemplate(isolate, nullptr);
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        ReadableStreamDefaultReader::kInternalFieldCount);
    tmpl->SetClassName(
        FIXED_ONE_BYTE_STRING(isolate, "ReadableStreamDefaultReader"));
    Local<Signature> sig = Signature::New(isolate, tmpl);
    tmpl->PrototypeTemplate()->SetAccessorProperty(
        FIXED_ONE_BYTE_STRING(isolate, "closed"),
        FunctionTemplate::New(isolate, GetClosed, Local<Value>(), sig, 0,
                              v8::ConstructorBehavior::kThrow,
                              v8::SideEffectType::kHasNoSideEffect));
    SetProtoMethod(isolate, tmpl, "read", Read);
    SetProtoMethod(isolate, tmpl, "releaseLock", ReleaseLock);
    SetProtoMethod(isolate, tmpl, "cancel", Cancel);
    bd->readable_stream_default_reader_ctor.Reset(isolate, tmpl);
  }
  return tmpl;
}

void ReadableStreamDefaultReader::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(Read);
  registry->Register(ReleaseLock);
  registry->Register(Cancel);
  registry->Register(GetClosed);
}

void ReadableStreamDefaultReader::AddReadRequest(
    Local<Promise::Resolver> resolver) {
  read_requests_.emplace_back(env()->isolate(), resolver);
}

Local<Promise::Resolver> ReadableStreamDefaultReader::TakeReadRequest() {
  CHECK(!read_requests_.empty());
  Local<Promise::Resolver> resolver = read_requests_.front().Get(env()->isolate());
  read_requests_.pop_front();
  return resolver;
}

void ReadableStreamDefaultReader::FulfillFront(Local<Value> chunk, bool done) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  CHECK(!read_requests_.empty());
  Local<Promise::Resolver> resolver = read_requests_.front().Get(isolate);
  read_requests_.pop_front();
  Local<Value> value = done ? Undefined(isolate).As<Value>() : chunk;
  USE(resolver->Resolve(context, MakeReadResult(isolate, context, value, done)));
}

void ReadableStreamDefaultReader::CloseAll() {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  while (!read_requests_.empty()) {
    Local<Promise::Resolver> resolver = read_requests_.front().Get(isolate);
    read_requests_.pop_front();
    USE(resolver->Resolve(
        context, MakeReadResult(isolate, context, Undefined(isolate), true)));
  }
}

void ReadableStreamDefaultReader::ErrorAll(Local<Value> error) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  while (!read_requests_.empty()) {
    Local<Promise::Resolver> resolver = read_requests_.front().Get(isolate);
    read_requests_.pop_front();
    USE(resolver->Reject(context, error));
  }
}

Local<Promise> ReadableStreamDefaultReader::closed_promise(Environment* env) {
  Isolate* isolate = env->isolate();
  Local<Promise::Resolver> resolver = closed_.Get(isolate);
  if (resolver.IsEmpty()) {
    // Should have been created in SetupInternal; create defensively.
    if (!Promise::Resolver::New(env->context()).ToLocal(&resolver))
      return Local<Promise>();
    closed_.Reset(isolate, resolver);
  }
  return resolver->GetPromise();
}

void ReadableStreamDefaultReader::ResolveClosed() {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Promise::Resolver> resolver = closed_.Get(isolate);
  if (resolver.IsEmpty()) return;
  USE(resolver->Resolve(env->context(), Undefined(isolate)));
}

void ReadableStreamDefaultReader::RejectClosed(Local<Value> error) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Promise::Resolver> resolver = closed_.Get(isolate);
  if (resolver.IsEmpty()) return;
  Local<Promise> promise = resolver->GetPromise();
  USE(resolver->Reject(env->context(), error));
  MarkHandled(env, promise);
}

bool ReadableStreamDefaultReader::SetupInternal(Local<Object> stream_obj) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  auto* stream = BaseObject::FromJSObject<ReadableStream>(stream_obj);
  CHECK_NOT_NULL(stream);
  if (stream->locked()) {
    isolate->ThrowException(InvalidStateError(
        isolate, context, "ReadableStream is locked"));
    return false;
  }
  // Wire reader<->stream via GC-traced internal fields.
  object()->SetInternalField(kStream, stream_obj);
  stream_obj->SetInternalField(ReadableStream::kReader, object());

  Local<Promise::Resolver> resolver;
  if (!Promise::Resolver::New(context).ToLocal(&resolver)) return false;
  closed_.Reset(isolate, resolver);
  switch (stream->state()) {
    case StreamState::kReadable:
      break;
    case StreamState::kClosed:
      USE(resolver->Resolve(context, Undefined(isolate)));
      break;
    case StreamState::kErrored:
      USE(resolver->Reject(context, stream->stored_error(env)));
      MarkHandled(env, resolver->GetPromise());
      break;
  }
  return true;
}

void ReadableStreamDefaultReader::Release() {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  ReadableStream* stream = this->stream();
  CHECK_NOT_NULL(stream);

  // Reject closed with a "reader released" error.
  Local<Value> released_error = MakeCodedError(
      isolate, context, /* range */ false, "ERR_INVALID_STATE",
      "Reader was released and can no longer be used to monitor the stream's "
      "state");
  Local<Promise::Resolver> resolver = closed_.Get(isolate);
  if (!resolver.IsEmpty()) {
    if (stream->state() == StreamState::kReadable) {
      USE(resolver->Reject(context, released_error));
    } else {
      // Replace with an already-rejected promise.
      Local<Promise::Resolver> r;
      if (Promise::Resolver::New(context).ToLocal(&r)) {
        USE(r->Reject(context, released_error));
        closed_.Reset(isolate, r);
        resolver = r;
      }
    }
    MarkHandled(env, resolver->GetPromise());
  }

  // Unlink stream<->reader.
  object()->SetInternalField(kStream, Undefined(isolate));
  stream->object()->SetInternalField(ReadableStream::kReader,
                                     Undefined(isolate));

  // Error any outstanding read requests with the releasing error.
  Local<Value> releasing_error = MakeCodedError(
      isolate, context, /* range */ false, "ERR_INVALID_STATE",
      "The reader is not attached to a stream");
  ErrorAll(releasing_error);
}

void ReadableStreamDefaultReader::Read(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  auto* reader =
      BaseObject::FromJSObject<ReadableStreamDefaultReader>(args.This());
  if (reader == nullptr) return;
  if (!reader->has_stream()) {
    Local<Promise::Resolver> resolver;
    if (Promise::Resolver::New(context).ToLocal(&resolver)) {
      USE(resolver->Reject(
          context, InvalidStateError(isolate, context,
                                     "The reader is not attached to a stream")));
      args.GetReturnValue().Set(resolver->GetPromise());
    }
    return;
  }

  ReadableStream* stream = reader->stream();
  stream->set_disturbed(true);

  Local<Promise::Resolver> resolver;
  if (!Promise::Resolver::New(context).ToLocal(&resolver)) return;
  args.GetReturnValue().Set(resolver->GetPromise());

  switch (stream->state()) {
    case StreamState::kClosed:
      USE(resolver->Resolve(
          context, MakeReadResult(isolate, context, Undefined(isolate), true)));
      return;
    case StreamState::kErrored:
      USE(resolver->Reject(context, stream->stored_error(env)));
      return;
    case StreamState::kReadable:
      if (stream->has_byte_controller())
        stream->byte_controller()->PullStepsDefault(resolver);
      else
        stream->default_controller()->PullSteps(resolver);
      return;
  }
}

void ReadableStreamDefaultReader::ReleaseLock(
    const FunctionCallbackInfo<Value>& args) {
  auto* reader =
      BaseObject::FromJSObject<ReadableStreamDefaultReader>(args.This());
  if (reader == nullptr) return;
  if (!reader->has_stream()) return;
  reader->Release();
}

void ReadableStreamDefaultReader::Cancel(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  auto* reader =
      BaseObject::FromJSObject<ReadableStreamDefaultReader>(args.This());
  if (reader == nullptr) return;
  if (!reader->has_stream()) {
    Local<Promise::Resolver> resolver;
    if (Promise::Resolver::New(context).ToLocal(&resolver)) {
      USE(resolver->Reject(
          context, InvalidStateError(isolate, context,
                                     "The reader is not attached to a stream")));
      args.GetReturnValue().Set(resolver->GetPromise());
    }
    return;
  }
  args.GetReturnValue().Set(reader->stream()->CancelInternal(args[0]));
}

void ReadableStreamDefaultReader::GetClosed(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  auto* reader =
      BaseObject::FromJSObject<ReadableStreamDefaultReader>(args.This());
  if (reader == nullptr) return;
  args.GetReturnValue().Set(reader->closed_promise(env));
}

// ===========================================================================
// ReadableStream
// ===========================================================================

ReadableStream::ReadableStream(Environment* env, Local<Object> object)
    : StreamBaseObject(env, object, Kind::kStream) {}

void ReadableStream::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("stored_error", stored_error_);
  tracker->TrackField("closed", closed_);
}

Local<FunctionTemplate> ReadableStream::GetConstructorTemplate(
    Environment* env) {
  BindingData* bd = BindingData::Get(env);
  Isolate* isolate = env->isolate();
  Local<FunctionTemplate> tmpl = bd->readable_stream_ctor.Get(isolate);
  if (tmpl.IsEmpty()) {
    tmpl = NewFunctionTemplate(isolate, nullptr);
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        ReadableStream::kInternalFieldCount);
    tmpl->SetClassName(FIXED_ONE_BYTE_STRING(isolate, "ReadableStream"));
    Local<Signature> sig = Signature::New(isolate, tmpl);
    tmpl->PrototypeTemplate()->SetAccessorProperty(
        FIXED_ONE_BYTE_STRING(isolate, "locked"),
        FunctionTemplate::New(isolate, GetLocked, Local<Value>(), sig, 0,
                              v8::ConstructorBehavior::kThrow,
                              v8::SideEffectType::kHasNoSideEffect));
    SetProtoMethod(isolate, tmpl, "cancel", Cancel);
    bd->readable_stream_ctor.Reset(isolate, tmpl);
  }
  return tmpl;
}

void ReadableStream::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(GetLocked);
  registry->Register(Cancel);
}

bool ReadableStream::HasInstance(Environment* env, Local<Value> value) {
  return GetConstructorTemplate(env)->HasInstance(value);
}

Local<Value> ReadableStream::stored_error(Environment* env) const {
  if (stored_error_.IsEmpty()) return Undefined(env->isolate());
  return stored_error_.Get(env->isolate());
}

void ReadableStream::set_stored_error(Isolate* isolate, Local<Value> e) {
  stored_error_.Reset(isolate, e);
}

ReadableStreamDefaultController* ReadableStream::default_controller() const {
  return GetStreamField<ReadableStreamDefaultController>(
      object(), kController, Kind::kDefaultController);
}

ReadableByteStreamController* ReadableStream::byte_controller() const {
  return GetStreamField<ReadableByteStreamController>(
      object(), kController, Kind::kByteController);
}

bool ReadableStream::has_byte_controller() const {
  return byte_controller() != nullptr;
}

StreamBaseObject* ReadableStream::generic_reader() const {
  Local<Value> v = object()->GetInternalField(kReader).As<Value>();
  if (!v->IsObject()) return nullptr;
  BaseObject* base = BaseObject::FromJSObject(v.As<Object>());
  return static_cast<StreamBaseObject*>(base);
}

ReadableStreamDefaultReader* ReadableStream::default_reader() const {
  return GetStreamField<ReadableStreamDefaultReader>(
      object(), kReader, Kind::kDefaultReader);
}

ReadableStreamBYOBReader* ReadableStream::byob_reader() const {
  return GetStreamField<ReadableStreamBYOBReader>(
      object(), kReader, Kind::kByobReader);
}

bool ReadableStream::locked() const {
  Local<Value> v = object()->GetInternalField(kReader).As<Value>();
  return v->IsObject();
}

bool ReadableStream::has_default_reader() const {
  return default_reader() != nullptr;
}

bool ReadableStream::has_byob_reader() const {
  return byob_reader() != nullptr;
}

void ReadableStream::SetController(Local<Object> controller_obj, bool is_byte) {
  object()->SetInternalField(kController, controller_obj);
  // kStream is the first internal field of both controller kinds.
  controller_obj->SetInternalField(ReadableStreamDefaultController::kStream,
                                   object());
}

Local<Promise> ReadableStream::closed_promise(Environment* env) {
  Isolate* isolate = env->isolate();
  Local<Promise::Resolver> resolver = closed_.Get(isolate);
  if (resolver.IsEmpty()) {
    if (!Promise::Resolver::New(env->context()).ToLocal(&resolver))
      return Local<Promise>();
    closed_.Reset(isolate, resolver);
  }
  return resolver->GetPromise();
}

Local<Promise> ReadableStream::CancelInternal(Local<Value> reason) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  set_disturbed(true);
  if (state_ == StreamState::kClosed) {
    Local<Promise::Resolver> resolver;
    if (!Promise::Resolver::New(context).ToLocal(&resolver))
      return Local<Promise>();
    USE(resolver->Resolve(context, Undefined(isolate)));
    return resolver->GetPromise();
  }
  if (state_ == StreamState::kErrored) {
    Local<Promise::Resolver> resolver;
    if (!Promise::Resolver::New(context).ToLocal(&resolver))
      return Local<Promise>();
    USE(resolver->Reject(context, stored_error(env)));
    Local<Promise> p = resolver->GetPromise();
    MarkHandled(env, p);
    return p;
  }
  Close();
  // A BYOB reader's pending read-into requests are closed (done) here, after the
  // stream is closed but before the controller cancel runs.
  if (has_byob_reader()) byob_reader()->CloseAll();
  Local<Promise> cancel_result = has_byte_controller()
                                     ? byte_controller()->CancelSteps(reason)
                                     : default_controller()->CancelSteps(reason);
  // Return cancel_result.then(() => undefined): a fulfil-only mapping to
  // undefined, with rejections propagating (mirrors readableStreamCancel).
  if (cancel_result.IsEmpty()) {
    Local<Promise::Resolver> resolver;
    if (!Promise::Resolver::New(context).ToLocal(&resolver))
      return Local<Promise>();
    USE(resolver->Resolve(context, Undefined(isolate)));
    return resolver->GetPromise();
  }
  Local<Function> noop;
  if (!Function::New(context, Noop).ToLocal(&noop)) return Local<Promise>();
  Local<Promise> result;
  if (!cancel_result->Then(context, noop).ToLocal(&result))
    return Local<Promise>();
  return result;
}

void ReadableStream::Close() {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  CHECK(state_ == StreamState::kReadable);
  state_ = StreamState::kClosed;
  // Resolve stream-level closed promise.
  {
    Local<Promise::Resolver> resolver = closed_.Get(isolate);
    if (!resolver.IsEmpty())
      USE(resolver->Resolve(env->context(), Undefined(isolate)));
  }
  StreamBaseObject* reader = generic_reader();
  if (reader == nullptr) return;
  // Resolve the reader's closed promise; only a default reader's pending read
  // requests are closed here (a BYOB reader's read-into requests are handled by
  // cancel / respond-in-closed-state, mirroring readableStreamClose).
  if (reader->stream_kind() == Kind::kDefaultReader) {
    auto* dr = static_cast<ReadableStreamDefaultReader*>(reader);
    dr->ResolveClosed();
    dr->CloseAll();
  } else {
    static_cast<ReadableStreamBYOBReader*>(reader)->ResolveClosed();
  }
}

void ReadableStream::ErrorStream(Local<Value> error) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  CHECK(state_ == StreamState::kReadable);
  state_ = StreamState::kErrored;
  set_stored_error(isolate, error);
  {
    Local<Promise::Resolver> resolver = closed_.Get(isolate);
    if (!resolver.IsEmpty()) {
      Local<Promise> p = resolver->GetPromise();
      USE(resolver->Reject(env->context(), error));
      MarkHandled(env, p);
    }
  }
  StreamBaseObject* reader = generic_reader();
  if (reader == nullptr) return;
  if (reader->stream_kind() == Kind::kDefaultReader) {
    auto* dr = static_cast<ReadableStreamDefaultReader*>(reader);
    dr->RejectClosed(error);
    dr->ErrorAll(error);
  } else {
    auto* br = static_cast<ReadableStreamBYOBReader*>(reader);
    br->RejectClosed(error);
    br->ErrorAll(error);
  }
}

void ReadableStream::GetLocked(const FunctionCallbackInfo<Value>& args) {
  auto* stream = BaseObject::FromJSObject<ReadableStream>(args.This());
  if (stream == nullptr) return;
  args.GetReturnValue().Set(stream->locked());
}

void ReadableStream::Cancel(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  auto* stream = BaseObject::FromJSObject<ReadableStream>(args.This());
  if (stream == nullptr) return;
  if (stream->locked()) {
    Local<Promise::Resolver> resolver;
    if (Promise::Resolver::New(context).ToLocal(&resolver)) {
      USE(resolver->Reject(
          context, InvalidStateError(isolate, context,
                                     "ReadableStream is locked")));
      args.GetReturnValue().Set(resolver->GetPromise());
    }
    return;
  }
  args.GetReturnValue().Set(stream->CancelInternal(args[0]));
}

// ===========================================================================
// ReadableByteStreamController
// ===========================================================================

ReadableByteStreamController::ReadableByteStreamController(Environment* env,
                                                          Local<Object> object)
    : StreamBaseObject(env, object, Kind::kByteController) {}

void ReadableByteStreamController::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("pull_algorithm", pull_algorithm_);
  tracker->TrackField("cancel_algorithm", cancel_algorithm_);
}

ReadableStream* ReadableByteStreamController::stream() const {
  return GetStreamField<ReadableStream>(object(), kStream, Kind::kStream);
}

ReadableStreamBYOBRequest* ReadableByteStreamController::byob_request_obj()
    const {
  return GetStreamField<ReadableStreamBYOBRequest>(
      object(), kByobRequest, Kind::kByobRequest);
}

Local<FunctionTemplate> ReadableByteStreamController::GetConstructorTemplate(
    Environment* env) {
  BindingData* bd = BindingData::Get(env);
  Isolate* isolate = env->isolate();
  Local<FunctionTemplate> tmpl =
      bd->readable_byte_stream_controller_ctor.Get(isolate);
  if (tmpl.IsEmpty()) {
    tmpl = NewFunctionTemplate(isolate, nullptr);
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        ReadableByteStreamController::kInternalFieldCount);
    tmpl->SetClassName(
        FIXED_ONE_BYTE_STRING(isolate, "ReadableByteStreamController"));
    Local<Signature> sig = Signature::New(isolate, tmpl);
    tmpl->PrototypeTemplate()->SetAccessorProperty(
        FIXED_ONE_BYTE_STRING(isolate, "byobRequest"),
        FunctionTemplate::New(isolate, GetByobRequest, Local<Value>(), sig, 0,
                              v8::ConstructorBehavior::kThrow,
                              v8::SideEffectType::kHasNoSideEffect));
    tmpl->PrototypeTemplate()->SetAccessorProperty(
        FIXED_ONE_BYTE_STRING(isolate, "desiredSize"),
        FunctionTemplate::New(isolate, GetDesiredSize, Local<Value>(), sig, 0,
                              v8::ConstructorBehavior::kThrow,
                              v8::SideEffectType::kHasNoSideEffect));
    SetProtoMethod(isolate, tmpl, "close", Close);
    SetProtoMethod(isolate, tmpl, "enqueue", Enqueue);
    SetProtoMethod(isolate, tmpl, "error", Error);
    bd->readable_byte_stream_controller_ctor.Reset(isolate, tmpl);
  }
  return tmpl;
}

void ReadableByteStreamController::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(GetByobRequest);
  registry->Register(GetDesiredSize);
  registry->Register(Close);
  registry->Register(Enqueue);
  registry->Register(Error);
}

double ReadableByteStreamController::GetDesiredSizeValue(bool* is_null) const {
  *is_null = false;
  switch (stream()->state()) {
    case StreamState::kErrored:
      *is_null = true;
      return 0;
    case StreamState::kClosed:
      return 0;
    default:
      return high_water_mark_ - queue_total_size_;
  }
}

bool ReadableByteStreamController::ShouldCallPull() const {
  ReadableStream* stream = this->stream();
  if (stream->state() != StreamState::kReadable || close_requested_ ||
      !started_) {
    return false;
  }
  if (stream->has_default_reader() &&
      stream->default_reader()->num_read_requests() > 0) {
    return true;
  }
  if (stream->has_byob_reader() &&
      stream->byob_reader()->num_read_into_requests() > 0) {
    return true;
  }
  bool is_null = false;
  double desired = GetDesiredSizeValue(&is_null);
  CHECK(!is_null);
  return desired > 0;
}

void ReadableByteStreamController::OnStartFulfilled() {
  started_ = true;
  CHECK(!pulling_);
  CHECK(!pull_again_);
  CallPullIfNeeded();
}

void ReadableByteStreamController::FinishPull() {
  pulling_ = false;
  if (pull_again_) {
    pull_again_ = false;
    CallPullIfNeeded();
  }
}

void ReadableByteStreamController::CallPullIfNeeded() {
  if (!ShouldCallPull()) return;
  if (pulling_) {
    pull_again_ = true;
    return;
  }
  CHECK(!pull_again_);
  pulling_ = true;
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  Local<Object> controller_obj = object();
  Local<Function> pull = pull_algorithm_.Get(isolate);
  Local<Value> argv[] = {controller_obj};
  Local<Value> result;
  if (!pull->Call(context, Undefined(isolate), 1, argv).ToLocal(&result))
    return;
  if (!result->IsPromise()) return;
  ThenReact(env, result.As<Promise>(), controller_obj, ReactPullFulfilled);
}

void ReadableByteStreamController::ClearAlgorithms() {
  pull_algorithm_.Reset();
  cancel_algorithm_.Reset();
}

void ReadableByteStreamController::InvalidateBYOBRequest() {
  ReadableStreamBYOBRequest* req = byob_request_obj();
  if (req == nullptr) return;
  Isolate* isolate = env()->isolate();
  req->object()->SetInternalField(ReadableStreamBYOBRequest::kController,
                                  Undefined(isolate));
  req->object()->SetInternalField(ReadableStreamBYOBRequest::kView,
                                  v8::Null(isolate));
  object()->SetInternalField(kByobRequest, Undefined(isolate));
}

void ReadableByteStreamController::ClearPendingPullIntos() {
  InvalidateBYOBRequest();
  pending_pull_intos_.clear();
}

Maybe<void> ReadableByteStreamController::CloseInternal() {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  ReadableStream* stream = this->stream();
  if (close_requested_ || stream->state() != StreamState::kReadable)
    return JustVoid();
  if (queue_total_size_ > 0) {
    close_requested_ = true;
    return JustVoid();
  }
  if (!pending_pull_intos_.empty()) {
    const PullIntoDescriptor& first = pending_pull_intos_.front();
    if (first.bytes_filled % first.element_size != 0) {
      Local<Value> error =
          InvalidStateError(isolate, context, "Partial read");
      ErrorInternal(error);
      isolate->ThrowException(error);
      return Nothing<void>();
    }
  }
  ClearAlgorithms();
  stream->Close();
  return JustVoid();
}

void ReadableByteStreamController::HandleQueueDrain() {
  CHECK(stream()->state() == StreamState::kReadable);
  if (queue_total_size_ == 0 && close_requested_) {
    ClearAlgorithms();
    stream()->Close();
    return;
  }
  CallPullIfNeeded();
}

void ReadableByteStreamController::EnqueueChunkToQueue(
    std::shared_ptr<BackingStore> buffer,
    size_t byte_offset,
    size_t byte_length) {
  queue_.push_back(ByteQueueEntry{std::move(buffer), byte_offset, byte_length});
  queue_total_size_ += byte_length;
}

void ReadableByteStreamController::EnqueueClonedChunkToQueue(
    const std::shared_ptr<BackingStore>& buffer,
    size_t byte_offset,
    size_t byte_length) {
  std::shared_ptr<BackingStore> clone =
      NewBackingStore(env()->isolate(), byte_length);
  if (byte_length > 0) {
    memcpy(clone->Data(),
           static_cast<char*>(buffer->Data()) + byte_offset,
           byte_length);
  }
  EnqueueChunkToQueue(std::move(clone), 0, byte_length);
}

void ReadableByteStreamController::EnqueueDetachedPullIntoToQueue(
    PullIntoDescriptor* desc) {
  CHECK(desc->type == PullIntoType::kNone);
  std::shared_ptr<BackingStore> buffer = desc->buffer;
  size_t byte_offset = desc->byte_offset;
  size_t bytes_filled = desc->bytes_filled;
  if (bytes_filled > 0)
    EnqueueClonedChunkToQueue(buffer, byte_offset, bytes_filled);
  // ShiftPendingPullInto (discard the front descriptor).
  CHECK(byob_request_obj() == nullptr);
  pending_pull_intos_.pop_front();
}

bool ReadableByteStreamController::FillPullIntoDescriptorFromQueue(
    PullIntoDescriptor* desc) {
  size_t element_size = desc->element_size;
  size_t max_bytes_to_copy = static_cast<size_t>(queue_total_size_);
  if (desc->byte_length - desc->bytes_filled < max_bytes_to_copy)
    max_bytes_to_copy = desc->byte_length - desc->bytes_filled;
  size_t max_bytes_filled = desc->bytes_filled + max_bytes_to_copy;
  size_t max_aligned_bytes = max_bytes_filled - (max_bytes_filled % element_size);
  size_t total_remaining = max_bytes_to_copy;
  bool ready = false;
  if (max_aligned_bytes >= desc->minimum_fill) {
    total_remaining = max_aligned_bytes - desc->bytes_filled;
    ready = true;
  }
  while (total_remaining > 0) {
    ByteQueueEntry& head = queue_.front();
    size_t bytes_to_copy =
        total_remaining < head.byte_length ? total_remaining : head.byte_length;
    size_t dest_start = desc->byte_offset + desc->bytes_filled;
    memcpy(static_cast<char*>(desc->buffer->Data()) + dest_start,
           static_cast<char*>(head.buffer->Data()) + head.byte_offset,
           bytes_to_copy);
    if (head.byte_length == bytes_to_copy) {
      queue_.pop_front();
    } else {
      head.byte_offset += bytes_to_copy;
      head.byte_length -= bytes_to_copy;
    }
    queue_total_size_ -= bytes_to_copy;
    desc->bytes_filled += bytes_to_copy;  // FillHeadPullIntoDescriptor
    total_remaining -= bytes_to_copy;
  }
  return ready;
}

void ReadableByteStreamController::FillReadRequestFromQueue(
    Local<Promise::Resolver> resolver) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  CHECK_GT(queue_total_size_, 0);
  ByteQueueEntry entry = queue_.front();
  queue_.pop_front();
  queue_total_size_ -= entry.byte_length;
  HandleQueueDrain();
  Local<ArrayBuffer> ab = ArrayBuffer::New(isolate, entry.buffer);
  Local<Object> view =
      Uint8Array::New(ab, entry.byte_offset, entry.byte_length).As<Object>();
  USE(resolver->Resolve(context,
                        MakeReadResult(isolate, context, view, false)));
}

void ReadableByteStreamController::ProcessReadRequestsUsingQueue() {
  ReadableStream* stream = this->stream();
  ReadableStreamDefaultReader* reader = stream->default_reader();
  CHECK_NOT_NULL(reader);
  while (reader->num_read_requests() > 0) {
    if (queue_total_size_ == 0) return;
    FillReadRequestFromQueue(reader->TakeReadRequest());
  }
}

std::deque<PullIntoDescriptor>
ReadableByteStreamController::ProcessPullIntoDescriptorsUsingQueue() {
  CHECK(!close_requested_);
  std::deque<PullIntoDescriptor> filled;
  while (!pending_pull_intos_.empty()) {
    if (queue_total_size_ == 0) break;
    PullIntoDescriptor* desc = &pending_pull_intos_.front();
    if (FillPullIntoDescriptorFromQueue(desc)) {
      CHECK(byob_request_obj() == nullptr);
      filled.push_back(std::move(pending_pull_intos_.front()));
      pending_pull_intos_.pop_front();
    }
  }
  return filled;
}

v8::MaybeLocal<Object> ReadableByteStreamController::ConvertPullIntoDescriptor(
    PullIntoDescriptor* desc) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  CHECK_LE(desc->bytes_filled, desc->byte_length);
  CHECK_EQ(desc->bytes_filled % desc->element_size, 0u);
  // Transfer: hand the owned BackingStore to a fresh user-visible ArrayBuffer.
  std::shared_ptr<BackingStore> bs = std::move(desc->buffer);
  Local<ArrayBuffer> ab = ArrayBuffer::New(isolate, bs);
  return MakeView(env, desc->view_type, ab, desc->byte_offset,
                  desc->bytes_filled / desc->element_size);
}

void ReadableByteStreamController::CommitPullIntoDescriptor(
    PullIntoDescriptor* desc) {
  ReadableStream* stream = this->stream();
  CHECK(stream->state() != StreamState::kErrored);
  CHECK(desc->type != PullIntoType::kNone);
  bool done = false;
  if (stream->state() == StreamState::kClosed) {
    CHECK_EQ(desc->bytes_filled % desc->element_size, 0u);
    done = true;
  }
  Local<Object> filled_view;
  if (!ConvertPullIntoDescriptor(desc).ToLocal(&filled_view)) return;
  if (desc->type == PullIntoType::kDefault) {
    stream->default_reader()->FulfillFront(filled_view, done);
  } else {
    CHECK(desc->type == PullIntoType::kByob);
    stream->byob_reader()->FulfillFront(filled_view, done);
  }
}

void ReadableByteStreamController::CommitPullIntoDescriptors(
    std::deque<PullIntoDescriptor>* descs) {
  for (size_t i = 0; i < descs->size(); ++i)
    CommitPullIntoDescriptor(&(*descs)[i]);
}

Maybe<void> ReadableByteStreamController::EnqueueInternal(Local<Object> view_obj) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  ReadableStream* stream = this->stream();
  Local<ArrayBufferView> view = view_obj.As<ArrayBufferView>();
  Local<ArrayBuffer> buffer = view->Buffer();
  size_t byte_offset = view->ByteOffset();
  size_t byte_length = view->ByteLength();

  if (close_requested_ || stream->state() != StreamState::kReadable)
    return JustVoid();

  std::shared_ptr<BackingStore> transferred;
  if (!TransferArrayBuffer(isolate, context, buffer, &transferred))
    return Nothing<void>();

  if (!pending_pull_intos_.empty()) {
    // The first pending pull-into's destination buffer is the BackingStore we
    // own; it cannot be detached out from under us. Invalidate the outstanding
    // BYOB request (which neutralizes any view the user is holding), then, if
    // the descriptor was detached (type 'none'), move its filled bytes to the
    // queue.
    InvalidateBYOBRequest();
    PullIntoDescriptor* first = &pending_pull_intos_.front();
    if (first->type == PullIntoType::kNone)
      EnqueueDetachedPullIntoToQueue(first);
  }

  if (stream->has_default_reader()) {
    ProcessReadRequestsUsingQueue();
    if (stream->default_reader()->num_read_requests() == 0) {
      EnqueueChunkToQueue(transferred, byte_offset, byte_length);
    } else {
      CHECK(queue_.empty());
      if (!pending_pull_intos_.empty()) {
        CHECK(pending_pull_intos_.front().type == PullIntoType::kDefault);
        CHECK(byob_request_obj() == nullptr);
        pending_pull_intos_.pop_front();  // ShiftPendingPullInto
      }
      Local<ArrayBuffer> ab = ArrayBuffer::New(isolate, transferred);
      Local<Object> tview =
          Uint8Array::New(ab, byte_offset, byte_length).As<Object>();
      stream->default_reader()->FulfillFront(tview, false);
    }
  } else if (stream->has_byob_reader()) {
    EnqueueChunkToQueue(transferred, byte_offset, byte_length);
    std::deque<PullIntoDescriptor> filled =
        ProcessPullIntoDescriptorsUsingQueue();
    CommitPullIntoDescriptors(&filled);
  } else {
    CHECK(!stream->locked());
    EnqueueChunkToQueue(transferred, byte_offset, byte_length);
  }
  CallPullIfNeeded();
  return JustVoid();
}

void ReadableByteStreamController::ErrorInternal(Local<Value> error) {
  ReadableStream* stream = this->stream();
  if (stream->state() != StreamState::kReadable) return;
  ClearPendingPullIntos();
  queue_.clear();
  queue_total_size_ = 0;
  ClearAlgorithms();
  stream->ErrorStream(error);
}

Local<Promise> ReadableByteStreamController::CancelSteps(Local<Value> reason) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  ClearPendingPullIntos();
  queue_.clear();
  queue_total_size_ = 0;
  Local<Function> cancel = cancel_algorithm_.Get(isolate);
  Local<Value> argv[] = {reason};
  Local<Value> result;
  Local<Promise> promise;
  if (cancel->Call(context, Undefined(isolate), 1, argv).ToLocal(&result) &&
      result->IsPromise()) {
    promise = result.As<Promise>();
  } else {
    Local<Promise::Resolver> resolver;
    if (Promise::Resolver::New(context).ToLocal(&resolver)) {
      USE(resolver->Resolve(context, Undefined(isolate)));
      promise = resolver->GetPromise();
    }
  }
  ClearAlgorithms();
  return promise;
}

void ReadableByteStreamController::PullStepsDefault(
    Local<Promise::Resolver> resolver) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  ReadableStream* stream = this->stream();
  CHECK(stream->has_default_reader());
  if (queue_total_size_ > 0) {
    CHECK_EQ(stream->default_reader()->num_read_requests(), 0u);
    FillReadRequestFromQueue(resolver);
    return;
  }
  if (has_auto_allocate_) {
    std::shared_ptr<BackingStore> bs =
        NewBackingStore(isolate, auto_allocate_chunk_size_);
    PullIntoDescriptor desc;
    desc.buffer = std::move(bs);
    desc.buffer_byte_length = auto_allocate_chunk_size_;
    desc.byte_offset = 0;
    desc.byte_length = auto_allocate_chunk_size_;
    desc.bytes_filled = 0;
    desc.minimum_fill = 1;
    desc.element_size = 1;
    desc.view_type = ViewType::kUint8Array;
    desc.type = PullIntoType::kDefault;
    pending_pull_intos_.push_back(std::move(desc));
  }
  stream->default_reader()->AddReadRequest(resolver);
  CallPullIfNeeded();
}

void ReadableByteStreamController::PullInto(Local<Object> view_obj,
                                           size_t min,
                                           Local<Promise::Resolver> resolver) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  ReadableStream* stream = this->stream();
  Local<ArrayBufferView> view = view_obj.As<ArrayBufferView>();
  uint32_t element_size = 1;
  ViewType view_type = ClassifyView(view, &element_size);
  size_t minimum_fill = min * element_size;
  Local<ArrayBuffer> buffer = view->Buffer();
  size_t byte_offset = view->ByteOffset();
  size_t byte_length = view->ByteLength();
  size_t buffer_byte_length = buffer->ByteLength();

  std::shared_ptr<BackingStore> transferred;
  {
    TryCatch try_catch(isolate);
    if (!TransferArrayBuffer(isolate, context, buffer, &transferred)) {
      Local<Value> exception = try_catch.Exception();
      try_catch.Reset();
      USE(resolver->Reject(context, exception));
      return;
    }
  }

  PullIntoDescriptor desc;
  desc.buffer = transferred;
  desc.buffer_byte_length = buffer_byte_length;
  desc.byte_offset = byte_offset;
  desc.byte_length = byte_length;
  desc.bytes_filled = 0;
  desc.minimum_fill = minimum_fill;
  desc.element_size = element_size;
  desc.view_type = view_type;
  desc.type = PullIntoType::kByob;

  if (!pending_pull_intos_.empty()) {
    pending_pull_intos_.push_back(std::move(desc));
    stream->byob_reader()->AddReadIntoRequest(resolver);
    return;
  }
  if (stream->state() == StreamState::kClosed) {
    Local<ArrayBuffer> ab = ArrayBuffer::New(isolate, desc.buffer);
    Local<Object> empty_view;
    if (!MakeView(env, view_type, ab, byte_offset, 0).ToLocal(&empty_view))
      return;
    USE(resolver->Resolve(
        context, MakeReadResult(isolate, context, empty_view, true)));
    return;
  }
  if (queue_total_size_ > 0) {
    if (FillPullIntoDescriptorFromQueue(&desc)) {
      Local<Object> filled_view;
      if (!ConvertPullIntoDescriptor(&desc).ToLocal(&filled_view)) return;
      HandleQueueDrain();
      USE(resolver->Resolve(
          context, MakeReadResult(isolate, context, filled_view, false)));
      return;
    }
    if (close_requested_) {
      Local<Value> error =
          InvalidStateError(isolate, context, "ReadableStream closed");
      ErrorInternal(error);
      USE(resolver->Reject(context, error));
      return;
    }
  }
  pending_pull_intos_.push_back(std::move(desc));
  stream->byob_reader()->AddReadIntoRequest(resolver);
  CallPullIfNeeded();
}

Maybe<void> ReadableByteStreamController::Respond(size_t bytes_written) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  CHECK(!pending_pull_intos_.empty());
  PullIntoDescriptor& desc = pending_pull_intos_.front();
  ReadableStream* stream = this->stream();
  if (stream->state() == StreamState::kClosed) {
    if (bytes_written != 0) {
      isolate->ThrowException(MakeCodedError(
          isolate, context, /* range */ false, "ERR_INVALID_ARG_VALUE",
          "bytesWritten must be 0 when the stream is closed"));
      return Nothing<void>();
    }
  } else {
    CHECK(stream->state() == StreamState::kReadable);
    if (bytes_written == 0) {
      isolate->ThrowException(MakeCodedError(
          isolate, context, /* range */ false, "ERR_INVALID_ARG_VALUE",
          "bytesWritten must be greater than 0"));
      return Nothing<void>();
    }
    if (desc.bytes_filled + bytes_written > desc.byte_length) {
      isolate->ThrowException(RangeCodedError(
          isolate, context, "bytesWritten is out of bounds"));
      return Nothing<void>();
    }
  }
  // desc.buffer is the BackingStore we own; the data the user wrote is already
  // in it. (The JS re-transfers here to detach the user's view; invalidating
  // the BYOB request in RespondInternal achieves the same.)
  return RespondInternal(bytes_written);
}

Maybe<void> ReadableByteStreamController::RespondWithNewView(
    Local<Object> view_obj) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  CHECK(!pending_pull_intos_.empty());
  PullIntoDescriptor& desc = pending_pull_intos_.front();
  ReadableStream* stream = this->stream();
  CHECK(stream->state() != StreamState::kErrored);
  Local<ArrayBufferView> view = view_obj.As<ArrayBufferView>();
  size_t view_byte_length = view->ByteLength();
  size_t view_byte_offset = view->ByteOffset();
  Local<ArrayBuffer> view_buffer = view->Buffer();
  size_t view_buffer_byte_length = view_buffer->ByteLength();

  if (stream->state() == StreamState::kClosed) {
    if (view_byte_length != 0) {
      isolate->ThrowException(
          InvalidStateError(isolate, context, "View is not zero-length"));
      return Nothing<void>();
    }
  } else {
    CHECK(stream->state() == StreamState::kReadable);
    if (view_byte_length == 0) {
      isolate->ThrowException(
          InvalidStateError(isolate, context, "View is zero-length"));
      return Nothing<void>();
    }
  }
  if (desc.byte_offset + desc.bytes_filled != view_byte_offset) {
    isolate->ThrowException(RangeCodedError(
        isolate, context, "The given view is not at the expected offset"));
    return Nothing<void>();
  }
  if (desc.bytes_filled + view_byte_length > desc.byte_length) {
    isolate->ThrowException(
        RangeCodedError(isolate, context, "The given view is too large"));
    return Nothing<void>();
  }
  if (desc.buffer_byte_length != view_buffer_byte_length) {
    isolate->ThrowException(RangeCodedError(
        isolate, context, "The given view has an unexpected buffer length"));
    return Nothing<void>();
  }
  std::shared_ptr<BackingStore> transferred;
  if (!TransferArrayBuffer(isolate, context, view_buffer, &transferred))
    return Nothing<void>();
  desc.buffer = std::move(transferred);
  return RespondInternal(view_byte_length);
}

Maybe<void> ReadableByteStreamController::RespondInternal(size_t bytes_written) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  PullIntoDescriptor* desc = &pending_pull_intos_.front();
  InvalidateBYOBRequest();
  ReadableStream* stream = this->stream();
  if (stream->state() == StreamState::kClosed) {
    if (bytes_written != 0) {
      isolate->ThrowException(InvalidStateError(
          isolate, context,
          "Controller is closed but view is not zero-length"));
      return Nothing<void>();
    }
    RespondInClosedState(desc);
  } else {
    CHECK(stream->state() == StreamState::kReadable);
    if (bytes_written == 0) {
      isolate->ThrowException(
          InvalidStateError(isolate, context, "View cannot be zero-length"));
      return Nothing<void>();
    }
    if (RespondInReadableState(bytes_written, desc).IsNothing())
      return Nothing<void>();
  }
  CallPullIfNeeded();
  return JustVoid();
}

void ReadableByteStreamController::RespondInClosedState(
    PullIntoDescriptor* desc) {
  CHECK_EQ(desc->bytes_filled % desc->element_size, 0u);
  if (desc->type == PullIntoType::kNone) {
    CHECK(byob_request_obj() == nullptr);
    pending_pull_intos_.pop_front();  // ShiftPendingPullInto
  }
  ReadableStream* stream = this->stream();
  if (stream->has_byob_reader()) {
    std::deque<PullIntoDescriptor> filled;
    size_t n = stream->byob_reader()->num_read_into_requests();
    for (size_t i = 0; i < n; ++i) {
      CHECK(byob_request_obj() == nullptr);
      filled.push_back(std::move(pending_pull_intos_.front()));
      pending_pull_intos_.pop_front();
    }
    CommitPullIntoDescriptors(&filled);
  }
}

Maybe<void> ReadableByteStreamController::RespondInReadableState(
    size_t bytes_written, PullIntoDescriptor* desc) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  if (desc->bytes_filled + bytes_written > desc->byte_length) {
    isolate->ThrowException(
        RangeCodedError(isolate, context, "The buffer size is invalid"));
    return Nothing<void>();
  }
  desc->bytes_filled += bytes_written;  // FillHeadPullIntoDescriptor

  if (desc->type == PullIntoType::kNone) {
    EnqueueDetachedPullIntoToQueue(desc);  // shifts the front descriptor
    std::deque<PullIntoDescriptor> filled =
        ProcessPullIntoDescriptorsUsingQueue();
    CommitPullIntoDescriptors(&filled);
    return JustVoid();
  }

  if (desc->bytes_filled < desc->minimum_fill) return JustVoid();

  // Shift the now-ready descriptor out before further mutation of the queue.
  CHECK(byob_request_obj() == nullptr);
  PullIntoDescriptor ready = std::move(pending_pull_intos_.front());
  pending_pull_intos_.pop_front();

  size_t remainder_size = ready.bytes_filled % ready.element_size;
  if (remainder_size > 0) {
    size_t end = ready.byte_offset + ready.bytes_filled;
    size_t start = end - remainder_size;
    EnqueueClonedChunkToQueue(ready.buffer, start, remainder_size);
  }
  ready.bytes_filled -= remainder_size;
  std::deque<PullIntoDescriptor> filled =
      ProcessPullIntoDescriptorsUsingQueue();
  CommitPullIntoDescriptor(&ready);
  CommitPullIntoDescriptors(&filled);
  return JustVoid();
}

void ReadableByteStreamController::OnReaderReleased() {
  if (pending_pull_intos_.empty()) return;
  PullIntoDescriptor first = std::move(pending_pull_intos_.front());
  first.type = PullIntoType::kNone;
  pending_pull_intos_.clear();
  pending_pull_intos_.push_back(std::move(first));
}

bool ReadableByteStreamController::Setup(Local<Function> start_algorithm,
                                        Local<Function> pull_algorithm,
                                        Local<Function> cancel_algorithm,
                                        double high_water_mark,
                                        size_t auto_allocate_chunk_size) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  high_water_mark_ = high_water_mark;
  if (auto_allocate_chunk_size > 0) {
    auto_allocate_chunk_size_ = auto_allocate_chunk_size;
    has_auto_allocate_ = true;
  }
  pull_algorithm_.Reset(isolate, pull_algorithm);
  cancel_algorithm_.Reset(isolate, cancel_algorithm);

  Local<Object> controller_obj = object();
  Local<Value> start_result;
  Local<Value> argv[] = {controller_obj};
  if (!start_algorithm->Call(context, Undefined(isolate), 1, argv)
           .ToLocal(&start_result)) {
    return false;
  }
  Local<Promise::Resolver> resolver;
  if (!Promise::Resolver::New(context).ToLocal(&resolver)) return false;
  USE(resolver->Resolve(context, start_result));
  ThenReact(env, resolver->GetPromise(), controller_obj, ReactStartFulfilled);
  return true;
}

void ReadableByteStreamController::GetByobRequest(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  auto* c =
      BaseObject::FromJSObject<ReadableByteStreamController>(args.This());
  if (c == nullptr) return;
  if (c->byob_request_obj() == nullptr && c->has_pending_pull_intos()) {
    const PullIntoDescriptor& d = c->first_pending_pull_into();
    Local<ArrayBuffer> ab = ArrayBuffer::New(isolate, d.buffer);
    Local<Object> view =
        Uint8Array::New(ab, d.byte_offset + d.bytes_filled,
                        d.byte_length - d.bytes_filled)
            .As<Object>();
    // Create the BYOBRequest and link it (GC-traced internal fields).
    Local<Function> ctor;
    if (!ReadableStreamBYOBRequest::GetConstructorTemplate(env)
             ->GetFunction(context)
             .ToLocal(&ctor))
      return;
    Local<Object> req_obj;
    if (!ctor->NewInstance(context).ToLocal(&req_obj)) return;
    BaseObjectPtr<ReadableStreamBYOBRequest> req =
        MakeBaseObject<ReadableStreamBYOBRequest>(env, req_obj);
    req->MakeWeak();
    req_obj->SetInternalField(ReadableStreamBYOBRequest::kController,
                              c->object());
    req_obj->SetInternalField(ReadableStreamBYOBRequest::kView, view);
    c->object()->SetInternalField(kByobRequest, req_obj);
    args.GetReturnValue().Set(req_obj);
    return;
  }
  Local<Value> cur = c->object()->GetInternalField(kByobRequest).As<Value>();
  if (cur->IsObject())
    args.GetReturnValue().Set(cur);
  else
    args.GetReturnValue().SetNull();
}

void ReadableByteStreamController::GetDesiredSize(
    const FunctionCallbackInfo<Value>& args) {
  auto* c =
      BaseObject::FromJSObject<ReadableByteStreamController>(args.This());
  if (c == nullptr) return;
  bool is_null = false;
  double desired = c->GetDesiredSizeValue(&is_null);
  if (is_null)
    args.GetReturnValue().SetNull();
  else
    args.GetReturnValue().Set(desired);
}

void ReadableByteStreamController::Close(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  auto* c =
      BaseObject::FromJSObject<ReadableByteStreamController>(args.This());
  if (c == nullptr) return;
  if (c->close_requested()) {
    isolate->ThrowException(
        InvalidStateError(isolate, context, "Controller is already closed"));
    return;
  }
  if (c->stream()->state() != StreamState::kReadable) {
    isolate->ThrowException(InvalidStateError(
        isolate, context, "ReadableStream is already closed"));
    return;
  }
  USE(c->CloseInternal());  // may leave a pending exception
}

void ReadableByteStreamController::Enqueue(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  auto* c =
      BaseObject::FromJSObject<ReadableByteStreamController>(args.This());
  if (c == nullptr) return;
  if (!args[0]->IsArrayBufferView()) {
    isolate->ThrowException(MakeCodedError(
        isolate, context, /* range */ false, "ERR_INVALID_ARG_TYPE",
        "chunk must be an ArrayBufferView"));
    return;
  }
  Local<ArrayBufferView> view = args[0].As<ArrayBufferView>();
  Local<ArrayBuffer> chunk_buffer = view->Buffer();
  size_t chunk_byte_length = view->ByteLength();
  size_t chunk_buffer_byte_length = chunk_buffer->ByteLength();
  if (chunk_byte_length == 0 || chunk_buffer_byte_length == 0) {
    isolate->ThrowException(InvalidStateError(
        isolate, context, "chunk ArrayBuffer is zero-length or detached"));
    return;
  }
  if (c->close_requested()) {
    isolate->ThrowException(
        InvalidStateError(isolate, context, "Controller is already closed"));
    return;
  }
  if (c->stream()->state() != StreamState::kReadable) {
    isolate->ThrowException(InvalidStateError(
        isolate, context, "ReadableStream is already closed"));
    return;
  }
  USE(c->EnqueueInternal(view.As<Object>()));  // may leave a pending exception
}

void ReadableByteStreamController::Error(
    const FunctionCallbackInfo<Value>& args) {
  auto* c =
      BaseObject::FromJSObject<ReadableByteStreamController>(args.This());
  if (c == nullptr) return;
  c->ErrorInternal(args[0]);
}

// ===========================================================================
// ReadableStreamBYOBRequest
// ===========================================================================

ReadableStreamBYOBRequest::ReadableStreamBYOBRequest(Environment* env,
                                                     Local<Object> object)
    : StreamBaseObject(env, object, Kind::kByobRequest) {}

void ReadableStreamBYOBRequest::MemoryInfo(MemoryTracker* tracker) const {}

ReadableByteStreamController* ReadableStreamBYOBRequest::controller() const {
  return GetStreamField<ReadableByteStreamController>(
      object(), kController, Kind::kByteController);
}

Local<FunctionTemplate> ReadableStreamBYOBRequest::GetConstructorTemplate(
    Environment* env) {
  BindingData* bd = BindingData::Get(env);
  Isolate* isolate = env->isolate();
  Local<FunctionTemplate> tmpl =
      bd->readable_stream_byob_request_ctor.Get(isolate);
  if (tmpl.IsEmpty()) {
    tmpl = NewFunctionTemplate(isolate, nullptr);
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        ReadableStreamBYOBRequest::kInternalFieldCount);
    tmpl->SetClassName(
        FIXED_ONE_BYTE_STRING(isolate, "ReadableStreamBYOBRequest"));
    Local<Signature> sig = Signature::New(isolate, tmpl);
    tmpl->PrototypeTemplate()->SetAccessorProperty(
        FIXED_ONE_BYTE_STRING(isolate, "view"),
        FunctionTemplate::New(isolate, GetView, Local<Value>(), sig, 0,
                              v8::ConstructorBehavior::kThrow,
                              v8::SideEffectType::kHasNoSideEffect));
    SetProtoMethod(isolate, tmpl, "respond", Respond);
    SetProtoMethod(isolate, tmpl, "respondWithNewView", RespondWithNewView);
    bd->readable_stream_byob_request_ctor.Reset(isolate, tmpl);
  }
  return tmpl;
}

void ReadableStreamBYOBRequest::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(GetView);
  registry->Register(Respond);
  registry->Register(RespondWithNewView);
}

void ReadableStreamBYOBRequest::GetView(
    const FunctionCallbackInfo<Value>& args) {
  auto* r = BaseObject::FromJSObject<ReadableStreamBYOBRequest>(args.This());
  if (r == nullptr) return;
  args.GetReturnValue().Set(r->object()->GetInternalField(kView).As<Value>());
}

void ReadableStreamBYOBRequest::Respond(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  auto* r = BaseObject::FromJSObject<ReadableStreamBYOBRequest>(args.This());
  if (r == nullptr) return;
  ReadableByteStreamController* c = r->controller();
  if (c == nullptr) {
    isolate->ThrowException(InvalidStateError(
        isolate, context, "This BYOB request has been invalidated"));
    return;
  }
  Local<Value> view_val = r->object()->GetInternalField(kView).As<Value>();
  if (view_val->IsArrayBufferView()) {
    Local<ArrayBuffer> view_buffer = view_val.As<ArrayBufferView>()->Buffer();
    if (view_buffer->WasDetached()) {
      isolate->ThrowException(InvalidStateError(
          isolate, context, "Viewed ArrayBuffer is detached"));
      return;
    }
  }
  int64_t bytes_written = 0;
  if (!args[0]->IntegerValue(context).To(&bytes_written)) return;
  if (bytes_written < 0) {
    isolate->ThrowException(RangeCodedError(
        isolate, context, "bytesWritten must be non-negative"));
    return;
  }
  USE(c->Respond(static_cast<size_t>(bytes_written)));
}

void ReadableStreamBYOBRequest::RespondWithNewView(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  auto* r = BaseObject::FromJSObject<ReadableStreamBYOBRequest>(args.This());
  if (r == nullptr) return;
  ReadableByteStreamController* c = r->controller();
  if (c == nullptr) {
    isolate->ThrowException(InvalidStateError(
        isolate, context, "This BYOB request has been invalidated"));
    return;
  }
  if (!args[0]->IsArrayBufferView()) {
    isolate->ThrowException(MakeCodedError(
        isolate, context, /* range */ false, "ERR_INVALID_ARG_TYPE",
        "view must be an ArrayBufferView"));
    return;
  }
  Local<ArrayBufferView> view = args[0].As<ArrayBufferView>();
  if (view->Buffer()->WasDetached()) {
    isolate->ThrowException(
        InvalidStateError(isolate, context, "Viewed ArrayBuffer is detached"));
    return;
  }
  USE(c->RespondWithNewView(view.As<Object>()));
}

// ===========================================================================
// ReadableStreamBYOBReader
// ===========================================================================

ReadableStreamBYOBReader::ReadableStreamBYOBReader(Environment* env,
                                                   Local<Object> object)
    : StreamBaseObject(env, object, Kind::kByobReader) {}

void ReadableStreamBYOBReader::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("closed", closed_);
}

ReadableStream* ReadableStreamBYOBReader::stream() const {
  return GetStreamField<ReadableStream>(object(), kStream, Kind::kStream);
}

bool ReadableStreamBYOBReader::has_stream() const {
  return stream() != nullptr;
}

Local<FunctionTemplate> ReadableStreamBYOBReader::GetConstructorTemplate(
    Environment* env) {
  BindingData* bd = BindingData::Get(env);
  Isolate* isolate = env->isolate();
  Local<FunctionTemplate> tmpl =
      bd->readable_stream_byob_reader_ctor.Get(isolate);
  if (tmpl.IsEmpty()) {
    tmpl = NewFunctionTemplate(isolate, nullptr);
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        ReadableStreamBYOBReader::kInternalFieldCount);
    tmpl->SetClassName(
        FIXED_ONE_BYTE_STRING(isolate, "ReadableStreamBYOBReader"));
    Local<Signature> sig = Signature::New(isolate, tmpl);
    tmpl->PrototypeTemplate()->SetAccessorProperty(
        FIXED_ONE_BYTE_STRING(isolate, "closed"),
        FunctionTemplate::New(isolate, GetClosed, Local<Value>(), sig, 0,
                              v8::ConstructorBehavior::kThrow,
                              v8::SideEffectType::kHasNoSideEffect));
    SetProtoMethod(isolate, tmpl, "read", Read);
    SetProtoMethod(isolate, tmpl, "releaseLock", ReleaseLock);
    SetProtoMethod(isolate, tmpl, "cancel", Cancel);
    bd->readable_stream_byob_reader_ctor.Reset(isolate, tmpl);
  }
  return tmpl;
}

void ReadableStreamBYOBReader::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(Read);
  registry->Register(ReleaseLock);
  registry->Register(Cancel);
  registry->Register(GetClosed);
}

void ReadableStreamBYOBReader::AddReadIntoRequest(
    Local<Promise::Resolver> resolver) {
  read_into_requests_.emplace_back(env()->isolate(), resolver);
}

void ReadableStreamBYOBReader::FulfillFront(Local<Value> view, bool done) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  CHECK(!read_into_requests_.empty());
  Local<Promise::Resolver> resolver = read_into_requests_.front().Get(isolate);
  read_into_requests_.pop_front();
  USE(resolver->Resolve(context, MakeReadResult(isolate, context, view, done)));
}

void ReadableStreamBYOBReader::CloseAll() {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  while (!read_into_requests_.empty()) {
    Local<Promise::Resolver> resolver = read_into_requests_.front().Get(isolate);
    read_into_requests_.pop_front();
    USE(resolver->Resolve(
        context, MakeReadResult(isolate, context, Undefined(isolate), true)));
  }
}

void ReadableStreamBYOBReader::ErrorAll(Local<Value> error) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  while (!read_into_requests_.empty()) {
    Local<Promise::Resolver> resolver = read_into_requests_.front().Get(isolate);
    read_into_requests_.pop_front();
    USE(resolver->Reject(context, error));
  }
}

Local<Promise> ReadableStreamBYOBReader::closed_promise(Environment* env) {
  Isolate* isolate = env->isolate();
  Local<Promise::Resolver> resolver = closed_.Get(isolate);
  if (resolver.IsEmpty()) {
    if (!Promise::Resolver::New(env->context()).ToLocal(&resolver))
      return Local<Promise>();
    closed_.Reset(isolate, resolver);
  }
  return resolver->GetPromise();
}

void ReadableStreamBYOBReader::ResolveClosed() {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Promise::Resolver> resolver = closed_.Get(isolate);
  if (resolver.IsEmpty()) return;
  USE(resolver->Resolve(env->context(), Undefined(isolate)));
}

void ReadableStreamBYOBReader::RejectClosed(Local<Value> error) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Promise::Resolver> resolver = closed_.Get(isolate);
  if (resolver.IsEmpty()) return;
  Local<Promise> promise = resolver->GetPromise();
  USE(resolver->Reject(env->context(), error));
  MarkHandled(env, promise);
}

bool ReadableStreamBYOBReader::SetupInternal(Local<Object> stream_obj) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  auto* stream = BaseObject::FromJSObject<ReadableStream>(stream_obj);
  CHECK_NOT_NULL(stream);
  if (stream->locked()) {
    isolate->ThrowException(
        InvalidStateError(isolate, context, "ReadableStream is locked"));
    return false;
  }
  if (!stream->has_byte_controller()) {
    isolate->ThrowException(MakeCodedError(
        isolate, context, /* range */ false, "ERR_INVALID_ARG_VALUE",
        "stream must be a byte stream"));
    return false;
  }
  object()->SetInternalField(kStream, stream_obj);
  stream_obj->SetInternalField(ReadableStream::kReader, object());

  Local<Promise::Resolver> resolver;
  if (!Promise::Resolver::New(context).ToLocal(&resolver)) return false;
  closed_.Reset(isolate, resolver);
  switch (stream->state()) {
    case StreamState::kReadable:
      break;
    case StreamState::kClosed:
      USE(resolver->Resolve(context, Undefined(isolate)));
      break;
    case StreamState::kErrored:
      USE(resolver->Reject(context, stream->stored_error(env)));
      MarkHandled(env, resolver->GetPromise());
      break;
  }
  return true;
}

void ReadableStreamBYOBReader::Release() {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  ReadableStream* stream = this->stream();
  CHECK_NOT_NULL(stream);

  Local<Value> released_error = MakeCodedError(
      isolate, context, /* range */ false, "ERR_INVALID_STATE",
      "Reader was released and can no longer be used to monitor the stream's "
      "state");
  Local<Promise::Resolver> resolver = closed_.Get(isolate);
  if (!resolver.IsEmpty()) {
    if (stream->state() == StreamState::kReadable) {
      USE(resolver->Reject(context, released_error));
    } else {
      Local<Promise::Resolver> r;
      if (Promise::Resolver::New(context).ToLocal(&r)) {
        USE(r->Reject(context, released_error));
        closed_.Reset(isolate, r);
        resolver = r;
      }
    }
    MarkHandled(env, resolver->GetPromise());
  }

  // controller[kRelease] runs before the stream<->reader link is broken.
  stream->byte_controller()->OnReaderReleased();

  object()->SetInternalField(kStream, Undefined(isolate));
  stream->object()->SetInternalField(ReadableStream::kReader,
                                     Undefined(isolate));

  Local<Value> releasing_error = MakeCodedError(
      isolate, context, /* range */ false, "ERR_INVALID_STATE",
      "The reader is not attached to a stream");
  ErrorAll(releasing_error);
}

void ReadableStreamBYOBReader::Read(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  auto* reader =
      BaseObject::FromJSObject<ReadableStreamBYOBReader>(args.This());
  if (reader == nullptr) return;

  Local<Promise::Resolver> resolver;
  if (!Promise::Resolver::New(context).ToLocal(&resolver)) return;
  args.GetReturnValue().Set(resolver->GetPromise());

  if (!args[0]->IsArrayBufferView()) {
    USE(resolver->Reject(
        context,
        MakeCodedError(isolate, context, /* range */ false,
                       "ERR_INVALID_ARG_TYPE",
                       "view must be an ArrayBufferView")));
    return;
  }
  Local<ArrayBufferView> view = args[0].As<ArrayBufferView>();
  Local<ArrayBuffer> view_buffer = view->Buffer();
  size_t view_byte_length = view->ByteLength();
  size_t view_buffer_byte_length = view_buffer->ByteLength();
  if (view_byte_length == 0 || view_buffer_byte_length == 0) {
    USE(resolver->Reject(
        context, InvalidStateError(
                     isolate, context,
                     "View or Viewed ArrayBuffer is zero-length or detached")));
    return;
  }

  // options.min (default 1).
  int64_t min = 1;
  if (args[1]->IsObject()) {
    Local<Value> min_val;
    if (!args[1].As<Object>()
             ->Get(context, FIXED_ONE_BYTE_STRING(isolate, "min"))
             .ToLocal(&min_val))
      return;
    if (!min_val->IsUndefined()) {
      if (!min_val->IntegerValue(context).To(&min)) return;
    }
  }
  if (min <= 0) {
    USE(resolver->Reject(
        context, MakeCodedError(isolate, context, /* range */ true,
                                "ERR_INVALID_ARG_VALUE",
                                "options.min must be greater than 0")));
    return;
  }
  // min must not exceed the element capacity of the view.
  uint32_t element_size = 1;
  ClassifyView(view, &element_size);
  size_t element_capacity = view_byte_length / element_size;
  if (static_cast<size_t>(min) > element_capacity) {
    USE(resolver->Reject(
        context, MakeCodedError(isolate, context, /* range */ true,
                                "ERR_OUT_OF_RANGE",
                                "options.min is out of range")));
    return;
  }

  if (!reader->has_stream()) {
    USE(resolver->Reject(
        context, InvalidStateError(isolate, context,
                                   "The reader is not attached to a stream")));
    return;
  }

  ReadableStream* stream = reader->stream();
  stream->set_disturbed(true);
  if (stream->state() == StreamState::kErrored) {
    USE(resolver->Reject(context, stream->stored_error(env)));
    return;
  }
  stream->byte_controller()->PullInto(
      view.As<Object>(), static_cast<size_t>(min), resolver);
}

void ReadableStreamBYOBReader::ReleaseLock(
    const FunctionCallbackInfo<Value>& args) {
  auto* reader =
      BaseObject::FromJSObject<ReadableStreamBYOBReader>(args.This());
  if (reader == nullptr) return;
  if (!reader->has_stream()) return;
  reader->Release();
}

void ReadableStreamBYOBReader::Cancel(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  auto* reader =
      BaseObject::FromJSObject<ReadableStreamBYOBReader>(args.This());
  if (reader == nullptr) return;
  if (!reader->has_stream()) {
    Local<Promise::Resolver> resolver;
    if (Promise::Resolver::New(context).ToLocal(&resolver)) {
      USE(resolver->Reject(
          context, InvalidStateError(isolate, context,
                                     "The reader is not attached to a stream")));
      args.GetReturnValue().Set(resolver->GetPromise());
    }
    return;
  }
  args.GetReturnValue().Set(reader->stream()->CancelInternal(args[0]));
}

void ReadableStreamBYOBReader::GetClosed(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  auto* reader =
      BaseObject::FromJSObject<ReadableStreamBYOBReader>(args.This());
  if (reader == nullptr) return;
  args.GetReturnValue().Set(reader->closed_promise(env));
}

// ===========================================================================
// Binding entry points
// ===========================================================================

void CreateReadableStream(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  // args: (startAlgorithm, pullAlgorithm, cancelAlgorithm, highWaterMark,
  //        sizeMode, sizeAlgorithm)
  CHECK(args[0]->IsFunction());
  CHECK(args[1]->IsFunction());
  CHECK(args[2]->IsFunction());
  CHECK(args[3]->IsNumber());
  CHECK(args[4]->IsUint32());
  Local<Function> start_algorithm = args[0].As<Function>();
  Local<Function> pull_algorithm = args[1].As<Function>();
  Local<Function> cancel_algorithm = args[2].As<Function>();
  double high_water_mark = args[3].As<Number>()->Value();
  SizeMode size_mode = static_cast<SizeMode>(args[4].As<v8::Uint32>()->Value());
  Local<Function> size_algorithm;
  if (args[5]->IsFunction()) size_algorithm = args[5].As<Function>();

  // Create the stream object.
  Local<Function> stream_ctor;
  if (!ReadableStream::GetConstructorTemplate(env)
           ->GetFunction(context)
           .ToLocal(&stream_ctor))
    return;
  Local<Object> stream_obj;
  if (!stream_ctor->NewInstance(context).ToLocal(&stream_obj)) return;
  BaseObjectPtr<ReadableStream> stream =
      MakeBaseObject<ReadableStream>(env, stream_obj);
  stream->MakeWeak();

  // Create the controller object.
  Local<Function> controller_ctor;
  if (!ReadableStreamDefaultController::GetConstructorTemplate(env)
           ->GetFunction(context)
           .ToLocal(&controller_ctor))
    return;
  Local<Object> controller_obj;
  if (!controller_ctor->NewInstance(context).ToLocal(&controller_obj)) return;
  BaseObjectPtr<ReadableStreamDefaultController> controller =
      MakeBaseObject<ReadableStreamDefaultController>(env, controller_obj);
  controller->MakeWeak();

  // Link stream <-> controller (GC-traced internal fields).
  stream->SetController(controller_obj, /* is_byte */ false);

  if (!controller->Setup(start_algorithm, pull_algorithm, cancel_algorithm,
                         high_water_mark, size_mode, size_algorithm)) {
    return;  // start threw synchronously; exception is pending.
  }

  args.GetReturnValue().Set(stream_obj);
}

void CreateReadableByteStream(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  // args: (startAlgorithm, pullAlgorithm, cancelAlgorithm, highWaterMark,
  //        autoAllocateChunkSize)  -- autoAllocateChunkSize 0 => undefined.
  CHECK(args[0]->IsFunction());
  CHECK(args[1]->IsFunction());
  CHECK(args[2]->IsFunction());
  CHECK(args[3]->IsNumber());
  CHECK(args[4]->IsUint32());
  Local<Function> start_algorithm = args[0].As<Function>();
  Local<Function> pull_algorithm = args[1].As<Function>();
  Local<Function> cancel_algorithm = args[2].As<Function>();
  double high_water_mark = args[3].As<Number>()->Value();
  size_t auto_allocate_chunk_size = args[4].As<v8::Uint32>()->Value();

  Local<Function> stream_ctor;
  if (!ReadableStream::GetConstructorTemplate(env)
           ->GetFunction(context)
           .ToLocal(&stream_ctor))
    return;
  Local<Object> stream_obj;
  if (!stream_ctor->NewInstance(context).ToLocal(&stream_obj)) return;
  BaseObjectPtr<ReadableStream> stream =
      MakeBaseObject<ReadableStream>(env, stream_obj);
  stream->MakeWeak();

  Local<Function> controller_ctor;
  if (!ReadableByteStreamController::GetConstructorTemplate(env)
           ->GetFunction(context)
           .ToLocal(&controller_ctor))
    return;
  Local<Object> controller_obj;
  if (!controller_ctor->NewInstance(context).ToLocal(&controller_obj)) return;
  BaseObjectPtr<ReadableByteStreamController> controller =
      MakeBaseObject<ReadableByteStreamController>(env, controller_obj);
  controller->MakeWeak();

  stream->SetController(controller_obj, /* is_byte */ true);

  if (!controller->Setup(start_algorithm, pull_algorithm, cancel_algorithm,
                         high_water_mark, auto_allocate_chunk_size)) {
    return;  // start threw synchronously; exception is pending.
  }

  args.GetReturnValue().Set(stream_obj);
}

void AcquireReadableStreamDefaultReader(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Local<Context> context = env->context();
  CHECK(args[0]->IsObject());
  Local<Object> stream_obj = args[0].As<Object>();

  Local<Function> reader_ctor;
  if (!ReadableStreamDefaultReader::GetConstructorTemplate(env)
           ->GetFunction(context)
           .ToLocal(&reader_ctor))
    return;
  Local<Object> reader_obj;
  if (!reader_ctor->NewInstance(context).ToLocal(&reader_obj)) return;
  BaseObjectPtr<ReadableStreamDefaultReader> reader =
      MakeBaseObject<ReadableStreamDefaultReader>(env, reader_obj);
  reader->MakeWeak();

  if (!reader->SetupInternal(stream_obj)) return;  // throws on lock
  args.GetReturnValue().Set(reader_obj);
}

void AcquireReadableStreamBYOBReader(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Local<Context> context = env->context();
  CHECK(args[0]->IsObject());
  Local<Object> stream_obj = args[0].As<Object>();

  Local<Function> reader_ctor;
  if (!ReadableStreamBYOBReader::GetConstructorTemplate(env)
           ->GetFunction(context)
           .ToLocal(&reader_ctor))
    return;
  Local<Object> reader_obj;
  if (!reader_ctor->NewInstance(context).ToLocal(&reader_obj)) return;
  BaseObjectPtr<ReadableStreamBYOBReader> reader =
      MakeBaseObject<ReadableStreamBYOBReader>(env, reader_obj);
  reader->MakeWeak();

  if (!reader->SetupInternal(stream_obj)) return;  // throws on lock / non-byte
  args.GetReturnValue().Set(reader_obj);
}

void InitializeReadableStream(Isolate* isolate, Local<ObjectTemplate> target) {
  SetMethod(isolate, target, "createReadableStream", CreateReadableStream);
  SetMethod(
      isolate, target, "createReadableByteStream", CreateReadableByteStream);
  SetMethod(isolate, target, "acquireReadableStreamDefaultReader",
            AcquireReadableStreamDefaultReader);
  SetMethod(isolate, target, "acquireReadableStreamBYOBReader",
            AcquireReadableStreamBYOBReader);
}

void RegisterReadableStreamExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(CreateReadableStream);
  registry->Register(CreateReadableByteStream);
  registry->Register(AcquireReadableStreamDefaultReader);
  registry->Register(AcquireReadableStreamBYOBReader);
  ReadableStream::RegisterExternalReferences(registry);
  ReadableStreamDefaultController::RegisterExternalReferences(registry);
  ReadableStreamDefaultReader::RegisterExternalReferences(registry);
  ReadableByteStreamController::RegisterExternalReferences(registry);
  ReadableStreamBYOBRequest::RegisterExternalReferences(registry);
  ReadableStreamBYOBReader::RegisterExternalReferences(registry);
}

}  // namespace webstreams
}  // namespace node
