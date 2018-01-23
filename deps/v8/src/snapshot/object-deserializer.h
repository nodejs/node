// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_OBJECT_DESERIALIZER_H_
#define V8_SNAPSHOT_OBJECT_DESERIALIZER_H_

#include "src/snapshot/deserializer.h"

namespace v8 {
namespace internal {

class SerializedCodeData;
class SharedFunctionInfo;
class WasmCompiledModule;

// Deserializes the object graph rooted at a given object.
class ObjectDeserializer final : public Deserializer<> {
 public:
  static MaybeHandle<SharedFunctionInfo> DeserializeSharedFunctionInfo(
      Isolate* isolate, const SerializedCodeData* data, Handle<String> source);

  static MaybeHandle<WasmCompiledModule> DeserializeWasmCompiledModule(
      Isolate* isolate, const SerializedCodeData* data,
      Vector<const byte> wire_bytes);

 private:
  explicit ObjectDeserializer(const SerializedCodeData* data)
      : Deserializer(data, true) {}

  // Deserialize an object graph. Fail gracefully.
  MaybeHandle<HeapObject> Deserialize(Isolate* isolate);

  void FlushICacheForNewCodeObjectsAndRecordEmbeddedObjects();
  void CommitPostProcessedObjects();
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_OBJECT_DESERIALIZER_H_
