// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_READ_ONLY_SERIALIZER_H_
#define V8_SNAPSHOT_READ_ONLY_SERIALIZER_H_

#include "src/snapshot/roots-serializer.h"

namespace v8 {
namespace internal {

// TODO(jgruber): Now that this does a memcpy-style serialization, there is no
// longer a fundamental reason to inherit from RootsSerializer. It's still
// convenient though because callers expect parts of the Serializer interface
// (e.g.: rehashability, serialization statistics, blob creation).
// Consider removing this inheritance.
class V8_EXPORT_PRIVATE ReadOnlySerializer : public RootsSerializer {
 public:
  ReadOnlySerializer(Isolate* isolate, Snapshot::SerializerFlags flags);
  ~ReadOnlySerializer() override;

  // Serializes the entire ReadOnlySpace as well as the ReadOnlyRoots table.
  void Serialize();

 private:
  void SerializeObjectImpl(Handle<HeapObject> o, SlotType slot_type) override {
    UNREACHABLE();
  }

  ReadOnlySerializer(const ReadOnlySerializer&) = delete;
  ReadOnlySerializer& operator=(const ReadOnlySerializer&) = delete;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_READ_ONLY_SERIALIZER_H_
