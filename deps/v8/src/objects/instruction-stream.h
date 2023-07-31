// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_INSTRUCTION_STREAM_H_
#define V8_OBJECTS_INSTRUCTION_STREAM_H_

#include "src/objects/heap-object.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class Code;

// InstructionStream contains the instruction stream for V8-generated code
// objects.
//
// When V8_EXTERNAL_CODE_SPACE is enabled, InstructionStream objects are
// allocated in a separate pointer compression cage instead of the cage where
// all the other objects are allocated.
class InstructionStream : public HeapObject {
 public:
  NEVER_READ_ONLY_SPACE

  // All InstructionStream objects have the following layout:
  //
  //  +--------------------------+
  //  |          header          |
  //  +--------------------------+  <-- body_start()
  //  |       instructions       |   == instruction_start()
  //  |           ...            |
  //  | padded to meta alignment |      see kMetadataAlignment
  //  +--------------------------+  <-- instruction_end()
  //  |         metadata         |   == metadata_start() (MS)
  //  |           ...            |
  //  |                          |  <-- MS + handler_table_offset()
  //  |                          |  <-- MS + constant_pool_offset()
  //  |                          |  <-- MS + code_comments_offset()
  //  |                          |  <-- MS + unwinding_info_offset()
  //  | padded to obj alignment  |
  //  +--------------------------+  <-- metadata_end() == body_end()
  //  | padded to kCodeAlignmentMinusCodeHeader
  //  +--------------------------+
  //
  // In other words, the variable-size 'body' consists of 'instructions' and
  // 'metadata'.

  // Constants for use in static asserts, stating whether the body is adjacent,
  // i.e. instructions and metadata areas are adjacent.
  static constexpr bool kOnHeapBodyIsContiguous = true;
  static constexpr bool kOffHeapBodyIsContiguous = false;
  static constexpr bool kBodyIsContiguous =
      kOnHeapBodyIsContiguous && kOffHeapBodyIsContiguous;

  inline Address instruction_start() const;

  // The metadata section is aligned to this value.
  static constexpr int kMetadataAlignment = kIntSize;

  // [code]: The associated Code object.
  //
  // Set to Smi::zero() during initialization. Heap iterators may see
  // InstructionStream objects in this state.
  inline Code code(AcquireLoadTag tag) const;
  inline void set_code(Code value, ReleaseStoreTag tag);
  inline Object raw_code(AcquireLoadTag tag) const;
  // Use when the InstructionStream may be uninitialized:
  inline bool TryGetCode(Code* code_out, AcquireLoadTag tag) const;
  inline bool TryGetCodeUnchecked(Code* code_out, AcquireLoadTag tag) const;

  // [relocation_info]: InstructionStream relocation information.
  inline ByteArray relocation_info() const;
  inline void set_relocation_info(ByteArray value);
  // Unchecked accessor to be used during GC.
  inline ByteArray unchecked_relocation_info() const;

  inline uint8_t* relocation_start() const;
  inline uint8_t* relocation_end() const;
  inline int relocation_size() const;

  // The size of the entire body section, containing instructions and inlined
  // metadata.
  DECL_PRIMITIVE_ACCESSORS(body_size, int)
  inline Address body_end() const;

  inline void clear_padding();

  static constexpr int TrailingPaddingSizeFor(int body_size) {
    return RoundUp<kCodeAlignment>(kHeaderSize + body_size) - kHeaderSize -
           body_size;
  }
  static constexpr int SizeFor(int body_size) {
    return kHeaderSize + body_size + TrailingPaddingSizeFor(body_size);
  }
  inline int Size() const;

  static inline InstructionStream FromTargetAddress(Address address);
  static inline InstructionStream FromEntryAddress(Address location_of_address);

  // Relocate the code by delta bytes.
  void Relocate(intptr_t delta);

  DECL_CAST(InstructionStream)
  DECL_PRINTER(InstructionStream)
  DECL_VERIFIER(InstructionStream)

  // Layout description.
#define ISTREAM_FIELDS(V)                                             \
  V(kStartOfStrongFieldsOffset, 0)                                    \
  V(kCodeOffset, kTaggedSize)                                         \
  V(kRelocationInfoOffset, kTaggedSize)                               \
  V(kEndOfStrongFieldsOffset, 0)                                      \
  /* Data or code not directly visited by GC directly starts here. */ \
  V(kDataStart, 0)                                                    \
  V(kBodySizeOffset, kIntSize)                                        \
  V(kUnalignedSize, OBJECT_POINTER_PADDING(kUnalignedSize))           \
  V(kHeaderSize, 0)
  DEFINE_FIELD_OFFSET_CONSTANTS(HeapObject::kHeaderSize, ISTREAM_FIELDS)
#undef ISTREAM_FIELDS

  static_assert(kCodeAlignment >= kHeaderSize);
  // We do two things to ensure kCodeAlignment of the entry address:
  // 1) Add kCodeAlignmentMinusCodeHeader padding once in the beginning of every
  //    MemoryChunk.
  // 2) Round up all IStream allocations to a multiple of kCodeAlignment, see
  //    TrailingPaddingSizeFor.
  // Together, the IStream object itself will always start at offset
  // kCodeAlignmentMinusCodeHeader, which aligns the entry to kCodeAlignment.
  static constexpr int kCodeAlignmentMinusCodeHeader =
      kCodeAlignment - kHeaderSize;

  class BodyDescriptor;

 private:
  // During the Code initialization process, InstructionStream::code is briefly
  // unset (the Code object has not been allocated yet). In this state it is
  // only visible through heap iteration.
  inline void initialize_code_to_smi_zero(ReleaseStoreTag);
  friend class Factory;

  // Must be used when loading any of InstructionStream's tagged fields.
  static inline PtrComprCageBase main_cage_base();

  OBJECT_CONSTRUCTORS(InstructionStream, HeapObject);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_INSTRUCTION_STREAM_H_
