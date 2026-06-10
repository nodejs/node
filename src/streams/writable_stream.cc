#include "streams/writable_stream.h"
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
#include <utility>

namespace node {
namespace webstreams {

using v8::Array;
using v8::Boolean;
using v8::Context;
using v8::Exception;
using v8::Function;
using v8::FunctionCallback;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Isolate;
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
using v8::Undefined;
using v8::Value;

namespace {

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
  Local<String> msg = OneByteString(isolate, message);
  Local<Value> err = Exception::TypeError(msg);
  err.As<Object>()
      ->Set(context,
            FIXED_ONE_BYTE_STRING(isolate, "code"),
            FIXED_ONE_BYTE_STRING(isolate, "ERR_INVALID_STATE"))
      .Check();
  return err;
}

void Noop(const FunctionCallbackInfo<Value>& args) {}

// Suppresses spurious unhandled-rejection warnings on internal promises.
void MarkHandled(Environment* env, Local<Promise> promise) {
  if (promise.IsEmpty()) return;
  Local<Context> context = env->context();
  Local<Function> noop;
  if (Function::New(context, Noop).ToLocal(&noop))
    USE(promise->Catch(context, noop));
}

Local<Promise> ResolvedUndefined(Environment* env) {
  Local<Context> context = env->context();
  Local<Promise::Resolver> resolver;
  if (!Promise::Resolver::New(context).ToLocal(&resolver))
    return Local<Promise>();
  USE(resolver->Resolve(context, Undefined(env->isolate())));
  return resolver->GetPromise();
}

Local<Promise> RejectedWith(Environment* env, Local<Value> error) {
  Local<Context> context = env->context();
  Local<Promise::Resolver> resolver;
  if (!Promise::Resolver::New(context).ToLocal(&resolver))
    return Local<Promise>();
  USE(resolver->Reject(context, error));
  return resolver->GetPromise();
}

// Reaction callbacks. args.Data() is the controller (or stream) wrapper.
void ReactStartFulfilled(const FunctionCallbackInfo<Value>& args) {
  auto* c =
      BaseObject::FromJSObject<WritableStreamDefaultController>(args.Data());
  if (c != nullptr) c->OnStartFulfilled();
}
void ReactStartRejected(const FunctionCallbackInfo<Value>& args) {
  auto* c =
      BaseObject::FromJSObject<WritableStreamDefaultController>(args.Data());
  if (c != nullptr) c->OnStartRejected(args[0]);
}
void ReactWriteFulfilled(const FunctionCallbackInfo<Value>& args) {
  auto* c =
      BaseObject::FromJSObject<WritableStreamDefaultController>(args.Data());
  if (c != nullptr) c->OnWriteFulfilled();
}
void ReactWriteRejected(const FunctionCallbackInfo<Value>& args) {
  auto* c =
      BaseObject::FromJSObject<WritableStreamDefaultController>(args.Data());
  if (c != nullptr) c->OnWriteRejected(args[0]);
}
void ReactCloseFulfilled(const FunctionCallbackInfo<Value>& args) {
  auto* c =
      BaseObject::FromJSObject<WritableStreamDefaultController>(args.Data());
  if (c != nullptr) c->OnCloseFulfilled();
}
void ReactCloseRejected(const FunctionCallbackInfo<Value>& args) {
  auto* c =
      BaseObject::FromJSObject<WritableStreamDefaultController>(args.Data());
  if (c != nullptr) c->OnCloseRejected(args[0]);
}
void ReactAbortFulfilled(const FunctionCallbackInfo<Value>& args) {
  auto* s = BaseObject::FromJSObject<WritableStream>(args.Data());
  if (s != nullptr) s->OnAbortAlgorithmFulfilled();
}
void ReactAbortRejected(const FunctionCallbackInfo<Value>& args) {
  auto* s = BaseObject::FromJSObject<WritableStream>(args.Data());
  if (s != nullptr) s->OnAbortAlgorithmRejected(args[0]);
}

void ThenReact(Environment* env,
               Local<Promise> promise,
               Local<Object> data,
               FunctionCallback on_fulfilled,
               FunctionCallback on_rejected) {
  Local<Context> context = env->context();
  Local<Function> ff;
  Local<Function> rj;
  if (!Function::New(context, on_fulfilled, data).ToLocal(&ff) ||
      !Function::New(context, on_rejected, data).ToLocal(&rj)) {
    return;
  }
  USE(promise->Then(context, ff, rj));
}

// Like ThenReact, but reuses cached reaction functions (creating them on first
// use), so the per-write hot path allocates no V8 Function. Mirrors the
// readable side's ThenReactCached.
void ThenReactCached(Environment* env,
                     Local<Promise> promise,
                     Local<Object> data,
                     FunctionCallback on_fulfilled,
                     FunctionCallback on_rejected,
                     v8::Global<Function>* ff_slot,
                     v8::Global<Function>* rj_slot) {
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  Local<Function> ff = ff_slot->Get(isolate);
  if (ff.IsEmpty()) {
    if (!Function::New(context, on_fulfilled, data).ToLocal(&ff)) return;
    ff_slot->Reset(isolate, ff);
  }
  Local<Function> rj = rj_slot->Get(isolate);
  if (rj.IsEmpty()) {
    if (!Function::New(context, on_rejected, data).ToLocal(&rj)) return;
    rj_slot->Reset(isolate, rj);
  }
  USE(promise->Then(context, ff, rj));
}

}  // namespace

// ===========================================================================
// PromiseSlot
// ===========================================================================

void PromiseSlot::SetPending(Environment* env) {
  Isolate* isolate = env->isolate();
  Local<Promise::Resolver> resolver;
  if (!Promise::Resolver::New(env->context()).ToLocal(&resolver)) return;
  resolver_.Reset(isolate, resolver);
  promise_.Reset(isolate, resolver->GetPromise());
}

void PromiseSlot::SetResolved(Environment* env) {
  Isolate* isolate = env->isolate();
  Local<Promise> p = ResolvedUndefined(env);
  resolver_.Reset();
  promise_.Reset(isolate, p);
}

void PromiseSlot::SetRejected(Environment* env, Local<Value> error) {
  Isolate* isolate = env->isolate();
  Local<Promise> p = RejectedWith(env, error);
  resolver_.Reset();
  promise_.Reset(isolate, p);
}

void PromiseSlot::Resolve(Environment* env) {
  if (resolver_.IsEmpty()) return;
  Local<Promise::Resolver> r = resolver_.Get(env->isolate());
  USE(r->Resolve(env->context(), Undefined(env->isolate())));
}

void PromiseSlot::Reject(Environment* env, Local<Value> error) {
  if (resolver_.IsEmpty()) return;
  Local<Promise::Resolver> r = resolver_.Get(env->isolate());
  USE(r->Reject(env->context(), error));
}

bool PromiseSlot::IsPending(Environment* env) const {
  if (promise_.IsEmpty()) return false;
  return promise_.Get(env->isolate())->State() == Promise::PromiseState::kPending;
}

Local<Promise> PromiseSlot::promise(Environment* env) const {
  if (promise_.IsEmpty()) return Local<Promise>();
  return promise_.Get(env->isolate());
}

void PromiseSlot::MemoryInfo(MemoryTracker* tracker, const char* label) const {
  tracker->TrackField(label, promise_);
}

// ===========================================================================
// WritableStreamDefaultController
// ===========================================================================

WritableStreamDefaultController::WritableStreamDefaultController(
    Environment* env, Local<Object> object)
    : StreamBaseObject(env, object, Kind::kWritableStreamDefaultController) {}

void WritableStreamDefaultController::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("queue", queue_);
  tracker->TrackField("write_algorithm", write_algorithm_);
  tracker->TrackField("close_algorithm", close_algorithm_);
  tracker->TrackField("abort_algorithm", abort_algorithm_);
  tracker->TrackField("size_algorithm", size_algorithm_);
}

Local<FunctionTemplate>
WritableStreamDefaultController::GetConstructorTemplate(Environment* env) {
  BindingData* bd = BindingData::Get(env);
  Isolate* isolate = env->isolate();
  Local<FunctionTemplate> tmpl =
      bd->writable_stream_default_controller_ctor.Get(isolate);
  if (tmpl.IsEmpty()) {
    tmpl = NewFunctionTemplate(isolate, nullptr);
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        WritableStreamDefaultController::kInternalFieldCount);
    tmpl->SetClassName(
        FIXED_ONE_BYTE_STRING(isolate, "WritableStreamDefaultController"));
    tmpl->PrototypeTemplate()->SetAccessorProperty(
        FIXED_ONE_BYTE_STRING(isolate, "signal"),
        NewGetter(isolate, "signal", GetSignal));
    SetProtoMethodLen(isolate, tmpl, "error", Error, 0);
    bd->writable_stream_default_controller_ctor.Reset(isolate, tmpl);
  }
  return tmpl;
}

void WritableStreamDefaultController::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(GetSignal);
  registry->Register(Error);
}

Maybe<void> WritableStreamDefaultController::EnqueueValueWithSize(
    Local<Value> value, double size) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  if (std::isnan(size) || size < 0 ||
      size == std::numeric_limits<double>::infinity()) {
    isolate->ThrowException(
        MakeCodedError(isolate, context, /* range */ true,
                       "ERR_INVALID_ARG_VALUE", "The argument 'size' is invalid"));
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

MaybeLocal<Value> WritableStreamDefaultController::DequeueValue() {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  CHECK_GT(queue_size_, 0u);
  Local<Array> queue = queue_.Get(isolate);
  Local<Value> value;
  if (!queue->Get(context, queue_head_).ToLocal(&value))
    return MaybeLocal<Value>();
  USE(queue->Set(context, queue_head_, Undefined(isolate)));
  queue_head_++;
  double size = sizes_.front();
  sizes_.pop_front();
  queue_size_--;
  queue_total_size_ -= size;
  if (queue_total_size_ < 0) queue_total_size_ = 0;
  if (queue_size_ == 0) {
    queue_.Reset();
    queue_head_ = 0;
  }
  return value;
}

void WritableStreamDefaultController::ResetQueue() {
  queue_.Reset();
  sizes_.clear();
  queue_head_ = 0;
  queue_size_ = 0;
  queue_total_size_ = 0;
  close_queued_ = false;
}

double WritableStreamDefaultController::GetDesiredSize() const {
  return high_water_mark_ - queue_total_size_;
}

bool WritableStreamDefaultController::GetBackpressure() const {
  return GetDesiredSize() <= 0;
}

double WritableStreamDefaultController::GetChunkSize(Local<Value> chunk) {
  if (size_mode_ == SizeMode::kCountOne) return 1;
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  TryCatch try_catch(isolate);
  double size = 1;
  bool ok = true;
  if (size_mode_ == SizeMode::kByteLength) {
    Local<Object> obj;
    Local<Value> bl;
    if (!chunk->ToObject(context).ToLocal(&obj) ||
        !obj->Get(context, FIXED_ONE_BYTE_STRING(isolate, "byteLength"))
             .ToLocal(&bl) ||
        !bl->NumberValue(context).To(&size)) {
      ok = false;
    }
  } else {  // kUserFn
    if (size_algorithm_.IsEmpty()) return 1;  // algorithms were cleared
    Local<Function> size_fn = size_algorithm_.Get(isolate);
    Local<Value> argv[] = {chunk};
    Local<Value> size_val;
    if (!size_fn->Call(context, Undefined(isolate), 1, argv).ToLocal(&size_val) ||
        !size_val->NumberValue(context).To(&size)) {
      ok = false;
    }
  }
  if (!ok) {
    Local<Value> exception = try_catch.Exception();
    try_catch.Reset();
    ErrorIfNeeded(exception);
    return 1;
  }
  return size;
}

void WritableStreamDefaultController::Write(Local<Value> chunk,
                                           double chunk_size) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  {
    TryCatch try_catch(isolate);
    if (EnqueueValueWithSize(chunk, chunk_size).IsNothing()) {
      Local<Value> exception = try_catch.Exception();
      try_catch.Reset();
      ErrorIfNeeded(exception);
      return;
    }
  }
  WritableStream* stream = this->stream();
  if (!stream->CloseQueuedOrInFlight() &&
      stream->state() == WritableState::kWritable) {
    stream->UpdateBackpressure(GetBackpressure());
  }
  AdvanceQueueIfNeeded();
}

void WritableStreamDefaultController::ProcessWrite(Local<Value> chunk) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  WritableStream* stream = this->stream();
  stream->MarkFirstWriteRequestInFlight();
  Local<Function> write = write_algorithm_.Get(isolate);
  Local<Object> controller_obj = object();
  // The write algorithm is the user's raw function (no per-call async JS
  // wrapper). The common cases — no write, or a synchronous write returning a
  // non-object — complete through the shared per-realm reaction on a promise
  // resolved with this controller's wrapper: same microtask timing as a
  // promise return, no per-call Function allocation.
  Local<Value> result;
  if (write.IsEmpty()) {
    result = Undefined(isolate);
  } else {
    Local<Value> recv = algo_receiver_.IsEmpty()
                            ? Undefined(isolate).As<Value>()
                            : algo_receiver_.Get(isolate);
    Local<Value> argv[] = {chunk, controller_obj};
    TryCatch try_catch(isolate);
    if (!write->Call(context, recv, 2, argv).ToLocal(&result)) {
      if (try_catch.HasTerminated()) return;
      // A synchronous throw behaves as a rejected write promise.
      Local<Value> exception = try_catch.Exception();
      try_catch.Reset();
      Local<Promise::Resolver> resolver;
      if (!Promise::Resolver::New(context).ToLocal(&resolver)) return;
      USE(resolver->Reject(context, exception));
      ThenReactCached(env, resolver->GetPromise(), controller_obj,
                      ReactWriteFulfilled, ReactWriteRejected,
                      &on_write_fulfilled_, &on_write_rejected_);
      return;
    }
  }
  if (result->IsPromise()) {
    ThenReactCached(env, result.As<Promise>(), controller_obj,
                    ReactWriteFulfilled, ReactWriteRejected,
                    &on_write_fulfilled_, &on_write_rejected_);
    return;
  }
  if (!result->IsObject()) {
    // Synchronous write: enqueue OnWriteFulfilled directly as a microtask via
    // the per-controller cached reaction — one API call, no promise; ordering
    // is identical (CallableTask and PromiseReactionJob share the FIFO queue).
    Local<Function> ff = on_write_fulfilled_.Get(isolate);
    if (ff.IsEmpty()) {
      if (!Function::New(context, ReactWriteFulfilled, controller_obj)
               .ToLocal(&ff))
        return;
      on_write_fulfilled_.Reset(isolate, ff);
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
                  ReactWriteFulfilled, ReactWriteRejected,
                  &on_write_fulfilled_, &on_write_rejected_);
}

void WritableStreamDefaultController::OnWriteFulfilled() {
  WritableStream* stream = this->stream();
  stream->FinishInFlightWrite();
  WritableState state = stream->state();
  CHECK(state == WritableState::kWritable ||
        state == WritableState::kErroring);
  USE(DequeueValue());
  if (!stream->CloseQueuedOrInFlight() && state == WritableState::kWritable)
    stream->UpdateBackpressure(GetBackpressure());
  AdvanceQueueIfNeeded();
}

void WritableStreamDefaultController::OnWriteRejected(Local<Value> error) {
  WritableStream* stream = this->stream();
  if (stream->state() == WritableState::kWritable) ClearAlgorithms();
  stream->FinishInFlightWriteWithError(error);
}

void WritableStreamDefaultController::ProcessClose() {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  WritableStream* stream = this->stream();
  stream->MarkCloseRequestInFlight();
  close_queued_ = false;  // dequeue the close sentinel
  CHECK(QueueIsEmpty());
  Local<Function> close = close_algorithm_.Get(isolate);
  Local<Value> result;
  Local<Object> controller_obj = object();
  // Called with the algorithm receiver (the sink for public streams — their
  // wrapped close ignores `this` — and the transform stream for the shared
  // transform trampolines, which recover it from the receiver).
  Local<Value> close_recv = algo_receiver_.IsEmpty()
                                ? Undefined(isolate).As<Value>()
                                : algo_receiver_.Get(isolate);
  bool got = close->Call(context, close_recv, 0, nullptr).ToLocal(&result);
  ClearAlgorithms();
  if (!got || !result->IsPromise()) return;
  ThenReact(env, result.As<Promise>(), controller_obj, ReactCloseFulfilled,
            ReactCloseRejected);
}

void WritableStreamDefaultController::OnCloseFulfilled() {
  stream()->FinishInFlightClose();
}

void WritableStreamDefaultController::OnCloseRejected(Local<Value> error) {
  stream()->FinishInFlightCloseWithError(error);
}

void WritableStreamDefaultController::ErrorIfNeeded(Local<Value> error) {
  if (stream()->state() == WritableState::kWritable) ErrorInternal(error);
}

void WritableStreamDefaultController::ErrorInternal(Local<Value> error) {
  WritableStream* stream = this->stream();
  CHECK(stream->state() == WritableState::kWritable);
  ClearAlgorithms();
  stream->StartErroring(error);
}

void WritableStreamDefaultController::CloseInternal() {
  close_queued_ = true;  // enqueue the close sentinel (size 0)
  AdvanceQueueIfNeeded();
}

void WritableStreamDefaultController::ClearAlgorithms() {
  write_algorithm_.Reset();
  close_algorithm_.Reset();
  abort_algorithm_.Reset();
  size_algorithm_.Reset();
  algo_receiver_.Reset();
  // Break the Global -> reaction Function -> wrapper -> controller cycle.
  on_write_fulfilled_.Reset();
  on_write_rejected_.Reset();
}

void WritableStreamDefaultController::AdvanceQueueIfNeeded() {
  WritableStream* stream = this->stream();
  if (!started_ || stream->has_in_flight_write_request()) return;
  if (stream->state() == WritableState::kErroring) {
    stream->FinishErroring();
    return;
  }
  if (QueueIsEmpty()) return;
  if (queue_size_ > 0) {
    Local<Value> chunk;
    Local<Context> context = env()->context();
    if (!queue_.Get(env()->isolate())
             ->Get(context, queue_head_)
             .ToLocal(&chunk))
      return;
    ProcessWrite(chunk);
  } else {
    // Only the close sentinel remains.
    ProcessClose();
  }
}

Local<Promise> WritableStreamDefaultController::AbortSteps(Local<Value> reason) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  Local<Function> abort = abort_algorithm_.Get(isolate);
  Local<Value> argv[] = {reason};
  Local<Value> result;
  Local<Promise> promise;
  Local<Value> abort_recv = algo_receiver_.IsEmpty()
                                ? Undefined(isolate).As<Value>()
                                : algo_receiver_.Get(isolate);
  if (!abort.IsEmpty() &&
      abort->Call(context, abort_recv, 1, argv).ToLocal(&result) &&
      result->IsPromise()) {
    promise = result.As<Promise>();
  } else {
    promise = ResolvedUndefined(env);
  }
  ClearAlgorithms();
  return promise;
}

void WritableStreamDefaultController::ErrorSteps() {
  ResetQueue();
}

void WritableStreamDefaultController::OnStartFulfilled() {
  started_ = true;
  AdvanceQueueIfNeeded();
}

void WritableStreamDefaultController::OnStartRejected(Local<Value> error) {
  started_ = true;
  stream()->DealWithRejection(error);
}

bool WritableStreamDefaultController::Setup(
    Local<Function> start_algorithm,
    Local<Function> write_algorithm,
    Local<Function> close_algorithm,
    Local<Function> abort_algorithm,
    double high_water_mark,
    SizeMode size_mode,
    Local<Function> size_algorithm,
    Local<Object> abort_controller,
    Local<Value> algo_receiver) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  high_water_mark_ = high_water_mark;
  size_mode_ = size_mode;
  if (!write_algorithm.IsEmpty())
    write_algorithm_.Reset(isolate, write_algorithm);
  close_algorithm_.Reset(isolate, close_algorithm);
  abort_algorithm_.Reset(isolate, abort_algorithm);
  if (!algo_receiver.IsEmpty() && !algo_receiver->IsUndefined())
    algo_receiver_.Reset(isolate, algo_receiver);
  if (size_mode == SizeMode::kUserFn)
    size_algorithm_.Reset(isolate, size_algorithm);
  object()->SetInternalField(kAbortController, abort_controller);

  WritableStream* stream = this->stream();
  stream->UpdateBackpressure(GetBackpressure());

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
    // Common case (see the readable controllers' Setup): nothing to await, no
    // thenable to chase, no possible rejection; reuse the shared per-realm
    // start reaction.
    USE(resolver->Resolve(context, controller_obj));
    ThenStartFulfilled(env, resolver->GetPromise());
    return true;
  }
  USE(resolver->Resolve(context, start_result));
  ThenReact(env, resolver->GetPromise(), controller_obj, ReactStartFulfilled,
            ReactStartRejected);
  return true;
}

void WritableStreamDefaultController::GetSignal(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Local<Context> context = env->context();
  if (!CheckReceiverInvalidThis(env, GetConstructorTemplate(env), args.This(),
                                "WritableStreamDefaultController"))
    return;
  auto* c = BaseObject::FromJSObject<WritableStreamDefaultController>(
      args.This());
  if (c == nullptr) return;
  Local<Value> ac = c->object()->GetInternalField(kAbortController).As<Value>();
  if (!ac->IsObject()) return;
  Local<Value> signal;
  if (ac.As<Object>()
          ->Get(context, FIXED_ONE_BYTE_STRING(env->isolate(), "signal"))
          .ToLocal(&signal)) {
    args.GetReturnValue().Set(signal);
  }
}

void WritableStreamDefaultController::Error(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (!CheckReceiverInvalidThis(env, GetConstructorTemplate(env), args.This(),
                                "WritableStreamDefaultController"))
    return;
  auto* c = BaseObject::FromJSObject<WritableStreamDefaultController>(
      args.This());
  if (c == nullptr) return;
  if (c->stream()->state() != WritableState::kWritable) return;
  c->ErrorInternal(args[0]);
}

// ===========================================================================
// WritableStreamDefaultWriter
// ===========================================================================

WritableStreamDefaultWriter::WritableStreamDefaultWriter(Environment* env,
                                                         Local<Object> object)
    : StreamBaseObject(env, object, Kind::kWritableStreamDefaultWriter) {}

void WritableStreamDefaultWriter::MemoryInfo(MemoryTracker* tracker) const {
  ready_.MemoryInfo(tracker, "ready");
  closed_.MemoryInfo(tracker, "closed");
}


Local<FunctionTemplate> WritableStreamDefaultWriter::GetConstructorTemplate(
    Environment* env) {
  BindingData* bd = BindingData::Get(env);
  Isolate* isolate = env->isolate();
  Local<FunctionTemplate> tmpl =
      bd->writable_stream_default_writer_ctor.Get(isolate);
  if (tmpl.IsEmpty()) {
    tmpl = NewFunctionTemplate(isolate, nullptr);
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        WritableStreamDefaultWriter::kInternalFieldCount);
    tmpl->SetClassName(
        FIXED_ONE_BYTE_STRING(isolate, "WritableStreamDefaultWriter"));
    tmpl->PrototypeTemplate()->SetAccessorProperty(
        FIXED_ONE_BYTE_STRING(isolate, "closed"),
        NewPromiseGetter(isolate, "closed", GetClosed));
    tmpl->PrototypeTemplate()->SetAccessorProperty(
        FIXED_ONE_BYTE_STRING(isolate, "ready"),
        NewPromiseGetter(isolate, "ready", GetReady));
    tmpl->PrototypeTemplate()->SetAccessorProperty(
        FIXED_ONE_BYTE_STRING(isolate, "desiredSize"),
        NewGetter(isolate, "desiredSize", GetDesiredSize));
    SetProtoMethodPromise(isolate, tmpl, "write", Write, 0);
    SetProtoMethodPromise(isolate, tmpl, "close", Close, 0);
    SetProtoMethodPromise(isolate, tmpl, "abort", Abort, 0);
    SetProtoMethodLen(isolate, tmpl, "releaseLock", ReleaseLock, 0);
    bd->writable_stream_default_writer_ctor.Reset(isolate, tmpl);
  }
  return tmpl;
}

void WritableStreamDefaultWriter::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(GetClosed);
  registry->Register(GetReady);
  registry->Register(GetDesiredSize);
  registry->Register(Write);
  registry->Register(Close);
  registry->Register(Abort);
  registry->Register(ReleaseLock);
}

double WritableStreamDefaultWriter::GetDesiredSizeValue(bool* is_null) const {
  WritableStream* stream = this->stream();
  *is_null = false;
  switch (stream->state()) {
    case WritableState::kErrored:
    case WritableState::kErroring:
      *is_null = true;
      return 0;
    case WritableState::kClosed:
      return 0;
    default:
      return stream->controller()->GetDesiredSize();
  }
}

void WritableStreamDefaultWriter::EnsureReadyPromiseRejected(
    Local<Value> error) {
  Environment* env = this->env();
  if (ready_.IsPending(env))
    ready_.Reject(env, error);
  else
    ready_.SetRejected(env, error);
  MarkHandled(env, ready_.promise(env));
}

void WritableStreamDefaultWriter::EnsureClosedPromiseRejected(
    Local<Value> error) {
  Environment* env = this->env();
  if (closed_.IsPending(env))
    closed_.Reject(env, error);
  else
    closed_.SetRejected(env, error);
  MarkHandled(env, closed_.promise(env));
}

Local<Promise> WritableStreamDefaultWriter::WriteInternal(Local<Value> chunk) {
  Environment* env = this->env();
  WritableStream* stream0 = this->stream();
  CHECK_NOT_NULL(stream0);
  WritableStreamDefaultController* controller = stream0->controller();
  double chunk_size = controller->GetChunkSize(chunk);
  if (this->stream() != stream0) {
    return RejectedWith(env, InvalidStateError(env->isolate(), env->context(),
                                               "Mismatched WritableStreams"));
  }
  WritableState state = stream0->state();
  if (state == WritableState::kErrored)
    return RejectedWith(env, stream0->stored_error(env));
  if (stream0->CloseQueuedOrInFlight() || state == WritableState::kClosed) {
    return RejectedWith(env, InvalidStateError(env->isolate(), env->context(),
                                               "WritableStream is closed"));
  }
  if (state == WritableState::kErroring)
    return RejectedWith(env, stream0->stored_error(env));
  CHECK(state == WritableState::kWritable);
  Local<Promise> promise = stream0->AddWriteRequest();
  controller->Write(chunk, chunk_size);
  return promise;
}

Local<Promise> WritableStreamDefaultWriter::CloseInternal() {
  return stream()->CloseStream();
}

Local<Promise> WritableStreamDefaultWriter::AbortInternal(Local<Value> reason) {
  return stream()->Abort(reason);
}

Local<Promise> WritableStreamDefaultWriter::CloseWithErrorPropagation() {
  Environment* env = this->env();
  WritableStream* stream = this->stream();
  WritableState state = stream->state();
  if (stream->CloseQueuedOrInFlight() || state == WritableState::kClosed)
    return ResolvedUndefined(env);
  if (state == WritableState::kErrored)
    return RejectedWith(env, stream->stored_error(env));
  CHECK(state == WritableState::kWritable ||
        state == WritableState::kErroring);
  return CloseInternal();
}

bool WritableStreamDefaultWriter::SetupInternal(Local<Object> stream_obj) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  auto* stream = BaseObject::FromJSObject<WritableStream>(stream_obj);
  CHECK_NOT_NULL(stream);
  if (stream->locked()) {
    isolate->ThrowException(
        InvalidStateError(isolate, context, "WritableStream is locked"));
    return false;
  }
  object()->SetInternalField(kStream, stream_obj);
  stream_obj->SetInternalField(WritableStream::kWriter, object());
  stream_cache_ = stream;
  stream->set_writer_cache(this);

  switch (stream->state()) {
    case WritableState::kWritable:
      if (!stream->CloseQueuedOrInFlight() && stream->backpressure())
        ready_.SetPending(env);
      else
        ready_.SetResolved(env);
      closed_.SetPending(env);
      break;
    case WritableState::kErroring:
      ready_.SetRejected(env, stream->stored_error(env));
      MarkHandled(env, ready_.promise(env));
      closed_.SetPending(env);
      break;
    case WritableState::kClosed:
      ready_.SetResolved(env);
      closed_.SetResolved(env);
      break;
    case WritableState::kErrored:
      ready_.SetRejected(env, stream->stored_error(env));
      closed_.SetRejected(env, stream->stored_error(env));
      MarkHandled(env, ready_.promise(env));
      MarkHandled(env, closed_.promise(env));
      break;
  }
  return true;
}

void WritableStreamDefaultWriter::Release() {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  WritableStream* stream = this->stream();
  CHECK_NOT_NULL(stream);
  Local<Value> released_error = InvalidStateError(
      isolate, context,
      "Writer was released and can no longer be used to monitor the stream's "
      "state");
  EnsureReadyPromiseRejected(released_error);
  EnsureClosedPromiseRejected(released_error);
  stream->object()->SetInternalField(WritableStream::kWriter,
                                     Undefined(isolate));
  object()->SetInternalField(kStream, Undefined(isolate));
  stream_cache_ = nullptr;
  stream->set_writer_cache(nullptr);
}

void WritableStreamDefaultWriter::GetClosed(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (!WritableStreamDefaultWriter::GetConstructorTemplate(env)->HasInstance(
          args.This())) {
    args.GetReturnValue().Set(IllegalInvocationRejection(env->context()));
    return;
  }
  auto* w =
      BaseObject::FromJSObject<WritableStreamDefaultWriter>(args.This());
  args.GetReturnValue().Set(w->closed_.promise(env));
}

void WritableStreamDefaultWriter::GetReady(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (!WritableStreamDefaultWriter::GetConstructorTemplate(env)->HasInstance(
          args.This())) {
    args.GetReturnValue().Set(IllegalInvocationRejection(env->context()));
    return;
  }
  auto* w =
      BaseObject::FromJSObject<WritableStreamDefaultWriter>(args.This());
  args.GetReturnValue().Set(w->ready_.promise(env));
}

void WritableStreamDefaultWriter::GetDesiredSize(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  if (!CheckReceiverInvalidThis(env, GetConstructorTemplate(env), args.This(),
                                "WritableStreamDefaultWriter"))
    return;
  auto* w =
      BaseObject::FromJSObject<WritableStreamDefaultWriter>(args.This());
  if (w == nullptr) return;
  if (!w->has_stream()) {
    isolate->ThrowException(InvalidStateError(
        isolate, context, "Writer is not bound to a WritableStream"));
    return;
  }
  bool is_null = false;
  double desired = w->GetDesiredSizeValue(&is_null);
  if (is_null)
    args.GetReturnValue().SetNull();
  else
    args.GetReturnValue().Set(desired);
}

void WritableStreamDefaultWriter::Write(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  if (!WritableStreamDefaultWriter::GetConstructorTemplate(env)->HasInstance(
          args.This())) {
    args.GetReturnValue().Set(IllegalInvocationRejection(context));
    return;
  }
  auto* w =
      BaseObject::FromJSObject<WritableStreamDefaultWriter>(args.This());
  if (!w->has_stream()) {
    args.GetReturnValue().Set(RejectedWith(
        env, InvalidStateError(isolate, context,
                               "Writer is not bound to a WritableStream")));
    return;
  }
  args.GetReturnValue().Set(w->WriteInternal(args[0]));
}

void WritableStreamDefaultWriter::Close(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  if (!WritableStreamDefaultWriter::GetConstructorTemplate(env)->HasInstance(
          args.This())) {
    args.GetReturnValue().Set(IllegalInvocationRejection(context));
    return;
  }
  auto* w =
      BaseObject::FromJSObject<WritableStreamDefaultWriter>(args.This());
  if (!w->has_stream()) {
    args.GetReturnValue().Set(RejectedWith(
        env, InvalidStateError(isolate, context,
                               "Writer is not bound to a WritableStream")));
    return;
  }
  if (w->stream()->CloseQueuedOrInFlight()) {
    args.GetReturnValue().Set(RejectedWith(
        env, InvalidStateError(isolate, context,
                               "Failure to close WritableStream")));
    return;
  }
  args.GetReturnValue().Set(w->CloseInternal());
}

void WritableStreamDefaultWriter::Abort(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  if (!WritableStreamDefaultWriter::GetConstructorTemplate(env)->HasInstance(
          args.This())) {
    args.GetReturnValue().Set(IllegalInvocationRejection(context));
    return;
  }
  auto* w =
      BaseObject::FromJSObject<WritableStreamDefaultWriter>(args.This());
  if (!w->has_stream()) {
    args.GetReturnValue().Set(RejectedWith(
        env, InvalidStateError(isolate, context,
                               "Writer is not bound to a WritableStream")));
    return;
  }
  args.GetReturnValue().Set(w->AbortInternal(args[0]));
}

void WritableStreamDefaultWriter::ReleaseLock(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (!CheckReceiverInvalidThis(env, GetConstructorTemplate(env), args.This(),
                                "WritableStreamDefaultWriter"))
    return;
  auto* w =
      BaseObject::FromJSObject<WritableStreamDefaultWriter>(args.This());
  if (w == nullptr) return;
  if (!w->has_stream()) return;
  w->Release();
}

// ===========================================================================
// WritableStream
// ===========================================================================

WritableStream::WritableStream(Environment* env, Local<Object> object)
    : StreamBaseObject(env, object, Kind::kWritableStream) {}

void WritableStream::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("stored_error", stored_error_);
  tracker->TrackField("close_request", close_request_);
  tracker->TrackField("in_flight_write_request", in_flight_write_request_);
  tracker->TrackField("in_flight_close_request", in_flight_close_request_);
  tracker->TrackField("closed", closed_);
}

Local<FunctionTemplate> WritableStream::GetConstructorTemplate(
    Environment* env) {
  BindingData* bd = BindingData::Get(env);
  Isolate* isolate = env->isolate();
  Local<FunctionTemplate> tmpl = bd->writable_stream_ctor.Get(isolate);
  if (tmpl.IsEmpty()) {
    tmpl = NewFunctionTemplate(isolate, nullptr);
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        WritableStream::kInternalFieldCount);
    tmpl->SetClassName(FIXED_ONE_BYTE_STRING(isolate, "WritableStream"));
    tmpl->PrototypeTemplate()->SetAccessorProperty(
        FIXED_ONE_BYTE_STRING(isolate, "locked"),
        NewGetter(isolate, "locked", GetLocked));
    SetProtoMethodPromise(isolate, tmpl, "abort", Abort, 0);
    SetProtoMethodPromise(isolate, tmpl, "close", Close, 0);
    bd->writable_stream_ctor.Reset(isolate, tmpl);
  }
  return tmpl;
}

void WritableStream::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(GetLocked);
  registry->Register(Abort);
  registry->Register(Close);
}

bool WritableStream::HasInstance(Environment* env, Local<Value> value) {
  return GetConstructorTemplate(env)->HasInstance(value);
}

Local<Value> WritableStream::stored_error(Environment* env) const {
  if (stored_error_.IsEmpty()) return Undefined(env->isolate());
  return stored_error_.Get(env->isolate());
}

void WritableStream::SetController(Local<Object> controller_obj) {
  object()->SetInternalField(kController, controller_obj);
  controller_obj->SetInternalField(
      WritableStreamDefaultController::kStream, object());
  // Mirror the traced fields into the raw-pointer caches (hot-path accessors).
  auto* controller =
      BaseObject::FromJSObject<WritableStreamDefaultController>(controller_obj);
  CHECK_NOT_NULL(controller);
  controller_cache_ = controller;
  controller->set_stream_cache(this);
}

void WritableStream::SetWriter(Local<Object> writer_obj) {
  object()->SetInternalField(kWriter, writer_obj);
  writer_cache_ =
      BaseObject::FromJSObject<WritableStreamDefaultWriter>(writer_obj);
}

void WritableStream::ClearWriter() {
  object()->SetInternalField(kWriter, Undefined(env()->isolate()));
  writer_cache_ = nullptr;
}

Local<Promise> WritableStream::closed_promise(Environment* env) {
  Isolate* isolate = env->isolate();
  Local<Promise::Resolver> resolver = closed_.Get(isolate);
  if (resolver.IsEmpty()) {
    if (!Promise::Resolver::New(env->context()).ToLocal(&resolver))
      return Local<Promise>();
    closed_.Reset(isolate, resolver);
    // Settle immediately if the stream is already in a terminal state, so a
    // late requester (e.g. node:stream `finished()`) does not hang.
    if (state_ == WritableState::kClosed) {
      USE(resolver->Resolve(env->context(), Undefined(isolate)));
    } else if (state_ == WritableState::kErrored) {
      Local<Promise> p = resolver->GetPromise();
      USE(resolver->Reject(env->context(), stored_error(env)));
      MarkHandled(env, p);
    }
  }
  return resolver->GetPromise();
}

Local<Promise> WritableStream::Abort(Local<Value> reason) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  if (state_ == WritableState::kClosed || state_ == WritableState::kErrored)
    return ResolvedUndefined(env);

  // controller.abortController.abort(reason)
  WritableStreamDefaultController* controller = this->controller();
  Local<Value> ac =
      controller->object()
          ->GetInternalField(WritableStreamDefaultController::kAbortController)
          .As<Value>();
  if (ac->IsObject()) {
    Local<Value> abort_fn;
    if (ac.As<Object>()
            ->Get(context, FIXED_ONE_BYTE_STRING(isolate, "abort"))
            .ToLocal(&abort_fn) &&
        abort_fn->IsFunction()) {
      Local<Value> argv[] = {reason};
      USE(abort_fn.As<Function>()->Call(context, ac, 1, argv));
    }
  }

  // Signalling abort may run a synchronous abort handler that transitions the
  // stream to a terminal state; re-read it (spec: WritableStreamAbort steps
  // after signalling) and resolve early instead of asserting.
  if (state_ == WritableState::kClosed || state_ == WritableState::kErrored)
    return ResolvedUndefined(env);

  if (has_pending_abort_)
    return pending_abort_promise_.Get(isolate)->GetPromise();

  CHECK(state_ == WritableState::kWritable ||
        state_ == WritableState::kErroring);

  bool was_already_erroring = false;
  if (state_ == WritableState::kErroring) {
    was_already_erroring = true;
    reason = Undefined(isolate);
  }

  Local<Promise::Resolver> resolver;
  if (!Promise::Resolver::New(context).ToLocal(&resolver))
    return Local<Promise>();
  has_pending_abort_ = true;
  pending_abort_promise_.Reset(isolate, resolver);
  pending_abort_reason_.Reset(isolate, reason);
  pending_abort_was_already_erroring_ = was_already_erroring;

  if (!was_already_erroring) StartErroring(reason);

  return resolver->GetPromise();
}

Local<Promise> WritableStream::CloseStream() {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  if (state_ == WritableState::kClosed || state_ == WritableState::kErrored) {
    return RejectedWith(
        env, InvalidStateError(isolate, context, "WritableStream is closed"));
  }
  CHECK(state_ == WritableState::kWritable ||
        state_ == WritableState::kErroring);
  CHECK(!CloseQueuedOrInFlight());
  Local<Promise::Resolver> resolver;
  if (!Promise::Resolver::New(context).ToLocal(&resolver))
    return Local<Promise>();
  close_request_.Reset(isolate, resolver);
  Local<Promise> promise = resolver->GetPromise();
  WritableStreamDefaultWriter* writer = this->writer();
  if (writer != nullptr && backpressure_ && state_ == WritableState::kWritable)
    writer->ready().Resolve(env);
  controller()->CloseInternal();
  return promise;
}

void WritableStream::UpdateBackpressure(bool backpressure) {
  CHECK(state_ == WritableState::kWritable);
  CHECK(!CloseQueuedOrInFlight());
  Environment* env = this->env();
  WritableStreamDefaultWriter* writer = this->writer();
  if (writer != nullptr && backpressure_ != backpressure) {
    if (backpressure)
      writer->ready().SetPending(env);
    else
      writer->ready().Resolve(env);
  }
  backpressure_ = backpressure;
}

void WritableStream::StartErroring(Local<Value> reason) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  CHECK(stored_error_.IsEmpty());
  CHECK(state_ == WritableState::kWritable);
  WritableStreamDefaultController* controller = this->controller();
  CHECK_NOT_NULL(controller);
  state_ = WritableState::kErroring;
  stored_error_.Reset(isolate, reason);
  WritableStreamDefaultWriter* writer = this->writer();
  if (writer != nullptr) writer->EnsureReadyPromiseRejected(reason);
  if (!HasOperationMarkedInFlight() && controller->started())
    FinishErroring();
}

void WritableStream::RejectCloseAndClosedPromiseIfNeeded() {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  CHECK(state_ == WritableState::kErrored);
  Local<Value> error = stored_error(env);
  if (!close_request_.IsEmpty()) {
    CHECK(in_flight_close_request_.IsEmpty());
    USE(close_request_.Get(isolate)->Reject(context, error));
    close_request_.Reset();
  }
  {
    Local<Promise::Resolver> resolver = closed_.Get(isolate);
    if (!resolver.IsEmpty()) {
      Local<Promise> p = resolver->GetPromise();
      USE(resolver->Reject(context, error));
      MarkHandled(env, p);
    }
  }
  WritableStreamDefaultWriter* writer = this->writer();
  if (writer != nullptr) {
    writer->closed().Reject(env, error);
    MarkHandled(env, writer->closed().promise(env));
  }
}

void WritableStream::MarkFirstWriteRequestInFlight() {
  CHECK(in_flight_write_request_.IsEmpty());
  CHECK(!write_requests_.empty());
  in_flight_write_request_ = std::move(write_requests_.front());
  write_requests_.pop_front();
}

void WritableStream::MarkCloseRequestInFlight() {
  CHECK(in_flight_close_request_.IsEmpty());
  CHECK(!close_request_.IsEmpty());
  in_flight_close_request_ = std::move(close_request_);
  close_request_.Reset();
}

bool WritableStream::HasOperationMarkedInFlight() const {
  return !in_flight_write_request_.IsEmpty() ||
         !in_flight_close_request_.IsEmpty();
}

void WritableStream::FinishInFlightWrite() {
  Environment* env = this->env();
  CHECK(!in_flight_write_request_.IsEmpty());
  USE(in_flight_write_request_.Get(env->isolate())
          ->Resolve(env->context(), Undefined(env->isolate())));
  in_flight_write_request_.Reset();
}

void WritableStream::FinishInFlightWriteWithError(Local<Value> error) {
  Environment* env = this->env();
  CHECK(!in_flight_write_request_.IsEmpty());
  USE(in_flight_write_request_.Get(env->isolate())
          ->Reject(env->context(), error));
  in_flight_write_request_.Reset();
  CHECK(state_ == WritableState::kWritable ||
        state_ == WritableState::kErroring);
  DealWithRejection(error);
}

void WritableStream::FinishInFlightClose() {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  CHECK(!in_flight_close_request_.IsEmpty());
  USE(in_flight_close_request_.Get(isolate)
          ->Resolve(env->context(), Undefined(isolate)));
  in_flight_close_request_.Reset();
  if (state_ == WritableState::kErroring) {
    stored_error_.Reset();
    if (has_pending_abort_) {
      USE(pending_abort_promise_.Get(isolate)
              ->Resolve(env->context(), Undefined(isolate)));
      has_pending_abort_ = false;
      pending_abort_promise_.Reset();
      pending_abort_reason_.Reset();
      pending_abort_was_already_erroring_ = false;
    }
  }
  state_ = WritableState::kClosed;
  WritableStreamDefaultWriter* writer = this->writer();
  if (writer != nullptr) writer->closed().Resolve(env);
  {
    Local<Promise::Resolver> resolver = closed_.Get(isolate);
    if (!resolver.IsEmpty())
      USE(resolver->Resolve(env->context(), Undefined(isolate)));
  }
}

void WritableStream::FinishInFlightCloseWithError(Local<Value> error) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  CHECK(!in_flight_close_request_.IsEmpty());
  USE(in_flight_close_request_.Get(isolate)->Reject(env->context(), error));
  in_flight_close_request_.Reset();
  CHECK(state_ == WritableState::kWritable ||
        state_ == WritableState::kErroring);
  if (has_pending_abort_) {
    USE(pending_abort_promise_.Get(isolate)->Reject(env->context(), error));
    has_pending_abort_ = false;
    pending_abort_promise_.Reset();
    pending_abort_reason_.Reset();
    pending_abort_was_already_erroring_ = false;
  }
  DealWithRejection(error);
}

void WritableStream::FinishErroring() {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  CHECK(state_ == WritableState::kErroring);
  CHECK(!HasOperationMarkedInFlight());
  state_ = WritableState::kErrored;
  controller()->ErrorSteps();
  Local<Value> stored = stored_error(env);
  while (!write_requests_.empty()) {
    USE(write_requests_.front().Get(isolate)->Reject(context, stored));
    write_requests_.pop_front();
  }

  if (!has_pending_abort_) {
    RejectCloseAndClosedPromiseIfNeeded();
    return;
  }

  bool was_already_erroring = pending_abort_was_already_erroring_;
  Local<Value> reason = pending_abort_reason_.IsEmpty()
                            ? Undefined(isolate).As<Value>()
                            : pending_abort_reason_.Get(isolate);
  Local<Promise::Resolver> abort_resolver = pending_abort_promise_.Get(isolate);
  has_pending_abort_ = false;
  pending_abort_reason_.Reset();
  pending_abort_was_already_erroring_ = false;
  // Keep the resolver reachable for the reaction.
  pending_abort_promise_.Reset();

  if (was_already_erroring) {
    USE(abort_resolver->Reject(context, stored));
    MarkHandled(env, abort_resolver->GetPromise());
    RejectCloseAndClosedPromiseIfNeeded();
    return;
  }

  processing_abort_resolver_.Reset(isolate, abort_resolver);
  Local<Promise> p = controller()->AbortSteps(reason);
  ThenReact(env, p, object(), ReactAbortFulfilled, ReactAbortRejected);
}

void WritableStream::OnAbortAlgorithmFulfilled() {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Promise::Resolver> resolver = processing_abort_resolver_.Get(isolate);
  processing_abort_resolver_.Reset();
  if (!resolver.IsEmpty())
    USE(resolver->Resolve(env->context(), Undefined(isolate)));
  RejectCloseAndClosedPromiseIfNeeded();
}

void WritableStream::OnAbortAlgorithmRejected(Local<Value> error) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Promise::Resolver> resolver = processing_abort_resolver_.Get(isolate);
  processing_abort_resolver_.Reset();
  if (!resolver.IsEmpty())
    USE(resolver->Reject(env->context(), error));
  RejectCloseAndClosedPromiseIfNeeded();
}

void WritableStream::DealWithRejection(Local<Value> error) {
  if (state_ == WritableState::kWritable) {
    StartErroring(error);
    return;
  }
  CHECK(state_ == WritableState::kErroring);
  FinishErroring();
}

bool WritableStream::CloseQueuedOrInFlight() const {
  return !close_request_.IsEmpty() || !in_flight_close_request_.IsEmpty();
}

Local<Promise> WritableStream::AddWriteRequest() {
  Environment* env = this->env();
  Local<Promise::Resolver> resolver;
  if (!Promise::Resolver::New(env->context()).ToLocal(&resolver))
    return Local<Promise>();
  write_requests_.emplace_back(env->isolate(), resolver);
  return resolver->GetPromise();
}

void WritableStream::GetLocked(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (!CheckReceiverInvalidThis(env, GetConstructorTemplate(env), args.This(),
                                "WritableStream"))
    return;
  auto* stream = BaseObject::FromJSObject<WritableStream>(args.This());
  if (stream == nullptr) return;
  args.GetReturnValue().Set(stream->locked());
}

void WritableStream::Abort(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  if (!WritableStream::HasInstance(env, args.This())) {
    args.GetReturnValue().Set(IllegalInvocationRejection(context));
    return;
  }
  auto* stream = BaseObject::FromJSObject<WritableStream>(args.This());
  if (stream->locked()) {
    args.GetReturnValue().Set(RejectedWith(
        env, InvalidStateError(isolate, context, "WritableStream is locked")));
    return;
  }
  args.GetReturnValue().Set(stream->Abort(args[0]));
}

void WritableStream::Close(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  if (!WritableStream::HasInstance(env, args.This())) {
    args.GetReturnValue().Set(IllegalInvocationRejection(context));
    return;
  }
  auto* stream = BaseObject::FromJSObject<WritableStream>(args.This());
  if (stream->locked()) {
    args.GetReturnValue().Set(RejectedWith(
        env, InvalidStateError(isolate, context, "WritableStream is locked")));
    return;
  }
  if (stream->CloseQueuedOrInFlight()) {
    args.GetReturnValue().Set(RejectedWith(
        env, InvalidStateError(isolate, context,
                               "Failure closing WritableStream")));
    return;
  }
  args.GetReturnValue().Set(stream->CloseStream());
}

// ===========================================================================
// Binding entry points
// ===========================================================================

MaybeLocal<Object> NewWritableStream(Environment* env,
                                     Local<Function> start_algorithm,
                                     Local<Function> write_algorithm,
                                     Local<Function> close_algorithm,
                                     Local<Function> abort_algorithm,
                                     double high_water_mark,
                                     SizeMode size_mode,
                                     Local<Function> size_algorithm,
                                     Local<Object> abort_controller,
                                     Local<Value> algo_receiver) {
  Local<Context> context = env->context();
  Local<Function> stream_ctor;
  if (!WritableStream::GetConstructorTemplate(env)
           ->GetFunction(context)
           .ToLocal(&stream_ctor))
    return MaybeLocal<Object>();
  Local<Object> stream_obj;
  if (!stream_ctor->NewInstance(context).ToLocal(&stream_obj))
    return MaybeLocal<Object>();
  BaseObjectPtr<WritableStream> stream =
      MakeBaseObject<WritableStream>(env, stream_obj);
  stream->MakeWeak();

  Local<Function> controller_ctor;
  if (!WritableStreamDefaultController::GetConstructorTemplate(env)
           ->GetFunction(context)
           .ToLocal(&controller_ctor))
    return MaybeLocal<Object>();
  Local<Object> controller_obj;
  if (!controller_ctor->NewInstance(context).ToLocal(&controller_obj))
    return MaybeLocal<Object>();
  BaseObjectPtr<WritableStreamDefaultController> controller =
      MakeBaseObject<WritableStreamDefaultController>(env, controller_obj);
  controller->MakeWeak();

  stream->SetController(controller_obj);

  if (!controller->Setup(start_algorithm, write_algorithm, close_algorithm,
                         abort_algorithm, high_water_mark, size_mode,
                         size_algorithm, abort_controller, algo_receiver)) {
    return MaybeLocal<Object>();  // start threw synchronously; exception pending.
  }
  return stream_obj;
}

void CreateWritableStream(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  // args: (start, write, close, abort, highWaterMark, sizeMode, size,
  //        abortController, algoReceiver)
  // write is the user's RAW write (or undefined), called with algoReceiver
  // (the underlying sink) as `this`.
  CHECK(args[0]->IsFunction());
  CHECK(args[2]->IsFunction());
  CHECK(args[3]->IsFunction());
  CHECK(args[4]->IsNumber());
  CHECK(args[5]->IsUint32());
  CHECK(args[7]->IsObject());
  Local<Function> write_algorithm;
  if (args[1]->IsFunction()) write_algorithm = args[1].As<Function>();
  Local<Function> size_algorithm;
  if (args[6]->IsFunction()) size_algorithm = args[6].As<Function>();
  Local<Object> stream_obj;
  if (!NewWritableStream(
           env, args[0].As<Function>(), write_algorithm,
           args[2].As<Function>(), args[3].As<Function>(),
           args[4].As<Number>()->Value(),
           static_cast<SizeMode>(args[5].As<v8::Uint32>()->Value()),
           size_algorithm, args[7].As<Object>(), args[8])
           .ToLocal(&stream_obj)) {
    return;
  }
  args.GetReturnValue().Set(stream_obj);
}

void AcquireWritableStreamDefaultWriter(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Local<Context> context = env->context();
  CHECK(args[0]->IsObject());
  Local<Object> stream_obj = args[0].As<Object>();

  Local<Function> writer_ctor;
  if (!WritableStreamDefaultWriter::GetConstructorTemplate(env)
           ->GetFunction(context)
           .ToLocal(&writer_ctor))
    return;
  Local<Object> writer_obj;
  if (!writer_ctor->NewInstance(context).ToLocal(&writer_obj)) return;
  BaseObjectPtr<WritableStreamDefaultWriter> writer =
      MakeBaseObject<WritableStreamDefaultWriter>(env, writer_obj);
  writer->MakeWeak();

  if (!writer->SetupInternal(stream_obj)) return;  // throws on lock
  args.GetReturnValue().Set(writer_obj);
}

// Internal introspection for the node:stream interop layer
// (kIsErrored/kIsWritable/kIsClosedPromise). Binding functions, not prototype
// properties, so the public WebIDL surface is unchanged.
void WritableStreamStateField(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsObject());
  auto* s = BaseObject::FromJSObject<WritableStream>(args[0].As<Object>());
  if (s == nullptr) return;
  args.GetReturnValue().Set(static_cast<uint32_t>(s->state()));
}

void WritableStreamClosedPromise(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsObject());
  auto* s = BaseObject::FromJSObject<WritableStream>(args[0].As<Object>());
  if (s == nullptr) return;
  Local<Promise> p = s->closed_promise(env);
  if (!p.IsEmpty()) args.GetReturnValue().Set(p);
}

// The stream's stored error; used by pipeTo's synchronous priority checks
// (isOrBecomesErrored) to act on an already-errored destination in order.
void WritableStreamStoredError(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsObject());
  auto* s = BaseObject::FromJSObject<WritableStream>(args[0].As<Object>());
  if (s == nullptr) return;
  args.GetReturnValue().Set(s->stored_error(env));
}

// Whether the stream has a close request queued or in flight; used by pipeTo to
// classify the destination state (close-queued vs errored vs writable).
void WritableStreamCloseQueuedOrInFlight(
    const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsObject());
  auto* s = BaseObject::FromJSObject<WritableStream>(args[0].As<Object>());
  if (s == nullptr) return;
  args.GetReturnValue().Set(s->CloseQueuedOrInFlight());
}

// Introspection helper for a controller's custom inspect: returns the
// WritableStream a WritableStreamDefaultController belongs to. Internal-only
// (the public surface exposes no controller->stream link).
void WritableStreamControllerStream(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (!WritableStreamDefaultController::GetConstructorTemplate(env)
           ->HasInstance(args[0])) {
    return;
  }
  auto* c = BaseObject::FromJSObject<WritableStreamDefaultController>(
      args[0].As<Object>());
  if (c == nullptr) return;
  args.GetReturnValue().Set(c->stream()->object());
}

void ExposeWritableStreamConstructors(Environment* env, Local<Object> target) {
  Local<Context> context = env->context();
  Isolate* isolate = env->isolate();
  auto expose = [&](const char* name, Local<FunctionTemplate> tmpl) {
    Local<Function> fn;
    if (tmpl->GetFunction(context).ToLocal(&fn))
      USE(target->Set(context, OneByteString(isolate, name), fn));
  };
  expose("WritableStream", WritableStream::GetConstructorTemplate(env));
  expose("WritableStreamDefaultWriter",
         WritableStreamDefaultWriter::GetConstructorTemplate(env));
  expose("WritableStreamDefaultController",
         WritableStreamDefaultController::GetConstructorTemplate(env));
}

void InitializeWritableStream(Isolate* isolate, Local<ObjectTemplate> target) {
  SetMethod(isolate, target, "createWritableStream", CreateWritableStream);
  SetMethod(isolate, target, "acquireWritableStreamDefaultWriter",
            AcquireWritableStreamDefaultWriter);
  SetMethod(isolate, target, "writableStreamStateField", WritableStreamStateField);
  SetMethod(isolate, target, "writableStreamClosedPromise",
            WritableStreamClosedPromise);
  SetMethod(isolate, target, "writableStreamCloseQueuedOrInFlight",
            WritableStreamCloseQueuedOrInFlight);
  SetMethod(isolate, target, "writableStreamStoredError",
            WritableStreamStoredError);
  SetMethod(isolate, target, "writableStreamControllerStream",
            WritableStreamControllerStream);
}

void RegisterWritableStreamExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(CreateWritableStream);
  registry->Register(AcquireWritableStreamDefaultWriter);
  registry->Register(WritableStreamStateField);
  registry->Register(WritableStreamClosedPromise);
  registry->Register(WritableStreamCloseQueuedOrInFlight);
  registry->Register(WritableStreamStoredError);
  registry->Register(WritableStreamControllerStream);
  WritableStream::RegisterExternalReferences(registry);
  WritableStreamDefaultController::RegisterExternalReferences(registry);
  WritableStreamDefaultWriter::RegisterExternalReferences(registry);
}

}  // namespace webstreams
}  // namespace node
