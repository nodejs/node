// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_STARTUP_SERIALIZER_H_
#define V8_SNAPSHOT_STARTUP_SERIALIZER_H_

#include <bitset>
#include "include/v8.h"
#include "src/snapshot/serializer.h"

namespace v8 {
namespace internal {

class StartupSerializer : public Serializer {
 public:
  StartupSerializer(
      Isolate* isolate,
      v8::SnapshotCreator::FunctionCodeHandling function_code_handling);
  ~StartupSerializer() override;

  // Serialize the current state of the heap.  The order is:
  // 1) Immortal immovable roots
  // 2) Remaining strong references.
  // 3) Partial snapshot cache.
  // 4) Weak references (e.g. the string table).
  void SerializeStrongReferences();
  void SerializeWeakReferencesAndDeferred();

  int PartialSnapshotCacheIndex(HeapObject* o);

 private:
  class PartialCacheIndexMap : public AddressMapBase {
   public:
    PartialCacheIndexMap() : map_(), next_index_(0) {}

    // Lookup object in the map. Return its index if found, or create
    // a new entry with new_index as value, and return kInvalidIndex.
    bool LookupOrInsert(HeapObject* obj, int* index_out) {
      base::HashMap::Entry* entry = LookupEntry(&map_, obj, false);
      if (entry != NULL) {
        *index_out = GetValue(entry);
        return true;
      }
      *index_out = next_index_;
      SetValue(LookupEntry(&map_, obj, true), next_index_++);
      return false;
    }

   private:
    base::HashMap map_;
    int next_index_;

    DISALLOW_COPY_AND_ASSIGN(PartialCacheIndexMap);
  };

  // The StartupSerializer has to serialize the root array, which is slightly
  // different.
  void VisitPointers(Object** start, Object** end) override;
  void SerializeObject(HeapObject* o, HowToCode how_to_code,
                       WhereToPoint where_to_point, int skip) override;
  void Synchronize(VisitorSynchronization::SyncTag tag) override;

  // Some roots should not be serialized, because their actual value depends on
  // absolute addresses and they are reset after deserialization, anyway.
  // In the first pass over the root list, we only serialize immortal immovable
  // roots. In the second pass, we serialize the rest.
  bool RootShouldBeSkipped(int root_index);

  bool clear_function_code_;
  bool serializing_builtins_;
  bool serializing_immortal_immovables_roots_;
  std::bitset<Heap::kStrongRootListLength> root_has_been_serialized_;
  PartialCacheIndexMap partial_cache_index_map_;
  DISALLOW_COPY_AND_ASSIGN(StartupSerializer);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_STARTUP_SERIALIZER_H_
