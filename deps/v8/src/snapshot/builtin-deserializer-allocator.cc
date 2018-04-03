// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/builtin-deserializer-allocator.h"

#include "src/heap/heap-inl.h"
#include "src/interpreter/interpreter.h"
#include "src/snapshot/builtin-deserializer.h"
#include "src/snapshot/deserializer.h"

namespace v8 {
namespace internal {

using interpreter::Bytecodes;
using interpreter::Interpreter;

BuiltinDeserializerAllocator::BuiltinDeserializerAllocator(
    Deserializer<BuiltinDeserializerAllocator>* deserializer)
    : deserializer_(deserializer) {}

BuiltinDeserializerAllocator::~BuiltinDeserializerAllocator() {
  delete handler_allocations_;
}

namespace {
int HandlerAllocationIndex(int code_object_id) {
  return code_object_id - BuiltinSnapshotUtils::kFirstHandlerIndex;
}
}  // namespace

Address BuiltinDeserializerAllocator::Allocate(AllocationSpace space,
                                               int size) {
  const int code_object_id = deserializer()->CurrentCodeObjectId();
  DCHECK_NE(BuiltinDeserializer::kNoCodeObjectId, code_object_id);
  DCHECK_EQ(CODE_SPACE, space);
  DCHECK_EQ(deserializer()->ExtractCodeObjectSize(code_object_id), size);
#ifdef DEBUG
  RegisterCodeObjectAllocation(code_object_id);
#endif

  if (BSU::IsBuiltinIndex(code_object_id)) {
    Object* obj = isolate()->builtins()->builtin(code_object_id);
    DCHECK(Internals::HasHeapObjectTag(obj));
    return HeapObject::cast(obj)->address();
  } else if (BSU::IsHandlerIndex(code_object_id)) {
    if (handler_allocation_ != nullptr) {
      // Lazy deserialization.
      DCHECK_NULL(handler_allocations_);
      return handler_allocation_;
    } else {
      // Eager deserialization.
      DCHECK_NULL(handler_allocation_);
      DCHECK_NOT_NULL(handler_allocations_);
      int index = HandlerAllocationIndex(code_object_id);
      DCHECK_NOT_NULL(handler_allocations_->at(index));
      return handler_allocations_->at(index);
    }
  }

  UNREACHABLE();
}

Heap::Reservation
BuiltinDeserializerAllocator::CreateReservationsForEagerBuiltinsAndHandlers() {
  Heap::Reservation result;

  // Reservations for builtins.

  // DeserializeLazy is always the first builtin reservation (to simplify logic
  // in InitializeBuiltinsTable).
  {
    DCHECK(!Builtins::IsLazy(Builtins::kDeserializeLazy));
    uint32_t builtin_size =
        deserializer()->ExtractCodeObjectSize(Builtins::kDeserializeLazy);
    DCHECK_LE(builtin_size, MemoryAllocator::PageAreaSize(CODE_SPACE));
    result.push_back({builtin_size, nullptr, nullptr});
  }

  for (int i = 0; i < BSU::kNumberOfBuiltins; i++) {
    if (i == Builtins::kDeserializeLazy) continue;

    // Skip lazy builtins. These will be replaced by the DeserializeLazy code
    // object in InitializeFromReservations and thus require no reserved space.
    if (deserializer()->IsLazyDeserializationEnabled() && Builtins::IsLazy(i)) {
      continue;
    }

    uint32_t builtin_size = deserializer()->ExtractCodeObjectSize(i);
    DCHECK_LE(builtin_size, MemoryAllocator::PageAreaSize(CODE_SPACE));
    result.push_back({builtin_size, nullptr, nullptr});
  }

  // Reservations for bytecode handlers.

  BSU::ForEachBytecode(
      [=, &result](Bytecode bytecode, OperandScale operand_scale) {
        if (!Bytecodes::BytecodeHasHandler(bytecode, operand_scale)) {
          // Bytecodes without a handler don't require a reservation.
          return;
        } else if (FLAG_lazy_handler_deserialization &&
                   deserializer()->IsLazyDeserializationEnabled() &&
                   Bytecodes::IsLazy(bytecode)) {
          // Skip lazy handlers. These will be replaced by the DeserializeLazy
          // code object in InitializeFromReservations and thus require no
          // reserved space.
          return;
        }

        const int index = BSU::BytecodeToIndex(bytecode, operand_scale);
        uint32_t handler_size = deserializer()->ExtractCodeObjectSize(index);
        DCHECK_LE(handler_size, MemoryAllocator::PageAreaSize(CODE_SPACE));
        result.push_back({handler_size, nullptr, nullptr});
      });

  return result;
}

void BuiltinDeserializerAllocator::InitializeBuiltinFromReservation(
    const Heap::Chunk& chunk, int builtin_id) {
  DCHECK_EQ(deserializer()->ExtractCodeObjectSize(builtin_id), chunk.size);
  DCHECK_EQ(chunk.size, chunk.end - chunk.start);

  SkipList::Update(chunk.start, chunk.size);
  isolate()->builtins()->set_builtin(builtin_id,
                                     HeapObject::FromAddress(chunk.start));

#ifdef DEBUG
  RegisterCodeObjectReservation(builtin_id);
#endif
}

void BuiltinDeserializerAllocator::InitializeHandlerFromReservation(
    const Heap::Chunk& chunk, interpreter::Bytecode bytecode,
    interpreter::OperandScale operand_scale) {
  DCHECK_EQ(deserializer()->ExtractCodeObjectSize(
                BSU::BytecodeToIndex(bytecode, operand_scale)),
            chunk.size);
  DCHECK_EQ(chunk.size, chunk.end - chunk.start);

  SkipList::Update(chunk.start, chunk.size);

  DCHECK_NOT_NULL(handler_allocations_);
  const int index =
      HandlerAllocationIndex(BSU::BytecodeToIndex(bytecode, operand_scale));
  handler_allocations_->at(index) = chunk.start;

#ifdef DEBUG
  RegisterCodeObjectReservation(BSU::BytecodeToIndex(bytecode, operand_scale));
#endif
}

void BuiltinDeserializerAllocator::InitializeFromReservations(
    const Heap::Reservation& reservation) {
  DCHECK(!AllowHeapAllocation::IsAllowed());

  // Initialize the builtins table.

  Builtins* builtins = isolate()->builtins();
  int reservation_index = 0;

  // Other builtins can be replaced by DeserializeLazy so it may not be lazy.
  // It always occupies the first reservation slot.
  {
    DCHECK(!Builtins::IsLazy(Builtins::kDeserializeLazy));
    InitializeBuiltinFromReservation(reservation[reservation_index],
                                     Builtins::kDeserializeLazy);
    reservation_index++;
  }

  Code* deserialize_lazy = builtins->builtin(Builtins::kDeserializeLazy);

  for (int i = 0; i < BSU::kNumberOfBuiltins; i++) {
    if (i == Builtins::kDeserializeLazy) continue;

    if (deserializer()->IsLazyDeserializationEnabled() && Builtins::IsLazy(i)) {
      builtins->set_builtin(i, deserialize_lazy);
    } else {
      InitializeBuiltinFromReservation(reservation[reservation_index], i);
      reservation_index++;
    }
  }

  // Initialize interpreter bytecode handler reservations.

  DCHECK_NULL(handler_allocations_);
  handler_allocations_ = new std::vector<Address>(BSU::kNumberOfHandlers);

  BSU::ForEachBytecode(
      [=, &reservation_index](Bytecode bytecode, OperandScale operand_scale) {
        if (!Bytecodes::BytecodeHasHandler(bytecode, operand_scale)) {
          // Bytecodes without a handler don't have a reservation.
          return;
        } else if (FLAG_lazy_handler_deserialization &&
                   deserializer()->IsLazyDeserializationEnabled() &&
                   Bytecodes::IsLazy(bytecode)) {
          // Likewise, bytecodes with lazy handlers don't either.
          return;
        }

        InitializeHandlerFromReservation(reservation[reservation_index],
                                         bytecode, operand_scale);
        reservation_index++;
      });

  DCHECK_EQ(reservation.size(), reservation_index);
}

void BuiltinDeserializerAllocator::ReserveAndInitializeBuiltinsTableForBuiltin(
    int builtin_id) {
  DCHECK(AllowHeapAllocation::IsAllowed());
  DCHECK(isolate()->builtins()->is_initialized());
  DCHECK(Builtins::IsBuiltinId(builtin_id));
  DCHECK_NE(Builtins::kDeserializeLazy, builtin_id);
  DCHECK_EQ(Builtins::kDeserializeLazy,
            isolate()->builtins()->builtin(builtin_id)->builtin_index());

  const uint32_t builtin_size =
      deserializer()->ExtractCodeObjectSize(builtin_id);
  DCHECK_LE(builtin_size, MemoryAllocator::PageAreaSize(CODE_SPACE));

  Handle<HeapObject> o =
      isolate()->factory()->NewCodeForDeserialization(builtin_size);

  // Note: After this point and until deserialization finishes, heap allocation
  // is disallowed. We currently can't safely assert this since we'd need to
  // pass the DisallowHeapAllocation scope out of this function.

  // Write the allocated filler object into the builtins table. It will be
  // returned by our custom Allocate method below once needed.

  isolate()->builtins()->set_builtin(builtin_id, *o);

#ifdef DEBUG
  RegisterCodeObjectReservation(builtin_id);
#endif
}

void BuiltinDeserializerAllocator::ReserveForHandler(
    Bytecode bytecode, OperandScale operand_scale) {
  DCHECK(AllowHeapAllocation::IsAllowed());
  DCHECK(isolate()->interpreter()->IsDispatchTableInitialized());

  const int code_object_id = BSU::BytecodeToIndex(bytecode, operand_scale);
  const uint32_t handler_size =
      deserializer()->ExtractCodeObjectSize(code_object_id);
  DCHECK_LE(handler_size, MemoryAllocator::PageAreaSize(CODE_SPACE));

  handler_allocation_ =
      isolate()->factory()->NewCodeForDeserialization(handler_size)->address();

// Note: After this point and until deserialization finishes, heap allocation
// is disallowed. We currently can't safely assert this since we'd need to
// pass the DisallowHeapAllocation scope out of this function.

#ifdef DEBUG
  RegisterCodeObjectReservation(code_object_id);
#endif
}

#ifdef DEBUG
void BuiltinDeserializerAllocator::RegisterCodeObjectReservation(
    int code_object_id) {
  const auto result = unused_reservations_.emplace(code_object_id);
  CHECK(result.second);  // False, iff builtin_id was already present in set.
}

void BuiltinDeserializerAllocator::RegisterCodeObjectAllocation(
    int code_object_id) {
  const size_t removed_elems = unused_reservations_.erase(code_object_id);
  CHECK_EQ(removed_elems, 1);
}

bool BuiltinDeserializerAllocator::ReservationsAreFullyUsed() const {
  // Not 100% precise but should be good enough.
  return unused_reservations_.empty();
}
#endif  // DEBUG

Isolate* BuiltinDeserializerAllocator::isolate() const {
  return deserializer()->isolate();
}

BuiltinDeserializer* BuiltinDeserializerAllocator::deserializer() const {
  return static_cast<BuiltinDeserializer*>(deserializer_);
}

}  // namespace internal
}  // namespace v8
