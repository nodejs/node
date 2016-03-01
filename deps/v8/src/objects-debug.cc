// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects.h"

#include "src/bootstrapper.h"
#include "src/disasm.h"
#include "src/disassembler.h"
#include "src/macro-assembler.h"
#include "src/ostreams.h"
#include "src/regexp/jsregexp.h"

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
    case SIMD128_VALUE_TYPE:
      Simd128Value::cast(this)->Simd128ValueVerify();
      break;
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
    case TRANSITION_ARRAY_TYPE:
      TransitionArray::cast(this)->TransitionArrayVerify();
      break;
    case FREE_SPACE_TYPE:
      FreeSpace::cast(this)->FreeSpaceVerify();
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
    case JS_CONTEXT_EXTENSION_OBJECT_TYPE:
    case JS_PROMISE_TYPE:
      JSObject::cast(this)->JSObjectVerify();
      break;
    case JS_GENERATOR_OBJECT_TYPE:
      JSGeneratorObject::cast(this)->JSGeneratorObjectVerify();
      break;
    case JS_MODULE_TYPE:
      JSModule::cast(this)->JSModuleVerify();
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
    case JS_SET_TYPE:
      JSSet::cast(this)->JSSetVerify();
      break;
    case JS_MAP_TYPE:
      JSMap::cast(this)->JSMapVerify();
      break;
    case JS_SET_ITERATOR_TYPE:
      JSSetIterator::cast(this)->JSSetIteratorVerify();
      break;
    case JS_MAP_ITERATOR_TYPE:
      JSMapIterator::cast(this)->JSMapIteratorVerify();
      break;
    case JS_ITERATOR_RESULT_TYPE:
      JSIteratorResult::cast(this)->JSIteratorResultVerify();
      break;
    case JS_WEAK_MAP_TYPE:
      JSWeakMap::cast(this)->JSWeakMapVerify();
      break;
    case JS_WEAK_SET_TYPE:
      JSWeakSet::cast(this)->JSWeakSetVerify();
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
  CHECK_GT(Hash(), 0u);
  CHECK(name()->IsUndefined() || name()->IsString());
}


void HeapNumber::HeapNumberVerify() {
  CHECK(IsHeapNumber() || IsMutableHeapNumber());
}


void Simd128Value::Simd128ValueVerify() { CHECK(IsSimd128Value()); }


void ByteArray::ByteArrayVerify() {
  CHECK(IsByteArray());
}


void BytecodeArray::BytecodeArrayVerify() {
  // TODO(oth): Walk bytecodes and immediate values to validate sanity.
  CHECK(IsBytecodeArray());
  CHECK(constant_pool()->IsFixedArray());
  VerifyHeapPointer(constant_pool());
}


void FreeSpace::FreeSpaceVerify() {
  CHECK(IsFreeSpace());
}


template <class Traits>
void FixedTypedArray<Traits>::FixedTypedArrayVerify() {
  CHECK(IsHeapObject() &&
        HeapObject::cast(this)->map()->instance_type() ==
            Traits::kInstanceType);
  if (base_pointer() == this) {
    CHECK(external_pointer() ==
          ExternalReference::fixed_typed_array_base_data_offset().address());
  } else {
    CHECK(base_pointer() == nullptr);
  }
}


bool JSObject::ElementsAreSafeToExamine() {
  // If a GC was caused while constructing this object, the elements
  // pointer may point to a one pointer filler map.
  return reinterpret_cast<Map*>(elements()) !=
      GetHeap()->one_pointer_filler_map();
}


void JSObject::JSObjectVerify() {
  VerifyHeapPointer(properties());
  VerifyHeapPointer(elements());

  if (HasSloppyArgumentsElements()) {
    CHECK(this->elements()->IsFixedArray());
    CHECK_GE(this->elements()->length(), 2);
  }

  if (HasFastProperties()) {
    int actual_unused_property_fields = map()->GetInObjectProperties() +
                                        properties()->length() -
                                        map()->NextFreePropertyIndex();
    if (map()->unused_property_fields() != actual_unused_property_fields) {
      // This could actually happen in the middle of StoreTransitionStub
      // when the new extended backing store is already set into the object and
      // the allocation of the MutableHeapNumber triggers GC (in this case map
      // is not updated yet).
      CHECK_EQ(map()->unused_property_fields(),
               actual_unused_property_fields - JSObject::kFieldsAdded);
    }
    DescriptorArray* descriptors = map()->instance_descriptors();
    for (int i = 0; i < map()->NumberOfOwnDescriptors(); i++) {
      if (descriptors->GetDetails(i).type() == DATA) {
        Representation r = descriptors->GetDetails(i).representation();
        FieldIndex index = FieldIndex::ForDescriptor(map(), i);
        if (IsUnboxedDoubleField(index)) {
          DCHECK(r.IsDouble());
          continue;
        }
        Object* value = RawFastPropertyAt(index);
        if (r.IsDouble()) DCHECK(value->IsMutableHeapNumber());
        if (value->IsUninitialized()) continue;
        if (r.IsSmi()) DCHECK(value->IsSmi());
        if (r.IsHeapObject()) DCHECK(value->IsHeapObject());
        HeapType* field_type = descriptors->GetFieldType(i);
        bool type_is_none = field_type->Is(HeapType::None());
        bool type_is_any = HeapType::Any()->Is(field_type);
        if (r.IsNone()) {
          CHECK(type_is_none);
        } else if (!type_is_any && !(type_is_none && r.IsHeapObject())) {
          // If allocation folding is off then GC could happen during inner
          // object literal creation and we will end up having and undefined
          // value that does not match the field type.
          CHECK(!field_type->NowStable() || field_type->NowContains(value) ||
                (!FLAG_use_allocation_folding && value->IsUndefined()));
        }
      }
    }
  }

  // If a GC was caused while constructing this object, the elements
  // pointer may point to a one pointer filler map.
  if (ElementsAreSafeToExamine()) {
    CHECK_EQ((map()->has_fast_smi_or_object_elements() ||
              (elements() == GetHeap()->empty_fixed_array())),
             (elements()->map() == GetHeap()->fixed_array_map() ||
              elements()->map() == GetHeap()->fixed_cow_array_map()));
    CHECK(map()->has_fast_object_elements() == HasFastObjectElements());
  }
}


void Map::MapVerify() {
  Heap* heap = GetHeap();
  CHECK(!heap->InNewSpace(this));
  CHECK(FIRST_TYPE <= instance_type() && instance_type() <= LAST_TYPE);
  CHECK(instance_size() == kVariableSizeSentinel ||
         (kPointerSize <= instance_size() &&
          instance_size() < heap->Capacity()));
  CHECK(GetBackPointer()->IsUndefined() ||
        !Map::cast(GetBackPointer())->is_stable());
  VerifyHeapPointer(prototype());
  VerifyHeapPointer(instance_descriptors());
  SLOW_DCHECK(instance_descriptors()->IsSortedNoDuplicates());
  SLOW_DCHECK(TransitionArray::IsSortedNoDuplicates(this));
  SLOW_DCHECK(TransitionArray::IsConsistentWithBackPointers(this));
  // TODO(ishell): turn it back to SLOW_DCHECK.
  CHECK(!FLAG_unbox_double_fields ||
        layout_descriptor()->IsConsistentWithMap(this));
}


void Map::DictionaryMapVerify() {
  MapVerify();
  CHECK(is_dictionary_map());
  CHECK(instance_descriptors()->IsEmpty());
  CHECK_EQ(0, unused_property_fields());
  CHECK_EQ(Heap::GetStaticVisitorIdForMap(this), visitor_id());
}


void Map::VerifyOmittedMapChecks() {
  if (!FLAG_omit_map_checks_for_leaf_maps) return;
  if (!is_stable() ||
      is_deprecated() ||
      is_dictionary_map()) {
    CHECK(dependent_code()->IsEmpty(DependentCode::kPrototypeCheckGroup));
  }
}


void CodeCache::CodeCacheVerify() {
  VerifyHeapPointer(default_cache());
  VerifyHeapPointer(normal_type_cache());
  CHECK(default_cache()->IsFixedArray());
  CHECK(normal_type_cache()->IsUndefined()
         || normal_type_cache()->IsCodeCacheHashTable());
}


void PolymorphicCodeCache::PolymorphicCodeCacheVerify() {
  VerifyHeapPointer(cache());
  CHECK(cache()->IsUndefined() || cache()->IsPolymorphicCodeCacheHashTable());
}


void TypeFeedbackInfo::TypeFeedbackInfoVerify() {
  VerifyObjectField(kStorage1Offset);
  VerifyObjectField(kStorage2Offset);
  VerifyObjectField(kStorage3Offset);
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


void TransitionArray::TransitionArrayVerify() {
  for (int i = 0; i < length(); i++) {
    Object* e = get(i);
    VerifyPointer(e);
  }
  CHECK_LE(LengthFor(number_of_transitions()), length());
  CHECK(next_link()->IsUndefined() || next_link()->IsSmi() ||
        next_link()->IsTransitionArray());
}


void JSGeneratorObject::JSGeneratorObjectVerify() {
  // In an expression like "new g()", there can be a point where a generator
  // object is allocated but its fields are all undefined, as it hasn't yet been
  // initialized by the generator.  Hence these weak checks.
  VerifyObjectField(kFunctionOffset);
  VerifyObjectField(kContextOffset);
  VerifyObjectField(kReceiverOffset);
  VerifyObjectField(kOperandStackOffset);
  VerifyObjectField(kContinuationOffset);
}


void JSModule::JSModuleVerify() {
  VerifyObjectField(kContextOffset);
  VerifyObjectField(kScopeInfoOffset);
  CHECK(context()->IsUndefined() ||
        Context::cast(context())->IsModuleContext());
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
  CHECK(value()->IsUndefined() || value()->IsSmi() || value()->IsHeapNumber());
  CHECK(year()->IsUndefined() || year()->IsSmi() || year()->IsNaN());
  CHECK(month()->IsUndefined() || month()->IsSmi() || month()->IsNaN());
  CHECK(day()->IsUndefined() || day()->IsSmi() || day()->IsNaN());
  CHECK(weekday()->IsUndefined() || weekday()->IsSmi() || weekday()->IsNaN());
  CHECK(hour()->IsUndefined() || hour()->IsSmi() || hour()->IsNaN());
  CHECK(min()->IsUndefined() || min()->IsSmi() || min()->IsNaN());
  CHECK(sec()->IsUndefined() || sec()->IsSmi() || sec()->IsNaN());
  CHECK(cache_stamp()->IsUndefined() ||
        cache_stamp()->IsSmi() ||
        cache_stamp()->IsNaN());

  if (month()->IsSmi()) {
    int month = Smi::cast(this->month())->value();
    CHECK(0 <= month && month <= 11);
  }
  if (day()->IsSmi()) {
    int day = Smi::cast(this->day())->value();
    CHECK(1 <= day && day <= 31);
  }
  if (hour()->IsSmi()) {
    int hour = Smi::cast(this->hour())->value();
    CHECK(0 <= hour && hour <= 23);
  }
  if (min()->IsSmi()) {
    int min = Smi::cast(this->min())->value();
    CHECK(0 <= min && min <= 59);
  }
  if (sec()->IsSmi()) {
    int sec = Smi::cast(this->sec())->value();
    CHECK(0 <= sec && sec <= 59);
  }
  if (weekday()->IsSmi()) {
    int weekday = Smi::cast(this->weekday())->value();
    CHECK(0 <= weekday && weekday <= 6);
  }
  if (cache_stamp()->IsSmi()) {
    CHECK(Smi::cast(cache_stamp())->value() <=
          Smi::cast(GetIsolate()->date_cache()->stamp())->value());
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
  if (IsInternalizedString()) {
    CHECK(!GetHeap()->InNewSpace(this));
  }
  if (IsConsString()) {
    ConsString::cast(this)->ConsStringVerify();
  } else if (IsSlicedString()) {
    SlicedString::cast(this)->SlicedStringVerify();
  }
}


void ConsString::ConsStringVerify() {
  CHECK(this->first()->IsString());
  CHECK(this->second() == GetHeap()->empty_string() ||
        this->second()->IsString());
  CHECK(this->length() >= ConsString::kMinLength);
  CHECK(this->length() == this->first()->length() + this->second()->length());
  if (this->IsFlat()) {
    // A flat cons can only be created by String::SlowTryFlatten.
    // Afterwards, the first part may be externalized.
    CHECK(this->first()->IsSeqString() || this->first()->IsExternalString());
  }
}


void SlicedString::SlicedStringVerify() {
  CHECK(!this->parent()->IsConsString());
  CHECK(!this->parent()->IsSlicedString());
  CHECK(this->length() >= SlicedString::kMinLength);
}


void JSBoundFunction::JSBoundFunctionVerify() {
  CHECK(IsJSBoundFunction());
  JSObjectVerify();
  VerifyObjectField(kBoundThisOffset);
  VerifyObjectField(kBoundTargetFunctionOffset);
  VerifyObjectField(kBoundArgumentsOffset);
  VerifyObjectField(kCreationContextOffset);
  CHECK(bound_target_function()->IsCallable());
  CHECK(creation_context()->IsNativeContext());
  CHECK(IsCallable());
  CHECK_EQ(IsConstructor(), bound_target_function()->IsConstructor());
}


void JSFunction::JSFunctionVerify() {
  CHECK(IsJSFunction());
  VerifyObjectField(kPrototypeOrInitialMapOffset);
  VerifyObjectField(kNextFunctionLinkOffset);
  CHECK(code()->IsCode());
  CHECK(next_function_link() == NULL ||
        next_function_link()->IsUndefined() ||
        next_function_link()->IsJSFunction());
  CHECK(map()->is_callable());
}


void SharedFunctionInfo::SharedFunctionInfoVerify() {
  CHECK(IsSharedFunctionInfo());
  VerifyObjectField(kNameOffset);
  VerifyObjectField(kCodeOffset);
  VerifyObjectField(kOptimizedCodeMapOffset);
  VerifyObjectField(kFeedbackVectorOffset);
  VerifyObjectField(kScopeInfoOffset);
  VerifyObjectField(kInstanceClassNameOffset);
  CHECK(function_data()->IsUndefined() || IsApiFunction() ||
        HasBuiltinFunctionId() || HasBytecodeArray());
  VerifyObjectField(kFunctionDataOffset);
  VerifyObjectField(kScriptOffset);
  VerifyObjectField(kDebugInfoOffset);
}


void JSGlobalProxy::JSGlobalProxyVerify() {
  CHECK(IsJSGlobalProxy());
  JSObjectVerify();
  VerifyObjectField(JSGlobalProxy::kNativeContextOffset);
  // Make sure that this object has no properties, elements.
  CHECK_EQ(0, properties()->length());
  CHECK_EQ(0, FixedArray::cast(elements())->length());
}


void JSGlobalObject::JSGlobalObjectVerify() {
  CHECK(IsJSGlobalObject());
  // Do not check the dummy global object for the builtins.
  if (GlobalDictionary::cast(properties())->NumberOfElements() == 0 &&
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
    CHECK(number == heap->nan_value());
  } else {
    CHECK(number->IsSmi());
    int value = Smi::cast(number)->value();
    // Hidden oddballs have negative smis.
    const int kLeastHiddenOddballNumber = -5;
    CHECK_LE(value, 1);
    CHECK(value >= kLeastHiddenOddballNumber);
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
  } else if (map() == heap->no_interceptor_result_sentinel_map()) {
    CHECK(this == heap->no_interceptor_result_sentinel());
  } else if (map() == heap->arguments_marker_map()) {
    CHECK(this == heap->arguments_marker());
  } else if (map() == heap->termination_exception_map()) {
    CHECK(this == heap->termination_exception());
  } else if (map() == heap->exception_map()) {
    CHECK(this == heap->exception());
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
  VerifyObjectField(kNextOffset);
}


void Code::CodeVerify() {
  CHECK(IsAligned(reinterpret_cast<intptr_t>(instruction_start()),
                  kCodeAlignment));
  relocation_info()->ObjectVerify();
  Address last_gc_pc = NULL;
  Isolate* isolate = GetIsolate();
  for (RelocIterator it(this); !it.done(); it.next()) {
    it.rinfo()->Verify(isolate);
    // Ensure that GC will not iterate twice over the same pointer.
    if (RelocInfo::IsGCRelocMode(it.rinfo()->rmode())) {
      CHECK(it.rinfo()->pc() != last_gc_pc);
      last_gc_pc = it.rinfo()->pc();
    }
  }
  CHECK(raw_type_feedback_info() == Smi::FromInt(0) ||
        raw_type_feedback_info()->IsSmi() == IsCodeStubOrIC());
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
        WeakHashTable* table =
            GetIsolate()->heap()->weak_object_to_code_table();
        Handle<HeapObject> key_obj(HeapObject::cast(obj), isolate);
        CHECK(DependentCode::cast(table->Lookup(key_obj))
                  ->Contains(DependentCode::kWeakCodeGroup, cell));
      }
    }
  }
}


void JSArray::JSArrayVerify() {
  JSObjectVerify();
  CHECK(length()->IsNumber() || length()->IsUndefined());
  // If a GC was caused while constructing this array, the elements
  // pointer may point to a one pointer filler map.
  if (ElementsAreSafeToExamine()) {
    CHECK(elements()->IsUndefined() ||
          elements()->IsFixedArray() ||
          elements()->IsFixedDoubleArray());
  }
}


void JSSet::JSSetVerify() {
  CHECK(IsJSSet());
  JSObjectVerify();
  VerifyHeapPointer(table());
  CHECK(table()->IsOrderedHashTable() || table()->IsUndefined());
  // TODO(arv): Verify OrderedHashTable too.
}


void JSMap::JSMapVerify() {
  CHECK(IsJSMap());
  JSObjectVerify();
  VerifyHeapPointer(table());
  CHECK(table()->IsOrderedHashTable() || table()->IsUndefined());
  // TODO(arv): Verify OrderedHashTable too.
}


void JSSetIterator::JSSetIteratorVerify() {
  CHECK(IsJSSetIterator());
  JSObjectVerify();
  VerifyHeapPointer(table());
  CHECK(table()->IsOrderedHashTable() || table()->IsUndefined());
  CHECK(index()->IsSmi() || index()->IsUndefined());
  CHECK(kind()->IsSmi() || kind()->IsUndefined());
}


void JSMapIterator::JSMapIteratorVerify() {
  CHECK(IsJSMapIterator());
  JSObjectVerify();
  VerifyHeapPointer(table());
  CHECK(table()->IsOrderedHashTable() || table()->IsUndefined());
  CHECK(index()->IsSmi() || index()->IsUndefined());
  CHECK(kind()->IsSmi() || kind()->IsUndefined());
}


void JSIteratorResult::JSIteratorResultVerify() {
  CHECK(IsJSIteratorResult());
  JSObjectVerify();
  VerifyPointer(done());
  VerifyPointer(value());
}


void JSWeakMap::JSWeakMapVerify() {
  CHECK(IsJSWeakMap());
  JSObjectVerify();
  VerifyHeapPointer(table());
  CHECK(table()->IsHashTable() || table()->IsUndefined());
}


void JSWeakSet::JSWeakSetVerify() {
  CHECK(IsJSWeakSet());
  JSObjectVerify();
  VerifyHeapPointer(table());
  CHECK(table()->IsHashTable() || table()->IsUndefined());
}


void JSRegExp::JSRegExpVerify() {
  JSObjectVerify();
  CHECK(data()->IsUndefined() || data()->IsFixedArray());
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
      // Smi : Not compiled yet (-1) or code prepared for flushing.
      // JSObject: Compilation error.
      // Code/ByteArray: Compiled code.
      CHECK(
          one_byte_data->IsSmi() ||
          (is_native ? one_byte_data->IsCode() : one_byte_data->IsByteArray()));
      Object* uc16_data = arr->get(JSRegExp::kIrregexpUC16CodeIndex);
      CHECK(uc16_data->IsSmi() ||
             (is_native ? uc16_data->IsCode() : uc16_data->IsByteArray()));

      Object* one_byte_saved =
          arr->get(JSRegExp::kIrregexpLatin1CodeSavedIndex);
      CHECK(one_byte_saved->IsSmi() || one_byte_saved->IsString() ||
            one_byte_saved->IsCode());
      Object* uc16_saved = arr->get(JSRegExp::kIrregexpUC16CodeSavedIndex);
      CHECK(uc16_saved->IsSmi() || uc16_saved->IsString() ||
             uc16_saved->IsCode());

      CHECK(arr->get(JSRegExp::kIrregexpCaptureCountIndex)->IsSmi());
      CHECK(arr->get(JSRegExp::kIrregexpMaxRegisterCountIndex)->IsSmi());
      break;
    }
    default:
      CHECK_EQ(JSRegExp::NOT_COMPILED, TypeTag());
      CHECK(data()->IsUndefined());
      break;
  }
}


void JSProxy::JSProxyVerify() {
  CHECK(IsJSProxy());
  VerifyPointer(target());
  VerifyPointer(handler());
  CHECK_EQ(target()->IsCallable(), map()->is_callable());
  CHECK_EQ(target()->IsConstructor(), map()->is_constructor());
  CHECK(hash()->IsSmi() || hash()->IsUndefined());
  CHECK(map()->prototype()->IsNull());
  // There should be no properties on a Proxy.
  CHECK_EQ(0, map()->NumberOfOwnDescriptors());
}


void JSArrayBuffer::JSArrayBufferVerify() {
  CHECK(IsJSArrayBuffer());
  JSObjectVerify();
  VerifyPointer(byte_length());
  CHECK(byte_length()->IsSmi() || byte_length()->IsHeapNumber()
        || byte_length()->IsUndefined());
}


void JSArrayBufferView::JSArrayBufferViewVerify() {
  CHECK(IsJSArrayBufferView());
  JSObjectVerify();
  VerifyPointer(buffer());
  CHECK(buffer()->IsJSArrayBuffer() || buffer()->IsUndefined()
        || buffer() == Smi::FromInt(0));

  VerifyPointer(raw_byte_offset());
  CHECK(raw_byte_offset()->IsSmi() || raw_byte_offset()->IsHeapNumber() ||
        raw_byte_offset()->IsUndefined());

  VerifyPointer(raw_byte_length());
  CHECK(raw_byte_length()->IsSmi() || raw_byte_length()->IsHeapNumber() ||
        raw_byte_length()->IsUndefined());
}


void JSTypedArray::JSTypedArrayVerify() {
  CHECK(IsJSTypedArray());
  JSArrayBufferViewVerify();
  VerifyPointer(raw_length());
  CHECK(raw_length()->IsSmi() || raw_length()->IsHeapNumber() ||
        raw_length()->IsUndefined());

  VerifyPointer(elements());
}


void JSDataView::JSDataViewVerify() {
  CHECK(IsJSDataView());
  JSArrayBufferViewVerify();
}


void Foreign::ForeignVerify() {
  CHECK(IsForeign());
}


void Box::BoxVerify() {
  CHECK(IsBox());
  value()->ObjectVerify();
}


void PrototypeInfo::PrototypeInfoVerify() {
  CHECK(IsPrototypeInfo());
  if (prototype_users()->IsWeakFixedArray()) {
    WeakFixedArray::cast(prototype_users())->FixedArrayVerify();
  } else {
    CHECK(prototype_users()->IsSmi());
  }
  CHECK(validity_cell()->IsCell() || validity_cell()->IsSmi());
}


void AccessorInfo::AccessorInfoVerify() {
  VerifyPointer(name());
  VerifyPointer(expected_receiver_type());
}


void SloppyBlockWithEvalContextExtension::
    SloppyBlockWithEvalContextExtensionVerify() {
  CHECK(IsSloppyBlockWithEvalContextExtension());
  VerifyObjectField(kScopeInfoOffset);
  VerifyObjectField(kExtensionOffset);
}


void ExecutableAccessorInfo::ExecutableAccessorInfoVerify() {
  CHECK(IsExecutableAccessorInfo());
  AccessorInfoVerify();
  VerifyPointer(getter());
  VerifyPointer(setter());
  VerifyPointer(data());
}


void AccessorPair::AccessorPairVerify() {
  CHECK(IsAccessorPair());
  VerifyPointer(getter());
  VerifyPointer(setter());
}


void AccessCheckInfo::AccessCheckInfoVerify() {
  CHECK(IsAccessCheckInfo());
  VerifyPointer(named_callback());
  VerifyPointer(indexed_callback());
  VerifyPointer(callback());
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


void CallHandlerInfo::CallHandlerInfoVerify() {
  CHECK(IsCallHandlerInfo());
  VerifyPointer(callback());
  VerifyPointer(data());
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
}


void ObjectTemplateInfo::ObjectTemplateInfoVerify() {
  CHECK(IsObjectTemplateInfo());
  TemplateInfoVerify();
  VerifyPointer(constructor());
  VerifyPointer(internal_field_count());
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
    for (int i = 0; i < length(); i++) {
      Object* e = FixedArray::get(i);
      if (e->IsMap()) {
        Map::cast(e)->DictionaryMapVerify();
      } else {
        CHECK(e->IsUndefined());
      }
    }
  }
}


void DebugInfo::DebugInfoVerify() {
  CHECK(IsDebugInfo());
  VerifyPointer(shared());
  VerifyPointer(code());
  VerifyPointer(break_points());
}


void BreakPointInfo::BreakPointInfoVerify() {
  CHECK(IsBreakPointInfo());
  VerifyPointer(break_point_objects());
}
#endif  // VERIFY_HEAP

#ifdef DEBUG

void JSObject::IncrementSpillStatistics(SpillInformation* info) {
  info->number_of_objects_++;
  // Named properties
  if (HasFastProperties()) {
    info->number_of_objects_with_fast_properties_++;
    info->number_of_fast_used_fields_   += map()->NextFreePropertyIndex();
    info->number_of_fast_unused_fields_ += map()->unused_property_fields();
  } else if (IsJSGlobalObject()) {
    GlobalDictionary* dict = global_dictionary();
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
    case FAST_HOLEY_SMI_ELEMENTS:
    case FAST_SMI_ELEMENTS:
    case FAST_HOLEY_DOUBLE_ELEMENTS:
    case FAST_DOUBLE_ELEMENTS:
    case FAST_HOLEY_ELEMENTS:
    case FAST_ELEMENTS: {
      info->number_of_objects_with_fast_elements_++;
      int holes = 0;
      FixedArray* e = FixedArray::cast(elements());
      int len = e->length();
      Heap* heap = GetHeap();
      for (int i = 0; i < len; i++) {
        if (e->get(i) == heap->the_hole_value()) holes++;
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
    case DICTIONARY_ELEMENTS: {
      SeededNumberDictionary* dict = element_dictionary();
      info->number_of_slow_used_elements_ += dict->NumberOfElements();
      info->number_of_slow_unused_elements_ +=
          dict->Capacity() - dict->NumberOfElements();
      break;
    }
    case FAST_SLOPPY_ARGUMENTS_ELEMENTS:
    case SLOW_SLOPPY_ARGUMENTS_ELEMENTS:
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
  Name* current_key = NULL;
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
  DCHECK(valid_entries == -1);
  Name* prev_key = NULL;
  PropertyKind prev_kind = kData;
  PropertyAttributes prev_attributes = NONE;
  uint32_t prev_hash = 0;
  for (int i = 0; i < number_of_transitions(); i++) {
    Name* key = GetSortedKey(i);
    uint32_t hash = key->Hash();
    PropertyKind kind = kData;
    PropertyAttributes attributes = NONE;
    if (!IsSpecialTransition(key)) {
      Map* target = GetTarget(i);
      PropertyDetails details = GetTargetDetails(key, target);
      kind = details.kind();
      attributes = details.attributes();
    } else {
      // Duplicate entries are not allowed for non-property transitions.
      CHECK_NE(prev_key, key);
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


// static
bool TransitionArray::IsSortedNoDuplicates(Map* map) {
  Object* raw_transitions = map->raw_transitions();
  if (IsFullTransitionArray(raw_transitions)) {
    return TransitionArray::cast(raw_transitions)->IsSortedNoDuplicates();
  }
  // Simple and non-existent transitions are always sorted.
  return true;
}


static bool CheckOneBackPointer(Map* current_map, Object* target) {
  return !target->IsMap() || Map::cast(target)->GetBackPointer() == current_map;
}


// static
bool TransitionArray::IsConsistentWithBackPointers(Map* map) {
  Object* transitions = map->raw_transitions();
  for (int i = 0; i < TransitionArray::NumberOfTransitions(transitions); ++i) {
    Map* target = TransitionArray::GetTarget(transitions, i);
    if (!CheckOneBackPointer(map, target)) return false;
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
  int mask = RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT) |
             RelocInfo::ModeMask(RelocInfo::CELL);
  bool skip_weak_cell = (mode == kNoContextSpecificPointers) ? false : true;
  for (RelocIterator it(this, mask); !it.done(); it.next()) {
    Object* target = it.rinfo()->rmode() == RelocInfo::CELL
                         ? it.rinfo()->target_cell()
                         : it.rinfo()->target_object();
    CHECK(!CanLeak(target, heap, skip_weak_cell));
  }
}


// Verify that the debugger can redirect old code to the new code.
void Code::VerifyRecompiledCode(Code* old_code, Code* new_code) {
  if (old_code->kind() != FUNCTION) return;
  if (new_code->kind() != FUNCTION) return;
  Isolate* isolate = old_code->GetIsolate();
  // Do not verify during bootstrapping. We may replace code using %SetCode.
  if (isolate->bootstrapper()->IsActive()) return;

  static const int mask = RelocInfo::kCodeTargetMask;
  RelocIterator old_it(old_code, mask);
  RelocIterator new_it(new_code, mask);
  Code* stack_check = isolate->builtins()->builtin(Builtins::kStackCheck);

  while (!old_it.done()) {
    RelocInfo* rinfo = old_it.rinfo();
    Code* target = Code::GetCodeFromTargetAddress(rinfo->target_address());
    CHECK(!target->is_handler() && !target->is_inline_cache_stub());
    if (target == stack_check) break;
    old_it.next();
  }

  while (!new_it.done()) {
    RelocInfo* rinfo = new_it.rinfo();
    Code* target = Code::GetCodeFromTargetAddress(rinfo->target_address());
    CHECK(!target->is_handler() && !target->is_inline_cache_stub());
    if (target == stack_check) break;
    new_it.next();
  }

  // Either both are done because there is no stack check.
  // Or we are past the prologue for both.
  CHECK_EQ(new_it.done(), old_it.done());

  // After the prologue, each call in the old code has a corresponding call
  // in the new code.
  while (!old_it.done() && !new_it.done()) {
    Code* old_target =
        Code::GetCodeFromTargetAddress(old_it.rinfo()->target_address());
    Code* new_target =
        Code::GetCodeFromTargetAddress(new_it.rinfo()->target_address());
    CHECK_EQ(old_target->kind(), new_target->kind());
    // Check call target for equality unless it's an IC or an interrupt check.
    // In both cases they may be patched to be something else.
    if (!old_target->is_handler() && !old_target->is_inline_cache_stub() &&
        new_target != isolate->builtins()->builtin(Builtins::kInterruptCheck)) {
      CHECK_EQ(old_target, new_target);
    }
    old_it.next();
    new_it.next();
  }

  // Both are done at the same time.
  CHECK_EQ(new_it.done(), old_it.done());
}


#endif  // DEBUG

}  // namespace internal
}  // namespace v8
