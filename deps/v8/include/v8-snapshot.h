// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_V8_SNAPSHOT_H_
#define INCLUDE_V8_SNAPSHOT_H_

#include "v8-internal.h"      // NOLINT(build/include_directory)
#include "v8-local-handle.h"  // NOLINT(build/include_directory)
#include "v8config.h"         // NOLINT(build/include_directory)

namespace v8 {

class Object;

class V8_EXPORT StartupData {
 public:
  /**
   * Whether the data created can be rehashed and and the hash seed can be
   * recomputed when deserialized.
   * Only valid for StartupData returned by SnapshotCreator::CreateBlob().
   */
  bool CanBeRehashed() const;
  /**
   * Allows embedders to verify whether the data is valid for the current
   * V8 instance.
   */
  bool IsValid() const;

  const char* data;
  int raw_size;
};

/**
 * Callback and supporting data used in SnapshotCreator to implement embedder
 * logic to serialize internal fields.
 * Internal fields that directly reference V8 objects are serialized without
 * calling this callback. Internal fields that contain aligned pointers are
 * serialized by this callback if it returns non-zero result. Otherwise it is
 * serialized verbatim.
 */
struct SerializeInternalFieldsCallback {
  using CallbackFunction = StartupData (*)(Local<Object> holder, int index,
                                           void* data);
  SerializeInternalFieldsCallback(CallbackFunction function = nullptr,
                                  void* data_arg = nullptr)
      : callback(function), data(data_arg) {}
  CallbackFunction callback;
  void* data;
};
// Note that these fields are called "internal fields" in the API and called
// "embedder fields" within V8.
using SerializeEmbedderFieldsCallback = SerializeInternalFieldsCallback;

/**
 * Callback and supporting data used to implement embedder logic to deserialize
 * internal fields.
 */
struct DeserializeInternalFieldsCallback {
  using CallbackFunction = void (*)(Local<Object> holder, int index,
                                    StartupData payload, void* data);
  DeserializeInternalFieldsCallback(CallbackFunction function = nullptr,
                                    void* data_arg = nullptr)
      : callback(function), data(data_arg) {}
  void (*callback)(Local<Object> holder, int index, StartupData payload,
                   void* data);
  void* data;
};

using DeserializeEmbedderFieldsCallback = DeserializeInternalFieldsCallback;

/**
 * Helper class to create a snapshot data blob.
 *
 * The Isolate used by a SnapshotCreator is owned by it, and will be entered
 * and exited by the constructor and destructor, respectively; The destructor
 * will also destroy the Isolate. Experimental language features, including
 * those available by default, are not available while creating a snapshot.
 */
class V8_EXPORT SnapshotCreator {
 public:
  enum class FunctionCodeHandling { kClear, kKeep };

  /**
   * Initialize and enter an isolate, and set it up for serialization.
   * The isolate is either created from scratch or from an existing snapshot.
   * The caller keeps ownership of the argument snapshot.
   * \param existing_blob existing snapshot from which to create this one.
   * \param external_references a null-terminated array of external references
   *        that must be equivalent to CreateParams::external_references.
   * \param owns_isolate whether this SnapshotCreator should call
   *        v8::Isolate::Dispose() during its destructor.
   */
  SnapshotCreator(Isolate* isolate,
                  const intptr_t* external_references = nullptr,
                  const StartupData* existing_blob = nullptr,
                  bool owns_isolate = true);

  /**
   * Create and enter an isolate, and set it up for serialization.
   * The isolate is either created from scratch or from an existing snapshot.
   * The caller keeps ownership of the argument snapshot.
   * \param existing_blob existing snapshot from which to create this one.
   * \param external_references a null-terminated array of external references
   *        that must be equivalent to CreateParams::external_references.
   */
  SnapshotCreator(const intptr_t* external_references = nullptr,
                  const StartupData* existing_blob = nullptr);

  /**
   * Destroy the snapshot creator, and exit and dispose of the Isolate
   * associated with it.
   */
  ~SnapshotCreator();

  /**
   * \returns the isolate prepared by the snapshot creator.
   */
  Isolate* GetIsolate();

  /**
   * Set the default context to be included in the snapshot blob.
   * The snapshot will not contain the global proxy, and we expect one or a
   * global object template to create one, to be provided upon deserialization.
   *
   * \param callback optional callback to serialize internal fields.
   */
  void SetDefaultContext(Local<Context> context,
                         SerializeInternalFieldsCallback callback =
                             SerializeInternalFieldsCallback());

  /**
   * Add additional context to be included in the snapshot blob.
   * The snapshot will include the global proxy.
   *
   * \param callback optional callback to serialize internal fields.
   *
   * \returns the index of the context in the snapshot blob.
   */
  size_t AddContext(Local<Context> context,
                    SerializeInternalFieldsCallback callback =
                        SerializeInternalFieldsCallback());

  /**
   * Attach arbitrary V8::Data to the context snapshot, which can be retrieved
   * via Context::GetDataFromSnapshotOnce after deserialization. This data does
   * not survive when a new snapshot is created from an existing snapshot.
   * \returns the index for retrieval.
   */
  template <class T>
  V8_INLINE size_t AddData(Local<Context> context, Local<T> object);

  /**
   * Attach arbitrary V8::Data to the isolate snapshot, which can be retrieved
   * via Isolate::GetDataFromSnapshotOnce after deserialization. This data does
   * not survive when a new snapshot is created from an existing snapshot.
   * \returns the index for retrieval.
   */
  template <class T>
  V8_INLINE size_t AddData(Local<T> object);

  /**
   * Created a snapshot data blob.
   * This must not be called from within a handle scope.
   * \param function_code_handling whether to include compiled function code
   *        in the snapshot.
   * \returns { nullptr, 0 } on failure, and a startup snapshot on success. The
   *        caller acquires ownership of the data array in the return value.
   */
  StartupData CreateBlob(FunctionCodeHandling function_code_handling);

  // Disallow copying and assigning.
  SnapshotCreator(const SnapshotCreator&) = delete;
  void operator=(const SnapshotCreator&) = delete;

 private:
  size_t AddData(Local<Context> context, internal::Address object);
  size_t AddData(internal::Address object);

  void* data_;
};

template <class T>
size_t SnapshotCreator::AddData(Local<Context> context, Local<T> object) {
  return AddData(context, internal::ValueHelper::ValueAsAddress(*object));
}

template <class T>
size_t SnapshotCreator::AddData(Local<T> object) {
  return AddData(internal::ValueHelper::ValueAsAddress(*object));
}

}  // namespace v8

#endif  // INCLUDE_V8_SNAPSHOT_H_
