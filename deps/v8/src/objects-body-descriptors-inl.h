// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_BODY_DESCRIPTORS_INL_H_
#define V8_OBJECTS_BODY_DESCRIPTORS_INL_H_

#include "src/assembler-inl.h"
#include "src/feedback-vector.h"
#include "src/objects-body-descriptors.h"
#include "src/objects/hash-table.h"
#include "src/objects/js-collection.h"
#include "src/transitions.h"
#include "src/wasm/wasm-objects-inl.h"

namespace v8 {
namespace internal {

template <int start_offset>
int FlexibleBodyDescriptor<start_offset>::SizeOf(Map* map, HeapObject* object) {
  return object->SizeFromMap(map);
}

bool BodyDescriptorBase::IsValidSlotImpl(Map* map, HeapObject* obj,
                                         int offset) {
  if (!FLAG_unbox_double_fields || map->HasFastPointerLayout()) {
    return true;
  } else {
    DCHECK(FLAG_unbox_double_fields);
    DCHECK(IsAligned(offset, kPointerSize));

    LayoutDescriptorHelper helper(map);
    DCHECK(!helper.all_fields_tagged());
    return helper.IsTagged(offset);
  }
}

template <typename ObjectVisitor>
void BodyDescriptorBase::IterateBodyImpl(Map* map, HeapObject* obj,
                                         int start_offset, int end_offset,
                                         ObjectVisitor* v) {
  if (!FLAG_unbox_double_fields || map->HasFastPointerLayout()) {
    IteratePointers(obj, start_offset, end_offset, v);
  } else {
    DCHECK(FLAG_unbox_double_fields);
    DCHECK(IsAligned(start_offset, kPointerSize) &&
           IsAligned(end_offset, kPointerSize));

    LayoutDescriptorHelper helper(map);
    DCHECK(!helper.all_fields_tagged());
    for (int offset = start_offset; offset < end_offset;) {
      int end_of_region_offset;
      if (helper.IsTagged(offset, end_offset, &end_of_region_offset)) {
        IteratePointers(obj, offset, end_of_region_offset, v);
      }
      offset = end_of_region_offset;
    }
  }
}

template <typename ObjectVisitor>
DISABLE_CFI_PERF void BodyDescriptorBase::IteratePointers(HeapObject* obj,
                                                          int start_offset,
                                                          int end_offset,
                                                          ObjectVisitor* v) {
  v->VisitPointers(obj, HeapObject::RawField(obj, start_offset),
                   HeapObject::RawField(obj, end_offset));
}

template <typename ObjectVisitor>
void BodyDescriptorBase::IteratePointer(HeapObject* obj, int offset,
                                        ObjectVisitor* v) {
  v->VisitPointer(obj, HeapObject::RawField(obj, offset));
}

template <typename ObjectVisitor>
DISABLE_CFI_PERF void BodyDescriptorBase::IterateMaybeWeakPointers(
    HeapObject* obj, int start_offset, int end_offset, ObjectVisitor* v) {
  v->VisitPointers(obj, HeapObject::RawMaybeWeakField(obj, start_offset),
                   HeapObject::RawMaybeWeakField(obj, end_offset));
}

template <typename ObjectVisitor>
void BodyDescriptorBase::IterateMaybeWeakPointer(HeapObject* obj, int offset,
                                                 ObjectVisitor* v) {
  v->VisitPointer(obj, HeapObject::RawMaybeWeakField(obj, offset));
}

template <typename ObjectVisitor>
DISABLE_CFI_PERF void BodyDescriptorBase::IterateCustomWeakPointers(
    HeapObject* obj, int start_offset, int end_offset, ObjectVisitor* v) {
  v->VisitCustomWeakPointers(obj, HeapObject::RawField(obj, start_offset),
                             HeapObject::RawField(obj, end_offset));
}

template <typename ObjectVisitor>
void BodyDescriptorBase::IterateCustomWeakPointer(HeapObject* obj, int offset,
                                                  ObjectVisitor* v) {
  v->VisitCustomWeakPointer(obj, HeapObject::RawField(obj, offset));
}

class JSObject::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static const int kStartOffset = JSReceiver::kPropertiesOrHashOffset;

  static bool IsValidSlot(Map* map, HeapObject* obj, int offset) {
    if (offset < kStartOffset) return false;
    return IsValidSlotImpl(map, obj, offset);
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map* map, HeapObject* obj, int object_size,
                                 ObjectVisitor* v) {
    IterateBodyImpl(map, obj, kStartOffset, object_size, v);
  }

  static inline int SizeOf(Map* map, HeapObject* object) {
    return map->instance_size();
  }
};

class JSObject::FastBodyDescriptor final : public BodyDescriptorBase {
 public:
  static const int kStartOffset = JSReceiver::kPropertiesOrHashOffset;

  static bool IsValidSlot(Map* map, HeapObject* obj, int offset) {
    return offset >= kStartOffset;
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map* map, HeapObject* obj, int object_size,
                                 ObjectVisitor* v) {
    IteratePointers(obj, kStartOffset, object_size, v);
  }

  static inline int SizeOf(Map* map, HeapObject* object) {
    return map->instance_size();
  }
};

class JSFunction::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map* map, HeapObject* obj, int offset) {
    if (offset < kSizeWithoutPrototype) return true;
    if (offset < kSizeWithPrototype && map->has_prototype_slot()) {
      return true;
    }
    return IsValidSlotImpl(map, obj, offset);
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map* map, HeapObject* obj, int object_size,
                                 ObjectVisitor* v) {
    int header_size = JSFunction::GetHeaderSize(map->has_prototype_slot());
    DCHECK_EQ(header_size, JSObject::GetHeaderSize(map));
    IteratePointers(obj, kPropertiesOrHashOffset, header_size, v);
    IterateBodyImpl(map, obj, header_size, object_size, v);
  }

  static inline int SizeOf(Map* map, HeapObject* object) {
    return map->instance_size();
  }
};

class AllocationSite::BodyDescriptor final : public BodyDescriptorBase {
 public:
  STATIC_ASSERT(AllocationSite::kCommonPointerFieldEndOffset ==
                AllocationSite::kPretenureDataOffset);
  STATIC_ASSERT(AllocationSite::kPretenureDataOffset + kInt32Size ==
                AllocationSite::kPretenureCreateCountOffset);
  STATIC_ASSERT(AllocationSite::kPretenureCreateCountOffset + kInt32Size ==
                AllocationSite::kWeakNextOffset);

  static bool IsValidSlot(Map* map, HeapObject* obj, int offset) {
    if (offset >= AllocationSite::kStartOffset &&
        offset < AllocationSite::kCommonPointerFieldEndOffset) {
      return true;
    }
    // check for weak_next offset
    if (map->instance_size() == AllocationSite::kSizeWithWeakNext &&
        offset == AllocationSite::kWeakNextOffset) {
      return true;
    }
    return false;
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map* map, HeapObject* obj, int object_size,
                                 ObjectVisitor* v) {
    // Iterate over all the common pointer fields
    IteratePointers(obj, AllocationSite::kStartOffset,
                    AllocationSite::kCommonPointerFieldEndOffset, v);
    // Skip PretenureDataOffset and PretenureCreateCount which are Int32 fields.
    // Visit weak_next only if it has weak_next field.
    if (object_size == AllocationSite::kSizeWithWeakNext) {
      IterateCustomWeakPointers(obj, AllocationSite::kWeakNextOffset,
                                AllocationSite::kSizeWithWeakNext, v);
    }
  }

  static inline int SizeOf(Map* map, HeapObject* object) {
    return map->instance_size();
  }
};

class JSArrayBuffer::BodyDescriptor final : public BodyDescriptorBase {
 public:
  STATIC_ASSERT(kByteLengthOffset + kPointerSize == kBackingStoreOffset);
  STATIC_ASSERT(kBackingStoreOffset + kPointerSize == kBitFieldSlot);
  STATIC_ASSERT(kBitFieldSlot + kPointerSize == kSize);

  static bool IsValidSlot(Map* map, HeapObject* obj, int offset) {
    if (offset < kBitFieldSlot) return true;
    if (offset < kSize) return false;
    return IsValidSlotImpl(map, obj, offset);
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map* map, HeapObject* obj, int object_size,
                                 ObjectVisitor* v) {
    // JSArrayBuffer instances contain raw data that the GC does not know about.
    IteratePointers(obj, kPropertiesOrHashOffset, kByteLengthOffset, v);
    IterateBodyImpl(map, obj, kSize, object_size, v);
  }

  static inline int SizeOf(Map* map, HeapObject* object) {
    return map->instance_size();
  }
};

class JSArrayBufferView::BodyDescriptor final : public BodyDescriptorBase {
 public:
  STATIC_ASSERT(kBufferOffset + kPointerSize == kByteOffsetOffset);
  STATIC_ASSERT(kByteOffsetOffset + kUIntptrSize == kByteLengthOffset);
  STATIC_ASSERT(kByteLengthOffset + kUIntptrSize == kHeaderSize);

  static bool IsValidSlot(Map* map, HeapObject* obj, int offset) {
    if (offset < kByteOffsetOffset) return true;
    if (offset < kHeaderSize) return false;
    return IsValidSlotImpl(map, obj, offset);
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map* map, HeapObject* obj, int object_size,
                                 ObjectVisitor* v) {
    // JSArrayBufferView contains raw data that the GC does not know about.
    IteratePointers(obj, kPropertiesOrHashOffset, kByteOffsetOffset, v);
    IterateBodyImpl(map, obj, kHeaderSize, object_size, v);
  }

  static inline int SizeOf(Map* map, HeapObject* object) {
    return map->instance_size();
  }
};

template <typename Derived>
class SmallOrderedHashTable<Derived>::BodyDescriptor final
    : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map* map, HeapObject* obj, int offset) {
    Derived* table = reinterpret_cast<Derived*>(obj);
    if (offset < kDataTableStartOffset) return false;
    if (offset >= table->GetBucketsStartOffset()) return false;
    return IsValidSlotImpl(map, obj, offset);
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map* map, HeapObject* obj, int object_size,
                                 ObjectVisitor* v) {
    Derived* table = reinterpret_cast<Derived*>(obj);

    int offset = kDataTableStartOffset;
    int entry = 0;
    for (int i = 0; i < table->Capacity(); i++) {
      for (int j = 0; j < Derived::kEntrySize; j++) {
        IteratePointer(obj, offset + (entry * kPointerSize), v);
        entry++;
      }
    }
  }

  static inline int SizeOf(Map* map, HeapObject* obj) {
    Derived* table = reinterpret_cast<Derived*>(obj);
    return table->SizeFor(table->Capacity());
  }
};

class ByteArray::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map* map, HeapObject* obj, int offset) {
    return false;
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map* map, HeapObject* obj, int object_size,
                                 ObjectVisitor* v) {}

  static inline int SizeOf(Map* map, HeapObject* obj) {
    return ByteArray::SizeFor(ByteArray::cast(obj)->synchronized_length());
  }
};

class BytecodeArray::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map* map, HeapObject* obj, int offset) {
    return offset >= kConstantPoolOffset &&
           offset <= kSourcePositionTableOffset;
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map* map, HeapObject* obj, int object_size,
                                 ObjectVisitor* v) {
    IteratePointer(obj, kConstantPoolOffset, v);
    IteratePointer(obj, kHandlerTableOffset, v);
    IteratePointer(obj, kSourcePositionTableOffset, v);
  }

  static inline int SizeOf(Map* map, HeapObject* obj) {
    return BytecodeArray::SizeFor(
        BytecodeArray::cast(obj)->synchronized_length());
  }
};

class BigInt::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map* map, HeapObject* obj, int offset) {
    return false;
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map* map, HeapObject* obj, int object_size,
                                 ObjectVisitor* v) {}

  static inline int SizeOf(Map* map, HeapObject* obj) {
    return BigInt::SizeFor(BigInt::cast(obj)->length());
  }
};

class FixedDoubleArray::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map* map, HeapObject* obj, int offset) {
    return false;
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map* map, HeapObject* obj, int object_size,
                                 ObjectVisitor* v) {}

  static inline int SizeOf(Map* map, HeapObject* obj) {
    return FixedDoubleArray::SizeFor(
        FixedDoubleArray::cast(obj)->synchronized_length());
  }
};

class FixedTypedArrayBase::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map* map, HeapObject* obj, int offset) {
    return offset == kBasePointerOffset;
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map* map, HeapObject* obj, int object_size,
                                 ObjectVisitor* v) {
    IteratePointer(obj, kBasePointerOffset, v);
  }

  static inline int SizeOf(Map* map, HeapObject* object) {
    return FixedTypedArrayBase::cast(object)->size();
  }
};

class WeakArrayBodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map* map, HeapObject* obj, int offset) {
    return true;
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map* map, HeapObject* obj, int object_size,
                                 ObjectVisitor* v) {
    IterateMaybeWeakPointers(obj, HeapObject::kHeaderSize, object_size, v);
  }

  static inline int SizeOf(Map* map, HeapObject* object) {
    return object->SizeFromMap(map);
  }
};

class FeedbackMetadata::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map* map, HeapObject* obj, int offset) {
    return false;
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map* map, HeapObject* obj, int object_size,
                                 ObjectVisitor* v) {}

  static inline int SizeOf(Map* map, HeapObject* obj) {
    return FeedbackMetadata::SizeFor(
        FeedbackMetadata::cast(obj)->synchronized_slot_count());
  }
};

class FeedbackVector::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map* map, HeapObject* obj, int offset) {
    return offset == kSharedFunctionInfoOffset ||
           offset == kOptimizedCodeOffset || offset >= kFeedbackSlotsOffset;
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map* map, HeapObject* obj, int object_size,
                                 ObjectVisitor* v) {
    IteratePointer(obj, kSharedFunctionInfoOffset, v);
    IterateMaybeWeakPointer(obj, kOptimizedCodeOffset, v);
    IterateMaybeWeakPointers(obj, kFeedbackSlotsOffset, object_size, v);
  }

  static inline int SizeOf(Map* map, HeapObject* obj) {
    return FeedbackVector::SizeFor(FeedbackVector::cast(obj)->length());
  }
};

class PreParsedScopeData::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map* map, HeapObject* obj, int offset) {
    return offset == kScopeDataOffset || offset >= kChildDataStartOffset;
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map* map, HeapObject* obj, int object_size,
                                 ObjectVisitor* v) {
    IteratePointer(obj, kScopeDataOffset, v);
    IteratePointers(obj, kChildDataStartOffset, object_size, v);
  }

  static inline int SizeOf(Map* map, HeapObject* obj) {
    return PreParsedScopeData::SizeFor(PreParsedScopeData::cast(obj)->length());
  }
};

class PrototypeInfo::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map* map, HeapObject* obj, int offset) {
    return offset >= HeapObject::kHeaderSize;
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map* map, HeapObject* obj, int object_size,
                                 ObjectVisitor* v) {
    IteratePointers(obj, HeapObject::kHeaderSize, kObjectCreateMapOffset, v);
    IterateMaybeWeakPointer(obj, kObjectCreateMapOffset, v);
    IteratePointers(obj, kObjectCreateMapOffset + kPointerSize, object_size, v);
  }

  static inline int SizeOf(Map* map, HeapObject* obj) {
    return obj->SizeFromMap(map);
  }
};

class JSWeakCollection::BodyDescriptorImpl final : public BodyDescriptorBase {
 public:
  STATIC_ASSERT(kTableOffset + kPointerSize == kSize);

  static bool IsValidSlot(Map* map, HeapObject* obj, int offset) {
    return IsValidSlotImpl(map, obj, offset);
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map* map, HeapObject* obj, int object_size,
                                 ObjectVisitor* v) {
    IterateBodyImpl(map, obj, kPropertiesOrHashOffset, object_size, v);
  }

  static inline int SizeOf(Map* map, HeapObject* object) {
    return map->instance_size();
  }
};

class Foreign::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map* map, HeapObject* obj, int offset) {
    return false;
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map* map, HeapObject* obj, int object_size,
                                 ObjectVisitor* v) {
    v->VisitExternalReference(Foreign::cast(obj),
                              reinterpret_cast<Address*>(HeapObject::RawField(
                                  obj, kForeignAddressOffset)));
  }

  static inline int SizeOf(Map* map, HeapObject* object) { return kSize; }
};

class ExternalOneByteString::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map* map, HeapObject* obj, int offset) {
    return false;
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map* map, HeapObject* obj, int object_size,
                                 ObjectVisitor* v) {}

  static inline int SizeOf(Map* map, HeapObject* object) { return kSize; }
};

class ExternalTwoByteString::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map* map, HeapObject* obj, int offset) {
    return false;
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map* map, HeapObject* obj, int object_size,
                                 ObjectVisitor* v) {}

  static inline int SizeOf(Map* map, HeapObject* object) { return kSize; }
};

class Code::BodyDescriptor final : public BodyDescriptorBase {
 public:
  STATIC_ASSERT(kRelocationInfoOffset + kPointerSize ==
                kDeoptimizationDataOffset);
  STATIC_ASSERT(kDeoptimizationDataOffset + kPointerSize ==
                kSourcePositionTableOffset);
  STATIC_ASSERT(kSourcePositionTableOffset + kPointerSize ==
                kCodeDataContainerOffset);
  STATIC_ASSERT(kCodeDataContainerOffset + kPointerSize == kDataStart);

  static bool IsValidSlot(Map* map, HeapObject* obj, int offset) {
    // Slots in code can't be invalid because we never trim code objects.
    return true;
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map* map, HeapObject* obj, ObjectVisitor* v) {
    int mode_mask = RelocInfo::ModeMask(RelocInfo::CODE_TARGET) |
                    RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT) |
                    RelocInfo::ModeMask(RelocInfo::EXTERNAL_REFERENCE) |
                    RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE) |
                    RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE_ENCODED) |
                    RelocInfo::ModeMask(RelocInfo::OFF_HEAP_TARGET) |
                    RelocInfo::ModeMask(RelocInfo::RUNTIME_ENTRY);

    // GC does not visit data/code in the header and in the body directly.
    IteratePointers(obj, kRelocationInfoOffset, kDataStart, v);

    RelocIterator it(Code::cast(obj), mode_mask);
    v->VisitRelocInfo(&it);
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map* map, HeapObject* obj, int object_size,
                                 ObjectVisitor* v) {
    IterateBody(map, obj, v);
  }

  static inline int SizeOf(Map* map, HeapObject* object) {
    return reinterpret_cast<Code*>(object)->CodeSize();
  }
};

class SeqOneByteString::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map* map, HeapObject* obj, int offset) {
    return false;
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map* map, HeapObject* obj, int object_size,
                                 ObjectVisitor* v) {}

  static inline int SizeOf(Map* map, HeapObject* obj) {
    SeqOneByteString* string = SeqOneByteString::cast(obj);
    return string->SizeFor(string->synchronized_length());
  }
};

class SeqTwoByteString::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map* map, HeapObject* obj, int offset) {
    return false;
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map* map, HeapObject* obj, int object_size,
                                 ObjectVisitor* v) {}

  static inline int SizeOf(Map* map, HeapObject* obj) {
    SeqTwoByteString* string = SeqTwoByteString::cast(obj);
    return string->SizeFor(string->synchronized_length());
  }
};

class WasmInstanceObject::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map* map, HeapObject* obj, int offset) {
    if (offset < kMemoryStartOffset) return true;
    if (offset < kModuleObjectOffset) return false;
    return IsValidSlotImpl(map, obj, offset);
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map* map, HeapObject* obj, int object_size,
                                 ObjectVisitor* v) {
    IteratePointers(obj, kPropertiesOrHashOffset, kFirstUntaggedOffset, v);
    IterateBodyImpl(map, obj, kSize, object_size, v);
  }

  static inline int SizeOf(Map* map, HeapObject* object) {
    return map->instance_size();
  }
};

class Map::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map* map, HeapObject* obj, int offset) {
    return offset >= Map::kPointerFieldsBeginOffset &&
           offset < Map::kPointerFieldsEndOffset;
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map* map, HeapObject* obj, int object_size,
                                 ObjectVisitor* v) {
    IteratePointers(obj, Map::kPointerFieldsBeginOffset,
                    Map::kTransitionsOrPrototypeInfoOffset, v);
    IterateMaybeWeakPointer(obj, kTransitionsOrPrototypeInfoOffset, v);
    IteratePointers(obj, Map::kTransitionsOrPrototypeInfoOffset + kPointerSize,
                    Map::kPointerFieldsEndOffset, v);
  }

  static inline int SizeOf(Map* map, HeapObject* obj) { return Map::kSize; }
};

class DataHandler::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map* map, HeapObject* obj, int offset) {
    return offset >= HeapObject::kHeaderSize;
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map* map, HeapObject* obj, int object_size,
                                 ObjectVisitor* v) {
    static_assert(kSmiHandlerOffset < kData1Offset,
                  "Field order must be in sync with this iteration code");
    static_assert(kData1Offset < kSizeWithData1,
                  "Field order must be in sync with this iteration code");
    IteratePointers(obj, kSmiHandlerOffset, kData1Offset, v);
    IterateMaybeWeakPointers(obj, kData1Offset, object_size, v);
  }

  static inline int SizeOf(Map* map, HeapObject* object) {
    return object->SizeFromMap(map);
  }
};

class Context::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map* map, HeapObject* obj, int offset) {
    return offset >= Context::kHeaderSize && offset < Context::kSize;
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map* map, HeapObject* obj, int object_size,
                                 ObjectVisitor* v) {
    IteratePointers(obj, Context::kHeaderSize,
                    Context::kHeaderSize + FIRST_WEAK_SLOT * kPointerSize, v);
    IterateCustomWeakPointers(
        obj, Context::kHeaderSize + FIRST_WEAK_SLOT * kPointerSize,
        Context::kSize, v);
  }

  static inline int SizeOf(Map* map, HeapObject* object) {
    return Context::kSize;
  }
};

class CodeDataContainer::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map* map, HeapObject* obj, int offset) {
    return offset >= CodeDataContainer::kHeaderSize &&
           offset < CodeDataContainer::kSize;
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map* map, HeapObject* obj, int object_size,
                                 ObjectVisitor* v) {
    IteratePointers(obj, CodeDataContainer::kHeaderSize,
                    CodeDataContainer::kPointerFieldsStrongEndOffset, v);
    IterateCustomWeakPointers(
        obj, CodeDataContainer::kPointerFieldsStrongEndOffset,
        CodeDataContainer::kPointerFieldsWeakEndOffset, v);
  }

  static inline int SizeOf(Map* map, HeapObject* object) {
    return CodeDataContainer::kSize;
  }
};

template <typename Op, typename ReturnType, typename T1, typename T2,
          typename T3, typename T4>
ReturnType BodyDescriptorApply(InstanceType type, T1 p1, T2 p2, T3 p3, T4 p4) {
  if (type < FIRST_NONSTRING_TYPE) {
    switch (type & kStringRepresentationMask) {
      case kSeqStringTag:
        return ReturnType();
      case kConsStringTag:
        return Op::template apply<ConsString::BodyDescriptor>(p1, p2, p3, p4);
      case kThinStringTag:
        return Op::template apply<ThinString::BodyDescriptor>(p1, p2, p3, p4);
      case kSlicedStringTag:
        return Op::template apply<SlicedString::BodyDescriptor>(p1, p2, p3, p4);
      case kExternalStringTag:
        if ((type & kStringEncodingMask) == kOneByteStringTag) {
          return Op::template apply<ExternalOneByteString::BodyDescriptor>(
              p1, p2, p3, p4);
        } else {
          return Op::template apply<ExternalTwoByteString::BodyDescriptor>(
              p1, p2, p3, p4);
        }
    }
    UNREACHABLE();
  }

  switch (type) {
    case FIXED_ARRAY_TYPE:
    case OBJECT_BOILERPLATE_DESCRIPTION_TYPE:
    case HASH_TABLE_TYPE:
    case ORDERED_HASH_MAP_TYPE:
    case ORDERED_HASH_SET_TYPE:
    case NAME_DICTIONARY_TYPE:
    case GLOBAL_DICTIONARY_TYPE:
    case NUMBER_DICTIONARY_TYPE:
    case SIMPLE_NUMBER_DICTIONARY_TYPE:
    case STRING_TABLE_TYPE:
    case EPHEMERON_HASH_TABLE_TYPE:
    case SCOPE_INFO_TYPE:
    case SCRIPT_CONTEXT_TABLE_TYPE:
    case AWAIT_CONTEXT_TYPE:
    case BLOCK_CONTEXT_TYPE:
    case CATCH_CONTEXT_TYPE:
    case DEBUG_EVALUATE_CONTEXT_TYPE:
    case EVAL_CONTEXT_TYPE:
    case FUNCTION_CONTEXT_TYPE:
    case MODULE_CONTEXT_TYPE:
    case NATIVE_CONTEXT_TYPE:
    case SCRIPT_CONTEXT_TYPE:
    case WITH_CONTEXT_TYPE:
      return Op::template apply<FixedArray::BodyDescriptor>(p1, p2, p3, p4);
    case WEAK_FIXED_ARRAY_TYPE:
      return Op::template apply<WeakFixedArray::BodyDescriptor>(p1, p2, p3, p4);
    case WEAK_ARRAY_LIST_TYPE:
      return Op::template apply<WeakArrayList::BodyDescriptor>(p1, p2, p3, p4);
    case FIXED_DOUBLE_ARRAY_TYPE:
      return ReturnType();
    case FEEDBACK_METADATA_TYPE:
      return Op::template apply<FeedbackMetadata::BodyDescriptor>(p1, p2, p3,
                                                                  p4);
    case PROPERTY_ARRAY_TYPE:
      return Op::template apply<PropertyArray::BodyDescriptor>(p1, p2, p3, p4);
    case DESCRIPTOR_ARRAY_TYPE:
      return Op::template apply<DescriptorArray::BodyDescriptor>(p1, p2, p3,
                                                                 p4);
    case TRANSITION_ARRAY_TYPE:
      return Op::template apply<TransitionArray::BodyDescriptor>(p1, p2, p3,
                                                                 p4);
    case FEEDBACK_CELL_TYPE:
      return Op::template apply<FeedbackCell::BodyDescriptor>(p1, p2, p3, p4);
    case FEEDBACK_VECTOR_TYPE:
      return Op::template apply<FeedbackVector::BodyDescriptor>(p1, p2, p3, p4);
    case JS_OBJECT_TYPE:
    case JS_ERROR_TYPE:
    case JS_ARGUMENTS_TYPE:
    case JS_ASYNC_FROM_SYNC_ITERATOR_TYPE:
    case JS_PROMISE_TYPE:
    case JS_CONTEXT_EXTENSION_OBJECT_TYPE:
    case JS_GENERATOR_OBJECT_TYPE:
    case JS_ASYNC_GENERATOR_OBJECT_TYPE:
    case JS_VALUE_TYPE:
    case JS_DATE_TYPE:
    case JS_ARRAY_TYPE:
    case JS_ARRAY_ITERATOR_TYPE:
    case JS_MODULE_NAMESPACE_TYPE:
    case JS_SET_TYPE:
    case JS_MAP_TYPE:
    case JS_SET_KEY_VALUE_ITERATOR_TYPE:
    case JS_SET_VALUE_ITERATOR_TYPE:
    case JS_MAP_KEY_ITERATOR_TYPE:
    case JS_MAP_KEY_VALUE_ITERATOR_TYPE:
    case JS_MAP_VALUE_ITERATOR_TYPE:
    case JS_STRING_ITERATOR_TYPE:
    case JS_REGEXP_STRING_ITERATOR_TYPE:
    case JS_REGEXP_TYPE:
    case JS_GLOBAL_PROXY_TYPE:
    case JS_GLOBAL_OBJECT_TYPE:
    case JS_API_OBJECT_TYPE:
    case JS_SPECIAL_API_OBJECT_TYPE:
    case JS_MESSAGE_OBJECT_TYPE:
    case JS_BOUND_FUNCTION_TYPE:
#ifdef V8_INTL_SUPPORT
    case JS_INTL_V8_BREAK_ITERATOR_TYPE:
    case JS_INTL_COLLATOR_TYPE:
    case JS_INTL_DATE_TIME_FORMAT_TYPE:
    case JS_INTL_LIST_FORMAT_TYPE:
    case JS_INTL_LOCALE_TYPE:
    case JS_INTL_NUMBER_FORMAT_TYPE:
    case JS_INTL_PLURAL_RULES_TYPE:
    case JS_INTL_RELATIVE_TIME_FORMAT_TYPE:
    case JS_INTL_SEGMENTER_TYPE:
#endif  // V8_INTL_SUPPORT
    case WASM_EXCEPTION_TYPE:
    case WASM_GLOBAL_TYPE:
    case WASM_MEMORY_TYPE:
    case WASM_MODULE_TYPE:
    case WASM_TABLE_TYPE:
      return Op::template apply<JSObject::BodyDescriptor>(p1, p2, p3, p4);
    case WASM_INSTANCE_TYPE:
      return Op::template apply<WasmInstanceObject::BodyDescriptor>(p1, p2, p3,
                                                                    p4);
    case JS_WEAK_MAP_TYPE:
    case JS_WEAK_SET_TYPE:
      return Op::template apply<JSWeakCollection::BodyDescriptor>(p1, p2, p3,
                                                                  p4);
    case JS_ARRAY_BUFFER_TYPE:
      return Op::template apply<JSArrayBuffer::BodyDescriptor>(p1, p2, p3, p4);
    case JS_DATA_VIEW_TYPE:
      return Op::template apply<JSDataView::BodyDescriptor>(p1, p2, p3, p4);
    case JS_TYPED_ARRAY_TYPE:
      return Op::template apply<JSTypedArray::BodyDescriptor>(p1, p2, p3, p4);
    case JS_FUNCTION_TYPE:
      return Op::template apply<JSFunction::BodyDescriptor>(p1, p2, p3, p4);
    case ODDBALL_TYPE:
      return Op::template apply<Oddball::BodyDescriptor>(p1, p2, p3, p4);
    case JS_PROXY_TYPE:
      return Op::template apply<JSProxy::BodyDescriptor>(p1, p2, p3, p4);
    case FOREIGN_TYPE:
      return Op::template apply<Foreign::BodyDescriptor>(p1, p2, p3, p4);
    case MAP_TYPE:
      return Op::template apply<Map::BodyDescriptor>(p1, p2, p3, p4);
    case CODE_TYPE:
      return Op::template apply<Code::BodyDescriptor>(p1, p2, p3, p4);
    case CELL_TYPE:
      return Op::template apply<Cell::BodyDescriptor>(p1, p2, p3, p4);
    case PROPERTY_CELL_TYPE:
      return Op::template apply<PropertyCell::BodyDescriptor>(p1, p2, p3, p4);
    case SYMBOL_TYPE:
      return Op::template apply<Symbol::BodyDescriptor>(p1, p2, p3, p4);
    case BYTECODE_ARRAY_TYPE:
      return Op::template apply<BytecodeArray::BodyDescriptor>(p1, p2, p3, p4);
    case SMALL_ORDERED_HASH_SET_TYPE:
      return Op::template apply<
          SmallOrderedHashTable<SmallOrderedHashSet>::BodyDescriptor>(p1, p2,
                                                                      p3, p4);
    case SMALL_ORDERED_HASH_MAP_TYPE:
      return Op::template apply<
          SmallOrderedHashTable<SmallOrderedHashMap>::BodyDescriptor>(p1, p2,
                                                                      p3, p4);
    case CODE_DATA_CONTAINER_TYPE:
      return Op::template apply<CodeDataContainer::BodyDescriptor>(p1, p2, p3,
                                                                   p4);
    case PRE_PARSED_SCOPE_DATA_TYPE:
      return Op::template apply<PreParsedScopeData::BodyDescriptor>(p1, p2, p3,
                                                                    p4);
    case UNCOMPILED_DATA_WITHOUT_PRE_PARSED_SCOPE_TYPE:
      return Op::template apply<
          UncompiledDataWithoutPreParsedScope::BodyDescriptor>(p1, p2, p3, p4);
    case UNCOMPILED_DATA_WITH_PRE_PARSED_SCOPE_TYPE:
      return Op::template apply<
          UncompiledDataWithPreParsedScope::BodyDescriptor>(p1, p2, p3, p4);
    case HEAP_NUMBER_TYPE:
    case MUTABLE_HEAP_NUMBER_TYPE:
    case FILLER_TYPE:
    case BYTE_ARRAY_TYPE:
    case FREE_SPACE_TYPE:
    case BIGINT_TYPE:
      return ReturnType();

#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype)                              \
  case FIXED_##TYPE##_ARRAY_TYPE:                                              \
    return Op::template apply<FixedTypedArrayBase::BodyDescriptor>(p1, p2, p3, \
                                                                   p4);
      TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE

    case SHARED_FUNCTION_INFO_TYPE: {
      return Op::template apply<SharedFunctionInfo::BodyDescriptor>(p1, p2, p3,
                                                                    p4);
    }
    case ALLOCATION_SITE_TYPE:
      return Op::template apply<AllocationSite::BodyDescriptor>(p1, p2, p3, p4);

#define MAKE_STRUCT_CASE(TYPE, Name, name) case TYPE:
      STRUCT_LIST(MAKE_STRUCT_CASE)
#undef MAKE_STRUCT_CASE
      if (type == PROTOTYPE_INFO_TYPE) {
        return Op::template apply<PrototypeInfo::BodyDescriptor>(p1, p2, p3,
                                                                 p4);
      } else {
        return Op::template apply<StructBodyDescriptor>(p1, p2, p3, p4);
      }
    case CALL_HANDLER_INFO_TYPE:
      return Op::template apply<StructBodyDescriptor>(p1, p2, p3, p4);
    case LOAD_HANDLER_TYPE:
    case STORE_HANDLER_TYPE:
      return Op::template apply<DataHandler::BodyDescriptor>(p1, p2, p3, p4);
    default:
      PrintF("Unknown type: %d\n", type);
      UNREACHABLE();
  }
}


template <typename ObjectVisitor>
void HeapObject::IterateFast(ObjectVisitor* v) {
  BodyDescriptorBase::IteratePointer(this, kMapOffset, v);
  IterateBodyFast(v);
}


template <typename ObjectVisitor>
void HeapObject::IterateBodyFast(ObjectVisitor* v) {
  Map* m = map();
  IterateBodyFast(m, SizeFromMap(m), v);
}


struct CallIterateBody {
  template <typename BodyDescriptor, typename ObjectVisitor>
  static void apply(Map* map, HeapObject* obj, int object_size,
                    ObjectVisitor* v) {
    BodyDescriptor::IterateBody(map, obj, object_size, v);
  }
};

template <typename ObjectVisitor>
void HeapObject::IterateBodyFast(Map* map, int object_size, ObjectVisitor* v) {
  BodyDescriptorApply<CallIterateBody, void>(map->instance_type(), map, this,
                                             object_size, v);
}
}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_BODY_DESCRIPTORS_INL_H_
