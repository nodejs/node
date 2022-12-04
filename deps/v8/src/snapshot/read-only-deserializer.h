// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_READ_ONLY_DESERIALIZER_H_
#define V8_SNAPSHOT_READ_ONLY_DESERIALIZER_H_

#include "src/snapshot/deserializer.h"
#include "src/snapshot/snapshot-data.h"

namespace v8 {
namespace internal {

// Deserializes the read-only blob, creating the read-only roots and the
// Read-only object cache used by the other deserializers.
class ReadOnlyDeserializer final : public Deserializer<Isolate> {
 public:
  explicit ReadOnlyDeserializer(Isolate* isolate, const SnapshotData* data,
                                bool can_rehash)
      : Deserializer(isolate, data->Payload(), data->GetMagicNumber(), false,
                     can_rehash) {}

  // Deserialize the snapshot into an empty heap.
  void DeserializeIntoIsolate();
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_READ_ONLY_DESERIALIZER_H_
