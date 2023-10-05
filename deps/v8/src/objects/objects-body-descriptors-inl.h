// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_OBJECTS_BODY_DESCRIPTORS_INL_H_
#define V8_OBJECTS_OBJECTS_BODY_DESCRIPTORS_INL_H_

#include <algorithm>

#include "src/codegen/reloc-info.h"
#include "src/common/globals.h"
#include "src/ic/handler-configuration.h"
#include "src/objects/arguments-inl.h"
#include "src/objects/bigint.h"
#include "src/objects/call-site-info.h"
#include "src/objects/cell.h"
#include "src/objects/data-handler.h"
#include "src/objects/embedder-data-array-inl.h"
#include "src/objects/fixed-array.h"
#include "src/objects/foreign-inl.h"
#include "src/objects/free-space-inl.h"
#include "src/objects/hash-table.h"
#include "src/objects/heap-number.h"
#include "src/objects/instance-type.h"
#include "src/objects/js-array-buffer.h"
#include "src/objects/js-atomics-synchronization-inl.h"
#include "src/objects/js-collection.h"
#include "src/objects/js-weak-refs.h"
#include "src/objects/literal-objects.h"
#include "src/objects/megadom-handler-inl.h"
#include "src/objects/objects-body-descriptors.h"
#include "src/objects/ordered-hash-table-inl.h"
#include "src/objects/property-descriptor-object.h"
#include "src/objects/source-text-module.h"
#include "src/objects/swiss-name-dictionary-inl.h"
#include "src/objects/synthetic-module.h"
#include "src/objects/template-objects-inl.h"
#include "src/objects/torque-defined-classes-inl.h"
#include "src/objects/transitions.h"
#include "src/objects/turbofan-types-inl.h"
#include "src/objects/turboshaft-types-inl.h"

#if V8_ENABLE_WEBASSEMBLY
#include "src/wasm/wasm-objects-inl.h"
#endif  // V8_ENABLE_WEBASSEMBLY

namespace v8 {
namespace internal {

template <int start_offset>
int FlexibleBodyDescriptor<start_offset>::SizeOf(Tagged<Map> map,
                                                 Tagged<HeapObject> object) {
  return object->SizeFromMap(map);
}

template <int start_offset>
int FlexibleWeakBodyDescriptor<start_offset>::SizeOf(
    Tagged<Map> map, Tagged<HeapObject> object) {
  return object->SizeFromMap(map);
}

template <typename ObjectVisitor>
void BodyDescriptorBase::IterateJSObjectBodyImpl(Tagged<Map> map,
                                                 Tagged<HeapObject> obj,
                                                 int start_offset,
                                                 int end_offset,
                                                 ObjectVisitor* v) {
#ifdef V8_COMPRESS_POINTERS
  static_assert(kEmbedderDataSlotSize == 2 * kTaggedSize);
  int header_end_offset = JSObject::GetHeaderSize(map);
  int inobject_fields_start_offset = map->GetInObjectPropertyOffset(0);
  // We are always requested to process header and embedder fields.
  DCHECK_LE(inobject_fields_start_offset, end_offset);
  // Embedder fields are located between header and inobject properties.
  if (header_end_offset < inobject_fields_start_offset) {
    // There are embedder fields.
    DCHECK_EQ(header_end_offset, JSObject::GetEmbedderFieldsStartOffset(map));
    IteratePointers(obj, start_offset, header_end_offset, v);
    for (int offset = header_end_offset; offset < inobject_fields_start_offset;
         offset += kEmbedderDataSlotSize) {
      IteratePointer(obj, offset + EmbedderDataSlot::kTaggedPayloadOffset, v);
      v->VisitExternalPointer(
          obj,
          obj->RawExternalPointerField(
              offset + EmbedderDataSlot::kExternalPointerOffset),
          kEmbedderDataSlotPayloadTag);
    }
    // Proceed processing inobject properties.
    start_offset = inobject_fields_start_offset;
  }
#else
  // We store raw aligned pointers as Smis, so it's safe to iterate the whole
  // embedder field area as tagged slots.
  static_assert(kEmbedderDataSlotSize == kTaggedSize);
#endif
  IteratePointers(obj, start_offset, end_offset, v);
}

template <typename ObjectVisitor>
DISABLE_CFI_PERF void BodyDescriptorBase::IteratePointers(
    Tagged<HeapObject> obj, int start_offset, int end_offset,
    ObjectVisitor* v) {
  if (start_offset == HeapObject::kMapOffset) {
    v->VisitMapPointer(obj);
    start_offset += kTaggedSize;
  }
  v->VisitPointers(obj, obj->RawField(start_offset), obj->RawField(end_offset));
}

template <typename ObjectVisitor>
void BodyDescriptorBase::IteratePointer(Tagged<HeapObject> obj, int offset,
                                        ObjectVisitor* v) {
  DCHECK_NE(offset, HeapObject::kMapOffset);
  v->VisitPointer(obj, obj->RawField(offset));
}

template <typename ObjectVisitor>
DISABLE_CFI_PERF void BodyDescriptorBase::IterateMaybeWeakPointers(
    Tagged<HeapObject> obj, int start_offset, int end_offset,
    ObjectVisitor* v) {
  v->VisitPointers(obj, obj->RawMaybeWeakField(start_offset),
                   obj->RawMaybeWeakField(end_offset));
}

template <typename ObjectVisitor>
void BodyDescriptorBase::IterateMaybeWeakPointer(Tagged<HeapObject> obj,
                                                 int offset, ObjectVisitor* v) {
  DCHECK_NE(offset, HeapObject::kMapOffset);
  v->VisitPointer(obj, obj->RawMaybeWeakField(offset));
}

template <typename ObjectVisitor>
DISABLE_CFI_PERF void BodyDescriptorBase::IterateCustomWeakPointers(
    Tagged<HeapObject> obj, int start_offset, int end_offset,
    ObjectVisitor* v) {
  v->VisitCustomWeakPointers(obj, obj->RawField(start_offset),
                             obj->RawField(end_offset));
}

template <typename ObjectVisitor>
DISABLE_CFI_PERF void BodyDescriptorBase::IterateEphemeron(
    Tagged<HeapObject> obj, int index, int key_offset, int value_offset,
    ObjectVisitor* v) {
  v->VisitEphemeron(obj, index, obj->RawField(key_offset),
                    obj->RawField(value_offset));
}

template <typename ObjectVisitor>
void BodyDescriptorBase::IterateCustomWeakPointer(Tagged<HeapObject> obj,
                                                  int offset,
                                                  ObjectVisitor* v) {
  v->VisitCustomWeakPointer(obj, obj->RawField(offset));
}

template <typename ObjectVisitor>
void BodyDescriptorBase::IterateMaybeIndirectPointer(Tagged<HeapObject> obj,
                                                     int offset,
                                                     ObjectVisitor* v,
                                                     IndirectPointerMode mode) {
#ifdef V8_CODE_POINTER_SANDBOXING
  v->VisitIndirectPointer(obj, obj->RawIndirectPointerField(offset), mode);
#else
  if (mode == IndirectPointerMode::kStrong) {
    IteratePointer(obj, offset, v);
  } else {
    IterateCustomWeakPointer(obj, offset, v);
  }
#endif
}

class HeapNumber::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {}

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return HeapNumber::kSize;
  }
};

// This is a descriptor for one/two pointer fillers.
class FreeSpaceFillerBodyDescriptor final : public DataOnlyBodyDescriptor {
 public:
  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> raw_object) {
    return map->instance_size();
  }
};

class FreeSpace::BodyDescriptor final : public DataOnlyBodyDescriptor {
 public:
  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> raw_object) {
    return FreeSpace::unchecked_cast(raw_object)->Size();
  }
};

class JSObject::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static const int kStartOffset = JSReceiver::kPropertiesOrHashOffset;

  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IterateJSObjectBodyImpl(map, obj, kStartOffset, object_size, v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return map->instance_size();
  }
};

class JSObject::FastBodyDescriptor final : public BodyDescriptorBase {
 public:
  static const int kStartOffset = JSReceiver::kPropertiesOrHashOffset;

  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IteratePointers(obj, kStartOffset, object_size, v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return map->instance_size();
  }
};

class WeakCell::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IteratePointers(obj, HeapObject::kHeaderSize, kTargetOffset, v);
    IterateCustomWeakPointer(obj, kTargetOffset, v);
    IterateCustomWeakPointer(obj, kUnregisterTokenOffset, v);
    IteratePointers(obj, kUnregisterTokenOffset + kTaggedSize, object_size, v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return map->instance_size();
  }
};

class JSWeakRef::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IteratePointers(obj, JSReceiver::kPropertiesOrHashOffset, kTargetOffset, v);
    IterateCustomWeakPointer(obj, kTargetOffset, v);
    IterateJSObjectBodyImpl(map, obj, kTargetOffset + kTaggedSize, object_size,
                            v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return map->instance_size();
  }
};

class JSFinalizationRegistry::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IteratePointers(obj, JSObject::BodyDescriptor::kStartOffset,
                    kNextDirtyOffset, v);
    IterateCustomWeakPointer(obj, kNextDirtyOffset, v);
    IterateJSObjectBodyImpl(map, obj, kNextDirtyOffset + kTaggedSize,
                            object_size, v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return map->instance_size();
  }
};

class AllocationSite::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static_assert(AllocationSite::kCommonPointerFieldEndOffset ==
                AllocationSite::kPretenureDataOffset);
  static_assert(AllocationSite::kPretenureDataOffset + kInt32Size ==
                AllocationSite::kPretenureCreateCountOffset);
  static_assert(AllocationSite::kPretenureCreateCountOffset + kInt32Size ==
                AllocationSite::kWeakNextOffset);

  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
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

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return map->instance_size();
  }
};

class JSFunction::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static const int kStartOffset = JSObject::BodyDescriptor::kStartOffset;

  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    // Iterate JSFunction header fields first.
    int header_size = JSFunction::GetHeaderSize(map->has_prototype_slot());
    DCHECK_GE(object_size, header_size);
    IteratePointers(obj, kStartOffset, kCodeOffset, v);

    // Iterate the code object pointer.
    // When the sandbox is enabled, Code objects are referenced via indirect
    // pointers through the code pointer table. Otherwise, the slot contains a
    // regular tagged pointer.
    // The code field is treated as a custom weak pointer. This field
    // is visited as a weak pointer if the Code is baseline code
    // and the bytecode array corresponding to this function is old. In the rest
    // of the cases this field is treated as strong pointer.
    // See MarkingVisitorBase::VisitJSFunction.
    IterateMaybeIndirectPointer(obj, kCodeOffset, v,
                                IndirectPointerMode::kCustom);
    DCHECK_GE(header_size, kCodeOffset);
    // Iterate rest of the header fields
    IteratePointers(obj, kCodeOffset + kTaggedSize, header_size, v);
    // Iterate rest of the fields starting after the header.
    IterateJSObjectBodyImpl(map, obj, header_size, object_size, v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return map->instance_size();
  }
};

class JSArrayBuffer::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    // JSArrayBuffer instances contain raw data that the GC does not know about.
    IteratePointers(obj, kPropertiesOrHashOffset, kEndOfTaggedFieldsOffset, v);
    IterateJSObjectBodyImpl(map, obj, kHeaderSize, object_size, v);
    v->VisitExternalPointer(map, obj->RawExternalPointerField(kExtensionOffset),
                            kArrayBufferExtensionTag);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return map->instance_size();
  }
};

class JSTypedArray::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    // JSTypedArray contains raw data that the GC does not know about.
    IteratePointers(obj, kPropertiesOrHashOffset, kEndOfTaggedFieldsOffset, v);
    // TODO(v8:4153): Remove this.
    IteratePointer(obj, kBasePointerOffset, v);
    IterateJSObjectBodyImpl(map, obj, kHeaderSize, object_size, v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return map->instance_size();
  }
};

class JSDataViewOrRabGsabDataView::BodyDescriptor final
    : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    // JSDataViewOrRabGsabDataView contains raw data that the GC does not know
    // about.
    IteratePointers(obj, kPropertiesOrHashOffset, kEndOfTaggedFieldsOffset, v);
    IterateJSObjectBodyImpl(map, obj, kHeaderSize, object_size, v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return map->instance_size();
  }
};

class JSExternalObject::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IteratePointers(obj, kPropertiesOrHashOffset, kEndOfTaggedFieldsOffset, v);
    v->VisitExternalPointer(obj, obj->RawExternalPointerField(kValueOffset),
                            kExternalObjectValueTag);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return map->instance_size();
  }
};

template <typename Derived>
class V8_EXPORT_PRIVATE SmallOrderedHashTable<Derived>::BodyDescriptor final
    : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    Derived table = Derived::cast(obj);
    int start_offset = DataTableStartOffset();
    int end_offset = table.GetBucketsStartOffset();
    IteratePointers(obj, start_offset, end_offset, v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> obj) {
    Derived table = Derived::cast(obj);
    return Derived::SizeFor(table.Capacity());
  }
};

class V8_EXPORT_PRIVATE SwissNameDictionary::BodyDescriptor final
    : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    Tagged<SwissNameDictionary> table =
        SwissNameDictionary::unchecked_cast(obj);
    static_assert(MetaTablePointerOffset() + kTaggedSize ==
                  DataTableStartOffset());
    int start_offset = MetaTablePointerOffset();
    int end_offset = table->DataTableEndOffset(table->Capacity());
    IteratePointers(obj, start_offset, end_offset, v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> obj) {
    Tagged<SwissNameDictionary> table =
        SwissNameDictionary::unchecked_cast(obj);
    return SwissNameDictionary::SizeFor(table->Capacity());
  }
};

class ByteArray::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {}

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> obj) {
    return ByteArray::SizeFor(
        ByteArray::unchecked_cast(obj)->length(kAcquireLoad));
  }
};

class BytecodeArray::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IteratePointer(obj, kConstantPoolOffset, v);
    IteratePointer(obj, kHandlerTableOffset, v);
    IteratePointer(obj, kSourcePositionTableOffset, v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> obj) {
    return BytecodeArray::SizeFor(
        BytecodeArray::cast(obj)->length(kAcquireLoad));
  }
};

class ExternalPointerArray::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    Tagged<ExternalPointerArray> array =
        ExternalPointerArray::unchecked_cast(obj);
    for (int i = 0; i < array->length(); i++) {
      // We don't currently track the (expected) tag of the elements of this
      // array, so we have to use the generic tag here. This is ok as long as
      // the visitor does not try to dereference the pointer (which it never
      // should). The alternative would probably be to store the tag as
      // additional metadata in the array itself, but then we also need to be
      // careful since an attacker could then modify the tag.
      v->VisitExternalPointer(array,
                              array->RawExternalPointerField(
                                  ExternalPointerArray::OffsetOfElementAt(i)),
                              kAnyExternalPointerTag);
    }
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> obj) {
    return ExternalPointerArray::SizeFor(
        ExternalPointerArray::cast(obj)->length(kAcquireLoad));
  }
};

class BigInt::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {}

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> obj) {
    return BigInt::SizeFor(BigInt::unchecked_cast(obj)->length(kAcquireLoad));
  }
};

class FixedDoubleArray::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {}

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> obj) {
    return FixedDoubleArray::SizeFor(
        FixedDoubleArray::unchecked_cast(obj)->length(kAcquireLoad));
  }
};

class FeedbackMetadata::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {}

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> obj) {
    return FeedbackMetadata::SizeFor(
        FeedbackMetadata::unchecked_cast(obj)->slot_count(kAcquireLoad));
  }
};

class PreparseData::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    Tagged<PreparseData> data = PreparseData::unchecked_cast(obj);
    int start_offset = data->inner_start_offset();
    int end_offset = start_offset + data->children_length() * kTaggedSize;
    IteratePointers(obj, start_offset, end_offset, v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> obj) {
    Tagged<PreparseData> data = PreparseData::unchecked_cast(obj);
    return PreparseData::SizeFor(data->data_length(), data->children_length());
  }
};

class SharedFunctionInfo::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IterateCustomWeakPointers(obj, kStartOfWeakFieldsOffset,
                              kEndOfWeakFieldsOffset, v);
    IteratePointers(obj, kStartOfStrongFieldsOffset, kEndOfStrongFieldsOffset,
                    v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> raw_object) {
    return kSize;
  }
};

class PromiseOnStack::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IteratePointers(obj, Struct::kHeaderSize, kPromiseOffset, v);
    IterateMaybeWeakPointer(obj, kPromiseOffset, v);
    static_assert(kPromiseOffset + kTaggedSize == kHeaderSize);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> obj) {
    return obj->SizeFromMap(map);
  }
};

class PrototypeInfo::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IteratePointers(obj, HeapObject::kHeaderSize, kObjectCreateMapOffset, v);
    IterateMaybeWeakPointer(obj, kObjectCreateMapOffset, v);
    static_assert(kObjectCreateMapOffset + kTaggedSize == kHeaderSize);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> obj) {
    return obj->SizeFromMap(map);
  }
};

class JSWeakCollection::BodyDescriptorImpl final : public BodyDescriptorBase {
 public:
  static_assert(kTableOffset + kTaggedSize == kHeaderSizeOfAllWeakCollections);

  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IterateJSObjectBodyImpl(map, obj, kPropertiesOrHashOffset, object_size, v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return map->instance_size();
  }
};

class JSSynchronizationPrimitive::BodyDescriptor final
    : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IteratePointers(obj, kPropertiesOrHashOffset, kEndOfTaggedFieldsOffset, v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return map->instance_size();
  }
};

class Foreign::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    v->VisitExternalPointer(obj,
                            obj->RawExternalPointerField(kForeignAddressOffset),
                            kForeignForeignAddressTag);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return kSize;
  }
};

#if V8_ENABLE_WEBASSEMBLY
class WasmTypeInfo::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IteratePointer(obj, kInstanceOffset, v);
    IteratePointers(obj, kSupertypesOffset, SizeOf(map, obj), v);

    v->VisitExternalPointer(obj,
                            obj->RawExternalPointerField(kNativeTypeOffset),
                            kWasmTypeInfoNativeTypeTag);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return kSupertypesOffset +
           WasmTypeInfo::cast(object)->supertypes_length() * kTaggedSize;
  }
};

class WasmApiFunctionRef::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IteratePointers(obj, kStartOfStrongFieldsOffset, kEndOfStrongFieldsOffset,
                    v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return kSize;
  }
};

class WasmExportedFunctionData::BodyDescriptor final
    : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    WasmFunctionData::BodyDescriptor::IterateBody<ObjectVisitor>(
        map, obj, object_size, v);
    IteratePointers(obj, kStartOfStrongFieldsOffset, kEndOfStrongFieldsOffset,
                    v);
    v->VisitExternalPointer(obj, obj->RawExternalPointerField(kSigOffset),
                            kWasmExportedFunctionDataSignatureTag);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return kSize;
  }
};

class WasmInternalFunction::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IteratePointers(obj, kStartOfStrongFieldsOffset, kEndOfStrongFieldsOffset,
                    v);
    v->VisitExternalPointer(obj,
                            obj->RawExternalPointerField(kCallTargetOffset),
                            kWasmInternalFunctionCallTargetTag);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return kSize;
  }
};

class WasmInstanceObject::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IteratePointers(obj, kPropertiesOrHashOffset, JSObject::kHeaderSize, v);
    for (uint16_t offset : kTaggedFieldOffsets) {
      IteratePointer(obj, offset, v);
    }
    IterateJSObjectBodyImpl(map, obj, kHeaderSize, object_size, v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return map->instance_size();
  }
};

class WasmArray::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    // The type is safe to use because it's kept alive by the {map}'s
    // WasmTypeInfo.
    if (!WasmArray::GcSafeType(map)->element_type().is_reference()) return;
    IteratePointers(obj, WasmArray::kHeaderSize, object_size, v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return WasmArray::SizeFor(map, WasmArray::unchecked_cast(object)->length());
  }
};

class WasmContinuationObject::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IteratePointers(obj, kStartOfStrongFieldsOffset, kEndOfStrongFieldsOffset,
                    v);
    v->VisitExternalPointer(obj, obj->RawExternalPointerField(kJmpbufOffset),
                            kWasmContinuationJmpbufTag);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return kSize;
  }
};

class WasmStruct::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    Tagged<WasmStruct> wasm_struct = WasmStruct::unchecked_cast(obj);
    // The {type} is safe to use because it's kept alive by the {map}'s
    // WasmTypeInfo.
    wasm::StructType* type = WasmStruct::GcSafeType(map);
    for (uint32_t i = 0; i < type->field_count(); i++) {
      if (!type->field(i).is_reference()) continue;
      int offset = static_cast<int>(type->field_offset(i));
      v->VisitPointer(wasm_struct, wasm_struct->RawField(offset));
    }
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return WasmStruct::GcSafeSize(map);
  }
};

class WasmNull::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {}

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> obj) {
    return WasmNull::kSize;
  }
};
#endif  // V8_ENABLE_WEBASSEMBLY

class ExternalString::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    Tagged<ExternalString> string = ExternalString::unchecked_cast(obj);
    v->VisitExternalPointer(obj,
                            string->RawExternalPointerField(kResourceOffset),
                            kExternalStringResourceTag);
    if (string->is_uncached()) return;
    v->VisitExternalPointer(
        obj, string->RawExternalPointerField(kResourceDataOffset),
        kExternalStringResourceDataTag);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    InstanceType type = map->instance_type();
    const auto is_uncached =
        (type & kUncachedExternalStringMask) == kUncachedExternalStringTag;
    return is_uncached ? kUncachedSize : kSizeOfAllExternalStrings;
  }
};

class CoverageInfo::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {}

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    Tagged<CoverageInfo> info = CoverageInfo::cast(object);
    return CoverageInfo::SizeFor(info->slot_count());
  }
};

class InstructionStream::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static_assert(static_cast<int>(HeapObject::kHeaderSize) ==
                static_cast<int>(kCodeOffset));
  static_assert(kCodeOffset + kTaggedSize == kRelocationInfoOffset);
  static_assert(kRelocationInfoOffset + kTaggedSize == kDataStart);

  static constexpr int kRelocModeMask =
      RelocInfo::ModeMask(RelocInfo::CODE_TARGET) |
      RelocInfo::ModeMask(RelocInfo::RELATIVE_CODE_TARGET) |
      RelocInfo::ModeMask(RelocInfo::FULL_EMBEDDED_OBJECT) |
      RelocInfo::ModeMask(RelocInfo::COMPRESSED_EMBEDDED_OBJECT) |
      RelocInfo::ModeMask(RelocInfo::EXTERNAL_REFERENCE) |
      RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE) |
      RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE_ENCODED) |
      RelocInfo::ModeMask(RelocInfo::OFF_HEAP_TARGET) |
      RelocInfo::ModeMask(RelocInfo::WASM_STUB_CALL);

  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 ObjectVisitor* v) {
    // GC does not visit data/code in the header and in the body directly.
    IteratePointers(obj, kStartOfStrongFieldsOffset, kEndOfStrongFieldsOffset,
                    v);

    Tagged<InstructionStream> istream = InstructionStream::unchecked_cast(obj);
    Tagged<Code> code;
    if (istream->TryGetCodeUnchecked(&code, kAcquireLoad)) {
      RelocIterator it(code, istream, istream->unchecked_relocation_info(),
                       kRelocModeMask);
      v->VisitRelocInfo(istream, &it);
    }
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IterateBody(map, obj, v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return InstructionStream::unchecked_cast(object)->Size();
  }
};

class Map::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IteratePointers(obj, Map::kStartOfStrongFieldsOffset,
                    Map::kEndOfStrongFieldsOffset, v);
    IterateMaybeWeakPointer(obj, kTransitionsOrPrototypeInfoOffset, v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> obj) {
    return Map::kSize;
  }
};

class DataHandler::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    static_assert(kSmiHandlerOffset < kData1Offset,
                  "Field order must be in sync with this iteration code");
    static_assert(kData1Offset < kSizeWithData1,
                  "Field order must be in sync with this iteration code");
    IteratePointers(obj, kSmiHandlerOffset, kData1Offset, v);
    IterateMaybeWeakPointers(obj, kData1Offset, object_size, v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return object->SizeFromMap(map);
  }
};

class NativeContext::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IteratePointers(obj, NativeContext::kStartOfStrongFieldsOffset,
                    NativeContext::kEndOfStrongFieldsOffset, v);
    IterateCustomWeakPointers(obj, NativeContext::kStartOfWeakFieldsOffset,
                              NativeContext::kEndOfWeakFieldsOffset, v);
    v->VisitExternalPointer(obj,
                            obj->RawExternalPointerField(kMicrotaskQueueOffset),
                            kNativeContextMicrotaskQueueTag);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return NativeContext::kSize;
  }
};

class Code::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IteratePointers(obj, Code::kStartOfStrongFieldsOffset,
                    Code::kEndOfStrongFieldsWithMainCageBaseOffset, v);

    static_assert(Code::kEndOfStrongFieldsWithMainCageBaseOffset ==
                  Code::kInstructionStreamOffset);
    static_assert(Code::kInstructionStreamOffset + kTaggedSize ==
                  Code::kEndOfStrongFieldsOffset);
    v->VisitInstructionStreamPointer(
        Code::cast(obj),
        obj->RawInstructionStreamField(kInstructionStreamOffset));
#ifdef V8_CODE_POINTER_SANDBOXING
    v->VisitIndirectPointerTableEntry(
        obj, obj->RawIndirectPointerField(kCodePointerTableEntryOffset));
#endif
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return Code::kSize;
  }
};

class EmbedderDataArray::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
#ifdef V8_COMPRESS_POINTERS
    static_assert(kEmbedderDataSlotSize == 2 * kTaggedSize);
    for (int offset = EmbedderDataArray::OffsetOfElementAt(0);
         offset < object_size; offset += kEmbedderDataSlotSize) {
      IteratePointer(obj, offset + EmbedderDataSlot::kTaggedPayloadOffset, v);
      v->VisitExternalPointer(
          obj,
          obj->RawExternalPointerField(
              offset + EmbedderDataSlot::kExternalPointerOffset),
          kEmbedderDataSlotPayloadTag);
    }

#else
    // We store raw aligned pointers as Smis, so it's safe to iterate the whole
    // array.
    static_assert(kEmbedderDataSlotSize == kTaggedSize);
    IteratePointers(obj, EmbedderDataArray::kHeaderSize, object_size, v);
#endif
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return object->SizeFromMap(map);
  }
};

template <typename Op, typename... Args>
auto BodyDescriptorApply(InstanceType type, Args&&... args) {
#define CALL_APPLY(ClassName) \
  Op::template apply<ClassName::BodyDescriptor>(std::forward<Args>(args)...)

  if (type < FIRST_NONSTRING_TYPE) {
    switch (type & kStringRepresentationMask) {
      case kSeqStringTag:
        if ((type & kStringEncodingMask) == kOneByteStringTag) {
          return CALL_APPLY(SeqOneByteString);
        } else {
          return CALL_APPLY(SeqTwoByteString);
        }
      case kConsStringTag:
        return CALL_APPLY(ConsString);
      case kThinStringTag:
        return CALL_APPLY(ThinString);
      case kSlicedStringTag:
        return CALL_APPLY(SlicedString);
      case kExternalStringTag:
        if ((type & kStringEncodingMask) == kOneByteStringTag) {
          return CALL_APPLY(ExternalOneByteString);
        } else {
          return CALL_APPLY(ExternalTwoByteString);
        }
    }
    UNREACHABLE();
  }
  if (InstanceTypeChecker::IsJSApiObject(type)) {
    return CALL_APPLY(JSObject);
  }

  switch (type) {
    case EMBEDDER_DATA_ARRAY_TYPE:
      return CALL_APPLY(EmbedderDataArray);
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
    case NAME_TO_INDEX_HASH_TABLE_TYPE:
    case REGISTERED_SYMBOL_TABLE_TYPE:
    case SCRIPT_CONTEXT_TABLE_TYPE:
      return CALL_APPLY(FixedArray);
    case EPHEMERON_HASH_TABLE_TYPE:
      return CALL_APPLY(EphemeronHashTable);
    case AWAIT_CONTEXT_TYPE:
    case BLOCK_CONTEXT_TYPE:
    case CATCH_CONTEXT_TYPE:
    case DEBUG_EVALUATE_CONTEXT_TYPE:
    case EVAL_CONTEXT_TYPE:
    case FUNCTION_CONTEXT_TYPE:
    case MODULE_CONTEXT_TYPE:
    case SCRIPT_CONTEXT_TYPE:
    case WITH_CONTEXT_TYPE:
      return CALL_APPLY(Context);
    case NATIVE_CONTEXT_TYPE:
      return CALL_APPLY(NativeContext);
    case FIXED_DOUBLE_ARRAY_TYPE:
      return CALL_APPLY(FixedDoubleArray);
    case FEEDBACK_METADATA_TYPE:
      return CALL_APPLY(FeedbackMetadata);
    case PROPERTY_ARRAY_TYPE:
      return CALL_APPLY(PropertyArray);
    case TRANSITION_ARRAY_TYPE:
      return CALL_APPLY(TransitionArray);
    case FEEDBACK_CELL_TYPE:
      return CALL_APPLY(FeedbackCell);
    case COVERAGE_INFO_TYPE:
      return CALL_APPLY(CoverageInfo);
#if V8_ENABLE_WEBASSEMBLY
    case WASM_API_FUNCTION_REF_TYPE:
      return CALL_APPLY(WasmApiFunctionRef);
    case WASM_ARRAY_TYPE:
      return CALL_APPLY(WasmArray);
    case WASM_CAPI_FUNCTION_DATA_TYPE:
      return CALL_APPLY(WasmCapiFunctionData);
    case WASM_EXCEPTION_PACKAGE_TYPE:
      return CALL_APPLY(WasmExceptionPackage);
    case WASM_EXPORTED_FUNCTION_DATA_TYPE:
      return CALL_APPLY(WasmExportedFunctionData);
    case WASM_INTERNAL_FUNCTION_TYPE:
      return CALL_APPLY(WasmInternalFunction);
    case WASM_JS_FUNCTION_DATA_TYPE:
      return CALL_APPLY(WasmJSFunctionData);
    case WASM_RESUME_DATA_TYPE:
      return CALL_APPLY(WasmResumeData);
    case WASM_CONTINUATION_OBJECT_TYPE:
      return CALL_APPLY(WasmContinuationObject);
    case WASM_STRUCT_TYPE:
      return CALL_APPLY(WasmStruct);
    case WASM_TYPE_INFO_TYPE:
      return CALL_APPLY(WasmTypeInfo);
    case WASM_SUSPENDER_OBJECT_TYPE:
      return CALL_APPLY(WasmSuspenderObject);
#endif  // V8_ENABLE_WEBASSEMBLY
    case JS_API_OBJECT_TYPE:
    case JS_ARGUMENTS_OBJECT_TYPE:
    case JS_ARRAY_ITERATOR_PROTOTYPE_TYPE:
    case JS_ARRAY_ITERATOR_TYPE:
    case JS_ARRAY_TYPE:
    case JS_ASYNC_FROM_SYNC_ITERATOR_TYPE:
    case JS_ASYNC_FUNCTION_OBJECT_TYPE:
    case JS_ASYNC_GENERATOR_OBJECT_TYPE:
    case JS_BOUND_FUNCTION_TYPE:
    case JS_CONTEXT_EXTENSION_OBJECT_TYPE:
    case JS_DATE_TYPE:
    case JS_ERROR_TYPE:
    case JS_FINALIZATION_REGISTRY_TYPE:
    case JS_GENERATOR_OBJECT_TYPE:
    case JS_GLOBAL_OBJECT_TYPE:
    case JS_GLOBAL_PROXY_TYPE:
    case JS_ITERATOR_FILTER_HELPER_TYPE:
    case JS_ITERATOR_MAP_HELPER_TYPE:
    case JS_ITERATOR_TAKE_HELPER_TYPE:
    case JS_ITERATOR_DROP_HELPER_TYPE:
    case JS_ITERATOR_FLAT_MAP_HELPER_TYPE:
    case JS_ITERATOR_PROTOTYPE_TYPE:
    case JS_MAP_ITERATOR_PROTOTYPE_TYPE:
    case JS_MAP_KEY_ITERATOR_TYPE:
    case JS_MAP_KEY_VALUE_ITERATOR_TYPE:
    case JS_MAP_TYPE:
    case JS_MAP_VALUE_ITERATOR_TYPE:
    case JS_MESSAGE_OBJECT_TYPE:
    case JS_MODULE_NAMESPACE_TYPE:
    case JS_OBJECT_PROTOTYPE_TYPE:
    case JS_OBJECT_TYPE:
    case JS_PRIMITIVE_WRAPPER_TYPE:
    case JS_PROMISE_PROTOTYPE_TYPE:
    case JS_PROMISE_TYPE:
    case JS_REG_EXP_PROTOTYPE_TYPE:
    case JS_REG_EXP_STRING_ITERATOR_TYPE:
    case JS_REG_EXP_TYPE:
    case JS_SET_ITERATOR_PROTOTYPE_TYPE:
    case JS_SET_KEY_VALUE_ITERATOR_TYPE:
    case JS_SET_PROTOTYPE_TYPE:
    case JS_SET_TYPE:
    case JS_SET_VALUE_ITERATOR_TYPE:
    case JS_SPECIAL_API_OBJECT_TYPE:
    case JS_SHADOW_REALM_TYPE:
    case JS_SHARED_ARRAY_TYPE:
    case JS_SHARED_STRUCT_TYPE:
    case JS_STRING_ITERATOR_PROTOTYPE_TYPE:
    case JS_STRING_ITERATOR_TYPE:
    case JS_TEMPORAL_CALENDAR_TYPE:
    case JS_TEMPORAL_DURATION_TYPE:
    case JS_TEMPORAL_INSTANT_TYPE:
    case JS_TEMPORAL_PLAIN_DATE_TYPE:
    case JS_TEMPORAL_PLAIN_DATE_TIME_TYPE:
    case JS_TEMPORAL_PLAIN_MONTH_DAY_TYPE:
    case JS_TEMPORAL_PLAIN_TIME_TYPE:
    case JS_TEMPORAL_PLAIN_YEAR_MONTH_TYPE:
    case JS_TEMPORAL_TIME_ZONE_TYPE:
    case JS_TEMPORAL_ZONED_DATE_TIME_TYPE:
    case JS_TYPED_ARRAY_PROTOTYPE_TYPE:
    case JS_VALID_ITERATOR_WRAPPER_TYPE:
    case JS_WRAPPED_FUNCTION_TYPE:
    case JS_RAW_JSON_TYPE:
#ifdef V8_INTL_SUPPORT
    case JS_V8_BREAK_ITERATOR_TYPE:
    case JS_COLLATOR_TYPE:
    case JS_DATE_TIME_FORMAT_TYPE:
    case JS_DISPLAY_NAMES_TYPE:
    case JS_DURATION_FORMAT_TYPE:
    case JS_LIST_FORMAT_TYPE:
    case JS_LOCALE_TYPE:
    case JS_NUMBER_FORMAT_TYPE:
    case JS_PLURAL_RULES_TYPE:
    case JS_RELATIVE_TIME_FORMAT_TYPE:
    case JS_SEGMENT_ITERATOR_TYPE:
    case JS_SEGMENTER_TYPE:
    case JS_SEGMENTS_TYPE:
#endif  // V8_INTL_SUPPORT
#if V8_ENABLE_WEBASSEMBLY
    case WASM_GLOBAL_OBJECT_TYPE:
    case WASM_MEMORY_OBJECT_TYPE:
    case WASM_MODULE_OBJECT_TYPE:
    case WASM_TABLE_OBJECT_TYPE:
    case WASM_TAG_OBJECT_TYPE:
    case WASM_VALUE_OBJECT_TYPE:
#endif  // V8_ENABLE_WEBASSEMBLY
      return CALL_APPLY(JSObject);
    case JS_FUNCTION_TYPE:
    case JS_CLASS_CONSTRUCTOR_TYPE:
    case JS_PROMISE_CONSTRUCTOR_TYPE:
    case JS_REG_EXP_CONSTRUCTOR_TYPE:
    case JS_ARRAY_CONSTRUCTOR_TYPE:
#define TYPED_ARRAY_CONSTRUCTORS_SWITCH(Type, type, TYPE, Ctype) \
  case TYPE##_TYPED_ARRAY_CONSTRUCTOR_TYPE:
      TYPED_ARRAYS(TYPED_ARRAY_CONSTRUCTORS_SWITCH)
#undef TYPED_ARRAY_CONSTRUCTORS_SWITCH
      return CALL_APPLY(JSFunction);
#if V8_ENABLE_WEBASSEMBLY
    case WASM_INSTANCE_OBJECT_TYPE:
      return CALL_APPLY(WasmInstanceObject);
    case WASM_NULL_TYPE:
      return CALL_APPLY(WasmNull);
#endif  // V8_ENABLE_WEBASSEMBLY
    case JS_WEAK_MAP_TYPE:
    case JS_WEAK_SET_TYPE:
      return CALL_APPLY(JSWeakCollection);
    case JS_ARRAY_BUFFER_TYPE:
      return CALL_APPLY(JSArrayBuffer);
    case JS_DATA_VIEW_TYPE:
      return CALL_APPLY(JSDataView);
    case JS_RAB_GSAB_DATA_VIEW_TYPE:
      return CALL_APPLY(JSRabGsabDataView);
    case JS_TYPED_ARRAY_TYPE:
      return CALL_APPLY(JSTypedArray);
    case JS_EXTERNAL_OBJECT_TYPE:
      return CALL_APPLY(JSExternalObject);
    case WEAK_CELL_TYPE:
      return CALL_APPLY(WeakCell);
    case JS_WEAK_REF_TYPE:
      return CALL_APPLY(JSWeakRef);
    case JS_PROXY_TYPE:
      return CALL_APPLY(JSProxy);
    case JS_ATOMICS_MUTEX_TYPE:
    case JS_ATOMICS_CONDITION_TYPE:
      return CALL_APPLY(JSSynchronizationPrimitive);
    case FOREIGN_TYPE:
      return CALL_APPLY(Foreign);
    case MAP_TYPE:
      return CALL_APPLY(Map);
    case INSTRUCTION_STREAM_TYPE:
      return CALL_APPLY(InstructionStream);
    case CELL_TYPE:
      return CALL_APPLY(Cell);
    case PROPERTY_CELL_TYPE:
      return CALL_APPLY(PropertyCell);
    case SYMBOL_TYPE:
      return CALL_APPLY(Symbol);
    case BYTECODE_ARRAY_TYPE:
      return CALL_APPLY(BytecodeArray);
    case SMALL_ORDERED_HASH_SET_TYPE:
      return CALL_APPLY(SmallOrderedHashTable<SmallOrderedHashSet>);
    case SMALL_ORDERED_HASH_MAP_TYPE:
      return CALL_APPLY(SmallOrderedHashTable<SmallOrderedHashMap>);
    case SMALL_ORDERED_NAME_DICTIONARY_TYPE:
      return CALL_APPLY(SmallOrderedHashTable<SmallOrderedNameDictionary>);
    case SWISS_NAME_DICTIONARY_TYPE:
      return CALL_APPLY(SwissNameDictionary);
    case CODE_TYPE:
      return CALL_APPLY(Code);
    case PREPARSE_DATA_TYPE:
      return CALL_APPLY(PreparseData);
    case SHARED_FUNCTION_INFO_TYPE:
      return CALL_APPLY(SharedFunctionInfo);
    case HEAP_NUMBER_TYPE:
      return CALL_APPLY(HeapNumber);
    case BYTE_ARRAY_TYPE:
      return CALL_APPLY(ByteArray);
    case EXTERNAL_POINTER_ARRAY_TYPE:
      return CALL_APPLY(ExternalPointerArray);
    case BIGINT_TYPE:
      return CALL_APPLY(BigInt);
    case ALLOCATION_SITE_TYPE:
      return CALL_APPLY(AllocationSite);
    case ODDBALL_TYPE:
      return CALL_APPLY(Oddball);
    case HOLE_TYPE:
      return CALL_APPLY(Hole);

#define MAKE_STRUCT_CASE(TYPE, Name, name) \
  case TYPE:                               \
    return CALL_APPLY(Name);
      STRUCT_LIST(MAKE_STRUCT_CASE)
#undef MAKE_STRUCT_CASE
    case ACCESSOR_INFO_TYPE:
      return CALL_APPLY(AccessorInfo);
    case CALL_HANDLER_INFO_TYPE:
      return CALL_APPLY(CallHandlerInfo);
    case LOAD_HANDLER_TYPE:
      return CALL_APPLY(LoadHandler);
    case STORE_HANDLER_TYPE:
      return CALL_APPLY(StoreHandler);
    case SOURCE_TEXT_MODULE_TYPE:
      return CALL_APPLY(SourceTextModule);
    case SYNTHETIC_MODULE_TYPE:
      return CALL_APPLY(SyntheticModule);
// TODO(turbofan): Avoid duplicated cases when the body descriptors are
// identical.
#define MAKE_TORQUE_BODY_DESCRIPTOR_APPLY(TYPE, TypeName) \
  case TYPE:                                              \
    return CALL_APPLY(TypeName);
      TORQUE_INSTANCE_TYPE_TO_BODY_DESCRIPTOR_LIST(
          MAKE_TORQUE_BODY_DESCRIPTOR_APPLY)
#undef MAKE_TORQUE_BODY_DESCRIPTOR_APPLY

    case FILLER_TYPE:
      return Op::template apply<FreeSpaceFillerBodyDescriptor>(
          std::forward<Args>(args)...);

    case FREE_SPACE_TYPE:
      return CALL_APPLY(FreeSpace);

    default:
      PrintF("Unknown type: %d\n", type);
      UNREACHABLE();
  }
#undef CALL_APPLY
}

template <typename ObjectVisitor>
void HeapObject::IterateFast(PtrComprCageBase cage_base, ObjectVisitor* v) {
  v->VisitMapPointer(*this);
  IterateBodyFast(cage_base, v);
}

template <typename ObjectVisitor>
void HeapObject::IterateFast(Tagged<Map> map, ObjectVisitor* v) {
  v->VisitMapPointer(*this);
  IterateBodyFast(map, SizeFromMap(map), v);
}

template <typename ObjectVisitor>
void HeapObject::IterateFast(Tagged<Map> map, int object_size,
                             ObjectVisitor* v) {
  v->VisitMapPointer(*this);
  IterateBodyFast(map, object_size, v);
}

template <typename ObjectVisitor>
void HeapObject::IterateBodyFast(PtrComprCageBase cage_base, ObjectVisitor* v) {
  Tagged<Map> m = map(cage_base);
  IterateBodyFast(m, SizeFromMap(m), v);
}

struct CallIterateBody {
  template <typename BodyDescriptor, typename ObjectVisitor>
  static void apply(Tagged<Map> map, Tagged<HeapObject> obj, int object_size,
                    ObjectVisitor* v) {
    BodyDescriptor::IterateBody(map, obj, object_size, v);
  }
};

template <typename ObjectVisitor>
void HeapObject::IterateBodyFast(Tagged<Map> map, int object_size,
                                 ObjectVisitor* v) {
  BodyDescriptorApply<CallIterateBody>(map->instance_type(), map, *this,
                                       object_size, v);
}

class EphemeronHashTable::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    int entries_start = EphemeronHashTable::kHeaderSize +
                        EphemeronHashTable::kElementsStartIndex * kTaggedSize;
    IteratePointers(obj, EphemeronHashTable::kHeaderSize, entries_start, v);
    Tagged<EphemeronHashTable> table = EphemeronHashTable::unchecked_cast(obj);
    for (InternalIndex i : table->IterateEntries()) {
      const int key_index = EphemeronHashTable::EntryToIndex(i);
      const int value_index = EphemeronHashTable::EntryToValueIndex(i);
      IterateEphemeron(obj, i.as_int(), OffsetOfElementAt(key_index),
                       OffsetOfElementAt(value_index), v);
    }
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return object->SizeFromMap(map);
  }
};

class AccessorInfo::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static_assert(AccessorInfo::kEndOfStrongFieldsOffset ==
                AccessorInfo::kMaybeRedirectedGetterOffset);
  static_assert(AccessorInfo::kMaybeRedirectedGetterOffset <
                AccessorInfo::kSetterOffset);
  static_assert(AccessorInfo::kSetterOffset < AccessorInfo::kFlagsOffset);
  static_assert(AccessorInfo::kFlagsOffset < AccessorInfo::kSize);

  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IteratePointers(obj, HeapObject::kHeaderSize,
                    AccessorInfo::kEndOfStrongFieldsOffset, v);
    v->VisitExternalPointer(obj,
                            obj->RawExternalPointerField(
                                AccessorInfo::kMaybeRedirectedGetterOffset),
                            kAccessorInfoGetterTag);
    v->VisitExternalPointer(
        obj, obj->RawExternalPointerField(AccessorInfo::kSetterOffset),
        kAccessorInfoSetterTag);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return kSize;
  }
};

class CallHandlerInfo::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static_assert(CallHandlerInfo::kEndOfStrongFieldsOffset ==
                CallHandlerInfo::kMaybeRedirectedCallbackOffset);

  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IteratePointers(obj, HeapObject::kHeaderSize,
                    CallHandlerInfo::kEndOfStrongFieldsOffset, v);
    v->VisitExternalPointer(
        obj,
        obj->RawExternalPointerField(
            CallHandlerInfo::kMaybeRedirectedCallbackOffset),
        kCallHandlerInfoCallbackTag);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return kSize;
  }
};

#include "torque-generated/objects-body-descriptors-inl.inc"

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_OBJECTS_BODY_DESCRIPTORS_INL_H_
