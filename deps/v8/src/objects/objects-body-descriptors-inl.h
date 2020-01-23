// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_OBJECTS_BODY_DESCRIPTORS_INL_H_
#define V8_OBJECTS_OBJECTS_BODY_DESCRIPTORS_INL_H_

#include "src/objects/objects-body-descriptors.h"

#include <algorithm>

#include "src/codegen/reloc-info.h"
#include "src/objects/cell.h"
#include "src/objects/data-handler.h"
#include "src/objects/feedback-vector.h"
#include "src/objects/foreign-inl.h"
#include "src/objects/hash-table.h"
#include "src/objects/js-collection.h"
#include "src/objects/js-weak-refs.h"
#include "src/objects/oddball.h"
#include "src/objects/ordered-hash-table.h"
#include "src/objects/source-text-module.h"
#include "src/objects/synthetic-module.h"
#include "src/objects/transitions.h"
#include "src/wasm/wasm-objects-inl.h"

namespace v8 {
namespace internal {

template <int start_offset>
int FlexibleBodyDescriptor<start_offset>::SizeOf(Map map, HeapObject object) {
  return object.SizeFromMap(map);
}

template <int start_offset>
int FlexibleWeakBodyDescriptor<start_offset>::SizeOf(Map map,
                                                     HeapObject object) {
  return object.SizeFromMap(map);
}

bool BodyDescriptorBase::IsValidJSObjectSlotImpl(Map map, HeapObject obj,
                                                 int offset) {
#ifdef V8_COMPRESS_POINTERS
  STATIC_ASSERT(kEmbedderDataSlotSize == 2 * kTaggedSize);
  int embedder_fields_offset = JSObject::GetEmbedderFieldsStartOffset(map);
  int inobject_fields_offset = map.GetInObjectPropertyOffset(0);
  // |embedder_fields_offset| may be greater than |inobject_fields_offset| if
  // the object does not have embedder fields but the check handles this
  // case properly.
  if (embedder_fields_offset <= offset && offset < inobject_fields_offset) {
    // offset points to embedder fields area:
    // [embedder_fields_offset, inobject_fields_offset).
    STATIC_ASSERT(base::bits::IsPowerOfTwo(kEmbedderDataSlotSize));
    return ((offset - embedder_fields_offset) & (kEmbedderDataSlotSize - 1)) ==
           EmbedderDataSlot::kTaggedPayloadOffset;
  }
#else
  // We store raw aligned pointers as Smis, so it's safe to treat the whole
  // embedder field area as tagged slots.
  STATIC_ASSERT(kEmbedderDataSlotSize == kTaggedSize);
#endif
  if (!FLAG_unbox_double_fields || map.HasFastPointerLayout()) {
    return true;
  } else {
    DCHECK(FLAG_unbox_double_fields);
    DCHECK(IsAligned(offset, kSystemPointerSize));

    LayoutDescriptorHelper helper(map);
    DCHECK(!helper.all_fields_tagged());
    return helper.IsTagged(offset);
  }
}

template <typename ObjectVisitor>
void BodyDescriptorBase::IterateJSObjectBodyImpl(Map map, HeapObject obj,
                                                 int start_offset,
                                                 int end_offset,
                                                 ObjectVisitor* v) {
#ifdef V8_COMPRESS_POINTERS
  STATIC_ASSERT(kEmbedderDataSlotSize == 2 * kTaggedSize);
  int header_size = JSObject::GetHeaderSize(map);
  int inobject_fields_offset = map.GetInObjectPropertyOffset(0);
  // We are always requested to process header and embedder fields.
  DCHECK_LE(inobject_fields_offset, end_offset);
  // Embedder fields are located between header and inobject properties.
  if (header_size < inobject_fields_offset) {
    // There are embedder fields.
    IteratePointers(obj, start_offset, header_size, v);
    // Iterate only tagged payload of the embedder slots and skip raw payload.
    DCHECK_EQ(header_size, JSObject::GetEmbedderFieldsStartOffset(map));
    for (int offset = header_size + EmbedderDataSlot::kTaggedPayloadOffset;
         offset < inobject_fields_offset; offset += kEmbedderDataSlotSize) {
      IteratePointer(obj, offset, v);
    }
    // Proceed processing inobject properties.
    start_offset = inobject_fields_offset;
  }
#else
  // We store raw aligned pointers as Smis, so it's safe to iterate the whole
  // embedder field area as tagged slots.
  STATIC_ASSERT(kEmbedderDataSlotSize == kTaggedSize);
#endif
  if (!FLAG_unbox_double_fields || map.HasFastPointerLayout()) {
    IteratePointers(obj, start_offset, end_offset, v);
  } else {
    DCHECK(FLAG_unbox_double_fields);
    DCHECK(IsAligned(start_offset, kSystemPointerSize) &&
           IsAligned(end_offset, kSystemPointerSize));

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
DISABLE_CFI_PERF void BodyDescriptorBase::IteratePointers(HeapObject obj,
                                                          int start_offset,
                                                          int end_offset,
                                                          ObjectVisitor* v) {
  v->VisitPointers(obj, obj.RawField(start_offset), obj.RawField(end_offset));
}

template <typename ObjectVisitor>
void BodyDescriptorBase::IteratePointer(HeapObject obj, int offset,
                                        ObjectVisitor* v) {
  v->VisitPointer(obj, obj.RawField(offset));
}

template <typename ObjectVisitor>
DISABLE_CFI_PERF void BodyDescriptorBase::IterateMaybeWeakPointers(
    HeapObject obj, int start_offset, int end_offset, ObjectVisitor* v) {
  v->VisitPointers(obj, obj.RawMaybeWeakField(start_offset),
                   obj.RawMaybeWeakField(end_offset));
}

template <typename ObjectVisitor>
void BodyDescriptorBase::IterateMaybeWeakPointer(HeapObject obj, int offset,
                                                 ObjectVisitor* v) {
  v->VisitPointer(obj, obj.RawMaybeWeakField(offset));
}

template <typename ObjectVisitor>
DISABLE_CFI_PERF void BodyDescriptorBase::IterateCustomWeakPointers(
    HeapObject obj, int start_offset, int end_offset, ObjectVisitor* v) {
  v->VisitCustomWeakPointers(obj, obj.RawField(start_offset),
                             obj.RawField(end_offset));
}

template <typename ObjectVisitor>
DISABLE_CFI_PERF void BodyDescriptorBase::IterateEphemeron(HeapObject obj,
                                                           int index,
                                                           int key_offset,
                                                           int value_offset,
                                                           ObjectVisitor* v) {
  v->VisitEphemeron(obj, index, obj.RawField(key_offset),
                    obj.RawField(value_offset));
}

template <typename ObjectVisitor>
void BodyDescriptorBase::IterateCustomWeakPointer(HeapObject obj, int offset,
                                                  ObjectVisitor* v) {
  v->VisitCustomWeakPointer(obj, obj.RawField(offset));
}

class JSObject::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static const int kStartOffset = JSReceiver::kPropertiesOrHashOffset;

  static bool IsValidSlot(Map map, HeapObject obj, int offset) {
    if (offset < kStartOffset) return false;
    return IsValidJSObjectSlotImpl(map, obj, offset);
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map map, HeapObject obj, int object_size,
                                 ObjectVisitor* v) {
    IterateJSObjectBodyImpl(map, obj, kStartOffset, object_size, v);
  }

  static inline int SizeOf(Map map, HeapObject object) {
    return map.instance_size();
  }
};

class JSObject::FastBodyDescriptor final : public BodyDescriptorBase {
 public:
  static const int kStartOffset = JSReceiver::kPropertiesOrHashOffset;

  static bool IsValidSlot(Map map, HeapObject obj, int offset) {
    return offset >= kStartOffset;
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map map, HeapObject obj, int object_size,
                                 ObjectVisitor* v) {
    IteratePointers(obj, kStartOffset, object_size, v);
  }

  static inline int SizeOf(Map map, HeapObject object) {
    return map.instance_size();
  }
};

class WeakCell::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map map, HeapObject obj, int offset) {
    return offset >= HeapObject::kHeaderSize;
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map map, HeapObject obj, int object_size,
                                 ObjectVisitor* v) {
    IteratePointers(obj, HeapObject::kHeaderSize, kTargetOffset, v);
    IterateCustomWeakPointer(obj, kTargetOffset, v);
    IteratePointers(obj, kTargetOffset + kTaggedSize, object_size, v);
  }

  static inline int SizeOf(Map map, HeapObject object) {
    return map.instance_size();
  }
};

class JSWeakRef::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map map, HeapObject obj, int offset) {
    return IsValidJSObjectSlotImpl(map, obj, offset);
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map map, HeapObject obj, int object_size,
                                 ObjectVisitor* v) {
    IteratePointers(obj, JSReceiver::kPropertiesOrHashOffset, kTargetOffset, v);
    IterateCustomWeakPointer(obj, kTargetOffset, v);
    IterateJSObjectBodyImpl(map, obj, kTargetOffset + kTaggedSize, object_size,
                            v);
  }

  static inline int SizeOf(Map map, HeapObject object) {
    return map.instance_size();
  }
};

class SharedFunctionInfo::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map map, HeapObject obj, int offset) {
    static_assert(kEndOfWeakFieldsOffset == kStartOfStrongFieldsOffset,
                  "Leverage that strong fields directly follow weak fields"
                  "to call FixedBodyDescriptor<...>::IsValidSlot below");
    return FixedBodyDescriptor<kStartOfWeakFieldsOffset,
                               kEndOfStrongFieldsOffset,
                               kAlignedSize>::IsValidSlot(map, obj, offset);
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map map, HeapObject obj, int object_size,
                                 ObjectVisitor* v) {
    IterateCustomWeakPointer(obj, kFunctionDataOffset, v);
    IteratePointers(obj, SharedFunctionInfo::kStartOfStrongFieldsOffset,
                    SharedFunctionInfo::kEndOfStrongFieldsOffset, v);
  }

  static inline int SizeOf(Map map, HeapObject object) {
    return map.instance_size();
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

  static bool IsValidSlot(Map map, HeapObject obj, int offset) {
    if (offset >= AllocationSite::kStartOffset &&
        offset < AllocationSite::kCommonPointerFieldEndOffset) {
      return true;
    }
    // check for weak_next offset
    if (map.instance_size() == AllocationSite::kSizeWithWeakNext &&
        offset == AllocationSite::kWeakNextOffset) {
      return true;
    }
    return false;
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map map, HeapObject obj, int object_size,
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

  static inline int SizeOf(Map map, HeapObject object) {
    return map.instance_size();
  }
};

class JSArrayBuffer::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map map, HeapObject obj, int offset) {
    if (offset < kEndOfTaggedFieldsOffset) return true;
    if (offset < kHeaderSize) return false;
    return IsValidJSObjectSlotImpl(map, obj, offset);
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map map, HeapObject obj, int object_size,
                                 ObjectVisitor* v) {
    // JSArrayBuffer instances contain raw data that the GC does not know about.
    IteratePointers(obj, kPropertiesOrHashOffset, kEndOfTaggedFieldsOffset, v);
    IterateJSObjectBodyImpl(map, obj, kHeaderSize, object_size, v);
  }

  static inline int SizeOf(Map map, HeapObject object) {
    return map.instance_size();
  }
};

class JSTypedArray::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map map, HeapObject obj, int offset) {
    if (offset < kEndOfTaggedFieldsOffset) return true;
    // TODO(v8:4153): Remove this.
    if (offset == kBasePointerOffset) return true;
    if (offset < kHeaderSize) return false;
    return IsValidJSObjectSlotImpl(map, obj, offset);
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map map, HeapObject obj, int object_size,
                                 ObjectVisitor* v) {
    // JSTypedArray contains raw data that the GC does not know about.
    IteratePointers(obj, kPropertiesOrHashOffset, kEndOfTaggedFieldsOffset, v);
    // TODO(v8:4153): Remove this.
    IteratePointer(obj, kBasePointerOffset, v);
    IterateJSObjectBodyImpl(map, obj, kHeaderSize, object_size, v);
  }

  static inline int SizeOf(Map map, HeapObject object) {
    return map.instance_size();
  }
};

class JSDataView::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map map, HeapObject obj, int offset) {
    if (offset < kEndOfTaggedFieldsOffset) return true;
    if (offset < kHeaderSize) return false;
    return IsValidJSObjectSlotImpl(map, obj, offset);
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map map, HeapObject obj, int object_size,
                                 ObjectVisitor* v) {
    // JSDataView contains raw data that the GC does not know about.
    IteratePointers(obj, kPropertiesOrHashOffset, kEndOfTaggedFieldsOffset, v);
    IterateJSObjectBodyImpl(map, obj, kHeaderSize, object_size, v);
  }

  static inline int SizeOf(Map map, HeapObject object) {
    return map.instance_size();
  }
};

template <typename Derived>
class V8_EXPORT_PRIVATE SmallOrderedHashTable<Derived>::BodyDescriptor final
    : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map map, HeapObject obj, int offset) {
    Derived table = Derived::cast(obj);
    // Only data table part contains tagged values.
    return (offset >= DataTableStartOffset()) &&
           (offset < table.GetBucketsStartOffset());
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map map, HeapObject obj, int object_size,
                                 ObjectVisitor* v) {
    Derived table = Derived::cast(obj);
    int start_offset = DataTableStartOffset();
    int end_offset = table.GetBucketsStartOffset();
    IteratePointers(obj, start_offset, end_offset, v);
  }

  static inline int SizeOf(Map map, HeapObject obj) {
    Derived table = Derived::cast(obj);
    return table.SizeFor(table.Capacity());
  }
};

class ByteArray::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map map, HeapObject obj, int offset) { return false; }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map map, HeapObject obj, int object_size,
                                 ObjectVisitor* v) {}

  static inline int SizeOf(Map map, HeapObject obj) {
    return ByteArray::SizeFor(ByteArray::cast(obj).synchronized_length());
  }
};

class BytecodeArray::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map map, HeapObject obj, int offset) {
    return offset >= kConstantPoolOffset &&
           offset <= kSourcePositionTableOffset;
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map map, HeapObject obj, int object_size,
                                 ObjectVisitor* v) {
    IteratePointer(obj, kConstantPoolOffset, v);
    IteratePointer(obj, kHandlerTableOffset, v);
    IteratePointer(obj, kSourcePositionTableOffset, v);
  }

  static inline int SizeOf(Map map, HeapObject obj) {
    return BytecodeArray::SizeFor(
        BytecodeArray::cast(obj).synchronized_length());
  }
};

class BigInt::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map map, HeapObject obj, int offset) { return false; }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map map, HeapObject obj, int object_size,
                                 ObjectVisitor* v) {}

  static inline int SizeOf(Map map, HeapObject obj) {
    return BigInt::SizeFor(BigInt::cast(obj).synchronized_length());
  }
};

class FixedDoubleArray::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map map, HeapObject obj, int offset) { return false; }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map map, HeapObject obj, int object_size,
                                 ObjectVisitor* v) {}

  static inline int SizeOf(Map map, HeapObject obj) {
    return FixedDoubleArray::SizeFor(
        FixedDoubleArray::cast(obj).synchronized_length());
  }
};

class FeedbackMetadata::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map map, HeapObject obj, int offset) { return false; }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map map, HeapObject obj, int object_size,
                                 ObjectVisitor* v) {}

  static inline int SizeOf(Map map, HeapObject obj) {
    return FeedbackMetadata::SizeFor(
        FeedbackMetadata::cast(obj).synchronized_slot_count());
  }
};

class FeedbackVector::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map map, HeapObject obj, int offset) {
    return offset == kSharedFunctionInfoOffset ||
           offset == kOptimizedCodeWeakOrSmiOffset ||
           offset == kClosureFeedbackCellArrayOffset ||
           offset >= kFeedbackSlotsOffset;
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map map, HeapObject obj, int object_size,
                                 ObjectVisitor* v) {
    IteratePointer(obj, kSharedFunctionInfoOffset, v);
    IterateMaybeWeakPointer(obj, kOptimizedCodeWeakOrSmiOffset, v);
    IteratePointer(obj, kClosureFeedbackCellArrayOffset, v);
    IterateMaybeWeakPointers(obj, kFeedbackSlotsOffset, object_size, v);
  }

  static inline int SizeOf(Map map, HeapObject obj) {
    return FeedbackVector::SizeFor(FeedbackVector::cast(obj).length());
  }
};

class PreparseData::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map map, HeapObject obj, int offset) {
    return offset >= PreparseData::cast(obj).inner_start_offset();
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map map, HeapObject obj, int object_size,
                                 ObjectVisitor* v) {
    PreparseData data = PreparseData::cast(obj);
    int start_offset = data.inner_start_offset();
    int end_offset = start_offset + data.children_length() * kTaggedSize;
    IteratePointers(obj, start_offset, end_offset, v);
  }

  static inline int SizeOf(Map map, HeapObject obj) {
    PreparseData data = PreparseData::cast(obj);
    return PreparseData::SizeFor(data.data_length(), data.children_length());
  }
};

class PrototypeInfo::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map map, HeapObject obj, int offset) {
    return offset >= HeapObject::kHeaderSize;
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map map, HeapObject obj, int object_size,
                                 ObjectVisitor* v) {
    IteratePointers(obj, HeapObject::kHeaderSize, kObjectCreateMapOffset, v);
    IterateMaybeWeakPointer(obj, kObjectCreateMapOffset, v);
    IteratePointers(obj, kObjectCreateMapOffset + kTaggedSize, object_size, v);
  }

  static inline int SizeOf(Map map, HeapObject obj) {
    return obj.SizeFromMap(map);
  }
};

class JSWeakCollection::BodyDescriptorImpl final : public BodyDescriptorBase {
 public:
  STATIC_ASSERT(kTableOffset + kTaggedSize == kSizeOfAllWeakCollections);

  static bool IsValidSlot(Map map, HeapObject obj, int offset) {
    return IsValidJSObjectSlotImpl(map, obj, offset);
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map map, HeapObject obj, int object_size,
                                 ObjectVisitor* v) {
    IterateJSObjectBodyImpl(map, obj, kPropertiesOrHashOffset, object_size, v);
  }

  static inline int SizeOf(Map map, HeapObject object) {
    return map.instance_size();
  }
};

class Foreign::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map map, HeapObject obj, int offset) { return false; }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map map, HeapObject obj, int object_size,
                                 ObjectVisitor* v) {
    v->VisitExternalReference(
        Foreign::cast(obj), reinterpret_cast<Address*>(
                                obj.RawField(kForeignAddressOffset).address()));
  }

  static inline int SizeOf(Map map, HeapObject object) { return kSize; }
};

class ExternalOneByteString::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map map, HeapObject obj, int offset) { return false; }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map map, HeapObject obj, int object_size,
                                 ObjectVisitor* v) {}

  static inline int SizeOf(Map map, HeapObject object) { return kSize; }
};

class ExternalTwoByteString::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map map, HeapObject obj, int offset) { return false; }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map map, HeapObject obj, int object_size,
                                 ObjectVisitor* v) {}

  static inline int SizeOf(Map map, HeapObject object) { return kSize; }
};

class Code::BodyDescriptor final : public BodyDescriptorBase {
 public:
  STATIC_ASSERT(kRelocationInfoOffset + kTaggedSize ==
                kDeoptimizationDataOffset);
  STATIC_ASSERT(kDeoptimizationDataOffset + kTaggedSize ==
                kSourcePositionTableOffset);
  STATIC_ASSERT(kSourcePositionTableOffset + kTaggedSize ==
                kCodeDataContainerOffset);
  STATIC_ASSERT(kCodeDataContainerOffset + kTaggedSize == kDataStart);

  static bool IsValidSlot(Map map, HeapObject obj, int offset) {
    // Slots in code can't be invalid because we never trim code objects.
    return true;
  }

  static constexpr int kRelocModeMask =
      RelocInfo::ModeMask(RelocInfo::CODE_TARGET) |
      RelocInfo::ModeMask(RelocInfo::RELATIVE_CODE_TARGET) |
      RelocInfo::ModeMask(RelocInfo::FULL_EMBEDDED_OBJECT) |
      RelocInfo::ModeMask(RelocInfo::COMPRESSED_EMBEDDED_OBJECT) |
      RelocInfo::ModeMask(RelocInfo::EXTERNAL_REFERENCE) |
      RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE) |
      RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE_ENCODED) |
      RelocInfo::ModeMask(RelocInfo::OFF_HEAP_TARGET) |
      RelocInfo::ModeMask(RelocInfo::RUNTIME_ENTRY);

  template <typename ObjectVisitor>
  static inline void IterateBody(Map map, HeapObject obj, ObjectVisitor* v) {
    // GC does not visit data/code in the header and in the body directly.
    IteratePointers(obj, kRelocationInfoOffset, kDataStart, v);

    RelocIterator it(Code::cast(obj), kRelocModeMask);
    v->VisitRelocInfo(&it);
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map map, HeapObject obj, int object_size,
                                 ObjectVisitor* v) {
    IterateBody(map, obj, v);
  }

  static inline int SizeOf(Map map, HeapObject object) {
    return Code::unchecked_cast(object).CodeSize();
  }
};

class SeqOneByteString::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map map, HeapObject obj, int offset) { return false; }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map map, HeapObject obj, int object_size,
                                 ObjectVisitor* v) {}

  static inline int SizeOf(Map map, HeapObject obj) {
    SeqOneByteString string = SeqOneByteString::cast(obj);
    return string.SizeFor(string.synchronized_length());
  }
};

class SeqTwoByteString::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map map, HeapObject obj, int offset) { return false; }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map map, HeapObject obj, int object_size,
                                 ObjectVisitor* v) {}

  static inline int SizeOf(Map map, HeapObject obj) {
    SeqTwoByteString string = SeqTwoByteString::cast(obj);
    return string.SizeFor(string.synchronized_length());
  }
};

class WasmInstanceObject::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map map, HeapObject obj, int offset) {
    SLOW_DCHECK(std::is_sorted(std::begin(kTaggedFieldOffsets),
                               std::end(kTaggedFieldOffsets)));
    STATIC_ASSERT(sizeof(*kTaggedFieldOffsets) == sizeof(uint16_t));
    if (offset < int{8 * sizeof(*kTaggedFieldOffsets)} &&
        std::binary_search(std::begin(kTaggedFieldOffsets),
                           std::end(kTaggedFieldOffsets),
                           static_cast<uint16_t>(offset))) {
      return true;
    }
    return IsValidJSObjectSlotImpl(map, obj, offset);
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map map, HeapObject obj, int object_size,
                                 ObjectVisitor* v) {
    IteratePointers(obj, kPropertiesOrHashOffset, JSObject::kHeaderSize, v);
    for (uint16_t offset : kTaggedFieldOffsets) {
      IteratePointer(obj, offset, v);
    }
    IterateJSObjectBodyImpl(map, obj, kSize, object_size, v);
  }

  static inline int SizeOf(Map map, HeapObject object) {
    return map.instance_size();
  }
};

class Map::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map map, HeapObject obj, int offset) {
    static_assert(
        Map::kEndOfStrongFieldsOffset == Map::kStartOfWeakFieldsOffset,
        "Leverage that weak fields directly follow strong fields for the "
        "check below");
    return offset >= Map::kStartOfStrongFieldsOffset &&
           offset < Map::kEndOfWeakFieldsOffset;
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map map, HeapObject obj, int object_size,
                                 ObjectVisitor* v) {
    IteratePointers(obj, Map::kStartOfStrongFieldsOffset,
                    Map::kEndOfStrongFieldsOffset, v);
    IterateMaybeWeakPointer(obj, kTransitionsOrPrototypeInfoOffset, v);
  }

  static inline int SizeOf(Map map, HeapObject obj) { return Map::kSize; }
};

class DataHandler::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map map, HeapObject obj, int offset) {
    return offset >= HeapObject::kHeaderSize;
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map map, HeapObject obj, int object_size,
                                 ObjectVisitor* v) {
    static_assert(kSmiHandlerOffset < kData1Offset,
                  "Field order must be in sync with this iteration code");
    static_assert(kData1Offset < kSizeWithData1,
                  "Field order must be in sync with this iteration code");
    IteratePointers(obj, kSmiHandlerOffset, kData1Offset, v);
    IterateMaybeWeakPointers(obj, kData1Offset, object_size, v);
  }

  static inline int SizeOf(Map map, HeapObject object) {
    return object.SizeFromMap(map);
  }
};

class NativeContext::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map map, HeapObject obj, int offset) {
    return offset < NativeContext::kEndOfTaggedFieldsOffset;
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map map, HeapObject obj, int object_size,
                                 ObjectVisitor* v) {
    IteratePointers(obj, NativeContext::kStartOfStrongFieldsOffset,
                    NativeContext::kEndOfStrongFieldsOffset, v);
    IterateCustomWeakPointers(obj, NativeContext::kStartOfWeakFieldsOffset,
                              NativeContext::kEndOfWeakFieldsOffset, v);
  }

  static inline int SizeOf(Map map, HeapObject object) {
    return NativeContext::kSize;
  }
};

class CodeDataContainer::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map map, HeapObject obj, int offset) {
    return offset >= CodeDataContainer::kHeaderSize &&
           offset < CodeDataContainer::kSize;
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map map, HeapObject obj, int object_size,
                                 ObjectVisitor* v) {
    IteratePointers(obj, CodeDataContainer::kHeaderSize,
                    CodeDataContainer::kPointerFieldsStrongEndOffset, v);
    IterateCustomWeakPointers(
        obj, CodeDataContainer::kPointerFieldsStrongEndOffset,
        CodeDataContainer::kPointerFieldsWeakEndOffset, v);
  }

  static inline int SizeOf(Map map, HeapObject object) {
    return CodeDataContainer::kSize;
  }
};

class EmbedderDataArray::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map map, HeapObject obj, int offset) {
#ifdef V8_COMPRESS_POINTERS
    STATIC_ASSERT(kEmbedderDataSlotSize == 2 * kTaggedSize);
    STATIC_ASSERT(base::bits::IsPowerOfTwo(kEmbedderDataSlotSize));
    return (offset < EmbedderDataArray::kHeaderSize) ||
           (((offset - EmbedderDataArray::kHeaderSize) &
             (kEmbedderDataSlotSize - 1)) ==
            EmbedderDataSlot::kTaggedPayloadOffset);
#else
    STATIC_ASSERT(kEmbedderDataSlotSize == kTaggedSize);
    // We store raw aligned pointers as Smis, so it's safe to iterate the whole
    // array.
    return true;
#endif
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map map, HeapObject obj, int object_size,
                                 ObjectVisitor* v) {
#ifdef V8_COMPRESS_POINTERS
    STATIC_ASSERT(kEmbedderDataSlotSize == 2 * kTaggedSize);
    // Iterate only tagged payload of the embedder slots and skip raw payload.
    for (int offset = EmbedderDataArray::OffsetOfElementAt(0) +
                      EmbedderDataSlot::kTaggedPayloadOffset;
         offset < object_size; offset += kEmbedderDataSlotSize) {
      IteratePointer(obj, offset, v);
    }
#else
    // We store raw aligned pointers as Smis, so it's safe to iterate the whole
    // array.
    STATIC_ASSERT(kEmbedderDataSlotSize == kTaggedSize);
    IteratePointers(obj, EmbedderDataArray::kHeaderSize, object_size, v);
#endif
  }

  static inline int SizeOf(Map map, HeapObject object) {
    return object.SizeFromMap(map);
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
    case EMBEDDER_DATA_ARRAY_TYPE:
      return Op::template apply<EmbedderDataArray::BodyDescriptor>(p1, p2, p3,
                                                                   p4);
    case FIXED_ARRAY_TYPE:
    case OBJECT_BOILERPLATE_DESCRIPTION_TYPE:
    case CLOSURE_FEEDBACK_CELL_ARRAY_TYPE:
    case HASH_TABLE_TYPE:
    case ORDERED_HASH_MAP_TYPE:
    case ORDERED_HASH_SET_TYPE:
    case ORDERED_NAME_DICTIONARY_TYPE:
    case NAME_DICTIONARY_TYPE:
    case GLOBAL_DICTIONARY_TYPE:
    case NUMBER_DICTIONARY_TYPE:
    case SIMPLE_NUMBER_DICTIONARY_TYPE:
    case STRING_TABLE_TYPE:
    case SCOPE_INFO_TYPE:
    case SCRIPT_CONTEXT_TABLE_TYPE:
      return Op::template apply<FixedArray::BodyDescriptor>(p1, p2, p3, p4);
    case EPHEMERON_HASH_TABLE_TYPE:
      return Op::template apply<EphemeronHashTable::BodyDescriptor>(p1, p2, p3,
                                                                    p4);
    case AWAIT_CONTEXT_TYPE:
    case BLOCK_CONTEXT_TYPE:
    case CATCH_CONTEXT_TYPE:
    case DEBUG_EVALUATE_CONTEXT_TYPE:
    case EVAL_CONTEXT_TYPE:
    case FUNCTION_CONTEXT_TYPE:
    case MODULE_CONTEXT_TYPE:
    case SCRIPT_CONTEXT_TYPE:
    case WITH_CONTEXT_TYPE:
      return Op::template apply<Context::BodyDescriptor>(p1, p2, p3, p4);
    case NATIVE_CONTEXT_TYPE:
      return Op::template apply<NativeContext::BodyDescriptor>(p1, p2, p3, p4);
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
    case JS_ARGUMENTS_OBJECT_TYPE:
    case JS_ASYNC_FROM_SYNC_ITERATOR_TYPE:
    case JS_PROMISE_TYPE:
    case JS_CONTEXT_EXTENSION_OBJECT_TYPE:
    case JS_GENERATOR_OBJECT_TYPE:
    case JS_ASYNC_FUNCTION_OBJECT_TYPE:
    case JS_ASYNC_GENERATOR_OBJECT_TYPE:
    case JS_PRIMITIVE_WRAPPER_TYPE:
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
    case JS_REG_EXP_STRING_ITERATOR_TYPE:
    case JS_REG_EXP_TYPE:
    case JS_GLOBAL_PROXY_TYPE:
    case JS_GLOBAL_OBJECT_TYPE:
    case JS_API_OBJECT_TYPE:
    case JS_SPECIAL_API_OBJECT_TYPE:
    case JS_MESSAGE_OBJECT_TYPE:
    case JS_BOUND_FUNCTION_TYPE:
    case JS_FINALIZATION_GROUP_CLEANUP_ITERATOR_TYPE:
    case JS_FINALIZATION_GROUP_TYPE:
#ifdef V8_INTL_SUPPORT
    case JS_V8_BREAK_ITERATOR_TYPE:
    case JS_COLLATOR_TYPE:
    case JS_DATE_TIME_FORMAT_TYPE:
    case JS_LIST_FORMAT_TYPE:
    case JS_LOCALE_TYPE:
    case JS_NUMBER_FORMAT_TYPE:
    case JS_PLURAL_RULES_TYPE:
    case JS_RELATIVE_TIME_FORMAT_TYPE:
    case JS_SEGMENT_ITERATOR_TYPE:
    case JS_SEGMENTER_TYPE:
#endif  // V8_INTL_SUPPORT
    case WASM_EXCEPTION_OBJECT_TYPE:
    case WASM_GLOBAL_OBJECT_TYPE:
    case WASM_MEMORY_OBJECT_TYPE:
    case WASM_MODULE_OBJECT_TYPE:
    case WASM_TABLE_OBJECT_TYPE:
      return Op::template apply<JSObject::BodyDescriptor>(p1, p2, p3, p4);
    case WASM_INSTANCE_OBJECT_TYPE:
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
    case WEAK_CELL_TYPE:
      return Op::template apply<WeakCell::BodyDescriptor>(p1, p2, p3, p4);
    case JS_WEAK_REF_TYPE:
      return Op::template apply<JSWeakRef::BodyDescriptor>(p1, p2, p3, p4);
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
    case SMALL_ORDERED_NAME_DICTIONARY_TYPE:
      return Op::template apply<
          SmallOrderedHashTable<SmallOrderedNameDictionary>::BodyDescriptor>(
          p1, p2, p3, p4);
    case CODE_DATA_CONTAINER_TYPE:
      return Op::template apply<CodeDataContainer::BodyDescriptor>(p1, p2, p3,
                                                                   p4);
    case PREPARSE_DATA_TYPE:
      return Op::template apply<PreparseData::BodyDescriptor>(p1, p2, p3, p4);
    case UNCOMPILED_DATA_WITHOUT_PREPARSE_DATA_TYPE:
      return Op::template apply<
          UncompiledDataWithoutPreparseData::BodyDescriptor>(p1, p2, p3, p4);
    case UNCOMPILED_DATA_WITH_PREPARSE_DATA_TYPE:
      return Op::template apply<UncompiledDataWithPreparseData::BodyDescriptor>(
          p1, p2, p3, p4);
    case HEAP_NUMBER_TYPE:
    case FILLER_TYPE:
    case BYTE_ARRAY_TYPE:
    case FREE_SPACE_TYPE:
    case BIGINT_TYPE:
      return ReturnType();

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
      } else if (type == WASM_CAPI_FUNCTION_DATA_TYPE) {
        return Op::template apply<WasmCapiFunctionData::BodyDescriptor>(p1, p2,
                                                                        p3, p4);
      } else if (type == WASM_INDIRECT_FUNCTION_TABLE_TYPE) {
        return Op::template apply<WasmIndirectFunctionTable::BodyDescriptor>(
            p1, p2, p3, p4);
      } else {
        return Op::template apply<StructBodyDescriptor>(p1, p2, p3, p4);
      }
    case CALL_HANDLER_INFO_TYPE:
      return Op::template apply<StructBodyDescriptor>(p1, p2, p3, p4);
    case LOAD_HANDLER_TYPE:
    case STORE_HANDLER_TYPE:
      return Op::template apply<DataHandler::BodyDescriptor>(p1, p2, p3, p4);
    case SOURCE_TEXT_MODULE_TYPE:
      return Op::template apply<SourceTextModule::BodyDescriptor>(p1, p2, p3,
                                                                  p4);
    case SYNTHETIC_MODULE_TYPE:
      return Op::template apply<SyntheticModule::BodyDescriptor>(p1, p2, p3,
                                                                 p4);
    default:
      PrintF("Unknown type: %d\n", type);
      UNREACHABLE();
  }
}

template <typename ObjectVisitor>
void HeapObject::IterateFast(ObjectVisitor* v) {
  BodyDescriptorBase::IteratePointer(*this, kMapOffset, v);
  IterateBodyFast(v);
}

template <typename ObjectVisitor>
void HeapObject::IterateBodyFast(ObjectVisitor* v) {
  Map m = map();
  IterateBodyFast(m, SizeFromMap(m), v);
}

struct CallIterateBody {
  template <typename BodyDescriptor, typename ObjectVisitor>
  static void apply(Map map, HeapObject obj, int object_size,
                    ObjectVisitor* v) {
    BodyDescriptor::IterateBody(map, obj, object_size, v);
  }
};

template <typename ObjectVisitor>
void HeapObject::IterateBodyFast(Map map, int object_size, ObjectVisitor* v) {
  BodyDescriptorApply<CallIterateBody, void>(map.instance_type(), map, *this,
                                             object_size, v);
}

class EphemeronHashTable::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(Map map, HeapObject obj, int offset) {
    return (offset >= EphemeronHashTable::kHeaderSize);
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map map, HeapObject obj, int object_size,
                                 ObjectVisitor* v) {
    int entries_start = EphemeronHashTable::kHeaderSize +
                        EphemeronHashTable::kElementsStartIndex * kTaggedSize;
    IteratePointers(obj, EphemeronHashTable::kHeaderSize, entries_start, v);
    EphemeronHashTable table = EphemeronHashTable::unchecked_cast(obj);
    int entries = table.Capacity();
    for (int i = 0; i < entries; ++i) {
      const int key_index = EphemeronHashTable::EntryToIndex(i);
      const int value_index = EphemeronHashTable::EntryToValueIndex(i);
      IterateEphemeron(obj, i, OffsetOfElementAt(key_index),
                       OffsetOfElementAt(value_index), v);
    }
  }

  static inline int SizeOf(Map map, HeapObject object) {
    return object.SizeFromMap(map);
  }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_OBJECTS_BODY_DESCRIPTORS_INL_H_
