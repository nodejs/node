// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects.h"

#include "src/assembler-inl.h"
#include "src/bootstrapper.h"
#include "src/disasm.h"
#include "src/disassembler.h"
#include "src/elements.h"
#include "src/field-type.h"
#include "src/layout-descriptor.h"
#include "src/macro-assembler.h"
#include "src/objects-inl.h"
#include "src/objects/bigint.h"
#include "src/objects/debug-objects-inl.h"
#include "src/objects/literal-objects.h"
#include "src/objects/module.h"
#include "src/ostreams.h"
#include "src/regexp/jsregexp.h"
#include "src/transitions.h"

namespace v8 {
namespace internal {

#ifdef VERIFY_HEAP

void Object::ObjectVerify() {
  if (IsSmi()) {
    Smi::cast(this)->SmiVerify();
  } else {
    HeapObject::cast(this)->HeapObjectVerify();
  }
  CHECK(!IsConstructor() || IsCallable());
}


void Object::VerifyPointer(Object* p) {
  if (p->IsHeapObject()) {
    HeapObject::VerifyHeapPointer(p);
  } else {
    CHECK(p->IsSmi());
  }
}


void Smi::SmiVerify() {
  CHECK(IsSmi());
  CHECK(!IsCallable());
  CHECK(!IsConstructor());
}


void HeapObject::HeapObjectVerify() {
  VerifyHeapPointer(map());
  CHECK(map()->IsMap());
  InstanceType instance_type = map()->instance_type();

  if (instance_type < FIRST_NONSTRING_TYPE) {
    String::cast(this)->StringVerify();
    return;
  }

  switch (instance_type) {
    case SYMBOL_TYPE:
      Symbol::cast(this)->SymbolVerify();
      break;
    case MAP_TYPE:
      Map::cast(this)->MapVerify();
      break;
    case HEAP_NUMBER_TYPE:
    case MUTABLE_HEAP_NUMBER_TYPE:
      HeapNumber::cast(this)->HeapNumberVerify();
      break;
    case BIGINT_TYPE:
      BigInt::cast(this)->BigIntVerify();
      break;
    case HASH_TABLE_TYPE:
    case FIXED_ARRAY_TYPE:
      FixedArray::cast(this)->FixedArrayVerify();
      break;
    case FIXED_DOUBLE_ARRAY_TYPE:
      FixedDoubleArray::cast(this)->FixedDoubleArrayVerify();
      break;
    case BYTE_ARRAY_TYPE:
      ByteArray::cast(this)->ByteArrayVerify();
      break;
    case BYTECODE_ARRAY_TYPE:
      BytecodeArray::cast(this)->BytecodeArrayVerify();
      break;
    case DESCRIPTOR_ARRAY_TYPE:
      DescriptorArray::cast(this)->DescriptorArrayVerify();
      break;
    case TRANSITION_ARRAY_TYPE:
      TransitionArray::cast(this)->TransitionArrayVerify();
      break;
    case PROPERTY_ARRAY_TYPE:
      PropertyArray::cast(this)->PropertyArrayVerify();
      break;
    case FREE_SPACE_TYPE:
      FreeSpace::cast(this)->FreeSpaceVerify();
      break;
    case FEEDBACK_VECTOR_TYPE:
      FeedbackVector::cast(this)->FeedbackVectorVerify();
      break;

#define VERIFY_TYPED_ARRAY(Type, type, TYPE, ctype, size)                      \
    case FIXED_##TYPE##_ARRAY_TYPE:                                            \
      Fixed##Type##Array::cast(this)->FixedTypedArrayVerify();                 \
      break;

    TYPED_ARRAYS(VERIFY_TYPED_ARRAY)
#undef VERIFY_TYPED_ARRAY

    case CODE_TYPE:
      Code::cast(this)->CodeVerify();
      break;
    case ODDBALL_TYPE:
      Oddball::cast(this)->OddballVerify();
      break;
    case JS_OBJECT_TYPE:
    case JS_ERROR_TYPE:
    case JS_API_OBJECT_TYPE:
    case JS_SPECIAL_API_OBJECT_TYPE:
    case JS_CONTEXT_EXTENSION_OBJECT_TYPE:
    case WASM_INSTANCE_TYPE:
    case WASM_MEMORY_TYPE:
    case WASM_MODULE_TYPE:
    case WASM_TABLE_TYPE:
      JSObject::cast(this)->JSObjectVerify();
      break;
    case JS_ARGUMENTS_TYPE:
      JSArgumentsObject::cast(this)->JSArgumentsObjectVerify();
      break;
    case JS_GENERATOR_OBJECT_TYPE:
      JSGeneratorObject::cast(this)->JSGeneratorObjectVerify();
      break;
    case JS_ASYNC_GENERATOR_OBJECT_TYPE:
      JSAsyncGeneratorObject::cast(this)->JSAsyncGeneratorObjectVerify();
      break;
    case JS_VALUE_TYPE:
      JSValue::cast(this)->JSValueVerify();
      break;
    case JS_DATE_TYPE:
      JSDate::cast(this)->JSDateVerify();
      break;
    case JS_BOUND_FUNCTION_TYPE:
      JSBoundFunction::cast(this)->JSBoundFunctionVerify();
      break;
    case JS_FUNCTION_TYPE:
      JSFunction::cast(this)->JSFunctionVerify();
      break;
    case JS_GLOBAL_PROXY_TYPE:
      JSGlobalProxy::cast(this)->JSGlobalProxyVerify();
      break;
    case JS_GLOBAL_OBJECT_TYPE:
      JSGlobalObject::cast(this)->JSGlobalObjectVerify();
      break;
    case CELL_TYPE:
      Cell::cast(this)->CellVerify();
      break;
    case PROPERTY_CELL_TYPE:
      PropertyCell::cast(this)->PropertyCellVerify();
      break;
    case WEAK_CELL_TYPE:
      WeakCell::cast(this)->WeakCellVerify();
      break;
    case JS_ARRAY_TYPE:
      JSArray::cast(this)->JSArrayVerify();
      break;
    case JS_MODULE_NAMESPACE_TYPE:
      JSModuleNamespace::cast(this)->JSModuleNamespaceVerify();
      break;
    case JS_SET_TYPE:
      JSSet::cast(this)->JSSetVerify();
      break;
    case JS_MAP_TYPE:
      JSMap::cast(this)->JSMapVerify();
      break;
    case JS_SET_KEY_VALUE_ITERATOR_TYPE:
    case JS_SET_VALUE_ITERATOR_TYPE:
      JSSetIterator::cast(this)->JSSetIteratorVerify();
      break;
    case JS_MAP_KEY_ITERATOR_TYPE:
    case JS_MAP_KEY_VALUE_ITERATOR_TYPE:
    case JS_MAP_VALUE_ITERATOR_TYPE:
      JSMapIterator::cast(this)->JSMapIteratorVerify();
      break;

#define ARRAY_ITERATOR_CASE(type) case type:
      ARRAY_ITERATOR_TYPE_LIST(ARRAY_ITERATOR_CASE)
#undef ARRAY_ITERATOR_CASE
      JSArrayIterator::cast(this)->JSArrayIteratorVerify();
      break;

    case JS_STRING_ITERATOR_TYPE:
      JSStringIterator::cast(this)->JSStringIteratorVerify();
      break;
    case JS_ASYNC_FROM_SYNC_ITERATOR_TYPE:
      JSAsyncFromSyncIterator::cast(this)->JSAsyncFromSyncIteratorVerify();
      break;
    case JS_WEAK_MAP_TYPE:
      JSWeakMap::cast(this)->JSWeakMapVerify();
      break;
    case JS_WEAK_SET_TYPE:
      JSWeakSet::cast(this)->JSWeakSetVerify();
      break;
    case JS_PROMISE_TYPE:
      JSPromise::cast(this)->JSPromiseVerify();
      break;
    case JS_REGEXP_TYPE:
      JSRegExp::cast(this)->JSRegExpVerify();
      break;
    case FILLER_TYPE:
      break;
    case JS_PROXY_TYPE:
      JSProxy::cast(this)->JSProxyVerify();
      break;
    case FOREIGN_TYPE:
      Foreign::cast(this)->ForeignVerify();
      break;
    case SHARED_FUNCTION_INFO_TYPE:
      SharedFunctionInfo::cast(this)->SharedFunctionInfoVerify();
      break;
    case JS_MESSAGE_OBJECT_TYPE:
      JSMessageObject::cast(this)->JSMessageObjectVerify();
      break;
    case JS_ARRAY_BUFFER_TYPE:
      JSArrayBuffer::cast(this)->JSArrayBufferVerify();
      break;
    case JS_TYPED_ARRAY_TYPE:
      JSTypedArray::cast(this)->JSTypedArrayVerify();
      break;
    case JS_DATA_VIEW_TYPE:
      JSDataView::cast(this)->JSDataViewVerify();
      break;
    case SMALL_ORDERED_HASH_SET_TYPE:
      SmallOrderedHashSet::cast(this)->SmallOrderedHashTableVerify();
      break;
    case SMALL_ORDERED_HASH_MAP_TYPE:
      SmallOrderedHashMap::cast(this)->SmallOrderedHashTableVerify();
      break;
    case CODE_DATA_CONTAINER_TYPE:
      CodeDataContainer::cast(this)->CodeDataContainerVerify();
      break;

#define MAKE_STRUCT_CASE(NAME, Name, name) \
  case NAME##_TYPE:                        \
    Name::cast(this)->Name##Verify();      \
    break;
    STRUCT_LIST(MAKE_STRUCT_CASE)
#undef MAKE_STRUCT_CASE

    default:
      UNREACHABLE();
      break;
  }
}


void HeapObject::VerifyHeapPointer(Object* p) {
  CHECK(p->IsHeapObject());
  HeapObject* ho = HeapObject::cast(p);
  CHECK(ho->GetHeap()->Contains(ho));
}


void Symbol::SymbolVerify() {
  CHECK(IsSymbol());
  CHECK(HasHashCode());
  CHECK_GT(Hash(), 0);
  CHECK(name()->IsUndefined(GetIsolate()) || name()->IsString());
}


void HeapNumber::HeapNumberVerify() {
  CHECK(IsHeapNumber() || IsMutableHeapNumber());
}

void ByteArray::ByteArrayVerify() {
  CHECK(IsByteArray());
}


void BytecodeArray::BytecodeArrayVerify() {
  // TODO(oth): Walk bytecodes and immediate values to validate sanity.
  // - All bytecodes are known and well formed.
  // - Jumps must go to new instructions starts.
  // - No Illegal bytecodes.
  // - No consecutive sequences of prefix Wide / ExtraWide.
  CHECK(IsBytecodeArray());
  CHECK(constant_pool()->IsFixedArray());
  VerifyHeapPointer(constant_pool());
}


void FreeSpace::FreeSpaceVerify() {
  CHECK(IsFreeSpace());
}

void FeedbackVector::FeedbackVectorVerify() { CHECK(IsFeedbackVector()); }

template <class Traits>
void FixedTypedArray<Traits>::FixedTypedArrayVerify() {
  CHECK(IsHeapObject() &&
        HeapObject::cast(this)->map()->instance_type() ==
            Traits::kInstanceType);
  if (base_pointer() == this) {
    CHECK(external_pointer() ==
          ExternalReference::fixed_typed_array_base_data_offset().address());
  } else {
    CHECK_NULL(base_pointer());
  }
}


bool JSObject::ElementsAreSafeToExamine() {
  // If a GC was caused while constructing this object, the elements
  // pointer may point to a one pointer filler map.
  return reinterpret_cast<Map*>(elements()) !=
      GetHeap()->one_pointer_filler_map();
}


void JSObject::JSObjectVerify() {
  VerifyPointer(raw_properties_or_hash());
  VerifyHeapPointer(elements());

  CHECK_IMPLIES(HasSloppyArgumentsElements(), IsJSArgumentsObject());
  if (HasFastProperties()) {
    int actual_unused_property_fields = map()->GetInObjectProperties() +
                                        property_array()->length() -
                                        map()->NextFreePropertyIndex();
    if (map()->UnusedPropertyFields() != actual_unused_property_fields) {
      // There are two reasons why this can happen:
      // - in the middle of StoreTransitionStub when the new extended backing
      //   store is already set into the object and the allocation of the
      //   MutableHeapNumber triggers GC while the map isn't updated yet.
      // - deletion of the last property can leave additional backing store
      //   capacity behind.
      CHECK_GT(actual_unused_property_fields, map()->UnusedPropertyFields());
      int delta = actual_unused_property_fields - map()->UnusedPropertyFields();
      CHECK_EQ(0, delta % JSObject::kFieldsAdded);
    }
    DescriptorArray* descriptors = map()->instance_descriptors();
    Isolate* isolate = GetIsolate();
    bool is_transitionable_fast_elements_kind =
        IsTransitionableFastElementsKind(map()->elements_kind());

    for (int i = 0; i < map()->NumberOfOwnDescriptors(); i++) {
      PropertyDetails details = descriptors->GetDetails(i);
      if (details.location() == kField) {
        DCHECK_EQ(kData, details.kind());
        Representation r = details.representation();
        FieldIndex index = FieldIndex::ForDescriptor(map(), i);
        if (IsUnboxedDoubleField(index)) {
          DCHECK(r.IsDouble());
          continue;
        }
        Object* value = RawFastPropertyAt(index);
        if (r.IsDouble()) DCHECK(value->IsMutableHeapNumber());
        if (value->IsUninitialized(isolate)) continue;
        if (r.IsSmi()) DCHECK(value->IsSmi());
        if (r.IsHeapObject()) DCHECK(value->IsHeapObject());
        FieldType* field_type = descriptors->GetFieldType(i);
        bool type_is_none = field_type->IsNone();
        bool type_is_any = field_type->IsAny();
        if (r.IsNone()) {
          CHECK(type_is_none);
        } else if (!type_is_any && !(type_is_none && r.IsHeapObject())) {
          CHECK(!field_type->NowStable() || field_type->NowContains(value));
        }
        CHECK_IMPLIES(is_transitionable_fast_elements_kind,
                      !Map::IsInplaceGeneralizableField(details.constness(), r,
                                                        field_type));
      }
    }

    if (map()->EnumLength() != kInvalidEnumCacheSentinel) {
      EnumCache* enum_cache = descriptors->GetEnumCache();
      FixedArray* keys = enum_cache->keys();
      FixedArray* indices = enum_cache->indices();
      CHECK_LE(map()->EnumLength(), keys->length());
      CHECK_IMPLIES(indices != isolate->heap()->empty_fixed_array(),
                    keys->length() == indices->length());
    }
  }

  // If a GC was caused while constructing this object, the elements
  // pointer may point to a one pointer filler map.
  if (ElementsAreSafeToExamine()) {
    CHECK_EQ((map()->has_fast_smi_or_object_elements() ||
              (elements() == GetHeap()->empty_fixed_array()) ||
              HasFastStringWrapperElements()),
             (elements()->map() == GetHeap()->fixed_array_map() ||
              elements()->map() == GetHeap()->fixed_cow_array_map()));
    CHECK(map()->has_fast_object_elements() == HasObjectElements());
  }
}


void Map::MapVerify() {
  Heap* heap = GetHeap();
  CHECK(!heap->InNewSpace(this));
  CHECK(FIRST_TYPE <= instance_type() && instance_type() <= LAST_TYPE);
  CHECK(instance_size() == kVariableSizeSentinel ||
        (kPointerSize <= instance_size() &&
         static_cast<size_t>(instance_size()) < heap->Capacity()));
  CHECK(GetBackPointer()->IsUndefined(heap->isolate()) ||
        !Map::cast(GetBackPointer())->is_stable());
  VerifyHeapPointer(prototype());
  VerifyHeapPointer(instance_descriptors());
  SLOW_DCHECK(instance_descriptors()->IsSortedNoDuplicates());
  DisallowHeapAllocation no_gc;
  SLOW_DCHECK(TransitionsAccessor(this, &no_gc).IsSortedNoDuplicates());
  SLOW_DCHECK(TransitionsAccessor(this, &no_gc).IsConsistentWithBackPointers());
  SLOW_DCHECK(!FLAG_unbox_double_fields ||
              layout_descriptor()->IsConsistentWithMap(this));
  if (!may_have_interesting_symbols()) {
    CHECK(!has_named_interceptor());
    CHECK(!is_dictionary_map());
    CHECK(!is_access_check_needed());
    DescriptorArray* const descriptors = instance_descriptors();
    for (int i = 0; i < NumberOfOwnDescriptors(); ++i) {
      CHECK(!descriptors->GetKey(i)->IsInterestingSymbol());
    }
  }
  CHECK_IMPLIES(has_named_interceptor(), may_have_interesting_symbols());
  CHECK_IMPLIES(is_dictionary_map(), may_have_interesting_symbols());
  CHECK_IMPLIES(is_access_check_needed(), may_have_interesting_symbols());
  CHECK_IMPLIES(IsJSObjectMap() && !CanHaveFastTransitionableElementsKind(),
                IsDictionaryElementsKind(elements_kind()) ||
                    IsTerminalElementsKind(elements_kind()));
}


void Map::DictionaryMapVerify() {
  MapVerify();
  CHECK(is_dictionary_map());
  CHECK_EQ(kInvalidEnumCacheSentinel, EnumLength());
  CHECK_EQ(GetHeap()->empty_descriptor_array(), instance_descriptors());
  CHECK_EQ(0, UnusedPropertyFields());
  CHECK_EQ(Map::GetVisitorId(this), visitor_id());
}

void AliasedArgumentsEntry::AliasedArgumentsEntryVerify() {
  VerifySmiField(kAliasedContextSlot);
}


void FixedArray::FixedArrayVerify() {
  for (int i = 0; i < length(); i++) {
    Object* e = get(i);
    VerifyPointer(e);
  }
}

void PropertyArray::PropertyArrayVerify() {
  if (length() == 0) {
    CHECK_EQ(this, this->GetHeap()->empty_property_array());
    return;
  }
  // There are no empty PropertyArrays.
  CHECK_LT(0, length());
  for (int i = 0; i < length(); i++) {
    Object* e = get(i);
    VerifyPointer(e);
  }
}

void FixedDoubleArray::FixedDoubleArrayVerify() {
  for (int i = 0; i < length(); i++) {
    if (!is_the_hole(i)) {
      uint64_t value = get_representation(i);
      uint64_t unexpected =
          bit_cast<uint64_t>(std::numeric_limits<double>::quiet_NaN()) &
          V8_UINT64_C(0x7FF8000000000000);
      // Create implementation specific sNaN by inverting relevant bit.
      unexpected ^= V8_UINT64_C(0x0008000000000000);
      CHECK((value & V8_UINT64_C(0x7FF8000000000000)) != unexpected ||
            (value & V8_UINT64_C(0x0007FFFFFFFFFFFF)) == V8_UINT64_C(0));
    }
  }
}

void DescriptorArray::DescriptorArrayVerify() {
  FixedArrayVerify();
  if (number_of_descriptors_storage() == 0) {
    Heap* heap = GetHeap();
    CHECK_EQ(heap->empty_descriptor_array(), this);
    CHECK_EQ(2, length());
    CHECK_EQ(0, number_of_descriptors());
    CHECK_EQ(heap->empty_enum_cache(), GetEnumCache());
  } else {
    CHECK_LT(2, length());
    CHECK_LE(LengthFor(number_of_descriptors()), length());
  }
}

void TransitionArray::TransitionArrayVerify() {
  FixedArrayVerify();
  CHECK_LE(LengthFor(number_of_transitions()), length());
}

void JSArgumentsObject::JSArgumentsObjectVerify() {
  if (IsSloppyArgumentsElementsKind(GetElementsKind())) {
    SloppyArgumentsElements::cast(elements())
        ->SloppyArgumentsElementsVerify(this);
  }
  Isolate* isolate = GetIsolate();
  if (isolate->IsInAnyContext(map(), Context::SLOPPY_ARGUMENTS_MAP_INDEX) ||
      isolate->IsInAnyContext(map(),
                              Context::SLOW_ALIASED_ARGUMENTS_MAP_INDEX) ||
      isolate->IsInAnyContext(map(),
                              Context::FAST_ALIASED_ARGUMENTS_MAP_INDEX)) {
    VerifyObjectField(JSSloppyArgumentsObject::kLengthOffset);
    VerifyObjectField(JSSloppyArgumentsObject::kCalleeOffset);
  } else if (isolate->IsInAnyContext(map(),
                                     Context::STRICT_ARGUMENTS_MAP_INDEX)) {
    VerifyObjectField(JSStrictArgumentsObject::kLengthOffset);
  }
  JSObjectVerify();
}

void SloppyArgumentsElements::SloppyArgumentsElementsVerify(JSObject* holder) {
  Isolate* isolate = GetIsolate();
  FixedArrayVerify();
  // Abort verification if only partially initialized (can't use arguments()
  // getter because it does FixedArray::cast()).
  if (get(kArgumentsIndex)->IsUndefined(isolate)) return;

  ElementsKind kind = holder->GetElementsKind();
  bool is_fast = kind == FAST_SLOPPY_ARGUMENTS_ELEMENTS;
  CHECK(IsFixedArray());
  CHECK_GE(length(), 2);
  CHECK_EQ(map(), isolate->heap()->sloppy_arguments_elements_map());
  Context* context_object = Context::cast(context());
  FixedArray* arg_elements = FixedArray::cast(arguments());
  if (arg_elements->length() == 0) {
    CHECK(arg_elements == isolate->heap()->empty_fixed_array());
    return;
  }
  ElementsAccessor* accessor;
  if (is_fast) {
    accessor = ElementsAccessor::ForKind(HOLEY_ELEMENTS);
  } else {
    accessor = ElementsAccessor::ForKind(DICTIONARY_ELEMENTS);
  }
  int nofMappedParameters = 0;
  int maxMappedIndex = 0;
  for (int i = 0; i < nofMappedParameters; i++) {
    // Verify that each context-mapped argument is either the hole or a valid
    // Smi within context length range.
    Object* mapped = get_mapped_entry(i);
    if (mapped->IsTheHole(isolate)) {
      // Slow sloppy arguments can be holey.
      if (!is_fast) continue;
      // Fast sloppy arguments elements are never holey. Either the element is
      // context-mapped or present in the arguments elements.
      CHECK(accessor->HasElement(holder, i, arg_elements));
      continue;
    }
    int mappedIndex = Smi::ToInt(mapped);
    nofMappedParameters++;
    CHECK_LE(maxMappedIndex, mappedIndex);
    maxMappedIndex = mappedIndex;
    Object* value = context_object->get(mappedIndex);
    CHECK(value->IsObject());
    // None of the context-mapped entries should exist in the arguments
    // elements.
    CHECK(!accessor->HasElement(holder, i, arg_elements));
  }
  CHECK_LE(nofMappedParameters, context_object->length());
  CHECK_LE(nofMappedParameters, arg_elements->length());
  CHECK_LE(maxMappedIndex, context_object->length());
  CHECK_LE(maxMappedIndex, arg_elements->length());
}

void JSGeneratorObject::JSGeneratorObjectVerify() {
  // In an expression like "new g()", there can be a point where a generator
  // object is allocated but its fields are all undefined, as it hasn't yet been
  // initialized by the generator.  Hence these weak checks.
  VerifyObjectField(kFunctionOffset);
  VerifyObjectField(kContextOffset);
  VerifyObjectField(kReceiverOffset);
  VerifyObjectField(kRegisterFileOffset);
  VerifyObjectField(kContinuationOffset);
}

void JSAsyncGeneratorObject::JSAsyncGeneratorObjectVerify() {
  // Check inherited fields
  JSGeneratorObjectVerify();
  VerifyObjectField(kQueueOffset);
  queue()->HeapObjectVerify();
}

void JSValue::JSValueVerify() {
  Object* v = value();
  if (v->IsHeapObject()) {
    VerifyHeapPointer(v);
  }
}


void JSDate::JSDateVerify() {
  if (value()->IsHeapObject()) {
    VerifyHeapPointer(value());
  }
  Isolate* isolate = GetIsolate();
  CHECK(value()->IsUndefined(isolate) || value()->IsSmi() ||
        value()->IsHeapNumber());
  CHECK(year()->IsUndefined(isolate) || year()->IsSmi() || year()->IsNaN());
  CHECK(month()->IsUndefined(isolate) || month()->IsSmi() || month()->IsNaN());
  CHECK(day()->IsUndefined(isolate) || day()->IsSmi() || day()->IsNaN());
  CHECK(weekday()->IsUndefined(isolate) || weekday()->IsSmi() ||
        weekday()->IsNaN());
  CHECK(hour()->IsUndefined(isolate) || hour()->IsSmi() || hour()->IsNaN());
  CHECK(min()->IsUndefined(isolate) || min()->IsSmi() || min()->IsNaN());
  CHECK(sec()->IsUndefined(isolate) || sec()->IsSmi() || sec()->IsNaN());
  CHECK(cache_stamp()->IsUndefined(isolate) || cache_stamp()->IsSmi() ||
        cache_stamp()->IsNaN());

  if (month()->IsSmi()) {
    int month = Smi::ToInt(this->month());
    CHECK(0 <= month && month <= 11);
  }
  if (day()->IsSmi()) {
    int day = Smi::ToInt(this->day());
    CHECK(1 <= day && day <= 31);
  }
  if (hour()->IsSmi()) {
    int hour = Smi::ToInt(this->hour());
    CHECK(0 <= hour && hour <= 23);
  }
  if (min()->IsSmi()) {
    int min = Smi::ToInt(this->min());
    CHECK(0 <= min && min <= 59);
  }
  if (sec()->IsSmi()) {
    int sec = Smi::ToInt(this->sec());
    CHECK(0 <= sec && sec <= 59);
  }
  if (weekday()->IsSmi()) {
    int weekday = Smi::ToInt(this->weekday());
    CHECK(0 <= weekday && weekday <= 6);
  }
  if (cache_stamp()->IsSmi()) {
    CHECK(Smi::ToInt(cache_stamp()) <=
          Smi::ToInt(isolate->date_cache()->stamp()));
  }
}


void JSMessageObject::JSMessageObjectVerify() {
  CHECK(IsJSMessageObject());
  VerifyObjectField(kStartPositionOffset);
  VerifyObjectField(kEndPositionOffset);
  VerifyObjectField(kArgumentsOffset);
  VerifyObjectField(kScriptOffset);
  VerifyObjectField(kStackFramesOffset);
}


void String::StringVerify() {
  CHECK(IsString());
  CHECK(length() >= 0 && length() <= Smi::kMaxValue);
  CHECK_IMPLIES(length() == 0, this == GetHeap()->empty_string());
  if (IsInternalizedString()) {
    CHECK(!GetHeap()->InNewSpace(this));
  }
  if (IsConsString()) {
    ConsString::cast(this)->ConsStringVerify();
  } else if (IsSlicedString()) {
    SlicedString::cast(this)->SlicedStringVerify();
  } else if (IsThinString()) {
    ThinString::cast(this)->ThinStringVerify();
  }
}


void ConsString::ConsStringVerify() {
  CHECK(this->first()->IsString());
  CHECK(this->second() == GetHeap()->empty_string() ||
        this->second()->IsString());
  CHECK_GE(this->length(), ConsString::kMinLength);
  CHECK(this->length() == this->first()->length() + this->second()->length());
  if (this->IsFlat()) {
    // A flat cons can only be created by String::SlowFlatten.
    // Afterwards, the first part may be externalized or internalized.
    CHECK(this->first()->IsSeqString() || this->first()->IsExternalString() ||
          this->first()->IsThinString());
  }
}

void ThinString::ThinStringVerify() {
  CHECK(this->actual()->IsInternalizedString());
  CHECK(this->actual()->IsSeqString() || this->actual()->IsExternalString());
}

void SlicedString::SlicedStringVerify() {
  CHECK(!this->parent()->IsConsString());
  CHECK(!this->parent()->IsSlicedString());
  CHECK_GE(this->length(), SlicedString::kMinLength);
}


void JSBoundFunction::JSBoundFunctionVerify() {
  CHECK(IsJSBoundFunction());
  JSObjectVerify();
  VerifyObjectField(kBoundThisOffset);
  VerifyObjectField(kBoundTargetFunctionOffset);
  VerifyObjectField(kBoundArgumentsOffset);
  CHECK(IsCallable());

  Isolate* const isolate = GetIsolate();
  if (!raw_bound_target_function()->IsUndefined(isolate)) {
    CHECK(bound_target_function()->IsCallable());
    CHECK_EQ(IsConstructor(), bound_target_function()->IsConstructor());
  }
}

void JSFunction::JSFunctionVerify() {
  CHECK(IsJSFunction());
  CHECK(code()->IsCode());
  CHECK(map()->is_callable());
  if (has_prototype_slot()) {
    VerifyObjectField(kPrototypeOrInitialMapOffset);
  }
}


void SharedFunctionInfo::SharedFunctionInfoVerify() {
  CHECK(IsSharedFunctionInfo());

  VerifyObjectField(kCodeOffset);
  VerifyObjectField(kDebugInfoOffset);
  VerifyObjectField(kFeedbackMetadataOffset);
  VerifyObjectField(kFunctionDataOffset);
  VerifyObjectField(kFunctionIdentifierOffset);
  VerifyObjectField(kInstanceClassNameOffset);
  VerifyObjectField(kNameOffset);
  VerifyObjectField(kOuterScopeInfoOffset);
  VerifyObjectField(kScopeInfoOffset);
  VerifyObjectField(kScriptOffset);

  CHECK(raw_name() == kNoSharedNameSentinel || raw_name()->IsString());

  Isolate* isolate = GetIsolate();
  CHECK(function_data()->IsUndefined(isolate) || IsApiFunction() ||
        HasBytecodeArray() || HasAsmWasmData() ||
        HasLazyDeserializationBuiltinId() || HasPreParsedScopeData());

  CHECK(function_identifier()->IsUndefined(isolate) || HasBuiltinFunctionId() ||
        HasInferredName());

  int expected_map_index = Context::FunctionMapIndex(
      language_mode(), kind(), true, has_shared_name(), needs_home_object());
  CHECK_EQ(expected_map_index, function_map_index());

  if (scope_info()->length() > 0) {
    CHECK(kind() == scope_info()->function_kind());
    CHECK_EQ(kind() == kModule, scope_info()->scope_type() == MODULE_SCOPE);
  }
}


void JSGlobalProxy::JSGlobalProxyVerify() {
  CHECK(IsJSGlobalProxy());
  JSObjectVerify();
  VerifyObjectField(JSGlobalProxy::kNativeContextOffset);
  // Make sure that this object has no properties, elements.
  CHECK_EQ(0, FixedArray::cast(elements())->length());
}


void JSGlobalObject::JSGlobalObjectVerify() {
  CHECK(IsJSGlobalObject());
  // Do not check the dummy global object for the builtins.
  if (global_dictionary()->NumberOfElements() == 0 &&
      elements()->length() == 0) {
    return;
  }
  JSObjectVerify();
}


void Oddball::OddballVerify() {
  CHECK(IsOddball());
  Heap* heap = GetHeap();
  VerifyHeapPointer(to_string());
  Object* number = to_number();
  if (number->IsHeapObject()) {
    CHECK(number == heap->nan_value() ||
          number == heap->hole_nan_value());
  } else {
    CHECK(number->IsSmi());
    int value = Smi::ToInt(number);
    // Hidden oddballs have negative smis.
    const int kLeastHiddenOddballNumber = -7;
    CHECK_LE(value, 1);
    CHECK_GE(value, kLeastHiddenOddballNumber);
  }
  if (map() == heap->undefined_map()) {
    CHECK(this == heap->undefined_value());
  } else if (map() == heap->the_hole_map()) {
    CHECK(this == heap->the_hole_value());
  } else if (map() == heap->null_map()) {
    CHECK(this == heap->null_value());
  } else if (map() == heap->boolean_map()) {
    CHECK(this == heap->true_value() ||
          this == heap->false_value());
  } else if (map() == heap->uninitialized_map()) {
    CHECK(this == heap->uninitialized_value());
  } else if (map() == heap->arguments_marker_map()) {
    CHECK(this == heap->arguments_marker());
  } else if (map() == heap->termination_exception_map()) {
    CHECK(this == heap->termination_exception());
  } else if (map() == heap->exception_map()) {
    CHECK(this == heap->exception());
  } else if (map() == heap->optimized_out_map()) {
    CHECK(this == heap->optimized_out());
  } else if (map() == heap->stale_register_map()) {
    CHECK(this == heap->stale_register());
  } else {
    UNREACHABLE();
  }
}


void Cell::CellVerify() {
  CHECK(IsCell());
  VerifyObjectField(kValueOffset);
}


void PropertyCell::PropertyCellVerify() {
  CHECK(IsPropertyCell());
  VerifyObjectField(kValueOffset);
}


void WeakCell::WeakCellVerify() {
  CHECK(IsWeakCell());
  VerifyObjectField(kValueOffset);
}

void CodeDataContainer::CodeDataContainerVerify() {
  CHECK(IsCodeDataContainer());
  VerifyObjectField(kNextCodeLinkOffset);
  CHECK(next_code_link()->IsCode() ||
        next_code_link()->IsUndefined(GetIsolate()));
}

void Code::CodeVerify() {
  CHECK(IsAligned(reinterpret_cast<intptr_t>(instruction_start()),
                  kCodeAlignment));
  relocation_info()->ObjectVerify();
  Address last_gc_pc = nullptr;
  Isolate* isolate = GetIsolate();
  for (RelocIterator it(this); !it.done(); it.next()) {
    it.rinfo()->Verify(isolate);
    // Ensure that GC will not iterate twice over the same pointer.
    if (RelocInfo::IsGCRelocMode(it.rinfo()->rmode())) {
      CHECK(it.rinfo()->pc() != last_gc_pc);
      last_gc_pc = it.rinfo()->pc();
    }
  }
}


void Code::VerifyEmbeddedObjectsDependency() {
  if (!CanContainWeakObjects()) return;
  WeakCell* cell = CachedWeakCell();
  DisallowHeapAllocation no_gc;
  Isolate* isolate = GetIsolate();
  HandleScope scope(isolate);
  int mode_mask = RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT);
  for (RelocIterator it(this, mode_mask); !it.done(); it.next()) {
    Object* obj = it.rinfo()->target_object();
    if (IsWeakObject(obj)) {
      if (obj->IsMap()) {
        Map* map = Map::cast(obj);
        CHECK(map->dependent_code()->Contains(DependentCode::kWeakCodeGroup,
                                              cell));
      } else if (obj->IsJSObject()) {
        if (isolate->heap()->InNewSpace(obj)) {
          ArrayList* list =
              GetIsolate()->heap()->weak_new_space_object_to_code_list();
          bool found = false;
          for (int i = 0; i < list->Length(); i += 2) {
            WeakCell* obj_cell = WeakCell::cast(list->Get(i));
            if (!obj_cell->cleared() && obj_cell->value() == obj &&
                WeakCell::cast(list->Get(i + 1)) == cell) {
              found = true;
              break;
            }
          }
          CHECK(found);
        } else {
          Handle<HeapObject> key_obj(HeapObject::cast(obj), isolate);
          DependentCode* dep =
              GetIsolate()->heap()->LookupWeakObjectToCodeDependency(key_obj);
          dep->Contains(DependentCode::kWeakCodeGroup, cell);
        }
      }
    }
  }
}


void JSArray::JSArrayVerify() {
  JSObjectVerify();
  Isolate* isolate = GetIsolate();
  CHECK(length()->IsNumber() || length()->IsUndefined(isolate));
  // If a GC was caused while constructing this array, the elements
  // pointer may point to a one pointer filler map.
  if (!ElementsAreSafeToExamine()) return;
  if (elements()->IsUndefined(isolate)) return;
  CHECK(elements()->IsFixedArray() || elements()->IsFixedDoubleArray());
  if (!length()->IsNumber()) return;
  // Verify that the length and the elements backing store are in sync.
  if (length()->IsSmi() && HasFastElements()) {
    int size = Smi::ToInt(length());
    // Holey / Packed backing stores might have slack or might have not been
    // properly initialized yet.
    CHECK(size <= elements()->length() ||
          elements() == isolate->heap()->empty_fixed_array());
  } else {
    CHECK(HasDictionaryElements());
    uint32_t array_length;
    CHECK(length()->ToArrayLength(&array_length));
    if (array_length == 0xffffffff) {
      CHECK(length()->ToArrayLength(&array_length));
    }
    if (array_length != 0) {
      NumberDictionary* dict = NumberDictionary::cast(elements());
      // The dictionary can never have more elements than the array length + 1.
      // If the backing store grows the verification might be triggered with
      // the old length in place.
      uint32_t nof_elements = static_cast<uint32_t>(dict->NumberOfElements());
      if (nof_elements != 0) nof_elements--;
      CHECK_LE(nof_elements, array_length);
    }
  }
}


void JSSet::JSSetVerify() {
  CHECK(IsJSSet());
  JSObjectVerify();
  VerifyHeapPointer(table());
  CHECK(table()->IsOrderedHashSet() || table()->IsUndefined(GetIsolate()));
  // TODO(arv): Verify OrderedHashTable too.
}


void JSMap::JSMapVerify() {
  CHECK(IsJSMap());
  JSObjectVerify();
  VerifyHeapPointer(table());
  CHECK(table()->IsOrderedHashMap() || table()->IsUndefined(GetIsolate()));
  // TODO(arv): Verify OrderedHashTable too.
}


void JSSetIterator::JSSetIteratorVerify() {
  CHECK(IsJSSetIterator());
  JSObjectVerify();
  VerifyHeapPointer(table());
  CHECK(table()->IsOrderedHashSet());
  CHECK(index()->IsSmi());
}


void JSMapIterator::JSMapIteratorVerify() {
  CHECK(IsJSMapIterator());
  JSObjectVerify();
  VerifyHeapPointer(table());
  CHECK(table()->IsOrderedHashMap());
  CHECK(index()->IsSmi());
}


void JSWeakMap::JSWeakMapVerify() {
  CHECK(IsJSWeakMap());
  JSObjectVerify();
  VerifyHeapPointer(table());
  CHECK(table()->IsHashTable() || table()->IsUndefined(GetIsolate()));
}

void JSArrayIterator::JSArrayIteratorVerify() {
  CHECK(IsJSArrayIterator());
  JSObjectVerify();
  CHECK(object()->IsJSReceiver() || object()->IsUndefined(GetIsolate()));

  CHECK_GE(index()->Number(), 0);
  CHECK_LE(index()->Number(), kMaxSafeInteger);
  CHECK(object_map()->IsMap() || object_map()->IsUndefined(GetIsolate()));
}

void JSStringIterator::JSStringIteratorVerify() {
  CHECK(IsJSStringIterator());
  JSObjectVerify();
  CHECK(string()->IsString());

  CHECK_GE(index(), 0);
  CHECK_LE(index(), String::kMaxLength);
}

void JSAsyncFromSyncIterator::JSAsyncFromSyncIteratorVerify() {
  CHECK(IsJSAsyncFromSyncIterator());
  JSObjectVerify();
  VerifyHeapPointer(sync_iterator());
}

void JSWeakSet::JSWeakSetVerify() {
  CHECK(IsJSWeakSet());
  JSObjectVerify();
  VerifyHeapPointer(table());
  CHECK(table()->IsHashTable() || table()->IsUndefined(GetIsolate()));
}

void PromiseCapability::PromiseCapabilityVerify() {
  CHECK(IsPromiseCapability());
  VerifyPointer(promise());
  VerifyPointer(resolve());
  VerifyPointer(reject());
}

void JSPromise::JSPromiseVerify() {
  CHECK(IsJSPromise());
  JSObjectVerify();
  Isolate* isolate = GetIsolate();
  CHECK(result()->IsUndefined(isolate) || result()->IsObject());
  CHECK(deferred_promise()->IsUndefined(isolate) ||
        deferred_promise()->IsJSReceiver() ||
        deferred_promise()->IsFixedArray());
  CHECK(deferred_on_resolve()->IsUndefined(isolate) ||
        deferred_on_resolve()->IsCallable() ||
        deferred_on_resolve()->IsFixedArray());
  CHECK(deferred_on_reject()->IsUndefined(isolate) ||
        deferred_on_reject()->IsCallable() ||
        deferred_on_reject()->IsFixedArray());
  CHECK(fulfill_reactions()->IsUndefined(isolate) ||
        fulfill_reactions()->IsCallable() || fulfill_reactions()->IsSymbol() ||
        fulfill_reactions()->IsFixedArray());
  CHECK(reject_reactions()->IsUndefined(isolate) ||
        reject_reactions()->IsSymbol() || reject_reactions()->IsCallable() ||
        reject_reactions()->IsFixedArray());
}

template <typename Derived>
void SmallOrderedHashTable<Derived>::SmallOrderedHashTableVerify() {
  CHECK(IsSmallOrderedHashTable());
  Isolate* isolate = GetIsolate();

  for (int entry = 0; entry < NumberOfBuckets(); entry++) {
    int bucket = GetFirstEntry(entry);
    if (bucket == kNotFound) continue;

    for (int offset = 0; offset < Derived::kEntrySize; offset++) {
      Object* val = GetDataEntry(bucket, offset);
      CHECK(!val->IsTheHole(isolate));
    }
  }

  for (int entry = 0; entry < NumberOfElements(); entry++) {
    int chain = GetNextEntry(entry);
    if (chain == kNotFound) continue;

    for (int offset = 0; offset < Derived::kEntrySize; offset++) {
      Object* val = GetDataEntry(chain, offset);
      CHECK(!val->IsTheHole(isolate));
    }
  }

  for (int entry = 0; entry < NumberOfElements(); entry++) {
    for (int offset = 0; offset < Derived::kEntrySize; offset++) {
      Object* val = GetDataEntry(entry, offset);
      VerifyPointer(val);
    }
  }

  for (int entry = NumberOfElements(); entry < Capacity(); entry++) {
    for (int offset = 0; offset < Derived::kEntrySize; offset++) {
      Object* val = GetDataEntry(entry, offset);
      CHECK(val->IsTheHole(isolate));
    }
  }
}

template void
SmallOrderedHashTable<SmallOrderedHashMap>::SmallOrderedHashTableVerify();
template void
SmallOrderedHashTable<SmallOrderedHashSet>::SmallOrderedHashTableVerify();

void JSRegExp::JSRegExpVerify() {
  JSObjectVerify();
  Isolate* isolate = GetIsolate();
  CHECK(data()->IsUndefined(isolate) || data()->IsFixedArray());
  switch (TypeTag()) {
    case JSRegExp::ATOM: {
      FixedArray* arr = FixedArray::cast(data());
      CHECK(arr->get(JSRegExp::kAtomPatternIndex)->IsString());
      break;
    }
    case JSRegExp::IRREGEXP: {
      bool is_native = RegExpImpl::UsesNativeRegExp();

      FixedArray* arr = FixedArray::cast(data());
      Object* one_byte_data = arr->get(JSRegExp::kIrregexpLatin1CodeIndex);
      // Smi : Not compiled yet (-1).
      // Code/ByteArray: Compiled code.
      CHECK(
          (one_byte_data->IsSmi() &&
           Smi::ToInt(one_byte_data) == JSRegExp::kUninitializedValue) ||
          (is_native ? one_byte_data->IsCode() : one_byte_data->IsByteArray()));
      Object* uc16_data = arr->get(JSRegExp::kIrregexpUC16CodeIndex);
      CHECK((uc16_data->IsSmi() &&
             Smi::ToInt(uc16_data) == JSRegExp::kUninitializedValue) ||
            (is_native ? uc16_data->IsCode() : uc16_data->IsByteArray()));

      CHECK(arr->get(JSRegExp::kIrregexpCaptureCountIndex)->IsSmi());
      CHECK(arr->get(JSRegExp::kIrregexpMaxRegisterCountIndex)->IsSmi());
      break;
    }
    default:
      CHECK_EQ(JSRegExp::NOT_COMPILED, TypeTag());
      CHECK(data()->IsUndefined(isolate));
      break;
  }
}

void JSProxy::JSProxyVerify() {
  CHECK(IsJSProxy());
  VerifyPointer(target());
  VerifyPointer(handler());
  Isolate* isolate = GetIsolate();
  CHECK_EQ(target()->IsCallable(), map()->is_callable());
  CHECK_EQ(target()->IsConstructor(), map()->is_constructor());
  CHECK(map()->prototype()->IsNull(isolate));
  // There should be no properties on a Proxy.
  CHECK_EQ(0, map()->NumberOfOwnDescriptors());
}


void JSArrayBuffer::JSArrayBufferVerify() {
  CHECK(IsJSArrayBuffer());
  JSObjectVerify();
  VerifyPointer(byte_length());
  CHECK(byte_length()->IsSmi() || byte_length()->IsHeapNumber() ||
        byte_length()->IsUndefined(GetIsolate()));
}


void JSArrayBufferView::JSArrayBufferViewVerify() {
  CHECK(IsJSArrayBufferView());
  JSObjectVerify();
  VerifyPointer(buffer());
  Isolate* isolate = GetIsolate();
  CHECK(buffer()->IsJSArrayBuffer() || buffer()->IsUndefined(isolate) ||
        buffer() == Smi::kZero);

  VerifyPointer(raw_byte_offset());
  CHECK(raw_byte_offset()->IsSmi() || raw_byte_offset()->IsHeapNumber() ||
        raw_byte_offset()->IsUndefined(isolate));

  VerifyPointer(raw_byte_length());
  CHECK(raw_byte_length()->IsSmi() || raw_byte_length()->IsHeapNumber() ||
        raw_byte_length()->IsUndefined(isolate));
}


void JSTypedArray::JSTypedArrayVerify() {
  CHECK(IsJSTypedArray());
  JSArrayBufferViewVerify();
  VerifyPointer(raw_length());
  CHECK(raw_length()->IsSmi() || raw_length()->IsUndefined(GetIsolate()));
  VerifyPointer(elements());
}


void JSDataView::JSDataViewVerify() {
  CHECK(IsJSDataView());
  JSArrayBufferViewVerify();
}


void Foreign::ForeignVerify() {
  CHECK(IsForeign());
}


void PromiseResolveThenableJobInfo::PromiseResolveThenableJobInfoVerify() {
  CHECK(IsPromiseResolveThenableJobInfo());
  CHECK(thenable()->IsJSReceiver());
  CHECK(then()->IsJSReceiver());
  CHECK(resolve()->IsJSFunction());
  CHECK(reject()->IsJSFunction());
  CHECK(context()->IsContext());
}

void PromiseReactionJobInfo::PromiseReactionJobInfoVerify() {
  Isolate* isolate = GetIsolate();
  CHECK(IsPromiseReactionJobInfo());
  CHECK(value()->IsObject());
  CHECK(tasks()->IsFixedArray() || tasks()->IsCallable() ||
        tasks()->IsSymbol());
  CHECK(deferred_promise()->IsUndefined(isolate) ||
        deferred_promise()->IsJSReceiver() ||
        deferred_promise()->IsFixedArray());
  CHECK(deferred_on_resolve()->IsUndefined(isolate) ||
        deferred_on_resolve()->IsCallable() ||
        deferred_on_resolve()->IsFixedArray());
  CHECK(deferred_on_reject()->IsUndefined(isolate) ||
        deferred_on_reject()->IsCallable() ||
        deferred_on_reject()->IsFixedArray());
  CHECK(context()->IsContext());
}

void AsyncGeneratorRequest::AsyncGeneratorRequestVerify() {
  CHECK(IsAsyncGeneratorRequest());
  VerifySmiField(kResumeModeOffset);
  CHECK_GE(resume_mode(), JSGeneratorObject::kNext);
  CHECK_LE(resume_mode(), JSGeneratorObject::kThrow);
  CHECK(promise()->IsJSPromise());
  VerifyPointer(value());
  VerifyPointer(next());
  next()->ObjectVerify();
}

void BigInt::BigIntVerify() {
  CHECK(IsBigInt());
  CHECK_GE(length(), 0);
  CHECK_IMPLIES(is_zero(), !sign());  // There is no -0n.
  // TODO(neis): Somewhere check that MSD is non-zero. Doesn't hold during some
  // operations that allocate which is why we can't test it here.
}

void JSModuleNamespace::JSModuleNamespaceVerify() {
  CHECK(IsJSModuleNamespace());
  VerifyPointer(module());
}

void ModuleInfoEntry::ModuleInfoEntryVerify() {
  Isolate* isolate = GetIsolate();
  CHECK(IsModuleInfoEntry());

  CHECK(export_name()->IsUndefined(isolate) || export_name()->IsString());
  CHECK(local_name()->IsUndefined(isolate) || local_name()->IsString());
  CHECK(import_name()->IsUndefined(isolate) || import_name()->IsString());

  VerifySmiField(kModuleRequestOffset);
  VerifySmiField(kCellIndexOffset);
  VerifySmiField(kBegPosOffset);
  VerifySmiField(kEndPosOffset);

  CHECK_IMPLIES(import_name()->IsString(), module_request() >= 0);
  CHECK_IMPLIES(export_name()->IsString() && import_name()->IsString(),
                local_name()->IsUndefined(isolate));
}

void Module::ModuleVerify() {
  CHECK(IsModule());

  VerifyPointer(code());
  VerifyPointer(exports());
  VerifyPointer(module_namespace());
  VerifyPointer(requested_modules());
  VerifyPointer(script());
  VerifyPointer(import_meta());
  VerifyPointer(exception());
  VerifySmiField(kHashOffset);
  VerifySmiField(kStatusOffset);

  CHECK((status() >= kEvaluating && code()->IsModuleInfo()) ||
        (status() == kInstantiated && code()->IsJSGeneratorObject()) ||
        (status() >= kInstantiating && code()->IsJSFunction()) ||
        (code()->IsSharedFunctionInfo()));

  CHECK_EQ(status() == kErrored, !exception()->IsTheHole(GetIsolate()));

  CHECK(module_namespace()->IsUndefined(GetIsolate()) ||
        module_namespace()->IsJSModuleNamespace());
  if (module_namespace()->IsJSModuleNamespace()) {
    CHECK_LE(kInstantiating, status());
    CHECK_EQ(JSModuleNamespace::cast(module_namespace())->module(), this);
  }

  CHECK_EQ(requested_modules()->length(), info()->module_requests()->length());

  CHECK(import_meta()->IsTheHole(GetIsolate()) || import_meta()->IsJSObject());

  CHECK_NE(hash(), 0);
}

void PrototypeInfo::PrototypeInfoVerify() {
  CHECK(IsPrototypeInfo());
  CHECK(weak_cell()->IsWeakCell() || weak_cell()->IsUndefined(GetIsolate()));
  if (prototype_users()->IsWeakFixedArray()) {
    WeakFixedArray::cast(prototype_users())->FixedArrayVerify();
  } else {
    CHECK(prototype_users()->IsSmi());
  }
  CHECK(validity_cell()->IsCell() || validity_cell()->IsSmi());
}

void Tuple2::Tuple2Verify() {
  CHECK(IsTuple2());
  Heap* heap = GetHeap();
  if (this == heap->empty_enum_cache()) {
    CHECK_EQ(heap->empty_fixed_array(), EnumCache::cast(this)->keys());
    CHECK_EQ(heap->empty_fixed_array(), EnumCache::cast(this)->indices());
  } else {
    VerifyObjectField(kValue1Offset);
    VerifyObjectField(kValue2Offset);
  }
}

void Tuple3::Tuple3Verify() {
  CHECK(IsTuple3());
  VerifyObjectField(kValue1Offset);
  VerifyObjectField(kValue2Offset);
  VerifyObjectField(kValue3Offset);
}

void ContextExtension::ContextExtensionVerify() {
  CHECK(IsContextExtension());
  VerifyObjectField(kScopeInfoOffset);
  VerifyObjectField(kExtensionOffset);
}

void AccessorInfo::AccessorInfoVerify() {
  CHECK(IsAccessorInfo());
  VerifyPointer(name());
  VerifyPointer(expected_receiver_type());
  VerifyPointer(getter());
  VerifyPointer(setter());
  VerifyPointer(js_getter());
  VerifyPointer(data());
}


void AccessorPair::AccessorPairVerify() {
  CHECK(IsAccessorPair());
  VerifyPointer(getter());
  VerifyPointer(setter());
}


void AccessCheckInfo::AccessCheckInfoVerify() {
  CHECK(IsAccessCheckInfo());
  VerifyPointer(callback());
  VerifyPointer(named_interceptor());
  VerifyPointer(indexed_interceptor());
  VerifyPointer(data());
}


void InterceptorInfo::InterceptorInfoVerify() {
  CHECK(IsInterceptorInfo());
  VerifyPointer(getter());
  VerifyPointer(setter());
  VerifyPointer(query());
  VerifyPointer(deleter());
  VerifyPointer(enumerator());
  VerifyPointer(data());
  VerifySmiField(kFlagsOffset);
}


void TemplateInfo::TemplateInfoVerify() {
  VerifyPointer(tag());
  VerifyPointer(property_list());
  VerifyPointer(property_accessors());
}


void FunctionTemplateInfo::FunctionTemplateInfoVerify() {
  CHECK(IsFunctionTemplateInfo());
  TemplateInfoVerify();
  VerifyPointer(serial_number());
  VerifyPointer(call_code());
  VerifyPointer(prototype_template());
  VerifyPointer(parent_template());
  VerifyPointer(named_property_handler());
  VerifyPointer(indexed_property_handler());
  VerifyPointer(instance_template());
  VerifyPointer(signature());
  VerifyPointer(access_check_info());
  VerifyPointer(cached_property_name());
}


void ObjectTemplateInfo::ObjectTemplateInfoVerify() {
  CHECK(IsObjectTemplateInfo());
  TemplateInfoVerify();
  VerifyPointer(constructor());
  VerifyPointer(data());
}


void AllocationSite::AllocationSiteVerify() {
  CHECK(IsAllocationSite());
}


void AllocationMemento::AllocationMementoVerify() {
  CHECK(IsAllocationMemento());
  VerifyHeapPointer(allocation_site());
  CHECK(!IsValid() || GetAllocationSite()->IsAllocationSite());
}


void Script::ScriptVerify() {
  CHECK(IsScript());
  VerifyPointer(source());
  VerifyPointer(name());
  VerifyPointer(wrapper());
  VerifyPointer(line_ends());
}


void NormalizedMapCache::NormalizedMapCacheVerify() {
  FixedArray::cast(this)->FixedArrayVerify();
  if (FLAG_enable_slow_asserts) {
    Isolate* isolate = GetIsolate();
    for (int i = 0; i < length(); i++) {
      Object* e = FixedArray::get(i);
      if (e->IsWeakCell()) {
        if (!WeakCell::cast(e)->cleared()) {
          Map::cast(WeakCell::cast(e)->value())->DictionaryMapVerify();
        }
      } else {
        CHECK(e->IsUndefined(isolate));
      }
    }
  }
}


void DebugInfo::DebugInfoVerify() {
  CHECK(IsDebugInfo());
  VerifyPointer(shared());
  VerifyPointer(debug_bytecode_array());
  VerifyPointer(break_points());
}


void StackFrameInfo::StackFrameInfoVerify() {
  CHECK(IsStackFrameInfo());
  VerifyPointer(script_name());
  VerifyPointer(script_name_or_source_url());
  VerifyPointer(function_name());
}

void PreParsedScopeData::PreParsedScopeDataVerify() {
  CHECK(IsPreParsedScopeData());
  CHECK(scope_data()->IsByteArray());
  CHECK(child_data()->IsFixedArray());
}

#endif  // VERIFY_HEAP

#ifdef DEBUG

void JSObject::IncrementSpillStatistics(SpillInformation* info) {
  info->number_of_objects_++;
  // Named properties
  if (HasFastProperties()) {
    info->number_of_objects_with_fast_properties_++;
    info->number_of_fast_used_fields_   += map()->NextFreePropertyIndex();
    info->number_of_fast_unused_fields_ += map()->UnusedPropertyFields();
  } else if (IsJSGlobalObject()) {
    GlobalDictionary* dict = JSGlobalObject::cast(this)->global_dictionary();
    info->number_of_slow_used_properties_ += dict->NumberOfElements();
    info->number_of_slow_unused_properties_ +=
        dict->Capacity() - dict->NumberOfElements();
  } else {
    NameDictionary* dict = property_dictionary();
    info->number_of_slow_used_properties_ += dict->NumberOfElements();
    info->number_of_slow_unused_properties_ +=
        dict->Capacity() - dict->NumberOfElements();
  }
  // Indexed properties
  switch (GetElementsKind()) {
    case HOLEY_SMI_ELEMENTS:
    case PACKED_SMI_ELEMENTS:
    case HOLEY_DOUBLE_ELEMENTS:
    case PACKED_DOUBLE_ELEMENTS:
    case HOLEY_ELEMENTS:
    case PACKED_ELEMENTS:
    case FAST_STRING_WRAPPER_ELEMENTS: {
      info->number_of_objects_with_fast_elements_++;
      int holes = 0;
      FixedArray* e = FixedArray::cast(elements());
      int len = e->length();
      Isolate* isolate = GetIsolate();
      for (int i = 0; i < len; i++) {
        if (e->get(i)->IsTheHole(isolate)) holes++;
      }
      info->number_of_fast_used_elements_   += len - holes;
      info->number_of_fast_unused_elements_ += holes;
      break;
    }

#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size)                       \
    case TYPE##_ELEMENTS:

    TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
    { info->number_of_objects_with_fast_elements_++;
      FixedArrayBase* e = FixedArrayBase::cast(elements());
      info->number_of_fast_used_elements_ += e->length();
      break;
    }
    case DICTIONARY_ELEMENTS:
    case SLOW_STRING_WRAPPER_ELEMENTS: {
      NumberDictionary* dict = element_dictionary();
      info->number_of_slow_used_elements_ += dict->NumberOfElements();
      info->number_of_slow_unused_elements_ +=
          dict->Capacity() - dict->NumberOfElements();
      break;
    }
    case FAST_SLOPPY_ARGUMENTS_ELEMENTS:
    case SLOW_SLOPPY_ARGUMENTS_ELEMENTS:
    case NO_ELEMENTS:
      break;
  }
}


void JSObject::SpillInformation::Clear() {
  number_of_objects_ = 0;
  number_of_objects_with_fast_properties_ = 0;
  number_of_objects_with_fast_elements_ = 0;
  number_of_fast_used_fields_ = 0;
  number_of_fast_unused_fields_ = 0;
  number_of_slow_used_properties_ = 0;
  number_of_slow_unused_properties_ = 0;
  number_of_fast_used_elements_ = 0;
  number_of_fast_unused_elements_ = 0;
  number_of_slow_used_elements_ = 0;
  number_of_slow_unused_elements_ = 0;
}


void JSObject::SpillInformation::Print() {
  PrintF("\n  JSObject Spill Statistics (#%d):\n", number_of_objects_);

  PrintF("    - fast properties (#%d): %d (used) %d (unused)\n",
         number_of_objects_with_fast_properties_,
         number_of_fast_used_fields_, number_of_fast_unused_fields_);

  PrintF("    - slow properties (#%d): %d (used) %d (unused)\n",
         number_of_objects_ - number_of_objects_with_fast_properties_,
         number_of_slow_used_properties_, number_of_slow_unused_properties_);

  PrintF("    - fast elements (#%d): %d (used) %d (unused)\n",
         number_of_objects_with_fast_elements_,
         number_of_fast_used_elements_, number_of_fast_unused_elements_);

  PrintF("    - slow elements (#%d): %d (used) %d (unused)\n",
         number_of_objects_ - number_of_objects_with_fast_elements_,
         number_of_slow_used_elements_, number_of_slow_unused_elements_);

  PrintF("\n");
}


bool DescriptorArray::IsSortedNoDuplicates(int valid_entries) {
  if (valid_entries == -1) valid_entries = number_of_descriptors();
  Name* current_key = nullptr;
  uint32_t current = 0;
  for (int i = 0; i < number_of_descriptors(); i++) {
    Name* key = GetSortedKey(i);
    if (key == current_key) {
      Print();
      return false;
    }
    current_key = key;
    uint32_t hash = GetSortedKey(i)->Hash();
    if (hash < current) {
      Print();
      return false;
    }
    current = hash;
  }
  return true;
}


bool TransitionArray::IsSortedNoDuplicates(int valid_entries) {
  DCHECK_EQ(valid_entries, -1);
  Name* prev_key = nullptr;
  PropertyKind prev_kind = kData;
  PropertyAttributes prev_attributes = NONE;
  uint32_t prev_hash = 0;
  for (int i = 0; i < number_of_transitions(); i++) {
    Name* key = GetSortedKey(i);
    uint32_t hash = key->Hash();
    PropertyKind kind = kData;
    PropertyAttributes attributes = NONE;
    if (!TransitionsAccessor::IsSpecialTransition(key)) {
      Map* target = GetTarget(i);
      PropertyDetails details =
          TransitionsAccessor::GetTargetDetails(key, target);
      kind = details.kind();
      attributes = details.attributes();
    } else {
      // Duplicate entries are not allowed for non-property transitions.
      DCHECK_NE(prev_key, key);
    }

    int cmp = CompareKeys(prev_key, prev_hash, prev_kind, prev_attributes, key,
                          hash, kind, attributes);
    if (cmp >= 0) {
      Print();
      return false;
    }
    prev_key = key;
    prev_hash = hash;
    prev_attributes = attributes;
    prev_kind = kind;
  }
  return true;
}

bool TransitionsAccessor::IsSortedNoDuplicates() {
  // Simple and non-existent transitions are always sorted.
  if (encoding() != kFullTransitionArray) return true;
  return transitions()->IsSortedNoDuplicates();
}


static bool CheckOneBackPointer(Map* current_map, Object* target) {
  return !target->IsMap() || Map::cast(target)->GetBackPointer() == current_map;
}

bool TransitionsAccessor::IsConsistentWithBackPointers() {
  int num_transitions = NumberOfTransitions();
  for (int i = 0; i < num_transitions; i++) {
    Map* target = GetTarget(i);
    if (!CheckOneBackPointer(map_, target)) return false;
  }
  return true;
}

// Estimates if there is a path from the object to a context.
// This function is not precise, and can return false even if
// there is a path to a context.
bool CanLeak(Object* obj, Heap* heap, bool skip_weak_cell) {
  if (!obj->IsHeapObject()) return false;
  if (obj->IsWeakCell()) {
    if (skip_weak_cell) return false;
    return CanLeak(WeakCell::cast(obj)->value(), heap, skip_weak_cell);
  }
  if (obj->IsCell()) {
    return CanLeak(Cell::cast(obj)->value(), heap, skip_weak_cell);
  }
  if (obj->IsPropertyCell()) {
    return CanLeak(PropertyCell::cast(obj)->value(), heap, skip_weak_cell);
  }
  if (obj->IsContext()) return true;
  if (obj->IsMap()) {
    Map* map = Map::cast(obj);
    for (int i = 0; i < Heap::kStrongRootListLength; i++) {
      Heap::RootListIndex root_index = static_cast<Heap::RootListIndex>(i);
      if (map == heap->root(root_index)) return false;
    }
    return true;
  }
  return CanLeak(HeapObject::cast(obj)->map(), heap, skip_weak_cell);
}


void Code::VerifyEmbeddedObjects(VerifyMode mode) {
  if (kind() == OPTIMIZED_FUNCTION) return;
  Heap* heap = GetIsolate()->heap();
  int mask = RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT);
  bool skip_weak_cell = (mode == kNoContextSpecificPointers) ? false : true;
  for (RelocIterator it(this, mask); !it.done(); it.next()) {
    Object* target = it.rinfo()->target_object();
    DCHECK(!CanLeak(target, heap, skip_weak_cell));
  }
}

#endif  // DEBUG

}  // namespace internal
}  // namespace v8
