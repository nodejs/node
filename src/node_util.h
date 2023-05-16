
#ifndef SRC_NODE_UTIL_H_
#define SRC_NODE_UTIL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#include "base_object.h"
#include "node_snapshotable.h"
#include "v8.h"

namespace node {
namespace util {

class WeakReference : public SnapshotableObject {
 public:
  SERIALIZABLE_OBJECT_METHODS()

  SET_OBJECT_ID(util_weak_reference)

  WeakReference(Realm* realm,
                v8::Local<v8::Object> object,
                v8::Local<v8::Object> target);
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Get(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void IncRef(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void DecRef(const v8::FunctionCallbackInfo<v8::Value>& args);

  SET_MEMORY_INFO_NAME(WeakReference)
  SET_SELF_SIZE(WeakReference)
  SET_NO_MEMORY_INFO()

  struct InternalFieldInfo : public node::InternalFieldInfoBase {
    SnapshotIndex target;
    uint64_t reference_count;
  };

 private:
  WeakReference(Realm* realm,
                v8::Local<v8::Object> object,
                v8::Local<v8::Object> target,
                uint64_t reference_count);
  v8::Global<v8::Object> target_;
  uint64_t reference_count_ = 0;

  SnapshotIndex target_index_ = 0;  // 0 means target_ is not snapshotted
};

}  // namespace util
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_UTIL_H_
