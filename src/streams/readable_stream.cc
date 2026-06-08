#include "streams/readable_stream.h"
#include "streams/streams_binding.h"
#include "base_object-inl.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "node_errors.h"
#include "node_external_reference.h"
#include "node_realm-inl.h"
#include "util-inl.h"
#include "util.h"
#include "v8.h"

#include <cmath>
#include <limits>

namespace node {
namespace webstreams {

using v8::Array;
using v8::Boolean;
using v8::Context;
using v8::Exception;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
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
using v8::Undefined;
using v8::Value;

namespace {

// Recovers a related BaseObject stored in a wrapper's internal field. Returns
// nullptr if the field is unset (undefined) or not the expected type.
template <typename T>
T* GetField(Local<Object> obj, int index) {
  Local<Value> v = obj->GetInternalField(index).template As<Value>();
  if (!v->IsObject()) return nullptr;
  return BaseObject::FromJSObject<T>(v.As<Object>());
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
// wrapper object; the controller is recovered with FromJSObject.
void ReactStartFulfilled(const FunctionCallbackInfo<Value>& args) {
  auto* c =
      BaseObject::FromJSObject<ReadableStreamDefaultController>(args.Data());
  if (c != nullptr) c->OnStartFulfilled();
}

void ReactPullFulfilled(const FunctionCallbackInfo<Value>& args) {
  auto* c =
      BaseObject::FromJSObject<ReadableStreamDefaultController>(args.Data());
  if (c != nullptr) c->FinishPull();
}

void ReactRejected(const FunctionCallbackInfo<Value>& args) {
  auto* c =
      BaseObject::FromJSObject<ReadableStreamDefaultController>(args.Data());
  if (c != nullptr) c->ErrorInternal(args[0]);
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

}  // namespace

// ===========================================================================
// ReadableStreamDefaultController
// ===========================================================================

ReadableStreamDefaultController::ReadableStreamDefaultController(
    Environment* env, Local<Object> object)
    : BaseObject(env, object) {}

void ReadableStreamDefaultController::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("queue", queue_);
  tracker->TrackField("pull_algorithm", pull_algorithm_);
  tracker->TrackField("cancel_algorithm", cancel_algorithm_);
  tracker->TrackField("size_algorithm", size_algorithm_);
}

ReadableStream* ReadableStreamDefaultController::stream() const {
  return GetField<ReadableStream>(object(), kStream);
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
    stream->default_reader()->FulfillFront(chunk);
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
    : BaseObject(env, object) {}

void ReadableStreamDefaultReader::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("closed", closed_);
}

ReadableStream* ReadableStreamDefaultReader::stream() const {
  return GetField<ReadableStream>(object(), kStream);
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

void ReadableStreamDefaultReader::FulfillFront(Local<Value> chunk) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  CHECK(!read_requests_.empty());
  Local<Promise::Resolver> resolver = read_requests_.front().Get(isolate);
  read_requests_.pop_front();
  USE(resolver->Resolve(context, MakeReadResult(isolate, context, chunk, false)));
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
    : BaseObject(env, object) {}

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
  return GetField<ReadableStreamDefaultController>(object(), kController);
}

ReadableStreamDefaultReader* ReadableStream::default_reader() const {
  return GetField<ReadableStreamDefaultReader>(object(), kReader);
}

bool ReadableStream::locked() const {
  Local<Value> v = object()->GetInternalField(kReader).As<Value>();
  return v->IsObject();
}

bool ReadableStream::has_default_reader() const {
  return default_reader() != nullptr;
}

void ReadableStream::SetController(Local<Object> controller_obj) {
  object()->SetInternalField(kController, controller_obj);
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
  Local<Promise> cancel_result = default_controller()->CancelSteps(reason);
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
  ReadableStreamDefaultReader* reader = default_reader();
  if (reader == nullptr) return;
  reader->ResolveClosed();
  reader->CloseAll();
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
  ReadableStreamDefaultReader* reader = default_reader();
  if (reader == nullptr) return;
  reader->RejectClosed(error);
  reader->ErrorAll(error);
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
  stream->SetController(controller_obj);

  if (!controller->Setup(start_algorithm, pull_algorithm, cancel_algorithm,
                         high_water_mark, size_mode, size_algorithm)) {
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

void InitializeReadableStream(Isolate* isolate, Local<ObjectTemplate> target) {
  SetMethod(isolate, target, "createReadableStream", CreateReadableStream);
  SetMethod(isolate, target, "acquireReadableStreamDefaultReader",
            AcquireReadableStreamDefaultReader);
}

void RegisterReadableStreamExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(CreateReadableStream);
  registry->Register(AcquireReadableStreamDefaultReader);
  ReadableStream::RegisterExternalReferences(registry);
  ReadableStreamDefaultController::RegisterExternalReferences(registry);
  ReadableStreamDefaultReader::RegisterExternalReferences(registry);
}

}  // namespace webstreams
}  // namespace node
