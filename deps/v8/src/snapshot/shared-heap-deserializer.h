// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_SHARED_HEAP_DESERIALIZER_H_
#define V8_SNAPSHOT_SHARED_HEAP_DESERIALIZER_H_

#include "src/snapshot/deserializer.h"
#include "src/snapshot/snapshot-data.h"

namespace v8 {
namespace internal {

// Initializes objects in the shared isolate that are not already included in
// the startup snapshot.
class SharedHeapDeserializer final : public Deserializer<Isolate> {
 public:
  explicit SharedHeapDeserializer(Isolate* isolate,
                                  const SnapshotData* shared_heap_data,
                                  bool can_rehash)
      : Deserializer(isolate, shared_heap_data->Payload(),
                     shared_heap_data->GetMagicNumber(), false, can_rehash) {}

  // Depending on runtime flags, deserialize shared heap objects into the
  // Isolate.
  void DeserializeIntoIsolate();

 private:
  void DeserializeStringTable();
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_SHARED_HEAP_DESERIALIZER_H_
