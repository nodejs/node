// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_BUILTIN_DESERIALIZER_H_
#define V8_SNAPSHOT_BUILTIN_DESERIALIZER_H_

#include "src/heap/heap.h"
#include "src/snapshot/deserializer.h"

namespace v8 {
namespace internal {

class BuiltinSnapshotData;

// Deserializes the builtins blob.
class BuiltinDeserializer final : public Deserializer {
 public:
  BuiltinDeserializer(Isolate* isolate, const BuiltinSnapshotData* data);

  // Builtins deserialization is tightly integrated with deserialization of the
  // startup blob. In particular, we need to ensure that no GC can occur
  // between startup- and builtins deserialization, as all builtins have been
  // pre-allocated and their pointers may not be invalidated.
  //
  // After this, the instruction cache must be flushed by the caller (we don't
  // do it ourselves since the startup serializer batch-flushes all code pages).
  void DeserializeEagerBuiltins();

  // Deserializes the single given builtin. Assumes that reservations have
  // already been allocated.
  Code* DeserializeBuiltin(int builtin_id);

  // These methods are used to pre-allocate builtin objects prior to
  // deserialization.
  // TODO(jgruber): Refactor reservation/allocation logic in deserializers to
  // make this less messy.
  Heap::Reservation CreateReservationsForEagerBuiltins();
  void InitializeBuiltinsTable(const Heap::Reservation& reservation);

  // Creates reservations and initializes the builtins table in preparation for
  // lazily deserializing a single builtin.
  void ReserveAndInitializeBuiltinsTableForBuiltin(int builtin_id);

 private:
  // TODO(jgruber): Remove once allocations have been refactored.
  void SetPositionToBuiltin(int builtin_id);

  // Extracts the size builtin Code objects (baked into the snapshot).
  uint32_t ExtractBuiltinSize(int builtin_id);

  // Used after memory allocation prior to isolate initialization, to register
  // the newly created object in code space and add it to the builtins table.
  void InitializeBuiltinFromReservation(const Heap::Chunk& chunk,
                                        int builtin_id);

  // Allocation works differently here than in other deserializers. Instead of
  // a statically-known memory area determined at serialization-time, our
  // memory requirements here are determined at runtime. Another major
  // difference is that we create builtin Code objects up-front (before
  // deserialization) in order to avoid having to patch builtin references
  // later on. See also the kBuiltin case in deserializer.cc.
  //
  // Allocate simply returns the pre-allocated object prepared by
  // InitializeBuiltinsTable.
  Address Allocate(int space_index, int size) override;

  // BuiltinDeserializer implements its own builtin iteration logic. Make sure
  // the RootVisitor API is not used accidentally.
  void VisitRootPointers(Root root, Object** start, Object** end) override {
    UNREACHABLE();
  }

  // Stores the builtin currently being deserialized. We need this to determine
  // where to 'allocate' from during deserialization.
  static const int kNoBuiltinId = -1;
  int current_builtin_id_ = kNoBuiltinId;

  // The offsets of each builtin within the serialized data. Equivalent to
  // BuiltinSerializer::builtin_offsets_ but on the deserialization side.
  Vector<const uint32_t> builtin_offsets_;

  friend class DeserializingBuiltinScope;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_BUILTIN_DESERIALIZER_H_
