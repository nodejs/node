#ifndef SRC_NODE_V8_H_
#define SRC_NODE_V8_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "aliased_buffer.h"
#include "base_object.h"
#include "node_snapshotable.h"
#include "util.h"
#include "v8.h"

namespace node {
class Environment;
struct InternalFieldInfo;

namespace v8_utils {
class BindingData : public SnapshotableObject {
 public:
  BindingData(Environment* env, v8::Local<v8::Object> obj);

  SERIALIZABLE_OBJECT_METHODS()
  static constexpr FastStringKey type_name{"node::v8::BindingData"};
  static constexpr EmbedderObjectType type_int =
      EmbedderObjectType::k_v8_binding_data;

  AliasedFloat64Array heap_statistics_buffer;
  AliasedFloat64Array heap_space_statistics_buffer;
  AliasedFloat64Array heap_code_statistics_buffer;

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_SELF_SIZE(BindingData)
  SET_MEMORY_INFO_NAME(BindingData)
};

}  // namespace v8_utils

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_V8_H_
