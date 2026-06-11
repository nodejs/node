#ifndef SRC_STREAMS_READABLE_STREAM_H_
#define SRC_STREAMS_READABLE_STREAM_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "base_object.h"
#include "cppgc_helpers.h"
#include "memory_tracker.h"
#include "streams/streams_binding.h"
#include "v8.h"

#include <deque>
#include <memory>

namespace node {

class Environment;
class ExternalReferenceRegistry;

namespace webstreams {

enum class StreamState : uint8_t { kReadable, kClosed, kErrored };

// Recorded settlement of a reader's lazily-materialized `closed` promise.
// Most readers never touch `closed`, so acquiring a reader records only the
// state; the resolver is created (and settled accordingly) on first access.
// kRejectedReleased records "rejected with the reader-released error" WITHOUT
// materializing the error: constructing a JS TypeError captures a stack
// trace, which dominates getReader()/releaseLock() churn if done eagerly. The
// error is built lazily on first `closed` access (see closed_promise).
enum class ClosedState : uint8_t {
  kPending,
  kResolved,
  kRejected,
  kRejectedReleased,
};

class ReadableStream;
class ReadableStreamDefaultReader;
class ReadableStreamBYOBReader;
class ReadableByteStreamController;
class ReadableStreamBYOBRequest;

// ReadableStreamDefaultController — owns the value queue and the queuing /
// backpressure bookkeeping for a (non-byte) ReadableStream. The queue holds
// arbitrary JS values, kept in a JS Array referenced from this object so chunks
// never need a per-value v8::Global. All state transitions and size math happen
// here without crossing into JS; the only crossings are invoking the user
// pull/cancel/size algorithms.
class ReadableStreamDefaultController final
    : CPPGC_MIXIN(ReadableStreamDefaultController) {
 public:
  SET_CPPGC_NAME(ReadableStreamDefaultController)
  SET_NO_MEMORY_INFO()
  void Trace(cppgc::Visitor* visitor) const final;

  // Internal field layout of the JS wrapper. Field kStream holds the owning
  // ReadableStream wrapper so the relationship is traced by V8's GC (mirroring
  // the JS implementation's stream<->controller object graph).
  enum InternalFields {
    kStream = CppgcMixin::kInternalFieldCount,
    kInternalFieldCount,
  };

  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);

  // Prototype methods / accessors exposed to JS.
  static void GetDesiredSize(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Close(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Enqueue(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Error(const v8::FunctionCallbackInfo<v8::Value>& args);

  ReadableStreamDefaultController(Environment* env, v8::Local<v8::Object> object);

  // Owning stream. Returns a cached raw pointer (hot path): the GC-traced
  // kStream internal field keeps the stream's wrapper — and therefore its
  // BaseObject — alive for this controller's lifetime, so the cache cannot
  // dangle. Set once by ReadableStream::SetController.
  ReadableStream* stream() const { return stream_cache_; }
  void set_stream_cache(ReadableStream* s) { stream_cache_ = s; }

  // --- internal operations (spec algorithms) ---
  // Returns desired size; sets *is_null when the stream is errored.
  double GetDesiredSizeValue(bool* is_null) const;
  bool CanCloseOrEnqueue() const;
  bool ShouldCallPull() const;
  void CallPullIfNeeded();
  void ClearAlgorithms();
  void CloseInternal();
  // Returns false (and schedules nothing) if a thrown error has been set on
  // try_catch; the caller in the JS entry point rethrows.
  v8::Maybe<void> EnqueueInternal(v8::Local<v8::Value> chunk);
  void ErrorInternal(v8::Local<v8::Value> error);
  v8::Local<v8::Promise> CancelSteps(v8::Local<v8::Value> reason);
  // PullSteps: satisfies a read by resolving `resolver`, either synchronously
  // from the queue or by parking it as a pending read request.
  void PullSteps(v8::Local<v8::Promise::Resolver> resolver);

  // Promise-reaction continuations invoked from C++ reaction callbacks.
  void OnStartFulfilled();  // started_ = true; CallPullIfNeeded()
  void FinishPull();        // pulling_ = false; retry if pull_again_

  void set_started(bool started) { started_ = started; }
  bool started() const { return started_; }
  void set_pulling(bool pulling) { pulling_ = pulling; }
  bool pulling() const { return pulling_; }
  bool has_pull_algorithm() const { return !pull_algorithm_.IsEmpty(); }
  bool close_requested() const { return close_requested_; }
  double queue_total_size() const { return queue_total_size_; }
  uint32_t queue_length() const {
    return static_cast<uint32_t>(queue_.size());
  }

  // Queue helpers operating on the value queue.
  v8::MaybeLocal<v8::Value> DequeueValue();
  v8::Maybe<void> EnqueueValueWithSize(v8::Local<v8::Value> value, double size);
  void ResetQueue();

  // Sets up the controller's algorithms and queuing parameters. The pull
  // callback is the user's RAW function, invoked with `algo_receiver` (the
  // underlying source) as receiver — CallPullIfNeeded handles non-promise
  // returns, thenables and synchronous throws natively. It may be empty (no
  // user pull). The cancel callback remains an already-wrapped JS function
  // (always returns a Promise); start may run synchronously and throw.
  // Returns false with a pending exception if start threw synchronously.
  bool Setup(v8::Local<v8::Function> start_algorithm,
             v8::Local<v8::Function> pull_algorithm,
             v8::Local<v8::Function> cancel_algorithm,
             double high_water_mark,
             SizeMode size_mode,
             v8::Local<v8::Function> size_algorithm,
             v8::Local<v8::Value> algo_receiver);

 private:
  // Traces the queue entries and handle members; only ever runs on the
  // mutator thread (the queue mutates as chunks enqueue/dequeue).
  void TraceOnMutatorThread(cppgc::Visitor* visitor) const;

  // The value queue: one TracedReference per buffered chunk (see
  // TracedValueQueueEntry in streams_binding.h for why this beats a
  // JS-Array-backed queue).
  FifoQueue<TracedValueQueueEntry> queue_;
  double queue_total_size_ = 0;

  double high_water_mark_ = 1;
  SizeMode size_mode_ = SizeMode::kCountOne;
  bool started_ = false;
  bool pulling_ = false;
  bool pull_again_ = false;
  bool close_requested_ = false;

  v8::TracedReference<v8::Function> pull_algorithm_;  // raw; may be empty
  v8::TracedReference<v8::Function> cancel_algorithm_;
  v8::TracedReference<v8::Function> size_algorithm_;
  v8::TracedReference<v8::Value> algo_receiver_;  // pull algorithm's `this`

  // Cached promise-reaction functions for an async (promise-returning) pull
  // (Data == this controller's wrapper); a sync pull uses the per-realm shared
  // reaction instead. Created once on first use.
  v8::TracedReference<v8::Function> on_pull_fulfilled_;
  v8::TracedReference<v8::Function> on_rejected_;

  // Raw-pointer mirror of the kStream internal field (see stream()).
  ReadableStream* stream_cache_ = nullptr;
};

// ReadableStreamDefaultReader — holds the queue of pending read requests as
// promise resolvers (no per-read request object), plus the reader's `closed`
// promise.
//
// Unlike the other stream objects this one is cppgc-managed: readers are
// created/discarded in bulk (getReader()/releaseLock() churn, tee, pipeTo,
// async iteration), and the BaseObject model costs a malloc, a persistent
// global handle and a weak callback per instance. cppgc replaces those with a
// bump allocation + TracedReference and lets the wrapper die with its JS
// object with no weak-callback pass. V8 references therefore live in
// v8::TracedReference (traced via Trace(); a v8::Global member would need an
// unsafe-in-sweep disposing destructor or a pre-finalizer).
class ReadableStreamDefaultReader final
    : CPPGC_MIXIN(ReadableStreamDefaultReader) {
 public:
  SET_CPPGC_NAME(ReadableStreamDefaultReader)
  SET_NO_MEMORY_INFO()
  void Trace(cppgc::Visitor* visitor) const final;

  enum InternalFields {
    kStream = CppgcMixin::kInternalFieldCount,
    // Fast slot for the front parked read request (the overwhelmingly common
    // single-outstanding-read case). An internal field is a plain tagged
    // store traced with the wrapper, where a TracedReference would pay a
    // traced-node create + destroy per parked read (Reset never recycles
    // nodes). Holds the request's primary: a promise resolver, or the settle
    // closure installed by the public read() wrapper.
    kParkedRead,
    kInternalFieldCount,
  };

  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);

  static void Read(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ReleaseLock(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Cancel(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetClosed(const v8::FunctionCallbackInfo<v8::Value>& args);

  ReadableStreamDefaultReader(Environment* env, v8::Local<v8::Object> object);

  // Locked-to stream; cached raw pointer mirroring the GC-traced kStream
  // internal field (set in SetupInternal, cleared in Release). While set, the
  // traced field keeps the stream's wrapper alive, so the cache cannot dangle.
  ReadableStream* stream() const { return stream_cache_; }
  bool has_stream() const { return stream_cache_ != nullptr; }

  size_t num_read_requests() const {
    return read_requests_.size() + (parked_read_in_slot_ ? 1 : 0);
  }
  // Parks a pending read; the resolver is settled later by Fulfill/Close/Error.
  void AddReadRequest(v8::Local<v8::Promise::Resolver> resolver);
  // Parks a pending read issued by the public read() JS wrapper: `settle` is
  // a JIT closure called as settle(value, done) to build { value, done } and
  // resolve the wrapper-created promise, or settle(error, false, true) to
  // reject it. Settling through it keeps the result construction and promise
  // resolution in JIT/builtin code (StoreICs and an IC'd then-lookup) instead
  // of the much dearer C++ API equivalents.
  void AddReadRequestClosure(v8::Local<v8::Function> settle);
  // Pops and returns the front pending read request resolver (CHECK it is the
  // resolver kind — only byte-stream paths call this, and the read() wrapper
  // never parks closures on byte streams).
  v8::Local<v8::Promise::Resolver> TakeReadRequest();
  // If the front pending read request is the read() wrapper's settle closure,
  // pops and returns it; otherwise (resolver kind, or no pending request)
  // returns empty and takes nothing. Used by the enqueue wrapper protocol to
  // settle the read with a JIT call from the enqueue() JS wrapper instead of
  // a C++->JS Function::Call.
  v8::Local<v8::Function> TakeFrontSettleClosure(v8::Isolate* isolate);
  // Resolves the front read request: {value: undefined, done: true} when done,
  // otherwise {value: chunk, done: false}.
  void FulfillFront(v8::Local<v8::Value> chunk, bool done);
  void CloseAll();
  void ErrorAll(v8::Local<v8::Value> error);

  v8::Local<v8::Promise> closed_promise(Environment* env);
  void ResolveClosed();
  void RejectClosed(v8::Local<v8::Value> error);

  // Wires reader<->stream (incl. the GC-traced internal fields) and the closed
  // promise according to the stream's current state. Returns false on lock
  // failure (caller throws).
  bool SetupInternal(v8::Local<v8::Object> stream_obj);
  void Release();

  // false for readers acquired internally by pipeTo/tee: their read results get
  // a null prototype so monkey-patched Object.prototype.then cannot observe the
  // pipe/tee (Web IDL "forAuthorCode").
  bool for_author_code() const { return for_author_code_; }
  void set_for_author_code(bool v) { for_author_code_ = v; }

 private:
  // Traces the TracedReference members. Only ever runs on the mutator thread:
  // the pending-read queue mutates as reads park/settle, so Trace() defers
  // concurrent marking here via DeferTraceToMutatorThreadIfConcurrent.
  void TraceOnMutatorThread(cppgc::Visitor* visitor) const;

  // A parked read request is either a promise resolver (native reads —
  // pipeTo, tee, byte streams, the async iterator) or the read() wrapper's
  // settle closure, discriminated by ->IsFunction().
  void AddReadRequestInternal(v8::Local<v8::Value> request);
  // Front parked read taken from/put into the kParkedRead internal field; the
  // FifoQueue only holds the overflow (2+ concurrent reads). Logical order is
  // [slot (if in use), read_requests_...]; the slot is only (re)used when both
  // are empty so FIFO order is preserved.
  v8::Local<v8::Value> TakeFrontReadRequest();
  bool parked_read_in_slot_ = false;

  FifoQueue<v8::TracedReference<v8::Value>> read_requests_;
  // Lazily materialized on first access; until then the settlement is
  // recorded in closed_state_/closed_reason_ (see ClosedState).
  v8::TracedReference<v8::Promise::Resolver> closed_;
  v8::TracedReference<v8::Value> closed_reason_;
  ClosedState closed_state_ = ClosedState::kPending;
  bool for_author_code_ = true;

  // Raw-pointer mirror of the kStream internal field (see stream()).
  ReadableStream* stream_cache_ = nullptr;
};

// ============================================================================
// Byte streams
// ============================================================================

// The constructor used to materialize a filled BYOB view, recorded from the
// view the user supplied (or Uint8Array for auto-allocated / default reads).
enum class ViewType : uint8_t {
  kDataView,
  kUint8Array,
  kInt8Array,
  kUint8ClampedArray,
  kInt16Array,
  kUint16Array,
  kInt32Array,
  kUint32Array,
  kFloat16Array,
  kFloat32Array,
  kFloat64Array,
  kBigInt64Array,
  kBigUint64Array,
  kBuffer,  // node::Buffer (a Uint8Array subclass; rebuilt via Buffer::New)
};

// A queued chunk of bytes. The BackingStore is owned (transferred from the
// enqueued view's ArrayBuffer), so the queue is zero-copy.
struct ByteQueueEntry {
  std::shared_ptr<v8::BackingStore> buffer;
  size_t byte_offset;
  size_t byte_length;
};

enum class PullIntoType : uint8_t { kDefault, kByob, kNone };

// Mirrors the spec's pull-into descriptor. The destination buffer is an owned
// BackingStore (transferred from the user's view), filled in place via memcpy.
struct PullIntoDescriptor {
  std::shared_ptr<v8::BackingStore> buffer;
  size_t buffer_byte_length;
  size_t byte_offset;
  size_t byte_length;
  size_t bytes_filled;
  size_t minimum_fill;
  uint32_t element_size;
  ViewType view_type;
  PullIntoType type;
};

// ReadableByteStreamController — owns a zero-copy byte queue (deque of owned
// BackingStores) and the pending pull-into descriptors. All fill/commit math is
// pure C++ (memcpy over BackingStore::Data()); JS is crossed only to invoke the
// user pull/cancel algorithms and to materialize a user-visible view once at
// fulfillment.
class ReadableByteStreamController final
    : CPPGC_MIXIN(ReadableByteStreamController) {
 public:
  SET_CPPGC_NAME(ReadableByteStreamController)
  SET_NO_MEMORY_INFO()
  void Trace(cppgc::Visitor* visitor) const final;

  enum InternalFields {
    kStream = CppgcMixin::kInternalFieldCount,
    kByobRequest,  // the current ReadableStreamBYOBRequest wrapper, or undefined
    kInternalFieldCount,
  };

  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);

  static void GetByobRequest(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetDesiredSize(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Close(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Enqueue(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Error(const v8::FunctionCallbackInfo<v8::Value>& args);

  ReadableByteStreamController(Environment* env, v8::Local<v8::Object> object);

  // Owning stream / current BYOBRequest. Cached raw pointers mirroring the
  // GC-traced kStream / kByobRequest internal fields, which keep the targets'
  // wrappers alive while set, so the caches cannot dangle.
  ReadableStream* stream() const { return stream_cache_; }
  void set_stream_cache(ReadableStream* s) { stream_cache_ = s; }
  ReadableStreamBYOBRequest* byob_request_obj() const {
    return byob_request_cache_;
  }

  // --- internal operations (spec algorithms) ---
  double GetDesiredSizeValue(bool* is_null) const;
  bool ShouldCallPull() const;
  void CallPullIfNeeded();
  void ClearAlgorithms();
  void ClearPendingPullIntos();
  void InvalidateBYOBRequest();
  // close(): may throw (partial read) -> returns Nothing with pending exception.
  v8::Maybe<void> CloseInternal();
  void HandleQueueDrain();
  // enqueue(view): may throw -> Nothing with pending exception.
  v8::Maybe<void> EnqueueInternal(v8::Local<v8::Object> view);
  void ErrorInternal(v8::Local<v8::Value> error);
  v8::Local<v8::Promise> CancelSteps(v8::Local<v8::Value> reason);
  // Default-reader read on a byte stream.
  void PullStepsDefault(v8::Local<v8::Promise::Resolver> resolver);
  // BYOB read: fills/parks; resolves `resolver` directly for sync completion.
  void PullInto(v8::Local<v8::Object> view,
                size_t min,
                v8::Local<v8::Promise::Resolver> resolver);
  // byobRequest.respond(n) / respondWithNewView(view): may throw.
  v8::Maybe<void> Respond(size_t bytes_written);
  v8::Maybe<void> RespondWithNewView(v8::Local<v8::Object> view);
  // [kRelease]: collapse pending pull-intos to a single 'none' descriptor.
  void OnReaderReleased();

  void OnStartFulfilled();
  void FinishPull();

  bool started() const { return started_; }
  bool close_requested() const { return close_requested_; }
  bool has_pending_pull_intos() const { return !pending_pull_intos_.empty(); }
  const PullIntoDescriptor& first_pending_pull_into() const {
    return pending_pull_intos_.front();
  }

  // See the default controller's Setup: pull is the user's raw function
  // (possibly empty), called with `algo_receiver` as receiver.
  bool Setup(v8::Local<v8::Function> start_algorithm,
             v8::Local<v8::Function> pull_algorithm,
             v8::Local<v8::Function> cancel_algorithm,
             double high_water_mark,
             size_t auto_allocate_chunk_size,  // 0 => undefined
             v8::Local<v8::Value> algo_receiver);

 private:
  // Traces the handle members; only ever runs on the mutator thread (the
  // cached reaction slots and algorithms are reset as the stream settles).
  void TraceOnMutatorThread(cppgc::Visitor* visitor) const;

  void EnqueueChunkToQueue(std::shared_ptr<v8::BackingStore> buffer,
                           size_t byte_offset,
                           size_t byte_length);
  // Slices [byte_offset, byte_offset+byte_length) of `buffer` into a fresh
  // BackingStore and enqueues it.
  void EnqueueClonedChunkToQueue(const std::shared_ptr<v8::BackingStore>& buffer,
                                 size_t byte_offset,
                                 size_t byte_length);
  void EnqueueDetachedPullIntoToQueue(PullIntoDescriptor* desc);
  bool FillPullIntoDescriptorFromQueue(PullIntoDescriptor* desc);
  void FillReadRequestFromQueue(v8::Local<v8::Promise::Resolver> resolver);
  void ProcessReadRequestsUsingQueue();
  // Fills and shifts ready pull-into descriptors from the queue, returning them
  // (NOT yet committed — the caller controls commit order to preserve the FIFO
  // ordering of read-into requests).
  std::deque<PullIntoDescriptor> ProcessPullIntoDescriptorsUsingQueue();
  void CommitPullIntoDescriptor(PullIntoDescriptor* desc);
  void CommitPullIntoDescriptors(std::deque<PullIntoDescriptor>* descs);
  v8::Maybe<void> RespondInternal(size_t bytes_written);
  void RespondInClosedState(PullIntoDescriptor* desc);
  v8::Maybe<void> RespondInReadableState(size_t bytes_written,
                                         PullIntoDescriptor* desc);
  // Builds the user-visible view from a filled descriptor (transfers buffer).
  v8::MaybeLocal<v8::Object> ConvertPullIntoDescriptor(PullIntoDescriptor* desc);

  std::deque<ByteQueueEntry> queue_;
  double queue_total_size_ = 0;
  double high_water_mark_ = 0;
  size_t auto_allocate_chunk_size_ = 0;
  bool has_auto_allocate_ = false;
  std::deque<PullIntoDescriptor> pending_pull_intos_;

  bool started_ = false;
  bool pulling_ = false;
  bool pull_again_ = false;
  bool close_requested_ = false;

  v8::TracedReference<v8::Function> pull_algorithm_;  // raw; may be empty
  v8::TracedReference<v8::Function> cancel_algorithm_;
  v8::TracedReference<v8::Value> algo_receiver_;  // pull algorithm's `this`

  // Cached promise-reaction functions for an async (promise-returning) pull
  // (Data == this controller's wrapper); a sync pull uses the per-realm shared
  // reaction instead. Created once on first use.
  v8::TracedReference<v8::Function> on_pull_fulfilled_;
  v8::TracedReference<v8::Function> on_rejected_;

  // Raw-pointer mirrors of the kStream / kByobRequest internal fields.
  ReadableStream* stream_cache_ = nullptr;
  ReadableStreamBYOBRequest* byob_request_cache_ = nullptr;
};

// ReadableStreamBYOBRequest — a thin view onto the controller's first pending
// pull-into. Holds a back-reference to the controller and the current view as
// GC-traced internal fields; both are cleared when the request is invalidated.
class ReadableStreamBYOBRequest final
    : CPPGC_MIXIN(ReadableStreamBYOBRequest) {
 public:
  SET_CPPGC_NAME(ReadableStreamBYOBRequest)
  SET_NO_MEMORY_INFO()
  void Trace(cppgc::Visitor* visitor) const final;

  enum InternalFields {
    kController = CppgcMixin::kInternalFieldCount,
    kView,
    kInternalFieldCount,
  };

  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);

  static void GetView(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Respond(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RespondWithNewView(const v8::FunctionCallbackInfo<v8::Value>& args);

  ReadableStreamBYOBRequest(Environment* env, v8::Local<v8::Object> object);

  // Owning controller; cached raw pointer mirroring the GC-traced kController
  // internal field (set at creation, cleared by InvalidateBYOBRequest). While
  // set, the traced field keeps the controller's wrapper alive.
  ReadableByteStreamController* controller() const { return controller_cache_; }
  void set_controller_cache(ReadableByteStreamController* c) {
    controller_cache_ = c;
  }

 private:
  ReadableByteStreamController* controller_cache_ = nullptr;
};

// ReadableStreamBYOBReader — holds the queue of pending read-into requests as
// promise resolvers plus the reader's `closed` promise. cppgc-managed, same
// rationale and structure as ReadableStreamDefaultReader above.
class ReadableStreamBYOBReader final : CPPGC_MIXIN(ReadableStreamBYOBReader) {
 public:
  SET_CPPGC_NAME(ReadableStreamBYOBReader)
  SET_NO_MEMORY_INFO()
  void Trace(cppgc::Visitor* visitor) const final;

  enum InternalFields {
    kStream = CppgcMixin::kInternalFieldCount,
    kInternalFieldCount,
  };

  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);

  static void Read(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ReleaseLock(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Cancel(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetClosed(const v8::FunctionCallbackInfo<v8::Value>& args);

  ReadableStreamBYOBReader(Environment* env, v8::Local<v8::Object> object);

  // Locked-to stream; cached raw pointer mirroring the GC-traced kStream
  // internal field (set in SetupInternal, cleared in Release).
  ReadableStream* stream() const { return stream_cache_; }
  bool has_stream() const { return stream_cache_ != nullptr; }

  size_t num_read_into_requests() const { return read_into_requests_.size(); }
  void AddReadIntoRequest(v8::Local<v8::Promise::Resolver> resolver);
  // Resolves the front read-into request with {value: view, done}.
  void FulfillFront(v8::Local<v8::Value> view, bool done);
  // Resolves all pending read-into requests with {value: undefined, done:true}.
  void CloseAll();
  void ErrorAll(v8::Local<v8::Value> error);

  v8::Local<v8::Promise> closed_promise(Environment* env);
  void ResolveClosed();
  void RejectClosed(v8::Local<v8::Value> error);

  bool SetupInternal(v8::Local<v8::Object> stream_obj);
  void Release();

  bool for_author_code() const { return for_author_code_; }
  void set_for_author_code(bool v) { for_author_code_ = v; }

 private:
  // See ReadableStreamDefaultReader::TraceOnMutatorThread.
  void TraceOnMutatorThread(cppgc::Visitor* visitor) const;

  FifoQueue<v8::TracedReference<v8::Promise::Resolver>> read_into_requests_;
  // Lazily materialized on first access; until then the settlement is
  // recorded in closed_state_/closed_reason_ (see ClosedState).
  v8::TracedReference<v8::Promise::Resolver> closed_;
  v8::TracedReference<v8::Value> closed_reason_;
  ClosedState closed_state_ = ClosedState::kPending;
  bool for_author_code_ = true;

  // Raw-pointer mirror of the kStream internal field (see stream()).
  ReadableStream* stream_cache_ = nullptr;
};

// ReadableStream — the user-facing object. Holds top-level state; the controller
// and (current) reader are referenced through GC-traced internal fields.
class ReadableStream final : CPPGC_MIXIN(ReadableStream) {
 public:
  SET_CPPGC_NAME(ReadableStream)
  SET_NO_MEMORY_INFO()
  void Trace(cppgc::Visitor* visitor) const final;

  enum InternalFields {
    kController = CppgcMixin::kInternalFieldCount,
    kReader,
    kInternalFieldCount,
  };

  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);
  static bool HasInstance(Environment* env, v8::Local<v8::Value> value);

  static void GetLocked(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Cancel(const v8::FunctionCallbackInfo<v8::Value>& args);

  ReadableStream(Environment* env, v8::Local<v8::Object> object);

  StreamState state() const { return state_; }
  bool disturbed() const { return disturbed_; }
  void set_disturbed(bool v) { disturbed_ = v; }
  v8::Local<v8::Value> stored_error(Environment* env) const;
  void set_stored_error(v8::Isolate* isolate, v8::Local<v8::Value> e);

  // Controllers (exactly one is non-null once set up). Cached raw pointers
  // mirroring the GC-traced kController / kReader internal fields; the traced
  // fields keep the targets' wrappers alive while set, so the caches cannot
  // dangle. Both controllers are cppgc-managed (not StreamBaseObjects), so
  // the cache is a void* discriminated by an explicit kind byte — no V8 API
  // crossing on these hot accessors.
  enum class ControllerKind : uint8_t { kNone, kDefault, kByte };
  ReadableStreamDefaultController* default_controller() const {
    return controller_kind_ == ControllerKind::kDefault
               ? static_cast<ReadableStreamDefaultController*>(controller_cache_)
               : nullptr;
  }
  ReadableByteStreamController* byte_controller() const {
    return controller_kind_ == ControllerKind::kByte
               ? static_cast<ReadableByteStreamController*>(controller_cache_)
               : nullptr;
  }
  bool has_byte_controller() const {
    return controller_kind_ == ControllerKind::kByte;
  }

  // Readers (at most one is attached). The default reader is cppgc-managed
  // (not a StreamBaseObject), so the cache is a void* discriminated by an
  // explicit kind byte rather than the StreamBaseObject kind tag.
  enum class ReaderKind : uint8_t { kNone, kDefault, kByob };
  ReadableStreamDefaultReader* default_reader() const {
    return reader_kind_ == ReaderKind::kDefault
               ? static_cast<ReadableStreamDefaultReader*>(reader_cache_)
               : nullptr;
  }
  ReadableStreamBYOBReader* byob_reader() const {
    return reader_kind_ == ReaderKind::kByob
               ? static_cast<ReadableStreamBYOBReader*>(reader_cache_)
               : nullptr;
  }
  bool locked() const { return reader_kind_ != ReaderKind::kNone; }
  bool has_default_reader() const {
    return reader_kind_ == ReaderKind::kDefault;
  }
  bool has_byob_reader() const { return reader_kind_ == ReaderKind::kByob; }
  void set_reader_cache(ReadableStreamDefaultReader* r) {
    reader_cache_ = r;
    reader_kind_ = ReaderKind::kDefault;
  }
  void set_reader_cache(ReadableStreamBYOBReader* r) {
    reader_cache_ = r;
    reader_kind_ = ReaderKind::kByob;
  }
  void clear_reader_cache() {
    reader_cache_ = nullptr;
    reader_kind_ = ReaderKind::kNone;
  }

  void SetController(v8::Local<v8::Object> controller_obj, bool is_byte);

  // --- internal operations ---
  v8::Local<v8::Promise> CancelInternal(v8::Local<v8::Value> reason);
  void Close();
  void ErrorStream(v8::Local<v8::Value> error);

  v8::Local<v8::Promise> closed_promise(Environment* env);
  void set_state(StreamState s) { state_ = s; }

 private:
  // Traces the handle members; only ever runs on the mutator thread (the
  // stored error / closed resolver are set as the stream settles).
  void TraceOnMutatorThread(cppgc::Visitor* visitor) const;

  StreamState state_ = StreamState::kReadable;
  bool disturbed_ = false;
  v8::TracedReference<v8::Value> stored_error_;
  // The stream-level closed promise.
  v8::TracedReference<v8::Promise::Resolver> closed_;

  // Raw-pointer mirrors of the kController / kReader internal fields. Both
  // caches are type-erased (see ControllerKind / ReaderKind above).
  void* controller_cache_ = nullptr;
  ControllerKind controller_kind_ = ControllerKind::kNone;
  void* reader_cache_ = nullptr;
  ReaderKind reader_kind_ = ReaderKind::kNone;
};

// Creates and sets up a default ReadableStream, returning its JS object. Used by
// both the binding entry and the C++ TransformStream. The algorithm functions
// follow the call convention: start(controller), pull(controller),
// cancel(reason). Returns an empty MaybeLocal with a pending exception if the
// start algorithm threw synchronously.
v8::MaybeLocal<v8::Object> NewReadableStream(
    Environment* env,
    v8::Local<v8::Function> start_algorithm,
    v8::Local<v8::Function> pull_algorithm,  // raw user fn; may be empty
    v8::Local<v8::Function> cancel_algorithm,
    double high_water_mark,
    SizeMode size_mode,
    v8::Local<v8::Function> size_algorithm,
    v8::Local<v8::Value> algo_receiver = v8::Local<v8::Value>());

// Binding entry points (registered on the `webstreams` internal binding).
// createReadableStream(start, pull, cancel, highWaterMark, sizeMode, size)
//   -> a fully set-up default ReadableStream JS object.
void CreateReadableStream(const v8::FunctionCallbackInfo<v8::Value>& args);
// createReadableByteStream(start, pull, cancel, highWaterMark,
//   autoAllocateChunkSize) -> a fully set-up byte ReadableStream JS object.
void CreateReadableByteStream(const v8::FunctionCallbackInfo<v8::Value>& args);
// acquireReadableStreamDefaultReader(stream) -> a default reader locked to it.
void AcquireReadableStreamDefaultReader(
    const v8::FunctionCallbackInfo<v8::Value>& args);
// acquireReadableStreamBYOBReader(stream) -> a BYOB reader locked to it.
void AcquireReadableStreamBYOBReader(
    const v8::FunctionCallbackInfo<v8::Value>& args);

// Registers the readable-stream binding methods + constructor templates and
// their external references. Called from the binding's per-isolate / external
// reference hooks.
// Exposes the readable-stream constructor functions on the binding object so the
// JS layer can use them as the public classes (their prototypes are shared).
void ExposeReadableStreamConstructors(Environment* env,
                                      v8::Local<v8::Object> target);

void InitializeReadableStream(v8::Isolate* isolate,
                              v8::Local<v8::ObjectTemplate> target);
void RegisterReadableStreamExternalReferences(
    ExternalReferenceRegistry* registry);

}  // namespace webstreams
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_STREAMS_READABLE_STREAM_H_
