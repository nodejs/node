// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_SHARED_HEAP_SERIALIZER_H_
#define V8_SNAPSHOT_SHARED_HEAP_SERIALIZER_H_

#include "src/snapshot/roots-serializer.h"

namespace v8 {
namespace internal {

class HeapObject;

// SharedHeapSerializer serializes objects that should be in the shared heap in
// the shared Isolate during startup. Currently the shared heap is only in use
// behind flags (e.g. --shared-string-table). When it is not in use, its
// contents are deserialized into each Isolate.
class V8_EXPORT_PRIVATE SharedHeapSerializer : public RootsSerializer {
 public:
  SharedHeapSerializer(Isolate* isolate, Snapshot::SerializerFlags flags);
  ~SharedHeapSerializer() override;
  SharedHeapSerializer(const SharedHeapSerializer&) = delete;
  SharedHeapSerializer& operator=(const SharedHeapSerializer&) = delete;

  // Terminate the shared heap object cache with an undefined value and
  // serialize the string table..
  void FinalizeSerialization();

  // If |obj| can be serialized in the shared heap snapshot then add it to the
  // shared heap object cache if not already present and emit a
  // SharedHeapObjectCache bytecode into |sink|. Returns whether this was
  // successful.
  bool SerializeUsingSharedHeapObjectCache(SnapshotByteSink* sink,
                                           Handle<HeapObject> obj);

  static bool CanBeInSharedOldSpace(Tagged<HeapObject> obj);

  static bool ShouldBeInSharedHeapObjectCache(Tagged<HeapObject> obj);

 private:
  bool ShouldReconstructSharedHeapObjectCacheForTesting() const;

  void ReconstructSharedHeapObjectCacheForTesting();

  void SerializeStringTable(StringTable* string_table);

  void SerializeObjectImpl(Handle<HeapObject> obj, SlotType slot_type) override;

#ifdef DEBUG
  IdentityMap<int, base::DefaultAllocationPolicy> serialized_objects_;
#endif
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_SHARED_HEAP_SERIALIZER_H_
