#ifndef SRC_STREAMS_TRANSFORM_STREAM_H_
#define SRC_STREAMS_TRANSFORM_STREAM_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "base_object.h"
#include "cppgc_helpers.h"
#include "memory_tracker.h"
#include "streams/streams_binding.h"
#include "v8.h"

namespace node {

class Environment;
class ExternalReferenceRegistry;

namespace webstreams {

class ReadableStream;
class WritableStream;
class TransformStream;

// TransformStreamDefaultController — holds the transform/flush/cancel algorithms
// and drives the transform stream's readable side (enqueue/close/error). It owns
// no queue of its own; queuing happens in the readable/writable controllers.
class TransformStreamDefaultController final
    : CPPGC_MIXIN(TransformStreamDefaultController) {
 public:
  SET_CPPGC_NAME(TransformStreamDefaultController)
  SET_NO_MEMORY_INFO()
  void Trace(cppgc::Visitor* visitor) const final;

  enum InternalFields {
    kStream = CppgcMixin::kInternalFieldCount,
    kInternalFieldCount,
  };

  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);

  static void GetDesiredSize(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Enqueue(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Error(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Terminate(const v8::FunctionCallbackInfo<v8::Value>& args);

  TransformStreamDefaultController(Environment* env,
                                   v8::Local<v8::Object> object);

  // Owning stream. Cached raw pointer (hot path): the GC-traced kStream
  // internal field keeps the stream's wrapper alive for this controller's
  // lifetime, so the cache cannot dangle. Set by TransformStream::SetController.
  TransformStream* stream() const { return stream_cache_; }
  void set_stream_cache(TransformStream* s) { stream_cache_ = s; }

  void SetAlgorithms(v8::Local<v8::Function> transform_algorithm,
                     v8::Local<v8::Function> flush_algorithm,
                     v8::Local<v8::Function> cancel_algorithm);
  void ClearAlgorithms();

  // transformStreamDefaultControllerEnqueue: may throw (Nothing + pending
  // exception) if the readable side cannot accept the chunk.
  v8::Maybe<void> EnqueueInternal(v8::Local<v8::Value> chunk);
  void ErrorInternal(v8::Local<v8::Value> reason);
  void Terminate();
  // performTransform: runs the user transform algorithm, error-propagating.
  v8::Local<v8::Promise> PerformTransform(v8::Local<v8::Value> chunk);

  bool has_finish_promise() const { return !finish_promise_.IsEmpty(); }
  v8::Local<v8::Promise> finish_promise(Environment* env) const;
  void SetFinishPending(Environment* env);
  void ResolveFinish(Environment* env);
  void RejectFinish(Environment* env, v8::Local<v8::Value> error);

  v8::Local<v8::Function> cancel_algorithm(Environment* env) const;
  v8::Local<v8::Function> flush_algorithm(Environment* env) const;

 private:
  // Traces the handle members; only ever runs on the mutator thread (the
  // algorithms and finish promise are reset as the stream settles).
  void TraceOnMutatorThread(cppgc::Visitor* visitor) const;

  v8::TracedReference<v8::Function> transform_algorithm_;
  v8::TracedReference<v8::Function> flush_algorithm_;
  v8::TracedReference<v8::Function> cancel_algorithm_;
  v8::TracedReference<v8::Promise> finish_promise_;
  v8::TracedReference<v8::Promise::Resolver> finish_resolver_;

  // Raw-pointer mirror of the kStream internal field (see stream()).
  TransformStream* stream_cache_ = nullptr;
};

// TransformStream — owns the backpressure coordination between its writable and
// readable halves. The readable/writable objects (themselves C++ streams) are
// stored as GC-traced internal fields.
class TransformStream final : CPPGC_MIXIN(TransformStream) {
 public:
  SET_CPPGC_NAME(TransformStream)
  SET_NO_MEMORY_INFO()
  void Trace(cppgc::Visitor* visitor) const final;

  enum InternalFields {
    kReadable = CppgcMixin::kInternalFieldCount,
    kWritable,
    kController,
    kInternalFieldCount,
  };

  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);

  static void GetReadable(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetWritable(const v8::FunctionCallbackInfo<v8::Value>& args);

  TransformStream(Environment* env, v8::Local<v8::Object> object);

  // Halves/controller: cached raw pointers mirroring the GC-traced kReadable /
  // kWritable / kController internal fields, which keep the targets' wrappers
  // alive for this stream's lifetime (set once during setup, never cleared).
  ReadableStream* readable() const { return readable_cache_; }
  WritableStream* writable() const { return writable_cache_; }
  TransformStreamDefaultController* controller() const {
    return controller_cache_;
  }

  bool backpressure() const { return backpressure_; }
  void SetBackpressure(bool backpressure);
  void UnblockWrite();
  void ErrorStream(v8::Local<v8::Value> error);
  void ErrorWritableAndUnblockWrite(v8::Local<v8::Value> error);

  void InitStartResolver(Environment* env);
  void ResolveStart(Environment* env, v8::Local<v8::Value> value);
  v8::Local<v8::Promise> start_promise(Environment* env) const;

  // Takes (and clears) the chunk parked by SinkWrite while awaiting a
  // backpressure change, and the resolver backing its sink-write promise.
  v8::Local<v8::Value> TakePendingWriteChunk();
  v8::Local<v8::Promise::Resolver> TakeSinkWriteResolver();

  // Sink/source algorithms wired into the readable/writable controllers.
  // SourcePull returns the per-realm direct-pull marker: the pull stays in
  // flight, and FinishPull is delivered as one microtask when backpressure
  // flips to true (see SetBackpressure) — no change promise, no reaction.
  v8::Local<v8::Promise> SinkWrite(v8::Local<v8::Value> chunk);
  v8::Local<v8::Promise> SinkClose();
  v8::Local<v8::Promise> SinkAbort(v8::Local<v8::Value> reason);
  v8::Local<v8::Value> SourcePull();
  v8::Local<v8::Promise> SourceCancel(v8::Local<v8::Value> reason);

  void SetReadable(v8::Local<v8::Object> readable_obj);
  void SetWritable(v8::Local<v8::Object> writable_obj);
  void SetController(v8::Local<v8::Object> controller_obj);

 private:
  // Traces the handle members; only ever runs on the mutator thread (the
  // backpressure/start promise slots are reset as the stream runs).
  void TraceOnMutatorThread(cppgc::Visitor* visitor) const;

  bool backpressure_ = false;
  // A direct-protocol pull is in flight: FinishPull is delivered as one
  // microtask at the next backpressure flip to true.
  bool pull_pending_direct_ = false;
  v8::TracedReference<v8::Promise::Resolver> start_resolver_;
  v8::TracedReference<v8::Promise> start_promise_;
  // SinkWrite's awaited-backpressure state: the writable dispatches at most
  // one write at a time (the next write starts only after the previous
  // sink-write promise settles), so the awaiting chunk and its sink-write
  // promise resolver live in single slots, and the continuation function is
  // created once per stream. The continuation is enqueued as one microtask
  // when backpressure flips to false (the exact FIFO position of the old
  // change-promise reaction) and resolves the sink resolver with
  // PerformTransform's promise — adoption keeps the writable's settle depth
  // identical to the old derived-promise chain.
  v8::TracedReference<v8::Value> pending_write_chunk_;
  v8::TracedReference<v8::Promise::Resolver> sink_write_resolver_;
  v8::TracedReference<v8::Function> sink_write_continuation_;

  // Raw-pointer mirrors of the kReadable / kWritable / kController fields.
  ReadableStream* readable_cache_ = nullptr;
  WritableStream* writable_cache_ = nullptr;
  TransformStreamDefaultController* controller_cache_ = nullptr;
};

// createTransformStream(startAlgorithm, transformAlgorithm, flushAlgorithm,
//   cancelAlgorithm, writableHWM, writableSizeMode, writableSize,
//   readableHWM, readableSizeMode, readableSize, abortController)
//   -> a fully set-up TransformStream JS object.
void CreateTransformStream(const v8::FunctionCallbackInfo<v8::Value>& args);

void ExposeTransformStreamConstructors(Environment* env,
                                       v8::Local<v8::Object> target);

void InitializeTransformStream(v8::Isolate* isolate,
                               v8::Local<v8::ObjectTemplate> target);
void RegisterTransformStreamExternalReferences(
    ExternalReferenceRegistry* registry);

}  // namespace webstreams
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_STREAMS_TRANSFORM_STREAM_H_
