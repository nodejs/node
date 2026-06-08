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
};

}  // namespace webstreams
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_STREAMS_STREAMS_BINDING_H_
