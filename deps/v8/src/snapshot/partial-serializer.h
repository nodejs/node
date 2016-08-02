// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_PARTIAL_SERIALIZER_H_
#define V8_SNAPSHOT_PARTIAL_SERIALIZER_H_

#include "src/address-map.h"
#include "src/snapshot/serializer.h"

namespace v8 {
namespace internal {

class PartialSerializer : public Serializer {
 public:
  PartialSerializer(Isolate* isolate, Serializer* startup_snapshot_serializer,
                    SnapshotByteSink* sink);

  ~PartialSerializer() override;

  // Serialize the objects reachable from a single object pointer.
  void Serialize(Object** o);

 private:
  class PartialCacheIndexMap : public AddressMapBase {
   public:
    PartialCacheIndexMap() : map_(HashMap::PointersMatch) {}

    static const int kInvalidIndex = -1;

    // Lookup object in the map. Return its index if found, or create
    // a new entry with new_index as value, and return kInvalidIndex.
    int LookupOrInsert(HeapObject* obj, int new_index) {
      HashMap::Entry* entry = LookupEntry(&map_, obj, false);
      if (entry != NULL) return GetValue(entry);
      SetValue(LookupEntry(&map_, obj, true), static_cast<uint32_t>(new_index));
      return kInvalidIndex;
    }

   private:
    HashMap map_;

    DISALLOW_COPY_AND_ASSIGN(PartialCacheIndexMap);
  };

  void SerializeObject(HeapObject* o, HowToCode how_to_code,
                       WhereToPoint where_to_point, int skip) override;

  int PartialSnapshotCacheIndex(HeapObject* o);
  bool ShouldBeInThePartialSnapshotCache(HeapObject* o);

  Serializer* startup_serializer_;
  PartialCacheIndexMap partial_cache_index_map_;
  int next_partial_cache_index_;
  DISALLOW_COPY_AND_ASSIGN(PartialSerializer);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_PARTIAL_SERIALIZER_H_
