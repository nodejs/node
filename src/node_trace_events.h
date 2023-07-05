#ifndef SRC_NODE_TRACE_EVENTS_H_
#define SRC_NODE_TRACE_EVENTS_H_

#include <cinttypes>
#include "aliased_buffer.h"
#include "node.h"
#include "node_snapshotable.h"
#include "util.h"
#include "v8-fast-api-calls.h"
#include "v8.h"

#include <string>

namespace node {
class ExternalReferenceRegistry;

namespace trace_events {

class BindingData : public SnapshotableObject {
 public:
  BindingData(Realm* realm, v8::Local<v8::Object> obj);

  using InternalFieldInfo = InternalFieldInfoBase;

  SERIALIZABLE_OBJECT_METHODS()
  SET_BINDING_ID(trace_events_binding_data)
  SET_NO_MEMORY_INFO()
  SET_SELF_SIZE(BindingData)
  SET_MEMORY_INFO_NAME(BindingData)

  static void CreatePerContextProperties(v8::Local<v8::Object> target,
                                         v8::Local<v8::Value> unused,
                                         v8::Local<v8::Context> context,
                                         void* priv);
};

}  // namespace trace_events

}  // namespace node

#endif  // SRC_NODE_TRACE_EVENTS_H_
