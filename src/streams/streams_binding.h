#ifndef SRC_STREAMS_STREAMS_BINDING_H_
#define SRC_STREAMS_STREAMS_BINDING_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "base_object.h"
#include "memory_tracker.h"
#include "node_snapshotable.h"
#include "v8.h"

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
// Carries a receiver signature (a brand-check failure throwing TypeError is the
// correct behavior for a plain attribute getter).
v8::Local<v8::FunctionTemplate> NewGetter(v8::Isolate* isolate,
                                          const char* prop,
                                          v8::FunctionCallback cb,
                                          v8::Local<v8::Signature> sig);

// Builds a getter FunctionTemplate ("get <prop>") for a Promise-typed attribute
// (e.g. reader.closed, writer.ready). Carries NO receiver signature: Web IDL
// requires the getter of a Promise-typed attribute to reject (not throw) on a
// brand-check failure, so the callback must run for foreign receivers too.
v8::Local<v8::FunctionTemplate> NewPromiseGetter(v8::Isolate* isolate,
                                                 const char* prop,
                                                 v8::FunctionCallback cb);

// Installs a synchronous operation on tmpl's prototype WITH a receiver
// signature and an explicit `.length` (for required arguments).
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

// A Promise rejected with TypeError("Illegal invocation"); returned by a
// Promise-returning operation invoked on a foreign receiver.
v8::Local<v8::Value> IllegalInvocationRejection(v8::Local<v8::Context> context);

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
};

}  // namespace webstreams
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_STREAMS_STREAMS_BINDING_H_
