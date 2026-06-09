#ifndef SRC_STREAMS_TRANSFORM_STREAM_H_
#define SRC_STREAMS_TRANSFORM_STREAM_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "base_object.h"
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
class TransformStreamDefaultController final : public StreamBaseObject {
 public:
  enum InternalFields {
    kStream = BaseObject::kInternalFieldCount,
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

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(TransformStreamDefaultController)
  SET_SELF_SIZE(TransformStreamDefaultController)

  TransformStream* stream() const;

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
  v8::Global<v8::Function> transform_algorithm_;
  v8::Global<v8::Function> flush_algorithm_;
  v8::Global<v8::Function> cancel_algorithm_;
  v8::Global<v8::Promise> finish_promise_;
  v8::Global<v8::Promise::Resolver> finish_resolver_;
};

// TransformStream — owns the backpressure coordination between its writable and
// readable halves. The readable/writable objects (themselves C++ streams) are
// stored as GC-traced internal fields.
class TransformStream final : public StreamBaseObject {
 public:
  enum InternalFields {
    kReadable = BaseObject::kInternalFieldCount,
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

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(TransformStream)
  SET_SELF_SIZE(TransformStream)

  ReadableStream* readable() const;
  WritableStream* writable() const;
  TransformStreamDefaultController* controller() const;

  bool backpressure() const { return backpressure_; }
  v8::Local<v8::Promise> backpressure_change_promise(Environment* env) const;
  void SetBackpressure(bool backpressure);
  void UnblockWrite();
  void ErrorStream(v8::Local<v8::Value> error);
  void ErrorWritableAndUnblockWrite(v8::Local<v8::Value> error);

  void InitStartResolver(Environment* env);
  void ResolveStart(Environment* env, v8::Local<v8::Value> value);
  v8::Local<v8::Promise> start_promise(Environment* env) const;

  // Sink/source algorithms wired into the readable/writable controllers.
  v8::Local<v8::Promise> SinkWrite(v8::Local<v8::Value> chunk);
  v8::Local<v8::Promise> SinkClose();
  v8::Local<v8::Promise> SinkAbort(v8::Local<v8::Value> reason);
  v8::Local<v8::Promise> SourcePull();
  v8::Local<v8::Promise> SourceCancel(v8::Local<v8::Value> reason);

  void SetReadable(v8::Local<v8::Object> readable_obj);
  void SetWritable(v8::Local<v8::Object> writable_obj);
  void SetController(v8::Local<v8::Object> controller_obj);

 private:
  bool backpressure_ = false;
  v8::Global<v8::Promise> backpressure_change_promise_;
  v8::Global<v8::Promise::Resolver> backpressure_change_resolver_;
  v8::Global<v8::Promise::Resolver> start_resolver_;
  v8::Global<v8::Promise> start_promise_;
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
