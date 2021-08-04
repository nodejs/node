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

// Deserializes the object graph rooted at a given object.
class ObjectDeserializer final : public Deserializer {
 public:
  static MaybeHandle<SharedFunctionInfo> DeserializeSharedFunctionInfo(
      Isolate* isolate, const SerializedCodeData* data, Handle<String> source);
  static MaybeHandle<SharedFunctionInfo> DeserializeSharedFunctionInfoOffThread(
      LocalIsolate* isolate, const SerializedCodeData* data,
      Handle<String> source);

 private:
  explicit ObjectDeserializer(Isolate* isolate, const SerializedCodeData* data);

  // Deserialize an object graph. Fail gracefully.
  MaybeHandle<HeapObject> Deserialize();

  void LinkAllocationSites();
  void CommitPostProcessedObjects();
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_OBJECT_DESERIALIZER_H_
