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

class StartupSerializer : public Serializer<> {
 public:
  explicit StartupSerializer(Isolate* isolate);
  ~StartupSerializer() override;

  // Serialize the current state of the heap.  The order is:
  // 1) Strong roots
  // 2) Builtins and bytecode handlers
  // 3) Partial snapshot cache
  // 4) Weak references (e.g. the string table)
  void SerializeStrongReferences();
  void SerializeWeakReferencesAndDeferred();

  int PartialSnapshotCacheIndex(HeapObject* o);

  bool can_be_rehashed() const { return can_be_rehashed_; }
  bool root_has_been_serialized(RootIndex root_index) const {
    return root_has_been_serialized_.test(static_cast<size_t>(root_index));
  }

 private:
  class PartialCacheIndexMap {
   public:
    PartialCacheIndexMap() : map_(), next_index_(0) {}

    // Lookup object in the map. Return its index if found, or create
    // a new entry with new_index as value, and return kInvalidIndex.
    bool LookupOrInsert(HeapObject* obj, int* index_out) {
      Maybe<uint32_t> maybe_index = map_.Get(obj);
      if (maybe_index.IsJust()) {
        *index_out = maybe_index.FromJust();
        return true;
      }
      *index_out = next_index_;
      map_.Set(obj, next_index_++);
      return false;
    }

   private:
    DisallowHeapAllocation no_allocation_;
    HeapObjectToIndexHashMap map_;
    int next_index_;

    DISALLOW_COPY_AND_ASSIGN(PartialCacheIndexMap);
  };

  // The StartupSerializer has to serialize the root array, which is slightly
  // different.
  void VisitRootPointers(Root root, const char* description, Object** start,
                         Object** end) override;
  void SerializeObject(HeapObject* o, HowToCode how_to_code,
                       WhereToPoint where_to_point, int skip) override;
  void Synchronize(VisitorSynchronization::SyncTag tag) override;
  bool MustBeDeferred(HeapObject* object) override;

  void CheckRehashability(HeapObject* obj);

  std::bitset<RootsTable::kEntriesCount> root_has_been_serialized_;
  PartialCacheIndexMap partial_cache_index_map_;
  std::vector<AccessorInfo*> accessor_infos_;
  std::vector<CallHandlerInfo*> call_handler_infos_;
  // Indicates whether we only serialized hash tables that we can rehash.
  // TODO(yangguo): generalize rehashing, and remove this flag.
  bool can_be_rehashed_;

  DISALLOW_COPY_AND_ASSIGN(StartupSerializer);
};

class SerializedHandleChecker : public RootVisitor {
 public:
  SerializedHandleChecker(Isolate* isolate, std::vector<Context*>* contexts);
  void VisitRootPointers(Root root, const char* description, Object** start,
                         Object** end) override;
  bool CheckGlobalAndEternalHandles();

 private:
  void AddToSet(FixedArray* serialized);

  Isolate* isolate_;
  std::unordered_set<Object*> serialized_;
  bool ok_ = true;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_STARTUP_SERIALIZER_H_
