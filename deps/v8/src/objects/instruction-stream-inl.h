// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_INSTRUCTION_STREAM_INL_H_
#define V8_OBJECTS_INSTRUCTION_STREAM_INL_H_

#include <optional>

#include "src/common/ptr-compr-inl.h"
#include "src/heap/heap-layout-inl.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/code.h"
#include "src/objects/instruction-stream.h"
#include "src/objects/objects-inl.h"  // For HeapObject::IsInstructionStream.

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8::internal {

OBJECT_CONSTRUCTORS_IMPL(InstructionStream, TrustedObject)
NEVER_READ_ONLY_SPACE_IMPL(InstructionStream)

uint32_t InstructionStream::body_size() const {
  return ReadField<uint32_t>(kBodySizeOffset);
}

// TODO(sroettger): remove unused setter functions once all code writes go
// through the WritableJitAllocation, e.g. the body_size setter above.

#if V8_EMBEDDED_CONSTANT_POOL_BOOL
Address InstructionStream::constant_pool() const {
  return address() + ReadField<int>(kConstantPoolOffsetOffset);
}
#else
Address InstructionStream::constant_pool() const { return kNullAddress; }
#endif

// static
Tagged<InstructionStream> InstructionStream::Initialize(
    Tagged<HeapObject> self, Tagged<Map> map, uint32_t body_size,
    int constant_pool_offset, Tagged<TrustedByteArray> reloc_info) {
  {
    WritableJitAllocation writable_allocation =
        ThreadIsolation::RegisterInstructionStreamAllocation(
            self.address(), InstructionStream::SizeFor(body_size));
    CHECK_EQ(InstructionStream::SizeFor(body_size), writable_allocation.size());

    writable_allocation.WriteHeaderSlot<Map, kMapOffset>(map, kRelaxedStore);

    writable_allocation.WriteHeaderSlot<uint32_t, kBodySizeOffset>(body_size);

    if constexpr (V8_EMBEDDED_CONSTANT_POOL_BOOL) {
      writable_allocation.WriteHeaderSlot<int, kConstantPoolOffsetOffset>(
          kHeaderSize + constant_pool_offset);
    }

    // During the Code initialization process, InstructionStream::code is
    // briefly unset (the Code object has not been allocated yet). In this state
    // it is only visible through heap iteration.
    writable_allocation.WriteHeaderSlot<Smi, kCodeOffset>(Smi::zero(),
                                                          kReleaseStore);

    DCHECK(!HeapLayout::InYoungGeneration(reloc_info));
    writable_allocation.WriteProtectedPointerHeaderSlot<TrustedByteArray,
                                                        kRelocationInfoOffset>(
        reloc_info, kRelaxedStore);

    // Clear header padding
    writable_allocation.ClearBytes(kUnalignedSize,
                                   kHeaderSize - kUnalignedSize);
    // Clear trailing padding.
    writable_allocation.ClearBytes(kHeaderSize + body_size,
                                   TrailingPaddingSizeFor(body_size));
  }

  Tagged<InstructionStream> istream = Cast<InstructionStream>(self);

  // We want to keep the code minimal that runs with write access to a JIT
  // allocation, so trigger the write barriers after the WritableJitAllocation
  // went out of scope.
  SLOW_DCHECK(!WriteBarrier::IsRequired(istream, map));
  CONDITIONAL_PROTECTED_POINTER_WRITE_BARRIER(*istream, kRelocationInfoOffset,
                                              reloc_info, UPDATE_WRITE_BARRIER);

  return istream;
}

// Copy from compilation artifacts stored in CodeDesc to the target on-heap
// objects.
//
// Note this is quite convoluted for historical reasons. The CodeDesc buffer
// contains instructions, a part of inline metadata, and the relocation info.
// Additionally, the unwinding_info is stored in a separate buffer
// `desc.unwinding_info`. In this method, we copy all these parts into the
// final on-heap representation.
//
// The off-heap representation:
//
// CodeDesc.buffer:
//
// +-------------------
// | instructions
// +-------------------
// | inline metadata
// | .. safepoint table
// | .. handler table
// | .. constant pool
// | .. code comments
// +-------------------
// | reloc info
// +-------------------
//
// CodeDesc.unwinding_info:  .. the unwinding info.
//
// This is transformed into the on-heap representation, where
// InstructionStream contains all instructions and inline metadata, and a
// pointer to the relocation info byte array.
void InstructionStream::Finalize(Tagged<Code> code,
                                 Tagged<TrustedByteArray> reloc_info,
                                 CodeDesc desc, Heap* heap) {
  DisallowGarbageCollection no_gc;
  std::optional<WriteBarrierPromise> promise;

  // Copy the relocation info first before we unlock the Jit allocation.
  // TODO(sroettger): reloc info should live in protected memory.
  DCHECK_EQ(reloc_info->length(), desc.reloc_size);
  CopyBytes(reloc_info->begin(), desc.buffer + desc.reloc_offset,
            static_cast<size_t>(desc.reloc_size));

  {
    WritableJitAllocation writable_allocation =
        ThreadIsolation::LookupJitAllocation(
            address(), InstructionStream::SizeFor(body_size()),
            ThreadIsolation::JitAllocationType::kInstructionStream, true);

    // Copy code and inline metadata.
    static_assert(InstructionStream::kOnHeapBodyIsContiguous);
    writable_allocation.CopyCode(kHeaderSize, desc.buffer,
                                 static_cast<size_t>(desc.instr_size));
    writable_allocation.CopyData(kHeaderSize + desc.instr_size,
                                 desc.unwinding_info,
                                 static_cast<size_t>(desc.unwinding_info_size));
    DCHECK_EQ(desc.body_size(), desc.instr_size + desc.unwinding_info_size);
    DCHECK_EQ(code->body_size(),
              code->instruction_size() + code->metadata_size());

    promise.emplace(RelocateFromDesc(writable_allocation, heap, desc,
                                     code->constant_pool(), no_gc));

    // Publish the code pointer after the istream has been fully initialized.
    writable_allocation.WriteProtectedPointerHeaderSlot<Code, kCodeOffset>(
        code, kReleaseStore);
  }

  // Trigger the write barriers after we dropped the JIT write permissions.
  RelocateFromDescWriteBarriers(heap, desc, code->constant_pool(), *promise,
                                no_gc);
  CONDITIONAL_PROTECTED_POINTER_WRITE_BARRIER(*this, kCodeOffset, code,
                                              UPDATE_WRITE_BARRIER);

  code->FlushICache();
}

bool InstructionStream::IsFullyInitialized() {
  return raw_code(kAcquireLoad) != Smi::zero();
}

Address InstructionStream::body_end() const {
  static_assert(kOnHeapBodyIsContiguous);
  return instruction_start() + body_size();
}

Tagged<Object> InstructionStream::raw_code(AcquireLoadTag tag) const {
  Tagged<Object> value = RawProtectedPointerField(kCodeOffset).Acquire_Load();
  DCHECK(!HeapLayout::InYoungGeneration(value));
  DCHECK(IsSmi(value) || HeapLayout::InTrustedSpace(Cast<HeapObject>(value)));
  return value;
}

Tagged<Code> InstructionStream::code(AcquireLoadTag tag) const {
  return Cast<Code>(raw_code(tag));
}

bool InstructionStream::TryGetCode(Tagged<Code>* code_out,
                                   AcquireLoadTag tag) const {
  Tagged<Object> maybe_code = raw_code(tag);
  if (maybe_code == Smi::zero()) return false;
  *code_out = Cast<Code>(maybe_code);
  return true;
}

bool InstructionStream::TryGetCodeUnchecked(Tagged<Code>* code_out,
                                            AcquireLoadTag tag) const {
  Tagged<Object> maybe_code = raw_code(tag);
  if (maybe_code == Smi::zero()) return false;
  *code_out = UncheckedCast<Code>(maybe_code);
  return true;
}

Tagged<TrustedByteArray> InstructionStream::relocation_info() const {
  return Cast<TrustedByteArray>(
      ReadProtectedPointerField(kRelocationInfoOffset));
}

Address InstructionStream::instruction_start() const {
  return field_address(kHeaderSize);
}

Tagged<TrustedByteArray> InstructionStream::unchecked_relocation_info() const {
  Tagged<Object> value =
      RawProtectedPointerField(kRelocationInfoOffset).Acquire_Load();
  return UncheckedCast<TrustedByteArray>(value);
}

uint8_t* InstructionStream::relocation_start() const {
  return relocation_info()->begin();
}

uint8_t* InstructionStream::relocation_end() const {
  return relocation_info()->end();
}

int InstructionStream::relocation_size() const {
  return relocation_info()->length();
}

int InstructionStream::Size() const { return SizeFor(body_size()); }

// static
Tagged<InstructionStream> InstructionStream::FromTargetAddress(
    Address address) {
  {
    // TODO(jgruber,v8:6666): Support embedded builtins here. We'd need to pass
    // in the current isolate.
    Address start =
        reinterpret_cast<Address>(Isolate::CurrentEmbeddedBlobCode());
    Address end = start + Isolate::CurrentEmbeddedBlobCodeSize();
    CHECK(address < start || address >= end);
  }

  Tagged<HeapObject> code =
      HeapObject::FromAddress(address - InstructionStream::kHeaderSize);
  // Unchecked cast because we can't rely on the map currently not being a
  // forwarding pointer.
  return UncheckedCast<InstructionStream>(code);
}

// static
Tagged<InstructionStream> InstructionStream::FromEntryAddress(
    Address location_of_address) {
  Address code_entry = base::Memory<Address>(location_of_address);
  Tagged<HeapObject> code =
      HeapObject::FromAddress(code_entry - InstructionStream::kHeaderSize);
  // Unchecked cast because we can't rely on the map currently not being a
  // forwarding pointer.
  return UncheckedCast<InstructionStream>(code);
}

// static
PtrComprCageBase InstructionStream::main_cage_base() {
#ifdef V8_COMPRESS_POINTERS
  return PtrComprCageBase{V8HeapCompressionScheme::base()};
#else
  return PtrComprCageBase{};
#endif
}

}  // namespace v8::internal

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_INSTRUCTION_STREAM_INL_H_
