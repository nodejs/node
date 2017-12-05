// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/builtin-deserializer.h"

#include "src/assembler-inl.h"
#include "src/objects-inl.h"
#include "src/snapshot/snapshot.h"

namespace v8 {
namespace internal {

// Tracks the builtin currently being deserialized (required for allocation).
class DeserializingBuiltinScope {
 public:
  DeserializingBuiltinScope(BuiltinDeserializer* builtin_deserializer,
                            int builtin_id)
      : builtin_deserializer_(builtin_deserializer) {
    DCHECK_EQ(BuiltinDeserializer::kNoBuiltinId,
              builtin_deserializer->current_builtin_id_);
    builtin_deserializer->current_builtin_id_ = builtin_id;
  }

  ~DeserializingBuiltinScope() {
    builtin_deserializer_->current_builtin_id_ =
        BuiltinDeserializer::kNoBuiltinId;
  }

 private:
  BuiltinDeserializer* builtin_deserializer_;

  DISALLOW_COPY_AND_ASSIGN(DeserializingBuiltinScope)
};

BuiltinDeserializer::BuiltinDeserializer(Isolate* isolate,
                                         const BuiltinSnapshotData* data)
    : Deserializer(data, false) {
  // We may have to relax this at some point to pack reloc infos and handler
  // tables into the builtin blob (instead of the partial snapshot cache).
  DCHECK(ReservesOnlyCodeSpace());

  builtin_offsets_ = data->BuiltinOffsets();
  DCHECK_EQ(Builtins::builtin_count, builtin_offsets_.length());
  DCHECK(std::is_sorted(builtin_offsets_.begin(), builtin_offsets_.end()));

  Initialize(isolate);
}

void BuiltinDeserializer::DeserializeEagerBuiltins() {
  DCHECK(!AllowHeapAllocation::IsAllowed());
  DCHECK_EQ(0, source()->position());

  Builtins* builtins = isolate()->builtins();
  for (int i = 0; i < Builtins::builtin_count; i++) {
    if (IsLazyDeserializationEnabled() && Builtins::IsLazy(i)) {
      // Do nothing. These builtins have been replaced by DeserializeLazy in
      // InitializeBuiltinsTable.
      DCHECK_EQ(builtins->builtin(Builtins::kDeserializeLazy),
                builtins->builtin(i));
    } else {
      builtins->set_builtin(i, DeserializeBuiltin(i));
    }
  }

#ifdef DEBUG
  for (int i = 0; i < Builtins::builtin_count; i++) {
    Object* o = builtins->builtin(i);
    DCHECK(o->IsCode() && Code::cast(o)->is_builtin());
  }
#endif
}

Code* BuiltinDeserializer::DeserializeBuiltin(int builtin_id) {
  DCHECK(!AllowHeapAllocation::IsAllowed());
  DCHECK(Builtins::IsBuiltinId(builtin_id));

  DeserializingBuiltinScope scope(this, builtin_id);

  const int initial_position = source()->position();
  SetPositionToBuiltin(builtin_id);

  Object* o = ReadDataSingle();
  DCHECK(o->IsCode() && Code::cast(o)->is_builtin());

  // Rewind.
  source()->set_position(initial_position);

  // Flush the instruction cache.
  Code* code = Code::cast(o);
  Assembler::FlushICache(isolate(), code->instruction_start(),
                         code->instruction_size());

  return code;
}

void BuiltinDeserializer::SetPositionToBuiltin(int builtin_id) {
  DCHECK(Builtins::IsBuiltinId(builtin_id));

  const uint32_t offset = builtin_offsets_[builtin_id];
  source()->set_position(offset);

  // Grab the size of the code object.
  byte data = source()->Get();

  // The first bytecode can either be kNewObject, or kNextChunk if the current
  // chunk has been exhausted. Since we do allocations differently here, we
  // don't care about kNextChunk and can simply skip over it.
  // TODO(jgruber): When refactoring (de)serializer allocations, ensure we don't
  // generate kNextChunk bytecodes anymore for the builtins snapshot. In fact,
  // the entire reservations mechanism is unused for the builtins snapshot.
  if (data == kNextChunk) {
    source()->Get();  // Skip over kNextChunk's {space} parameter.
  } else {
    source()->set_position(offset);  // Rewind.
  }
}

uint32_t BuiltinDeserializer::ExtractBuiltinSize(int builtin_id) {
  DCHECK(Builtins::IsBuiltinId(builtin_id));

  const int initial_position = source()->position();

  // Grab the size of the code object.
  SetPositionToBuiltin(builtin_id);
  byte data = source()->Get();

  USE(data);
  DCHECK_EQ(kNewObject | kPlain | kStartOfObject | CODE_SPACE, data);
  const uint32_t result = source()->GetInt() << kObjectAlignmentBits;

  // Rewind.
  source()->set_position(initial_position);

  return result;
}

Heap::Reservation BuiltinDeserializer::CreateReservationsForEagerBuiltins() {
  DCHECK(ReservesOnlyCodeSpace());

  Heap::Reservation result;

  // DeserializeLazy is always the first reservation (to simplify logic in
  // InitializeBuiltinsTable).
  {
    DCHECK(!Builtins::IsLazy(Builtins::kDeserializeLazy));
    uint32_t builtin_size = ExtractBuiltinSize(Builtins::kDeserializeLazy);
    DCHECK_LE(builtin_size, MemoryAllocator::PageAreaSize(CODE_SPACE));
    result.push_back({builtin_size, nullptr, nullptr});
  }

  for (int i = 0; i < Builtins::builtin_count; i++) {
    if (i == Builtins::kDeserializeLazy) continue;

    // Skip lazy builtins. These will be replaced by the DeserializeLazy code
    // object in InitializeBuiltinsTable and thus require no reserved space.
    if (IsLazyDeserializationEnabled() && Builtins::IsLazy(i)) continue;

    uint32_t builtin_size = ExtractBuiltinSize(i);
    DCHECK_LE(builtin_size, MemoryAllocator::PageAreaSize(CODE_SPACE));
    result.push_back({builtin_size, nullptr, nullptr});
  }

  return result;
}

void BuiltinDeserializer::InitializeBuiltinFromReservation(
    const Heap::Chunk& chunk, int builtin_id) {
  DCHECK_EQ(ExtractBuiltinSize(builtin_id), chunk.size);
  DCHECK_EQ(chunk.size, chunk.end - chunk.start);

  SkipList::Update(chunk.start, chunk.size);
  isolate()->builtins()->set_builtin(builtin_id,
                                     HeapObject::FromAddress(chunk.start));
}

void BuiltinDeserializer::InitializeBuiltinsTable(
    const Heap::Reservation& reservation) {
  DCHECK(!AllowHeapAllocation::IsAllowed());

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

  for (int i = 0; i < Builtins::builtin_count; i++) {
    if (i == Builtins::kDeserializeLazy) continue;

    if (IsLazyDeserializationEnabled() && Builtins::IsLazy(i)) {
      builtins->set_builtin(i, deserialize_lazy);
    } else {
      InitializeBuiltinFromReservation(reservation[reservation_index], i);
      reservation_index++;
    }
  }

  DCHECK_EQ(reservation.size(), reservation_index);
}

void BuiltinDeserializer::ReserveAndInitializeBuiltinsTableForBuiltin(
    int builtin_id) {
  DCHECK(AllowHeapAllocation::IsAllowed());
  DCHECK(isolate()->builtins()->is_initialized());
  DCHECK(Builtins::IsBuiltinId(builtin_id));
  DCHECK_NE(Builtins::kDeserializeLazy, builtin_id);
  DCHECK_EQ(Builtins::kDeserializeLazy,
            isolate()->builtins()->builtin(builtin_id)->builtin_index());

  const uint32_t builtin_size = ExtractBuiltinSize(builtin_id);
  DCHECK_LE(builtin_size, MemoryAllocator::PageAreaSize(CODE_SPACE));

  Handle<HeapObject> o =
      isolate()->factory()->NewCodeForDeserialization(builtin_size);

  // Note: After this point and until deserialization finishes, heap allocation
  // is disallowed. We currently can't safely assert this since we'd need to
  // pass the DisallowHeapAllocation scope out of this function.

  // Write the allocated filler object into the builtins table. It will be
  // returned by our custom Allocate method below once needed.

  isolate()->builtins()->set_builtin(builtin_id, *o);
}

Address BuiltinDeserializer::Allocate(int space_index, int size) {
  DCHECK_EQ(CODE_SPACE, space_index);
  DCHECK_EQ(ExtractBuiltinSize(current_builtin_id_), size);
  Object* obj = isolate()->builtins()->builtin(current_builtin_id_);
  DCHECK(Internals::HasHeapObjectTag(obj));
  return HeapObject::cast(obj)->address();
}

}  // namespace internal
}  // namespace v8
