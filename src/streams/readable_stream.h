#ifndef SRC_STREAMS_READABLE_STREAM_H_
#define SRC_STREAMS_READABLE_STREAM_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "base_object.h"
#include "memory_tracker.h"
#include "v8.h"

#include <deque>

namespace node {

class Environment;
class ExternalReferenceRegistry;

namespace webstreams {

enum class StreamState : uint8_t { kReadable, kClosed, kErrored };

// How a default controller computes the size of a chunk. The two built-in
// queuing strategies are recognized at setup time so the per-chunk size()
// call into JS can be skipped entirely.
enum class SizeMode : uint8_t {
  kCountOne,    // CountQueuingStrategy: size === 1
  kByteLength,  // ByteLengthQueuingStrategy: size === chunk.byteLength
  kUserFn,      // arbitrary user-provided size() function
};

class ReadableStream;
class ReadableStreamDefaultReader;

// ReadableStreamDefaultController — owns the value queue and the queuing /
// backpressure bookkeeping for a (non-byte) ReadableStream. The queue holds
// arbitrary JS values, kept in a JS Array referenced from this object so chunks
// never need a per-value v8::Global. All state transitions and size math happen
// here without crossing into JS; the only crossings are invoking the user
// pull/cancel/size algorithms.
class ReadableStreamDefaultController final : public BaseObject {
 public:
  // Internal field layout of the JS wrapper. Field kStream holds the owning
  // ReadableStream wrapper so the relationship is traced by V8's GC (mirroring
  // the JS implementation's stream<->controller object graph).
  enum InternalFields {
    kStream = BaseObject::kInternalFieldCount,
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

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(ReadableStreamDefaultController)
  SET_SELF_SIZE(ReadableStreamDefaultController)

  // Owning stream, recovered from the traced internal field.
  ReadableStream* stream() const;

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
  bool close_requested() const { return close_requested_; }
  double queue_total_size() const { return queue_total_size_; }
  uint32_t queue_length() const { return queue_size_; }

  // Queue helpers operating on the JS-resident value queue.
  v8::MaybeLocal<v8::Value> DequeueValue();
  v8::Maybe<void> EnqueueValueWithSize(v8::Local<v8::Value> value, double size);
  void ResetQueue();

  // Sets up the controller's algorithms and queuing parameters. The pull/cancel
  // callbacks are already-wrapped JS functions (always return a Promise); start
  // may run synchronously and throw. Returns false with a pending exception if
  // the start algorithm threw synchronously.
  bool Setup(v8::Local<v8::Function> start_algorithm,
             v8::Local<v8::Function> pull_algorithm,
             v8::Local<v8::Function> cancel_algorithm,
             double high_water_mark,
             SizeMode size_mode,
             v8::Local<v8::Function> size_algorithm);

 private:
  // The value queue is a JS Array stored in a single Global on this object (one
  // handle, not one-per-chunk). queue_head_ is the index of the next item to
  // dequeue; consumed slots are nulled and the array is reset when it empties.
  // Per-chunk sizes live in a C++ deque to avoid boxing doubles into Numbers.
  v8::Global<v8::Array> queue_;
  std::deque<double> sizes_;
  uint32_t queue_head_ = 0;
  uint32_t queue_size_ = 0;
  double queue_total_size_ = 0;

  double high_water_mark_ = 1;
  SizeMode size_mode_ = SizeMode::kCountOne;
  bool started_ = false;
  bool pulling_ = false;
  bool pull_again_ = false;
  bool close_requested_ = false;

  v8::Global<v8::Function> pull_algorithm_;
  v8::Global<v8::Function> cancel_algorithm_;
  v8::Global<v8::Function> size_algorithm_;
};

// ReadableStreamDefaultReader — holds the queue of pending read requests as
// promise resolvers (no per-read request object), plus the reader's `closed`
// promise.
class ReadableStreamDefaultReader final : public BaseObject {
 public:
  enum InternalFields {
    kStream = BaseObject::kInternalFieldCount,
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

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(ReadableStreamDefaultReader)
  SET_SELF_SIZE(ReadableStreamDefaultReader)

  ReadableStream* stream() const;
  bool has_stream() const;

  size_t num_read_requests() const { return read_requests_.size(); }
  // Parks a pending read; the resolver is settled later by Fulfill/Close/Error.
  void AddReadRequest(v8::Local<v8::Promise::Resolver> resolver);
  void FulfillFront(v8::Local<v8::Value> chunk);
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

 private:
  std::deque<v8::Global<v8::Promise::Resolver>> read_requests_;
  v8::Global<v8::Promise::Resolver> closed_;
};

// ReadableStream — the user-facing object. Holds top-level state; the controller
// and (current) reader are referenced through GC-traced internal fields.
class ReadableStream final : public BaseObject {
 public:
  enum InternalFields {
    kController = BaseObject::kInternalFieldCount,
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

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(ReadableStream)
  SET_SELF_SIZE(ReadableStream)

  StreamState state() const { return state_; }
  bool disturbed() const { return disturbed_; }
  void set_disturbed(bool v) { disturbed_ = v; }
  v8::Local<v8::Value> stored_error(Environment* env) const;
  void set_stored_error(v8::Isolate* isolate, v8::Local<v8::Value> e);

  ReadableStreamDefaultController* default_controller() const;
  ReadableStreamDefaultReader* default_reader() const;
  bool locked() const;
  bool has_default_reader() const;

  void SetController(v8::Local<v8::Object> controller_obj);

  // --- internal operations ---
  v8::Local<v8::Promise> CancelInternal(v8::Local<v8::Value> reason);
  void Close();
  void ErrorStream(v8::Local<v8::Value> error);

  v8::Local<v8::Promise> closed_promise(Environment* env);
  void set_state(StreamState s) { state_ = s; }

 private:
  StreamState state_ = StreamState::kReadable;
  bool disturbed_ = false;
  v8::Global<v8::Value> stored_error_;
  v8::Global<v8::Promise::Resolver> closed_;  // the stream-level closed promise
};

// Binding entry points (registered on the `webstreams` internal binding).
// createReadableStream(start, pull, cancel, highWaterMark, sizeMode, size)
//   -> a fully set-up default ReadableStream JS object.
void CreateReadableStream(const v8::FunctionCallbackInfo<v8::Value>& args);
// acquireReadableStreamDefaultReader(stream) -> a default reader locked to it.
void AcquireReadableStreamDefaultReader(
    const v8::FunctionCallbackInfo<v8::Value>& args);

// Registers the readable-stream binding methods + constructor templates and
// their external references. Called from the binding's per-isolate / external
// reference hooks.
void InitializeReadableStream(v8::Isolate* isolate,
                              v8::Local<v8::ObjectTemplate> target);
void RegisterReadableStreamExternalReferences(
    ExternalReferenceRegistry* registry);

}  // namespace webstreams
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_STREAMS_READABLE_STREAM_H_
