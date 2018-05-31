// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_BUILTIN_DESERIALIZER_ALLOCATOR_H_
#define V8_SNAPSHOT_BUILTIN_DESERIALIZER_ALLOCATOR_H_

#include <unordered_set>

#include "src/globals.h"
#include "src/heap/heap.h"
#include "src/interpreter/interpreter.h"
#include "src/snapshot/serializer-common.h"

namespace v8 {
namespace internal {

template <class AllocatorT>
class Deserializer;

class BuiltinDeserializer;
class BuiltinSnapshotUtils;

class BuiltinDeserializerAllocator final {
  using BSU = BuiltinSnapshotUtils;
  using Bytecode = interpreter::Bytecode;
  using OperandScale = interpreter::OperandScale;

 public:
  BuiltinDeserializerAllocator(
      Deserializer<BuiltinDeserializerAllocator>* deserializer);

  ~BuiltinDeserializerAllocator();

  // ------- Allocation Methods -------
  // Methods related to memory allocation during deserialization.

  // Allocation works differently here than in other deserializers. Instead of
  // a statically-known memory area determined at serialization-time, our
  // memory requirements here are determined at runtime. Another major
  // difference is that we create builtin Code objects up-front (before
  // deserialization) in order to avoid having to patch builtin references
  // later on. See also the kBuiltin case in deserializer.cc.
  //
  // There are three ways that we use to reserve / allocate space. In all
  // cases, required objects are requested from the GC prior to
  // deserialization. 1. pre-allocated builtin code objects are written into
  // the builtins table (this is to make deserialization of builtin references
  // easier). Pre-allocated handler code objects are 2. stored in the
  // {handler_allocations_} vector (at eager-deserialization time) and 3.
  // stored in {handler_allocation_} (at lazy-deserialization time).
  //
  // Allocate simply returns the pre-allocated object prepared by
  // InitializeFromReservations.
  Address Allocate(AllocationSpace space, int size);

  void MoveToNextChunk(AllocationSpace space) { UNREACHABLE(); }
  void SetAlignment(AllocationAlignment alignment) { UNREACHABLE(); }

  void set_next_reference_is_weak(bool next_reference_is_weak) {
    next_reference_is_weak_ = next_reference_is_weak;
  }

  bool GetAndClearNextReferenceIsWeak() {
    bool saved = next_reference_is_weak_;
    next_reference_is_weak_ = false;
    return saved;
  }

#ifdef DEBUG
  bool next_reference_is_weak() const { return next_reference_is_weak_; }
#endif

  HeapObject* GetMap(uint32_t index) { UNREACHABLE(); }
  HeapObject* GetLargeObject(uint32_t index) { UNREACHABLE(); }
  HeapObject* GetObject(AllocationSpace space, uint32_t chunk_index,
                        uint32_t chunk_offset) {
    UNREACHABLE();
  }

  // ------- Reservation Methods -------
  // Methods related to memory reservations (prior to deserialization).

  // Builtin deserialization does not bake reservations into the snapshot, hence
  // this is a nop.
  void DecodeReservation(std::vector<SerializedData::Reservation> res) {}

  // These methods are used to pre-allocate builtin objects prior to
  // deserialization.
  // TODO(jgruber): Refactor reservation/allocation logic in deserializers to
  // make this less messy.
  Heap::Reservation CreateReservationsForEagerBuiltinsAndHandlers();
  void InitializeFromReservations(const Heap::Reservation& reservation);

  // Creates reservations and initializes the builtins table in preparation for
  // lazily deserializing a single builtin.
  void ReserveAndInitializeBuiltinsTableForBuiltin(int builtin_id);

  // Pre-allocates a code object preparation for lazily deserializing a single
  // handler.
  void ReserveForHandler(Bytecode bytecode, OperandScale operand_scale);

#ifdef DEBUG
  bool ReservationsAreFullyUsed() const;
#endif

 private:
  Isolate* isolate() const;
  BuiltinDeserializer* deserializer() const;

  // Used after memory allocation prior to isolate initialization, to register
  // the newly created object in code space and add it to the builtins table.
  void InitializeBuiltinFromReservation(const Heap::Chunk& chunk,
                                        int builtin_id);

  // As above, but for interpreter bytecode handlers.
  void InitializeHandlerFromReservation(
      const Heap::Chunk& chunk, interpreter::Bytecode bytecode,
      interpreter::OperandScale operand_scale);

#ifdef DEBUG
  void RegisterCodeObjectReservation(int code_object_id);
  void RegisterCodeObjectAllocation(int code_object_id);
  std::unordered_set<int> unused_reservations_;
#endif

 private:
  // The current deserializer. Note that this always points to a
  // BuiltinDeserializer instance, but we can't perform the cast during
  // construction since that makes vtable-based checks fail.
  Deserializer<BuiltinDeserializerAllocator>* const deserializer_;

  // Stores allocated space for bytecode handlers during eager deserialization.
  std::vector<Address>* handler_allocations_ = nullptr;

  // Stores the allocated space for a single handler during lazy
  // deserialization.
  Address handler_allocation_ = nullptr;

  bool next_reference_is_weak_ = false;

  DISALLOW_COPY_AND_ASSIGN(BuiltinDeserializerAllocator)
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_BUILTIN_DESERIALIZER_ALLOCATOR_H_
