#include "streams/readable_stream.h"
#include "streams/streams_binding.h"
#include "streams/writable_stream.h"
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
using v8::String;
using v8::TryCatch;
using v8::Uint16Array;
using v8::Uint32Array;
using v8::Uint8Array;
using v8::Uint8ClampedArray;
using v8::Undefined;
using v8::Value;

namespace {

// Builds a { value, done } read result. When `for_author_code` is false (reads
// issued internally by pipeTo/tee) the object is given a null prototype so it is
// never a thenable even if author code monkey-patched Object.prototype.then —
// otherwise resolving the read promise with it would invoke that patched then,
// making piping/teeing observable. Author-facing reads keep Object.prototype.
Local<Object> MakeReadResult(Environment* env,
                             Local<Context> context,
                             Local<Value> value,
                             bool done,
                             bool for_author_code = true) {
  // Clones a cached boilerplate object instead of assembling { value, done }
  // property-by-property: Object::Clone is a flat heap copy that preserves the
  // boilerplate's map, so the hot path skips the string interning, lookup, and
  // map-transition machinery entirely. `done` is baked into the boilerplate;
  // only `value` is written (an overwrite of an existing own property), and the
  // done=true results (value always undefined) need no write at all.
  Isolate* isolate = env->isolate();
  BindingData* bd = BindingData::Get(env);
  v8::Global<v8::Object>& slot =
      for_author_code ? (done ? bd->read_result_done : bd->read_result_not_done)
                      : (done ? bd->read_result_done_null_proto
                              : bd->read_result_not_done_null_proto);
  Local<Object> boilerplate = slot.Get(isolate);
  if (boilerplate.IsEmpty()) {
    // CreateDataProperty (not Set): defining the own properties must not be
    // observable through — or corrupted by — accessors a user may have patched
    // onto Object.prototype.
    boilerplate = Object::New(isolate);
    boilerplate
        ->CreateDataProperty(context, env->value_string(), Undefined(isolate))
        .Check();
    boilerplate
        ->CreateDataProperty(
            context, env->done_string(), Boolean::New(isolate, done))
        .Check();
    if (!for_author_code)
      boilerplate->SetPrototypeV2(context, Null(isolate)).Check();
    slot.Reset(isolate, boilerplate);
  }
  Local<Object> obj = boilerplate->Clone(isolate);
  // Overwrites the clone's own `value` property; never reaches the prototype.
  // Skipped only when the value IS undefined (already the boilerplate's value)
  // — note done=true results may carry a value (a BYOB read's partially-filled
  // view when the stream closes), so this must not key off `done`.
  if (!value->IsUndefined())
    obj->CreateDataProperty(context, env->value_string(), value).Check();
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

// A TypeError carrying an ERR_* code (WPT requires TypeError for these).
Local<Value> TypeErrorWith(Isolate* isolate,
                           Local<Context> context,
                           const char* code,
                           const char* message) {
  Local<String> msg = OneByteString(isolate, message);
  Local<Value> err = Exception::TypeError(msg);
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

// The reader-released `closed` rejection reason. NEVER construct this eagerly
// in Release(): error construction captures a stack trace, which dominates
// getReader()/releaseLock() churn. Built only when a settled-with-it promise
// actually exists or is first observed (ClosedState::kRejectedReleased).
Local<Value> ReaderReleasedError(Isolate* isolate, Local<Context> context) {
  return InvalidStateError(
      isolate, context,
      "Reader was released and can no longer be used to monitor the stream's "
      "state");
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
// carry the controller wrapper as their Data. Used by cold paths (start);
// the per-pull hot path uses ThenReactCached below.
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

// Like ThenReact, but reuses the controller's cached reaction functions
// (creating them on first use), so the per-pull path allocates no V8 Function.
void ThenReactCached(Environment* env,
                     Local<Promise> promise,
                     Local<Object> controller_obj,
                     v8::FunctionCallback on_fulfilled,
                     v8::Global<Function>* ff_slot,
                     v8::Global<Function>* rj_slot) {
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  Local<Function> ff = ff_slot->Get(isolate);
  if (ff.IsEmpty()) {
    if (!Function::New(context, on_fulfilled, controller_obj).ToLocal(&ff))
      return;
    ff_slot->Reset(isolate, ff);
  }
  Local<Function> rj = rj_slot->Get(isolate);
  if (rj.IsEmpty()) {
    if (!Function::New(context, ReactRejected, controller_obj).ToLocal(&rj))
      return;
    rj_slot->Reset(isolate, rj);
  }
  USE(promise->Then(context, ff, rj));
}

// A function that returns undefined; used both to map a promise's fulfilment
// value to undefined and as a no-op rejection handler.
void Noop(const FunctionCallbackInfo<Value>& args) {}

// Marks a promise as handled to avoid spurious unhandled-rejection warnings on
// internal promises, mirroring `PromisePrototypeThen(p, undefined, () => {})`.
// Cold path (close/error/release), so the per-call Function is acceptable.
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
    isolate->ThrowException(TypeErrorWith(
        isolate, context, "ERR_INVALID_STATE",
        "Cannot transfer a detached ArrayBuffer"));
    return false;
  }
  if (!ab->IsDetachable()) {
    isolate->ThrowException(TypeErrorWith(
        isolate, context, "ERR_INVALID_STATE",
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
ViewType ClassifyView(Environment* env,
                      Local<ArrayBufferView> view,
                      uint32_t* element_size) {
  if (view->IsDataView()) {
    *element_size = 1;
    return ViewType::kDataView;
  }
  if (view->IsUint8Array()) {
    *element_size = 1;
    // node::Buffer::HasInstance() is true for *any* Uint8Array, so it cannot be
    // used to tell a real Buffer apart from a plain Uint8Array. Compare the
    // prototype instead, so the WHATWG-mandated view constructor is preserved
    // (a plain Uint8Array must round-trip as a Uint8Array, not a Buffer).
    if (view->GetPrototypeV2()->StrictEquals(env->buffer_prototype_object()))
      return ViewType::kBuffer;
    return ViewType::kUint8Array;
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
    tmpl->PrototypeTemplate()->SetAccessorProperty(
        FIXED_ONE_BYTE_STRING(isolate, "desiredSize"),
        NewGetter(isolate, "desiredSize", GetDesiredSize));
    SetProtoMethodLen(isolate, tmpl, "close", Close, 0);
    SetProtoMethodLen(isolate, tmpl, "enqueue", Enqueue, 0);
    SetProtoMethodLen(isolate, tmpl, "error", Error, 0);
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
  // The pull algorithm is the user's raw function (no per-call async JS
  // wrapper). The common cases — no pull, or a synchronous pull returning a
  // non-object — complete through the shared per-realm reaction on a promise
  // resolved with this controller's wrapper: same microtask timing as a
  // promise return, no per-call Function allocation.
  Local<Value> result;
  if (pull.IsEmpty()) {
    result = Undefined(isolate);
  } else {
    Local<Value> recv = algo_receiver_.IsEmpty()
                            ? Undefined(isolate).As<Value>()
                            : algo_receiver_.Get(isolate);
    Local<Value> argv[] = {controller_obj};
    TryCatch try_catch(isolate);
    if (!pull->Call(context, recv, 1, argv).ToLocal(&result)) {
      if (try_catch.HasTerminated()) return;
      // A synchronous throw behaves as a rejected pull promise (the former
      // wrapper's semantics): error the stream in a microtask.
      Local<Value> exception = try_catch.Exception();
      try_catch.Reset();
      Local<Promise::Resolver> resolver;
      if (!Promise::Resolver::New(context).ToLocal(&resolver)) return;
      USE(resolver->Reject(context, exception));
      ThenReactCached(env, resolver->GetPromise(), controller_obj,
                      ReactPullFulfilled, &on_pull_fulfilled_, &on_rejected_);
      return;
    }
  }
  if (result->IsPromise()) {
    ThenReactCached(env, result.As<Promise>(), controller_obj,
                    ReactPullFulfilled, &on_pull_fulfilled_, &on_rejected_);
    return;
  }
  if (!result->IsObject()) {
    // Synchronous pull: enqueue FinishPull directly as a microtask via the
    // per-controller cached reaction — one API call, no promise; ordering is
    // identical (CallableTask and PromiseReactionJob share the FIFO queue).
    Local<Function> ff = on_pull_fulfilled_.Get(isolate);
    if (ff.IsEmpty()) {
      if (!Function::New(context, ReactPullFulfilled, controller_obj)
               .ToLocal(&ff))
        return;
      on_pull_fulfilled_.Reset(isolate, ff);
    }
    isolate->EnqueueMicrotask(ff);
    return;
  }
  // Thenable (non-promise object): full promise resolution so its `then` is
  // honored, reacted to with the per-controller cached functions.
  Local<Promise::Resolver> resolver;
  if (!Promise::Resolver::New(context).ToLocal(&resolver)) return;
  USE(resolver->Resolve(context, result));
  ThenReactCached(env, resolver->GetPromise(), controller_obj,
                  ReactPullFulfilled, &on_pull_fulfilled_, &on_rejected_);
}

void ReadableStreamDefaultController::ClearAlgorithms() {
  pull_algorithm_.Reset();
  cancel_algorithm_.Reset();
  size_algorithm_.Reset();
  algo_receiver_.Reset();
  // Break the controller<->wrapper cycle created by the cached reaction
  // functions (their Data is this controller's wrapper). No further pulls occur
  // after the algorithms are cleared, so they will not be recreated.
  on_pull_fulfilled_.Reset();
  on_rejected_.Reset();
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
    // Capture any thrown/range error, then re-throw it AFTER the TryCatch has
    // gone out of scope — a v8::TryCatch destructor cancels a pending exception
    // raised while it is still alive, which would silently swallow size()
    // throws and invalid-size RangeErrors.
    Local<Value> pending_exception;
    {
      TryCatch try_catch(isolate);
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
        pending_exception = try_catch.Exception();
        try_catch.Reset();
      }
    }
    if (!pending_exception.IsEmpty()) {
      ErrorInternal(pending_exception);
      isolate->ThrowException(pending_exception);
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
  // Receiver: the source for public streams (their wrapped cancel ignores
  // `this`); the transform stream for the shared transform trampolines.
  Local<Value> cancel_recv = algo_receiver_.IsEmpty()
                                 ? Undefined(isolate).As<Value>()
                                 : algo_receiver_.Get(isolate);
  if (cancel->Call(context, cancel_recv, 1, argv).ToLocal(&result) &&
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
    USE(resolver->Resolve(
        context, MakeReadResult(env, context, chunk, false,
                                stream->default_reader()->for_author_code())));
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
                                            Local<Function> size_algorithm,
                                            Local<Value> algo_receiver) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  high_water_mark_ = high_water_mark;
  size_mode_ = size_mode;
  if (!pull_algorithm.IsEmpty())
    pull_algorithm_.Reset(isolate, pull_algorithm);
  cancel_algorithm_.Reset(isolate, cancel_algorithm);
  if (!algo_receiver.IsEmpty() && !algo_receiver->IsUndefined())
    algo_receiver_.Reset(isolate, algo_receiver);
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
  if (!start_result->IsObject()) {
    // Common case: start returned undefined/a primitive, so there is nothing
    // to await (and no thenable to chase). Resolve with the controller wrapper
    // and reuse the shared per-realm reaction — preserves the "started in a
    // microtask" timing without two per-creation Function allocations.
    USE(resolver->Resolve(context, controller_obj));
    ThenStartFulfilled(env, resolver->GetPromise());
    return true;
  }
  USE(resolver->Resolve(context, start_result));
  ThenReact(env, resolver->GetPromise(), controller_obj, ReactStartFulfilled);
  return true;
}

void ReadableStreamDefaultController::GetDesiredSize(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (!CheckReceiverInvalidThis(env, GetConstructorTemplate(env), args.This(),
                                "ReadableStreamDefaultController"))
    return;
  auto* c = BaseObject::FromJSObject<ReadableStreamDefaultController>(
      args.This());
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
  if (!CheckReceiverInvalidThis(env, GetConstructorTemplate(env), args.This(),
                                "ReadableStreamDefaultController"))
    return;
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
  if (!CheckReceiverInvalidThis(env, GetConstructorTemplate(env), args.This(),
                                "ReadableStreamDefaultController"))
    return;
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
  Environment* env = Environment::GetCurrent(args);
  if (!CheckReceiverInvalidThis(env, GetConstructorTemplate(env), args.This(),
                                "ReadableStreamDefaultController"))
    return;
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
    tmpl->PrototypeTemplate()->SetAccessorProperty(
        FIXED_ONE_BYTE_STRING(isolate, "closed"),
        NewPromiseGetter(isolate, "closed", GetClosed));
    SetProtoMethodPromise(isolate, tmpl, "read", Read, 0);
    SetProtoMethodLen(isolate, tmpl, "releaseLock", ReleaseLock, 0);
    SetProtoMethodPromise(isolate, tmpl, "cancel", Cancel, 0);
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
  USE(resolver->Resolve(
      context,
      MakeReadResult(env, context, value, done, for_author_code_)));
}

void ReadableStreamDefaultReader::CloseAll() {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  while (!read_requests_.empty()) {
    Local<Promise::Resolver> resolver = read_requests_.front().Get(isolate);
    read_requests_.pop_front();
    USE(resolver->Resolve(
        context, MakeReadResult(env, context, Undefined(isolate), true,
                                for_author_code_)));
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
    // Lazily materialize, settled per the recorded state (most readers never
    // touch `closed`, so acquiring one allocates no resolver).
    if (!Promise::Resolver::New(env->context()).ToLocal(&resolver))
      return Local<Promise>();
    closed_.Reset(isolate, resolver);
    if (closed_state_ == ClosedState::kResolved) {
      USE(resolver->Resolve(env->context(), Undefined(isolate)));
    } else if (closed_state_ != ClosedState::kPending) {
      Local<Value> reason = closed_state_ == ClosedState::kRejectedReleased
                                ? ReaderReleasedError(isolate, env->context())
                                : closed_reason_.Get(isolate);
      Local<Promise> promise = resolver->GetPromise();
      USE(resolver->Reject(env->context(), reason));
      MarkHandled(env, promise);
    }
  }
  return resolver->GetPromise();
}

void ReadableStreamDefaultReader::ResolveClosed() {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Promise::Resolver> resolver = closed_.Get(isolate);
  if (resolver.IsEmpty()) {
    closed_state_ = ClosedState::kResolved;
    return;
  }
  USE(resolver->Resolve(env->context(), Undefined(isolate)));
}

void ReadableStreamDefaultReader::RejectClosed(Local<Value> error) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Promise::Resolver> resolver = closed_.Get(isolate);
  if (resolver.IsEmpty()) {
    closed_state_ = ClosedState::kRejected;
    closed_reason_.Reset(isolate, error);
    return;
  }
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
  // Wire reader<->stream via GC-traced internal fields, mirrored into the
  // raw-pointer caches for the hot-path accessors.
  object()->SetInternalField(kStream, stream_obj);
  stream_obj->SetInternalField(ReadableStream::kReader, object());
  stream_cache_ = stream;
  stream->set_reader_cache(this);

  // Record the closed-promise settlement; the resolver itself is materialized
  // only on first access to `closed` (see closed_promise).
  switch (stream->state()) {
    case StreamState::kReadable:
      closed_state_ = ClosedState::kPending;
      break;
    case StreamState::kClosed:
      closed_state_ = ClosedState::kResolved;
      break;
    case StreamState::kErrored:
      closed_state_ = ClosedState::kRejected;
      closed_reason_.Reset(isolate, stream->stored_error(env));
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
  Local<Promise::Resolver> resolver = closed_.Get(isolate);
  if (resolver.IsEmpty()) {
    // Not yet materialized: after release, `closed` is always rejected with
    // the released error (spec ReadableStreamReaderGenericRelease). The error
    // itself is built lazily on first access — constructing it here would put
    // a stack capture on every releaseLock().
    closed_state_ = ClosedState::kRejectedReleased;
    closed_reason_.Reset();
  } else {
    Local<Value> released_error = ReaderReleasedError(isolate, context);
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

  // The controller's release steps run before the stream<->reader link is
  // broken. For a byte source this re-tags the first pending pull-into as
  // "none" (its buffer belongs to the released reader) and drops the rest.
  if (stream->has_byte_controller())
    stream->byte_controller()->OnReaderReleased();

  // Unlink stream<->reader (traced fields and raw-pointer caches).
  object()->SetInternalField(kStream, Undefined(isolate));
  stream->object()->SetInternalField(ReadableStream::kReader,
                                     Undefined(isolate));
  stream_cache_ = nullptr;
  stream->set_reader_cache(nullptr);

  // Error any outstanding read requests with the releasing error (created
  // only if there are any — error construction captures a stack trace).
  if (!read_requests_.empty()) {
    ErrorAll(InvalidStateError(isolate, context,
                               "The reader is not attached to a stream"));
  }
}

// Shared tail of the default reader's read(): builds the read promise and
// dispatches by stream state. Used by both the prototype Read below and the
// single-crossing ReaderFastRead binding.
static void DoDefaultReaderRead(Environment* env,
                                ReadableStreamDefaultReader* reader,
                                const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
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
          context, MakeReadResult(env, context, Undefined(isolate), true,
                                  reader->for_author_code())));
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

void ReadableStreamDefaultReader::Read(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Local<Context> context = env->context();
  if (!ReadableStreamDefaultReader::GetConstructorTemplate(env)->HasInstance(
          args.This())) {
    args.GetReturnValue().Set(IllegalInvocationRejection(context));
    return;
  }
  auto* reader =
      BaseObject::FromJSObject<ReadableStreamDefaultReader>(args.This());
  DoDefaultReaderRead(env, reader, args);
}

void ReadableStreamDefaultReader::ReleaseLock(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (!CheckReceiverInvalidThis(env, GetConstructorTemplate(env), args.This(),
                                "ReadableStreamDefaultReader"))
    return;
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
  if (!ReadableStreamDefaultReader::GetConstructorTemplate(env)->HasInstance(
          args.This())) {
    args.GetReturnValue().Set(IllegalInvocationRejection(context));
    return;
  }
  auto* reader =
      BaseObject::FromJSObject<ReadableStreamDefaultReader>(args.This());
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
  if (!ReadableStreamDefaultReader::GetConstructorTemplate(env)->HasInstance(
          args.This())) {
    args.GetReturnValue().Set(IllegalInvocationRejection(env->context()));
    return;
  }
  auto* reader =
      BaseObject::FromJSObject<ReadableStreamDefaultReader>(args.This());
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
    tmpl->PrototypeTemplate()->SetAccessorProperty(
        FIXED_ONE_BYTE_STRING(isolate, "locked"),
        NewGetter(isolate, "locked", GetLocked));
    SetProtoMethodPromise(isolate, tmpl, "cancel", Cancel, 0);
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

void ReadableStream::SetController(Local<Object> controller_obj, bool is_byte) {
  object()->SetInternalField(kController, controller_obj);
  // kStream is the first internal field of both controller kinds.
  controller_obj->SetInternalField(ReadableStreamDefaultController::kStream,
                                   object());
  // Mirror the traced fields into the raw-pointer caches (hot-path accessors).
  auto* base = static_cast<StreamBaseObject*>(
      BaseObject::FromJSObject(controller_obj));
  CHECK_NOT_NULL(base);
  controller_cache_ = base;
  if (is_byte) {
    static_cast<ReadableByteStreamController*>(base)->set_stream_cache(this);
  } else {
    static_cast<ReadableStreamDefaultController*>(base)->set_stream_cache(this);
  }
}

Local<Promise> ReadableStream::closed_promise(Environment* env) {
  Isolate* isolate = env->isolate();
  Local<Promise::Resolver> resolver = closed_.Get(isolate);
  if (resolver.IsEmpty()) {
    if (!Promise::Resolver::New(env->context()).ToLocal(&resolver))
      return Local<Promise>();
    closed_.Reset(isolate, resolver);
    // The stream may already be settled by the time the stream-level closed
    // promise is first requested (e.g. node:stream `finished()` on an
    // already-closed stream); settle the freshly-created resolver to match.
    if (state_ == StreamState::kClosed) {
      USE(resolver->Resolve(env->context(), Undefined(isolate)));
    } else if (state_ == StreamState::kErrored) {
      Local<Promise> p = resolver->GetPromise();
      USE(resolver->Reject(env->context(), stored_error(env)));
      MarkHandled(env, p);
    }
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
  Environment* env = Environment::GetCurrent(args);
  if (!CheckReceiverInvalidThis(env, GetConstructorTemplate(env), args.This(),
                                "ReadableStream"))
    return;
  auto* stream = BaseObject::FromJSObject<ReadableStream>(args.This());
  if (stream == nullptr) return;
  args.GetReturnValue().Set(stream->locked());
}

void ReadableStream::Cancel(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  if (!ReadableStream::GetConstructorTemplate(env)->HasInstance(args.This())) {
    args.GetReturnValue().Set(IllegalInvocationRejection(context));
    return;
  }
  auto* stream = BaseObject::FromJSObject<ReadableStream>(args.This());
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
    tmpl->PrototypeTemplate()->SetAccessorProperty(
        FIXED_ONE_BYTE_STRING(isolate, "byobRequest"),
        NewGetter(isolate, "byobRequest", GetByobRequest));
    tmpl->PrototypeTemplate()->SetAccessorProperty(
        FIXED_ONE_BYTE_STRING(isolate, "desiredSize"),
        NewGetter(isolate, "desiredSize", GetDesiredSize));
    SetProtoMethodLen(isolate, tmpl, "close", Close, 0);
    SetProtoMethodLen(isolate, tmpl, "enqueue", Enqueue, 1);
    SetProtoMethodLen(isolate, tmpl, "error", Error, 0);
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
  // Raw pull calling convention; see the default controller's CallPullIfNeeded.
  Local<Value> result;
  if (pull.IsEmpty()) {
    result = Undefined(isolate);
  } else {
    Local<Value> recv = algo_receiver_.IsEmpty()
                            ? Undefined(isolate).As<Value>()
                            : algo_receiver_.Get(isolate);
    Local<Value> argv[] = {controller_obj};
    TryCatch try_catch(isolate);
    if (!pull->Call(context, recv, 1, argv).ToLocal(&result)) {
      if (try_catch.HasTerminated()) return;
      Local<Value> exception = try_catch.Exception();
      try_catch.Reset();
      Local<Promise::Resolver> resolver;
      if (!Promise::Resolver::New(context).ToLocal(&resolver)) return;
      USE(resolver->Reject(context, exception));
      ThenReactCached(env, resolver->GetPromise(), controller_obj,
                      ReactPullFulfilled, &on_pull_fulfilled_, &on_rejected_);
      return;
    }
  }
  if (result->IsPromise()) {
    ThenReactCached(env, result.As<Promise>(), controller_obj,
                    ReactPullFulfilled, &on_pull_fulfilled_, &on_rejected_);
    return;
  }
  if (!result->IsObject()) {
    // Synchronous pull: microtask via the cached reaction; see the default
    // controller's CallPullIfNeeded.
    Local<Function> ff = on_pull_fulfilled_.Get(isolate);
    if (ff.IsEmpty()) {
      if (!Function::New(context, ReactPullFulfilled, controller_obj)
               .ToLocal(&ff))
        return;
      on_pull_fulfilled_.Reset(isolate, ff);
    }
    isolate->EnqueueMicrotask(ff);
    return;
  }
  Local<Promise::Resolver> resolver;
  if (!Promise::Resolver::New(context).ToLocal(&resolver)) return;
  USE(resolver->Resolve(context, result));
  ThenReactCached(env, resolver->GetPromise(), controller_obj,
                  ReactPullFulfilled, &on_pull_fulfilled_, &on_rejected_);
}

void ReadableByteStreamController::ClearAlgorithms() {
  pull_algorithm_.Reset();
  cancel_algorithm_.Reset();
  algo_receiver_.Reset();
  // Break the controller<->wrapper cycle from the cached reaction functions.
  on_pull_fulfilled_.Reset();
  on_rejected_.Reset();
}

void ReadableByteStreamController::InvalidateBYOBRequest() {
  ReadableStreamBYOBRequest* req = byob_request_obj();
  if (req == nullptr) return;
  Isolate* isolate = env()->isolate();
  // The spec transfers the pull-into descriptor's ArrayBuffer on every
  // respond()/enqueue(), which detaches the view this request handed out. Our
  // descriptor owns a shared BackingStore (not a JS ArrayBuffer), so detach the
  // exact ArrayBuffer wrapper the user is holding to reproduce that observable
  // behavior; the BackingStore survives via our own shared_ptr.
  Local<Value> view =
      req->object()->GetInternalField(ReadableStreamBYOBRequest::kView)
          .As<Value>();
  if (view->IsArrayBufferView()) {
    Local<ArrayBuffer> buffer = view.As<ArrayBufferView>()->Buffer();
    if (!buffer->WasDetached()) USE(buffer->Detach(Local<Value>()));
  }
  req->object()->SetInternalField(ReadableStreamBYOBRequest::kController,
                                  Undefined(isolate));
  req->object()->SetInternalField(ReadableStreamBYOBRequest::kView,
                                  v8::Null(isolate));
  object()->SetInternalField(kByobRequest, Undefined(isolate));
  req->set_controller_cache(nullptr);
  byob_request_cache_ = nullptr;
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
  ReadableStreamDefaultReader* dr = stream()->default_reader();
  USE(resolver->Resolve(
      context, MakeReadResult(env, context, view, false,
                              dr != nullptr ? dr->for_author_code() : true)));
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

  // Spec: if the first pending pull-into's buffer has been detached by the user
  // (it is exposed as byobRequest.view.buffer), enqueue() must throw a
  // TypeError — before the incoming chunk's buffer is transferred. Our
  // descriptor owns a shared BackingStore, so detect this via the outstanding
  // BYOB request's view.
  if (!pending_pull_intos_.empty()) {
    ReadableStreamBYOBRequest* req = byob_request_obj();
    if (req != nullptr) {
      Local<Value> v =
          req->object()->GetInternalField(ReadableStreamBYOBRequest::kView)
              .As<Value>();
      if (v->IsArrayBufferView() &&
          v.As<ArrayBufferView>()->Buffer()->WasDetached()) {
        isolate->ThrowException(TypeErrorWith(
            isolate, context, "ERR_INVALID_STATE",
            "The BYOB request's buffer has been detached"));
        return Nothing<void>();
      }
    }
  }

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
  Local<Value> cancel_recv = algo_receiver_.IsEmpty()
                                 ? Undefined(isolate).As<Value>()
                                 : algo_receiver_.Get(isolate);
  if (cancel->Call(context, cancel_recv, 1, argv).ToLocal(&result) &&
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
  ViewType view_type = ClassifyView(env, view, &element_size);
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
        context, MakeReadResult(env, context, empty_view, true,
                                stream->byob_reader()->for_author_code())));
    return;
  }
  if (queue_total_size_ > 0) {
    if (FillPullIntoDescriptorFromQueue(&desc)) {
      Local<Object> filled_view;
      if (!ConvertPullIntoDescriptor(&desc).ToLocal(&filled_view)) return;
      HandleQueueDrain();
      USE(resolver->Resolve(
          context, MakeReadResult(env, context, filled_view, false,
                                  stream->byob_reader()->for_author_code())));
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
    isolate->ThrowException(MakeCodedError(
        isolate, context, /* range */ true, "ERR_INVALID_ARG_VALUE",
        "The given view has an unexpected buffer length"));
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
                                        size_t auto_allocate_chunk_size,
                                        Local<Value> algo_receiver) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  high_water_mark_ = high_water_mark;
  if (auto_allocate_chunk_size > 0) {
    auto_allocate_chunk_size_ = auto_allocate_chunk_size;
    has_auto_allocate_ = true;
  }
  if (!pull_algorithm.IsEmpty())
    pull_algorithm_.Reset(isolate, pull_algorithm);
  cancel_algorithm_.Reset(isolate, cancel_algorithm);
  if (!algo_receiver.IsEmpty() && !algo_receiver->IsUndefined())
    algo_receiver_.Reset(isolate, algo_receiver);

  Local<Object> controller_obj = object();
  Local<Value> start_result;
  Local<Value> argv[] = {controller_obj};
  if (!start_algorithm->Call(context, Undefined(isolate), 1, argv)
           .ToLocal(&start_result)) {
    return false;
  }
  Local<Promise::Resolver> resolver;
  if (!Promise::Resolver::New(context).ToLocal(&resolver)) return false;
  if (!start_result->IsObject()) {
    // Common case (see the default controller's Setup): nothing to await, no
    // thenable to chase; reuse the shared per-realm start reaction.
    USE(resolver->Resolve(context, controller_obj));
    ThenStartFulfilled(env, resolver->GetPromise());
    return true;
  }
  USE(resolver->Resolve(context, start_result));
  ThenReact(env, resolver->GetPromise(), controller_obj, ReactStartFulfilled);
  return true;
}

void ReadableByteStreamController::GetByobRequest(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  if (!CheckReceiverInvalidThis(env, GetConstructorTemplate(env), args.This(),
                                "ReadableByteStreamController"))
    return;
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
    req->set_controller_cache(c);
    c->byob_request_cache_ = req.get();
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
  Environment* env = Environment::GetCurrent(args);
  if (!CheckReceiverInvalidThis(env, GetConstructorTemplate(env), args.This(),
                                "ReadableByteStreamController"))
    return;
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
  if (!CheckReceiverInvalidThis(env, GetConstructorTemplate(env), args.This(),
                                "ReadableByteStreamController"))
    return;
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
  if (!CheckReceiverInvalidThis(env, GetConstructorTemplate(env), args.This(),
                                "ReadableByteStreamController"))
    return;
  auto* c =
      BaseObject::FromJSObject<ReadableByteStreamController>(args.This());
  if (c == nullptr) return;
  if (!args[0]->IsArrayBufferView()) {
    isolate->ThrowException(TypeErrorWith(
        isolate, context, "ERR_INVALID_ARG_TYPE",
        "chunk must be an ArrayBufferView"));
    return;
  }
  Local<ArrayBufferView> view = args[0].As<ArrayBufferView>();
  Local<ArrayBuffer> chunk_buffer = view->Buffer();
  // A SharedArrayBuffer cannot be transferred/detached, so a byte stream may
  // not enqueue a view backed by one (matches the JS implementation).
  if (chunk_buffer.As<Value>()->IsSharedArrayBuffer()) {
    isolate->ThrowException(TypeErrorWith(
        isolate, context, "ERR_INVALID_ARG_VALUE",
        "The \"chunk\" argument must not be backed by a SharedArrayBuffer"));
    return;
  }
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
  Environment* env = Environment::GetCurrent(args);
  if (!CheckReceiverInvalidThis(env, GetConstructorTemplate(env), args.This(),
                                "ReadableByteStreamController"))
    return;
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
    tmpl->PrototypeTemplate()->SetAccessorProperty(
        FIXED_ONE_BYTE_STRING(isolate, "view"),
        NewGetter(isolate, "view", GetView));
    SetProtoMethodLen(isolate, tmpl, "respond", Respond, 1);
    SetProtoMethodLen(isolate, tmpl, "respondWithNewView", RespondWithNewView, 1);
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
  Environment* env = Environment::GetCurrent(args);
  if (!CheckReceiverInvalidThis(env, GetConstructorTemplate(env), args.This(),
                                "ReadableStreamBYOBRequest"))
    return;
  auto* r = BaseObject::FromJSObject<ReadableStreamBYOBRequest>(args.This());
  if (r == nullptr) return;
  args.GetReturnValue().Set(r->object()->GetInternalField(kView).As<Value>());
}

void ReadableStreamBYOBRequest::Respond(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  if (!CheckReceiverInvalidThis(env, GetConstructorTemplate(env), args.This(),
                                "ReadableStreamBYOBRequest"))
    return;
  if (args.Length() < 1) {
    isolate->ThrowException(v8::Exception::TypeError(FIXED_ONE_BYTE_STRING(
        isolate, "Failed to execute 'respond': 1 argument required")));
    return;
  }
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
  if (!CheckReceiverInvalidThis(env, GetConstructorTemplate(env), args.This(),
                                "ReadableStreamBYOBRequest"))
    return;
  auto* r = BaseObject::FromJSObject<ReadableStreamBYOBRequest>(args.This());
  if (r == nullptr) return;
  ReadableByteStreamController* c = r->controller();
  if (c == nullptr) {
    isolate->ThrowException(InvalidStateError(
        isolate, context, "This BYOB request has been invalidated"));
    return;
  }
  if (!args[0]->IsArrayBufferView()) {
    isolate->ThrowException(TypeErrorWith(
        isolate, context, "ERR_INVALID_ARG_TYPE",
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
    tmpl->PrototypeTemplate()->SetAccessorProperty(
        FIXED_ONE_BYTE_STRING(isolate, "closed"),
        NewPromiseGetter(isolate, "closed", GetClosed));
    SetProtoMethodPromise(isolate, tmpl, "read", Read, 1);
    SetProtoMethodLen(isolate, tmpl, "releaseLock", ReleaseLock, 0);
    SetProtoMethodPromise(isolate, tmpl, "cancel", Cancel, 0);
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
  USE(resolver->Resolve(
      context,
      MakeReadResult(env, context, view, done, for_author_code_)));
}

void ReadableStreamBYOBReader::CloseAll() {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  while (!read_into_requests_.empty()) {
    Local<Promise::Resolver> resolver = read_into_requests_.front().Get(isolate);
    read_into_requests_.pop_front();
    USE(resolver->Resolve(
        context, MakeReadResult(env, context, Undefined(isolate), true,
                                for_author_code_)));
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
    // Lazily materialize, settled per the recorded state (see the default
    // reader's closed_promise).
    if (!Promise::Resolver::New(env->context()).ToLocal(&resolver))
      return Local<Promise>();
    closed_.Reset(isolate, resolver);
    if (closed_state_ == ClosedState::kResolved) {
      USE(resolver->Resolve(env->context(), Undefined(isolate)));
    } else if (closed_state_ != ClosedState::kPending) {
      Local<Value> reason = closed_state_ == ClosedState::kRejectedReleased
                                ? ReaderReleasedError(isolate, env->context())
                                : closed_reason_.Get(isolate);
      Local<Promise> promise = resolver->GetPromise();
      USE(resolver->Reject(env->context(), reason));
      MarkHandled(env, promise);
    }
  }
  return resolver->GetPromise();
}

void ReadableStreamBYOBReader::ResolveClosed() {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Promise::Resolver> resolver = closed_.Get(isolate);
  if (resolver.IsEmpty()) {
    closed_state_ = ClosedState::kResolved;
    return;
  }
  USE(resolver->Resolve(env->context(), Undefined(isolate)));
}

void ReadableStreamBYOBReader::RejectClosed(Local<Value> error) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Promise::Resolver> resolver = closed_.Get(isolate);
  if (resolver.IsEmpty()) {
    closed_state_ = ClosedState::kRejected;
    closed_reason_.Reset(isolate, error);
    return;
  }
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
    isolate->ThrowException(
        InvalidStateError(isolate, context, "stream must be a byte stream"));
    return false;
  }
  object()->SetInternalField(kStream, stream_obj);
  stream_obj->SetInternalField(ReadableStream::kReader, object());
  stream_cache_ = stream;
  stream->set_reader_cache(this);

  // Record the closed-promise settlement; the resolver itself is materialized
  // only on first access to `closed` (see closed_promise).
  switch (stream->state()) {
    case StreamState::kReadable:
      closed_state_ = ClosedState::kPending;
      break;
    case StreamState::kClosed:
      closed_state_ = ClosedState::kResolved;
      break;
    case StreamState::kErrored:
      closed_state_ = ClosedState::kRejected;
      closed_reason_.Reset(isolate, stream->stored_error(env));
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

  Local<Promise::Resolver> resolver = closed_.Get(isolate);
  if (resolver.IsEmpty()) {
    // Not yet materialized: after release, `closed` is always rejected with
    // the released error (spec ReadableStreamReaderGenericRelease). The error
    // itself is built lazily on first access — constructing it here would put
    // a stack capture on every releaseLock().
    closed_state_ = ClosedState::kRejectedReleased;
    closed_reason_.Reset();
  } else {
    Local<Value> released_error = ReaderReleasedError(isolate, context);
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
  stream_cache_ = nullptr;
  stream->set_reader_cache(nullptr);

  // Created only if there are outstanding read-into requests (error
  // construction captures a stack trace).
  if (!read_into_requests_.empty()) {
    ErrorAll(InvalidStateError(isolate, context,
                               "The reader is not attached to a stream"));
  }
}

void ReadableStreamBYOBReader::Read(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  if (!ReadableStreamBYOBReader::GetConstructorTemplate(env)->HasInstance(
          args.This())) {
    args.GetReturnValue().Set(IllegalInvocationRejection(context));
    return;
  }
  auto* reader =
      BaseObject::FromJSObject<ReadableStreamBYOBReader>(args.This());

  Local<Promise::Resolver> resolver;
  if (!Promise::Resolver::New(context).ToLocal(&resolver)) return;
  args.GetReturnValue().Set(resolver->GetPromise());

  if (!args[0]->IsArrayBufferView()) {
    USE(resolver->Reject(
        context,
        TypeErrorWith(isolate, context,
                       "ERR_INVALID_ARG_TYPE",
                       "view must be an ArrayBufferView")));
    return;
  }
  Local<ArrayBufferView> view = args[0].As<ArrayBufferView>();
  Local<ArrayBuffer> view_buffer = view->Buffer();
  // A SharedArrayBuffer cannot be transferred/detached, so a BYOB read may not
  // be issued into a view backed by one (matches the JS implementation).
  if (view_buffer.As<Value>()->IsSharedArrayBuffer()) {
    USE(resolver->Reject(
        context,
        TypeErrorWith(
            isolate, context, "ERR_INVALID_ARG_VALUE",
            "The \"view\" argument must not be backed by a SharedArrayBuffer")));
    return;
  }
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
    // Spec: a min of 0 (or, after coercion, a non-positive value) is a
    // TypeError, not a RangeError.
    USE(resolver->Reject(
        context, InvalidStateError(isolate, context,
                                   "options.min must be greater than 0")));
    return;
  }
  // min must not exceed the element capacity of the view.
  uint32_t element_size = 1;
  ClassifyView(env, view, &element_size);
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
  Environment* env = Environment::GetCurrent(args);
  if (!CheckReceiverInvalidThis(env, GetConstructorTemplate(env), args.This(),
                                "ReadableStreamBYOBReader"))
    return;
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
  if (!ReadableStreamBYOBReader::GetConstructorTemplate(env)->HasInstance(
          args.This())) {
    args.GetReturnValue().Set(IllegalInvocationRejection(context));
    return;
  }
  auto* reader =
      BaseObject::FromJSObject<ReadableStreamBYOBReader>(args.This());
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
  if (!ReadableStreamBYOBReader::GetConstructorTemplate(env)->HasInstance(
          args.This())) {
    args.GetReturnValue().Set(IllegalInvocationRejection(env->context()));
    return;
  }
  auto* reader =
      BaseObject::FromJSObject<ReadableStreamBYOBReader>(args.This());
  args.GetReturnValue().Set(reader->closed_promise(env));
}

// ===========================================================================
// Binding entry points
// ===========================================================================

MaybeLocal<Object> NewReadableStream(Environment* env,
                                     Local<Function> start_algorithm,
                                     Local<Function> pull_algorithm,
                                     Local<Function> cancel_algorithm,
                                     double high_water_mark,
                                     SizeMode size_mode,
                                     Local<Function> size_algorithm,
                                     Local<Value> algo_receiver) {
  Local<Context> context = env->context();
  // Create the stream object.
  Local<Function> stream_ctor;
  if (!ReadableStream::GetConstructorTemplate(env)
           ->GetFunction(context)
           .ToLocal(&stream_ctor))
    return MaybeLocal<Object>();
  Local<Object> stream_obj;
  if (!stream_ctor->NewInstance(context).ToLocal(&stream_obj))
    return MaybeLocal<Object>();
  BaseObjectPtr<ReadableStream> stream =
      MakeBaseObject<ReadableStream>(env, stream_obj);
  stream->MakeWeak();

  // Create the controller object.
  Local<Function> controller_ctor;
  if (!ReadableStreamDefaultController::GetConstructorTemplate(env)
           ->GetFunction(context)
           .ToLocal(&controller_ctor))
    return MaybeLocal<Object>();
  Local<Object> controller_obj;
  if (!controller_ctor->NewInstance(context).ToLocal(&controller_obj))
    return MaybeLocal<Object>();
  BaseObjectPtr<ReadableStreamDefaultController> controller =
      MakeBaseObject<ReadableStreamDefaultController>(env, controller_obj);
  controller->MakeWeak();

  // Link stream <-> controller (GC-traced internal fields).
  stream->SetController(controller_obj, /* is_byte */ false);

  if (!controller->Setup(start_algorithm, pull_algorithm, cancel_algorithm,
                         high_water_mark, size_mode, size_algorithm,
                         algo_receiver)) {
    return MaybeLocal<Object>();  // start threw synchronously; exception pending.
  }
  return stream_obj;
}

void CreateReadableStream(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  // args: (startAlgorithm, pullAlgorithm, cancelAlgorithm, highWaterMark,
  //        sizeMode, sizeAlgorithm, algoReceiver)
  // pullAlgorithm is the user's RAW pull (or undefined); algoReceiver is the
  // underlying source object used as its `this`.
  CHECK(args[0]->IsFunction());
  CHECK(args[2]->IsFunction());
  CHECK(args[3]->IsNumber());
  CHECK(args[4]->IsUint32());
  Local<Function> pull_algorithm;
  if (args[1]->IsFunction()) pull_algorithm = args[1].As<Function>();
  Local<Function> size_algorithm;
  if (args[5]->IsFunction()) size_algorithm = args[5].As<Function>();
  Local<Object> stream_obj;
  if (!NewReadableStream(
           env, args[0].As<Function>(), pull_algorithm,
           args[2].As<Function>(), args[3].As<Number>()->Value(),
           static_cast<SizeMode>(args[4].As<v8::Uint32>()->Value()),
           size_algorithm, args[6])
           .ToLocal(&stream_obj)) {
    return;
  }
  args.GetReturnValue().Set(stream_obj);
}

void CreateReadableByteStream(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  // args: (startAlgorithm, pullAlgorithm, cancelAlgorithm, highWaterMark,
  //        autoAllocateChunkSize, algoReceiver)
  // autoAllocateChunkSize 0 => undefined; pullAlgorithm is the user's RAW pull
  // (or undefined) called with algoReceiver as `this`.
  CHECK(args[0]->IsFunction());
  CHECK(args[2]->IsFunction());
  CHECK(args[3]->IsNumber());
  CHECK(args[4]->IsUint32());
  Local<Function> start_algorithm = args[0].As<Function>();
  Local<Function> pull_algorithm;
  if (args[1]->IsFunction()) pull_algorithm = args[1].As<Function>();
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
                         high_water_mark, auto_allocate_chunk_size, args[5])) {
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
  // args[1] === true marks an internal (pipeTo/tee) reader: read results get a
  // null prototype so they are not observable thenables.
  if (args[1]->IsTrue()) reader->set_for_author_code(false);

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
  if (args[1]->IsTrue()) reader->set_for_author_code(false);

  if (!reader->SetupInternal(stream_obj)) return;  // throws on lock / non-byte
  args.GetReturnValue().Set(reader_obj);
}

// Internal introspection helpers for the node:stream interop layer
// (kIsDisturbed/kIsErrored/kIsReadable/kIsClosedPromise). These are binding
// functions rather than prototype properties so the public WebIDL surface
// stays unchanged.
void ReadableStreamStateField(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsObject());
  auto* s = BaseObject::FromJSObject<ReadableStream>(args[0].As<Object>());
  if (s == nullptr) return;
  args.GetReturnValue().Set(static_cast<uint32_t>(s->state()));
}

void ReadableStreamDisturbed(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsObject());
  auto* s = BaseObject::FromJSObject<ReadableStream>(args[0].As<Object>());
  if (s == nullptr) return;
  args.GetReturnValue().Set(s->disturbed());
}

void ReadableStreamClosedPromise(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsObject());
  auto* s = BaseObject::FromJSObject<ReadableStream>(args[0].As<Object>());
  if (s == nullptr) return;
  Local<Promise> p = s->closed_promise(env);
  if (!p.IsEmpty()) args.GetReturnValue().Set(p);
}

// The spec's internal ReadableStreamCancel, exposed for internal callers (tee,
// pipeTo) that must cancel a *locked* stream. The public cancel() rejects on a
// locked stream; this bypasses that check by running the cancel steps directly.
void ReadableStreamCancel(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsObject());
  auto* s = BaseObject::FromJSObject<ReadableStream>(args[0].As<Object>());
  if (s == nullptr) return;
  args.GetReturnValue().Set(s->CancelInternal(args[1]));
}

// The stream's stored error; used by pipeTo's synchronous priority checks
// (isOrBecomesErrored) to act on an already-errored source in order.
void ReadableStreamStoredError(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsObject());
  auto* s = BaseObject::FromJSObject<ReadableStream>(args[0].As<Object>());
  if (s == nullptr) return;
  args.GetReturnValue().Set(s->stored_error(env));
}

// pipeTo hot loop: moves chunks already buffered in the source's C++ value
// queue straight into the destination's write queue without touching JS — no
// read promise, no { value, done } result, no per-write request resolver
// (each write occupies an untracked request slot; pipeTo's shutdown waits on
// the destination's flush promise instead). Spec-legal because how chunks
// move through a pipe is unobservable; the user-visible effects (pull calls,
// write calls, backpressure, closed promises) are identical to the
// read-then-writer.write() sequence this replaces. Only used by pipeTo while
// it holds both locks. Returns:
//   0 — source queue empty (or closed/errored/byte): fall back to reader.read()
//   1 — destination backpressure: arm the pump; write completions re-enter
//   2 — destination no longer accepts writes: stop fast-pathing
int PipeTransferStep(ReadableStream* s, WritableStream* d) {
  ReadableStreamDefaultController* rc = s->default_controller();
  WritableStreamDefaultController* wc = d->controller();
  if (rc == nullptr || wc == nullptr) return rc == nullptr ? 0 : 2;
  while (true) {
    // Destination checks (mirrors WritableStreamDefaultWriterWrite's guards;
    // re-evaluated per chunk because the source pull below runs user code).
    if (d->state() != WritableState::kWritable || d->CloseQueuedOrInFlight())
      return 2;
    if (wc->GetDesiredSize() <= 0) return 1;
    // Source dequeue (mirrors the default controller's buffered PullSteps).
    if (s->state() != StreamState::kReadable || rc->queue_length() == 0)
      return 0;
    s->set_disturbed(true);
    Local<Value> chunk;
    if (!rc->DequeueValue().ToLocal(&chunk)) return 2;  // exception pending
    if (rc->close_requested() && rc->queue_length() == 0) {
      rc->ClearAlgorithms();
      s->Close();
    } else {
      rc->CallPullIfNeeded();  // may run the user's pull synchronously
    }
    // The pull (or a user size algorithm) may have made the destination
    // unwritable; writer.write() would return a rejected promise here and
    // drop the chunk — match that by dropping it.
    if (d->state() != WritableState::kWritable || d->CloseQueuedOrInFlight())
      return 2;
    double chunk_size = wc->GetChunkSize(chunk);  // errors controller on throw
    if (d->state() != WritableState::kWritable || d->CloseQueuedOrInFlight())
      return 2;
    d->AddWriteRequestUntracked();
    wc->Write(chunk, chunk_size);
  }
}

// read() fast path: synchronously dequeues a buffered chunk through a default
// reader, returning the raw chunk; the JS wrapper around the native read()
// builds the { value, done: false } result and the already-resolved promise in
// JIT code, skipping the C++ Promise::Resolver and result-clone crossings.
// Mirrors the buffered branch of the default controller's PullSteps (dequeue,
// then close-if-drained or pull). Returns the caller's sentinel (args[1]) when
// nothing can be dequeued synchronously — released reader, non-readable state,
// byte controller, empty queue — in which case the caller falls back to the
// native read(), which handles parking, close, error and brand rejection. A
// non-empty queue implies no pending read requests, so dequeuing here cannot
// jump the queue.
// True when `chunk` has the realm's original %Promise.prototype% anywhere on
// its prototype chain — i.e. the JS wrapper's ObjectPrototypeIsPrototypeOf
// promise test would (mis)classify it as a read promise. Such chunks (real
// promises, promise subclass instances, Object.create(Promise.prototype))
// must not be returned raw from ReaderFastRead.
static bool LooksLikePromise(Local<Context> context,
                             Local<Value> chunk,
                             Local<Object> promise_proto) {
  if (!chunk->IsObject()) return false;
  Local<Value> proto = chunk.As<Object>()->GetPrototypeV2();
  while (proto->IsObject()) {
    if (proto->StrictEquals(promise_proto)) return true;
    proto = proto.As<Object>()->GetPrototypeV2();
  }
  return false;
}

void ReaderFastRead(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  // Real brand check (HasInstance): args[0] is the wrapper's untrusted `this`
  // and FromJSObject's cast is unchecked across BaseObject subclasses. Only a
  // foreign receiver falls back to the native read (for its Web IDL
  // brand-check rejection); every other case completes in this crossing.
  if (!ReadableStreamDefaultReader::GetConstructorTemplate(env)->HasInstance(
          args[0])) {
    args.GetReturnValue().Set(args[1]);
    return;
  }
  auto* r = BaseObject::FromJSObject<ReadableStreamDefaultReader>(
      args[0].As<Object>());
  Local<Context> context = env->context();
  // Fast path: buffered chunk on an author-code reader. Non-author-code
  // readers (tee's internal reader) need null-prototype read results so a
  // patched Object.prototype.then cannot observe the chunks; they take the
  // DoDefaultReaderRead path below, same as before this binding existed.
  if (r->has_stream() && r->for_author_code()) {
    ReadableStream* s = r->stream();
    ReadableStreamDefaultController* c = s->default_controller();
    if (c != nullptr && s->state() == StreamState::kReadable &&
        c->queue_length() > 0) {
      s->set_disturbed(true);
      Local<Value> chunk;
      if (!c->DequeueValue().ToLocal(&chunk)) return;
      if (c->close_requested() && c->queue_length() == 0) {
        c->ClearAlgorithms();
        s->Close();
      } else {
        c->CallPullIfNeeded();
      }
      auto* bd = BindingData::Get(env);
      Local<Object> promise_proto;
      if (bd->promise_prototype.IsEmpty()) {
        // Cache the realm's original %Promise.prototype% (primordial-safe:
        // taken from a freshly created promise, not the patchable global).
        Local<Promise::Resolver> probe;
        if (!Promise::Resolver::New(context).ToLocal(&probe)) return;
        promise_proto =
            probe->GetPromise()->GetPrototypeV2().As<Object>();
        bd->promise_prototype.Reset(env->isolate(), promise_proto);
      } else {
        promise_proto = bd->promise_prototype.Get(env->isolate());
      }
      if (!LooksLikePromise(context, chunk, promise_proto)) {
        args.GetReturnValue().Set(chunk);
        return;
      }
      // Promise-lookalike chunk: ambiguous with the read-promise return, so
      // build the result promise natively (rare).
      Local<Promise::Resolver> resolver;
      if (!Promise::Resolver::New(context).ToLocal(&resolver)) return;
      USE(resolver->Resolve(
          context, MakeReadResult(env, context, chunk, false, true)));
      args.GetReturnValue().Set(resolver->GetPromise());
      return;
    }
  }
  // Everything else — released reader, closed/errored stream, byte
  // controller, empty queue, non-author reader — is the full native read,
  // still in this single crossing; its return is always a real promise.
  DoDefaultReaderRead(env, r, args);
}

// Write-completion re-entry for an armed pump (declared in
// streams_binding.h; called from the writable side). Disarms — settling the
// stall promise so pipeTo's step loop wakes — on every outcome except
// destination backpressure, which keeps the pump armed for the next write
// completion.
void RunPipePump(WritableStream* d) {
  Local<Object> src_obj = d->pump_source_obj(d->env()->isolate());
  auto* s = src_obj.IsEmpty()
                ? nullptr
                : BaseObject::FromJSObject<ReadableStream>(src_obj);
  if (s == nullptr || PipeTransferStep(s, d) != 1)
    d->DisarmPumpAndSettleStall();
}

// Binding entry: runs the transfer loop once from pipeTo's step. Returns int
// 0 / 2 (see PipeTransferStep) — or, on destination backpressure, arms the
// pump and returns its stall promise; chunks then move entirely in C++ off
// write completions until the pump stalls and the promise resolves.
void PipePump(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsObject());
  CHECK(args[1]->IsObject());
  auto* s = BaseObject::FromJSObject<ReadableStream>(args[0].As<Object>());
  auto* d = BaseObject::FromJSObject<WritableStream>(args[1].As<Object>());
  if (s == nullptr || d == nullptr) {
    args.GetReturnValue().Set(2);
    return;
  }
  int status = PipeTransferStep(s, d);
  if (status != 1) {
    args.GetReturnValue().Set(status);
    return;
  }
  Local<Promise> stall = d->PumpStallPromise();
  if (stall.IsEmpty()) {  // resolver allocation failed; exception pending
    args.GetReturnValue().Set(2);
    return;
  }
  d->ArmPump(args[0].As<Object>());
  args.GetReturnValue().Set(stall);
}

// pipeTo shutdown helper: stops the pump (if armed) so no further chunks move
// after shutdown begins, mirroring the JS step loop's shuttingDown checks.
void PipePumpDisarm(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsObject());
  auto* d = BaseObject::FromJSObject<WritableStream>(args[0].As<Object>());
  if (d == nullptr) return;
  d->DisarmPumpAndSettleStall();
}

// Introspection helper for white-box tests: returns a ReadableStream's
// controller object (default or byte). The public surface exposes no
// stream->controller link.
void ReadableStreamController(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (!ReadableStream::GetConstructorTemplate(env)->HasInstance(args[0])) return;
  auto* s = BaseObject::FromJSObject<ReadableStream>(args[0].As<Object>());
  if (s == nullptr) return;
  args.GetReturnValue().Set(
      s->object()->GetInternalField(ReadableStream::kController).As<Value>());
}

// Introspection helper for white-box tests: releases whatever reader is
// currently locked to the stream (mirrors the spec's internal
// ReadableStreamReaderGenericRelease on stream's reader). Used to exercise the
// "reader released out from under the async iterator" path.
void ReadableStreamReleaseReader(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (!ReadableStream::GetConstructorTemplate(env)->HasInstance(args[0])) return;
  auto* s = BaseObject::FromJSObject<ReadableStream>(args[0].As<Object>());
  if (s == nullptr) return;
  StreamBaseObject* reader = s->generic_reader();
  if (reader == nullptr) return;
  if (reader->stream_kind() == StreamBaseObject::Kind::kDefaultReader)
    static_cast<ReadableStreamDefaultReader*>(reader)->Release();
  else if (reader->stream_kind() == StreamBaseObject::Kind::kByobReader)
    static_cast<ReadableStreamBYOBReader*>(reader)->Release();
}

// Robust brand checks: a prototype-chain test (ObjectPrototypeIsPrototypeOf in
// JS) accepts forgeries like Object.create(ReadableStream.prototype); these use
// the constructor template's HasInstance, which only matches real native
// instances.
void IsReadableStream(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  args.GetReturnValue().Set(ReadableStream::HasInstance(env, args[0]));
}

// The spec's internal ReadableStreamDefaultController{Enqueue,Close,Error},
// which silently no-op when the controller can no longer close/enqueue (unlike
// the public methods, which throw). Used by tee/transfer, whose algorithms must
// tolerate a branch that has since been closed or errored.
void DefaultControllerEnqueue(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsObject());
  auto* c =
      BaseObject::FromJSObject<ReadableStreamDefaultController>(
          args[0].As<Object>());
  if (c == nullptr || !c->CanCloseOrEnqueue()) return;
  USE(c->EnqueueInternal(args[1]));  // leaves exception pending on size() throw
}

void DefaultControllerClose(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsObject());
  auto* c =
      BaseObject::FromJSObject<ReadableStreamDefaultController>(
          args[0].As<Object>());
  if (c == nullptr) return;
  c->CloseInternal();  // already guarded by CanCloseOrEnqueue
}

void DefaultControllerError(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsObject());
  auto* c =
      BaseObject::FromJSObject<ReadableStreamDefaultController>(
          args[0].As<Object>());
  if (c == nullptr) return;
  c->ErrorInternal(args[1]);  // no-op if the stream is not readable
}

void ExposeReadableStreamConstructors(Environment* env, Local<Object> target) {
  Local<Context> context = env->context();
  Isolate* isolate = env->isolate();
  auto expose = [&](const char* name, Local<FunctionTemplate> tmpl) {
    Local<Function> fn;
    if (tmpl->GetFunction(context).ToLocal(&fn))
      USE(target->Set(context, OneByteString(isolate, name), fn));
  };
  expose("ReadableStream", ReadableStream::GetConstructorTemplate(env));
  expose("ReadableStreamDefaultReader",
         ReadableStreamDefaultReader::GetConstructorTemplate(env));
  expose("ReadableStreamBYOBReader",
         ReadableStreamBYOBReader::GetConstructorTemplate(env));
  expose("ReadableStreamBYOBRequest",
         ReadableStreamBYOBRequest::GetConstructorTemplate(env));
  expose("ReadableStreamDefaultController",
         ReadableStreamDefaultController::GetConstructorTemplate(env));
  expose("ReadableByteStreamController",
         ReadableByteStreamController::GetConstructorTemplate(env));
}

void InitializeReadableStream(Isolate* isolate, Local<ObjectTemplate> target) {
  SetMethod(isolate, target, "createReadableStream", CreateReadableStream);
  SetMethod(
      isolate, target, "createReadableByteStream", CreateReadableByteStream);
  SetMethod(isolate, target, "acquireReadableStreamDefaultReader",
            AcquireReadableStreamDefaultReader);
  SetMethod(isolate, target, "acquireReadableStreamBYOBReader",
            AcquireReadableStreamBYOBReader);
  SetMethod(isolate, target, "readableStreamStateField", ReadableStreamStateField);
  SetMethod(isolate, target, "readableStreamDisturbed", ReadableStreamDisturbed);
  SetMethod(isolate, target, "readableStreamClosedPromise",
            ReadableStreamClosedPromise);
  SetMethod(isolate, target, "readableStreamCancel", ReadableStreamCancel);
  SetMethod(isolate, target, "isReadableStream", IsReadableStream);
  SetMethod(isolate, target, "readableStreamStoredError",
            ReadableStreamStoredError);
  SetMethod(isolate, target, "pipePump", PipePump);
  SetMethod(isolate, target, "pipePumpDisarm", PipePumpDisarm);
  SetMethod(isolate, target, "readerFastRead", ReaderFastRead);
  SetMethod(isolate, target, "readableStreamController",
            ReadableStreamController);
  SetMethod(isolate, target, "readableStreamReleaseReader",
            ReadableStreamReleaseReader);
  SetMethod(isolate, target, "readableStreamDefaultControllerEnqueue",
            DefaultControllerEnqueue);
  SetMethod(isolate, target, "readableStreamDefaultControllerClose",
            DefaultControllerClose);
  SetMethod(isolate, target, "readableStreamDefaultControllerError",
            DefaultControllerError);
}

void RegisterReadableStreamExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(CreateReadableStream);
  registry->Register(CreateReadableByteStream);
  registry->Register(AcquireReadableStreamDefaultReader);
  registry->Register(AcquireReadableStreamBYOBReader);
  registry->Register(ReadableStreamStateField);
  registry->Register(ReadableStreamDisturbed);
  registry->Register(ReadableStreamClosedPromise);
  registry->Register(ReadableStreamCancel);
  registry->Register(IsReadableStream);
  registry->Register(ReadableStreamStoredError);
  registry->Register(PipePump);
  registry->Register(PipePumpDisarm);
  registry->Register(ReaderFastRead);
  registry->Register(ReadableStreamController);
  registry->Register(ReadableStreamReleaseReader);
  registry->Register(DefaultControllerEnqueue);
  registry->Register(DefaultControllerClose);
  registry->Register(DefaultControllerError);
  ReadableStream::RegisterExternalReferences(registry);
  ReadableStreamDefaultController::RegisterExternalReferences(registry);
  ReadableStreamDefaultReader::RegisterExternalReferences(registry);
  ReadableByteStreamController::RegisterExternalReferences(registry);
  ReadableStreamBYOBRequest::RegisterExternalReferences(registry);
  ReadableStreamBYOBReader::RegisterExternalReferences(registry);
}

}  // namespace webstreams
}  // namespace node
