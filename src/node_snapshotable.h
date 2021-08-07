
#ifndef SRC_NODE_SNAPSHOTABLE_H_
#define SRC_NODE_SNAPSHOTABLE_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "base_object.h"
#include "util.h"

namespace node {

class Environment;
struct EnvSerializeInfo;
struct SnapshotData;

#define SERIALIZABLE_OBJECT_TYPES(V)                                           \
  V(fs_binding_data, fs::BindingData)                                          \
  V(v8_binding_data, v8_utils::BindingData)                                    \
  V(blob_binding_data, BlobBindingData)

enum class EmbedderObjectType : uint8_t {
  k_default = 0,
#define V(PropertyName, NativeType) k_##PropertyName,
  SERIALIZABLE_OBJECT_TYPES(V)
#undef V
};

// When serializing an embedder object, we'll serialize the native states
// into a chunk that can be mapped into a subclass of InternalFieldInfo,
// and pass it into the V8 callback as the payload of StartupData.
// TODO(joyeecheung): the classification of types seem to be wrong.
// We'd need a type for each field of each class of native object.
// Maybe it's fine - we'll just use the type to invoke BaseObject constructors
// and specify that the BaseObject has only one field for us to serialize.
// And for non-BaseObject embedder objects, we'll use field-wise types.
// The memory chunk looks like this:
//
// [   type   ] - EmbedderObjectType (a uint8_t)
// [  length  ] - a size_t
// [    ...   ] - custom bytes of size |length - header size|
struct InternalFieldInfo {
  EmbedderObjectType type;
  size_t length;

  InternalFieldInfo() = delete;

  static InternalFieldInfo* New(EmbedderObjectType type) {
    return New(type, sizeof(InternalFieldInfo));
  }

  static InternalFieldInfo* New(EmbedderObjectType type, size_t length) {
    InternalFieldInfo* result =
        reinterpret_cast<InternalFieldInfo*>(::operator new[](length));
    result->type = type;
    result->length = length;
    return result;
  }

  InternalFieldInfo* Copy() const {
    InternalFieldInfo* result =
        reinterpret_cast<InternalFieldInfo*>(::operator new[](length));
    memcpy(result, this, length);
    return result;
  }

  void Delete() { ::operator delete[](this); }
};

// An interface for snapshotable native objects to inherit from.
// Use the SERIALIZABLE_OBJECT_METHODS() macro in the class to define
// the following methods to implement:
//
// - PrepareForSerialization(): This would be run prior to context
//   serialization. Use this method to e.g. release references that
//   can be re-initialized, or perform property store operations
//   that needs a V8 context.
// - Serialize(): This would be called during context serialization,
//   once for each embedder field of the object.
//   Allocate and construct an InternalFieldInfo object that contains
//   data that can be used to deserialize native states.
// - Deserialize(): This would be called after the context is
//   deserialized and the object graph is complete, once for each
//   embedder field of the object. Use this to restore native states
//   in the object.
class SnapshotableObject : public BaseObject {
 public:
  SnapshotableObject(Environment* env,
                     v8::Local<v8::Object> wrap,
                     EmbedderObjectType type = EmbedderObjectType::k_default);
  const char* GetTypeNameChars() const;

  virtual void PrepareForSerialization(v8::Local<v8::Context> context,
                                       v8::SnapshotCreator* creator) = 0;
  virtual InternalFieldInfo* Serialize(int index) = 0;
  bool is_snapshotable() const override { return true; }
  // We'll make sure that the type is set in the constructor
  EmbedderObjectType type() { return type_; }

 private:
  EmbedderObjectType type_;
};

#define SERIALIZABLE_OBJECT_METHODS()                                          \
  void PrepareForSerialization(v8::Local<v8::Context> context,                 \
                               v8::SnapshotCreator* creator) override;         \
  InternalFieldInfo* Serialize(int index) override;                            \
  static void Deserialize(v8::Local<v8::Context> context,                      \
                          v8::Local<v8::Object> holder,                        \
                          int index,                                           \
                          InternalFieldInfo* info);

v8::StartupData SerializeNodeContextInternalFields(v8::Local<v8::Object> holder,
                                                   int index,
                                                   void* env);
void DeserializeNodeInternalFields(v8::Local<v8::Object> holder,
                                   int index,
                                   v8::StartupData payload,
                                   void* env);
void SerializeBindingData(Environment* env,
                          v8::SnapshotCreator* creator,
                          EnvSerializeInfo* info);

bool IsSnapshotableType(FastStringKey key);

class SnapshotBuilder {
 public:
  static std::string Generate(const std::vector<std::string> args,
                              const std::vector<std::string> exec_args);
  static void Generate(SnapshotData* out,
                       const std::vector<std::string> args,
                       const std::vector<std::string> exec_args);
};
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_SNAPSHOTABLE_H_
