// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_BODY_DESCRIPTORS_INL_H_
#define V8_OBJECTS_BODY_DESCRIPTORS_INL_H_

#include "src/objects-body-descriptors.h"

namespace v8 {
namespace internal {

template <int start_offset>
int FlexibleBodyDescriptor<start_offset>::SizeOf(Map* map, HeapObject* object) {
  return object->SizeFromMap(map);
}


bool BodyDescriptorBase::IsValidSlotImpl(HeapObject* obj, int offset) {
  if (!FLAG_unbox_double_fields || obj->map()->HasFastPointerLayout()) {
    return true;
  } else {
    DCHECK(FLAG_unbox_double_fields);
    DCHECK(IsAligned(offset, kPointerSize));

    LayoutDescriptorHelper helper(obj->map());
    DCHECK(!helper.all_fields_tagged());
    return helper.IsTagged(offset);
  }
}

template <typename ObjectVisitor>
void BodyDescriptorBase::IterateBodyImpl(HeapObject* obj, int start_offset,
                                         int end_offset, ObjectVisitor* v) {
  if (!FLAG_unbox_double_fields || obj->map()->HasFastPointerLayout()) {
    IteratePointers(obj, start_offset, end_offset, v);
  } else {
    DCHECK(FLAG_unbox_double_fields);
    DCHECK(IsAligned(start_offset, kPointerSize) &&
           IsAligned(end_offset, kPointerSize));

    LayoutDescriptorHelper helper(obj->map());
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


template <typename StaticVisitor>
void BodyDescriptorBase::IterateBodyImpl(Heap* heap, HeapObject* obj,
                                         int start_offset, int end_offset) {
  if (!FLAG_unbox_double_fields || obj->map()->HasFastPointerLayout()) {
    IteratePointers<StaticVisitor>(heap, obj, start_offset, end_offset);
  } else {
    DCHECK(FLAG_unbox_double_fields);
    DCHECK(IsAligned(start_offset, kPointerSize) &&
           IsAligned(end_offset, kPointerSize));

    LayoutDescriptorHelper helper(obj->map());
    DCHECK(!helper.all_fields_tagged());
    for (int offset = start_offset; offset < end_offset;) {
      int end_of_region_offset;
      if (helper.IsTagged(offset, end_offset, &end_of_region_offset)) {
        IteratePointers<StaticVisitor>(heap, obj, offset, end_of_region_offset);
      }
      offset = end_of_region_offset;
    }
  }
}


template <typename ObjectVisitor>
DISABLE_CFI_PERF
void BodyDescriptorBase::IteratePointers(HeapObject* obj, int start_offset,
                                         int end_offset, ObjectVisitor* v) {
  v->VisitPointers(HeapObject::RawField(obj, start_offset),
                   HeapObject::RawField(obj, end_offset));
}


template <typename StaticVisitor>
DISABLE_CFI_PERF
void BodyDescriptorBase::IteratePointers(Heap* heap, HeapObject* obj,
                                         int start_offset, int end_offset) {
  StaticVisitor::VisitPointers(heap, obj,
                               HeapObject::RawField(obj, start_offset),
                               HeapObject::RawField(obj, end_offset));
}


template <typename ObjectVisitor>
void BodyDescriptorBase::IteratePointer(HeapObject* obj, int offset,
                                        ObjectVisitor* v) {
  v->VisitPointer(HeapObject::RawField(obj, offset));
}


template <typename StaticVisitor>
void BodyDescriptorBase::IteratePointer(Heap* heap, HeapObject* obj,
                                        int offset) {
  StaticVisitor::VisitPointer(heap, obj, HeapObject::RawField(obj, offset));
}


// Iterates the function object according to the visiting policy.
template <JSFunction::BodyVisitingPolicy body_visiting_policy>
class JSFunction::BodyDescriptorImpl final : public BodyDescriptorBase {
 public:
  STATIC_ASSERT(kNonWeakFieldsEndOffset == kCodeEntryOffset);
  STATIC_ASSERT(kCodeEntryOffset + kPointerSize == kNextFunctionLinkOffset);
  STATIC_ASSERT(kNextFunctionLinkOffset + kPointerSize == kSize);

  static bool IsValidSlot(HeapObject* obj, int offset) {
    if (offset < kSize) return true;
    return IsValidSlotImpl(obj, offset);
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(HeapObject* obj, int object_size,
                                 ObjectVisitor* v) {
    IteratePointers(obj, kPropertiesOffset, kNonWeakFieldsEndOffset, v);

    if (body_visiting_policy & kVisitCodeEntry) {
      v->VisitCodeEntry(obj->address() + kCodeEntryOffset);
    }

    if (body_visiting_policy & kVisitNextFunction) {
      IteratePointers(obj, kNextFunctionLinkOffset, kSize, v);
    }
    IterateBodyImpl(obj, kSize, object_size, v);
  }

  template <typename StaticVisitor>
  static inline void IterateBody(HeapObject* obj, int object_size) {
    Heap* heap = obj->GetHeap();
    IteratePointers<StaticVisitor>(heap, obj, kPropertiesOffset,
                                   kNonWeakFieldsEndOffset);

    if (body_visiting_policy & kVisitCodeEntry) {
      StaticVisitor::VisitCodeEntry(heap, obj,
                                    obj->address() + kCodeEntryOffset);
    }

    if (body_visiting_policy & kVisitNextFunction) {
      IteratePointers<StaticVisitor>(heap, obj, kNextFunctionLinkOffset, kSize);
    }
    IterateBodyImpl<StaticVisitor>(heap, obj, kSize, object_size);
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

  static bool IsValidSlot(HeapObject* obj, int offset) {
    if (offset < kBackingStoreOffset) return true;
    if (offset < kSize) return false;
    return IsValidSlotImpl(obj, offset);
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(HeapObject* obj, int object_size,
                                 ObjectVisitor* v) {
    IteratePointers(obj, kPropertiesOffset, kBackingStoreOffset, v);
    IterateBodyImpl(obj, kSize, object_size, v);
  }

  template <typename StaticVisitor>
  static inline void IterateBody(HeapObject* obj, int object_size) {
    Heap* heap = obj->GetHeap();
    IteratePointers<StaticVisitor>(heap, obj, kPropertiesOffset,
                                   kBackingStoreOffset);
    IterateBodyImpl<StaticVisitor>(heap, obj, kSize, object_size);
  }

  static inline int SizeOf(Map* map, HeapObject* object) {
    return map->instance_size();
  }
};


class BytecodeArray::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(HeapObject* obj, int offset) {
    return offset >= kConstantPoolOffset &&
           offset <= kSourcePositionTableOffset;
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(HeapObject* obj, int object_size,
                                 ObjectVisitor* v) {
    IteratePointer(obj, kConstantPoolOffset, v);
    IteratePointer(obj, kHandlerTableOffset, v);
    IteratePointer(obj, kSourcePositionTableOffset, v);
  }

  template <typename StaticVisitor>
  static inline void IterateBody(HeapObject* obj, int object_size) {
    Heap* heap = obj->GetHeap();
    IteratePointer<StaticVisitor>(heap, obj, kConstantPoolOffset);
    IteratePointer<StaticVisitor>(heap, obj, kHandlerTableOffset);
    IteratePointer<StaticVisitor>(heap, obj, kSourcePositionTableOffset);
  }

  static inline int SizeOf(Map* map, HeapObject* obj) {
    return reinterpret_cast<BytecodeArray*>(obj)->BytecodeArraySize();
  }
};


class FixedTypedArrayBase::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(HeapObject* obj, int offset) {
    return offset == kBasePointerOffset;
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(HeapObject* obj, int object_size,
                                 ObjectVisitor* v) {
    IteratePointer(obj, kBasePointerOffset, v);
  }

  template <typename StaticVisitor>
  static inline void IterateBody(HeapObject* obj, int object_size) {
    Heap* heap = obj->GetHeap();
    IteratePointer<StaticVisitor>(heap, obj, kBasePointerOffset);
  }

  static inline int SizeOf(Map* map, HeapObject* object) {
    return reinterpret_cast<FixedTypedArrayBase*>(object)->size();
  }
};


template <JSWeakCollection::BodyVisitingPolicy body_visiting_policy>
class JSWeakCollection::BodyDescriptorImpl final : public BodyDescriptorBase {
 public:
  STATIC_ASSERT(kTableOffset + kPointerSize == kNextOffset);
  STATIC_ASSERT(kNextOffset + kPointerSize == kSize);

  static bool IsValidSlot(HeapObject* obj, int offset) {
    return IsValidSlotImpl(obj, offset);
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(HeapObject* obj, int object_size,
                                 ObjectVisitor* v) {
    if (body_visiting_policy == kVisitStrong) {
      IterateBodyImpl(obj, kPropertiesOffset, object_size, v);
    } else {
      IteratePointers(obj, kPropertiesOffset, kTableOffset, v);
      IterateBodyImpl(obj, kSize, object_size, v);
    }
  }

  template <typename StaticVisitor>
  static inline void IterateBody(HeapObject* obj, int object_size) {
    Heap* heap = obj->GetHeap();
    if (body_visiting_policy == kVisitStrong) {
      IterateBodyImpl<StaticVisitor>(heap, obj, kPropertiesOffset, object_size);
    } else {
      IteratePointers<StaticVisitor>(heap, obj, kPropertiesOffset,
                                     kTableOffset);
      IterateBodyImpl<StaticVisitor>(heap, obj, kSize, object_size);
    }
  }

  static inline int SizeOf(Map* map, HeapObject* object) {
    return map->instance_size();
  }
};


class Foreign::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(HeapObject* obj, int offset) { return false; }

  template <typename ObjectVisitor>
  static inline void IterateBody(HeapObject* obj, int object_size,
                                 ObjectVisitor* v) {
    v->VisitExternalReference(reinterpret_cast<Address*>(
        HeapObject::RawField(obj, kForeignAddressOffset)));
  }

  template <typename StaticVisitor>
  static inline void IterateBody(HeapObject* obj, int object_size) {
    StaticVisitor::VisitExternalReference(reinterpret_cast<Address*>(
        HeapObject::RawField(obj, kForeignAddressOffset)));
  }

  static inline int SizeOf(Map* map, HeapObject* object) { return kSize; }
};


class ExternalOneByteString::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(HeapObject* obj, int offset) { return false; }

  template <typename ObjectVisitor>
  static inline void IterateBody(HeapObject* obj, int object_size,
                                 ObjectVisitor* v) {
    typedef v8::String::ExternalOneByteStringResource Resource;
    v->VisitExternalOneByteString(reinterpret_cast<Resource**>(
        HeapObject::RawField(obj, kResourceOffset)));
  }

  template <typename StaticVisitor>
  static inline void IterateBody(HeapObject* obj, int object_size) {
    typedef v8::String::ExternalOneByteStringResource Resource;
    StaticVisitor::VisitExternalOneByteString(reinterpret_cast<Resource**>(
        HeapObject::RawField(obj, kResourceOffset)));
  }

  static inline int SizeOf(Map* map, HeapObject* object) { return kSize; }
};


class ExternalTwoByteString::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static bool IsValidSlot(HeapObject* obj, int offset) { return false; }

  template <typename ObjectVisitor>
  static inline void IterateBody(HeapObject* obj, int object_size,
                                 ObjectVisitor* v) {
    typedef v8::String::ExternalStringResource Resource;
    v->VisitExternalTwoByteString(reinterpret_cast<Resource**>(
        HeapObject::RawField(obj, kResourceOffset)));
  }

  template <typename StaticVisitor>
  static inline void IterateBody(HeapObject* obj, int object_size) {
    typedef v8::String::ExternalStringResource Resource;
    StaticVisitor::VisitExternalTwoByteString(reinterpret_cast<Resource**>(
        HeapObject::RawField(obj, kResourceOffset)));
  }

  static inline int SizeOf(Map* map, HeapObject* object) { return kSize; }
};


class Code::BodyDescriptor final : public BodyDescriptorBase {
 public:
  STATIC_ASSERT(kRelocationInfoOffset + kPointerSize == kHandlerTableOffset);
  STATIC_ASSERT(kHandlerTableOffset + kPointerSize ==
                kDeoptimizationDataOffset);
  STATIC_ASSERT(kDeoptimizationDataOffset + kPointerSize ==
                kSourcePositionTableOffset);
  STATIC_ASSERT(kSourcePositionTableOffset + kPointerSize ==
                kTypeFeedbackInfoOffset);
  STATIC_ASSERT(kTypeFeedbackInfoOffset + kPointerSize == kNextCodeLinkOffset);

  static bool IsValidSlot(HeapObject* obj, int offset) {
    // Slots in code can't be invalid because we never trim code objects.
    return true;
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(HeapObject* obj, ObjectVisitor* v) {
    int mode_mask = RelocInfo::kCodeTargetMask |
                    RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT) |
                    RelocInfo::ModeMask(RelocInfo::CELL) |
                    RelocInfo::ModeMask(RelocInfo::EXTERNAL_REFERENCE) |
                    RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE) |
                    RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE_ENCODED) |
                    RelocInfo::ModeMask(RelocInfo::RUNTIME_ENTRY) |
                    RelocInfo::kDebugBreakSlotMask;

    IteratePointers(obj, kRelocationInfoOffset, kNextCodeLinkOffset, v);
    v->VisitNextCodeLink(HeapObject::RawField(obj, kNextCodeLinkOffset));

    RelocIterator it(reinterpret_cast<Code*>(obj), mode_mask);
    Isolate* isolate = obj->GetIsolate();
    for (; !it.done(); it.next()) {
      it.rinfo()->Visit(isolate, v);
    }
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(HeapObject* obj, int object_size,
                                 ObjectVisitor* v) {
    IterateBody(obj, v);
  }

  template <typename StaticVisitor>
  static inline void IterateBody(HeapObject* obj) {
    int mode_mask = RelocInfo::kCodeTargetMask |
                    RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT) |
                    RelocInfo::ModeMask(RelocInfo::CELL) |
                    RelocInfo::ModeMask(RelocInfo::EXTERNAL_REFERENCE) |
                    RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE) |
                    RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE_ENCODED) |
                    RelocInfo::ModeMask(RelocInfo::RUNTIME_ENTRY) |
                    RelocInfo::kDebugBreakSlotMask;

    Heap* heap = obj->GetHeap();
    IteratePointers<StaticVisitor>(heap, obj, kRelocationInfoOffset,
                                   kNextCodeLinkOffset);
    StaticVisitor::VisitNextCodeLink(
        heap, HeapObject::RawField(obj, kNextCodeLinkOffset));

    RelocIterator it(reinterpret_cast<Code*>(obj), mode_mask);
    for (; !it.done(); it.next()) {
      it.rinfo()->template Visit<StaticVisitor>(heap);
    }
  }

  template <typename StaticVisitor>
  static inline void IterateBody(HeapObject* obj, int object_size) {
    IterateBody<StaticVisitor>(obj);
  }

  static inline int SizeOf(Map* map, HeapObject* object) {
    return reinterpret_cast<Code*>(object)->CodeSize();
  }
};


template <typename Op, typename ReturnType, typename T1, typename T2,
          typename T3>
ReturnType BodyDescriptorApply(InstanceType type, T1 p1, T2 p2, T3 p3) {
  if (type < FIRST_NONSTRING_TYPE) {
    switch (type & kStringRepresentationMask) {
      case kSeqStringTag:
        return ReturnType();
      case kConsStringTag:
        return Op::template apply<ConsString::BodyDescriptor>(p1, p2, p3);
      case kSlicedStringTag:
        return Op::template apply<SlicedString::BodyDescriptor>(p1, p2, p3);
      case kExternalStringTag:
        if ((type & kStringEncodingMask) == kOneByteStringTag) {
          return Op::template apply<ExternalOneByteString::BodyDescriptor>(
              p1, p2, p3);
        } else {
          return Op::template apply<ExternalTwoByteString::BodyDescriptor>(
              p1, p2, p3);
        }
    }
    UNREACHABLE();
    return ReturnType();
  }

  switch (type) {
    case FIXED_ARRAY_TYPE:
      return Op::template apply<FixedArray::BodyDescriptor>(p1, p2, p3);
    case FIXED_DOUBLE_ARRAY_TYPE:
      return ReturnType();
    case TRANSITION_ARRAY_TYPE:
      return Op::template apply<TransitionArray::BodyDescriptor>(p1, p2, p3);
    case JS_OBJECT_TYPE:
    case JS_ERROR_TYPE:
    case JS_ARGUMENTS_TYPE:
    case JS_PROMISE_TYPE:
    case JS_CONTEXT_EXTENSION_OBJECT_TYPE:
    case JS_GENERATOR_OBJECT_TYPE:
    case JS_VALUE_TYPE:
    case JS_DATE_TYPE:
    case JS_ARRAY_TYPE:
    case JS_TYPED_ARRAY_TYPE:
    case JS_DATA_VIEW_TYPE:
    case JS_SET_TYPE:
    case JS_MAP_TYPE:
    case JS_SET_ITERATOR_TYPE:
    case JS_MAP_ITERATOR_TYPE:
    case JS_STRING_ITERATOR_TYPE:
    case JS_REGEXP_TYPE:
    case JS_GLOBAL_PROXY_TYPE:
    case JS_GLOBAL_OBJECT_TYPE:
    case JS_API_OBJECT_TYPE:
    case JS_SPECIAL_API_OBJECT_TYPE:
    case JS_MESSAGE_OBJECT_TYPE:
    case JS_BOUND_FUNCTION_TYPE:
      return Op::template apply<JSObject::BodyDescriptor>(p1, p2, p3);
    case JS_WEAK_MAP_TYPE:
    case JS_WEAK_SET_TYPE:
      return Op::template apply<JSWeakCollection::BodyDescriptor>(p1, p2, p3);
    case JS_ARRAY_BUFFER_TYPE:
      return Op::template apply<JSArrayBuffer::BodyDescriptor>(p1, p2, p3);
    case JS_FUNCTION_TYPE:
      return Op::template apply<JSFunction::BodyDescriptor>(p1, p2, p3);
    case ODDBALL_TYPE:
      return Op::template apply<Oddball::BodyDescriptor>(p1, p2, p3);
    case JS_PROXY_TYPE:
      return Op::template apply<JSProxy::BodyDescriptor>(p1, p2, p3);
    case FOREIGN_TYPE:
      return Op::template apply<Foreign::BodyDescriptor>(p1, p2, p3);
    case MAP_TYPE:
      return Op::template apply<Map::BodyDescriptor>(p1, p2, p3);
    case CODE_TYPE:
      return Op::template apply<Code::BodyDescriptor>(p1, p2, p3);
    case CELL_TYPE:
      return Op::template apply<Cell::BodyDescriptor>(p1, p2, p3);
    case PROPERTY_CELL_TYPE:
      return Op::template apply<PropertyCell::BodyDescriptor>(p1, p2, p3);
    case WEAK_CELL_TYPE:
      return Op::template apply<WeakCell::BodyDescriptor>(p1, p2, p3);
    case SYMBOL_TYPE:
      return Op::template apply<Symbol::BodyDescriptor>(p1, p2, p3);
    case BYTECODE_ARRAY_TYPE:
      return Op::template apply<BytecodeArray::BodyDescriptor>(p1, p2, p3);

    case HEAP_NUMBER_TYPE:
    case MUTABLE_HEAP_NUMBER_TYPE:
    case SIMD128_VALUE_TYPE:
    case FILLER_TYPE:
    case BYTE_ARRAY_TYPE:
    case FREE_SPACE_TYPE:
      return ReturnType();

#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) \
  case FIXED_##TYPE##_ARRAY_TYPE:                       \
    return Op::template apply<FixedTypedArrayBase::BodyDescriptor>(p1, p2, p3);
      TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE

    case SHARED_FUNCTION_INFO_TYPE: {
      return Op::template apply<SharedFunctionInfo::BodyDescriptor>(p1, p2, p3);
    }

#define MAKE_STRUCT_CASE(NAME, Name, name) case NAME##_TYPE:
      STRUCT_LIST(MAKE_STRUCT_CASE)
#undef MAKE_STRUCT_CASE
      if (type == ALLOCATION_SITE_TYPE) {
        return Op::template apply<AllocationSite::BodyDescriptor>(p1, p2, p3);
      } else {
        return Op::template apply<StructBodyDescriptor>(p1, p2, p3);
      }
    default:
      PrintF("Unknown type: %d\n", type);
      UNREACHABLE();
      return ReturnType();
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
  IterateBodyFast(m->instance_type(), SizeFromMap(m), v);
}


struct CallIterateBody {
  template <typename BodyDescriptor, typename ObjectVisitor>
  static void apply(HeapObject* obj, int object_size, ObjectVisitor* v) {
    BodyDescriptor::IterateBody(obj, object_size, v);
  }
};

template <typename ObjectVisitor>
void HeapObject::IterateBodyFast(InstanceType type, int object_size,
                                 ObjectVisitor* v) {
  BodyDescriptorApply<CallIterateBody, void>(type, this, object_size, v);
}
}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_BODY_DESCRIPTORS_INL_H_
