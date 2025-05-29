#ifndef SRC_NODE_BLOB_H_
#define SRC_NODE_BLOB_H_

#include "v8-function-callback.h"
#include "v8-template.h"
#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <string>
#include <unordered_map>
#include <vector>
#include "async_wrap.h"
#include "base_object.h"
#include "dataqueue/queue.h"
#include "env.h"
#include "memory_tracker.h"
#include "node_internals.h"
#include "node_snapshotable.h"
#include "node_worker.h"
#include "v8-fast-api-calls.h"
#include "v8.h"

namespace node {

class Blob : public BaseObject {
 public:
  static void RegisterExternalReferences(
      ExternalReferenceRegistry* registry);

  static void CreatePerIsolateProperties(IsolateData* isolate_data,
                                         v8::Local<v8::ObjectTemplate> target);
  static void CreatePerContextProperties(v8::Local<v8::Object> target,
                                         v8::Local<v8::Value> unused,
                                         v8::Local<v8::Context> context,
                                         void* priv);

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetReader(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ToSlice(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void StoreDataObject(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetDataObject(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RevokeObjectURL(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void FastRevokeObjectURL(v8::Local<v8::Value> receiver,
                                  v8::Local<v8::Value> raw_input,
                                  v8::FastApiCallbackOptions& options);

  static v8::CFunction fast_revoke_object_url_method;

  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);

  static BaseObjectPtr<Blob> Create(Environment* env,
                                    std::shared_ptr<DataQueue> data_queue);

  static bool HasInstance(Environment* env, v8::Local<v8::Value> object);

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(Blob)
  SET_SELF_SIZE(Blob)

  BaseObjectPtr<Blob> Slice(Environment* env, size_t start, size_t end);

  inline size_t length() const { return this->data_queue_->size().value(); }

  class BlobTransferData : public worker::TransferData {
   public:
    explicit BlobTransferData(std::shared_ptr<DataQueue> data_queue)
        : data_queue(data_queue) {}

    BaseObjectPtr<BaseObject> Deserialize(
        Environment* env,
        v8::Local<v8::Context> context,
        std::unique_ptr<worker::TransferData> self) override;

    SET_MEMORY_INFO_NAME(BlobTransferData)
    SET_SELF_SIZE(BlobTransferData)
    SET_NO_MEMORY_INFO()

   private:
    std::shared_ptr<DataQueue> data_queue;
  };

  class Reader final : public AsyncWrap {
   public:
    static bool HasInstance(Environment* env, v8::Local<v8::Value> value);
    static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
        Environment* env);
    static BaseObjectPtr<Reader> Create(Environment* env,
                                        BaseObjectPtr<Blob> blob);
    static void Pull(const v8::FunctionCallbackInfo<v8::Value>& args);

    explicit Reader(Environment* env,
                    v8::Local<v8::Object> obj,
                    BaseObjectPtr<Blob> strong_ptr);

    SET_NO_MEMORY_INFO()
    SET_MEMORY_INFO_NAME(Blob::Reader)
    SET_SELF_SIZE(Reader)

   private:
    std::shared_ptr<DataQueue::Reader> inner_;
    BaseObjectPtr<Blob> strong_ptr_;
    bool eos_ = false;
  };

  BaseObject::TransferMode GetTransferMode() const override;
  std::unique_ptr<worker::TransferData> CloneForMessaging() const override;

  Blob(Environment* env,
       v8::Local<v8::Object> obj,
       std::shared_ptr<DataQueue> data_queue);

  DataQueue& getDataQueue() const { return *data_queue_; }

 private:
  std::shared_ptr<DataQueue> data_queue_;
};

class BlobBindingData : public SnapshotableObject {
 public:
  explicit BlobBindingData(Realm* realm, v8::Local<v8::Object> wrap);

  using InternalFieldInfo = InternalFieldInfoBase;

  SERIALIZABLE_OBJECT_METHODS()

  SET_BINDING_ID(blob_binding_data)

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
