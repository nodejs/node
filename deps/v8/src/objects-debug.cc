// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/disasm.h"
#include "src/disassembler.h"
#include "src/heap/objects-visiting.h"
#include "src/jsregexp.h"
#include "src/macro-assembler.h"
#include "src/ostreams.h"

namespace v8 {
namespace internal {

#ifdef VERIFY_HEAP

void Object::ObjectVerify() {
  if (IsSmi()) {
    Smi::cast(this)->SmiVerify();
  } else {
    HeapObject::cast(this)->HeapObjectVerify();
  }
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
    case FIXED_ARRAY_TYPE:
      FixedArray::cast(this)->FixedArrayVerify();
      break;
    case FIXED_DOUBLE_ARRAY_TYPE:
      FixedDoubleArray::cast(this)->FixedDoubleArrayVerify();
      break;
    case CONSTANT_POOL_ARRAY_TYPE:
      ConstantPoolArray::cast(this)->ConstantPoolArrayVerify();
      break;
    case BYTE_ARRAY_TYPE:
      ByteArray::cast(this)->ByteArrayVerify();
      break;
    case FREE_SPACE_TYPE:
      FreeSpace::cast(this)->FreeSpaceVerify();
      break;

#define VERIFY_TYPED_ARRAY(Type, type, TYPE, ctype, size)                      \
    case EXTERNAL_##TYPE##_ARRAY_TYPE:                                         \
      External##Type##Array::cast(this)->External##Type##ArrayVerify();        \
      break;                                                                   \
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
    case JS_FUNCTION_TYPE:
      JSFunction::cast(this)->JSFunctionVerify();
      break;
    case JS_GLOBAL_PROXY_TYPE:
      JSGlobalProxy::cast(this)->JSGlobalProxyVerify();
      break;
    case JS_GLOBAL_OBJECT_TYPE:
      JSGlobalObject::cast(this)->JSGlobalObjectVerify();
      break;
    case JS_BUILTINS_OBJECT_TYPE:
      JSBuiltinsObject::cast(this)->JSBuiltinsObjectVerify();
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
    case JS_FUNCTION_PROXY_TYPE:
      JSFunctionProxy::cast(this)->JSFunctionProxyVerify();
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
  CHECK_GT(Hash(), 0);
  CHECK(name()->IsUndefined() || name()->IsString());
  CHECK(flags()->IsSmi());
}


void HeapNumber::HeapNumberVerify() {
  CHECK(IsHeapNumber() || IsMutableHeapNumber());
}


void ByteArray::ByteArrayVerify() {
  CHECK(IsByteArray());
}


void FreeSpace::FreeSpaceVerify() {
  CHECK(IsFreeSpace());
}


#define EXTERNAL_ARRAY_VERIFY(Type, type, TYPE, ctype, size)                  \
  void External##Type##Array::External##Type##ArrayVerify() {                 \
    CHECK(IsExternal##Type##Array());                                         \
  }

TYPED_ARRAYS(EXTERNAL_ARRAY_VERIFY)
#undef EXTERNAL_ARRAY_VERIFY


template <class Traits>
void FixedTypedArray<Traits>::FixedTypedArrayVerify() {
  CHECK(IsHeapObject() &&
        HeapObject::cast(this)->map()->instance_type() ==
            Traits::kInstanceType);
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

  if (GetElementsKind() == SLOPPY_ARGUMENTS_ELEMENTS) {
    CHECK(this->elements()->IsFixedArray());
    CHECK_GE(this->elements()->length(), 2);
  }

  if (HasFastProperties()) {
    int actual_unused_property_fields = map()->inobject_properties() +
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
      if (descriptors->GetDetails(i).type() == FIELD) {
        Representation r = descriptors->GetDetails(i).representation();
        FieldIndex index = FieldIndex::ForDescriptor(map(), i);
        Object* value = RawFastPropertyAt(index);
        if (r.IsDouble()) DCHECK(value->IsMutableHeapNumber());
        if (value->IsUninitialized()) continue;
        if (r.IsSmi()) DCHECK(value->IsSmi());
        if (r.IsHeapObject()) DCHECK(value->IsHeapObject());
        HeapType* field_type = descriptors->GetFieldType(i);
        if (r.IsNone()) {
          CHECK(field_type->Is(HeapType::None()));
        } else if (!HeapType::Any()->Is(field_type)) {
          CHECK(!field_type->NowStable() || field_type->NowContains(value));
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
  VerifyHeapPointer(prototype());
  VerifyHeapPointer(instance_descriptors());
  SLOW_DCHECK(instance_descriptors()->IsSortedNoDuplicates());
  if (HasTransitionArray()) {
    SLOW_DCHECK(transitions()->IsSortedNoDuplicates());
    SLOW_DCHECK(transitions()->IsConsistentWithBackPointers(this));
  }
}


void Map::DictionaryMapVerify() {
  MapVerify();
  CHECK(is_dictionary_map());
  CHECK(instance_descriptors()->IsEmpty());
  CHECK_EQ(0, pre_allocated_property_fields());
  CHECK_EQ(0, unused_property_fields());
  CHECK_EQ(StaticVisitorBase::GetVisitorId(instance_type(), instance_size()),
      visitor_id());
}


void Map::VerifyOmittedMapChecks() {
  if (!FLAG_omit_map_checks_for_leaf_maps) return;
  if (!is_stable() ||
      is_deprecated() ||
      HasTransitionArray() ||
      is_dictionary_map()) {
    CHECK_EQ(0, dependent_code()->number_of_entries(
        DependentCode::kPrototypeCheckGroup));
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
      double value = get_scalar(i);
      CHECK(!std::isnan(value) ||
            (bit_cast<uint64_t>(value) ==
             bit_cast<uint64_t>(canonical_not_the_hole_nan_as_double())) ||
            ((bit_cast<uint64_t>(value) & Double::kSignMask) != 0));
    }
  }
}


void ConstantPoolArray::ConstantPoolArrayVerify() {
  CHECK(IsConstantPoolArray());
  ConstantPoolArray::Iterator code_iter(this, ConstantPoolArray::CODE_PTR);
  while (!code_iter.is_finished()) {
    Address code_entry = get_code_ptr_entry(code_iter.next_index());
    VerifyPointer(Code::GetCodeFromTargetAddress(code_entry));
  }
  ConstantPoolArray::Iterator heap_iter(this, ConstantPoolArray::HEAP_PTR);
  while (!heap_iter.is_finished()) {
    VerifyObjectField(OffsetOfElementAt(heap_iter.next_index()));
  }
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
  VerifyObjectField(kStackHandlerIndexOffset);
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
  CHECK(type()->IsString());
  CHECK(arguments()->IsJSArray());
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


void JSFunction::JSFunctionVerify() {
  CHECK(IsJSFunction());
  VerifyObjectField(kPrototypeOrInitialMapOffset);
  VerifyObjectField(kNextFunctionLinkOffset);
  CHECK(code()->IsCode());
  CHECK(next_function_link() == NULL ||
        next_function_link()->IsUndefined() ||
        next_function_link()->IsJSFunction());
}


void SharedFunctionInfo::SharedFunctionInfoVerify() {
  CHECK(IsSharedFunctionInfo());
  VerifyObjectField(kNameOffset);
  VerifyObjectField(kCodeOffset);
  VerifyObjectField(kOptimizedCodeMapOffset);
  VerifyObjectField(kFeedbackVectorOffset);
  VerifyObjectField(kScopeInfoOffset);
  VerifyObjectField(kInstanceClassNameOffset);
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
  CHECK_EQ(FAST_HOLEY_SMI_ELEMENTS, GetElementsKind());
  CHECK_EQ(0, FixedArray::cast(elements())->length());
}


void JSGlobalObject::JSGlobalObjectVerify() {
  CHECK(IsJSGlobalObject());
  JSObjectVerify();
  for (int i = GlobalObject::kBuiltinsOffset;
       i < JSGlobalObject::kSize;
       i += kPointerSize) {
    VerifyObjectField(i);
  }
}


void JSBuiltinsObject::JSBuiltinsObjectVerify() {
  CHECK(IsJSBuiltinsObject());
  JSObjectVerify();
  for (int i = GlobalObject::kBuiltinsOffset;
       i < JSBuiltinsObject::kSize;
       i += kPointerSize) {
    VerifyObjectField(i);
  }
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
  VerifyObjectField(kTypeOffset);
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
  DisallowHeapAllocation no_gc;
  Isolate* isolate = GetIsolate();
  HandleScope scope(isolate);
  int mode_mask = RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT);
  for (RelocIterator it(this, mode_mask); !it.done(); it.next()) {
    Object* obj = it.rinfo()->target_object();
    if (IsWeakObject(obj)) {
      if (obj->IsMap()) {
        Map* map = Map::cast(obj);
        DependentCode::DependencyGroup group = is_optimized_code() ?
            DependentCode::kWeakCodeGroup : DependentCode::kWeakICGroup;
        CHECK(map->dependent_code()->Contains(group, this));
      } else if (obj->IsJSObject()) {
        Object* raw_table = GetIsolate()->heap()->weak_object_to_code_table();
        WeakHashTable* table = WeakHashTable::cast(raw_table);
        Handle<Object> key_obj(obj, isolate);
        CHECK(DependentCode::cast(table->Lookup(key_obj))->Contains(
            DependentCode::kWeakCodeGroup, this));
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
  VerifyPointer(handler());
  CHECK(hash()->IsSmi() || hash()->IsUndefined());
}


void JSFunctionProxy::JSFunctionProxyVerify() {
  CHECK(IsJSFunctionProxy());
  JSProxyVerify();
  VerifyPointer(call_trap());
  VerifyPointer(construct_trap());
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

  VerifyPointer(byte_offset());
  CHECK(byte_offset()->IsSmi() || byte_offset()->IsHeapNumber()
        || byte_offset()->IsUndefined());

  VerifyPointer(byte_length());
  CHECK(byte_length()->IsSmi() || byte_length()->IsHeapNumber()
        || byte_length()->IsUndefined());
}


void JSTypedArray::JSTypedArrayVerify() {
  CHECK(IsJSTypedArray());
  JSArrayBufferViewVerify();
  VerifyPointer(length());
  CHECK(length()->IsSmi() || length()->IsHeapNumber()
        || length()->IsUndefined());

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


void AccessorInfo::AccessorInfoVerify() {
  VerifyPointer(name());
  VerifyPointer(flag());
  VerifyPointer(expected_receiver_type());
}


void ExecutableAccessorInfo::ExecutableAccessorInfoVerify() {
  CHECK(IsExecutableAccessorInfo());
  AccessorInfoVerify();
  VerifyPointer(getter());
  VerifyPointer(setter());
  VerifyPointer(data());
}


void DeclaredAccessorDescriptor::DeclaredAccessorDescriptorVerify() {
  CHECK(IsDeclaredAccessorDescriptor());
  VerifyPointer(serialized_data());
}


void DeclaredAccessorInfo::DeclaredAccessorInfoVerify() {
  CHECK(IsDeclaredAccessorInfo());
  AccessorInfoVerify();
  VerifyPointer(descriptor());
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


void SignatureInfo::SignatureInfoVerify() {
  CHECK(IsSignatureInfo());
  VerifyPointer(receiver());
  VerifyPointer(args());
}


void TypeSwitchInfo::TypeSwitchInfoVerify() {
  CHECK(IsTypeSwitchInfo());
  VerifyPointer(types());
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
  line_offset()->SmiVerify();
  column_offset()->SmiVerify();
  VerifyPointer(wrapper());
  type()->SmiVerify();
  VerifyPointer(line_ends());
  VerifyPointer(id());
}


void JSFunctionResultCache::JSFunctionResultCacheVerify() {
  JSFunction::cast(get(kFactoryIndex))->ObjectVerify();

  int size = Smi::cast(get(kCacheSizeIndex))->value();
  CHECK(kEntriesIndex <= size);
  CHECK(size <= length());
  CHECK_EQ(0, size % kEntrySize);

  int finger = Smi::cast(get(kFingerIndex))->value();
  CHECK(kEntriesIndex <= finger);
  CHECK((finger < size) || (finger == kEntriesIndex && finger == size));
  CHECK_EQ(0, finger % kEntrySize);

  if (FLAG_enable_slow_asserts) {
    for (int i = kEntriesIndex; i < size; i++) {
      CHECK(!get(i)->IsTheHole());
      get(i)->ObjectVerify();
    }
    for (int i = size; i < length(); i++) {
      CHECK(get(i)->IsTheHole());
      get(i)->ObjectVerify();
    }
  }
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
  VerifyPointer(original_code());
  VerifyPointer(code());
  VerifyPointer(break_points());
}


void BreakPointInfo::BreakPointInfoVerify() {
  CHECK(IsBreakPointInfo());
  code_position()->SmiVerify();
  source_position()->SmiVerify();
  statement_position()->SmiVerify();
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
    case EXTERNAL_##TYPE##_ELEMENTS:                                          \
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
    case SLOPPY_ARGUMENTS_ELEMENTS:
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
  bool prev_is_data_property = false;
  PropertyAttributes prev_attributes = NONE;
  uint32_t prev_hash = 0;
  for (int i = 0; i < number_of_transitions(); i++) {
    Name* key = GetSortedKey(i);
    uint32_t hash = key->Hash();
    bool is_data_property = false;
    PropertyAttributes attributes = NONE;
    if (!IsSpecialTransition(key)) {
      Map* target = GetTarget(i);
      PropertyDetails details = GetTargetDetails(key, target);
      is_data_property = details.type() == FIELD || details.type() == CONSTANT;
      attributes = details.attributes();
    } else {
      // Duplicate entries are not allowed for non-property transitions.
      CHECK_NE(prev_key, key);
    }

    int cmp =
        CompareKeys(prev_key, prev_hash, prev_is_data_property, prev_attributes,
                    key, hash, is_data_property, attributes);
    if (cmp >= 0) {
      Print();
      return false;
    }
    prev_key = key;
    prev_hash = hash;
    prev_attributes = attributes;
    prev_is_data_property = is_data_property;
  }
  return true;
}


static bool CheckOneBackPointer(Map* current_map, Object* target) {
  return !target->IsMap() || Map::cast(target)->GetBackPointer() == current_map;
}


bool TransitionArray::IsConsistentWithBackPointers(Map* current_map) {
  for (int i = 0; i < number_of_transitions(); ++i) {
    if (!CheckOneBackPointer(current_map, GetTarget(i))) return false;
  }
  return true;
}


void Code::VerifyEmbeddedObjectsInFullCode() {
  // Check that no context-specific object has been embedded.
  Heap* heap = GetIsolate()->heap();
  int mask = RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT);
  for (RelocIterator it(this, mask); !it.done(); it.next()) {
    Object* obj = it.rinfo()->target_object();
    if (obj->IsCell()) obj = Cell::cast(obj)->value();
    if (obj->IsPropertyCell()) obj = PropertyCell::cast(obj)->value();
    if (!obj->IsHeapObject()) continue;
    Map* map = obj->IsMap() ? Map::cast(obj) : HeapObject::cast(obj)->map();
    int i = 0;
    while (map != heap->roots_array_start()[i++]) {
      CHECK_LT(i, Heap::kStrongRootListLength);
    }
  }
}


#endif  // DEBUG

} }  // namespace v8::internal
