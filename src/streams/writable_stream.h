#ifndef SRC_STREAMS_WRITABLE_STREAM_H_
#define SRC_STREAMS_WRITABLE_STREAM_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "base_object.h"
#include "memory_tracker.h"
#include "streams/streams_binding.h"
#include "v8.h"

#include <deque>

namespace node {

class Environment;
class ExternalReferenceRegistry;

namespace webstreams {

enum class WritableState : uint8_t {
  kWritable,
  kErroring,
  kClosed,
  kErrored,
};

class WritableStream;
class WritableStreamDefaultController;

// A settle-able promise slot mirroring the JS `{ promise, resolve, reject }`
// pattern used for the writer's ready/closed promises. It can hold either a
// pending promise with a live resolver, or an already-settled promise with no
// resolver (the "resolve/reject is undefined" case).
class PromiseSlot {
 public:
  void SetPending(Environment* env);
  void SetResolved(Environment* env);            // PromiseResolve()
  void SetRejected(Environment* env, v8::Local<v8::Value> error);  // PromiseReject
  void Resolve(Environment* env);                // resolve?.()
  void Reject(Environment* env, v8::Local<v8::Value> error);       // reject?.(e)
  bool IsPending(Environment* env) const;
  bool IsEmpty() const { return promise_.IsEmpty(); }
  v8::Local<v8::Promise> promise(Environment* env) const;
  void MemoryInfo(MemoryTracker* tracker, const char* label) const;

 private:
  v8::Global<v8::Promise> promise_;
  v8::Global<v8::Promise::Resolver> resolver_;
};

// WritableStreamDefaultController — owns the write queue and queuing parameters.
// The value queue holds chunks (the spec's close "sentinel" is modeled by the
// close_queued_ flag since close is always the final queue entry). State math
// and queue bookkeeping happen in C++; JS is crossed only to run the user
// write/close/abort/size algorithms and to drive the controller's AbortSignal.
class WritableStreamDefaultController final : public StreamBaseObject {
 public:
  enum InternalFields {
    kStream = BaseObject::kInternalFieldCount,
    kAbortController,  // the JS AbortController whose .signal is exposed
    kInternalFieldCount,
  };

  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);

  static void GetSignal(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Error(const v8::FunctionCallbackInfo<v8::Value>& args);

  WritableStreamDefaultController(Environment* env, v8::Local<v8::Object> object);

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(WritableStreamDefaultController)
  SET_SELF_SIZE(WritableStreamDefaultController)

  WritableStream* stream() const;

  // --- internal operations (spec algorithms) ---
  void Write(v8::Local<v8::Value> chunk, double chunk_size);
  void ProcessWrite(v8::Local<v8::Value> chunk);
  void ProcessClose();
  double GetDesiredSize() const;
  // Computes the chunk size; on a thrown size() it errors the controller (if
  // needed) and returns 1, matching the spec's getChunkSize.
  double GetChunkSize(v8::Local<v8::Value> chunk);
  void ErrorIfNeeded(v8::Local<v8::Value> error);
  void ErrorInternal(v8::Local<v8::Value> error);
  void CloseInternal();  // enqueue the close sentinel + advance
  void ClearAlgorithms();
  bool GetBackpressure() const;
  void AdvanceQueueIfNeeded();
  // [kAbort]: run the abort algorithm then clear algorithms; returns its promise.
  v8::Local<v8::Promise> AbortSteps(v8::Local<v8::Value> reason);
  void ErrorSteps();  // [kError]: resetQueue

  void OnStartFulfilled();
  void OnStartRejected(v8::Local<v8::Value> error);
  // Reactions to the user write/close algorithm promises.
  void OnWriteFulfilled();
  void OnWriteRejected(v8::Local<v8::Value> error);
  void OnCloseFulfilled();
  void OnCloseRejected(v8::Local<v8::Value> error);

  bool started() const { return started_; }

  bool Setup(v8::Local<v8::Function> start_algorithm,
             v8::Local<v8::Function> write_algorithm,
             v8::Local<v8::Function> close_algorithm,
             v8::Local<v8::Function> abort_algorithm,
             double high_water_mark,
             SizeMode size_mode,
             v8::Local<v8::Function> size_algorithm,
             v8::Local<v8::Object> abort_controller);

 private:
  // Value queue (chunks only); same hybrid layout as the readable default
  // controller. The close sentinel is represented by close_queued_.
  v8::MaybeLocal<v8::Value> DequeueValue();
  v8::Maybe<void> EnqueueValueWithSize(v8::Local<v8::Value> value, double size);
  void ResetQueue();
  bool QueueIsEmpty() const { return queue_size_ == 0 && !close_queued_; }

  v8::Global<v8::Array> queue_;
  std::deque<double> sizes_;
  uint32_t queue_head_ = 0;
  uint32_t queue_size_ = 0;
  double queue_total_size_ = 0;
  bool close_queued_ = false;

  double high_water_mark_ = 1;
  SizeMode size_mode_ = SizeMode::kCountOne;
  bool started_ = false;

  v8::Global<v8::Function> write_algorithm_;
  v8::Global<v8::Function> close_algorithm_;
  v8::Global<v8::Function> abort_algorithm_;
  v8::Global<v8::Function> size_algorithm_;
};

// WritableStreamDefaultWriter — holds the writer's ready/closed promise slots.
class WritableStreamDefaultWriter final : public StreamBaseObject {
 public:
  enum InternalFields {
    kStream = BaseObject::kInternalFieldCount,
    kInternalFieldCount,
  };

  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);

  static void GetClosed(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetReady(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetDesiredSize(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Write(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Close(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Abort(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ReleaseLock(const v8::FunctionCallbackInfo<v8::Value>& args);

  WritableStreamDefaultWriter(Environment* env, v8::Local<v8::Object> object);

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(WritableStreamDefaultWriter)
  SET_SELF_SIZE(WritableStreamDefaultWriter)

  WritableStream* stream() const;
  bool has_stream() const;

  PromiseSlot& ready() { return ready_; }
  PromiseSlot& closed() { return closed_; }

  v8::Local<v8::Promise> WriteInternal(v8::Local<v8::Value> chunk);
  v8::Local<v8::Promise> CloseInternal();
  v8::Local<v8::Promise> AbortInternal(v8::Local<v8::Value> reason);
  v8::Local<v8::Promise> CloseWithErrorPropagation();
  double GetDesiredSizeValue(bool* is_null) const;
  void EnsureReadyPromiseRejected(v8::Local<v8::Value> error);
  void EnsureClosedPromiseRejected(v8::Local<v8::Value> error);

  bool SetupInternal(v8::Local<v8::Object> stream_obj);
  void Release();

 private:
  PromiseSlot ready_;
  PromiseSlot closed_;
};

// WritableStream — the user-facing object. Owns the write/close/abort request
// bookkeeping; the controller and writer are referenced via GC-traced internal
// fields.
class WritableStream final : public StreamBaseObject {
 public:
  enum InternalFields {
    kController = BaseObject::kInternalFieldCount,
    kWriter,
    kInternalFieldCount,
  };

  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);
  static bool HasInstance(Environment* env, v8::Local<v8::Value> value);

  static void GetLocked(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Abort(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Close(const v8::FunctionCallbackInfo<v8::Value>& args);

  WritableStream(Environment* env, v8::Local<v8::Object> object);

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(WritableStream)
  SET_SELF_SIZE(WritableStream)

  WritableState state() const { return state_; }
  void set_state(WritableState s) { state_ = s; }
  bool backpressure() const { return backpressure_; }
  v8::Local<v8::Value> stored_error(Environment* env) const;

  WritableStreamDefaultController* controller() const;
  WritableStreamDefaultWriter* writer() const;
  bool locked() const;

  void SetController(v8::Local<v8::Object> controller_obj);
  void SetWriter(v8::Local<v8::Object> writer_obj);
  void ClearWriter();

  v8::Local<v8::Promise> closed_promise(Environment* env);

  // --- internal operations (spec algorithms) ---
  v8::Local<v8::Promise> Abort(v8::Local<v8::Value> reason);
  v8::Local<v8::Promise> CloseStream();
  void UpdateBackpressure(bool backpressure);
  void StartErroring(v8::Local<v8::Value> reason);
  void RejectCloseAndClosedPromiseIfNeeded();
  void MarkFirstWriteRequestInFlight();
  void MarkCloseRequestInFlight();
  bool HasOperationMarkedInFlight() const;
  void FinishInFlightWrite();
  void FinishInFlightWriteWithError(v8::Local<v8::Value> error);
  void FinishInFlightClose();
  void FinishInFlightCloseWithError(v8::Local<v8::Value> error);
  void FinishErroring();
  void DealWithRejection(v8::Local<v8::Value> error);
  bool CloseQueuedOrInFlight() const;
  v8::Local<v8::Promise> AddWriteRequest();

  // Settle the in-flight abort request after the controller's abort algorithm
  // (run from FinishErroring) resolves/rejects.
  void OnAbortAlgorithmFulfilled();
  void OnAbortAlgorithmRejected(v8::Local<v8::Value> error);

  bool has_in_flight_write_request() const {
    return !in_flight_write_request_.IsEmpty();
  }

 private:
  WritableState state_ = WritableState::kWritable;
  v8::Global<v8::Value> stored_error_;
  bool backpressure_ = false;

  std::deque<v8::Global<v8::Promise::Resolver>> write_requests_;
  v8::Global<v8::Promise::Resolver> close_request_;
  v8::Global<v8::Promise::Resolver> in_flight_write_request_;
  v8::Global<v8::Promise::Resolver> in_flight_close_request_;

  bool has_pending_abort_ = false;
  v8::Global<v8::Promise::Resolver> pending_abort_promise_;
  v8::Global<v8::Value> pending_abort_reason_;
  bool pending_abort_was_already_erroring_ = false;

  // The abort request resolver moved out of pending_abort_promise_ while the
  // controller's abort algorithm runs during FinishErroring (settled by the
  // abort-algorithm reaction callbacks).
  v8::Global<v8::Promise::Resolver> processing_abort_resolver_;

  v8::Global<v8::Promise::Resolver> closed_;  // stream-level closed promise
};

// Creates and sets up a WritableStream, returning its JS object. Used by both
// the binding entry and the C++ TransformStream. Call convention:
// start(controller), write(chunk, controller), close(), abort(reason). Returns
// an empty MaybeLocal with a pending exception if start threw synchronously.
v8::MaybeLocal<v8::Object> NewWritableStream(
    Environment* env,
    v8::Local<v8::Function> start_algorithm,
    v8::Local<v8::Function> write_algorithm,
    v8::Local<v8::Function> close_algorithm,
    v8::Local<v8::Function> abort_algorithm,
    double high_water_mark,
    SizeMode size_mode,
    v8::Local<v8::Function> size_algorithm,
    v8::Local<v8::Object> abort_controller);

// Binding entry points.
// createWritableStream(start, write, close, abort, highWaterMark, sizeMode,
//   size, abortController) -> a fully set-up WritableStream JS object.
void CreateWritableStream(const v8::FunctionCallbackInfo<v8::Value>& args);
// acquireWritableStreamDefaultWriter(stream) -> a writer locked to it.
void AcquireWritableStreamDefaultWriter(
    const v8::FunctionCallbackInfo<v8::Value>& args);

void ExposeWritableStreamConstructors(Environment* env,
                                      v8::Local<v8::Object> target);

void InitializeWritableStream(v8::Isolate* isolate,
                              v8::Local<v8::ObjectTemplate> target);
void RegisterWritableStreamExternalReferences(
    ExternalReferenceRegistry* registry);

}  // namespace webstreams
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_STREAMS_WRITABLE_STREAM_H_
