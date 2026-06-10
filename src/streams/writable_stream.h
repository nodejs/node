#ifndef SRC_STREAMS_WRITABLE_STREAM_H_
#define SRC_STREAMS_WRITABLE_STREAM_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "base_object.h"
#include "memory_tracker.h"
#include "streams/streams_binding.h"
#include "v8.h"

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
// pattern used for the writer's ready/closed promises. The slot records its
// state lazily: no V8 promise (or rejection error) exists until the promise
// is first observed via promise(), so writers whose ready/closed are never
// touched — the common case — allocate nothing. Once observed, the slot is
// sticky-materialized and behaves like a real { promise, resolver } pair.
class PromiseSlot {
 public:
  void SetPending(Environment* env);
  void SetResolved(Environment* env);            // PromiseResolve()
  void Resolve(Environment* env);                // resolve?.()
  // Rejects if currently pending; no-op when already settled. The rejected
  // promise is marked handled (materialized lazily on first observation).
  void RejectIfPendingHandled(Environment* env, v8::Local<v8::Value> error);
  // Ensure-rejected: rejects if pending, otherwise REPLACES a settled promise
  // with one rejected with `error`; marked handled either way.
  void RejectOrReplaceHandled(Environment* env, v8::Local<v8::Value> error);
  // Records "rejected with the writer-released error" without constructing
  // the error (error construction captures a stack trace); the error is built
  // if/when the promise is first observed.
  void SetRejectedReleased() {
    state_ = State::kRejectedReleasedLazy;
    reason_.Reset();
    promise_.Reset();
    resolver_.Reset();
  }
  bool IsPending(Environment* env) const;
  bool IsMaterialized() const { return state_ == State::kMaterialized; }
  // `shared_released_reason`, when given, stores the lazily-built released
  // error so a writer's ready and closed promises reject with the SAME error
  // object (spec: one releasedError for both).
  v8::Local<v8::Promise> promise(
      Environment* env,
      v8::Global<v8::Value>* shared_released_reason = nullptr);
  void MemoryInfo(MemoryTracker* tracker, const char* label) const;

 private:
  enum class State : uint8_t {
    kNone,                  // pre-setup
    kPendingLazy,           // pending, promise not yet observed
    kResolvedLazy,          // resolved(undefined), not yet observed
    kRejectedLazy,          // rejected with reason_, not yet observed
    kRejectedReleasedLazy,  // rejected with the released error, unbuilt
    kMaterialized,          // promise_ (+ resolver_ when pending) is live
  };

  State state_ = State::kNone;
  v8::Global<v8::Value> reason_;  // kRejectedLazy only
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

  // Owning stream. Cached raw pointer (hot path): the GC-traced kStream
  // internal field keeps the stream's wrapper alive for this controller's
  // lifetime, so the cache cannot dangle. Set by WritableStream::SetController.
  WritableStream* stream() const { return stream_cache_; }
  void set_stream_cache(WritableStream* s) { stream_cache_ = s; }

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

  // The write callback is the user's RAW function, invoked with
  // `algo_receiver` (the underlying sink) as receiver — ProcessWrite handles
  // non-promise returns, thenables and synchronous throws natively. It may be
  // empty (no user write). close/abort remain already-wrapped JS functions.
  bool Setup(v8::Local<v8::Function> start_algorithm,
             v8::Local<v8::Function> write_algorithm,
             v8::Local<v8::Function> close_algorithm,
             v8::Local<v8::Function> abort_algorithm,
             double high_water_mark,
             SizeMode size_mode,
             v8::Local<v8::Function> size_algorithm,
             v8::Local<v8::Object> abort_controller,
             v8::Local<v8::Value> algo_receiver);

 private:
  // Value queue (chunks only); same hybrid layout as the readable default
  // controller. The close sentinel is represented by close_queued_.
  v8::MaybeLocal<v8::Value> DequeueValue();
  v8::Maybe<void> EnqueueValueWithSize(v8::Local<v8::Value> value, double size);
  void ResetQueue();
  bool QueueIsEmpty() const { return queue_size_ == 0 && !close_queued_; }

  v8::Global<v8::Array> queue_;
  FifoQueue<double> sizes_;
  uint32_t queue_head_ = 0;
  uint32_t queue_size_ = 0;
  double queue_total_size_ = 0;
  bool close_queued_ = false;

  double high_water_mark_ = 1;
  SizeMode size_mode_ = SizeMode::kCountOne;
  bool started_ = false;

  v8::Global<v8::Function> write_algorithm_;  // raw user fn; may be empty
  v8::Global<v8::Function> close_algorithm_;
  v8::Global<v8::Function> abort_algorithm_;
  v8::Global<v8::Function> size_algorithm_;
  v8::Global<v8::Value> algo_receiver_;  // `this` for the raw write algorithm

  // Cached promise-reaction functions for the write hot path (Data == this
  // controller's wrapper). Created once on first write and reused for every
  // subsequent write, so the per-write path allocates no V8 Function. The
  // wrapper->controller->Global cycle is broken by ClearAlgorithms.
  v8::Global<v8::Function> on_write_fulfilled_;
  v8::Global<v8::Function> on_write_rejected_;

  // Raw-pointer mirror of the kStream internal field (see stream()).
  WritableStream* stream_cache_ = nullptr;
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

  // Locked-to stream; cached raw pointer mirroring the GC-traced kStream
  // internal field (set in SetupInternal, cleared in Release).
  WritableStream* stream() const { return stream_cache_; }
  bool has_stream() const { return stream_cache_ != nullptr; }

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
  // Lazily-built reader-released rejection reason, shared by ready_ and
  // closed_ so both reject with the same error object (see
  // PromiseSlot::promise).
  v8::Global<v8::Value> released_reason_;

  // Raw-pointer mirror of the kStream internal field (see stream()).
  WritableStream* stream_cache_ = nullptr;
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

  // Controller/writer: cached raw pointers mirroring the GC-traced
  // kController / kWriter internal fields; the traced fields keep the targets'
  // wrappers alive while set, so the caches cannot dangle.
  WritableStreamDefaultController* controller() const {
    return controller_cache_;
  }
  WritableStreamDefaultWriter* writer() const { return writer_cache_; }
  bool locked() const { return writer_cache_ != nullptr; }
  void set_writer_cache(WritableStreamDefaultWriter* w) { writer_cache_ = w; }

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
  // Tracks a write whose completion no caller observes (pipeTo's fast
  // transfer): pushes an empty slot onto write_requests_ so the in-flight
  // bookkeeping stays 1:1 with queued chunks without allocating a resolver.
  void AddWriteRequestUntracked();
  // A promise resolved (never rejected) once every queued + in-flight write
  // has settled, or the stream has errored (so no write can ever complete).
  // Used by pipeTo's shutdown to wait on untracked fast-transfer writes.
  v8::Local<v8::Promise> FlushPromise();

  // --- pipe pump (pipeTo hot loop, see PipePump in readable_stream.cc) ---
  // While armed, each write completion re-enters the C++ transfer loop
  // (RunPipePump) instead of bouncing through JS via writer.ready, so a
  // steady-state pipe moves chunks without ever leaving C++. The stall
  // promise settles (resolves; never rejects) when the pump disarms for any
  // reason — source drained/closed/errored, destination no longer writable —
  // at which point pipeTo's JS step loop re-evaluates.
  bool pump_armed() const { return pump_armed_; }
  void ArmPump(v8::Local<v8::Object> source_obj);
  v8::Local<v8::Promise> PumpStallPromise();
  void DisarmPumpAndSettleStall();
  v8::Local<v8::Object> pump_source_obj(v8::Isolate* isolate) const {
    return pump_source_obj_.IsEmpty() ? v8::Local<v8::Object>()
                                      : pump_source_obj_.Get(isolate);
  }

  // Settle the in-flight abort request after the controller's abort algorithm
  // (run from FinishErroring) resolves/rejects.
  void OnAbortAlgorithmFulfilled();
  void OnAbortAlgorithmRejected(v8::Local<v8::Value> error);

  bool has_in_flight_write_request() const {
    return write_request_in_flight_;
  }

 private:
  void MaybeSettleFlush();

  WritableState state_ = WritableState::kWritable;
  v8::Global<v8::Value> stored_error_;
  bool backpressure_ = false;

  // May contain empty slots: an untracked fast-transfer write (see
  // AddWriteRequestUntracked) occupies a position with no resolver.
  FifoQueue<v8::Global<v8::Promise::Resolver>> write_requests_;
  v8::Global<v8::Promise::Resolver> close_request_;
  v8::Global<v8::Promise::Resolver> in_flight_write_request_;
  // True while a write request is marked in flight; in_flight_write_request_
  // alone cannot signal this since an untracked request's slot is empty.
  bool write_request_in_flight_ = false;
  v8::Global<v8::Promise::Resolver> in_flight_close_request_;
  // Lazily created by FlushPromise(); settled by MaybeSettleFlush().
  v8::Global<v8::Promise::Resolver> flush_resolver_;

  // Pipe pump state (see pump_armed() above). The source's wrapper is pinned
  // strongly only while the pump is armed.
  bool pump_armed_ = false;
  v8::Global<v8::Object> pump_source_obj_;
  v8::Global<v8::Promise::Resolver> pump_stall_resolver_;

  bool has_pending_abort_ = false;
  v8::Global<v8::Promise::Resolver> pending_abort_promise_;
  v8::Global<v8::Value> pending_abort_reason_;
  bool pending_abort_was_already_erroring_ = false;

  // The abort request resolver moved out of pending_abort_promise_ while the
  // controller's abort algorithm runs during FinishErroring (settled by the
  // abort-algorithm reaction callbacks).
  v8::Global<v8::Promise::Resolver> processing_abort_resolver_;

  v8::Global<v8::Promise::Resolver> closed_;  // stream-level closed promise

  // Raw-pointer mirrors of the kController / kWriter internal fields.
  WritableStreamDefaultController* controller_cache_ = nullptr;
  WritableStreamDefaultWriter* writer_cache_ = nullptr;
};

// Creates and sets up a WritableStream, returning its JS object. Used by both
// the binding entry and the C++ TransformStream. Call convention:
// start(controller), write(chunk, controller), close(), abort(reason). Returns
// an empty MaybeLocal with a pending exception if start threw synchronously.
v8::MaybeLocal<v8::Object> NewWritableStream(
    Environment* env,
    v8::Local<v8::Function> start_algorithm,
    v8::Local<v8::Function> write_algorithm,  // raw user fn; may be empty
    v8::Local<v8::Function> close_algorithm,
    v8::Local<v8::Function> abort_algorithm,
    double high_water_mark,
    SizeMode size_mode,
    v8::Local<v8::Function> size_algorithm,
    v8::Local<v8::Object> abort_controller,
    v8::Local<v8::Value> algo_receiver = v8::Local<v8::Value>());

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
