// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_READ_ONLY_SERIALIZER_H_
#define V8_SNAPSHOT_READ_ONLY_SERIALIZER_H_

#include <unordered_set>

#include "src/base/hashmap.h"
#include "src/snapshot/roots-serializer.h"

namespace v8 {
namespace internal {

class HeapObject;
class SnapshotByteSink;

class V8_EXPORT_PRIVATE ReadOnlySerializer : public RootsSerializer {
 public:
  ReadOnlySerializer(Isolate* isolate, Snapshot::SerializerFlags flags);
  ~ReadOnlySerializer() override;
  ReadOnlySerializer(const ReadOnlySerializer&) = delete;
  ReadOnlySerializer& operator=(const ReadOnlySerializer&) = delete;

  void SerializeReadOnlyRoots();

  // Completes the serialization of the read-only object cache (after it has
  // been filled by other serializers) and serializes any deferred objects.
  void FinalizeSerialization();

 private:
  void ReconstructReadOnlyObjectCacheForTesting();

  void SerializeObjectImpl(Handle<HeapObject> o) override;
  bool MustBeDeferred(HeapObject object) override;

  // If |obj| can be serialized in the read-only snapshot then add it to the
  // read-only object cache if not already present and emit a
  // ReadOnlyObjectCache bytecode into |sink|. Returns whether this was
  // successful.
  bool SerializeUsingReadOnlyObjectCache(SnapshotByteSink* sink,
                                         Handle<HeapObject> obj);

#ifdef V8_STATIC_ROOTS
  using CodeEntryPointVector = std::vector<Address>;
  void WipeCodeEntryPointsForDeterministicSerialization(
      CodeEntryPointVector& saved_entry_points);
  void RestoreCodeEntryPoints(const CodeEntryPointVector& saved_entry_points);
#endif  // V8_STATIC_ROOTS

#ifdef DEBUG
  IdentityMap<int, base::DefaultAllocationPolicy> serialized_objects_;
  bool did_serialize_not_mapped_symbol_;
#endif

  // For SerializeUsingReadOnlyObjectCache.
  friend class SharedHeapSerializer;
  friend class StartupSerializer;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_READ_ONLY_SERIALIZER_H_
