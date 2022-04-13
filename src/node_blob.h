#ifndef SRC_NODE_BLOB_H_
#define SRC_NODE_BLOB_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "async_wrap.h"
#include "base_object.h"
#include "env.h"
#include "memory_tracker.h"
#include "node_internals.h"
#include "node_snapshotable.h"
#include "node_worker.h"
#include "v8.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace node {

struct BlobEntry {
  std::shared_ptr<v8::BackingStore> store;
  size_t length;
  size_t offset;
};

class Blob : public BaseObject {
 public:
  static void RegisterExternalReferences(
      ExternalReferenceRegistry* registry);

  static void Initialize(
      v8::Local<v8::Object> target,
      v8::Local<v8::Value> unused,
      v8::Local<v8::Context> context,
      void* priv);

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ToArrayBuffer(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ToSlice(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void StoreDataObject(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetDataObject(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RevokeDataObject(const v8::FunctionCallbackInfo<v8::Value>& args);

  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);

  static BaseObjectPtr<Blob> Create(
      Environment* env,
      const std::vector<BlobEntry> store,
      size_t length);

  static bool HasInstance(Environment* env, v8::Local<v8::Value> object);

  const std::vector<BlobEntry> entries() const {
    return store_;
  }

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(Blob)
  SET_SELF_SIZE(Blob)

  // Copies the contents of the Blob into an ArrayBuffer.
  v8::MaybeLocal<v8::Value> GetArrayBuffer(Environment* env);

  BaseObjectPtr<Blob> Slice(Environment* env, size_t start, size_t end);

  inline size_t length() const { return length_; }

  class BlobTransferData : public worker::TransferData {
   public:
    explicit BlobTransferData(
        const std::vector<BlobEntry>& store,
      size_t length)
        : store_(store),
          length_(length) {}

    BaseObjectPtr<BaseObject> Deserialize(
        Environment* env,
        v8::Local<v8::Context> context,
        std::unique_ptr<worker::TransferData> self) override;

    SET_MEMORY_INFO_NAME(BlobTransferData)
    SET_SELF_SIZE(BlobTransferData)
    SET_NO_MEMORY_INFO()

   private:
    std::vector<BlobEntry> store_;
    size_t length_ = 0;
  };

  BaseObject::TransferMode GetTransferMode() const override;
  std::unique_ptr<worker::TransferData> CloneForMessaging() const override;

  Blob(
      Environment* env,
      v8::Local<v8::Object> obj,
      const std::vector<BlobEntry>& store,
      size_t length);

 private:
  std::vector<BlobEntry> store_;
  size_t length_ = 0;
};

class FixedSizeBlobCopyJob : public AsyncWrap, public ThreadPoolWork {
 public:
  enum class Mode {
    SYNC,
    ASYNC
  };

  static void RegisterExternalReferences(
      ExternalReferenceRegistry* registry);
  static void Initialize(Environment* env, v8::Local<v8::Object> target);
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Run(const v8::FunctionCallbackInfo<v8::Value>& args);

  bool IsNotIndicativeOfMemoryLeakAtExit() const override {
    return true;
  }

  void DoThreadPoolWork() override;
  void AfterThreadPoolWork(int status) override;

  Mode mode() const { return mode_; }

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(FixedSizeBlobCopyJob)
  SET_SELF_SIZE(FixedSizeBlobCopyJob)

 private:
  FixedSizeBlobCopyJob(
    Environment* env,
    v8::Local<v8::Object> object,
    Blob* blob,
    Mode mode = Mode::ASYNC);

  Mode mode_;
  std::vector<BlobEntry> source_;
  std::shared_ptr<v8::BackingStore> destination_;
  size_t length_ = 0;
};

class BlobBindingData : public SnapshotableObject {
 public:
  explicit BlobBindingData(Environment* env, v8::Local<v8::Object> wrap);

  SERIALIZABLE_OBJECT_METHODS()

  static constexpr FastStringKey type_name{"node::BlobBindingData"};
  static constexpr EmbedderObjectType type_int =
      EmbedderObjectType::k_blob_binding_data;

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_SELF_SIZE(BlobBindingData)
  SET_MEMORY_INFO_NAME(BlobBindingData)

  struct StoredDataObject : public MemoryRetainer {
    BaseObjectPtr<Blob> blob;
    size_t length;
    std::string type;

    StoredDataObject() = default;

    StoredDataObject(
        const BaseObjectPtr<Blob>& blob_,
        size_t length_,
        const std::string& type_);

    void MemoryInfo(MemoryTracker* tracker) const override;
    SET_SELF_SIZE(StoredDataObject)
    SET_MEMORY_INFO_NAME(StoredDataObject)
  };

  void store_data_object(
      const std::string& uuid,
      const StoredDataObject& object);

  void revoke_data_object(const std::string& uuid);

  StoredDataObject get_data_object(const std::string& uuid);

 private:
  std::unordered_map<std::string, StoredDataObject> data_objects_;
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_NODE_BLOB_H_
