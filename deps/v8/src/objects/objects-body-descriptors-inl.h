// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_OBJECTS_BODY_DESCRIPTORS_INL_H_
#define V8_OBJECTS_OBJECTS_BODY_DESCRIPTORS_INL_H_

#include "src/objects/objects-body-descriptors.h"
// Include the non-inl header before the rest of the headers.

#include <algorithm>
#include <cstddef>

#include "include/v8-internal.h"
#include "src/base/logging.h"
#include "src/codegen/reloc-info.h"
#include "src/common/globals.h"
#include "src/heap/heap-layout-inl.h"
#include "src/ic/handler-configuration.h"
#include "src/objects/arguments-inl.h"
#include "src/objects/bigint.h"
#include "src/objects/call-site-info-inl.h"
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
#include "src/objects/js-objects.h"
#include "src/objects/js-proxy.h"
#include "src/objects/js-weak-refs.h"
#include "src/objects/literal-objects.h"
#include "src/objects/megadom-handler-inl.h"
#include "src/objects/ordered-hash-table-inl.h"
#include "src/objects/property-descriptor-object.h"
#include "src/objects/scope-info-inl.h"
#include "src/objects/sort-state.h"
#include "src/objects/source-text-module.h"
#include "src/objects/swiss-name-dictionary-inl.h"
#include "src/objects/synthetic-module.h"
#include "src/objects/tagged-field.h"
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
          obj, obj->RawExternalPointerField(
                   offset + EmbedderDataSlot::kExternalPointerOffset,
                   {kFirstEmbedderDataTag, kLastEmbedderDataTag}));
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
// static
void BodyDescriptorBase::IterateJSObjectBodyWithoutEmbedderFieldsImpl(
    Tagged<Map> map, Tagged<HeapObject> obj, int start_offset, int end_offset,
    ObjectVisitor* v) {
  // This body iteration assumes that there's no embedder fields.
  DCHECK_IMPLIES(JSObject::MayHaveEmbedderFields(map),
                 UncheckedCast<JSObject>(obj)->GetEmbedderFieldCount(map) == 0);
  IteratePointers(obj, start_offset, end_offset, v);
}

// This is a BodyDescriptor helper for usage within JSAPIObjectWithEmbedderSlots
// and JSSpecialObject. The class hierarchies are separate but
// `kCppHeapWrappableOffset` is the same for both.
class JSAPIObjectWithEmbedderSlotsOrJSSpecialObjectBodyDescriptor
    : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateJSAPIObjectWithEmbedderSlotsHeader(
      Tagged<Map> map, Tagged<HeapObject> obj, int object_size,
      ObjectVisitor* v) {
    // Visit JSObject header.
    IteratePointers(obj, offsetof(JSObject, properties_or_hash_),
                    JSObject::kEndOfStrongFieldsOffset, v);

    // Visit JSAPIObjectWithEmbedderSlots or JSSpecialObject header.
    static_assert(JSObject::kEndOfStrongFieldsOffset ==
                  JSAPIObjectWithEmbedderSlots::kCppHeapWrappableOffset);
    static_assert(JSAPIObjectWithEmbedderSlots::kCppHeapWrappableOffset ==
                  JSSpecialObject::kCppHeapWrappableOffset);
    static_assert(JSAPIObjectWithEmbedderSlots::kCppHeapWrappableOffset +
                      kCppHeapPointerSlotSize ==
                  JSAPIObjectWithEmbedderSlots::kHeaderSize);
    v->VisitCppHeapPointer(
        obj, obj->RawCppHeapPointerField(
                 JSAPIObjectWithEmbedderSlots::kCppHeapWrappableOffset));
  }

  template <typename ConcreteType, typename ObjectVisitor>
  static inline void IterateJSAPIObjectWithEmbedderSlotsTail(
      Tagged<Map> map, Tagged<HeapObject> obj, int object_size,
      ObjectVisitor* v) {
    // Visit the tail of JSObject with possible embedder fields and in-object
    // properties. Note that embedder fields are processed in the JSObject base
    // class as there's other object hierarchies that contain embedder fields as
    // well.
    IterateJSObjectBodyImpl(map, obj, ConcreteType::kHeaderSize, object_size,
                            v);
  }

  template <typename ConcreteType, typename ObjectVisitor>
  static inline void IterateJSAPIObjectWithoutEmbedderSlotsTail(
      Tagged<Map> map, Tagged<HeapObject> obj, int object_size,
      ObjectVisitor* v) {
    IterateJSObjectBodyWithoutEmbedderFieldsImpl(
        map, obj, ConcreteType::kHeaderSize, object_size, v);
  }

  static constexpr int kHeaderSize = JSSpecialObject::kHeaderSize;

  static_assert(JSAPIObjectWithEmbedderSlots::kHeaderSize ==
                JSSpecialObject::kHeaderSize);
  static_assert(Internals::kJSAPIObjectWithEmbedderSlotsHeaderSize ==
                JSSpecialObject::kHeaderSize);
};

class JSAPIObjectWithEmbedderSlots::BodyDescriptor
    : public JSAPIObjectWithEmbedderSlotsOrJSSpecialObjectBodyDescriptor {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IterateJSAPIObjectWithEmbedderSlotsHeader(map, obj, object_size, v);
    IterateJSAPIObjectWithEmbedderSlotsTail<
        JSAPIObjectWithEmbedderSlotsOrJSSpecialObjectBodyDescriptor>(
        map, obj, object_size, v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return map->instance_size();
  }
};

class CppHeapExternalObject::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    static_assert(CppHeapExternalObject::kCppHeapWrappableOffset +
                      kCppHeapPointerSlotSize ==
                  CppHeapExternalObject::kHeaderSize);
    v->VisitCppHeapPointer(obj,
                           obj->RawCppHeapPointerField(
                               CppHeapExternalObject::kCppHeapWrappableOffset));
    DCHECK_EQ(object_size, kHeaderSize);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    DCHECK_EQ(map->instance_size(), kHeaderSize);
    return kHeaderSize;
  }
};

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
void BodyDescriptorBase::IterateTrustedPointer(
    Tagged<HeapObject> obj, int offset, ObjectVisitor* v,
    IndirectPointerMode mode, IndirectPointerTagRange tag_range) {
#ifdef V8_ENABLE_SANDBOX
  v->VisitIndirectPointer(obj, obj->RawIndirectPointerField(offset, tag_range),
                          mode);
#else
  if (mode == IndirectPointerMode::kStrong) {
    IteratePointer(obj, offset, v);
  } else {
    IterateCustomWeakPointer(obj, offset, v);
  }
#endif
}

template <typename ObjectVisitor, typename T, IndirectPointerTagRange kTagRange>
void BodyDescriptorBase::IterateTrustedPointer(
    Tagged<HeapObject> obj, TrustedPointerMember<T, kTagRange>* member,
    ObjectVisitor* v, IndirectPointerMode mode) {
#ifdef V8_ENABLE_SANDBOX
  v->VisitIndirectPointer(obj, IndirectPointerSlot(member), mode);
#else
  if (mode == IndirectPointerMode::kStrong) {
    v->VisitPointer(obj, ObjectSlot(member));
  } else {
    v->VisitCustomWeakPointer(obj, ObjectSlot(member));
  }
#endif
}

template <typename ObjectVisitor>
void BodyDescriptorBase::IterateCodePointer(Tagged<HeapObject> obj, int offset,
                                            ObjectVisitor* v,
                                            IndirectPointerMode mode) {
  IterateTrustedPointer(obj, offset, v, mode, kCodeIndirectPointerTag);
}

template <typename ObjectVisitor>
void BodyDescriptorBase::IterateCodePointer(
    Tagged<HeapObject> obj,
    TrustedPointerMember<Code, kCodeIndirectPointerTag>* member,
    ObjectVisitor* v, IndirectPointerMode mode) {
  IterateTrustedPointer(obj, member, v, mode);
}

template <typename ObjectVisitor>
void BodyDescriptorBase::IterateSelfIndirectPointer(
    Tagged<HeapObject> obj, IndirectPointerTagRange tag_range,
    ObjectVisitor* v) {
#ifdef V8_ENABLE_SANDBOX
  v->VisitTrustedPointerTableEntry(
      obj, obj->RawIndirectPointerField(
               ExposedTrustedObject::kSelfIndirectPointerOffset, tag_range));
#endif
}

template <typename ObjectVisitor>
void BodyDescriptorBase::IterateProtectedPointer(Tagged<HeapObject> obj,
                                                 int offset, ObjectVisitor* v) {
  DCHECK(IsTrustedObject(obj));
  Tagged<TrustedObject> host = TrustedCast<TrustedObject>(obj);
  v->VisitProtectedPointer(host, host->RawProtectedPointerField(offset));
}

template <typename ObjectVisitor>
void BodyDescriptorBase::IterateJSDispatchEntry(Tagged<HeapObject> obj,
                                                int offset, ObjectVisitor* v) {
  JSDispatchHandle handle(
      obj->Relaxed_ReadField<JSDispatchHandle::underlying_type>(offset));
  v->VisitJSDispatchTableEntry(obj, handle);
}

class HeapNumber::BodyDescriptor final : public DataOnlyBodyDescriptor {
 public:
  static constexpr int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return sizeof(HeapNumber);
  }
};

#define TURBOSHAFT_FIXED_DATA_BODY_DESCRIPTOR(T)                   \
  class T::BodyDescriptor final : public DataOnlyBodyDescriptor {  \
   public:                                                         \
    static constexpr int SizeOf(Tagged<Map>, Tagged<HeapObject>) { \
      return sizeof(T);                                            \
    }                                                              \
  }
TURBOSHAFT_FIXED_DATA_BODY_DESCRIPTOR(TurboshaftWord32RangeType);
TURBOSHAFT_FIXED_DATA_BODY_DESCRIPTOR(TurboshaftWord64RangeType);
TURBOSHAFT_FIXED_DATA_BODY_DESCRIPTOR(TurboshaftFloat64RangeType);
#undef TURBOSHAFT_FIXED_DATA_BODY_DESCRIPTOR

class TurboshaftWord32SetType::BodyDescriptor final
    : public DataOnlyBodyDescriptor {
 public:
  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> obj) {
    return TurboshaftWord32SetType::SizeFor(
        UncheckedCast<TurboshaftWord32SetType>(obj)->set_size());
  }
};

class TurboshaftWord64SetType::BodyDescriptor final
    : public DataOnlyBodyDescriptor {
 public:
  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> obj) {
    return TurboshaftWord64SetType::SizeFor(
        UncheckedCast<TurboshaftWord64SetType>(obj)->set_size());
  }
};

class TurboshaftFloat64SetType::BodyDescriptor final
    : public DataOnlyBodyDescriptor {
 public:
  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> obj) {
    return TurboshaftFloat64SetType::SizeFor(
        UncheckedCast<TurboshaftFloat64SetType>(obj)->set_size());
  }
};

class TurbofanBitsetType::BodyDescriptor final : public DataOnlyBodyDescriptor {
 public:
  static constexpr int SizeOf(Tagged<Map>, Tagged<HeapObject>) {
    return sizeof(TurbofanBitsetType);
  }
};

class TurbofanRangeType::BodyDescriptor final : public DataOnlyBodyDescriptor {
 public:
  static constexpr int SizeOf(Tagged<Map>, Tagged<HeapObject>) {
    return sizeof(TurbofanRangeType);
  }
};

class TurbofanOtherNumberConstantType::BodyDescriptor final
    : public DataOnlyBodyDescriptor {
 public:
  static constexpr int SizeOf(Tagged<Map>, Tagged<HeapObject>) {
    return sizeof(TurbofanOtherNumberConstantType);
  }
};

class TurbofanUnionType::BodyDescriptor final
    : public FixedBodyDescriptor<offsetof(TurbofanUnionType, type1_),
                                 sizeof(TurbofanUnionType),
                                 sizeof(TurbofanUnionType)> {};

class TurbofanHeapConstantType::BodyDescriptor final
    : public FixedBodyDescriptor<offsetof(TurbofanHeapConstantType, constant_),
                                 sizeof(TurbofanHeapConstantType),
                                 sizeof(TurbofanHeapConstantType)> {};

class OnHeapBasicBlockProfilerData::BodyDescriptor final
    : public FixedBodyDescriptor<offsetof(OnHeapBasicBlockProfilerData,
                                          block_ids_),
                                 sizeof(OnHeapBasicBlockProfilerData),
                                 sizeof(OnHeapBasicBlockProfilerData)> {};

class SortState::BodyDescriptor final
    : public FixedBodyDescriptor<offsetof(SortState, receiver_),
                                 sizeof(SortState), sizeof(SortState)> {};

#if V8_ENABLE_WEBASSEMBLY
// Only cached_map_ is weak (Weak<Map>|Null); signature_ and callback_data_ are
// strong but visiting them as MaybeObject is safe because strong tagged values
// decode back as strong.
class WasmFastApiCallData::BodyDescriptor final
    : public FixedWeakBodyDescriptor<offsetof(WasmFastApiCallData, signature_),
                                     sizeof(WasmFastApiCallData),
                                     sizeof(WasmFastApiCallData)> {};

class WasmStringViewIter::BodyDescriptor final
    : public FixedBodyDescriptor<offsetof(WasmStringViewIter, string_),
                                 offsetof(WasmStringViewIter, offset_),
                                 sizeof(WasmStringViewIter)> {};
#endif  // V8_ENABLE_WEBASSEMBLY

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
    return UncheckedCast<FreeSpace>(raw_object)->Size();
  }
};

class JSObject::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static const int kStartOffset = offsetof(JSReceiver, properties_or_hash_);

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
  static const int kStartOffset = offsetof(JSReceiver, properties_or_hash_);

  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IteratePointers(obj, kStartOffset, object_size, v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return map->instance_size();
  }
};

class JSMessageObject::BodyDescriptor final
    : public FixedBodyDescriptor<offsetof(JSMessageObject, properties_or_hash_),
                                 offsetof(JSMessageObject, bytecode_offset_) +
                                     kTaggedSize,
                                 sizeof(JSMessageObject)> {};

class JSDate::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IteratePointers(obj, offsetof(JSDate, properties_or_hash_),
                    offsetof(JSDate, value_), v);
    IterateJSObjectBodyImpl(map, obj, offsetof(JSDate, year_), object_size, v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return map->instance_size();
  }
};

class JSRegExp::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static const int kStartOffset = offsetof(JSReceiver, properties_or_hash_);

  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    Tagged<JSRegExp> regexp = UncheckedCast<JSRegExp>(obj);
    IteratePointers(obj, offsetof(JSReceiver, properties_or_hash_),
                    JSObject::kHeaderSize, v);
    IterateTrustedPointer(obj, &regexp->data_, v, IndirectPointerMode::kStrong);
    IteratePointer(obj, JSRegExp::kFlagsOffset, v);
    IterateJSObjectBodyImpl(map, obj, JSRegExp::kHeaderSize, object_size, v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return map->instance_size();
  }
};

class RegExpData::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    // If new pointers are added to RegExpData, make sure to also add them to
    // the subclasses descriptors (AtomRegExpData and IrRegExpData).
    // We don't directly call the base class IterateBody, as in the future
    // the subclasses will have a different indirect pointer tag from the base
    // class (once inheritance hierarchies are supported for indirect pointer
    // tags).
    IterateSelfIndirectPointer(obj, kRegExpDataIndirectPointerTag, v);
    IteratePointer(obj, offsetof(RegExpData, original_source_), v);
    IteratePointer(obj, offsetof(RegExpData, escaped_source_), v);
    IteratePointer(obj, offsetof(RegExpData, wrapper_), v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> obj) {
    return sizeof(RegExpData);
  }
};

class AtomRegExpData::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IterateSelfIndirectPointer(obj, kRegExpDataIndirectPointerTag, v);

    IteratePointer(obj, offsetof(AtomRegExpData, original_source_), v);
    IteratePointer(obj, offsetof(AtomRegExpData, escaped_source_), v);
    IteratePointer(obj, offsetof(AtomRegExpData, wrapper_), v);

    IteratePointer(obj, offsetof(AtomRegExpData, pattern_), v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> obj) {
    return sizeof(AtomRegExpData);
  }
};

class IrRegExpData::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IterateSelfIndirectPointer(obj, kRegExpDataIndirectPointerTag, v);

    IteratePointer(obj, offsetof(IrRegExpData, original_source_), v);
    IteratePointer(obj, offsetof(IrRegExpData, escaped_source_), v);
    IteratePointer(obj, offsetof(IrRegExpData, wrapper_), v);

    IterateProtectedPointer(obj, offsetof(IrRegExpData, latin1_bytecode_), v);
    IterateProtectedPointer(obj, offsetof(IrRegExpData, uc16_bytecode_), v);
    IterateProtectedPointer(obj, offsetof(IrRegExpData, capture_name_map_), v);
    IterateCodePointer(obj, &TrustedCast<IrRegExpData>(obj)->latin1_code_, v,
                       IndirectPointerMode::kStrong);
    IterateCodePointer(obj, &TrustedCast<IrRegExpData>(obj)->uc16_code_, v,
                       IndirectPointerMode::kStrong);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> obj) {
    return sizeof(IrRegExpData);
  }
};

class RegExpDataWrapper::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IterateTrustedPointer(obj, &Cast<RegExpDataWrapper>(obj)->data_, v,
                          IndirectPointerMode::kStrong);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> obj) {
    return sizeof(RegExpDataWrapper);
  }
};

// ScopeInfo's GC-visible tagged region starts at kFirstTaggedSlotOffset
// (after the non-tagged flags + optional padding header) and runs to
// the end of the variable-length tail.
class ScopeInfo::BodyDescriptor final
    : public SuffixRangeBodyDescriptor<ScopeInfo::kFirstTaggedSlotOffset> {
 public:
  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> raw_object) {
    return UncheckedCast<ScopeInfo>(raw_object)->AllocatedSize();
  }
};

// FeedbackVector has three strong header slots (shared_function_info_,
// closure_feedback_cell_array_, parent_feedback_cell_) followed by a
// variable-length maybe-weak tail of feedback slots.
class FeedbackVector::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IteratePointers(obj, FeedbackVector::kStartOfStrongFieldsOffset,
                    FeedbackVector::kHeaderSize, v);
    IterateMaybeWeakPointers(obj, FeedbackVector::kHeaderSize, object_size, v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> obj) {
    return FeedbackVector::SizeFor(
        UncheckedCast<FeedbackVector>(obj)->length());
  }
};

// DescriptorArray has a strong enum_cache_ header slot followed by a
// variable-length tail of (key, details, value) triples. Within each entry
// key and details are always strong; only the value slot may hold a weak
// reference to a Map.
// Note: the marking visitor (see MarkingVisitorBase::VisitDescriptorArray)
// bypasses this BodyDescriptor's IterateBody for incremental marking; this
// descriptor is still used by other paths (size queries, non-incremental
// visitors).
class DescriptorArray::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IteratePointer(obj, DescriptorArray::kEnumCacheOffset, v);
    const int nof_all_descriptors =
        UncheckedCast<DescriptorArray>(obj)->number_of_all_descriptors();
    for (int i = 0; i < nof_all_descriptors; ++i) {
      const int entry_offset = DescriptorArray::OffsetOfDescriptorAt(i);
      IteratePointers(obj, entry_offset + DescriptorArray::kEntryKeyOffset,
                      entry_offset + DescriptorArray::kEntryValueOffset, v);
      IterateMaybeWeakPointer(
          obj, entry_offset + DescriptorArray::kEntryValueOffset, v);
    }
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> obj) {
    return DescriptorArray::SizeFor(
        UncheckedCast<DescriptorArray>(obj)->number_of_all_descriptors());
  }
};

class WeakCell::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static constexpr int kTargetOffset = offsetof(WeakCell, target_);
  static constexpr int kUnregisterTokenOffset =
      offsetof(WeakCell, unregister_token_);
  static_assert(kUnregisterTokenOffset == kTargetOffset + kTaggedSize);

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
    IteratePointers(obj, offsetof(JSWeakRef, properties_or_hash_),
                    offsetof(JSWeakRef, target_), v);
    IterateCustomWeakPointer(obj, offsetof(JSWeakRef, target_), v);
    IterateJSObjectBodyImpl(
        map, obj, offsetof(JSWeakRef, target_) + kTaggedSize, object_size, v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return map->instance_size();
  }
};

class JSProxy::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    // Iterate properties_or_hash_, target_, handler_; skip non-tagged flags_.
    IteratePointers(obj, offsetof(JSProxy, properties_or_hash_),
                    offsetof(JSProxy, flags_), v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return sizeof(JSProxy);
  }
};

class JSFinalizationRegistry::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IteratePointers(obj, offsetof(JSFinalizationRegistry, properties_or_hash_),
                    offsetof(JSFinalizationRegistry, next_dirty_), v);
    IterateCustomWeakPointer(obj, offsetof(JSFinalizationRegistry, next_dirty_),
                             v);
    IterateJSObjectBodyImpl(
        map, obj, offsetof(JSFinalizationRegistry, next_dirty_) + kTaggedSize,
        object_size, v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return map->instance_size();
  }
};

class AllocationSite::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static constexpr int kCommonPointerFieldsStart = sizeof(HeapObjectLayout);
  static constexpr int kCommonPointerFieldsEnd =
      offsetof(AllocationSite, dependent_code_) + kTaggedSize;

  static_assert(kCommonPointerFieldsEnd ==
                offsetof(AllocationSite, pretenure_data_));
  static_assert(offsetof(AllocationSite, pretenure_data_) + kInt32Size ==
                offsetof(AllocationSite, pretenure_create_count_));
  static_assert(offsetof(AllocationSite, pretenure_create_count_) +
                    kInt32Size ==
                offsetof(AllocationSiteWithWeakNext, weak_next_));

  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    // Iterate over all the common pointer fields
    IteratePointers(obj, kCommonPointerFieldsStart, kCommonPointerFieldsEnd, v);
    // Skip PretenureDataOffset and PretenureCreateCount which are Int32 fields.
    // Visit weak_next only if it has weak_next field.
    if (object_size == sizeof(AllocationSiteWithWeakNext)) {
      IterateCustomWeakPointers(
          obj, offsetof(AllocationSiteWithWeakNext, weak_next_),
          sizeof(AllocationSiteWithWeakNext), v);
    }
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return map->instance_size();
  }
};

class JSFunction::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static const int kStartOffset = JSObject::BodyDescriptor::kStartOffset;
  static const int kDispatchHandleOffset = JSFunction::kDispatchHandleOffset;

  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    // Iterate JSFunction header fields up to dispatch handle field.
    IteratePointers(obj, kStartOffset, kDispatchHandleOffset, v);

    IterateJSDispatchEntry(obj, kDispatchHandleOffset, v);

    // Iterate rest of the header fields and JSObject body.
    IterateJSObjectBodyImpl(map, obj, kDispatchHandleOffset + kTaggedSize,
                            object_size, v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return map->instance_size();
  }
};

class JSArrayBuffer::BodyDescriptor final
    : public JSAPIObjectWithEmbedderSlots::BodyDescriptor {
 public:
  using Base = JSAPIObjectWithEmbedderSlots::BodyDescriptor;

  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    // JSObject with wrapper field.
    IterateJSAPIObjectWithEmbedderSlotsHeader(map, obj, object_size, v);
    // Only weak field is views_or_detach_key_.
    IterateMaybeWeakPointers(
        obj, JSArrayBuffer::kViewsOrDetachKeyOffset,
        JSArrayBuffer::kViewsOrDetachKeyOffset + kTaggedSize, v);
    v->VisitExternalPointer(
        obj, obj->RawExternalPointerField(JSArrayBuffer::kExtensionOffset,
                                          kArrayBufferExtensionTag));
    // JSObject tail: possible embedder fields + in-object properties.
    if constexpr (JSArrayBuffer::kContainsEmbedderFields) {
      IterateJSAPIObjectWithEmbedderSlotsTail<JSArrayBuffer>(map, obj,
                                                             object_size, v);
    } else {
      IterateJSAPIObjectWithoutEmbedderSlotsTail<JSArrayBuffer>(map, obj,
                                                                object_size, v);
    }
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return map->instance_size();
  }
};

class JSArrayBufferView::BodyDescriptor
    : public JSAPIObjectWithEmbedderSlots::BodyDescriptor {
 public:
  using Base = JSAPIObjectWithEmbedderSlots::BodyDescriptor;

  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    // JSObject with wrapper field.
    IterateJSAPIObjectWithEmbedderSlotsHeader(map, obj, object_size, v);
    // buffer_ is the single strong tagged field.
    IteratePointers(obj, JSArrayBufferView::kBufferOffset,
                    JSArrayBufferView::kBufferOffset + kTaggedSize, v);
  }
};

class JSTypedArray::BodyDescriptor : public JSArrayBufferView::BodyDescriptor {
 public:
  using Base = JSArrayBufferView::BodyDescriptor;

  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    // JSArrayBufferView (including JSObject).
    Base::IterateBody(map, obj, object_size, v);
    // JSTypedArray: base_pointer_ is the only tagged field (raw_length_ and
    // external_pointer_ are raw).
    IteratePointers(obj, JSTypedArray::kBasePointerOffset,
                    JSTypedArray::kBasePointerOffset + kTaggedSize, v);

    // JSObject tail: possible embedder fields + in-object properties.
    if constexpr (JSTypedArray::kContainsEmbedderFields) {
      IterateJSAPIObjectWithEmbedderSlotsTail<JSTypedArray>(map, obj,
                                                            object_size, v);
    } else {
      IterateJSAPIObjectWithoutEmbedderSlotsTail<JSTypedArray>(map, obj,
                                                               object_size, v);
    }
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return map->instance_size();
  }
};

class JSDataViewOrRabGsabDataView::BodyDescriptor final
    : public JSArrayBufferView::BodyDescriptor {
 public:
  using Base = JSArrayBufferView::BodyDescriptor;

  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    // JSArrayBufferView (including JSObject).
    Base::IterateBody(map, obj, object_size, v);
    // JSDataViewOrRabGsabDataView: data_pointer_ is raw, no tagged fields
    // beyond what JSArrayBufferView already iterates.
    // JSObject tail: possible embedder fields + in-object properties.
    if constexpr (JSDataViewOrRabGsabDataView::kContainsEmbedderFields) {
      IterateJSAPIObjectWithEmbedderSlotsTail<JSDataViewOrRabGsabDataView>(
          map, obj, object_size, v);
    } else {
      IterateJSAPIObjectWithoutEmbedderSlotsTail<JSDataViewOrRabGsabDataView>(
          map, obj, object_size, v);
    }
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
    DCHECK_EQ(0, map->GetInObjectProperties());
    IteratePointers(obj, offsetof(JSReceiver, properties_or_hash_),
                    JSObject::kHeaderSize, v);
    Tagged<JSExternalObject> external = UncheckedCast<JSExternalObject>(obj);
    v->VisitExternalPointer(
        obj,
        ExternalPointerSlot(&external->value_, kExternalObjectValueTagRange));
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
    Tagged<Foreign> foreign = UncheckedCast<Foreign>(obj);
    ExternalPointerTagRange tag_range =
        HeapLayout::InWritableSharedSpace(obj)
            ? kAnySharedManagedExternalPointerTagRange
            : kAnyForeignExternalPointerTagRange;
    v->VisitExternalPointer(
        obj, ExternalPointerSlot(&foreign->foreign_address_, tag_range));
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return sizeof(Foreign);
  }
};

template <typename Derived>
class V8_EXPORT_PRIVATE SmallOrderedHashTableImpl<Derived>::BodyDescriptor final
    : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    Tagged<Derived> table = Cast<Derived>(obj);
    int start_offset = DataTableStartOffset();
    int end_offset = table->GetBucketsStartOffset();
    IteratePointers(obj, start_offset, end_offset, v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> obj) {
    Tagged<Derived> table = Cast<Derived>(obj);
    return Derived::SizeFor(table->Capacity());
  }
};

class V8_EXPORT_PRIVATE SwissNameDictionary::BodyDescriptor final
    : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    Tagged<SwissNameDictionary> table = UncheckedCast<SwissNameDictionary>(obj);
    static_assert(MetaTablePointerOffset() + kTaggedSize ==
                  DataTableStartOffset());
    int start_offset = MetaTablePointerOffset();
    int end_offset = table->DataTableEndOffset(table->Capacity());
    IteratePointers(obj, start_offset, end_offset, v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> obj) {
    Tagged<SwissNameDictionary> table = UncheckedCast<SwissNameDictionary>(obj);
    return SwissNameDictionary::SizeFor(table->Capacity());
  }
};

class ByteArray::BodyDescriptor final : public DataOnlyBodyDescriptor {
 public:
  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> obj) {
    return UncheckedCast<ByteArray>(obj)->AllocatedSize();
  }
};

class TrustedByteArray::BodyDescriptor final : public DataOnlyBodyDescriptor {
 public:
  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> obj) {
    return UncheckedCast<TrustedByteArray>(obj)->AllocatedSize();
  }
};

class BytecodeArray::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IterateSelfIndirectPointer(obj, kBytecodeArrayIndirectPointerTag, v);
    IteratePointer(obj, offsetof(BytecodeArray, wrapper_), v);
    IterateProtectedPointer(obj,
                            offsetof(BytecodeArray, source_position_table_), v);
    IterateProtectedPointer(obj, offsetof(BytecodeArray, handler_table_), v);
    IterateProtectedPointer(obj, offsetof(BytecodeArray, constant_pool_), v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<BytecodeArray> obj) {
    return BytecodeArray::SizeFor(obj->length(kAcquireLoad));
  }
};

class BytecodeWrapper::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    Tagged<BytecodeWrapper> wrapper = UncheckedCast<BytecodeWrapper>(obj);
    IterateTrustedPointer(obj, &wrapper->bytecode_, v,
                          IndirectPointerMode::kStrong);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> obj) {
    return sizeof(BytecodeWrapper);
  }
};

class BigInt::BodyDescriptor final : public DataOnlyBodyDescriptor {
 public:
  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> obj) {
    return BigInt::SizeFor(UncheckedCast<BigInt>(obj)->length(kAcquireLoad));
  }
};

class FixedDoubleArray::BodyDescriptor final : public DataOnlyBodyDescriptor {
 public:
  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> obj) {
    return UncheckedCast<FixedDoubleArray>(obj)->AllocatedSize();
  }
};

class FeedbackMetadata::BodyDescriptor final : public DataOnlyBodyDescriptor {
 public:
  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> obj) {
    return UncheckedCast<FeedbackMetadata>(obj)->AllocatedSize();
  }
};

class PreparseData::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    Tagged<PreparseData> data = UncheckedCast<PreparseData>(obj);
    int start_offset = data->children_start_offset();
    int end_offset = start_offset + data->children_length() * kTaggedSize;
    IteratePointers(obj, start_offset, end_offset, v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> obj) {
    Tagged<PreparseData> data = UncheckedCast<PreparseData>(obj);
    return PreparseData::SizeFor(data->data_length(), data->children_length());
  }
};

class InterpreterData::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IterateSelfIndirectPointer(obj, kInterpreterDataIndirectPointerTag, v);
    IterateProtectedPointer(obj, offsetof(InterpreterData, bytecode_array_), v);
    IterateProtectedPointer(
        obj, offsetof(InterpreterData, interpreter_trampoline_), v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> raw_object) {
    return sizeof(InterpreterData);
  }
};

class UncompiledDataWithoutPreparseData::BodyDescriptor final
    : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IterateSelfIndirectPointer(obj, kUncompiledDataIndirectPointerTag, v);
    IteratePointer(
        obj, offsetof(UncompiledDataWithoutPreparseData, inferred_name_), v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> raw_object) {
    return sizeof(UncompiledDataWithoutPreparseData);
  }
};

class UncompiledDataWithPreparseData::BodyDescriptor final
    : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IterateSelfIndirectPointer(obj, kUncompiledDataIndirectPointerTag, v);
    IteratePointer(obj,
                   offsetof(UncompiledDataWithPreparseData, inferred_name_), v);
    IteratePointer(obj,
                   offsetof(UncompiledDataWithPreparseData, preparse_data_), v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> raw_object) {
    return sizeof(UncompiledDataWithPreparseData);
  }
};

class UncompiledDataWithoutPreparseDataWithJob::BodyDescriptor final
    : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IterateSelfIndirectPointer(obj, kUncompiledDataIndirectPointerTag, v);
    IteratePointer(
        obj, offsetof(UncompiledDataWithoutPreparseDataWithJob, inferred_name_),
        v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> raw_object) {
    return sizeof(UncompiledDataWithoutPreparseDataWithJob);
  }
};

class UncompiledDataWithPreparseDataAndJob::BodyDescriptor final
    : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IterateSelfIndirectPointer(obj, kUncompiledDataIndirectPointerTag, v);
    IteratePointer(
        obj, offsetof(UncompiledDataWithPreparseDataAndJob, inferred_name_), v);
    IteratePointer(
        obj, offsetof(UncompiledDataWithPreparseDataAndJob, preparse_data_), v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> raw_object) {
    return sizeof(UncompiledDataWithPreparseDataAndJob);
  }
};

class SharedFunctionInfo::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IterateTrustedPointer(obj, kTrustedFunctionDataOffset, v,
                          IndirectPointerMode::kCustom,
                          kTrustedDataIndirectPointerRange);
    IteratePointers(obj, kStartOfStrongFieldsOffset, kEndOfStrongFieldsOffset,
                    v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> raw_object) {
    return kSize;
  }
};

class DebugInfo::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IteratePointers(obj, offsetof(DebugInfo, shared_),
                    offsetof(DebugInfo, coverage_info_) + kTaggedSize, v);
    Tagged<DebugInfo> debug_info = Cast<DebugInfo>(obj);
    IterateTrustedPointer(obj, &debug_info->debug_bytecode_array_, v,
                          IndirectPointerMode::kStrong);
    IterateTrustedPointer(obj, &debug_info->original_bytecode_array_, v,
                          IndirectPointerMode::kStrong);
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
    IteratePointers(obj, HeapObject::kHeaderSize, object_size, v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> obj) {
    return obj->SizeFromMap(map);
  }
};

class JSWeakCollection::BodyDescriptorImpl final : public BodyDescriptorBase {
 public:
  static_assert(offsetof(JSWeakCollection, table_) + kTaggedSize ==
                sizeof(JSWeakCollection));

  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IterateJSObjectBodyImpl(map, obj,
                            offsetof(JSWeakCollection, properties_or_hash_),
                            object_size, v);
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
    IteratePointers(obj,
                    offsetof(JSSynchronizationPrimitive, properties_or_hash_),
                    kEndOfTaggedFieldsOffset, v);
    v->VisitExternalPointer(obj,
                            obj->RawExternalPointerField(kWaiterQueueHeadOffset,
                                                         kWaiterQueueNodeTag));
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return map->instance_size();
  }
};

class Hole::BodyDescriptor : public DataOnlyBodyDescriptor {
 public:
  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return sizeof(Hole);
  }
};

#if V8_ENABLE_WEBASSEMBLY
class WasmTypeInfo::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IteratePointers(obj, WasmTypeInfo::kSupertypesLengthOffset,
                    SizeOf(map, obj), v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return Cast<WasmTypeInfo>(object)->AllocatedSize();
  }
};

class WasmFuncRef::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    Tagged<WasmFuncRef> func_ref = UncheckedCast<WasmFuncRef>(obj);
    IterateTrustedPointer(obj, &func_ref->trusted_internal_, v,
                          IndirectPointerMode::kStrong);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return WasmFuncRef::kSize;
  }
};

class WasmResumeData::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    Tagged<WasmResumeData> data = UncheckedCast<WasmResumeData>(obj);
    IteratePointer(obj, WasmResumeData::kOnResumeOffset, v);
    IterateTrustedPointer(obj, &data->trusted_suspender_, v,
                          IndirectPointerMode::kStrong);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return WasmResumeData::kSize;
  }
};

class WasmStackObject::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    v->VisitExternalPointer(
        obj, obj->RawExternalPointerField(WasmStackObject::kStackOffset,
                                          kWasmStackMemoryTag));
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return WasmStackObject::kSize;
  }
};

class WasmSuspendingObject::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IteratePointers(obj, offsetof(JSReceiver, properties_or_hash_),
                    WasmSuspendingObject::kHeaderSize, v);
    IterateJSObjectBodyImpl(map, obj, WasmSuspendingObject::kHeaderSize,
                            object_size, v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return map->instance_size();
  }
};

class WasmModuleObject::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IteratePointers(obj, offsetof(JSReceiver, properties_or_hash_),
                    WasmModuleObject::kHeaderSize, v);
    IterateJSObjectBodyImpl(map, obj, WasmModuleObject::kHeaderSize,
                            object_size, v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return map->instance_size();
  }
};

class WasmInstanceObject::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    Tagged<WasmInstanceObject> instance =
        UncheckedCast<WasmInstanceObject>(obj);
    IteratePointers(obj, offsetof(JSReceiver, properties_or_hash_),
                    WasmInstanceObject::kTrustedDataOffset, v);
    IterateTrustedPointer(obj, &instance->trusted_data_, v,
                          IndirectPointerMode::kStrong);
    IteratePointers(obj, WasmInstanceObject::kModuleObjectOffset,
                    WasmInstanceObject::kHeaderSize, v);
    IterateJSObjectBodyImpl(map, obj, WasmInstanceObject::kHeaderSize,
                            object_size, v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return map->instance_size();
  }
};

class WasmTagObject::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    Tagged<WasmTagObject> tag_object = UncheckedCast<WasmTagObject>(obj);
    IteratePointers(obj, offsetof(JSReceiver, properties_or_hash_),
                    WasmTagObject::kTrustedDataOffset, v);
    IterateTrustedPointer(obj, &tag_object->trusted_data_, v,
                          IndirectPointerMode::kStrong);
    IterateJSObjectBodyImpl(map, obj, WasmTagObject::kHeaderSize, object_size,
                            v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return map->instance_size();
  }
};

class WasmMemoryMapDescriptor::BodyDescriptor final
    : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IteratePointers(obj, JSObject::BodyDescriptor::kStartOffset,
                    WasmMemoryMapDescriptor::kMemoryOffset, v);
    IterateMaybeWeakPointer(obj, WasmMemoryMapDescriptor::kMemoryOffset, v);
    IterateJSObjectBodyImpl(map, obj, WasmMemoryMapDescriptor::kHeaderSize,
                            object_size, v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return map->instance_size();
  }
};

class WasmImportData::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IterateProtectedPointer(
        obj, offsetof(WasmImportData, protected_instance_data_), v);
    IterateProtectedPointer(
        obj, offsetof(WasmImportData, protected_call_origin_), v);
    IteratePointer(obj, offsetof(WasmImportData, native_context_), v);
    IteratePointer(obj, offsetof(WasmImportData, callable_), v);
    IteratePointer(obj, offsetof(WasmImportData, wrapper_budget_), v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return sizeof(WasmImportData);
  }
};

class WasmInternalFunction::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IterateSelfIndirectPointer(obj, kWasmInternalFunctionIndirectPointerTag, v);
    IterateProtectedPointer(
        obj, offsetof(WasmInternalFunction, protected_implicit_arg_), v);
    IteratePointer(obj, offsetof(WasmInternalFunction, external_), v);
    IteratePointer(obj, offsetof(WasmInternalFunction, function_index_), v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return sizeof(WasmInternalFunction);
  }
};

class WasmFunctionData::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    Tagged<WasmFunctionData> data = UncheckedCast<WasmFunctionData>(obj);
    IterateSelfIndirectPointer(obj, kWasmFunctionDataIndirectPointerTagRange,
                               v);
    IterateCodePointer(obj, &data->wrapper_code_, v,
                       IndirectPointerMode::kStrong);
    IteratePointer(obj, offsetof(WasmFunctionData, func_ref_), v);
    IteratePointer(obj, offsetof(WasmFunctionData, js_promise_flags_), v);
    IterateProtectedPointer(obj,
                            offsetof(WasmFunctionData, protected_internal_), v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return sizeof(WasmFunctionData);
  }
};

class WasmExportedFunctionData::BodyDescriptor final
    : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    WasmFunctionData::BodyDescriptor::IterateBody(map, obj, object_size, v);
    Tagged<WasmExportedFunctionData> data =
        UncheckedCast<WasmExportedFunctionData>(obj);
    IterateProtectedPointer(
        obj, offsetof(WasmExportedFunctionData, protected_instance_data_), v);
    IteratePointer(obj, offsetof(WasmExportedFunctionData, function_index_), v);
    IteratePointer(obj, offsetof(WasmExportedFunctionData, wrapper_budget_), v);
    IteratePointer(obj, offsetof(WasmExportedFunctionData, packed_args_size_),
                   v);
    IterateCodePointer(obj, &data->c_wrapper_code_, v,
                       IndirectPointerMode::kStrong);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return sizeof(WasmExportedFunctionData);
  }
};

class WasmCapiFunctionData::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    WasmFunctionData::BodyDescriptor::IterateBody(map, obj, object_size, v);
    IteratePointer(obj, offsetof(WasmCapiFunctionData, embedder_data_), v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return sizeof(WasmCapiFunctionData);
  }
};

class WasmSuspenderObject::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IterateSelfIndirectPointer(obj, kWasmSuspenderIndirectPointerTag, v);
    v->VisitExternalPointer(
        obj, obj->RawExternalPointerField(offsetof(WasmSuspenderObject, stack_),
                                          kWasmStackMemoryTag));
    IterateProtectedPointer(obj, offsetof(WasmSuspenderObject, parent_), v);
    IteratePointer(obj, offsetof(WasmSuspenderObject, promise_), v);
    IteratePointer(obj, offsetof(WasmSuspenderObject, resume_), v);
    IteratePointer(obj, offsetof(WasmSuspenderObject, reject_), v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return sizeof(WasmSuspenderObject);
  }
};

class WasmTrustedInstanceData::BodyDescriptor final
    : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    bool shared = HeapLayout::InAnySharedSpace(obj);
    IndirectPointerTag tag =
        shared ? kSharedWasmTrustedInstanceDataIndirectPointerTag
               : kWasmTrustedInstanceDataIndirectPointerTag;
    IterateSelfIndirectPointer(obj, tag, v);
    for (uint16_t offset : kTaggedFieldOffsets) {
      IteratePointer(obj, offset, v);
    }

    for (uint16_t offset : kProtectedFieldOffsets) {
      IterateProtectedPointer(obj, offset, v);
    }
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return kSize;
  }
};

class WasmTableObject::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    Tagged<WasmTableObject> table = UncheckedCast<WasmTableObject>(obj);
    IteratePointers(obj, JSObject::BodyDescriptor::kStartOffset,
                    WasmTableObject::kTrustedDispatchTableOffset, v);
    IterateTrustedPointer(obj, &table->trusted_dispatch_table_, v,
                          IndirectPointerMode::kStrong);
    IterateTrustedPointer(obj, &table->trusted_data_, v,
                          IndirectPointerMode::kStrong);
    IterateJSObjectBodyImpl(map, obj, WasmTableObject::kHeaderSize, object_size,
                            v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return map->instance_size();
  }
};

class WasmGlobalObject::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    Tagged<WasmGlobalObject> global = UncheckedCast<WasmGlobalObject>(obj);
    IteratePointers(obj, JSObject::BodyDescriptor::kStartOffset,
                    WasmGlobalObject::kTrustedDataOffset, v);
    IterateTrustedPointer(obj, &global->trusted_data_, v,
                          IndirectPointerMode::kStrong);
    IteratePointers(obj, WasmGlobalObject::kBufferOffset,
                    WasmGlobalObject::kHeaderSize, v);
    IterateJSObjectBodyImpl(map, obj, WasmGlobalObject::kHeaderSize,
                            object_size, v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return map->instance_size();
  }
};

class WasmDispatchTable::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    bool shared = HeapLayout::InAnySharedSpace(obj);
    IndirectPointerTag tag = shared ? kSharedWasmDispatchTableIndirectPointerTag
                                    : kWasmDispatchTableIndirectPointerTag;
    IterateSelfIndirectPointer(obj, tag, v);
    IterateProtectedPointer(obj, kProtectedOffheapDataOffset, v);
    IterateProtectedPointer(obj, kProtectedUsesOffset, v);
    int length = TrustedCast<WasmDispatchTable>(obj)->length(kAcquireLoad);
    for (int i = 0; i < length; ++i) {
      IterateProtectedPointer(obj, OffsetOf(i) + kImplicitArgBias, v);
    }
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<WasmDispatchTable> object) {
    int capacity = object->capacity();
    return SizeFor(capacity);
  }
};

class WasmDispatchTableForImports::BodyDescriptor final
    : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IterateProtectedPointer(obj, kProtectedOffheapDataOffset, v);
    int length =
        TrustedCast<WasmDispatchTableForImports>(obj)->length(kAcquireLoad);
    for (int i = 0; i < length; ++i) {
      IterateProtectedPointer(obj, OffsetOf(i) + kImplicitArgBias, v);
    }
  }

  static inline int SizeOf(Tagged<Map> map,
                           Tagged<WasmDispatchTableForImports> object) {
    int length = object->length();
    return SizeFor(length);
  }
};

class WasmArray::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    if (!WasmArray::GcSafeElementType(map).is_ref()) return;
    IteratePointers(obj, WasmArray::kHeaderSize, object_size, v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return WasmArray::SizeFor(map, UncheckedCast<WasmArray>(object)->length());
  }
};

class WasmStruct::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    Tagged<WasmStruct> wasm_struct = UncheckedCast<WasmStruct>(obj);
    const wasm::CanonicalStructType* type = WasmStruct::GcSafeType(map);
    if (type->is_descriptor()) {
      // The associated Map is stored where the first field would otherwise be.
      DCHECK(type->field_count() == 0 || type->field_offset(0) != 0);
      v->VisitPointer(wasm_struct, wasm_struct->RawField(0));
    }
    for (uint32_t i = 0; i < type->field_count(); i++) {
      int offset = static_cast<int>(type->field_offset(i));
      if (type->field(i).is_ref()) {
        v->VisitPointer(wasm_struct, wasm_struct->RawField(offset));
      }
    }
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return WasmStruct::GcSafeSize(map);
  }
};

class WasmNull::BodyDescriptor : public DataOnlyBodyDescriptor {
 public:
  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return WasmNull::kSize;
  }
};

class WasmMemoryObject::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IteratePointers(obj, JSObject::BodyDescriptor::kStartOffset,
                    WasmMemoryObject::kAddressTypeOffset, v);
    IterateJSObjectBodyImpl(map, obj, WasmMemoryObject::kHeaderSize,
                            object_size, v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return map->instance_size();
  }
};
#endif  // V8_ENABLE_WEBASSEMBLY

class ExternalString::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    Tagged<ExternalString> string = UncheckedCast<ExternalString>(obj);
    v->VisitExternalPointer(obj, ExternalPointerSlot(&string->resource_));
    if (string->is_uncached()) return;
    v->VisitExternalPointer(obj, ExternalPointerSlot(&string->resource_data_));
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    InstanceType type = map->instance_type();
    const auto is_uncached =
        (type & kUncachedExternalStringMask) == kUncachedExternalStringTag;
    return is_uncached ? sizeof(UncachedExternalString)
                       : sizeof(ExternalString);
  }
};

class CoverageInfo::BodyDescriptor final : public DataOnlyBodyDescriptor {
 public:
  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    Tagged<CoverageInfo> info = Cast<CoverageInfo>(object);
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
      RelocInfo::ModeMask(RelocInfo::WASM_STUB_CALL) |
      RelocInfo::ModeMask(RelocInfo::JS_DISPATCH_HANDLE);

  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 ObjectVisitor* v) {
    IterateProtectedPointer(obj, kCodeOffset, v);
    IterateProtectedPointer(obj, kRelocationInfoOffset, v);

    Tagged<InstructionStream> istream = UncheckedCast<InstructionStream>(obj);
    if (istream->IsFullyInitialized()) {
      RelocIterator it(istream, kRelocModeMask);
      v->VisitRelocInfo(istream, &it);
    }
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IterateBody(map, obj, v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return UncheckedCast<InstructionStream>(object)->Size();
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
    if (IsExtendedMap(UncheckedCast<Map>(obj))) {
      // Extended maps have additional fields after the transitions or
      // prototype info.
      IteratePointers(obj, ExtendedMap::kStartOfStrongExtendedFieldsOffset,
                      object_size, v);
    }
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> obj) {
    return UncheckedCast<Map>(obj)->AllocatedSize();
  }
};

class DataHandler::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    static_assert(
        offsetof(DataHandler, smi_handler_) < OFFSET_OF_DATA_START(DataHandler),
        "Field order must be in sync with this iteration code");
    IteratePointers(obj, offsetof(DataHandler, smi_handler_),
                    OFFSET_OF_DATA_START(DataHandler), v);
    IterateMaybeWeakPointers(obj, OFFSET_OF_DATA_START(DataHandler),
                             object_size, v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return object->SizeFromMap(map);
  }
};

// Covers the non-NativeContext leaves (Block / Script / Function / ...),
// whose slots are all strong. NativeContext has its own descriptor below
// to cover the weak tail slots that only exist past MIN_CONTEXT_EXTENDED_SLOTS.
class Context::BodyDescriptor final
    : public SuffixRangeBodyDescriptor<OFFSET_OF_DATA_START(Context)> {
 public:
  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> raw_object) {
    return UncheckedCast<Context>(raw_object)->AllocatedSize();
  }
};

class NativeContext::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    // Strong fields start at elements[0]; length_ is a Smi before that and
    // doesn't need visitation.
    IteratePointers(obj, OFFSET_OF_DATA_START(Context),
                    NativeContext::kEndOfStrongFieldsOffset, v);
    IterateCustomWeakPointers(obj, NativeContext::kStartOfWeakFieldsOffset,
                              NativeContext::kEndOfWeakFieldsOffset, v);
    v->VisitExternalPointer(
        obj, obj->RawExternalPointerField(kMicrotaskQueueOffset,
                                          kNativeContextMicrotaskQueueTag));
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
    IterateSelfIndirectPointer(obj, kCodeIndirectPointerTag, v);
    IterateProtectedPointer(
        obj, Code::kDeoptimizationDataOrInterpreterDataOffset, v);
    IterateProtectedPointer(obj, Code::kPositionTableOffset, v);
    IteratePointers(obj, Code::kStartOfStrongFieldsOffset,
                    Code::kEndOfStrongFieldsWithMainCageBaseOffset, v);

    static_assert(Code::kEndOfStrongFieldsWithMainCageBaseOffset ==
                  Code::kInstructionStreamOffset);

    IterateJSDispatchEntry(obj, kDispatchHandleOffset, v);

    v->VisitInstructionStreamPointer(
        TrustedCast<Code>(obj),
        obj->RawInstructionStreamField(kInstructionStreamOffset));
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return Code::kSize;
  }
};

class CodeWrapper::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    Tagged<CodeWrapper> wrapper = UncheckedCast<CodeWrapper>(obj);
    IterateCodePointer(obj, &wrapper->code_, v, IndirectPointerMode::kStrong);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> obj) {
    return sizeof(CodeWrapper);
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
          obj, obj->RawExternalPointerField(
                   offset + EmbedderDataSlot::kExternalPointerOffset,
                   {kFirstEmbedderDataTag, kLastEmbedderDataTag}));
    }

#else
    // We store raw aligned pointers as Smis, so it's safe to iterate the whole
    // array.
    static_assert(kEmbedderDataSlotSize == kTaggedSize);
    IteratePointers(obj, sizeof(EmbedderDataArray), object_size, v);
#endif
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return object->SizeFromMap(map);
  }
};

class EphemeronHashTable::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    int entries_start = EphemeronHashTable::OffsetOfElementAt(
        EphemeronHashTable::kElementsStartIndex);
    IteratePointers(obj, OFFSET_OF_DATA_START(EphemeronHashTable),
                    entries_start, v);
    Tagged<EphemeronHashTable> table = UncheckedCast<EphemeronHashTable>(obj);
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
                AccessorInfo::kGetterOffset);
  static_assert(AccessorInfo::kGetterOffset < AccessorInfo::kSetterOffset);
  static_assert(AccessorInfo::kSetterOffset < AccessorInfo::kFlagsOffset);
  static_assert(AccessorInfo::kFlagsOffset < AccessorInfo::kSize);

  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IteratePointers(obj, HeapObject::kHeaderSize,
                    AccessorInfo::kEndOfStrongFieldsOffset, v);
    v->VisitExternalPointer(
        obj, obj->RawExternalPointerField(AccessorInfo::kGetterOffset,
                                          kAccessorInfoGetterTag));
    v->VisitExternalPointer(
        obj, obj->RawExternalPointerField(AccessorInfo::kSetterOffset,
                                          kAccessorInfoSetterTag));
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return kSize;
  }
};

class InterceptorInfo::BodyDescriptor final : public BodyDescriptorBase {
 public:
  static_assert(InterceptorInfo::kEndOfStrongFieldsOffset ==
                InterceptorInfo::kFlagsOffset);
  static_assert(InterceptorInfo::kFlagsOffset < InterceptorInfo::kGetterOffset);
  static_assert(InterceptorInfo::kGetterOffset <
                InterceptorInfo::kSetterOffset);
  static_assert(InterceptorInfo::kSetterOffset < InterceptorInfo::kQueryOffset);
  static_assert(InterceptorInfo::kQueryOffset <
                InterceptorInfo::kDescriptorOffset);
  static_assert(InterceptorInfo::kDescriptorOffset <
                InterceptorInfo::kDeleterOffset);
  static_assert(InterceptorInfo::kDeleterOffset <
                InterceptorInfo::kEnumeratorOffset);
  static_assert(InterceptorInfo::kEnumeratorOffset <
                InterceptorInfo::kDefinerOffset);
  static_assert(InterceptorInfo::kDefinerOffset <
                InterceptorInfo::kIndexOfOffset);
  static_assert(InterceptorInfo::kIndexOfOffsetEnd + 1 ==
                InterceptorInfo::kSize);

  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IteratePointers(obj, HeapObject::kHeaderSize,
                    InterceptorInfo::kEndOfStrongFieldsOffset, v);

    const bool is_named = Cast<InterceptorInfo>(obj)->is_named();

#define VISIT_NAMED_FIELD(Name, name)                                \
  v->VisitExternalPointer(obj, obj->RawExternalPointerField(         \
                                   InterceptorInfo::k##Name##Offset, \
                                   kApiNamedProperty##Name##CallbackTag));

#define VISIT_INDEXED_FIELD(Name, name)                              \
  v->VisitExternalPointer(obj, obj->RawExternalPointerField(         \
                                   InterceptorInfo::k##Name##Offset, \
                                   kApiIndexedProperty##Name##CallbackTag));

    if (is_named) {
      NAMED_INTERCEPTOR_INFO_CALLBACK_LIST(VISIT_NAMED_FIELD)
    } else {
      INDEXED_INTERCEPTOR_INFO_CALLBACK_LIST(VISIT_INDEXED_FIELD)
    }
#undef VISIT_NAMED_FIELD
#undef VISIT_INDEXED_FIELD
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return kSize;
  }
};

class FunctionTemplateInfo::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IteratePointers(
        obj, offsetof(TemplateInfo, template_info_flags_),
        offsetof(FunctionTemplateInfo, callback_data_) + kTaggedSize, v);
    v->VisitExternalPointer(obj, obj->RawExternalPointerField(
                                     offsetof(FunctionTemplateInfo, callback_),
                                     kFunctionTemplateInfoCallbackTag));
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return sizeof(FunctionTemplateInfo);
  }
};

// TODO(jgruber): Combine these into generic Suffix descriptors.
class FixedArray::BodyDescriptor final
    : public SuffixRangeBodyDescriptor<FixedArrayBase::kHeaderSize> {
 public:
  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> raw_object) {
    return UncheckedCast<FixedArray>(raw_object)->AllocatedSize();
  }
};

class TrustedFixedArray::BodyDescriptor final
    : public SuffixRangeBodyDescriptor<TrustedFixedArray::kHeaderSize> {
 public:
  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> raw_object) {
    return UncheckedCast<TrustedFixedArray>(raw_object)->AllocatedSize();
  }
};

class ProtectedFixedArray::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    for (int offset = OFFSET_OF_DATA_START(ProtectedFixedArray);
         offset < object_size; offset += kTaggedSize) {
      IterateProtectedPointer(obj, offset, v);
    }
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> raw_object) {
    return UncheckedCast<ProtectedFixedArray>(raw_object)->AllocatedSize();
  }
};

class SloppyArgumentsElements::BodyDescriptor final
    : public SuffixRangeBodyDescriptor<SloppyArgumentsElements::kHeaderSize> {
 public:
  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> raw_object) {
    return UncheckedCast<SloppyArgumentsElements>(raw_object)->AllocatedSize();
  }
};

class RegExpMatchInfo::BodyDescriptor final
    : public SuffixRangeBodyDescriptor<RegExpMatchInfo::kHeaderSize> {
 public:
  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> raw_object) {
    return UncheckedCast<RegExpMatchInfo>(raw_object)->AllocatedSize();
  }
};

class ArrayList::BodyDescriptor final
    : public SuffixRangeBodyDescriptor<ArrayList::kHeaderSize> {
 public:
  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> raw_object) {
    return UncheckedCast<ArrayList>(raw_object)->AllocatedSize();
  }
};

class WeakArrayList::BodyDescriptor final
    : public SuffixRangeWeakBodyDescriptor<WeakArrayList::kHeaderSize> {
 public:
  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> raw_object) {
    return UncheckedCast<WeakArrayList>(raw_object)->AllocatedSize();
  }
};

class ObjectBoilerplateDescription::BodyDescriptor final
    : public SuffixRangeBodyDescriptor<
          ObjectBoilerplateDescription::kHeaderSize> {
 public:
  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> raw_object) {
    return UncheckedCast<ObjectBoilerplateDescription>(raw_object)
        ->AllocatedSize();
  }
};

class FeedbackCell::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    IteratePointer(obj, offsetof(FeedbackCell, value_), v);

    IterateJSDispatchEntry(obj, offsetof(FeedbackCell, dispatch_handle_), v);
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> object) {
    return sizeof(FeedbackCell);
  }
};

class ClosureFeedbackCellArray::BodyDescriptor final
    : public SuffixRangeBodyDescriptor<ClosureFeedbackCellArray::kHeaderSize> {
 public:
  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> raw_object) {
    return UncheckedCast<ClosureFeedbackCellArray>(raw_object)->AllocatedSize();
  }
};

class ScriptContextTable::BodyDescriptor final
    : public SuffixRangeBodyDescriptor<ScriptContextTable::kHeaderSize> {
 public:
  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> raw_object) {
    return UncheckedCast<ScriptContextTable>(raw_object)->AllocatedSize();
  }
};

class WeakFixedArray::BodyDescriptor final
    : public SuffixRangeWeakBodyDescriptor<WeakFixedArray::kHeaderSize> {
 public:
  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> raw_object) {
    return UncheckedCast<WeakFixedArray>(raw_object)->AllocatedSize();
  }
};

class WeakHomomorphicFixedArray::BodyDescriptor final
    : public SuffixRangeWeakBodyDescriptor<
          WeakHomomorphicFixedArray::kHeaderSize> {
 public:
  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> raw_object) {
    return UncheckedCast<WeakHomomorphicFixedArray>(raw_object)
        ->AllocatedSize();
  }
};

class TrustedWeakFixedArray::BodyDescriptor final
    : public SuffixRangeWeakBodyDescriptor<TrustedWeakFixedArray::kHeaderSize> {
 public:
  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> raw_object) {
    return UncheckedCast<TrustedWeakFixedArray>(raw_object)->AllocatedSize();
  }
};

class ProtectedWeakFixedArray::BodyDescriptor final
    : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    Tagged<TrustedObject> host = TrustedCast<TrustedObject>(obj);
    for (int offset = OFFSET_OF_DATA_START(ProtectedWeakFixedArray);
         offset < object_size; offset += kTaggedSize) {
      v->VisitProtectedPointer(host,
                               host->RawProtectedMaybeObjectField(offset));
    }
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> raw_object) {
    return UncheckedCast<ProtectedWeakFixedArray>(raw_object)->AllocatedSize();
  }
};

class DoubleStringCache::BodyDescriptor final : public BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IterateBody(Tagged<Map> map, Tagged<HeapObject> obj,
                                 int object_size, ObjectVisitor* v) {
    Tagged<DoubleStringCache> host = Cast<DoubleStringCache>(obj);
    for (int offset = OFFSET_OF_DATA_START(DoubleStringCache);
         offset < object_size; offset += sizeof(DoubleStringCache::Entry)) {
      // Visit tagged value of each entry.
      ObjectSlot slot(host->address() + offset +
                      offsetof(DoubleStringCache::Entry, value_));
      v->VisitPointer(host, slot);
    }
  }

  static inline int SizeOf(Tagged<Map> map, Tagged<HeapObject> raw_object) {
    return UncheckedCast<DoubleStringCache>(raw_object)->AllocatedSize();
  }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_OBJECTS_BODY_DESCRIPTORS_INL_H_
