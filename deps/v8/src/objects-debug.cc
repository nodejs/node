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
#include "src/objects/arguments-inl.h"
#include "src/objects/bigint.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/js-collator-inl.h"
#endif  // V8_INTL_SUPPORT
#include "src/objects/data-handler-inl.h"
#include "src/objects/debug-objects-inl.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/js-collection-inl.h"
#include "src/objects/js-generator-inl.h"
#include "src/objects/literal-objects-inl.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/js-list-format-inl.h"
#include "src/objects/js-locale-inl.h"
#endif  // V8_INTL_SUPPORT
#include "src/objects/js-regexp-inl.h"
#include "src/objects/js-regexp-string-iterator-inl.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/js-plural-rules-inl.h"
#include "src/objects/js-relative-time-format-inl.h"
#endif  // V8_INTL_SUPPORT
#include "src/objects/maybe-object.h"
#include "src/objects/microtask-inl.h"
#include "src/objects/module-inl.h"
#include "src/objects/promise-inl.h"
#include "src/ostreams.h"
#include "src/regexp/jsregexp.h"
#include "src/transitions.h"
#include "src/wasm/wasm-objects-inl.h"

namespace v8 {
namespace internal {

// Heap Verification Overview
// --------------------------
// - Each InstanceType has a separate XXXVerify method which checks an object's
//   integrity in isolation.
// - --verify-heap will iterate over all gc spaces and call ObjectVerify() on
//   every encountered tagged pointer.
// - Verification should be pushed down to the specific instance type if its
//   integrity is independent of an outer object.
// - In cases where the InstanceType is too genernic (e.g. FixedArray) the
//   XXXVerify of the outer method has to do recursive verification.
// - If the corresponding objects have inheritence the parent's Verify method
//   is called as well.
// - For any field containing pointes VerifyPointer(...) should be called.
//
// Caveats
// -------
// - Assume that any of the verify methods is incomplete!
// - Some integrity checks are only partially done due to objects being in
//   partially initialized states when a gc happens, for instance when outer
//   objects are allocted before inner ones.
//

#ifdef VERIFY_HEAP

void Object::ObjectVerify(Isolate* isolate) {
  if (IsSmi()) {
    Smi::cast(this)->SmiVerify(isolate);
  } else {
    HeapObject::cast(this)->HeapObjectVerify(isolate);
  }
  CHECK(!IsConstructor() || IsCallable());
}

void Object::VerifyPointer(Isolate* isolate, Object* p) {
  if (p->IsHeapObject()) {
    HeapObject::VerifyHeapPointer(isolate, p);
  } else {
    CHECK(p->IsSmi());
  }
}

void MaybeObject::VerifyMaybeObjectPointer(Isolate* isolate, MaybeObject* p) {
  HeapObject* heap_object;
  if (p->ToStrongOrWeakHeapObject(&heap_object)) {
    HeapObject::VerifyHeapPointer(isolate, heap_object);
  } else {
    CHECK(p->IsSmi() || p->IsClearedWeakHeapObject());
  }
}

namespace {
void VerifyForeignPointer(Isolate* isolate, HeapObject* host, Object* foreign) {
  host->VerifyPointer(isolate, foreign);
  CHECK(foreign->IsUndefined(isolate) || Foreign::IsNormalized(foreign));
}
}  // namespace

void Smi::SmiVerify(Isolate* isolate) {
  CHECK(IsSmi());
  CHECK(!IsCallable());
  CHECK(!IsConstructor());
}

void HeapObject::HeapObjectVerify(Isolate* isolate) {
  VerifyHeapPointer(isolate, map());
  CHECK(map()->IsMap());

  switch (map()->instance_type()) {
#define STRING_TYPE_CASE(TYPE, size, name, camel_name) case TYPE:
    STRING_TYPE_LIST(STRING_TYPE_CASE)
#undef STRING_TYPE_CASE
    String::cast(this)->StringVerify(isolate);
    break;
    case SYMBOL_TYPE:
      Symbol::cast(this)->SymbolVerify(isolate);
      break;
    case MAP_TYPE:
      Map::cast(this)->MapVerify(isolate);
      break;
    case HEAP_NUMBER_TYPE:
      CHECK(IsHeapNumber());
      break;
    case MUTABLE_HEAP_NUMBER_TYPE:
      CHECK(IsMutableHeapNumber());
      break;
    case BIGINT_TYPE:
      BigInt::cast(this)->BigIntVerify(isolate);
      break;
    case CALL_HANDLER_INFO_TYPE:
      CallHandlerInfo::cast(this)->CallHandlerInfoVerify(isolate);
      break;
    case OBJECT_BOILERPLATE_DESCRIPTION_TYPE:
      ObjectBoilerplateDescription::cast(this)
          ->ObjectBoilerplateDescriptionVerify(isolate);
      break;
    // FixedArray types
    case HASH_TABLE_TYPE:
    case ORDERED_HASH_MAP_TYPE:
    case ORDERED_HASH_SET_TYPE:
    case NAME_DICTIONARY_TYPE:
    case GLOBAL_DICTIONARY_TYPE:
    case NUMBER_DICTIONARY_TYPE:
    case SIMPLE_NUMBER_DICTIONARY_TYPE:
    case STRING_TABLE_TYPE:
    case EPHEMERON_HASH_TABLE_TYPE:
    case FIXED_ARRAY_TYPE:
    case SCOPE_INFO_TYPE:
    case SCRIPT_CONTEXT_TABLE_TYPE:
    case BLOCK_CONTEXT_TYPE:
    case CATCH_CONTEXT_TYPE:
    case DEBUG_EVALUATE_CONTEXT_TYPE:
    case EVAL_CONTEXT_TYPE:
    case FUNCTION_CONTEXT_TYPE:
    case MODULE_CONTEXT_TYPE:
    case NATIVE_CONTEXT_TYPE:
    case SCRIPT_CONTEXT_TYPE:
    case WITH_CONTEXT_TYPE:
      FixedArray::cast(this)->FixedArrayVerify(isolate);
      break;
    case WEAK_FIXED_ARRAY_TYPE:
      WeakFixedArray::cast(this)->WeakFixedArrayVerify(isolate);
      break;
    case WEAK_ARRAY_LIST_TYPE:
      WeakArrayList::cast(this)->WeakArrayListVerify(isolate);
      break;
    case FIXED_DOUBLE_ARRAY_TYPE:
      FixedDoubleArray::cast(this)->FixedDoubleArrayVerify(isolate);
      break;
    case FEEDBACK_METADATA_TYPE:
      FeedbackMetadata::cast(this)->FeedbackMetadataVerify(isolate);
      break;
    case BYTE_ARRAY_TYPE:
      ByteArray::cast(this)->ByteArrayVerify(isolate);
      break;
    case BYTECODE_ARRAY_TYPE:
      BytecodeArray::cast(this)->BytecodeArrayVerify(isolate);
      break;
    case DESCRIPTOR_ARRAY_TYPE:
      DescriptorArray::cast(this)->DescriptorArrayVerify(isolate);
      break;
    case TRANSITION_ARRAY_TYPE:
      TransitionArray::cast(this)->TransitionArrayVerify(isolate);
      break;
    case PROPERTY_ARRAY_TYPE:
      PropertyArray::cast(this)->PropertyArrayVerify(isolate);
      break;
    case FREE_SPACE_TYPE:
      FreeSpace::cast(this)->FreeSpaceVerify(isolate);
      break;
    case FEEDBACK_CELL_TYPE:
      FeedbackCell::cast(this)->FeedbackCellVerify(isolate);
      break;
    case FEEDBACK_VECTOR_TYPE:
      FeedbackVector::cast(this)->FeedbackVectorVerify(isolate);
      break;

#define VERIFY_TYPED_ARRAY(Type, type, TYPE, ctype)                 \
  case FIXED_##TYPE##_ARRAY_TYPE:                                   \
    Fixed##Type##Array::cast(this)->FixedTypedArrayVerify(isolate); \
    break;

      TYPED_ARRAYS(VERIFY_TYPED_ARRAY)
#undef VERIFY_TYPED_ARRAY

    case CODE_TYPE:
      Code::cast(this)->CodeVerify(isolate);
      break;
    case ODDBALL_TYPE:
      Oddball::cast(this)->OddballVerify(isolate);
      break;
    case JS_OBJECT_TYPE:
    case JS_ERROR_TYPE:
    case JS_API_OBJECT_TYPE:
    case JS_SPECIAL_API_OBJECT_TYPE:
    case JS_CONTEXT_EXTENSION_OBJECT_TYPE:
    case WASM_GLOBAL_TYPE:
    case WASM_MEMORY_TYPE:
    case WASM_TABLE_TYPE:
      JSObject::cast(this)->JSObjectVerify(isolate);
      break;
    case WASM_MODULE_TYPE:
      WasmModuleObject::cast(this)->WasmModuleObjectVerify(isolate);
      break;
    case WASM_INSTANCE_TYPE:
      WasmInstanceObject::cast(this)->WasmInstanceObjectVerify(isolate);
      break;
    case JS_ARGUMENTS_TYPE:
      JSArgumentsObject::cast(this)->JSArgumentsObjectVerify(isolate);
      break;
    case JS_GENERATOR_OBJECT_TYPE:
      JSGeneratorObject::cast(this)->JSGeneratorObjectVerify(isolate);
      break;
    case JS_ASYNC_GENERATOR_OBJECT_TYPE:
      JSAsyncGeneratorObject::cast(this)->JSAsyncGeneratorObjectVerify(isolate);
      break;
    case JS_VALUE_TYPE:
      JSValue::cast(this)->JSValueVerify(isolate);
      break;
    case JS_DATE_TYPE:
      JSDate::cast(this)->JSDateVerify(isolate);
      break;
    case JS_BOUND_FUNCTION_TYPE:
      JSBoundFunction::cast(this)->JSBoundFunctionVerify(isolate);
      break;
    case JS_FUNCTION_TYPE:
      JSFunction::cast(this)->JSFunctionVerify(isolate);
      break;
    case JS_GLOBAL_PROXY_TYPE:
      JSGlobalProxy::cast(this)->JSGlobalProxyVerify(isolate);
      break;
    case JS_GLOBAL_OBJECT_TYPE:
      JSGlobalObject::cast(this)->JSGlobalObjectVerify(isolate);
      break;
    case CELL_TYPE:
      Cell::cast(this)->CellVerify(isolate);
      break;
    case PROPERTY_CELL_TYPE:
      PropertyCell::cast(this)->PropertyCellVerify(isolate);
      break;
    case JS_ARRAY_TYPE:
      JSArray::cast(this)->JSArrayVerify(isolate);
      break;
    case JS_MODULE_NAMESPACE_TYPE:
      JSModuleNamespace::cast(this)->JSModuleNamespaceVerify(isolate);
      break;
    case JS_SET_TYPE:
      JSSet::cast(this)->JSSetVerify(isolate);
      break;
    case JS_MAP_TYPE:
      JSMap::cast(this)->JSMapVerify(isolate);
      break;
    case JS_SET_KEY_VALUE_ITERATOR_TYPE:
    case JS_SET_VALUE_ITERATOR_TYPE:
      JSSetIterator::cast(this)->JSSetIteratorVerify(isolate);
      break;
    case JS_MAP_KEY_ITERATOR_TYPE:
    case JS_MAP_KEY_VALUE_ITERATOR_TYPE:
    case JS_MAP_VALUE_ITERATOR_TYPE:
      JSMapIterator::cast(this)->JSMapIteratorVerify(isolate);
      break;
    case JS_ARRAY_ITERATOR_TYPE:
      JSArrayIterator::cast(this)->JSArrayIteratorVerify(isolate);
      break;
    case JS_STRING_ITERATOR_TYPE:
      JSStringIterator::cast(this)->JSStringIteratorVerify(isolate);
      break;
    case JS_ASYNC_FROM_SYNC_ITERATOR_TYPE:
      JSAsyncFromSyncIterator::cast(this)->JSAsyncFromSyncIteratorVerify(
          isolate);
      break;
    case JS_WEAK_MAP_TYPE:
      JSWeakMap::cast(this)->JSWeakMapVerify(isolate);
      break;
    case JS_WEAK_SET_TYPE:
      JSWeakSet::cast(this)->JSWeakSetVerify(isolate);
      break;
    case JS_PROMISE_TYPE:
      JSPromise::cast(this)->JSPromiseVerify(isolate);
      break;
    case JS_REGEXP_TYPE:
      JSRegExp::cast(this)->JSRegExpVerify(isolate);
      break;
    case JS_REGEXP_STRING_ITERATOR_TYPE:
      JSRegExpStringIterator::cast(this)->JSRegExpStringIteratorVerify(isolate);
      break;
    case FILLER_TYPE:
      break;
    case JS_PROXY_TYPE:
      JSProxy::cast(this)->JSProxyVerify(isolate);
      break;
    case FOREIGN_TYPE:
      Foreign::cast(this)->ForeignVerify(isolate);
      break;
    case PRE_PARSED_SCOPE_DATA_TYPE:
      PreParsedScopeData::cast(this)->PreParsedScopeDataVerify(isolate);
      break;
    case UNCOMPILED_DATA_WITHOUT_PRE_PARSED_SCOPE_TYPE:
      UncompiledDataWithoutPreParsedScope::cast(this)
          ->UncompiledDataWithoutPreParsedScopeVerify(isolate);
      break;
    case UNCOMPILED_DATA_WITH_PRE_PARSED_SCOPE_TYPE:
      UncompiledDataWithPreParsedScope::cast(this)
          ->UncompiledDataWithPreParsedScopeVerify(isolate);
      break;
    case SHARED_FUNCTION_INFO_TYPE:
      SharedFunctionInfo::cast(this)->SharedFunctionInfoVerify(isolate);
      break;
    case JS_MESSAGE_OBJECT_TYPE:
      JSMessageObject::cast(this)->JSMessageObjectVerify(isolate);
      break;
    case JS_ARRAY_BUFFER_TYPE:
      JSArrayBuffer::cast(this)->JSArrayBufferVerify(isolate);
      break;
    case JS_TYPED_ARRAY_TYPE:
      JSTypedArray::cast(this)->JSTypedArrayVerify(isolate);
      break;
    case JS_DATA_VIEW_TYPE:
      JSDataView::cast(this)->JSDataViewVerify(isolate);
      break;
    case SMALL_ORDERED_HASH_SET_TYPE:
      SmallOrderedHashSet::cast(this)->SmallOrderedHashTableVerify(isolate);
      break;
    case SMALL_ORDERED_HASH_MAP_TYPE:
      SmallOrderedHashMap::cast(this)->SmallOrderedHashTableVerify(isolate);
      break;
    case CODE_DATA_CONTAINER_TYPE:
      CodeDataContainer::cast(this)->CodeDataContainerVerify(isolate);
      break;
#ifdef V8_INTL_SUPPORT
    case JS_INTL_COLLATOR_TYPE:
      JSCollator::cast(this)->JSCollatorVerify(isolate);
      break;
    case JS_INTL_LIST_FORMAT_TYPE:
      JSListFormat::cast(this)->JSListFormatVerify(isolate);
      break;
    case JS_INTL_LOCALE_TYPE:
      JSLocale::cast(this)->JSLocaleVerify(isolate);
      break;
    case JS_INTL_PLURAL_RULES_TYPE:
      JSPluralRules::cast(this)->JSPluralRulesVerify(isolate);
      break;
    case JS_INTL_RELATIVE_TIME_FORMAT_TYPE:
      JSRelativeTimeFormat::cast(this)->JSRelativeTimeFormatVerify(isolate);
      break;
#endif  // V8_INTL_SUPPORT

#define MAKE_STRUCT_CASE(NAME, Name, name)   \
  case NAME##_TYPE:                          \
    Name::cast(this)->Name##Verify(isolate); \
    break;
      STRUCT_LIST(MAKE_STRUCT_CASE)
#undef MAKE_STRUCT_CASE

    case ALLOCATION_SITE_TYPE:
      AllocationSite::cast(this)->AllocationSiteVerify(isolate);
      break;

    case LOAD_HANDLER_TYPE:
      LoadHandler::cast(this)->LoadHandlerVerify(isolate);
      break;

    case STORE_HANDLER_TYPE:
      StoreHandler::cast(this)->StoreHandlerVerify(isolate);
      break;
  }
}

void HeapObject::VerifyHeapPointer(Isolate* isolate, Object* p) {
  CHECK(p->IsHeapObject());
  HeapObject* ho = HeapObject::cast(p);
  CHECK(isolate->heap()->Contains(ho));
}

void Symbol::SymbolVerify(Isolate* isolate) {
  CHECK(IsSymbol());
  CHECK(HasHashCode());
  CHECK_GT(Hash(), 0);
  CHECK(name()->IsUndefined(isolate) || name()->IsString());
  CHECK_IMPLIES(IsPrivateField(), IsPrivate());
}

void ByteArray::ByteArrayVerify(Isolate* isolate) { CHECK(IsByteArray()); }

void BytecodeArray::BytecodeArrayVerify(Isolate* isolate) {
  // TODO(oth): Walk bytecodes and immediate values to validate sanity.
  // - All bytecodes are known and well formed.
  // - Jumps must go to new instructions starts.
  // - No Illegal bytecodes.
  // - No consecutive sequences of prefix Wide / ExtraWide.
  CHECK(IsBytecodeArray());
  CHECK(constant_pool()->IsFixedArray());
  VerifyHeapPointer(isolate, constant_pool());
}

void FreeSpace::FreeSpaceVerify(Isolate* isolate) { CHECK(IsFreeSpace()); }

void FeedbackCell::FeedbackCellVerify(Isolate* isolate) {
  CHECK(IsFeedbackCell());

  VerifyHeapPointer(isolate, value());
  CHECK(value()->IsUndefined(isolate) || value()->IsFeedbackVector());
}

void FeedbackVector::FeedbackVectorVerify(Isolate* isolate) {
  CHECK(IsFeedbackVector());
  MaybeObject* code = optimized_code_weak_or_smi();
  MaybeObject::VerifyMaybeObjectPointer(isolate, code);
  CHECK(code->IsSmi() || code->IsClearedWeakHeapObject() ||
        code->IsWeakHeapObject());
}

template <class Traits>
void FixedTypedArray<Traits>::FixedTypedArrayVerify(Isolate* isolate) {
  CHECK(IsHeapObject() &&
        HeapObject::cast(this)->map()->instance_type() ==
            Traits::kInstanceType);
  if (base_pointer() == this) {
    CHECK(reinterpret_cast<Address>(external_pointer()) ==
          ExternalReference::fixed_typed_array_base_data_offset().address());
  } else {
    CHECK_NULL(base_pointer());
  }
}

bool JSObject::ElementsAreSafeToExamine() const {
  // If a GC was caused while constructing this object, the elements
  // pointer may point to a one pointer filler map.
  return reinterpret_cast<Map*>(elements()) !=
         GetReadOnlyRoots().one_pointer_filler_map();
}

namespace {
void VerifyJSObjectElements(Isolate* isolate, JSObject* object) {
  // Only TypedArrays can have these specialized elements.
  if (object->IsJSTypedArray()) {
    // TODO(cbruni): Fix CreateTypedArray to either not instantiate the object
    // or propertly initialize it on errors during construction.
    /* CHECK(object->HasFixedTypedArrayElements()); */
    /* CHECK(object->elements()->IsFixedTypedArrayBase()); */
    return;
  }
  CHECK(!object->HasFixedTypedArrayElements());
  CHECK(!object->elements()->IsFixedTypedArrayBase());

  if (object->HasDoubleElements()) {
    if (object->elements()->length() > 0) {
      CHECK(object->elements()->IsFixedDoubleArray());
    }
    return;
  }

  FixedArray* elements = FixedArray::cast(object->elements());
  if (object->HasSmiElements()) {
    // We might have a partially initialized backing store, in which case we
    // allow the hole + smi values.
    for (int i = 0; i < elements->length(); i++) {
      Object* value = elements->get(i);
      CHECK(value->IsSmi() || value->IsTheHole(isolate));
    }
  } else if (object->HasObjectElements()) {
    for (int i = 0; i < elements->length(); i++) {
      Object* element = elements->get(i);
      CHECK_IMPLIES(!element->IsSmi(), !HasWeakHeapObjectTag(element));
    }
  }
}
}  // namespace

void JSObject::JSObjectVerify(Isolate* isolate) {
  VerifyPointer(isolate, raw_properties_or_hash());
  VerifyHeapPointer(isolate, elements());

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
      CHECK_IMPLIES(indices != ReadOnlyRoots(isolate).empty_fixed_array(),
                    keys->length() == indices->length());
    }
  }

  // If a GC was caused while constructing this object, the elements
  // pointer may point to a one pointer filler map.
  if (ElementsAreSafeToExamine()) {
    CHECK_EQ((map()->has_fast_smi_or_object_elements() ||
              (elements() == GetReadOnlyRoots().empty_fixed_array()) ||
              HasFastStringWrapperElements()),
             (elements()->map() == GetReadOnlyRoots().fixed_array_map() ||
              elements()->map() == GetReadOnlyRoots().fixed_cow_array_map()));
    CHECK_EQ(map()->has_fast_object_elements(), HasObjectElements());
    VerifyJSObjectElements(isolate, this);
  }
}

void Map::MapVerify(Isolate* isolate) {
  Heap* heap = isolate->heap();
  CHECK(!Heap::InNewSpace(this));
  CHECK(FIRST_TYPE <= instance_type() && instance_type() <= LAST_TYPE);
  CHECK(instance_size() == kVariableSizeSentinel ||
        (kPointerSize <= instance_size() &&
         static_cast<size_t>(instance_size()) < heap->Capacity()));
  CHECK(GetBackPointer()->IsUndefined(heap->isolate()) ||
        !Map::cast(GetBackPointer())->is_stable());
  VerifyHeapPointer(isolate, prototype());
  VerifyHeapPointer(isolate, instance_descriptors());
  SLOW_DCHECK(instance_descriptors()->IsSortedNoDuplicates());
  DisallowHeapAllocation no_gc;
  SLOW_DCHECK(
      TransitionsAccessor(isolate, this, &no_gc).IsSortedNoDuplicates());
  SLOW_DCHECK(TransitionsAccessor(isolate, this, &no_gc)
                  .IsConsistentWithBackPointers());
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
  if (is_prototype_map()) {
    DCHECK(prototype_info() == Smi::kZero ||
           prototype_info()->IsPrototypeInfo());
  }
  CHECK(prototype_validity_cell()->IsSmi() ||
        prototype_validity_cell()->IsCell());
}

void Map::DictionaryMapVerify(Isolate* isolate) {
  MapVerify(isolate);
  CHECK(is_dictionary_map());
  CHECK_EQ(kInvalidEnumCacheSentinel, EnumLength());
  CHECK_EQ(ReadOnlyRoots(isolate).empty_descriptor_array(),
           instance_descriptors());
  CHECK_EQ(0, UnusedPropertyFields());
  CHECK_EQ(Map::GetVisitorId(this), visitor_id());
}

void AliasedArgumentsEntry::AliasedArgumentsEntryVerify(Isolate* isolate) {
  VerifySmiField(kAliasedContextSlot);
}

void FixedArray::FixedArrayVerify(Isolate* isolate) {
  for (int i = 0; i < length(); i++) {
    Object* e = get(i);
    VerifyPointer(isolate, e);
  }
}

void WeakFixedArray::WeakFixedArrayVerify(Isolate* isolate) {
  for (int i = 0; i < length(); i++) {
    MaybeObject::VerifyMaybeObjectPointer(isolate, Get(i));
  }
}

void WeakArrayList::WeakArrayListVerify(Isolate* isolate) {
  for (int i = 0; i < length(); i++) {
    MaybeObject::VerifyMaybeObjectPointer(isolate, Get(i));
  }
}

void PropertyArray::PropertyArrayVerify(Isolate* isolate) {
  if (length() == 0) {
    CHECK_EQ(this, ReadOnlyRoots(isolate).empty_property_array());
    return;
  }
  // There are no empty PropertyArrays.
  CHECK_LT(0, length());
  for (int i = 0; i < length(); i++) {
    Object* e = get(i);
    VerifyPointer(isolate, e);
  }
}

void FixedDoubleArray::FixedDoubleArrayVerify(Isolate* isolate) {
  for (int i = 0; i < length(); i++) {
    if (!is_the_hole(i)) {
      uint64_t value = get_representation(i);
      uint64_t unexpected =
          bit_cast<uint64_t>(std::numeric_limits<double>::quiet_NaN()) &
          uint64_t{0x7FF8000000000000};
      // Create implementation specific sNaN by inverting relevant bit.
      unexpected ^= uint64_t{0x0008000000000000};
      CHECK((value & uint64_t{0x7FF8000000000000}) != unexpected ||
            (value & uint64_t{0x0007FFFFFFFFFFFF}) == uint64_t{0});
    }
  }
}

void FeedbackMetadata::FeedbackMetadataVerify(Isolate* isolate) {
  if (slot_count() == 0) {
    CHECK_EQ(ReadOnlyRoots(isolate).empty_feedback_metadata(), this);
  } else {
    FeedbackMetadataIterator iter(this);
    while (iter.HasNext()) {
      iter.Next();
      FeedbackSlotKind kind = iter.kind();
      CHECK_NE(FeedbackSlotKind::kInvalid, kind);
      CHECK_GT(FeedbackSlotKind::kKindsNumber, kind);
    }
  }
}

void DescriptorArray::DescriptorArrayVerify(Isolate* isolate) {
  WeakFixedArrayVerify(isolate);
  int nof_descriptors = number_of_descriptors();
  if (number_of_descriptors_storage() == 0) {
    Heap* heap = isolate->heap();
    CHECK_EQ(ReadOnlyRoots(heap).empty_descriptor_array(), this);
    CHECK_EQ(2, length());
    CHECK_EQ(0, nof_descriptors);
    CHECK_EQ(ReadOnlyRoots(heap).empty_enum_cache(), GetEnumCache());
  } else {
    CHECK_LT(2, length());
    CHECK_LE(LengthFor(nof_descriptors), length());

    // Check that properties with private symbols names are non-enumerable.
    for (int descriptor = 0; descriptor < nof_descriptors; descriptor++) {
      Object* key = get(ToKeyIndex(descriptor))->ToObject();
      // number_of_descriptors() may be out of sync with the actual descriptors
      // written during descriptor array construction.
      if (key->IsUndefined(isolate)) continue;
      PropertyDetails details = GetDetails(descriptor);
      if (Name::cast(key)->IsPrivate()) {
        CHECK_NE(details.attributes() & DONT_ENUM, 0);
      }
      MaybeObject* value = get(ToValueIndex(descriptor));
      HeapObject* heap_object;
      if (details.location() == kField) {
        CHECK(value == MaybeObject::FromObject(FieldType::None()) ||
              value == MaybeObject::FromObject(FieldType::Any()) ||
              value->IsClearedWeakHeapObject() ||
              (value->ToWeakHeapObject(&heap_object) && heap_object->IsMap()));
      } else {
        CHECK(!value->IsWeakOrClearedHeapObject());
        CHECK(!value->ToObject()->IsMap());
      }
    }
  }
}

void TransitionArray::TransitionArrayVerify(Isolate* isolate) {
  WeakFixedArrayVerify(isolate);
  CHECK_LE(LengthFor(number_of_transitions()), length());
}

void JSArgumentsObject::JSArgumentsObjectVerify(Isolate* isolate) {
  if (IsSloppyArgumentsElementsKind(GetElementsKind())) {
    SloppyArgumentsElements::cast(elements())
        ->SloppyArgumentsElementsVerify(isolate, this);
  }
  if (isolate->IsInAnyContext(map(), Context::SLOPPY_ARGUMENTS_MAP_INDEX) ||
      isolate->IsInAnyContext(map(),
                              Context::SLOW_ALIASED_ARGUMENTS_MAP_INDEX) ||
      isolate->IsInAnyContext(map(),
                              Context::FAST_ALIASED_ARGUMENTS_MAP_INDEX)) {
    VerifyObjectField(isolate, JSSloppyArgumentsObject::kLengthOffset);
    VerifyObjectField(isolate, JSSloppyArgumentsObject::kCalleeOffset);
  } else if (isolate->IsInAnyContext(map(),
                                     Context::STRICT_ARGUMENTS_MAP_INDEX)) {
    VerifyObjectField(isolate, JSStrictArgumentsObject::kLengthOffset);
  }
  JSObjectVerify(isolate);
}

void SloppyArgumentsElements::SloppyArgumentsElementsVerify(Isolate* isolate,
                                                            JSObject* holder) {
  FixedArrayVerify(isolate);
  // Abort verification if only partially initialized (can't use arguments()
  // getter because it does FixedArray::cast()).
  if (get(kArgumentsIndex)->IsUndefined(isolate)) return;

  ElementsKind kind = holder->GetElementsKind();
  bool is_fast = kind == FAST_SLOPPY_ARGUMENTS_ELEMENTS;
  CHECK(IsFixedArray());
  CHECK_GE(length(), 2);
  CHECK_EQ(map(), ReadOnlyRoots(isolate).sloppy_arguments_elements_map());
  Context* context_object = Context::cast(context());
  FixedArray* arg_elements = FixedArray::cast(arguments());
  if (arg_elements->length() == 0) {
    CHECK(arg_elements == ReadOnlyRoots(isolate).empty_fixed_array());
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

void JSGeneratorObject::JSGeneratorObjectVerify(Isolate* isolate) {
  // In an expression like "new g()", there can be a point where a generator
  // object is allocated but its fields are all undefined, as it hasn't yet been
  // initialized by the generator.  Hence these weak checks.
  VerifyObjectField(isolate, kFunctionOffset);
  VerifyObjectField(isolate, kContextOffset);
  VerifyObjectField(isolate, kReceiverOffset);
  VerifyObjectField(isolate, kParametersAndRegistersOffset);
  VerifyObjectField(isolate, kContinuationOffset);
}

void JSAsyncGeneratorObject::JSAsyncGeneratorObjectVerify(Isolate* isolate) {
  // Check inherited fields
  JSGeneratorObjectVerify(isolate);
  VerifyObjectField(isolate, kQueueOffset);
  queue()->HeapObjectVerify(isolate);
}

void JSValue::JSValueVerify(Isolate* isolate) {
  Object* v = value();
  if (v->IsHeapObject()) {
    VerifyHeapPointer(isolate, v);
  }
}

void JSDate::JSDateVerify(Isolate* isolate) {
  if (value()->IsHeapObject()) {
    VerifyHeapPointer(isolate, value());
  }
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

void JSMessageObject::JSMessageObjectVerify(Isolate* isolate) {
  CHECK(IsJSMessageObject());
  VerifyObjectField(isolate, kStartPositionOffset);
  VerifyObjectField(isolate, kEndPositionOffset);
  VerifyObjectField(isolate, kArgumentsOffset);
  VerifyObjectField(isolate, kScriptOffset);
  VerifyObjectField(isolate, kStackFramesOffset);
}

void String::StringVerify(Isolate* isolate) {
  CHECK(IsString());
  CHECK(length() >= 0 && length() <= Smi::kMaxValue);
  CHECK_IMPLIES(length() == 0, this == ReadOnlyRoots(isolate).empty_string());
  if (IsInternalizedString()) {
    CHECK(!Heap::InNewSpace(this));
  }
  if (IsConsString()) {
    ConsString::cast(this)->ConsStringVerify(isolate);
  } else if (IsSlicedString()) {
    SlicedString::cast(this)->SlicedStringVerify(isolate);
  } else if (IsThinString()) {
    ThinString::cast(this)->ThinStringVerify(isolate);
  }
}

void ConsString::ConsStringVerify(Isolate* isolate) {
  CHECK(this->first()->IsString());
  CHECK(this->second() == ReadOnlyRoots(isolate).empty_string() ||
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

void ThinString::ThinStringVerify(Isolate* isolate) {
  CHECK(this->actual()->IsInternalizedString());
  CHECK(this->actual()->IsSeqString() || this->actual()->IsExternalString());
}

void SlicedString::SlicedStringVerify(Isolate* isolate) {
  CHECK(!this->parent()->IsConsString());
  CHECK(!this->parent()->IsSlicedString());
  CHECK_GE(this->length(), SlicedString::kMinLength);
}

void JSBoundFunction::JSBoundFunctionVerify(Isolate* isolate) {
  CHECK(IsJSBoundFunction());
  JSObjectVerify(isolate);
  VerifyObjectField(isolate, kBoundThisOffset);
  VerifyObjectField(isolate, kBoundTargetFunctionOffset);
  VerifyObjectField(isolate, kBoundArgumentsOffset);
  CHECK(IsCallable());

  if (!raw_bound_target_function()->IsUndefined(isolate)) {
    CHECK(bound_target_function()->IsCallable());
    CHECK_EQ(IsConstructor(), bound_target_function()->IsConstructor());
  }
}

void JSFunction::JSFunctionVerify(Isolate* isolate) {
  CHECK(IsJSFunction());
  JSObjectVerify(isolate);
  VerifyHeapPointer(isolate, feedback_cell());
  CHECK(feedback_cell()->IsFeedbackCell());
  CHECK(code()->IsCode());
  CHECK(map()->is_callable());
  Handle<JSFunction> function(this, isolate);
  LookupIterator it(isolate, function, isolate->factory()->prototype_string(),
                    LookupIterator::OWN_SKIP_INTERCEPTOR);
  if (has_prototype_slot()) {
    VerifyObjectField(isolate, kPrototypeOrInitialMapOffset);
  }

  if (has_prototype_property()) {
    CHECK(it.IsFound());
    CHECK_EQ(LookupIterator::ACCESSOR, it.state());
    CHECK(it.GetAccessors()->IsAccessorInfo());
  } else {
    CHECK(!it.IsFound() || it.state() != LookupIterator::ACCESSOR ||
          !it.GetAccessors()->IsAccessorInfo());
  }
}

void SharedFunctionInfo::SharedFunctionInfoVerify(Isolate* isolate) {
  CHECK(IsSharedFunctionInfo());

  VerifyObjectField(isolate, kFunctionDataOffset);
  VerifyObjectField(isolate, kOuterScopeInfoOrFeedbackMetadataOffset);
  VerifyObjectField(isolate, kScriptOrDebugInfoOffset);
  VerifyObjectField(isolate, kNameOrScopeInfoOffset);

  Object* value = name_or_scope_info();
  CHECK(value == kNoSharedNameSentinel || value->IsString() ||
        value->IsScopeInfo());
  if (value->IsScopeInfo()) {
    CHECK_LT(0, ScopeInfo::cast(value)->length());
    CHECK_NE(value, ReadOnlyRoots(isolate).empty_scope_info());
  }

  CHECK(HasWasmExportedFunctionData() || IsApiFunction() ||
        HasBytecodeArray() || HasAsmWasmData() || HasBuiltinId() ||
        HasUncompiledDataWithPreParsedScope() ||
        HasUncompiledDataWithoutPreParsedScope());

  CHECK(script_or_debug_info()->IsUndefined(isolate) ||
        script_or_debug_info()->IsScript() || HasDebugInfo());

  if (!is_compiled()) {
    CHECK(!HasFeedbackMetadata());
    CHECK(outer_scope_info()->IsScopeInfo() ||
          outer_scope_info()->IsTheHole(isolate));
  } else if (HasBytecodeArray()) {
    CHECK(HasFeedbackMetadata());
    CHECK(feedback_metadata()->IsFeedbackMetadata());
  }

  int expected_map_index = Context::FunctionMapIndex(
      language_mode(), kind(), true, HasSharedName(), needs_home_object());
  CHECK_EQ(expected_map_index, function_map_index());

  if (scope_info()->length() > 0) {
    ScopeInfo* info = scope_info();
    CHECK(kind() == info->function_kind());
    CHECK_EQ(kind() == kModule, info->scope_type() == MODULE_SCOPE);
  }

  if (IsApiFunction()) {
    CHECK(construct_as_builtin());
  } else if (!HasBuiltinId()) {
    CHECK(!construct_as_builtin());
  } else {
    int id = builtin_id();
    if (id != Builtins::kCompileLazy && id != Builtins::kEmptyFunction) {
      CHECK(construct_as_builtin());
    } else {
      CHECK(!construct_as_builtin());
    }
  }
}

void JSGlobalProxy::JSGlobalProxyVerify(Isolate* isolate) {
  CHECK(IsJSGlobalProxy());
  JSObjectVerify(isolate);
  VerifyObjectField(isolate, JSGlobalProxy::kNativeContextOffset);
  CHECK(map()->is_access_check_needed());
  // Make sure that this object has no properties, elements.
  CHECK_EQ(0, FixedArray::cast(elements())->length());
}

void JSGlobalObject::JSGlobalObjectVerify(Isolate* isolate) {
  CHECK(IsJSGlobalObject());
  // Do not check the dummy global object for the builtins.
  if (global_dictionary()->NumberOfElements() == 0 &&
      elements()->length() == 0) {
    return;
  }
  JSObjectVerify(isolate);
}

void Oddball::OddballVerify(Isolate* isolate) {
  CHECK(IsOddball());
  Heap* heap = isolate->heap();
  VerifyHeapPointer(isolate, to_string());
  Object* number = to_number();
  if (number->IsHeapObject()) {
    CHECK(number == ReadOnlyRoots(heap).nan_value() ||
          number == ReadOnlyRoots(heap).hole_nan_value());
  } else {
    CHECK(number->IsSmi());
    int value = Smi::ToInt(number);
    // Hidden oddballs have negative smis.
    const int kLeastHiddenOddballNumber = -7;
    CHECK_LE(value, 1);
    CHECK_GE(value, kLeastHiddenOddballNumber);
  }

  ReadOnlyRoots roots(heap);
  if (map() == roots.undefined_map()) {
    CHECK(this == roots.undefined_value());
  } else if (map() == roots.the_hole_map()) {
    CHECK(this == roots.the_hole_value());
  } else if (map() == roots.null_map()) {
    CHECK(this == roots.null_value());
  } else if (map() == roots.boolean_map()) {
    CHECK(this == roots.true_value() || this == roots.false_value());
  } else if (map() == roots.uninitialized_map()) {
    CHECK(this == roots.uninitialized_value());
  } else if (map() == roots.arguments_marker_map()) {
    CHECK(this == roots.arguments_marker());
  } else if (map() == roots.termination_exception_map()) {
    CHECK(this == roots.termination_exception());
  } else if (map() == roots.exception_map()) {
    CHECK(this == roots.exception());
  } else if (map() == roots.optimized_out_map()) {
    CHECK(this == roots.optimized_out());
  } else if (map() == roots.stale_register_map()) {
    CHECK(this == roots.stale_register());
  } else if (map() == roots.self_reference_marker_map()) {
    // Multiple instances of this oddball may exist at once.
    CHECK_EQ(kind(), Oddball::kSelfReferenceMarker);
  } else {
    UNREACHABLE();
  }
}

void Cell::CellVerify(Isolate* isolate) {
  CHECK(IsCell());
  VerifyObjectField(isolate, kValueOffset);
}

void PropertyCell::PropertyCellVerify(Isolate* isolate) {
  CHECK(IsPropertyCell());
  VerifyObjectField(isolate, kValueOffset);
}

void CodeDataContainer::CodeDataContainerVerify(Isolate* isolate) {
  CHECK(IsCodeDataContainer());
  VerifyObjectField(isolate, kNextCodeLinkOffset);
  CHECK(next_code_link()->IsCode() || next_code_link()->IsUndefined(isolate));
}

void Code::CodeVerify(Isolate* isolate) {
  CHECK_LE(constant_pool_offset(), InstructionSize());
  CHECK(IsAligned(InstructionStart(), kCodeAlignment));
  relocation_info()->ObjectVerify(isolate);
  Address last_gc_pc = kNullAddress;

  for (RelocIterator it(this); !it.done(); it.next()) {
    it.rinfo()->Verify(isolate);
    // Ensure that GC will not iterate twice over the same pointer.
    if (RelocInfo::IsGCRelocMode(it.rinfo()->rmode())) {
      CHECK(it.rinfo()->pc() != last_gc_pc);
      last_gc_pc = it.rinfo()->pc();
    }
  }
}

void JSArray::JSArrayVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
  CHECK(length()->IsNumber() || length()->IsUndefined(isolate));
  // If a GC was caused while constructing this array, the elements
  // pointer may point to a one pointer filler map.
  if (!ElementsAreSafeToExamine()) return;
  if (elements()->IsUndefined(isolate)) return;
  CHECK(elements()->IsFixedArray() || elements()->IsFixedDoubleArray());
  if (elements()->length() == 0) {
    CHECK_EQ(elements(), ReadOnlyRoots(isolate).empty_fixed_array());
  }
  if (!length()->IsNumber()) return;
  // Verify that the length and the elements backing store are in sync.
  if (length()->IsSmi() && HasFastElements()) {
    if (elements()->length() > 0) {
      CHECK_IMPLIES(HasDoubleElements(), elements()->IsFixedDoubleArray());
    }
    int size = Smi::ToInt(length());
    // Holey / Packed backing stores might have slack or might have not been
    // properly initialized yet.
    CHECK(size <= elements()->length() ||
          elements() == ReadOnlyRoots(isolate).empty_fixed_array());
  } else {
    CHECK(HasDictionaryElements());
    uint32_t array_length;
    CHECK(length()->ToArrayLength(&array_length));
    if (array_length == 0xFFFFFFFF) {
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

void JSSet::JSSetVerify(Isolate* isolate) {
  CHECK(IsJSSet());
  JSObjectVerify(isolate);
  VerifyHeapPointer(isolate, table());
  CHECK(table()->IsOrderedHashSet() || table()->IsUndefined(isolate));
  // TODO(arv): Verify OrderedHashTable too.
}

void JSMap::JSMapVerify(Isolate* isolate) {
  CHECK(IsJSMap());
  JSObjectVerify(isolate);
  VerifyHeapPointer(isolate, table());
  CHECK(table()->IsOrderedHashMap() || table()->IsUndefined(isolate));
  // TODO(arv): Verify OrderedHashTable too.
}

void JSSetIterator::JSSetIteratorVerify(Isolate* isolate) {
  CHECK(IsJSSetIterator());
  JSObjectVerify(isolate);
  VerifyHeapPointer(isolate, table());
  CHECK(table()->IsOrderedHashSet());
  CHECK(index()->IsSmi());
}

void JSMapIterator::JSMapIteratorVerify(Isolate* isolate) {
  CHECK(IsJSMapIterator());
  JSObjectVerify(isolate);
  VerifyHeapPointer(isolate, table());
  CHECK(table()->IsOrderedHashMap());
  CHECK(index()->IsSmi());
}

void JSWeakMap::JSWeakMapVerify(Isolate* isolate) {
  CHECK(IsJSWeakMap());
  JSObjectVerify(isolate);
  VerifyHeapPointer(isolate, table());
  CHECK(table()->IsEphemeronHashTable() || table()->IsUndefined(isolate));
}

void JSArrayIterator::JSArrayIteratorVerify(Isolate* isolate) {
  CHECK(IsJSArrayIterator());
  JSObjectVerify(isolate);
  CHECK(iterated_object()->IsJSReceiver());

  CHECK_GE(next_index()->Number(), 0);
  CHECK_LE(next_index()->Number(), kMaxSafeInteger);

  if (iterated_object()->IsJSTypedArray()) {
    // JSTypedArray::length is limited to Smi range.
    CHECK(next_index()->IsSmi());
    CHECK_LE(next_index()->Number(), Smi::kMaxValue);
  } else if (iterated_object()->IsJSArray()) {
    // JSArray::length is limited to Uint32 range.
    CHECK_LE(next_index()->Number(), kMaxUInt32);
  }
}

void JSStringIterator::JSStringIteratorVerify(Isolate* isolate) {
  CHECK(IsJSStringIterator());
  JSObjectVerify(isolate);
  CHECK(string()->IsString());

  CHECK_GE(index(), 0);
  CHECK_LE(index(), String::kMaxLength);
}

void JSAsyncFromSyncIterator::JSAsyncFromSyncIteratorVerify(Isolate* isolate) {
  CHECK(IsJSAsyncFromSyncIterator());
  JSObjectVerify(isolate);
  VerifyHeapPointer(isolate, sync_iterator());
}

void JSWeakSet::JSWeakSetVerify(Isolate* isolate) {
  CHECK(IsJSWeakSet());
  JSObjectVerify(isolate);
  VerifyHeapPointer(isolate, table());
  CHECK(table()->IsEphemeronHashTable() || table()->IsUndefined(isolate));
}

void Microtask::MicrotaskVerify(Isolate* isolate) { CHECK(IsMicrotask()); }

void CallableTask::CallableTaskVerify(Isolate* isolate) {
  CHECK(IsCallableTask());
  MicrotaskVerify(isolate);
  VerifyHeapPointer(isolate, callable());
  CHECK(callable()->IsCallable());
  VerifyHeapPointer(isolate, context());
  CHECK(context()->IsContext());
}

void CallbackTask::CallbackTaskVerify(Isolate* isolate) {
  CHECK(IsCallbackTask());
  MicrotaskVerify(isolate);
  VerifyHeapPointer(isolate, callback());
  VerifyHeapPointer(isolate, data());
}

void PromiseReactionJobTask::PromiseReactionJobTaskVerify(Isolate* isolate) {
  CHECK(IsPromiseReactionJobTask());
  MicrotaskVerify(isolate);
  VerifyPointer(isolate, argument());
  VerifyHeapPointer(isolate, context());
  CHECK(context()->IsContext());
  VerifyHeapPointer(isolate, handler());
  CHECK(handler()->IsUndefined(isolate) || handler()->IsCallable());
  VerifyHeapPointer(isolate, promise_or_capability());
  CHECK(promise_or_capability()->IsJSPromise() ||
        promise_or_capability()->IsPromiseCapability());
}

void PromiseFulfillReactionJobTask::PromiseFulfillReactionJobTaskVerify(
    Isolate* isolate) {
  CHECK(IsPromiseFulfillReactionJobTask());
  PromiseReactionJobTaskVerify(isolate);
}

void PromiseRejectReactionJobTask::PromiseRejectReactionJobTaskVerify(
    Isolate* isolate) {
  CHECK(IsPromiseRejectReactionJobTask());
  PromiseReactionJobTaskVerify(isolate);
}

void PromiseResolveThenableJobTask::PromiseResolveThenableJobTaskVerify(
    Isolate* isolate) {
  CHECK(IsPromiseResolveThenableJobTask());
  MicrotaskVerify(isolate);
  VerifyHeapPointer(isolate, context());
  CHECK(context()->IsContext());
  VerifyHeapPointer(isolate, promise_to_resolve());
  CHECK(promise_to_resolve()->IsJSPromise());
  VerifyHeapPointer(isolate, then());
  CHECK(then()->IsCallable());
  CHECK(then()->IsJSReceiver());
  VerifyHeapPointer(isolate, thenable());
  CHECK(thenable()->IsJSReceiver());
}

void PromiseCapability::PromiseCapabilityVerify(Isolate* isolate) {
  CHECK(IsPromiseCapability());

  VerifyHeapPointer(isolate, promise());
  CHECK(promise()->IsJSReceiver() || promise()->IsUndefined(isolate));
  VerifyPointer(isolate, resolve());
  VerifyPointer(isolate, reject());
}

void PromiseReaction::PromiseReactionVerify(Isolate* isolate) {
  CHECK(IsPromiseReaction());

  VerifyPointer(isolate, next());
  CHECK(next()->IsSmi() || next()->IsPromiseReaction());
  VerifyHeapPointer(isolate, reject_handler());
  CHECK(reject_handler()->IsUndefined(isolate) ||
        reject_handler()->IsCallable());
  VerifyHeapPointer(isolate, fulfill_handler());
  CHECK(fulfill_handler()->IsUndefined(isolate) ||
        fulfill_handler()->IsCallable());
  VerifyHeapPointer(isolate, promise_or_capability());
  CHECK(promise_or_capability()->IsJSPromise() ||
        promise_or_capability()->IsPromiseCapability());
}

void JSPromise::JSPromiseVerify(Isolate* isolate) {
  CHECK(IsJSPromise());
  JSObjectVerify(isolate);
  VerifyPointer(isolate, reactions_or_result());
  VerifySmiField(kFlagsOffset);
  if (status() == Promise::kPending) {
    CHECK(reactions()->IsSmi() || reactions()->IsPromiseReaction());
  }
}

template <typename Derived>
void SmallOrderedHashTable<Derived>::SmallOrderedHashTableVerify(
    Isolate* isolate) {
  CHECK(IsSmallOrderedHashTable());

  int capacity = Capacity();
  CHECK_GE(capacity, kMinCapacity);
  CHECK_LE(capacity, kMaxCapacity);

  for (int entry = 0; entry < NumberOfBuckets(); entry++) {
    int bucket = GetFirstEntry(entry);
    if (bucket == kNotFound) continue;
    CHECK_GE(bucket, 0);
    CHECK_LE(bucket, capacity);
  }

  for (int entry = 0; entry < NumberOfElements(); entry++) {
    int chain = GetNextEntry(entry);
    if (chain == kNotFound) continue;
    CHECK_GE(chain, 0);
    CHECK_LE(chain, capacity);
  }

  for (int entry = 0; entry < NumberOfElements(); entry++) {
    for (int offset = 0; offset < Derived::kEntrySize; offset++) {
      Object* val = GetDataEntry(entry, offset);
      VerifyPointer(isolate, val);
    }
  }

  for (int entry = NumberOfElements(); entry < NumberOfDeletedElements();
       entry++) {
    for (int offset = 0; offset < Derived::kEntrySize; offset++) {
      Object* val = GetDataEntry(entry, offset);
      CHECK(val->IsTheHole(isolate));
    }
  }

  for (int entry = NumberOfElements() + NumberOfDeletedElements();
       entry < Capacity(); entry++) {
    for (int offset = 0; offset < Derived::kEntrySize; offset++) {
      Object* val = GetDataEntry(entry, offset);
      CHECK(val->IsTheHole(isolate));
    }
  }
}

template void SmallOrderedHashTable<
    SmallOrderedHashMap>::SmallOrderedHashTableVerify(Isolate* isolate);
template void SmallOrderedHashTable<
    SmallOrderedHashSet>::SmallOrderedHashTableVerify(Isolate* isolate);

void JSRegExp::JSRegExpVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
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

void JSRegExpStringIterator::JSRegExpStringIteratorVerify(Isolate* isolate) {
  CHECK(IsJSRegExpStringIterator());
  JSObjectVerify(isolate);
  CHECK(iterating_string()->IsString());
  CHECK(iterating_regexp()->IsObject());
  VerifySmiField(kFlagsOffset);
}

void JSProxy::JSProxyVerify(Isolate* isolate) {
  CHECK(IsJSProxy());
  CHECK(map()->GetConstructor()->IsJSFunction());
  VerifyPointer(isolate, target());
  VerifyPointer(isolate, handler());
  if (!IsRevoked()) {
    CHECK_EQ(target()->IsCallable(), map()->is_callable());
    CHECK_EQ(target()->IsConstructor(), map()->is_constructor());
  }
  CHECK(map()->prototype()->IsNull(isolate));
  // There should be no properties on a Proxy.
  CHECK_EQ(0, map()->NumberOfOwnDescriptors());
}

void JSArrayBuffer::JSArrayBufferVerify(Isolate* isolate) {
  CHECK(IsJSArrayBuffer());
  JSObjectVerify(isolate);
  VerifyPointer(isolate, byte_length());
  CHECK(byte_length()->IsSmi() || byte_length()->IsHeapNumber() ||
        byte_length()->IsUndefined(isolate));
}

void JSArrayBufferView::JSArrayBufferViewVerify(Isolate* isolate) {
  CHECK(IsJSArrayBufferView());
  JSObjectVerify(isolate);
  VerifyPointer(isolate, buffer());
  CHECK(buffer()->IsJSArrayBuffer() || buffer()->IsUndefined(isolate) ||
        buffer() == Smi::kZero);

  VerifyPointer(isolate, raw_byte_offset());
  CHECK(raw_byte_offset()->IsSmi() || raw_byte_offset()->IsHeapNumber() ||
        raw_byte_offset()->IsUndefined(isolate));

  VerifyPointer(isolate, raw_byte_length());
  CHECK(raw_byte_length()->IsSmi() || raw_byte_length()->IsHeapNumber() ||
        raw_byte_length()->IsUndefined(isolate));
}

void JSTypedArray::JSTypedArrayVerify(Isolate* isolate) {
  CHECK(IsJSTypedArray());
  JSArrayBufferViewVerify(isolate);
  VerifyPointer(isolate, raw_length());
  CHECK(raw_length()->IsSmi() || raw_length()->IsUndefined(isolate));
  VerifyPointer(isolate, elements());
}

void JSDataView::JSDataViewVerify(Isolate* isolate) {
  CHECK(IsJSDataView());
  JSArrayBufferViewVerify(isolate);
}

void Foreign::ForeignVerify(Isolate* isolate) { CHECK(IsForeign()); }

void AsyncGeneratorRequest::AsyncGeneratorRequestVerify(Isolate* isolate) {
  CHECK(IsAsyncGeneratorRequest());
  VerifySmiField(kResumeModeOffset);
  CHECK_GE(resume_mode(), JSGeneratorObject::kNext);
  CHECK_LE(resume_mode(), JSGeneratorObject::kThrow);
  CHECK(promise()->IsJSPromise());
  VerifyPointer(isolate, value());
  VerifyPointer(isolate, next());
  next()->ObjectVerify(isolate);
}

void BigInt::BigIntVerify(Isolate* isolate) {
  CHECK(IsBigInt());
  CHECK_GE(length(), 0);
  CHECK_IMPLIES(is_zero(), !sign());  // There is no -0n.
}

void JSModuleNamespace::JSModuleNamespaceVerify(Isolate* isolate) {
  CHECK(IsJSModuleNamespace());
  VerifyPointer(isolate, module());
}

void ModuleInfoEntry::ModuleInfoEntryVerify(Isolate* isolate) {
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

void Module::ModuleVerify(Isolate* isolate) {
  CHECK(IsModule());

  VerifyPointer(isolate, code());
  VerifyPointer(isolate, exports());
  VerifyPointer(isolate, module_namespace());
  VerifyPointer(isolate, requested_modules());
  VerifyPointer(isolate, script());
  VerifyPointer(isolate, import_meta());
  VerifyPointer(isolate, exception());
  VerifySmiField(kHashOffset);
  VerifySmiField(kStatusOffset);

  CHECK((status() >= kEvaluating && code()->IsModuleInfo()) ||
        (status() == kInstantiated && code()->IsJSGeneratorObject()) ||
        (status() == kInstantiating && code()->IsJSFunction()) ||
        (code()->IsSharedFunctionInfo()));

  CHECK_EQ(status() == kErrored, !exception()->IsTheHole(isolate));

  CHECK(module_namespace()->IsUndefined(isolate) ||
        module_namespace()->IsJSModuleNamespace());
  if (module_namespace()->IsJSModuleNamespace()) {
    CHECK_LE(kInstantiating, status());
    CHECK_EQ(JSModuleNamespace::cast(module_namespace())->module(), this);
  }

  CHECK_EQ(requested_modules()->length(), info()->module_requests()->length());

  CHECK(import_meta()->IsTheHole(isolate) || import_meta()->IsJSObject());

  CHECK_NE(hash(), 0);
}

void PrototypeInfo::PrototypeInfoVerify(Isolate* isolate) {
  CHECK(IsPrototypeInfo());
  Object* module_ns = module_namespace();
  CHECK(module_ns->IsJSModuleNamespace() || module_ns->IsUndefined(isolate));
  if (prototype_users()->IsWeakArrayList()) {
    PrototypeUsers::Verify(WeakArrayList::cast(prototype_users()));
  } else {
    CHECK(prototype_users()->IsSmi());
  }
}

void PrototypeUsers::Verify(WeakArrayList* array) {
  if (array->length() == 0) {
    // Allow empty & uninitialized lists.
    return;
  }
  // Verify empty slot chain.
  int empty_slot = Smi::ToInt(empty_slot_index(array));
  int empty_slots_count = 0;
  while (empty_slot != kNoEmptySlotsMarker) {
    CHECK_GT(empty_slot, 0);
    CHECK_LT(empty_slot, array->length());
    empty_slot = Smi::ToInt(array->Get(empty_slot)->ToSmi());
    ++empty_slots_count;
  }

  // Verify that all elements are either weak pointers or SMIs marking empty
  // slots.
  int weak_maps_count = 0;
  for (int i = kFirstIndex; i < array->length(); ++i) {
    HeapObject* heap_object;
    MaybeObject* object = array->Get(i);
    if ((object->ToWeakHeapObject(&heap_object) && heap_object->IsMap()) ||
        object->IsClearedWeakHeapObject()) {
      ++weak_maps_count;
    } else {
      CHECK(object->IsSmi());
    }
  }

  CHECK_EQ(weak_maps_count + empty_slots_count + 1, array->length());
}

void Tuple2::Tuple2Verify(Isolate* isolate) {
  CHECK(IsTuple2());
  Heap* heap = isolate->heap();
  if (this == ReadOnlyRoots(heap).empty_enum_cache()) {
    CHECK_EQ(ReadOnlyRoots(heap).empty_fixed_array(),
             EnumCache::cast(this)->keys());
    CHECK_EQ(ReadOnlyRoots(heap).empty_fixed_array(),
             EnumCache::cast(this)->indices());
  } else {
    VerifyObjectField(isolate, kValue1Offset);
    VerifyObjectField(isolate, kValue2Offset);
  }
}

void Tuple3::Tuple3Verify(Isolate* isolate) {
  CHECK(IsTuple3());
  VerifyObjectField(isolate, kValue1Offset);
  VerifyObjectField(isolate, kValue2Offset);
  VerifyObjectField(isolate, kValue3Offset);
}

void ObjectBoilerplateDescription::ObjectBoilerplateDescriptionVerify(
    Isolate* isolate) {
  CHECK(IsObjectBoilerplateDescription());
  CHECK_GE(this->length(),
           ObjectBoilerplateDescription::kDescriptionStartIndex);
  this->FixedArrayVerify(isolate);
}

void ArrayBoilerplateDescription::ArrayBoilerplateDescriptionVerify(
    Isolate* isolate) {
  CHECK(IsArrayBoilerplateDescription());
  CHECK(constant_elements()->IsFixedArrayBase());
  VerifyObjectField(isolate, kConstantElementsOffset);
}

void WasmDebugInfo::WasmDebugInfoVerify(Isolate* isolate) {
  CHECK(IsWasmDebugInfo());
  VerifyObjectField(isolate, kInstanceOffset);
  CHECK(wasm_instance()->IsWasmInstanceObject());
  VerifyObjectField(isolate, kInterpreterHandleOffset);
  CHECK(interpreter_handle()->IsUndefined(isolate) ||
        interpreter_handle()->IsForeign());
  VerifyObjectField(isolate, kInterpretedFunctionsOffset);
  VerifyObjectField(isolate, kLocalsNamesOffset);
  VerifyObjectField(isolate, kCWasmEntriesOffset);
  VerifyObjectField(isolate, kCWasmEntryMapOffset);
}

void WasmInstanceObject::WasmInstanceObjectVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
  CHECK(IsWasmInstanceObject());

  // Just generically check all tagged fields. Don't check the untagged fields,
  // as some of them might still contain the "undefined" value if the
  // WasmInstanceObject is not fully set up yet.
  for (int offset = kHeaderSize; offset < kFirstUntaggedOffset;
       offset += kPointerSize) {
    VerifyObjectField(isolate, offset);
  }
}

void WasmExportedFunctionData::WasmExportedFunctionDataVerify(
    Isolate* isolate) {
  CHECK(IsWasmExportedFunctionData());
  VerifyObjectField(isolate, kWrapperCodeOffset);
  CHECK(wrapper_code()->kind() == Code::JS_TO_WASM_FUNCTION ||
        wrapper_code()->kind() == Code::C_WASM_ENTRY);
  VerifyObjectField(isolate, kInstanceOffset);
  VerifySmiField(kJumpTableOffsetOffset);
  VerifySmiField(kFunctionIndexOffset);
}

void WasmModuleObject::WasmModuleObjectVerify(Isolate* isolate) {
  CHECK(IsWasmModuleObject());
  VerifyObjectField(isolate, kNativeModuleOffset);
  CHECK(managed_native_module()->IsForeign());
  VerifyObjectField(isolate, kExportWrappersOffset);
  CHECK(export_wrappers()->IsFixedArray());
  VerifyObjectField(isolate, kScriptOffset);
  VerifyObjectField(isolate, kAsmJsOffsetTableOffset);
  VerifyObjectField(isolate, kBreakPointInfosOffset);
}

void DataHandler::DataHandlerVerify(Isolate* isolate) {
  CHECK(IsDataHandler());
  CHECK_IMPLIES(!smi_handler()->IsSmi(),
                smi_handler()->IsCode() && IsStoreHandler());
  CHECK(validity_cell()->IsSmi() || validity_cell()->IsCell());
  int data_count = data_field_count();
  if (data_count >= 1) {
    VerifyMaybeObjectField(isolate, kData1Offset);
  }
  if (data_count >= 2) {
    VerifyMaybeObjectField(isolate, kData2Offset);
  }
  if (data_count >= 3) {
    VerifyMaybeObjectField(isolate, kData3Offset);
  }
}

void LoadHandler::LoadHandlerVerify(Isolate* isolate) {
  DataHandler::DataHandlerVerify(isolate);
  // TODO(ishell): check handler integrity
}

void StoreHandler::StoreHandlerVerify(Isolate* isolate) {
  DataHandler::DataHandlerVerify(isolate);
  // TODO(ishell): check handler integrity
}

void AccessorInfo::AccessorInfoVerify(Isolate* isolate) {
  CHECK(IsAccessorInfo());
  VerifyPointer(isolate, name());
  VerifyPointer(isolate, expected_receiver_type());
  VerifyForeignPointer(isolate, this, getter());
  VerifyForeignPointer(isolate, this, setter());
  VerifyForeignPointer(isolate, this, js_getter());
  VerifyPointer(isolate, data());
}

void AccessorPair::AccessorPairVerify(Isolate* isolate) {
  CHECK(IsAccessorPair());
  VerifyPointer(isolate, getter());
  VerifyPointer(isolate, setter());
}

void AccessCheckInfo::AccessCheckInfoVerify(Isolate* isolate) {
  CHECK(IsAccessCheckInfo());
  VerifyPointer(isolate, callback());
  VerifyPointer(isolate, named_interceptor());
  VerifyPointer(isolate, indexed_interceptor());
  VerifyPointer(isolate, data());
}

void CallHandlerInfo::CallHandlerInfoVerify(Isolate* isolate) {
  CHECK(IsCallHandlerInfo());
  CHECK(map() == ReadOnlyRoots(isolate).side_effect_call_handler_info_map() ||
        map() ==
            ReadOnlyRoots(isolate).side_effect_free_call_handler_info_map() ||
        map() == ReadOnlyRoots(isolate)
                     .next_call_side_effect_free_call_handler_info_map());
  VerifyPointer(isolate, callback());
  VerifyPointer(isolate, js_callback());
  VerifyPointer(isolate, data());
}

void InterceptorInfo::InterceptorInfoVerify(Isolate* isolate) {
  CHECK(IsInterceptorInfo());
  VerifyForeignPointer(isolate, this, getter());
  VerifyForeignPointer(isolate, this, setter());
  VerifyForeignPointer(isolate, this, query());
  VerifyForeignPointer(isolate, this, deleter());
  VerifyForeignPointer(isolate, this, enumerator());
  VerifyPointer(isolate, data());
  VerifySmiField(kFlagsOffset);
}

void TemplateInfo::TemplateInfoVerify(Isolate* isolate) {
  VerifyPointer(isolate, tag());
  VerifyPointer(isolate, property_list());
  VerifyPointer(isolate, property_accessors());
}

void FunctionTemplateInfo::FunctionTemplateInfoVerify(Isolate* isolate) {
  CHECK(IsFunctionTemplateInfo());
  TemplateInfoVerify(isolate);
  VerifyPointer(isolate, serial_number());
  VerifyPointer(isolate, call_code());
  VerifyPointer(isolate, prototype_template());
  VerifyPointer(isolate, parent_template());
  VerifyPointer(isolate, named_property_handler());
  VerifyPointer(isolate, indexed_property_handler());
  VerifyPointer(isolate, instance_template());
  VerifyPointer(isolate, signature());
  VerifyPointer(isolate, access_check_info());
  VerifyPointer(isolate, cached_property_name());
}

void ObjectTemplateInfo::ObjectTemplateInfoVerify(Isolate* isolate) {
  CHECK(IsObjectTemplateInfo());
  TemplateInfoVerify(isolate);
  VerifyPointer(isolate, constructor());
  VerifyPointer(isolate, data());
}

void AllocationSite::AllocationSiteVerify(Isolate* isolate) {
  CHECK(IsAllocationSite());
}

void AllocationMemento::AllocationMementoVerify(Isolate* isolate) {
  CHECK(IsAllocationMemento());
  VerifyHeapPointer(isolate, allocation_site());
  CHECK(!IsValid() || GetAllocationSite()->IsAllocationSite());
}

void Script::ScriptVerify(Isolate* isolate) {
  CHECK(IsScript());
  VerifyPointer(isolate, source());
  VerifyPointer(isolate, name());
  VerifyPointer(isolate, line_ends());
  for (int i = 0; i < shared_function_infos()->length(); ++i) {
    MaybeObject* maybe_object = shared_function_infos()->Get(i);
    HeapObject* heap_object;
    CHECK(maybe_object->IsWeakHeapObject() ||
          maybe_object->IsClearedWeakHeapObject() ||
          (maybe_object->ToStrongHeapObject(&heap_object) &&
           heap_object->IsUndefined(isolate)));
  }
}

void NormalizedMapCache::NormalizedMapCacheVerify(Isolate* isolate) {
  WeakFixedArray::cast(this)->WeakFixedArrayVerify(isolate);
  if (FLAG_enable_slow_asserts) {
    for (int i = 0; i < length(); i++) {
      MaybeObject* e = WeakFixedArray::Get(i);
      HeapObject* heap_object;
      if (e->ToWeakHeapObject(&heap_object)) {
        Map::cast(heap_object)->DictionaryMapVerify(isolate);
      } else {
        CHECK(e->IsClearedWeakHeapObject() ||
              (e->ToStrongHeapObject(&heap_object) &&
               heap_object->IsUndefined(isolate)));
      }
    }
  }
}

void DebugInfo::DebugInfoVerify(Isolate* isolate) {
  CHECK(IsDebugInfo());
  VerifyPointer(isolate, shared());
  VerifyPointer(isolate, script());
  VerifyPointer(isolate, original_bytecode_array());
  VerifyPointer(isolate, break_points());
}

void StackFrameInfo::StackFrameInfoVerify(Isolate* isolate) {
  CHECK(IsStackFrameInfo());
  VerifyPointer(isolate, script_name());
  VerifyPointer(isolate, script_name_or_source_url());
  VerifyPointer(isolate, function_name());
}

void PreParsedScopeData::PreParsedScopeDataVerify(Isolate* isolate) {
  CHECK(IsPreParsedScopeData());
  CHECK(scope_data()->IsByteArray());
  CHECK_GE(length(), 0);

  for (int i = 0; i < length(); ++i) {
    Object* child = child_data(i);
    CHECK(child->IsPreParsedScopeData() || child->IsNull());
    VerifyPointer(isolate, child);
  }
}

void UncompiledDataWithPreParsedScope::UncompiledDataWithPreParsedScopeVerify(
    Isolate* isolate) {
  CHECK(IsUncompiledDataWithPreParsedScope());
  VerifyPointer(isolate, inferred_name());
  VerifyPointer(isolate, pre_parsed_scope_data());
}

void UncompiledDataWithoutPreParsedScope::
    UncompiledDataWithoutPreParsedScopeVerify(Isolate* isolate) {
  CHECK(IsUncompiledDataWithoutPreParsedScope());
  VerifyPointer(isolate, inferred_name());
}

void InterpreterData::InterpreterDataVerify(Isolate* isolate) {
  CHECK(IsInterpreterData());
  CHECK(bytecode_array()->IsBytecodeArray());
  CHECK(interpreter_trampoline()->IsCode());
}

#ifdef V8_INTL_SUPPORT
void JSCollator::JSCollatorVerify(Isolate* isolate) {
  CHECK(IsJSCollator());
  JSObjectVerify(isolate);
  VerifyObjectField(isolate, kICUCollatorOffset);
  VerifyObjectField(isolate, kFlagsOffset);
  VerifyObjectField(isolate, kBoundCompareOffset);
}

void JSListFormat::JSListFormatVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
  VerifyObjectField(isolate, kLocaleOffset);
  VerifyObjectField(isolate, kFormatterOffset);
  VerifyObjectField(isolate, kFlagsOffset);
}

void JSLocale::JSLocaleVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
  VerifyObjectField(isolate, kLanguageOffset);
  VerifyObjectField(isolate, kScriptOffset);
  VerifyObjectField(isolate, kRegionOffset);
  VerifyObjectField(isolate, kBaseNameOffset);
  VerifyObjectField(isolate, kLocaleOffset);
  // Unicode extension fields.
  VerifyObjectField(isolate, kCalendarOffset);
  VerifyObjectField(isolate, kCaseFirstOffset);
  VerifyObjectField(isolate, kCollationOffset);
  VerifyObjectField(isolate, kHourCycleOffset);
  VerifyObjectField(isolate, kNumericOffset);
  VerifyObjectField(isolate, kNumberingSystemOffset);
}

void JSPluralRules::JSPluralRulesVerify(Isolate* isolate) {
  CHECK(IsJSPluralRules());
  JSObjectVerify(isolate);
  VerifyObjectField(isolate, kLocaleOffset);
  VerifyObjectField(isolate, kTypeOffset);
  VerifyObjectField(isolate, kICUPluralRulesOffset);
  VerifyObjectField(isolate, kICUDecimalFormatOffset);
}

void JSRelativeTimeFormat::JSRelativeTimeFormatVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
  VerifyObjectField(isolate, kLocaleOffset);
  VerifyObjectField(isolate, kFormatterOffset);
  VerifyObjectField(isolate, kFlagsOffset);
}
#endif  // V8_INTL_SUPPORT

#endif  // VERIFY_HEAP

#ifdef DEBUG

void JSObject::IncrementSpillStatistics(Isolate* isolate,
                                        SpillInformation* info) {
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
      for (int i = 0; i < len; i++) {
        if (e->get(i)->IsTheHole(isolate)) holes++;
      }
      info->number_of_fast_used_elements_   += len - holes;
      info->number_of_fast_unused_elements_ += holes;
      break;
    }

#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype) case TYPE##_ELEMENTS:

      TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
      {
        info->number_of_objects_with_fast_elements_++;
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
    if (!TransitionsAccessor::IsSpecialTransition(key->GetReadOnlyRoots(),
                                                  key)) {
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
bool CanLeak(Object* obj, Heap* heap) {
  if (!obj->IsHeapObject()) return false;
  if (obj->IsCell()) {
    return CanLeak(Cell::cast(obj)->value(), heap);
  }
  if (obj->IsPropertyCell()) {
    return CanLeak(PropertyCell::cast(obj)->value(), heap);
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
  return CanLeak(HeapObject::cast(obj)->map(), heap);
}

void Code::VerifyEmbeddedObjects(Isolate* isolate, VerifyMode mode) {
  if (kind() == OPTIMIZED_FUNCTION) return;
  Heap* heap = isolate->heap();
  int mask = RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT);
  for (RelocIterator it(this, mask); !it.done(); it.next()) {
    Object* target = it.rinfo()->target_object();
    DCHECK(!CanLeak(target, heap));
  }
}

#endif  // DEBUG

}  // namespace internal
}  // namespace v8
