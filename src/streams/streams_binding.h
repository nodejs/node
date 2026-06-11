#ifndef SRC_STREAMS_STREAMS_BINDING_H_
#define SRC_STREAMS_STREAMS_BINDING_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "base_object.h"
#include "memory_tracker.h"
#include "node_snapshotable.h"
#include "v8.h"

#include <vector>

namespace node {

class Environment;
class ExternalReferenceRegistry;
class IsolateData;
class Realm;

namespace webstreams {

// How a controller computes the size of a chunk. The two built-in queuing
// strategies are recognized at setup time so the per-chunk size() call into JS
// can be skipped entirely.
enum class SizeMode : uint8_t {
  kCountOne,    // CountQueuingStrategy: size === 1
  kByteLength,  // ByteLengthQueuingStrategy: size === chunk.byteLength
  kUserFn,      // arbitrary user-provided size() function
};

// Web IDL conformance helpers shared by the readable/writable/transform
// binding setup. idlharness requires accessor getters to be named "get <prop>",
// operations to carry the right `.length`, and Promise-returning operations to
// *reject* (not throw) when invoked on a foreign receiver.

// Builds an accessor getter FunctionTemplate whose `.name` is "get <prop>".
// Carries NO receiver signature: the callback brand-checks internally (via
// CheckReceiverInvalidThis) so a foreign receiver yields the public
// ERR_INVALID_THIS TypeError instead of a bare V8 "Illegal invocation".
v8::Local<v8::FunctionTemplate> NewGetter(v8::Isolate* isolate,
                                          const char* prop,
                                          v8::FunctionCallback cb);

// Builds a getter FunctionTemplate ("get <prop>") for a Promise-typed attribute
// (e.g. reader.closed, writer.ready). Carries NO receiver signature: Web IDL
// requires the getter of a Promise-typed attribute to reject (not throw) on a
// brand-check failure, so the callback must run for foreign receivers too.
v8::Local<v8::FunctionTemplate> NewPromiseGetter(v8::Isolate* isolate,
                                                 const char* prop,
                                                 v8::FunctionCallback cb);

// Installs a synchronous operation on tmpl's prototype WITHOUT a receiver
// signature and with an explicit `.length` (for required arguments). The
// callback brand-checks internally (via CheckReceiverInvalidThis) so a foreign
// receiver yields ERR_INVALID_THIS rather than a bare "Illegal invocation".
void SetProtoMethodLen(v8::Isolate* isolate,
                       v8::Local<v8::FunctionTemplate> tmpl,
                       const char* name,
                       v8::FunctionCallback cb,
                       int length);

// Installs a Promise-returning operation on tmpl's prototype WITHOUT a receiver
// signature, so the C++ callback runs even for a foreign receiver and can reject
// the returned promise per Web IDL instead of throwing "Illegal invocation".
void SetProtoMethodPromise(v8::Isolate* isolate,
                           v8::Local<v8::FunctionTemplate> tmpl,
                           const char* name,
                           v8::FunctionCallback cb,
                           int length);

// A Promise rejected with an ERR_INVALID_THIS TypeError; returned by a
// Promise-returning operation invoked on a foreign receiver.
v8::Local<v8::Value> IllegalInvocationRejection(v8::Local<v8::Context> context);

// FIFO queue over std::vector with a head index. Unlike std::deque — which
// mallocs its map plus a ~512-byte node block in its constructor — this
// allocates nothing until the first push, which matters because every
// reader/writer/controller embeds one or more of these and most are never
// pushed to (a reader that only reads buffered chunks parks no requests).
// pop_front() is O(1); the dead prefix is compacted once it dominates so a
// long-lived queue that never fully drains cannot grow unboundedly.
template <typename T>
class FifoQueue {
 public:
  bool empty() const { return head_ == items_.size(); }
  size_t size() const { return items_.size() - head_; }
  T& front() { return items_[head_]; }
  const T& front() const { return items_[head_]; }
  T& back() { return items_.back(); }
  void push_back(const T& value) { items_.push_back(value); }
  void push_back(T&& value) { items_.push_back(std::move(value)); }
  template <typename... Args>
  void emplace_back(Args&&... args) {
    items_.emplace_back(std::forward<Args>(args)...);
  }
  void pop_front() {
    ++head_;
    if (head_ == items_.size()) {
      items_.clear();
      head_ = 0;
    } else if (head_ >= 32 && head_ * 2 >= items_.size()) {
      items_.erase(items_.begin(), items_.begin() + head_);
      head_ = 0;
    }
  }
  void clear() {
    items_.clear();
    head_ = 0;
  }
  // Visits the live range front-to-back (used to Trace a queue of
  // v8::TracedReference from a cppgc-managed owner).
  template <typename F>
  void ForEach(F&& f) const {
    for (size_t i = head_; i < items_.size(); ++i) f(items_[i]);
  }

 private:
  std::vector<T> items_;
  size_t head_ = 0;
};

class WritableStream;

// Re-enters pipeTo's C++ transfer loop after a write completes on a pump-armed
// destination (defined in readable_stream.cc, which sees both stream types;
// called from the writable side's write-fulfilled reaction).
void RunPipePump(WritableStream* dest);

// Attaches the shared per-realm start-fulfilled reaction to `promise`, which
// must have been resolved with a controller's wrapper object. Setup fast path
// for a start algorithm that returned a non-object: marks the controller
// started (by kind tag) in a microtask without per-creation Function
// allocations.
void ThenStartFulfilled(Environment* env, v8::Local<v8::Promise> promise);

// Brand-checks `receiver` against `ctor` for a synchronous operation/getter.
// Returns true on success; otherwise throws an ERR_INVALID_THIS TypeError named
// for `type_name` and returns false (the caller must then return from the V8
// callback). Pairs with the signature-free NewGetter/SetProtoMethodLen so the
// public surface reports ERR_INVALID_THIS instead of "Illegal invocation".
bool CheckReceiverInvalidThis(Environment* env,
                              v8::Local<v8::FunctionTemplate> ctor,
                              v8::Local<v8::Value> receiver,
                              const char* type_name);

// Common base for every webstreams BaseObject. Carries a small kind tag so a
// related object recovered from a GC-traced internal field can be safely
// downcast: BaseObject::FromJSObject<T> performs an unchecked static_cast and
// RTTI is disabled in the Node build, so the tag is the discriminator. For
// example a ReadableStream's controller field may hold either a default or a
// byte controller, and its reader field either a default or a BYOB reader; the
// tag tells them apart in O(1).
class StreamBaseObject : public BaseObject {
 public:
  enum class Kind : uint8_t {
    kStream,
    kDefaultController,
    kByteController,
    kDefaultReader,
    kByobReader,
    kByobRequest,
    kWritableStream,
    kWritableStreamDefaultWriter,
    kWritableStreamDefaultController,
    kTransformStream,
    kTransformStreamDefaultController,
  };

  StreamBaseObject(Environment* env, v8::Local<v8::Object> object, Kind kind)
      : BaseObject(env, object), stream_kind_(kind) {}

  Kind stream_kind() const { return stream_kind_; }

 private:
  const Kind stream_kind_;
};

// Per-realm state for the WHATWG Streams C++ implementation. Holds the cached
// property name symbols (e.g. the `value`/`done` keys of read results) and the
// shared promise-reaction functions so that the per-operation hot paths never
// allocate them. Subsequent phases populate the cached handles; this scaffold
// only wires up registration and snapshot support.
class BindingData : public SnapshotableObject {
 public:
  BindingData(Realm* realm, v8::Local<v8::Object> object);

  using InternalFieldInfo = InternalFieldInfoBase;

  SERIALIZABLE_OBJECT_METHODS()
  SET_BINDING_ID(webstreams_binding_data)

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_SELF_SIZE(BindingData)
  SET_MEMORY_INFO_NAME(BindingData)

  static void CreatePerIsolateProperties(IsolateData* isolate_data,
                                         v8::Local<v8::ObjectTemplate> target);
  static void CreatePerContextProperties(v8::Local<v8::Object> target,
                                         v8::Local<v8::Value> unused,
                                         v8::Local<v8::Context> context,
                                         void* priv);
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);

  // Returns the per-realm binding data for the current context.
  static BindingData* Get(Environment* env);

  // Lazily-created constructor templates for the readable-stream object model
  // (stored per-realm; created on first use).
  v8::Global<v8::FunctionTemplate> readable_stream_ctor;
  v8::Global<v8::FunctionTemplate> readable_stream_default_controller_ctor;
  v8::Global<v8::FunctionTemplate> readable_stream_default_reader_ctor;
  v8::Global<v8::FunctionTemplate> readable_byte_stream_controller_ctor;
  v8::Global<v8::FunctionTemplate> readable_stream_byob_reader_ctor;
  v8::Global<v8::FunctionTemplate> readable_stream_byob_request_ctor;
  v8::Global<v8::FunctionTemplate> writable_stream_ctor;
  v8::Global<v8::FunctionTemplate> writable_stream_default_writer_ctor;
  v8::Global<v8::FunctionTemplate> writable_stream_default_controller_ctor;
  v8::Global<v8::FunctionTemplate> transform_stream_ctor;
  v8::Global<v8::FunctionTemplate> transform_stream_default_controller_ctor;

  // Shared start-fulfilled reaction for the common "start returned a
  // non-object" case: the start promise is resolved with the *controller
  // wrapper* so this single per-realm function (rather than two per-creation
  // Function allocations) can recover it from the fulfilment value and mark
  // the controller started (by kind tag). Lazily created.
  v8::Global<v8::Function> start_fulfilled_reaction;

  // Shared per-realm TransformStream algorithm trampolines. All but start are
  // invoked by our own controllers with the transform stream's wrapper as
  // receiver (algo_receiver), so one function per realm replaces five
  // per-creation Function allocations. Lazily created.
  v8::Global<v8::Function> transform_write_tramp;
  v8::Global<v8::Function> transform_close_tramp;
  v8::Global<v8::Function> transform_abort_tramp;
  v8::Global<v8::Function> transform_pull_tramp;
  v8::Global<v8::Function> transform_cancel_tramp;

  // Boilerplate { value: undefined, done: <bool> } read-result objects, cloned
  // per read (v8::Object::Clone is a flat CopyJSObject) instead of building the
  // object property-by-property through the map-transition machinery. The
  // null-prototype variants serve non-author-code reads (pipeTo/tee), which must
  // never observe a patched Object.prototype.then. Lazily created on first use.
  v8::Global<v8::Object> read_result_not_done;
  v8::Global<v8::Object> read_result_done;
  v8::Global<v8::Object> read_result_not_done_null_proto;
  v8::Global<v8::Object> read_result_done_null_proto;

  // The realm's original %Promise.prototype%, cached for ReaderFastRead's
  // promise-lookalike chunk test (taken from a freshly created promise, not
  // the patchable global). Lazily created.
  v8::Global<v8::Object> promise_prototype;
};

}  // namespace webstreams
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_STREAMS_STREAMS_BINDING_H_
