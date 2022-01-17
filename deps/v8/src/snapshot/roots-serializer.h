// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_ROOTS_SERIALIZER_H_
#define V8_SNAPSHOT_ROOTS_SERIALIZER_H_

#include <bitset>

#include "src/objects/visitors.h"
#include "src/snapshot/serializer.h"

namespace v8 {
namespace internal {

class HeapObject;
class Object;
class Isolate;
enum class RootIndex : uint16_t;

// Base class for serializer that iterate over roots. Also maintains a cache
// that can be used to share non-root objects with other serializers.
class RootsSerializer : public Serializer {
 public:
  // The serializer expects that all roots before |first_root_to_be_serialized|
  // are already serialized.
  RootsSerializer(Isolate* isolate, Snapshot::SerializerFlags flags,
                  RootIndex first_root_to_be_serialized);
  RootsSerializer(const RootsSerializer&) = delete;
  RootsSerializer& operator=(const RootsSerializer&) = delete;

  bool can_be_rehashed() const { return can_be_rehashed_; }
  bool root_has_been_serialized(RootIndex root_index) const {
    return root_has_been_serialized_.test(static_cast<size_t>(root_index));
  }

  bool IsRootAndHasBeenSerialized(HeapObject obj) const {
    RootIndex root_index;
    return root_index_map()->Lookup(obj, &root_index) &&
           root_has_been_serialized(root_index);
  }

 protected:
  void CheckRehashability(HeapObject obj);

  // Serializes |object| if not previously seen and returns its cache index.
  int SerializeInObjectCache(Handle<HeapObject> object);

 private:
  void VisitRootPointers(Root root, const char* description,
                         FullObjectSlot start, FullObjectSlot end) override;
  void Synchronize(VisitorSynchronization::SyncTag tag) override;

  const RootIndex first_root_to_be_serialized_;
  std::bitset<RootsTable::kEntriesCount> root_has_been_serialized_;
  ObjectCacheIndexMap object_cache_index_map_;
  // Indicates whether we only serialized hash tables that we can rehash.
  // TODO(yangguo): generalize rehashing, and remove this flag.
  bool can_be_rehashed_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_ROOTS_SERIALIZER_H_
