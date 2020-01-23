// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_OBJECTS_INL_H_
#define V8_OBJECTS_JS_OBJECTS_INL_H_

#include "src/objects/js-objects.h"

#include "src/heap/heap-write-barrier.h"
#include "src/objects/elements.h"
#include "src/objects/embedder-data-slot-inl.h"
#include "src/objects/feedback-cell-inl.h"
#include "src/objects/feedback-vector.h"
#include "src/objects/field-index-inl.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/heap-number-inl.h"
#include "src/objects/keys.h"
#include "src/objects/lookup-inl.h"
#include "src/objects/property-array-inl.h"
#include "src/objects/prototype-inl.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/slots.h"
#include "src/objects/smi-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

OBJECT_CONSTRUCTORS_IMPL(JSReceiver, HeapObject)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSObject)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSCustomElementsObject)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSSpecialObject)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSAsyncFromSyncIterator)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSFunctionOrBoundFunction)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSBoundFunction)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSDate)
OBJECT_CONSTRUCTORS_IMPL(JSFunction, JSFunctionOrBoundFunction)
OBJECT_CONSTRUCTORS_IMPL(JSGlobalObject, JSSpecialObject)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSGlobalProxy)
JSIteratorResult::JSIteratorResult(Address ptr) : JSObject(ptr) {}
OBJECT_CONSTRUCTORS_IMPL(JSMessageObject, JSObject)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSPrimitiveWrapper)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSStringIterator)

NEVER_READ_ONLY_SPACE_IMPL(JSReceiver)

CAST_ACCESSOR(JSFunction)
CAST_ACCESSOR(JSGlobalObject)
CAST_ACCESSOR(JSIteratorResult)
CAST_ACCESSOR(JSMessageObject)
CAST_ACCESSOR(JSReceiver)

MaybeHandle<Object> JSReceiver::GetProperty(Isolate* isolate,
                                            Handle<JSReceiver> receiver,
                                            Handle<Name> name) {
  LookupIterator it(isolate, receiver, name, receiver);
  if (!it.IsFound()) return it.factory()->undefined_value();
  return Object::GetProperty(&it);
}

MaybeHandle<Object> JSReceiver::GetElement(Isolate* isolate,
                                           Handle<JSReceiver> receiver,
                                           uint32_t index) {
  LookupIterator it(isolate, receiver, index, receiver);
  if (!it.IsFound()) return it.factory()->undefined_value();
  return Object::GetProperty(&it);
}

Handle<Object> JSReceiver::GetDataProperty(Handle<JSReceiver> object,
                                           Handle<Name> name) {
  LookupIterator it(object, name, object,
                    LookupIterator::PROTOTYPE_CHAIN_SKIP_INTERCEPTOR);
  if (!it.IsFound()) return it.factory()->undefined_value();
  return GetDataProperty(&it);
}

MaybeHandle<HeapObject> JSReceiver::GetPrototype(Isolate* isolate,
                                                 Handle<JSReceiver> receiver) {
  // We don't expect access checks to be needed on JSProxy objects.
  DCHECK(!receiver->IsAccessCheckNeeded() || receiver->IsJSObject());
  PrototypeIterator iter(isolate, receiver, kStartAtReceiver,
                         PrototypeIterator::END_AT_NON_HIDDEN);
  do {
    if (!iter.AdvanceFollowingProxies()) return MaybeHandle<HeapObject>();
  } while (!iter.IsAtEnd());
  return PrototypeIterator::GetCurrent(iter);
}

MaybeHandle<Object> JSReceiver::GetProperty(Isolate* isolate,
                                            Handle<JSReceiver> receiver,
                                            const char* name) {
  Handle<String> str = isolate->factory()->InternalizeUtf8String(name);
  return GetProperty(isolate, receiver, str);
}

// static
V8_WARN_UNUSED_RESULT MaybeHandle<FixedArray> JSReceiver::OwnPropertyKeys(
    Handle<JSReceiver> object) {
  return KeyAccumulator::GetKeys(object, KeyCollectionMode::kOwnOnly,
                                 ALL_PROPERTIES,
                                 GetKeysConversion::kConvertToString);
}

bool JSObject::PrototypeHasNoElements(Isolate* isolate, JSObject object) {
  DisallowHeapAllocation no_gc;
  HeapObject prototype = HeapObject::cast(object.map().prototype());
  ReadOnlyRoots roots(isolate);
  HeapObject null = roots.null_value();
  FixedArrayBase empty_fixed_array = roots.empty_fixed_array();
  FixedArrayBase empty_slow_element_dictionary =
      roots.empty_slow_element_dictionary();
  while (prototype != null) {
    Map map = prototype.map();
    if (map.IsCustomElementsReceiverMap()) return false;
    FixedArrayBase elements = JSObject::cast(prototype).elements();
    if (elements != empty_fixed_array &&
        elements != empty_slow_element_dictionary) {
      return false;
    }
    prototype = HeapObject::cast(map.prototype());
  }
  return true;
}

ACCESSORS(JSReceiver, raw_properties_or_hash, Object, kPropertiesOrHashOffset)

void JSObject::EnsureCanContainHeapObjectElements(Handle<JSObject> object) {
  JSObject::ValidateElements(*object);
  ElementsKind elements_kind = object->map().elements_kind();
  if (!IsObjectElementsKind(elements_kind)) {
    if (IsHoleyElementsKind(elements_kind)) {
      TransitionElementsKind(object, HOLEY_ELEMENTS);
    } else {
      TransitionElementsKind(object, PACKED_ELEMENTS);
    }
  }
}

template <typename TSlot>
void JSObject::EnsureCanContainElements(Handle<JSObject> object, TSlot objects,
                                        uint32_t count,
                                        EnsureElementsMode mode) {
  static_assert(std::is_same<TSlot, FullObjectSlot>::value ||
                    std::is_same<TSlot, ObjectSlot>::value,
                "Only ObjectSlot and FullObjectSlot are expected here");
  ElementsKind current_kind = object->GetElementsKind();
  ElementsKind target_kind = current_kind;
  {
    DisallowHeapAllocation no_allocation;
    DCHECK(mode != ALLOW_COPIED_DOUBLE_ELEMENTS);
    bool is_holey = IsHoleyElementsKind(current_kind);
    if (current_kind == HOLEY_ELEMENTS) return;
    Object the_hole = object->GetReadOnlyRoots().the_hole_value();
    for (uint32_t i = 0; i < count; ++i, ++objects) {
      Object current = *objects;
      if (current == the_hole) {
        is_holey = true;
        target_kind = GetHoleyElementsKind(target_kind);
      } else if (!current.IsSmi()) {
        if (mode == ALLOW_CONVERTED_DOUBLE_ELEMENTS && current.IsNumber()) {
          if (IsSmiElementsKind(target_kind)) {
            if (is_holey) {
              target_kind = HOLEY_DOUBLE_ELEMENTS;
            } else {
              target_kind = PACKED_DOUBLE_ELEMENTS;
            }
          }
        } else if (is_holey) {
          target_kind = HOLEY_ELEMENTS;
          break;
        } else {
          target_kind = PACKED_ELEMENTS;
        }
      }
    }
  }
  if (target_kind != current_kind) {
    TransitionElementsKind(object, target_kind);
  }
}

void JSObject::EnsureCanContainElements(Handle<JSObject> object,
                                        Handle<FixedArrayBase> elements,
                                        uint32_t length,
                                        EnsureElementsMode mode) {
  ReadOnlyRoots roots = object->GetReadOnlyRoots();
  if (elements->map() != roots.fixed_double_array_map()) {
    DCHECK(elements->map() == roots.fixed_array_map() ||
           elements->map() == roots.fixed_cow_array_map());
    if (mode == ALLOW_COPIED_DOUBLE_ELEMENTS) {
      mode = DONT_ALLOW_DOUBLE_ELEMENTS;
    }
    ObjectSlot objects =
        Handle<FixedArray>::cast(elements)->GetFirstElementAddress();
    EnsureCanContainElements(object, objects, length, mode);
    return;
  }

  DCHECK(mode == ALLOW_COPIED_DOUBLE_ELEMENTS);
  if (object->GetElementsKind() == HOLEY_SMI_ELEMENTS) {
    TransitionElementsKind(object, HOLEY_DOUBLE_ELEMENTS);
  } else if (object->GetElementsKind() == PACKED_SMI_ELEMENTS) {
    Handle<FixedDoubleArray> double_array =
        Handle<FixedDoubleArray>::cast(elements);
    for (uint32_t i = 0; i < length; ++i) {
      if (double_array->is_the_hole(i)) {
        TransitionElementsKind(object, HOLEY_DOUBLE_ELEMENTS);
        return;
      }
    }
    TransitionElementsKind(object, PACKED_DOUBLE_ELEMENTS);
  }
}

void JSObject::SetMapAndElements(Handle<JSObject> object, Handle<Map> new_map,
                                 Handle<FixedArrayBase> value) {
  Isolate* isolate = object->GetIsolate();
  JSObject::MigrateToMap(isolate, object, new_map);
  DCHECK((object->map().has_fast_smi_or_object_elements() ||
          (*value == ReadOnlyRoots(isolate).empty_fixed_array()) ||
          object->map().has_fast_string_wrapper_elements()) ==
         (value->map() == ReadOnlyRoots(isolate).fixed_array_map() ||
          value->map() == ReadOnlyRoots(isolate).fixed_cow_array_map()));
  DCHECK((*value == ReadOnlyRoots(isolate).empty_fixed_array()) ||
         (object->map().has_fast_double_elements() ==
          value->IsFixedDoubleArray()));
  object->set_elements(*value);
}

void JSObject::initialize_elements() {
  FixedArrayBase elements = map().GetInitialElements();
  set_elements(elements, SKIP_WRITE_BARRIER);
}

DEF_GETTER(JSObject, GetIndexedInterceptor, InterceptorInfo) {
  return map(isolate).GetIndexedInterceptor(isolate);
}

DEF_GETTER(JSObject, GetNamedInterceptor, InterceptorInfo) {
  return map(isolate).GetNamedInterceptor(isolate);
}

// static
int JSObject::GetHeaderSize(Map map) {
  // Check for the most common kind of JavaScript object before
  // falling into the generic switch. This speeds up the internal
  // field operations considerably on average.
  InstanceType instance_type = map.instance_type();
  return instance_type == JS_OBJECT_TYPE
             ? JSObject::kHeaderSize
             : GetHeaderSize(instance_type, map.has_prototype_slot());
}

// static
int JSObject::GetEmbedderFieldsStartOffset(Map map) {
  // Embedder fields are located after the object header.
  return GetHeaderSize(map);
}

int JSObject::GetEmbedderFieldsStartOffset() {
  return GetEmbedderFieldsStartOffset(map());
}

// static
int JSObject::GetEmbedderFieldCount(Map map) {
  int instance_size = map.instance_size();
  if (instance_size == kVariableSizeSentinel) return 0;
  // Embedder fields are located after the object header, whereas in-object
  // properties are located at the end of the object. We don't have to round up
  // the header size here because division by kEmbedderDataSlotSizeInTaggedSlots
  // will swallow potential padding in case of (kTaggedSize !=
  // kSystemPointerSize) anyway.
  return (((instance_size - GetEmbedderFieldsStartOffset(map)) >>
           kTaggedSizeLog2) -
          map.GetInObjectProperties()) /
         kEmbedderDataSlotSizeInTaggedSlots;
}

int JSObject::GetEmbedderFieldCount() const {
  return GetEmbedderFieldCount(map());
}

int JSObject::GetEmbedderFieldOffset(int index) {
  DCHECK_LT(static_cast<unsigned>(index),
            static_cast<unsigned>(GetEmbedderFieldCount()));
  return GetEmbedderFieldsStartOffset() + (kEmbedderDataSlotSize * index);
}

Object JSObject::GetEmbedderField(int index) {
  return EmbedderDataSlot(*this, index).load_tagged();
}

void JSObject::SetEmbedderField(int index, Object value) {
  EmbedderDataSlot::store_tagged(*this, index, value);
}

void JSObject::SetEmbedderField(int index, Smi value) {
  EmbedderDataSlot(*this, index).store_smi(value);
}

bool JSObject::IsUnboxedDoubleField(FieldIndex index) const {
  Isolate* isolate = GetIsolateForPtrCompr(*this);
  return IsUnboxedDoubleField(isolate, index);
}

bool JSObject::IsUnboxedDoubleField(Isolate* isolate, FieldIndex index) const {
  if (!FLAG_unbox_double_fields) return false;
  return map(isolate).IsUnboxedDoubleField(isolate, index);
}

// Access fast-case object properties at index. The use of these routines
// is needed to correctly distinguish between properties stored in-object and
// properties stored in the properties array.
Object JSObject::RawFastPropertyAt(FieldIndex index) const {
  Isolate* isolate = GetIsolateForPtrCompr(*this);
  return RawFastPropertyAt(isolate, index);
}

Object JSObject::RawFastPropertyAt(Isolate* isolate, FieldIndex index) const {
  DCHECK(!IsUnboxedDoubleField(isolate, index));
  if (index.is_inobject()) {
    return TaggedField<Object>::load(isolate, *this, index.offset());
  } else {
    return property_array(isolate).get(isolate, index.outobject_array_index());
  }
}

double JSObject::RawFastDoublePropertyAt(FieldIndex index) const {
  DCHECK(IsUnboxedDoubleField(index));
  return ReadField<double>(index.offset());
}

uint64_t JSObject::RawFastDoublePropertyAsBitsAt(FieldIndex index) const {
  DCHECK(IsUnboxedDoubleField(index));
  return ReadField<uint64_t>(index.offset());
}

void JSObject::RawFastInobjectPropertyAtPut(FieldIndex index, Object value,
                                            WriteBarrierMode mode) {
  DCHECK(index.is_inobject());
  int offset = index.offset();
  WRITE_FIELD(*this, offset, value);
  CONDITIONAL_WRITE_BARRIER(*this, offset, value, mode);
}

void JSObject::RawFastPropertyAtPut(FieldIndex index, Object value,
                                    WriteBarrierMode mode) {
  if (index.is_inobject()) {
    RawFastInobjectPropertyAtPut(index, value, mode);
  } else {
    DCHECK_EQ(UPDATE_WRITE_BARRIER, mode);
    property_array().set(index.outobject_array_index(), value);
  }
}

void JSObject::RawFastDoublePropertyAsBitsAtPut(FieldIndex index,
                                                uint64_t bits) {
  // Double unboxing is enabled only on 64-bit platforms without pointer
  // compression.
  DCHECK_EQ(kDoubleSize, kTaggedSize);
  Address field_addr = FIELD_ADDR(*this, index.offset());
  base::Relaxed_Store(reinterpret_cast<base::AtomicWord*>(field_addr),
                      static_cast<base::AtomicWord>(bits));
}

void JSObject::FastPropertyAtPut(FieldIndex index, Object value) {
  if (IsUnboxedDoubleField(index)) {
    DCHECK(value.IsHeapNumber());
    // Ensure that all bits of the double value are preserved.
    RawFastDoublePropertyAsBitsAtPut(index,
                                     HeapNumber::cast(value).value_as_bits());
  } else {
    RawFastPropertyAtPut(index, value);
  }
}

void JSObject::WriteToField(InternalIndex descriptor, PropertyDetails details,
                            Object value) {
  DCHECK_EQ(kField, details.location());
  DCHECK_EQ(kData, details.kind());
  DisallowHeapAllocation no_gc;
  FieldIndex index = FieldIndex::ForDescriptor(map(), descriptor);
  if (details.representation().IsDouble()) {
    // Manipulating the signaling NaN used for the hole and uninitialized
    // double field sentinel in C++, e.g. with bit_cast or value()/set_value(),
    // will change its value on ia32 (the x87 stack is used to return values
    // and stores to the stack silently clear the signalling bit).
    uint64_t bits;
    if (value.IsSmi()) {
      bits = bit_cast<uint64_t>(static_cast<double>(Smi::ToInt(value)));
    } else if (value.IsUninitialized()) {
      bits = kHoleNanInt64;
    } else {
      DCHECK(value.IsHeapNumber());
      bits = HeapNumber::cast(value).value_as_bits();
    }
    if (IsUnboxedDoubleField(index)) {
      RawFastDoublePropertyAsBitsAtPut(index, bits);
    } else {
      auto box = HeapNumber::cast(RawFastPropertyAt(index));
      box.set_value_as_bits(bits);
    }
  } else {
    RawFastPropertyAtPut(index, value);
  }
}

int JSObject::GetInObjectPropertyOffset(int index) {
  return map().GetInObjectPropertyOffset(index);
}

Object JSObject::InObjectPropertyAt(int index) {
  int offset = GetInObjectPropertyOffset(index);
  return TaggedField<Object>::load(*this, offset);
}

Object JSObject::InObjectPropertyAtPut(int index, Object value,
                                       WriteBarrierMode mode) {
  // Adjust for the number of properties stored in the object.
  int offset = GetInObjectPropertyOffset(index);
  WRITE_FIELD(*this, offset, value);
  CONDITIONAL_WRITE_BARRIER(*this, offset, value, mode);
  return value;
}

void JSObject::InitializeBody(Map map, int start_offset,
                              Object pre_allocated_value, Object filler_value) {
  DCHECK_IMPLIES(filler_value.IsHeapObject(),
                 !ObjectInYoungGeneration(filler_value));
  DCHECK_IMPLIES(pre_allocated_value.IsHeapObject(),
                 !ObjectInYoungGeneration(pre_allocated_value));
  int size = map.instance_size();
  int offset = start_offset;
  if (filler_value != pre_allocated_value) {
    int end_of_pre_allocated_offset =
        size - (map.UnusedPropertyFields() * kTaggedSize);
    DCHECK_LE(kHeaderSize, end_of_pre_allocated_offset);
    while (offset < end_of_pre_allocated_offset) {
      WRITE_FIELD(*this, offset, pre_allocated_value);
      offset += kTaggedSize;
    }
  }
  while (offset < size) {
    WRITE_FIELD(*this, offset, filler_value);
    offset += kTaggedSize;
  }
}

ACCESSORS(JSFunction, raw_feedback_cell, FeedbackCell, kFeedbackCellOffset)

ACCESSORS(JSGlobalObject, native_context, NativeContext, kNativeContextOffset)
ACCESSORS(JSGlobalObject, global_proxy, JSGlobalProxy, kGlobalProxyOffset)

FeedbackVector JSFunction::feedback_vector() const {
  DCHECK(has_feedback_vector());
  return FeedbackVector::cast(raw_feedback_cell().value());
}

ClosureFeedbackCellArray JSFunction::closure_feedback_cell_array() const {
  DCHECK(has_closure_feedback_cell_array());
  return ClosureFeedbackCellArray::cast(raw_feedback_cell().value());
}

// Code objects that are marked for deoptimization are not considered to be
// optimized. This is because the JSFunction might have been already
// deoptimized but its code() still needs to be unlinked, which will happen on
// its next activation.
// TODO(jupvfranco): rename this function. Maybe RunOptimizedCode,
// or IsValidOptimizedCode.
bool JSFunction::IsOptimized() {
  return is_compiled() && code().kind() == Code::OPTIMIZED_FUNCTION &&
         !code().marked_for_deoptimization();
}

bool JSFunction::HasOptimizedCode() {
  return IsOptimized() ||
         (has_feedback_vector() && feedback_vector().has_optimized_code() &&
          !feedback_vector().optimized_code().marked_for_deoptimization());
}

bool JSFunction::HasOptimizationMarker() {
  return has_feedback_vector() && feedback_vector().has_optimization_marker();
}

void JSFunction::ClearOptimizationMarker() {
  DCHECK(has_feedback_vector());
  feedback_vector().ClearOptimizationMarker();
}

// Optimized code marked for deoptimization will tier back down to running
// interpreted on its next activation, and already doesn't count as IsOptimized.
bool JSFunction::IsInterpreted() {
  return is_compiled() && (code().is_interpreter_trampoline_builtin() ||
                           (code().kind() == Code::OPTIMIZED_FUNCTION &&
                            code().marked_for_deoptimization()));
}

bool JSFunction::ChecksOptimizationMarker() {
  return code().checks_optimization_marker();
}

bool JSFunction::IsMarkedForOptimization() {
  return has_feedback_vector() && feedback_vector().optimization_marker() ==
                                      OptimizationMarker::kCompileOptimized;
}

bool JSFunction::IsMarkedForConcurrentOptimization() {
  return has_feedback_vector() &&
         feedback_vector().optimization_marker() ==
             OptimizationMarker::kCompileOptimizedConcurrent;
}

bool JSFunction::IsInOptimizationQueue() {
  return has_feedback_vector() && feedback_vector().optimization_marker() ==
                                      OptimizationMarker::kInOptimizationQueue;
}

void JSFunction::CompleteInobjectSlackTrackingIfActive() {
  if (!has_prototype_slot()) return;
  if (has_initial_map() && initial_map().IsInobjectSlackTrackingInProgress()) {
    initial_map().CompleteInobjectSlackTracking(GetIsolate());
  }
}

AbstractCode JSFunction::abstract_code() {
  if (IsInterpreted()) {
    return AbstractCode::cast(shared().GetBytecodeArray());
  } else {
    return AbstractCode::cast(code());
  }
}

int JSFunction::length() { return shared().length(); }

Code JSFunction::code() const {
  return Code::cast(RELAXED_READ_FIELD(*this, kCodeOffset));
}

void JSFunction::set_code(Code value) {
  DCHECK(!ObjectInYoungGeneration(value));
  RELAXED_WRITE_FIELD(*this, kCodeOffset, value);
#ifndef V8_DISABLE_WRITE_BARRIERS
  MarkingBarrier(*this, RawField(kCodeOffset), value);
#endif
}

void JSFunction::set_code_no_write_barrier(Code value) {
  DCHECK(!ObjectInYoungGeneration(value));
  RELAXED_WRITE_FIELD(*this, kCodeOffset, value);
}

// TODO(ishell): Why relaxed read but release store?
DEF_GETTER(JSFunction, shared, SharedFunctionInfo) {
  return SharedFunctionInfo::cast(
      RELAXED_READ_FIELD(*this, kSharedFunctionInfoOffset));
}

void JSFunction::set_shared(SharedFunctionInfo value, WriteBarrierMode mode) {
  // Release semantics to support acquire read in NeedsResetDueToFlushedBytecode
  RELEASE_WRITE_FIELD(*this, kSharedFunctionInfoOffset, value);
  CONDITIONAL_WRITE_BARRIER(*this, kSharedFunctionInfoOffset, value, mode);
}

void JSFunction::ClearOptimizedCodeSlot(const char* reason) {
  if (has_feedback_vector() && feedback_vector().has_optimized_code()) {
    if (FLAG_trace_opt) {
      PrintF("[evicting entry from optimizing code feedback slot (%s) for ",
             reason);
      ShortPrint();
      PrintF("]\n");
    }
    feedback_vector().ClearOptimizedCode();
  }
}

void JSFunction::SetOptimizationMarker(OptimizationMarker marker) {
  DCHECK(has_feedback_vector());
  DCHECK(ChecksOptimizationMarker());
  DCHECK(!HasOptimizedCode());

  feedback_vector().SetOptimizationMarker(marker);
}

bool JSFunction::has_feedback_vector() const {
  return shared().is_compiled() &&
         raw_feedback_cell().value().IsFeedbackVector();
}

bool JSFunction::has_closure_feedback_cell_array() const {
  return shared().is_compiled() &&
         raw_feedback_cell().value().IsClosureFeedbackCellArray();
}

Context JSFunction::context() {
  return TaggedField<Context, kContextOffset>::load(*this);
}

bool JSFunction::has_context() const {
  return TaggedField<HeapObject, kContextOffset>::load(*this).IsContext();
}

JSGlobalProxy JSFunction::global_proxy() { return context().global_proxy(); }

NativeContext JSFunction::native_context() {
  return context().native_context();
}

void JSFunction::set_context(HeapObject value) {
  DCHECK(value.IsUndefined() || value.IsContext());
  WRITE_FIELD(*this, kContextOffset, value);
  WRITE_BARRIER(*this, kContextOffset, value);
}

ACCESSORS_CHECKED(JSFunction, prototype_or_initial_map, HeapObject,
                  kPrototypeOrInitialMapOffset, map().has_prototype_slot())

DEF_GETTER(JSFunction, has_prototype_slot, bool) {
  return map(isolate).has_prototype_slot();
}

DEF_GETTER(JSFunction, initial_map, Map) {
  return Map::cast(prototype_or_initial_map(isolate));
}

DEF_GETTER(JSFunction, has_initial_map, bool) {
  DCHECK(has_prototype_slot(isolate));
  return prototype_or_initial_map(isolate).IsMap(isolate);
}

DEF_GETTER(JSFunction, has_instance_prototype, bool) {
  DCHECK(has_prototype_slot(isolate));
  // Can't use ReadOnlyRoots(isolate) as this isolate could be produced by
  // i::GetIsolateForPtrCompr(HeapObject).
  return has_initial_map(isolate) ||
         !prototype_or_initial_map(isolate).IsTheHole(
             GetReadOnlyRoots(isolate));
}

DEF_GETTER(JSFunction, has_prototype, bool) {
  DCHECK(has_prototype_slot(isolate));
  return map(isolate).has_non_instance_prototype() ||
         has_instance_prototype(isolate);
}

DEF_GETTER(JSFunction, has_prototype_property, bool) {
  return (has_prototype_slot(isolate) && IsConstructor(isolate)) ||
         IsGeneratorFunction(shared(isolate).kind());
}

DEF_GETTER(JSFunction, PrototypeRequiresRuntimeLookup, bool) {
  return !has_prototype_property(isolate) ||
         map(isolate).has_non_instance_prototype();
}

DEF_GETTER(JSFunction, instance_prototype, HeapObject) {
  DCHECK(has_instance_prototype(isolate));
  if (has_initial_map(isolate)) return initial_map(isolate).prototype(isolate);
  // When there is no initial map and the prototype is a JSReceiver, the
  // initial map field is used for the prototype field.
  return HeapObject::cast(prototype_or_initial_map(isolate));
}

DEF_GETTER(JSFunction, prototype, Object) {
  DCHECK(has_prototype(isolate));
  // If the function's prototype property has been set to a non-JSReceiver
  // value, that value is stored in the constructor field of the map.
  if (map(isolate).has_non_instance_prototype()) {
    Object prototype = map(isolate).GetConstructor(isolate);
    // The map must have a prototype in that field, not a back pointer.
    DCHECK(!prototype.IsMap(isolate));
    DCHECK(!prototype.IsFunctionTemplateInfo(isolate));
    return prototype;
  }
  return instance_prototype(isolate);
}

bool JSFunction::is_compiled() const {
  return code().builtin_index() != Builtins::kCompileLazy &&
         shared().is_compiled();
}

bool JSFunction::NeedsResetDueToFlushedBytecode() {
  // Do a raw read for shared and code fields here since this function may be
  // called on a concurrent thread and the JSFunction might not be fully
  // initialized yet.
  Object maybe_shared = ACQUIRE_READ_FIELD(*this, kSharedFunctionInfoOffset);
  Object maybe_code = RELAXED_READ_FIELD(*this, kCodeOffset);

  if (!maybe_shared.IsSharedFunctionInfo() || !maybe_code.IsCode()) {
    return false;
  }

  SharedFunctionInfo shared = SharedFunctionInfo::cast(maybe_shared);
  Code code = Code::cast(maybe_code);
  return !shared.is_compiled() &&
         code.builtin_index() != Builtins::kCompileLazy;
}

void JSFunction::ResetIfBytecodeFlushed() {
  if (FLAG_flush_bytecode && NeedsResetDueToFlushedBytecode()) {
    // Bytecode was flushed and function is now uncompiled, reset JSFunction
    // by setting code to CompileLazy and clearing the feedback vector.
    set_code(GetIsolate()->builtins()->builtin(i::Builtins::kCompileLazy));
    raw_feedback_cell().reset();
  }
}

bool JSMessageObject::DidEnsureSourcePositionsAvailable() const {
  return shared_info().IsUndefined();
}

int JSMessageObject::GetStartPosition() const {
  DCHECK(DidEnsureSourcePositionsAvailable());
  return start_position();
}

int JSMessageObject::GetEndPosition() const {
  DCHECK(DidEnsureSourcePositionsAvailable());
  return end_position();
}

MessageTemplate JSMessageObject::type() const {
  return MessageTemplateFromInt(raw_type());
}

void JSMessageObject::set_type(MessageTemplate value) {
  set_raw_type(static_cast<int>(value));
}

ACCESSORS(JSMessageObject, argument, Object, kArgumentsOffset)
ACCESSORS(JSMessageObject, script, Script, kScriptOffset)
ACCESSORS(JSMessageObject, stack_frames, Object, kStackFramesOffset)
ACCESSORS(JSMessageObject, shared_info, HeapObject, kSharedInfoOffset)
ACCESSORS(JSMessageObject, bytecode_offset, Smi, kBytecodeOffsetOffset)
SMI_ACCESSORS(JSMessageObject, start_position, kStartPositionOffset)
SMI_ACCESSORS(JSMessageObject, end_position, kEndPositionOffset)
SMI_ACCESSORS(JSMessageObject, error_level, kErrorLevelOffset)
SMI_ACCESSORS(JSMessageObject, raw_type, kMessageTypeOffset)

DEF_GETTER(JSObject, GetElementsKind, ElementsKind) {
  ElementsKind kind = map(isolate).elements_kind();
#if VERIFY_HEAP && DEBUG
  FixedArrayBase fixed_array = FixedArrayBase::unchecked_cast(
      TaggedField<HeapObject, kElementsOffset>::load(isolate, *this));

  // If a GC was caused while constructing this object, the elements
  // pointer may point to a one pointer filler map.
  if (ElementsAreSafeToExamine(isolate)) {
    Map map = fixed_array.map(isolate);
    if (IsSmiOrObjectElementsKind(kind)) {
      DCHECK(map == GetReadOnlyRoots(isolate).fixed_array_map() ||
             map == GetReadOnlyRoots(isolate).fixed_cow_array_map());
    } else if (IsDoubleElementsKind(kind)) {
      DCHECK(fixed_array.IsFixedDoubleArray(isolate) ||
             fixed_array == GetReadOnlyRoots(isolate).empty_fixed_array());
    } else if (kind == DICTIONARY_ELEMENTS) {
      DCHECK(fixed_array.IsFixedArray(isolate));
      DCHECK(fixed_array.IsNumberDictionary(isolate));
    } else {
      DCHECK(kind > DICTIONARY_ELEMENTS ||
             IsAnyNonextensibleElementsKind(kind));
    }
    DCHECK(
        !IsSloppyArgumentsElementsKind(kind) ||
        (elements(isolate).IsFixedArray() && elements(isolate).length() >= 2));
  }
#endif
  return kind;
}

DEF_GETTER(JSObject, GetElementsAccessor, ElementsAccessor*) {
  return ElementsAccessor::ForKind(GetElementsKind(isolate));
}

DEF_GETTER(JSObject, HasObjectElements, bool) {
  return IsObjectElementsKind(GetElementsKind(isolate));
}

DEF_GETTER(JSObject, HasSmiElements, bool) {
  return IsSmiElementsKind(GetElementsKind(isolate));
}

DEF_GETTER(JSObject, HasSmiOrObjectElements, bool) {
  return IsSmiOrObjectElementsKind(GetElementsKind(isolate));
}

DEF_GETTER(JSObject, HasDoubleElements, bool) {
  return IsDoubleElementsKind(GetElementsKind(isolate));
}

DEF_GETTER(JSObject, HasHoleyElements, bool) {
  return IsHoleyElementsKind(GetElementsKind(isolate));
}

DEF_GETTER(JSObject, HasFastElements, bool) {
  return IsFastElementsKind(GetElementsKind(isolate));
}

DEF_GETTER(JSObject, HasFastPackedElements, bool) {
  return IsFastPackedElementsKind(GetElementsKind(isolate));
}

DEF_GETTER(JSObject, HasDictionaryElements, bool) {
  return GetElementsKind(isolate) == DICTIONARY_ELEMENTS;
}

DEF_GETTER(JSObject, HasPackedElements, bool) {
  return GetElementsKind(isolate) == PACKED_ELEMENTS;
}

DEF_GETTER(JSObject, HasAnyNonextensibleElements, bool) {
  return IsAnyNonextensibleElementsKind(GetElementsKind(isolate));
}

DEF_GETTER(JSObject, HasSealedElements, bool) {
  return IsSealedElementsKind(GetElementsKind(isolate));
}

DEF_GETTER(JSObject, HasNonextensibleElements, bool) {
  return IsNonextensibleElementsKind(GetElementsKind(isolate));
}

DEF_GETTER(JSObject, HasFastArgumentsElements, bool) {
  return GetElementsKind(isolate) == FAST_SLOPPY_ARGUMENTS_ELEMENTS;
}

DEF_GETTER(JSObject, HasSlowArgumentsElements, bool) {
  return GetElementsKind(isolate) == SLOW_SLOPPY_ARGUMENTS_ELEMENTS;
}

DEF_GETTER(JSObject, HasSloppyArgumentsElements, bool) {
  return IsSloppyArgumentsElementsKind(GetElementsKind(isolate));
}

DEF_GETTER(JSObject, HasStringWrapperElements, bool) {
  return IsStringWrapperElementsKind(GetElementsKind(isolate));
}

DEF_GETTER(JSObject, HasFastStringWrapperElements, bool) {
  return GetElementsKind(isolate) == FAST_STRING_WRAPPER_ELEMENTS;
}

DEF_GETTER(JSObject, HasSlowStringWrapperElements, bool) {
  return GetElementsKind(isolate) == SLOW_STRING_WRAPPER_ELEMENTS;
}

DEF_GETTER(JSObject, HasTypedArrayElements, bool) {
  DCHECK(!elements(isolate).is_null());
  return map(isolate).has_typed_array_elements();
}

#define FIXED_TYPED_ELEMENTS_CHECK(Type, type, TYPE, ctype) \
  DEF_GETTER(JSObject, HasFixed##Type##Elements, bool) {    \
    return map(isolate).elements_kind() == TYPE##_ELEMENTS; \
  }

TYPED_ARRAYS(FIXED_TYPED_ELEMENTS_CHECK)

#undef FIXED_TYPED_ELEMENTS_CHECK

DEF_GETTER(JSObject, HasNamedInterceptor, bool) {
  return map(isolate).has_named_interceptor();
}

DEF_GETTER(JSObject, HasIndexedInterceptor, bool) {
  return map(isolate).has_indexed_interceptor();
}

DEF_GETTER(JSGlobalObject, global_dictionary, GlobalDictionary) {
  DCHECK(!HasFastProperties(isolate));
  DCHECK(IsJSGlobalObject(isolate));
  return GlobalDictionary::cast(raw_properties_or_hash(isolate));
}

void JSGlobalObject::set_global_dictionary(GlobalDictionary dictionary) {
  DCHECK(IsJSGlobalObject());
  set_raw_properties_or_hash(dictionary);
}

DEF_GETTER(JSObject, element_dictionary, NumberDictionary) {
  DCHECK(HasDictionaryElements(isolate) ||
         HasSlowStringWrapperElements(isolate));
  return NumberDictionary::cast(elements(isolate));
}

void JSReceiver::initialize_properties(Isolate* isolate) {
  ReadOnlyRoots roots(isolate);
  DCHECK(!ObjectInYoungGeneration(roots.empty_fixed_array()));
  DCHECK(!ObjectInYoungGeneration(roots.empty_property_dictionary()));
  if (map(isolate).is_dictionary_map()) {
    WRITE_FIELD(*this, kPropertiesOrHashOffset,
                roots.empty_property_dictionary());
  } else {
    WRITE_FIELD(*this, kPropertiesOrHashOffset, roots.empty_fixed_array());
  }
}

DEF_GETTER(JSReceiver, HasFastProperties, bool) {
  DCHECK(raw_properties_or_hash(isolate).IsSmi() ||
         ((raw_properties_or_hash(isolate).IsGlobalDictionary(isolate) ||
           raw_properties_or_hash(isolate).IsNameDictionary(isolate)) ==
          map(isolate).is_dictionary_map()));
  return !map(isolate).is_dictionary_map();
}

DEF_GETTER(JSReceiver, property_dictionary, NameDictionary) {
  DCHECK(!IsJSGlobalObject(isolate));
  DCHECK(!HasFastProperties(isolate));
  // Can't use ReadOnlyRoots(isolate) as this isolate could be produced by
  // i::GetIsolateForPtrCompr(HeapObject).
  Object prop = raw_properties_or_hash(isolate);
  if (prop.IsSmi()) {
    return GetReadOnlyRoots(isolate).empty_property_dictionary();
  }
  return NameDictionary::cast(prop);
}

// TODO(gsathya): Pass isolate directly to this function and access
// the heap from this.
DEF_GETTER(JSReceiver, property_array, PropertyArray) {
  DCHECK(HasFastProperties(isolate));
  // Can't use ReadOnlyRoots(isolate) as this isolate could be produced by
  // i::GetIsolateForPtrCompr(HeapObject).
  Object prop = raw_properties_or_hash(isolate);
  if (prop.IsSmi() || prop == GetReadOnlyRoots(isolate).empty_fixed_array()) {
    return GetReadOnlyRoots(isolate).empty_property_array();
  }
  return PropertyArray::cast(prop);
}

Maybe<bool> JSReceiver::HasProperty(Handle<JSReceiver> object,
                                    Handle<Name> name) {
  LookupIterator it = LookupIterator::PropertyOrElement(object->GetIsolate(),
                                                        object, name, object);
  return HasProperty(&it);
}

Maybe<bool> JSReceiver::HasOwnProperty(Handle<JSReceiver> object,
                                       uint32_t index) {
  if (object->IsJSModuleNamespace()) return Just(false);

  if (object->IsJSObject()) {  // Shortcut.
    LookupIterator it(object->GetIsolate(), object, index, object,
                      LookupIterator::OWN);
    return HasProperty(&it);
  }

  Maybe<PropertyAttributes> attributes =
      JSReceiver::GetOwnPropertyAttributes(object, index);
  MAYBE_RETURN(attributes, Nothing<bool>());
  return Just(attributes.FromJust() != ABSENT);
}

Maybe<PropertyAttributes> JSReceiver::GetPropertyAttributes(
    Handle<JSReceiver> object, Handle<Name> name) {
  LookupIterator it = LookupIterator::PropertyOrElement(object->GetIsolate(),
                                                        object, name, object);
  return GetPropertyAttributes(&it);
}

Maybe<PropertyAttributes> JSReceiver::GetOwnPropertyAttributes(
    Handle<JSReceiver> object, Handle<Name> name) {
  LookupIterator it = LookupIterator::PropertyOrElement(
      object->GetIsolate(), object, name, object, LookupIterator::OWN);
  return GetPropertyAttributes(&it);
}

Maybe<PropertyAttributes> JSReceiver::GetOwnPropertyAttributes(
    Handle<JSReceiver> object, uint32_t index) {
  LookupIterator it(object->GetIsolate(), object, index, object,
                    LookupIterator::OWN);
  return GetPropertyAttributes(&it);
}

Maybe<bool> JSReceiver::HasElement(Handle<JSReceiver> object, uint32_t index) {
  LookupIterator it(object->GetIsolate(), object, index, object);
  return HasProperty(&it);
}

Maybe<PropertyAttributes> JSReceiver::GetElementAttributes(
    Handle<JSReceiver> object, uint32_t index) {
  Isolate* isolate = object->GetIsolate();
  LookupIterator it(isolate, object, index, object);
  return GetPropertyAttributes(&it);
}

Maybe<PropertyAttributes> JSReceiver::GetOwnElementAttributes(
    Handle<JSReceiver> object, uint32_t index) {
  Isolate* isolate = object->GetIsolate();
  LookupIterator it(isolate, object, index, object, LookupIterator::OWN);
  return GetPropertyAttributes(&it);
}

bool JSGlobalObject::IsDetached() {
  return global_proxy().IsDetachedFrom(*this);
}

bool JSGlobalProxy::IsDetachedFrom(JSGlobalObject global) const {
  const PrototypeIterator iter(this->GetIsolate(), *this);
  return iter.GetCurrent() != global;
}

inline int JSGlobalProxy::SizeWithEmbedderFields(int embedder_field_count) {
  DCHECK_GE(embedder_field_count, 0);
  return kSize + embedder_field_count * kEmbedderDataSlotSize;
}

ACCESSORS(JSIteratorResult, value, Object, kValueOffset)
ACCESSORS(JSIteratorResult, done, Object, kDoneOffset)

TQ_SMI_ACCESSORS(JSStringIterator, index)

// If the fast-case backing storage takes up much more memory than a dictionary
// backing storage would, the object should have slow elements.
// static
static inline bool ShouldConvertToSlowElements(uint32_t used_elements,
                                               uint32_t new_capacity) {
  uint32_t size_threshold = NumberDictionary::kPreferFastElementsSizeFactor *
                            NumberDictionary::ComputeCapacity(used_elements) *
                            NumberDictionary::kEntrySize;
  return size_threshold <= new_capacity;
}

static inline bool ShouldConvertToSlowElements(JSObject object,
                                               uint32_t capacity,
                                               uint32_t index,
                                               uint32_t* new_capacity) {
  STATIC_ASSERT(JSObject::kMaxUncheckedOldFastElementsLength <=
                JSObject::kMaxUncheckedFastElementsLength);
  if (index < capacity) {
    *new_capacity = capacity;
    return false;
  }
  if (index - capacity >= JSObject::kMaxGap) return true;
  *new_capacity = JSObject::NewElementsCapacity(index + 1);
  DCHECK_LT(index, *new_capacity);
  // TODO(ulan): Check if it works with young large objects.
  if (*new_capacity <= JSObject::kMaxUncheckedOldFastElementsLength ||
      (*new_capacity <= JSObject::kMaxUncheckedFastElementsLength &&
       ObjectInYoungGeneration(object))) {
    return false;
  }
  return ShouldConvertToSlowElements(object.GetFastElementsUsage(),
                                     *new_capacity);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_OBJECTS_INL_H_
