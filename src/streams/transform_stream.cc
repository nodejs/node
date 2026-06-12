#include "streams/transform_stream.h"
#include "streams/readable_stream.h"
#include "streams/writable_stream.h"
#include "streams/streams_binding.h"
#include "base_object-inl.h"
#include "cppgc/allocation.h"
#include "cppgc/visitor.h"
#include "cppgc_helpers-inl.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "node_errors.h"
#include "node_external_reference.h"
#include "node_realm-inl.h"
#include "util-inl.h"
#include "util.h"
#include "v8.h"

namespace node {
namespace webstreams {

using v8::Array;
using v8::Context;
using v8::Exception;
using v8::Function;
using v8::FunctionCallback;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
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

Local<Promise> ResolvedUndefined(Environment* env) {
  Local<Context> context = env->context();
  Local<Promise::Resolver> resolver;
  if (!Promise::Resolver::New(context).ToLocal(&resolver))
    return Local<Promise>();
  USE(resolver->Resolve(context, Undefined(env->isolate())));
  return resolver->GetPromise();
}

// Attaches fulfill/reject reactions sharing the same Data object.
void ThenReact(Environment* env,
               Local<Promise> promise,
               Local<Value> data,
               FunctionCallback on_fulfilled,
               FunctionCallback on_rejected) {
  Local<Context> context = env->context();
  Local<Function> ff;
  Local<Function> rj;
  if (!Function::New(context, on_fulfilled, data, 0,
                     v8::ConstructorBehavior::kThrow)
           .ToLocal(&ff) ||
      !Function::New(context, on_rejected, data, 0,
                     v8::ConstructorBehavior::kThrow)
           .ToLocal(&rj)) {
    return;
  }
  USE(promise->Then(context, ff, rj));
}

Local<Array> MakeHolder(Isolate* isolate,
                        Local<Context> context,
                        Local<Value> a,
                        Local<Value> b) {
  Local<Array> holder = Array::New(isolate, 2);
  USE(holder->Set(context, 0, a));
  USE(holder->Set(context, 1, b));
  return holder;
}

// --- reaction callbacks ---

void PerformTransformRejected(const FunctionCallbackInfo<Value>& args) {
  auto* stream = CppgcMixin::Unwrap<TransformStream>(args.Data().As<Object>());
  Isolate* isolate = args.GetIsolate();
  if (stream != nullptr) stream->ErrorStream(args[0]);
  isolate->ThrowException(args[0]);  // rethrow so the derived promise rejects
}

void SinkWriteAfterBackpressure(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  auto* stream = CppgcMixin::Unwrap<TransformStream>(args.Data().As<Object>());
  if (stream == nullptr) return;
  // Take the chunk unconditionally so the slot is free for the next write
  // even when the wait ends in an erroring writable.
  Local<Value> chunk = stream->TakePendingWriteChunk();
  WritableStream* writable = stream->writable();
  if (writable->state() == WritableState::kErroring) {
    isolate->ThrowException(writable->stored_error(env));
    return;
  }
  args.GetReturnValue().Set(stream->controller()->PerformTransform(chunk));
}

void SinkCloseFulfilled(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  auto* stream = CppgcMixin::Unwrap<TransformStream>(args.Data().As<Object>());
  if (stream == nullptr) return;
  ReadableStream* readable = stream->readable();
  TransformStreamDefaultController* controller = stream->controller();
  if (readable->state() == StreamState::kErrored) {
    controller->RejectFinish(env, readable->stored_error(env));
  } else {
    readable->default_controller()->CloseInternal();
    controller->ResolveFinish(env);
  }
}

void SinkCloseRejected(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  auto* stream = CppgcMixin::Unwrap<TransformStream>(args.Data().As<Object>());
  if (stream == nullptr) return;
  stream->readable()->default_controller()->ErrorInternal(args[0]);
  stream->controller()->RejectFinish(env, args[0]);
}

void SinkAbortFulfilled(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Local<Context> context = env->context();
  Local<Array> holder = args.Data().As<Array>();
  Local<Value> stream_val;
  Local<Value> reason;
  if (!holder->Get(context, 0).ToLocal(&stream_val) ||
      !holder->Get(context, 1).ToLocal(&reason))
    return;
  auto* stream = CppgcMixin::Unwrap<TransformStream>(stream_val.As<Object>());
  if (stream == nullptr) return;
  ReadableStream* readable = stream->readable();
  TransformStreamDefaultController* controller = stream->controller();
  if (readable->state() == StreamState::kErrored) {
    controller->RejectFinish(env, readable->stored_error(env));
  } else {
    readable->default_controller()->ErrorInternal(reason);
    controller->ResolveFinish(env);
  }
}

void SinkAbortRejected(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Local<Context> context = env->context();
  Local<Array> holder = args.Data().As<Array>();
  Local<Value> stream_val;
  if (!holder->Get(context, 0).ToLocal(&stream_val)) return;
  auto* stream = CppgcMixin::Unwrap<TransformStream>(stream_val.As<Object>());
  if (stream == nullptr) return;
  stream->readable()->default_controller()->ErrorInternal(args[0]);
  stream->controller()->RejectFinish(env, args[0]);
}

void SourceCancelFulfilled(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Local<Context> context = env->context();
  Local<Array> holder = args.Data().As<Array>();
  Local<Value> stream_val;
  Local<Value> reason;
  if (!holder->Get(context, 0).ToLocal(&stream_val) ||
      !holder->Get(context, 1).ToLocal(&reason))
    return;
  auto* stream = CppgcMixin::Unwrap<TransformStream>(stream_val.As<Object>());
  if (stream == nullptr) return;
  WritableStream* writable = stream->writable();
  TransformStreamDefaultController* controller = stream->controller();
  if (writable->state() == WritableState::kErrored) {
    controller->RejectFinish(env, writable->stored_error(env));
  } else {
    writable->controller()->ErrorIfNeeded(reason);
    stream->UnblockWrite();
    controller->ResolveFinish(env);
  }
}

void SourceCancelRejected(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Local<Context> context = env->context();
  Local<Array> holder = args.Data().As<Array>();
  Local<Value> stream_val;
  if (!holder->Get(context, 0).ToLocal(&stream_val)) return;
  auto* stream = CppgcMixin::Unwrap<TransformStream>(stream_val.As<Object>());
  if (stream == nullptr) return;
  stream->writable()->controller()->ErrorIfNeeded(args[0]);
  stream->UnblockWrite();
  stream->controller()->RejectFinish(env, args[0]);
}

// --- algorithm trampolines ---
// start carries the transform stream as Data (Setup invokes it with an
// undefined receiver); the others are SHARED per realm and recover the
// transform stream from their receiver — the readable/writable controllers
// invoke them with algo_receiver == the transform stream's wrapper, set in
// CreateTransformStream. They are never exposed to user code.

void TrampStart(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  auto* stream = CppgcMixin::Unwrap<TransformStream>(args.Data().As<Object>());
  if (stream == nullptr) return;
  args.GetReturnValue().Set(stream->start_promise(env));
}

void TrampWrite(const FunctionCallbackInfo<Value>& args) {
  auto* stream = CppgcMixin::Unwrap<TransformStream>(args.This().As<Object>());
  if (stream == nullptr) return;
  args.GetReturnValue().Set(stream->SinkWrite(args[0]));
}

void TrampClose(const FunctionCallbackInfo<Value>& args) {
  auto* stream = CppgcMixin::Unwrap<TransformStream>(args.This().As<Object>());
  if (stream == nullptr) return;
  args.GetReturnValue().Set(stream->SinkClose());
}

void TrampAbort(const FunctionCallbackInfo<Value>& args) {
  auto* stream = CppgcMixin::Unwrap<TransformStream>(args.This().As<Object>());
  if (stream == nullptr) return;
  args.GetReturnValue().Set(stream->SinkAbort(args[0]));
}

void TrampPull(const FunctionCallbackInfo<Value>& args) {
  auto* stream = CppgcMixin::Unwrap<TransformStream>(args.This().As<Object>());
  if (stream == nullptr) return;
  args.GetReturnValue().Set(stream->SourcePull());
}

void TrampCancel(const FunctionCallbackInfo<Value>& args) {
  auto* stream = CppgcMixin::Unwrap<TransformStream>(args.This().As<Object>());
  if (stream == nullptr) return;
  args.GetReturnValue().Set(stream->SourceCancel(args[0]));
}

}  // namespace

// ===========================================================================
// TransformStreamDefaultController
// ===========================================================================

TransformStreamDefaultController::TransformStreamDefaultController(
    Environment* env, Local<Object> object) {
  // Untracked: created once per stream, default destructor (TracedReference
  // cleanup is automatic), and the destructor never touches the Realm.
  CppgcMixin::Wrap(this, env, object, CppgcMixin::Tracking::kUntracked);
}

void TransformStreamDefaultController::Trace(cppgc::Visitor* visitor) const {
  // The algorithms and finish promise are reset as the stream settles, so
  // defer to the mutator thread.
  if (visitor->DeferTraceToMutatorThreadIfConcurrent(
          this,
          [](cppgc::Visitor* v, const void* self) {
            static_cast<const TransformStreamDefaultController*>(self)
                ->TraceOnMutatorThread(v);
          },
          sizeof(TransformStreamDefaultController))) {
    return;
  }
  TraceOnMutatorThread(visitor);
}

void TransformStreamDefaultController::TraceOnMutatorThread(
    cppgc::Visitor* visitor) const {
  CppgcMixin::Trace(visitor);
  visitor->Trace(transform_algorithm_);
  visitor->Trace(flush_algorithm_);
  visitor->Trace(cancel_algorithm_);
  visitor->Trace(finish_promise_);
  visitor->Trace(finish_resolver_);
}

Local<FunctionTemplate>
TransformStreamDefaultController::GetConstructorTemplate(Environment* env) {
  BindingData* bd = BindingData::Get(env);
  Isolate* isolate = env->isolate();
  Local<FunctionTemplate> tmpl =
      bd->transform_stream_default_controller_ctor.Get(isolate);
  if (tmpl.IsEmpty()) {
    tmpl = NewFunctionTemplate(isolate, nullptr);
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        TransformStreamDefaultController::kInternalFieldCount);
    tmpl->SetClassName(
        FIXED_ONE_BYTE_STRING(isolate, "TransformStreamDefaultController"));
    tmpl->PrototypeTemplate()->SetAccessorProperty(
        FIXED_ONE_BYTE_STRING(isolate, "desiredSize"),
        NewGetter(isolate, "desiredSize", GetDesiredSize));
    SetProtoMethodLen(isolate, tmpl, "enqueue", Enqueue, 0);
    SetProtoMethodLen(isolate, tmpl, "error", Error, 0);
    SetProtoMethodLen(isolate, tmpl, "terminate", Terminate, 0);
    bd->transform_stream_default_controller_ctor.Reset(isolate, tmpl);
  }
  return tmpl;
}

void TransformStreamDefaultController::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(GetDesiredSize);
  registry->Register(Enqueue);
  registry->Register(Error);
  registry->Register(Terminate);
}

void TransformStreamDefaultController::SetAlgorithms(
    Local<Function> transform_algorithm,
    Local<Function> flush_algorithm,
    Local<Function> cancel_algorithm) {
  Isolate* isolate = env()->isolate();
  transform_algorithm_.Reset(isolate, transform_algorithm);
  flush_algorithm_.Reset(isolate, flush_algorithm);
  cancel_algorithm_.Reset(isolate, cancel_algorithm);
}

void TransformStreamDefaultController::ClearAlgorithms() {
  transform_algorithm_.Reset();
  flush_algorithm_.Reset();
  cancel_algorithm_.Reset();
}

Local<Function> TransformStreamDefaultController::cancel_algorithm(
    Environment* env) const {
  if (cancel_algorithm_.IsEmpty()) return Local<Function>();
  return cancel_algorithm_.Get(env->isolate());
}

Local<Function> TransformStreamDefaultController::flush_algorithm(
    Environment* env) const {
  if (flush_algorithm_.IsEmpty()) return Local<Function>();
  return flush_algorithm_.Get(env->isolate());
}

Local<Promise> TransformStreamDefaultController::finish_promise(
    Environment* env) const {
  if (finish_promise_.IsEmpty()) return Local<Promise>();
  return finish_promise_.Get(env->isolate());
}

void TransformStreamDefaultController::SetFinishPending(Environment* env) {
  Isolate* isolate = env->isolate();
  Local<Promise::Resolver> resolver;
  if (!Promise::Resolver::New(env->context()).ToLocal(&resolver)) return;
  finish_resolver_.Reset(isolate, resolver);
  finish_promise_.Reset(isolate, resolver->GetPromise());
}

void TransformStreamDefaultController::ResolveFinish(Environment* env) {
  if (finish_resolver_.IsEmpty()) return;
  USE(finish_resolver_.Get(env->isolate())
          ->Resolve(env->context(), Undefined(env->isolate())));
}

void TransformStreamDefaultController::RejectFinish(Environment* env,
                                                   Local<Value> error) {
  if (finish_resolver_.IsEmpty()) return;
  USE(finish_resolver_.Get(env->isolate())->Reject(env->context(), error));
}

Local<Promise> TransformStreamDefaultController::PerformTransform(
    Local<Value> chunk) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  if (transform_algorithm_.IsEmpty()) return ResolvedUndefined(env);
  Local<Function> transform = transform_algorithm_.Get(isolate);
  Local<Value> argv[] = {chunk, object()};
  Local<Value> result;
  if (!transform->Call(context, Undefined(isolate), 2, argv).ToLocal(&result))
    return ResolvedUndefined(env);
  if (!result->IsPromise()) return ResolvedUndefined(env);
  Local<Function> rj;
  if (!Function::New(context, PerformTransformRejected, stream()->object(), 0,
                     v8::ConstructorBehavior::kThrow)
           .ToLocal(&rj))
    return result.As<Promise>();
  Local<Promise> chained;
  if (!result.As<Promise>()->Catch(context, rj).ToLocal(&chained))
    return result.As<Promise>();
  return chained;
}

Maybe<void> TransformStreamDefaultController::EnqueueInternal(
    Local<Value> chunk) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  TransformStream* stream = this->stream();
  ReadableStream* readable = stream->readable();
  ReadableStreamDefaultController* rc = readable->default_controller();
  if (!rc->CanCloseOrEnqueue()) {
    isolate->ThrowException(
        InvalidStateError(isolate, context, "Unable to enqueue"));
    return Nothing<void>();
  }
  bool readable_errored = false;
  {
    TryCatch try_catch(isolate);
    if (rc->EnqueueInternal(chunk).IsNothing()) {
      Local<Value> error = try_catch.Exception();
      try_catch.Reset();
      stream->ErrorWritableAndUnblockWrite(error);
      readable_errored = true;
    }
  }
  // Throw the readable's stored error only after the TryCatch above is out of
  // scope: its destructor would otherwise cancel a pending exception raised
  // while it is still alive, silently swallowing the size()/enqueue failure.
  if (readable_errored) {
    isolate->ThrowException(readable->stored_error(env));
    return Nothing<void>();
  }
  bool backpressure = !rc->ShouldCallPull();
  if (backpressure != stream->backpressure()) {
    CHECK(backpressure);
    stream->SetBackpressure(true);
  }
  return JustVoid();
}

void TransformStreamDefaultController::ErrorInternal(Local<Value> reason) {
  stream()->ErrorStream(reason);
}

void TransformStreamDefaultController::Terminate() {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  TransformStream* stream = this->stream();
  stream->readable()->default_controller()->CloseInternal();
  stream->ErrorWritableAndUnblockWrite(InvalidStateError(
      isolate, context, "TransformStream has been terminated"));
}

void TransformStreamDefaultController::GetDesiredSize(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (!CheckReceiverInvalidThis(env, GetConstructorTemplate(env), args.This(),
                                "TransformStreamDefaultController"))
    return;
  auto* c = CppgcMixin::Unwrap<TransformStreamDefaultController>(
      args.This().As<Object>());
  if (c == nullptr) return;
  ReadableStreamDefaultController* rc =
      c->stream()->readable()->default_controller();
  bool is_null = false;
  double desired = rc->GetDesiredSizeValue(&is_null);
  if (is_null)
    args.GetReturnValue().SetNull();
  else
    args.GetReturnValue().Set(desired);
}

void TransformStreamDefaultController::Enqueue(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (!CheckReceiverInvalidThis(env, GetConstructorTemplate(env), args.This(),
                                "TransformStreamDefaultController"))
    return;
  auto* c = CppgcMixin::Unwrap<TransformStreamDefaultController>(
      args.This().As<Object>());
  if (c == nullptr) return;
  USE(c->EnqueueInternal(args[0]));  // may leave a pending exception
}

void TransformStreamDefaultController::Error(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (!CheckReceiverInvalidThis(env, GetConstructorTemplate(env), args.This(),
                                "TransformStreamDefaultController"))
    return;
  auto* c = CppgcMixin::Unwrap<TransformStreamDefaultController>(
      args.This().As<Object>());
  if (c == nullptr) return;
  c->ErrorInternal(args[0]);
}

void TransformStreamDefaultController::Terminate(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (!CheckReceiverInvalidThis(env, GetConstructorTemplate(env), args.This(),
                                "TransformStreamDefaultController"))
    return;
  auto* c = CppgcMixin::Unwrap<TransformStreamDefaultController>(
      args.This().As<Object>());
  if (c == nullptr) return;
  c->Terminate();
}

// ===========================================================================
// TransformStream
// ===========================================================================

TransformStream::TransformStream(Environment* env, Local<Object> object) {
  // Untracked: created once per construction/transfer, default destructor,
  // and the destructor never touches the Realm.
  CppgcMixin::Wrap(this, env, object, CppgcMixin::Tracking::kUntracked);
}

void TransformStream::Trace(cppgc::Visitor* visitor) const {
  // The backpressure/start promise slots are reset as the stream runs, so
  // defer to the mutator thread.
  if (visitor->DeferTraceToMutatorThreadIfConcurrent(
          this,
          [](cppgc::Visitor* v, const void* self) {
            static_cast<const TransformStream*>(self)->TraceOnMutatorThread(v);
          },
          sizeof(TransformStream))) {
    return;
  }
  TraceOnMutatorThread(visitor);
}

void TransformStream::TraceOnMutatorThread(cppgc::Visitor* visitor) const {
  CppgcMixin::Trace(visitor);
  visitor->Trace(backpressure_change_promise_);
  visitor->Trace(backpressure_change_resolver_);
  visitor->Trace(start_resolver_);
  visitor->Trace(start_promise_);
  visitor->Trace(pending_write_chunk_);
  visitor->Trace(sink_write_continuation_);
}

Local<FunctionTemplate> TransformStream::GetConstructorTemplate(
    Environment* env) {
  BindingData* bd = BindingData::Get(env);
  Isolate* isolate = env->isolate();
  Local<FunctionTemplate> tmpl = bd->transform_stream_ctor.Get(isolate);
  if (tmpl.IsEmpty()) {
    tmpl = NewFunctionTemplate(isolate, nullptr);
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        TransformStream::kInternalFieldCount);
    tmpl->SetClassName(FIXED_ONE_BYTE_STRING(isolate, "TransformStream"));
    tmpl->PrototypeTemplate()->SetAccessorProperty(
        FIXED_ONE_BYTE_STRING(isolate, "readable"),
        NewGetter(isolate, "readable", GetReadable));
    tmpl->PrototypeTemplate()->SetAccessorProperty(
        FIXED_ONE_BYTE_STRING(isolate, "writable"),
        NewGetter(isolate, "writable", GetWritable));
    bd->transform_stream_ctor.Reset(isolate, tmpl);
  }
  return tmpl;
}

void TransformStream::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(GetReadable);
  registry->Register(GetWritable);
}

Local<Promise> TransformStream::backpressure_change_promise(
    Environment* env) const {
  if (backpressure_change_promise_.IsEmpty()) return Local<Promise>();
  return backpressure_change_promise_.Get(env->isolate());
}

void TransformStream::SetBackpressure(bool backpressure) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  CHECK(backpressure_ != backpressure);
  if (!backpressure_change_resolver_.IsEmpty()) {
    USE(backpressure_change_resolver_.Get(isolate)
            ->Resolve(env->context(), Undefined(isolate)));
  }
  Local<Promise::Resolver> resolver;
  if (!Promise::Resolver::New(env->context()).ToLocal(&resolver)) return;
  backpressure_change_resolver_.Reset(isolate, resolver);
  backpressure_change_promise_.Reset(isolate, resolver->GetPromise());
  backpressure_ = backpressure;
}

void TransformStream::UnblockWrite() {
  if (backpressure_) SetBackpressure(false);
}

void TransformStream::ErrorStream(Local<Value> error) {
  readable()->default_controller()->ErrorInternal(error);
  ErrorWritableAndUnblockWrite(error);
}

void TransformStream::ErrorWritableAndUnblockWrite(Local<Value> error) {
  controller()->ClearAlgorithms();
  writable()->controller()->ErrorIfNeeded(error);
  UnblockWrite();
}

void TransformStream::InitStartResolver(Environment* env) {
  Isolate* isolate = env->isolate();
  Local<Promise::Resolver> resolver;
  if (!Promise::Resolver::New(env->context()).ToLocal(&resolver)) return;
  start_resolver_.Reset(isolate, resolver);
  start_promise_.Reset(isolate, resolver->GetPromise());
}

void TransformStream::ResolveStart(Environment* env, Local<Value> value) {
  if (start_resolver_.IsEmpty()) return;
  USE(start_resolver_.Get(env->isolate())->Resolve(env->context(), value));
}

Local<Promise> TransformStream::start_promise(Environment* env) const {
  if (start_promise_.IsEmpty()) return Local<Promise>();
  return start_promise_.Get(env->isolate());
}

Local<Value> TransformStream::TakePendingWriteChunk() {
  Isolate* isolate = env()->isolate();
  Local<Value> chunk = pending_write_chunk_.Get(isolate);
  pending_write_chunk_.Reset();  // eager; ~TracedReference frees nothing
  return chunk;
}

Local<Promise> TransformStream::SinkWrite(Local<Value> chunk) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  TransformStreamDefaultController* controller = this->controller();
  if (!backpressure_) return controller->PerformTransform(chunk);
  Local<Promise> bp_change = backpressure_change_promise(env);
  // Single in-flight write: the previous write's continuation has already
  // taken its chunk, so the slot is free. Assigning store (Reset), never the
  // TracedReference constructor — see the readers' park queue.
  CHECK(pending_write_chunk_.IsEmpty());
  pending_write_chunk_.Reset(isolate, chunk);
  Local<Function> cont;
  if (sink_write_continuation_.IsEmpty()) {
    if (!Function::New(context, SinkWriteAfterBackpressure, object(), 0,
                       v8::ConstructorBehavior::kThrow)
             .ToLocal(&cont)) {
      pending_write_chunk_.Reset();
      return ResolvedUndefined(env);
    }
    sink_write_continuation_.Reset(isolate, cont);
  } else {
    cont = sink_write_continuation_.Get(isolate);
  }
  Local<Promise> chained;
  if (!bp_change->Then(context, cont).ToLocal(&chained)) {
    pending_write_chunk_.Reset();
    return ResolvedUndefined(env);
  }
  return chained;
}

Local<Promise> TransformStream::SinkClose() {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  TransformStreamDefaultController* controller = this->controller();
  if (controller->has_finish_promise()) return controller->finish_promise(env);
  controller->SetFinishPending(env);
  Local<Function> flush = controller->flush_algorithm(env);
  Local<Promise> flush_promise;
  if (!flush.IsEmpty()) {
    Local<Value> argv[] = {controller->object()};
    Local<Value> result;
    if (flush->Call(context, Undefined(isolate), 1, argv).ToLocal(&result) &&
        result->IsPromise()) {
      flush_promise = result.As<Promise>();
    }
  }
  if (flush_promise.IsEmpty()) flush_promise = ResolvedUndefined(env);
  controller->ClearAlgorithms();
  ThenReact(env, flush_promise, object(), SinkCloseFulfilled, SinkCloseRejected);
  return controller->finish_promise(env);
}

Local<Promise> TransformStream::SinkAbort(Local<Value> reason) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  TransformStreamDefaultController* controller = this->controller();
  if (controller->has_finish_promise()) return controller->finish_promise(env);
  controller->SetFinishPending(env);
  Local<Function> cancel = controller->cancel_algorithm(env);
  Local<Promise> cancel_promise;
  if (!cancel.IsEmpty()) {
    Local<Value> argv[] = {reason};
    Local<Value> result;
    if (cancel->Call(context, Undefined(isolate), 1, argv).ToLocal(&result) &&
        result->IsPromise()) {
      cancel_promise = result.As<Promise>();
    }
  }
  if (cancel_promise.IsEmpty()) cancel_promise = ResolvedUndefined(env);
  controller->ClearAlgorithms();
  Local<Array> holder = MakeHolder(isolate, context, object(), reason);
  ThenReact(env, cancel_promise, holder, SinkAbortFulfilled, SinkAbortRejected);
  return controller->finish_promise(env);
}

Local<Promise> TransformStream::SourcePull() {
  Environment* env = this->env();
  CHECK(backpressure_);
  SetBackpressure(false);
  return backpressure_change_promise(env);
}

Local<Promise> TransformStream::SourceCancel(Local<Value> reason) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  TransformStreamDefaultController* controller = this->controller();
  if (controller->has_finish_promise()) return controller->finish_promise(env);
  controller->SetFinishPending(env);
  Local<Function> cancel = controller->cancel_algorithm(env);
  Local<Promise> cancel_promise;
  if (!cancel.IsEmpty()) {
    Local<Value> argv[] = {reason};
    Local<Value> result;
    if (cancel->Call(context, Undefined(isolate), 1, argv).ToLocal(&result) &&
        result->IsPromise()) {
      cancel_promise = result.As<Promise>();
    }
  }
  if (cancel_promise.IsEmpty()) cancel_promise = ResolvedUndefined(env);
  controller->ClearAlgorithms();
  Local<Array> holder = MakeHolder(isolate, context, object(), reason);
  ThenReact(
      env, cancel_promise, holder, SourceCancelFulfilled, SourceCancelRejected);
  return controller->finish_promise(env);
}

void TransformStream::SetReadable(Local<Object> readable_obj) {
  object()->SetInternalField(kReadable, readable_obj);
  readable_cache_ = CppgcMixin::Unwrap<ReadableStream>(readable_obj);
  CHECK_NOT_NULL(readable_cache_);
}

void TransformStream::SetWritable(Local<Object> writable_obj) {
  object()->SetInternalField(kWritable, writable_obj);
  writable_cache_ = CppgcMixin::Unwrap<WritableStream>(writable_obj);
  CHECK_NOT_NULL(writable_cache_);
}

void TransformStream::SetController(Local<Object> controller_obj) {
  object()->SetInternalField(kController, controller_obj);
  controller_obj->SetInternalField(
      TransformStreamDefaultController::kStream, object());
  // Mirror the traced fields into the raw-pointer caches (hot-path accessors).
  auto* controller =
      CppgcMixin::Unwrap<TransformStreamDefaultController>(controller_obj);
  CHECK_NOT_NULL(controller);
  controller_cache_ = controller;
  controller->set_stream_cache(this);
}

void TransformStream::GetReadable(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (!CheckReceiverInvalidThis(env, GetConstructorTemplate(env), args.This(),
                                "TransformStream"))
    return;
  auto* stream = CppgcMixin::Unwrap<TransformStream>(args.This().As<Object>());
  if (stream == nullptr) return;
  args.GetReturnValue().Set(
      stream->object()->GetInternalField(kReadable).As<Value>());
}

void TransformStream::GetWritable(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (!CheckReceiverInvalidThis(env, GetConstructorTemplate(env), args.This(),
                                "TransformStream"))
    return;
  auto* stream = CppgcMixin::Unwrap<TransformStream>(args.This().As<Object>());
  if (stream == nullptr) return;
  args.GetReturnValue().Set(
      stream->object()->GetInternalField(kWritable).As<Value>());
}

// ===========================================================================
// Binding entry points
// ===========================================================================

void CreateTransformStream(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  // args: (startAlgorithm, transformAlgorithm, flushAlgorithm, cancelAlgorithm,
  //        wHWM, wSizeMode, wSize, rHWM, rSizeMode, rSize, abortController)
  CHECK(args[0]->IsFunction() || args[0]->IsUndefined());
  CHECK(args[1]->IsFunction());
  CHECK(args[2]->IsFunction());
  CHECK(args[3]->IsFunction());
  CHECK(args[4]->IsNumber());
  CHECK(args[5]->IsUint32());
  CHECK(args[7]->IsNumber());
  CHECK(args[8]->IsUint32());
  CHECK(args[10]->IsObject());
  // Empty when the transformer has no start() — the common case. The halves
  // then skip the start-promise plumbing (no start trampoline, no shared
  // start resolver, no per-creation reaction Functions) and mark their
  // controllers started via the shared per-realm reaction instead, with the
  // same one-microtask-after-construction timing and writable-before-readable
  // order.
  Local<Function> start_algorithm;
  if (args[0]->IsFunction()) start_algorithm = args[0].As<Function>();
  Local<Function> transform_algorithm = args[1].As<Function>();
  Local<Function> flush_algorithm = args[2].As<Function>();
  Local<Function> cancel_algorithm = args[3].As<Function>();
  double writable_hwm = args[4].As<Number>()->Value();
  SizeMode writable_size_mode =
      static_cast<SizeMode>(args[5].As<v8::Uint32>()->Value());
  Local<Function> writable_size;
  if (args[6]->IsFunction()) writable_size = args[6].As<Function>();
  double readable_hwm = args[7].As<Number>()->Value();
  SizeMode readable_size_mode =
      static_cast<SizeMode>(args[8].As<v8::Uint32>()->Value());
  Local<Function> readable_size;
  if (args[9]->IsFunction()) readable_size = args[9].As<Function>();
  Local<Object> abort_controller = args[10].As<Object>();

  // Transform stream + controller objects.
  Local<Function> stream_ctor;
  if (!TransformStream::GetConstructorTemplate(env)
           ->GetFunction(context)
           .ToLocal(&stream_ctor))
    return;
  Local<Object> stream_obj;
  if (!stream_ctor->NewInstance(context).ToLocal(&stream_obj)) return;
  // cppgc-managed: a bump allocation traced from the wrapper; no malloc,
  // persistent handle or weak callback (cf. the BaseObject stream).
  auto* stream = cppgc::MakeGarbageCollected<TransformStream>(
      env->cppgc_allocation_handle(), env, stream_obj);

  Local<Function> controller_ctor;
  if (!TransformStreamDefaultController::GetConstructorTemplate(env)
           ->GetFunction(context)
           .ToLocal(&controller_ctor))
    return;
  Local<Object> controller_obj;
  if (!controller_ctor->NewInstance(context).ToLocal(&controller_obj)) return;
  auto* controller =
      cppgc::MakeGarbageCollected<TransformStreamDefaultController>(
          env->cppgc_allocation_handle(), env, controller_obj);

  // The shared start promise must exist before the readable/writable run their
  // (trampoline) start algorithm. With no user start there is nothing to
  // await, so neither the resolver nor the trampoline is created.
  if (!start_algorithm.IsEmpty()) stream->InitStartResolver(env);
  stream->SetController(controller_obj);
  controller->SetAlgorithms(transform_algorithm, flush_algorithm,
                            cancel_algorithm);

  // Trampolines bridging the readable/writable controller algorithms back into
  // the C++ transform stream. Only start carries the stream as Data (it is
  // invoked with an undefined receiver); the rest are shared per realm and
  // recover the stream from their receiver — the controllers call them with
  // algo_receiver == stream_obj (passed to the factories below).
  Local<Function> start_tramp;
  if (!start_algorithm.IsEmpty())
    USE(Function::New(context, TrampStart, stream_obj, 0,
                      v8::ConstructorBehavior::kThrow)
            .ToLocal(&start_tramp));
  BindingData* bd = BindingData::Get(env);
  auto shared_tramp = [&](v8::Global<Function>* slot, FunctionCallback cb) {
    Local<Function> fn = slot->Get(isolate);
    if (fn.IsEmpty()) {
      USE(Function::New(context, cb, Local<Value>(), 0,
                        v8::ConstructorBehavior::kThrow)
              .ToLocal(&fn));
      slot->Reset(isolate, fn);
    }
    return fn;
  };
  Local<Function> write_tramp =
      shared_tramp(&bd->transform_write_tramp, TrampWrite);
  Local<Function> close_tramp =
      shared_tramp(&bd->transform_close_tramp, TrampClose);
  Local<Function> abort_tramp =
      shared_tramp(&bd->transform_abort_tramp, TrampAbort);
  Local<Function> pull_tramp =
      shared_tramp(&bd->transform_pull_tramp, TrampPull);
  Local<Function> cancel_tramp =
      shared_tramp(&bd->transform_cancel_tramp, TrampCancel);

  Local<Object> writable_obj;
  if (!NewWritableStream(env, start_tramp, write_tramp, close_tramp, abort_tramp,
                         writable_hwm, writable_size_mode, writable_size,
                         abort_controller, stream_obj)
           .ToLocal(&writable_obj)) {
    return;
  }
  Local<Object> readable_obj;
  if (!NewReadableStream(env, start_tramp, pull_tramp, cancel_tramp,
                         readable_hwm, readable_size_mode, readable_size,
                         stream_obj)
           .ToLocal(&readable_obj)) {
    return;
  }
  stream->SetWritable(writable_obj);
  stream->SetReadable(readable_obj);

  // Initial backpressure is true (matches initializeTransformStream).
  stream->SetBackpressure(true);

  // Run the start algorithm and resolve the shared start promise with its
  // result; a synchronous throw propagates out of construction. (With no user
  // start the halves already took the started fast path above.)
  if (!start_algorithm.IsEmpty()) {
    Local<Value> start_result;
    Local<Value> argv[] = {controller_obj};
    if (!start_algorithm->Call(context, Undefined(isolate), 1, argv)
             .ToLocal(&start_result)) {
      return;
    }
    stream->ResolveStart(env, start_result);
  }

  args.GetReturnValue().Set(stream_obj);
}

// Introspection helper for the custom inspect of a controller: returns the
// TransformStream a TransformStreamDefaultController belongs to. Internal-only
// (the public surface exposes no controller->stream link).
void TransformStreamControllerStream(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (!TransformStreamDefaultController::GetConstructorTemplate(env)
           ->HasInstance(args[0])) {
    return;
  }
  auto* c = CppgcMixin::Unwrap<TransformStreamDefaultController>(
      args[0].As<Object>());
  if (c == nullptr) return;
  args.GetReturnValue().Set(c->stream()->object());
}

void ExposeTransformStreamConstructors(Environment* env, Local<Object> target) {
  Local<Context> context = env->context();
  Isolate* isolate = env->isolate();
  auto expose = [&](const char* name, Local<FunctionTemplate> tmpl) {
    Local<Function> fn;
    if (tmpl->GetFunction(context).ToLocal(&fn))
      USE(target->Set(context, OneByteString(isolate, name), fn));
  };
  expose("TransformStream", TransformStream::GetConstructorTemplate(env));
  expose("TransformStreamDefaultController",
         TransformStreamDefaultController::GetConstructorTemplate(env));
}

void InitializeTransformStream(Isolate* isolate, Local<ObjectTemplate> target) {
  SetMethod(isolate, target, "createTransformStream", CreateTransformStream);
  SetMethod(isolate, target, "transformStreamControllerStream",
            TransformStreamControllerStream);
}

void RegisterTransformStreamExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(CreateTransformStream);
  registry->Register(TransformStreamControllerStream);
  TransformStream::RegisterExternalReferences(registry);
  TransformStreamDefaultController::RegisterExternalReferences(registry);
}

}  // namespace webstreams
}  // namespace node
