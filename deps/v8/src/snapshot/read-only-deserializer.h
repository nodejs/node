// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_READ_ONLY_DESERIALIZER_H_
#define V8_SNAPSHOT_READ_ONLY_DESERIALIZER_H_

#include "src/snapshot/deserializer.h"

namespace v8 {
namespace internal {

class SnapshotData;

// Deserializes the read-only blob and creates the read-only roots table.
class ReadOnlyDeserializer final : public Deserializer<Isolate> {
 public:
  ReadOnlyDeserializer(Isolate* isolate, const SnapshotData* data,
                       bool can_rehash);

  void DeserializeIntoIsolate();

 private:
  void PostProcessNewObjects();
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_READ_ONLY_DESERIALIZER_H_
