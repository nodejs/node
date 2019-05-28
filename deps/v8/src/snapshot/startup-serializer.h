// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_STARTUP_SERIALIZER_H_
#define V8_SNAPSHOT_STARTUP_SERIALIZER_H_

#include <unordered_set>

#include "src/snapshot/roots-serializer.h"

namespace v8 {
namespace internal {

class HeapObject;
class SnapshotByteSink;
class ReadOnlySerializer;

class V8_EXPORT_PRIVATE StartupSerializer : public RootsSerializer {
 public:
  StartupSerializer(Isolate* isolate, ReadOnlySerializer* read_only_serializer);
  ~StartupSerializer() override;

  // Serialize the current state of the heap.  The order is:
  // 1) Strong roots
  // 2) Builtins and bytecode handlers
  // 3) Partial snapshot cache
  // 4) Weak references (e.g. the string table)
  void SerializeStrongReferences();
  void SerializeWeakReferencesAndDeferred();

  // If |obj| can be serialized in the read-only snapshot then add it to the
  // read-only object cache if not already present and emits a
  // ReadOnlyObjectCache bytecode into |sink|. Returns whether this was
  // successful.
  bool SerializeUsingReadOnlyObjectCache(SnapshotByteSink* sink,
                                         HeapObject obj);

  // Adds |obj| to the partial snapshot object cache if not already present and
  // emits a PartialSnapshotCache bytecode into |sink|.
  void SerializeUsingPartialSnapshotCache(SnapshotByteSink* sink,
                                          HeapObject obj);

 private:
  void SerializeObject(HeapObject o) override;

  ReadOnlySerializer* read_only_serializer_;
  std::vector<AccessorInfo> accessor_infos_;
  std::vector<CallHandlerInfo> call_handler_infos_;

  DISALLOW_COPY_AND_ASSIGN(StartupSerializer);
};

class SerializedHandleChecker : public RootVisitor {
 public:
  SerializedHandleChecker(Isolate* isolate, std::vector<Context>* contexts);
  void VisitRootPointers(Root root, const char* description,
                         FullObjectSlot start, FullObjectSlot end) override;
  bool CheckGlobalAndEternalHandles();

 private:
  void AddToSet(FixedArray serialized);

  Isolate* isolate_;
  std::unordered_set<Object, Object::Hasher> serialized_;
  bool ok_ = true;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_STARTUP_SERIALIZER_H_
