// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/objects.h"

#include "src/codegen/assembler-inl.h"
#include "src/date/date.h"
#include "src/diagnostics/disasm.h"
#include "src/diagnostics/disassembler.h"
#include "src/heap/combined-heap.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/ic/handler-configuration-inl.h"
#include "src/init/bootstrapper.h"
#include "src/logging/counters.h"
#include "src/objects/allocation-site-inl.h"
#include "src/objects/arguments-inl.h"
#include "src/objects/bigint.h"
#include "src/objects/cell-inl.h"
#include "src/objects/data-handler-inl.h"
#include "src/objects/debug-objects-inl.h"
#include "src/objects/elements.h"
#include "src/objects/embedder-data-array-inl.h"
#include "src/objects/embedder-data-slot-inl.h"
#include "src/objects/feedback-cell-inl.h"
#include "src/objects/field-type.h"
#include "src/objects/foreign-inl.h"
#include "src/objects/free-space-inl.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/layout-descriptor.h"
#include "src/objects/objects-inl.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/js-break-iterator-inl.h"
#include "src/objects/js-collator-inl.h"
#endif  // V8_INTL_SUPPORT
#include "src/objects/js-collection-inl.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/js-date-time-format-inl.h"
#endif  // V8_INTL_SUPPORT
#include "src/objects/js-generator-inl.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/js-list-format-inl.h"
#include "src/objects/js-locale-inl.h"
#include "src/objects/js-number-format-inl.h"
#include "src/objects/js-plural-rules-inl.h"
#endif  // V8_INTL_SUPPORT
#include "src/objects/js-regexp-inl.h"
#include "src/objects/js-regexp-string-iterator-inl.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/js-relative-time-format-inl.h"
#include "src/objects/js-segment-iterator-inl.h"
#include "src/objects/js-segmenter-inl.h"
#endif  // V8_INTL_SUPPORT
#include "src/objects/js-weak-refs-inl.h"
#include "src/objects/literal-objects-inl.h"
#include "src/objects/maybe-object.h"
#include "src/objects/microtask-inl.h"
#include "src/objects/module-inl.h"
#include "src/objects/oddball-inl.h"
#include "src/objects/promise-inl.h"
#include "src/objects/stack-frame-info-inl.h"
#include "src/objects/struct-inl.h"
#include "src/objects/template-objects-inl.h"
#include "src/objects/transitions-inl.h"
#include "src/regexp/jsregexp.h"
#include "src/utils/ostreams.h"
#include "src/wasm/wasm-objects-inl.h"
#include "torque-generated/class-verifiers-tq.h"

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

#define USE_TORQUE_VERIFIER(Class)                                \
  void Class::Class##Verify(Isolate* isolate) {                   \
    TorqueGeneratedClassVerifiers::Class##Verify(*this, isolate); \
  }

void Object::ObjectVerify(Isolate* isolate) {
  RuntimeCallTimerScope timer(isolate, RuntimeCallCounterId::kObjectVerify);
  if (IsSmi()) {
    Smi::cast(*this).SmiVerify(isolate);
  } else {
    HeapObject::cast(*this).HeapObjectVerify(isolate);
  }
  CHECK(!IsConstructor() || IsCallable());
}

void Object::VerifyPointer(Isolate* isolate, Object p) {
  if (p.IsHeapObject()) {
    HeapObject::VerifyHeapPointer(isolate, p);
  } else {
    CHECK(p.IsSmi());
  }
}

void MaybeObject::VerifyMaybeObjectPointer(Isolate* isolate, MaybeObject p) {
  HeapObject heap_object;
  if (p->GetHeapObject(&heap_object)) {
    HeapObject::VerifyHeapPointer(isolate, heap_object);
  } else {
    CHECK(p->IsSmi() || p->IsCleared());
  }
}

namespace {
void VerifyForeignPointer(Isolate* isolate, HeapObject host, Object foreign) {
  host.VerifyPointer(isolate, foreign);
  CHECK(foreign.IsUndefined(isolate) || Foreign::IsNormalized(foreign));
}
}  // namespace

void Smi::SmiVerify(Isolate* isolate) {
  CHECK(IsSmi());
  CHECK(!IsCallable());
  CHECK(!IsConstructor());
}

void HeapObject::HeapObjectVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::HeapObjectVerify(*this, isolate);

  switch (map().instance_type()) {
#define STRING_TYPE_CASE(TYPE, size, name, CamelName) case TYPE:
    STRING_TYPE_LIST(STRING_TYPE_CASE)
#undef STRING_TYPE_CASE
    if (IsConsString()) {
      ConsString::cast(*this).ConsStringVerify(isolate);
    } else if (IsSlicedString()) {
      SlicedString::cast(*this).SlicedStringVerify(isolate);
    } else if (IsThinString()) {
      ThinString::cast(*this).ThinStringVerify(isolate);
    } else {
      String::cast(*this).StringVerify(isolate);
    }
    break;
    case SYMBOL_TYPE:
      Symbol::cast(*this).SymbolVerify(isolate);
      break;
    case MAP_TYPE:
      Map::cast(*this).MapVerify(isolate);
      break;
    case HEAP_NUMBER_TYPE:
      CHECK(IsHeapNumber());
      break;
    case MUTABLE_HEAP_NUMBER_TYPE:
      CHECK(IsMutableHeapNumber());
      break;
    case BIGINT_TYPE:
      BigInt::cast(*this).BigIntVerify(isolate);
      break;
    case CALL_HANDLER_INFO_TYPE:
      CallHandlerInfo::cast(*this).CallHandlerInfoVerify(isolate);
      break;
    case OBJECT_BOILERPLATE_DESCRIPTION_TYPE:
      ObjectBoilerplateDescription::cast(*this)
          .ObjectBoilerplateDescriptionVerify(isolate);
      break;
    case EMBEDDER_DATA_ARRAY_TYPE:
      EmbedderDataArray::cast(*this).EmbedderDataArrayVerify(isolate);
      break;
    // FixedArray types
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
    case EPHEMERON_HASH_TABLE_TYPE:
    case FIXED_ARRAY_TYPE:
    case SCOPE_INFO_TYPE:
    case SCRIPT_CONTEXT_TABLE_TYPE:
      FixedArray::cast(*this).FixedArrayVerify(isolate);
      break;
    case AWAIT_CONTEXT_TYPE:
    case BLOCK_CONTEXT_TYPE:
    case CATCH_CONTEXT_TYPE:
    case DEBUG_EVALUATE_CONTEXT_TYPE:
    case EVAL_CONTEXT_TYPE:
    case FUNCTION_CONTEXT_TYPE:
    case MODULE_CONTEXT_TYPE:
    case SCRIPT_CONTEXT_TYPE:
    case WITH_CONTEXT_TYPE:
      Context::cast(*this).ContextVerify(isolate);
      break;
    case NATIVE_CONTEXT_TYPE:
      NativeContext::cast(*this).NativeContextVerify(isolate);
      break;
    case WEAK_FIXED_ARRAY_TYPE:
      WeakFixedArray::cast(*this).WeakFixedArrayVerify(isolate);
      break;
    case WEAK_ARRAY_LIST_TYPE:
      WeakArrayList::cast(*this).WeakArrayListVerify(isolate);
      break;
    case FIXED_DOUBLE_ARRAY_TYPE:
      FixedDoubleArray::cast(*this).FixedDoubleArrayVerify(isolate);
      break;
    case FEEDBACK_METADATA_TYPE:
      FeedbackMetadata::cast(*this).FeedbackMetadataVerify(isolate);
      break;
    case BYTE_ARRAY_TYPE:
      ByteArray::cast(*this).ByteArrayVerify(isolate);
      break;
    case BYTECODE_ARRAY_TYPE:
      BytecodeArray::cast(*this).BytecodeArrayVerify(isolate);
      break;
    case DESCRIPTOR_ARRAY_TYPE:
      DescriptorArray::cast(*this).DescriptorArrayVerify(isolate);
      break;
    case TRANSITION_ARRAY_TYPE:
      TransitionArray::cast(*this).TransitionArrayVerify(isolate);
      break;
    case PROPERTY_ARRAY_TYPE:
      PropertyArray::cast(*this).PropertyArrayVerify(isolate);
      break;
    case FREE_SPACE_TYPE:
      FreeSpace::cast(*this).FreeSpaceVerify(isolate);
      break;
    case FEEDBACK_CELL_TYPE:
      FeedbackCell::cast(*this).FeedbackCellVerify(isolate);
      break;
    case FEEDBACK_VECTOR_TYPE:
      FeedbackVector::cast(*this).FeedbackVectorVerify(isolate);
      break;

    case CODE_TYPE:
      Code::cast(*this).CodeVerify(isolate);
      break;
    case ODDBALL_TYPE:
      Oddball::cast(*this).OddballVerify(isolate);
      break;
    case JS_OBJECT_TYPE:
    case JS_ERROR_TYPE:
    case JS_API_OBJECT_TYPE:
    case JS_SPECIAL_API_OBJECT_TYPE:
    case JS_CONTEXT_EXTENSION_OBJECT_TYPE:
      JSObject::cast(*this).JSObjectVerify(isolate);
      break;
    case WASM_MODULE_TYPE:
      WasmModuleObject::cast(*this).WasmModuleObjectVerify(isolate);
      break;
    case WASM_TABLE_TYPE:
      WasmTableObject::cast(*this).WasmTableObjectVerify(isolate);
      break;
    case WASM_MEMORY_TYPE:
      WasmMemoryObject::cast(*this).WasmMemoryObjectVerify(isolate);
      break;
    case WASM_GLOBAL_TYPE:
      WasmGlobalObject::cast(*this).WasmGlobalObjectVerify(isolate);
      break;
    case WASM_EXCEPTION_TYPE:
      WasmExceptionObject::cast(*this).WasmExceptionObjectVerify(isolate);
      break;
    case WASM_INSTANCE_TYPE:
      WasmInstanceObject::cast(*this).WasmInstanceObjectVerify(isolate);
      break;
    case JS_ARGUMENTS_TYPE:
      JSArgumentsObject::cast(*this).JSArgumentsObjectVerify(isolate);
      break;
    case JS_GENERATOR_OBJECT_TYPE:
      JSGeneratorObject::cast(*this).JSGeneratorObjectVerify(isolate);
      break;
    case JS_ASYNC_FUNCTION_OBJECT_TYPE:
      JSAsyncFunctionObject::cast(*this).JSAsyncFunctionObjectVerify(isolate);
      break;
    case JS_ASYNC_GENERATOR_OBJECT_TYPE:
      JSAsyncGeneratorObject::cast(*this).JSAsyncGeneratorObjectVerify(isolate);
      break;
    case JS_VALUE_TYPE:
      JSValue::cast(*this).JSValueVerify(isolate);
      break;
    case JS_DATE_TYPE:
      JSDate::cast(*this).JSDateVerify(isolate);
      break;
    case JS_BOUND_FUNCTION_TYPE:
      JSBoundFunction::cast(*this).JSBoundFunctionVerify(isolate);
      break;
    case JS_FUNCTION_TYPE:
      JSFunction::cast(*this).JSFunctionVerify(isolate);
      break;
    case JS_GLOBAL_PROXY_TYPE:
      JSGlobalProxy::cast(*this).JSGlobalProxyVerify(isolate);
      break;
    case JS_GLOBAL_OBJECT_TYPE:
      JSGlobalObject::cast(*this).JSGlobalObjectVerify(isolate);
      break;
    case CELL_TYPE:
      Cell::cast(*this).CellVerify(isolate);
      break;
    case PROPERTY_CELL_TYPE:
      PropertyCell::cast(*this).PropertyCellVerify(isolate);
      break;
    case JS_ARRAY_TYPE:
      JSArray::cast(*this).JSArrayVerify(isolate);
      break;
    case JS_MODULE_NAMESPACE_TYPE:
      JSModuleNamespace::cast(*this).JSModuleNamespaceVerify(isolate);
      break;
    case JS_SET_TYPE:
      JSSet::cast(*this).JSSetVerify(isolate);
      break;
    case JS_MAP_TYPE:
      JSMap::cast(*this).JSMapVerify(isolate);
      break;
    case JS_SET_KEY_VALUE_ITERATOR_TYPE:
    case JS_SET_VALUE_ITERATOR_TYPE:
      JSSetIterator::cast(*this).JSSetIteratorVerify(isolate);
      break;
    case JS_MAP_KEY_ITERATOR_TYPE:
    case JS_MAP_KEY_VALUE_ITERATOR_TYPE:
    case JS_MAP_VALUE_ITERATOR_TYPE:
      JSMapIterator::cast(*this).JSMapIteratorVerify(isolate);
      break;
    case JS_ARRAY_ITERATOR_TYPE:
      JSArrayIterator::cast(*this).JSArrayIteratorVerify(isolate);
      break;
    case JS_STRING_ITERATOR_TYPE:
      JSStringIterator::cast(*this).JSStringIteratorVerify(isolate);
      break;
    case JS_ASYNC_FROM_SYNC_ITERATOR_TYPE:
      JSAsyncFromSyncIterator::cast(*this).JSAsyncFromSyncIteratorVerify(
          isolate);
      break;
    case WEAK_CELL_TYPE:
      WeakCell::cast(*this).WeakCellVerify(isolate);
      break;
    case JS_WEAK_REF_TYPE:
      JSWeakRef::cast(*this).JSWeakRefVerify(isolate);
      break;
    case JS_FINALIZATION_GROUP_TYPE:
      JSFinalizationGroup::cast(*this).JSFinalizationGroupVerify(isolate);
      break;
    case JS_FINALIZATION_GROUP_CLEANUP_ITERATOR_TYPE:
      JSFinalizationGroupCleanupIterator::cast(*this)
          .JSFinalizationGroupCleanupIteratorVerify(isolate);
      break;
    case JS_WEAK_MAP_TYPE:
      JSWeakMap::cast(*this).JSWeakMapVerify(isolate);
      break;
    case JS_WEAK_SET_TYPE:
      JSWeakSet::cast(*this).JSWeakSetVerify(isolate);
      break;
    case JS_PROMISE_TYPE:
      JSPromise::cast(*this).JSPromiseVerify(isolate);
      break;
    case JS_REGEXP_TYPE:
      JSRegExp::cast(*this).JSRegExpVerify(isolate);
      break;
    case JS_REGEXP_STRING_ITERATOR_TYPE:
      JSRegExpStringIterator::cast(*this).JSRegExpStringIteratorVerify(isolate);
      break;
    case FILLER_TYPE:
      break;
    case JS_PROXY_TYPE:
      JSProxy::cast(*this).JSProxyVerify(isolate);
      break;
    case FOREIGN_TYPE:
      Foreign::cast(*this).ForeignVerify(isolate);
      break;
    case PREPARSE_DATA_TYPE:
      PreparseData::cast(*this).PreparseDataVerify(isolate);
      break;
    case UNCOMPILED_DATA_WITHOUT_PREPARSE_DATA_TYPE:
      UncompiledDataWithoutPreparseData::cast(*this)
          .UncompiledDataWithoutPreparseDataVerify(isolate);
      break;
    case UNCOMPILED_DATA_WITH_PREPARSE_DATA_TYPE:
      UncompiledDataWithPreparseData::cast(*this)
          .UncompiledDataWithPreparseDataVerify(isolate);
      break;
    case SHARED_FUNCTION_INFO_TYPE:
      SharedFunctionInfo::cast(*this).SharedFunctionInfoVerify(isolate);
      break;
    case JS_MESSAGE_OBJECT_TYPE:
      JSMessageObject::cast(*this).JSMessageObjectVerify(isolate);
      break;
    case JS_ARRAY_BUFFER_TYPE:
      JSArrayBuffer::cast(*this).JSArrayBufferVerify(isolate);
      break;
    case JS_TYPED_ARRAY_TYPE:
      JSTypedArray::cast(*this).JSTypedArrayVerify(isolate);
      break;
    case JS_DATA_VIEW_TYPE:
      JSDataView::cast(*this).JSDataViewVerify(isolate);
      break;
    case SMALL_ORDERED_HASH_SET_TYPE:
      SmallOrderedHashSet::cast(*this).SmallOrderedHashSetVerify(isolate);
      break;
    case SMALL_ORDERED_HASH_MAP_TYPE:
      SmallOrderedHashMap::cast(*this).SmallOrderedHashMapVerify(isolate);
      break;
    case SMALL_ORDERED_NAME_DICTIONARY_TYPE:
      SmallOrderedNameDictionary::cast(*this).SmallOrderedNameDictionaryVerify(
          isolate);
      break;
    case CODE_DATA_CONTAINER_TYPE:
      CodeDataContainer::cast(*this).CodeDataContainerVerify(isolate);
      break;
#ifdef V8_INTL_SUPPORT
    case JS_INTL_V8_BREAK_ITERATOR_TYPE:
      JSV8BreakIterator::cast(*this).JSV8BreakIteratorVerify(isolate);
      break;
    case JS_INTL_COLLATOR_TYPE:
      JSCollator::cast(*this).JSCollatorVerify(isolate);
      break;
    case JS_INTL_DATE_TIME_FORMAT_TYPE:
      JSDateTimeFormat::cast(*this).JSDateTimeFormatVerify(isolate);
      break;
    case JS_INTL_LIST_FORMAT_TYPE:
      JSListFormat::cast(*this).JSListFormatVerify(isolate);
      break;
    case JS_INTL_LOCALE_TYPE:
      JSLocale::cast(*this).JSLocaleVerify(isolate);
      break;
    case JS_INTL_NUMBER_FORMAT_TYPE:
      JSNumberFormat::cast(*this).JSNumberFormatVerify(isolate);
      break;
    case JS_INTL_PLURAL_RULES_TYPE:
      JSPluralRules::cast(*this).JSPluralRulesVerify(isolate);
      break;
    case JS_INTL_RELATIVE_TIME_FORMAT_TYPE:
      JSRelativeTimeFormat::cast(*this).JSRelativeTimeFormatVerify(isolate);
      break;
    case JS_INTL_SEGMENT_ITERATOR_TYPE:
      JSSegmentIterator::cast(*this).JSSegmentIteratorVerify(isolate);
      break;
    case JS_INTL_SEGMENTER_TYPE:
      JSSegmenter::cast(*this).JSSegmenterVerify(isolate);
      break;
#endif  // V8_INTL_SUPPORT

#define MAKE_STRUCT_CASE(TYPE, Name, name)   \
  case TYPE:                                 \
    Name::cast(*this).Name##Verify(isolate); \
    break;
      STRUCT_LIST(MAKE_STRUCT_CASE)
#undef MAKE_STRUCT_CASE

    case ALLOCATION_SITE_TYPE:
      AllocationSite::cast(*this).AllocationSiteVerify(isolate);
      break;

    case LOAD_HANDLER_TYPE:
      LoadHandler::cast(*this).LoadHandlerVerify(isolate);
      break;

    case STORE_HANDLER_TYPE:
      StoreHandler::cast(*this).StoreHandlerVerify(isolate);
      break;
  }
}

// static
void HeapObject::VerifyHeapPointer(Isolate* isolate, Object p) {
  CHECK(p.IsHeapObject());
  CHECK(IsValidHeapObject(isolate->heap(), HeapObject::cast(p)));
}

void Symbol::SymbolVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::SymbolVerify(*this, isolate);
  CHECK(HasHashCode());
  CHECK_GT(Hash(), 0);
  CHECK(name().IsUndefined(isolate) || name().IsString());
  CHECK_IMPLIES(IsPrivateName(), IsPrivate());
}

USE_TORQUE_VERIFIER(ByteArray)

void BytecodeArray::BytecodeArrayVerify(Isolate* isolate) {
  // TODO(oth): Walk bytecodes and immediate values to validate sanity.
  // - All bytecodes are known and well formed.
  // - Jumps must go to new instructions starts.
  // - No Illegal bytecodes.
  // - No consecutive sequences of prefix Wide / ExtraWide.
  CHECK(IsBytecodeArray());
  CHECK(constant_pool().IsFixedArray());
  VerifyHeapPointer(isolate, constant_pool());
}

USE_TORQUE_VERIFIER(FreeSpace)

USE_TORQUE_VERIFIER(FeedbackCell)

void FeedbackVector::FeedbackVectorVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::FeedbackVectorVerify(*this, isolate);
  MaybeObject code = optimized_code_weak_or_smi();
  MaybeObject::VerifyMaybeObjectPointer(isolate, code);
  CHECK(code->IsSmi() || code->IsWeakOrCleared());
}

bool JSObject::ElementsAreSafeToExamine() const {
  // If a GC was caused while constructing this object, the elements
  // pointer may point to a one pointer filler map.
  return elements() != GetReadOnlyRoots().one_pointer_filler_map();
}

namespace {

void VerifyJSObjectElements(Isolate* isolate, JSObject object) {
  // Only TypedArrays can have these specialized elements.
  if (object.IsJSTypedArray()) {
    // TODO(bmeurer,v8:4153): Fix CreateTypedArray to either not instantiate
    // the object or propertly initialize it on errors during construction.
    /* CHECK(object->HasTypedArrayElements()); */
    return;
  }
  CHECK(!object.elements().IsByteArray());

  if (object.HasDoubleElements()) {
    if (object.elements().length() > 0) {
      CHECK(object.elements().IsFixedDoubleArray());
    }
    return;
  }

  FixedArray elements = FixedArray::cast(object.elements());
  if (object.HasSmiElements()) {
    // We might have a partially initialized backing store, in which case we
    // allow the hole + smi values.
    for (int i = 0; i < elements.length(); i++) {
      Object value = elements.get(i);
      CHECK(value.IsSmi() || value.IsTheHole(isolate));
    }
  } else if (object.HasObjectElements()) {
    for (int i = 0; i < elements.length(); i++) {
      Object element = elements.get(i);
      CHECK(!HasWeakHeapObjectTag(element));
    }
  }
}
}  // namespace

void JSObject::JSObjectVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::JSObjectVerify(*this, isolate);
  VerifyHeapPointer(isolate, elements());

  CHECK_IMPLIES(HasSloppyArgumentsElements(), IsJSArgumentsObject());
  if (HasFastProperties()) {
    int actual_unused_property_fields = map().GetInObjectProperties() +
                                        property_array().length() -
                                        map().NextFreePropertyIndex();
    if (map().UnusedPropertyFields() != actual_unused_property_fields) {
      // There are two reasons why this can happen:
      // - in the middle of StoreTransitionStub when the new extended backing
      //   store is already set into the object and the allocation of the
      //   MutableHeapNumber triggers GC while the map isn't updated yet.
      // - deletion of the last property can leave additional backing store
      //   capacity behind.
      CHECK_GT(actual_unused_property_fields, map().UnusedPropertyFields());
      int delta = actual_unused_property_fields - map().UnusedPropertyFields();
      CHECK_EQ(0, delta % JSObject::kFieldsAdded);
    }
    DescriptorArray descriptors = map().instance_descriptors();
    bool is_transitionable_fast_elements_kind =
        IsTransitionableFastElementsKind(map().elements_kind());

    for (int i = 0; i < map().NumberOfOwnDescriptors(); i++) {
      PropertyDetails details = descriptors.GetDetails(i);
      if (details.location() == kField) {
        DCHECK_EQ(kData, details.kind());
        Representation r = details.representation();
        FieldIndex index = FieldIndex::ForDescriptor(map(), i);
        if (IsUnboxedDoubleField(index)) {
          DCHECK(r.IsDouble());
          continue;
        }
        if (COMPRESS_POINTERS_BOOL && index.is_inobject()) {
          VerifyObjectField(isolate, index.offset());
        }
        Object value = RawFastPropertyAt(index);
        if (r.IsDouble()) DCHECK(value.IsMutableHeapNumber());
        if (value.IsUninitialized(isolate)) continue;
        if (r.IsSmi()) DCHECK(value.IsSmi());
        if (r.IsHeapObject()) DCHECK(value.IsHeapObject());
        FieldType field_type = descriptors.GetFieldType(i);
        bool type_is_none = field_type.IsNone();
        bool type_is_any = field_type.IsAny();
        if (r.IsNone()) {
          CHECK(type_is_none);
        } else if (!type_is_any && !(type_is_none && r.IsHeapObject())) {
          CHECK(!field_type.NowStable() || field_type.NowContains(value));
        }
        CHECK_IMPLIES(is_transitionable_fast_elements_kind,
                      Map::IsMostGeneralFieldType(r, field_type));
      }
    }

    if (map().EnumLength() != kInvalidEnumCacheSentinel) {
      EnumCache enum_cache = descriptors.enum_cache();
      FixedArray keys = enum_cache.keys();
      FixedArray indices = enum_cache.indices();
      CHECK_LE(map().EnumLength(), keys.length());
      CHECK_IMPLIES(indices != ReadOnlyRoots(isolate).empty_fixed_array(),
                    keys.length() == indices.length());
    }
  }

  // If a GC was caused while constructing this object, the elements
  // pointer may point to a one pointer filler map.
  if (ElementsAreSafeToExamine()) {
    CHECK_EQ((map().has_fast_smi_or_object_elements() ||
              map().has_frozen_or_sealed_elements() ||
              (elements() == GetReadOnlyRoots().empty_fixed_array()) ||
              HasFastStringWrapperElements()),
             (elements().map() == GetReadOnlyRoots().fixed_array_map() ||
              elements().map() == GetReadOnlyRoots().fixed_cow_array_map()));
    CHECK_EQ(map().has_fast_object_elements(), HasObjectElements());
    VerifyJSObjectElements(isolate, *this);
  }
}

void Map::MapVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::MapVerify(*this, isolate);
  Heap* heap = isolate->heap();
  CHECK(!ObjectInYoungGeneration(*this));
  CHECK(FIRST_TYPE <= instance_type() && instance_type() <= LAST_TYPE);
  CHECK(instance_size() == kVariableSizeSentinel ||
        (kTaggedSize <= instance_size() &&
         static_cast<size_t>(instance_size()) < heap->Capacity()));
  CHECK(GetBackPointer().IsUndefined(isolate) ||
        !Map::cast(GetBackPointer()).is_stable());
  SLOW_DCHECK(instance_descriptors().IsSortedNoDuplicates());
  DisallowHeapAllocation no_gc;
  SLOW_DCHECK(
      TransitionsAccessor(isolate, *this, &no_gc).IsSortedNoDuplicates());
  SLOW_DCHECK(TransitionsAccessor(isolate, *this, &no_gc)
                  .IsConsistentWithBackPointers());
  SLOW_DCHECK(!FLAG_unbox_double_fields ||
              layout_descriptor().IsConsistentWithMap(*this));
  if (!may_have_interesting_symbols()) {
    CHECK(!has_named_interceptor());
    CHECK(!is_dictionary_map());
    CHECK(!is_access_check_needed());
    DescriptorArray const descriptors = instance_descriptors();
    for (int i = 0; i < NumberOfOwnDescriptors(); ++i) {
      CHECK(!descriptors.GetKey(i).IsInterestingSymbol());
    }
  }
  CHECK_IMPLIES(has_named_interceptor(), may_have_interesting_symbols());
  CHECK_IMPLIES(is_dictionary_map(), may_have_interesting_symbols());
  CHECK_IMPLIES(is_access_check_needed(), may_have_interesting_symbols());
  CHECK_IMPLIES(IsJSObjectMap() && !CanHaveFastTransitionableElementsKind(),
                IsDictionaryElementsKind(elements_kind()) ||
                    IsTerminalElementsKind(elements_kind()) ||
                    IsHoleyFrozenOrSealedElementsKind(elements_kind()));
  CHECK_IMPLIES(is_deprecated(), !is_stable());
  if (is_prototype_map()) {
    DCHECK(prototype_info() == Smi::kZero ||
           prototype_info().IsPrototypeInfo());
  }
}

void Map::DictionaryMapVerify(Isolate* isolate) {
  MapVerify(isolate);
  CHECK(is_dictionary_map());
  CHECK_EQ(kInvalidEnumCacheSentinel, EnumLength());
  CHECK_EQ(ReadOnlyRoots(isolate).empty_descriptor_array(),
           instance_descriptors());
  CHECK_EQ(0, UnusedPropertyFields());
  CHECK_EQ(Map::GetVisitorId(*this), visitor_id());
}

USE_TORQUE_VERIFIER(AliasedArgumentsEntry)

void EmbedderDataArray::EmbedderDataArrayVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::EmbedderDataArrayVerify(*this, isolate);
  EmbedderDataSlot start(*this, 0);
  EmbedderDataSlot end(*this, length());
  for (EmbedderDataSlot slot = start; slot < end; ++slot) {
    Object e = slot.load_tagged();
    Object::VerifyPointer(isolate, e);
  }
}

USE_TORQUE_VERIFIER(FixedArray)

void WeakFixedArray::WeakFixedArrayVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::WeakFixedArrayVerify(*this, isolate);
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
  TorqueGeneratedClassVerifiers::PropertyArrayVerify(*this, isolate);
  if (length() == 0) {
    CHECK_EQ(*this, ReadOnlyRoots(isolate).empty_property_array());
    return;
  }
  // There are no empty PropertyArrays.
  CHECK_LT(0, length());
  for (int i = 0; i < length(); i++) {
    Object e = get(i);
    Object::VerifyPointer(isolate, e);
  }
}

void FixedDoubleArray::FixedDoubleArrayVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::FixedDoubleArrayVerify(*this, isolate);
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

void Context::ContextVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::ContextVerify(*this, isolate);
  for (int i = 0; i < length(); i++) {
    VerifyObjectField(isolate, OffsetOfElementAt(i));
  }
}

void NativeContext::NativeContextVerify(Isolate* isolate) {
  ContextVerify(isolate);
  CHECK_EQ(length(), NativeContext::NATIVE_CONTEXT_SLOTS);
  CHECK_EQ(kSize, map().instance_size());
}

void FeedbackMetadata::FeedbackMetadataVerify(Isolate* isolate) {
  if (slot_count() == 0 && closure_feedback_cell_count() == 0) {
    CHECK_EQ(ReadOnlyRoots(isolate).empty_feedback_metadata(), *this);
  } else {
    FeedbackMetadataIterator iter(*this);
    while (iter.HasNext()) {
      iter.Next();
      FeedbackSlotKind kind = iter.kind();
      CHECK_NE(FeedbackSlotKind::kInvalid, kind);
      CHECK_GT(FeedbackSlotKind::kKindsNumber, kind);
    }
  }
}

void DescriptorArray::DescriptorArrayVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::DescriptorArrayVerify(*this, isolate);
  for (int i = 0; i < number_of_all_descriptors(); i++) {
    MaybeObject::VerifyMaybeObjectPointer(isolate, get(ToKeyIndex(i)));
    MaybeObject::VerifyMaybeObjectPointer(isolate, get(ToDetailsIndex(i)));
    MaybeObject::VerifyMaybeObjectPointer(isolate, get(ToValueIndex(i)));
  }
  if (number_of_all_descriptors() == 0) {
    Heap* heap = isolate->heap();
    CHECK_EQ(ReadOnlyRoots(heap).empty_descriptor_array(), *this);
    CHECK_EQ(0, number_of_all_descriptors());
    CHECK_EQ(0, number_of_descriptors());
    CHECK_EQ(ReadOnlyRoots(heap).empty_enum_cache(), enum_cache());
  } else {
    CHECK_LT(0, number_of_all_descriptors());
    CHECK_LE(number_of_descriptors(), number_of_all_descriptors());

    // Check that properties with private symbols names are non-enumerable.
    for (int descriptor = 0; descriptor < number_of_descriptors();
         descriptor++) {
      Object key = get(ToKeyIndex(descriptor))->cast<Object>();
      // number_of_descriptors() may be out of sync with the actual descriptors
      // written during descriptor array construction.
      if (key.IsUndefined(isolate)) continue;
      PropertyDetails details = GetDetails(descriptor);
      if (Name::cast(key).IsPrivate()) {
        CHECK_NE(details.attributes() & DONT_ENUM, 0);
      }
      MaybeObject value = get(ToValueIndex(descriptor));
      HeapObject heap_object;
      if (details.location() == kField) {
        CHECK(
            value == MaybeObject::FromObject(FieldType::None()) ||
            value == MaybeObject::FromObject(FieldType::Any()) ||
            value->IsCleared() ||
            (value->GetHeapObjectIfWeak(&heap_object) && heap_object.IsMap()));
      } else {
        CHECK(!value->IsWeakOrCleared());
        CHECK(!value->cast<Object>().IsMap());
      }
    }
  }
}

void TransitionArray::TransitionArrayVerify(Isolate* isolate) {
  WeakFixedArrayVerify(isolate);
  CHECK_LE(LengthFor(number_of_transitions()), length());
}

void JSArgumentsObject::JSArgumentsObjectVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::JSArgumentsObjectVerify(*this, isolate);
  if (IsSloppyArgumentsElementsKind(GetElementsKind())) {
    SloppyArgumentsElements::cast(elements())
        .SloppyArgumentsElementsVerify(isolate, *this);
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
}

void SloppyArgumentsElements::SloppyArgumentsElementsVerify(Isolate* isolate,
                                                            JSObject holder) {
  FixedArrayVerify(isolate);
  // Abort verification if only partially initialized (can't use arguments()
  // getter because it does FixedArray::cast()).
  if (get(kArgumentsIndex).IsUndefined(isolate)) return;

  ElementsKind kind = holder.GetElementsKind();
  bool is_fast = kind == FAST_SLOPPY_ARGUMENTS_ELEMENTS;
  CHECK(IsFixedArray());
  CHECK_GE(length(), 2);
  CHECK_EQ(map(), ReadOnlyRoots(isolate).sloppy_arguments_elements_map());
  Context context_object = context();
  FixedArray arg_elements = FixedArray::cast(arguments());
  if (arg_elements.length() == 0) {
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
    Object mapped = get_mapped_entry(i);
    if (mapped.IsTheHole(isolate)) {
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
    Object value = context_object.get(mappedIndex);
    CHECK(value.IsObject());
    // None of the context-mapped entries should exist in the arguments
    // elements.
    CHECK(!accessor->HasElement(holder, i, arg_elements));
  }
  CHECK_LE(nofMappedParameters, context_object.length());
  CHECK_LE(nofMappedParameters, arg_elements.length());
  CHECK_LE(maxMappedIndex, context_object.length());
  CHECK_LE(maxMappedIndex, arg_elements.length());
}

USE_TORQUE_VERIFIER(JSGeneratorObject)

void JSAsyncFunctionObject::JSAsyncFunctionObjectVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::JSAsyncFunctionObjectVerify(*this, isolate);
  promise().HeapObjectVerify(isolate);
}

void JSAsyncGeneratorObject::JSAsyncGeneratorObjectVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::JSAsyncGeneratorObjectVerify(*this, isolate);
  queue().HeapObjectVerify(isolate);
}

USE_TORQUE_VERIFIER(JSValue)

void JSDate::JSDateVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::JSDateVerify(*this, isolate);

  if (month().IsSmi()) {
    int month = Smi::ToInt(this->month());
    CHECK(0 <= month && month <= 11);
  }
  if (day().IsSmi()) {
    int day = Smi::ToInt(this->day());
    CHECK(1 <= day && day <= 31);
  }
  if (hour().IsSmi()) {
    int hour = Smi::ToInt(this->hour());
    CHECK(0 <= hour && hour <= 23);
  }
  if (min().IsSmi()) {
    int min = Smi::ToInt(this->min());
    CHECK(0 <= min && min <= 59);
  }
  if (sec().IsSmi()) {
    int sec = Smi::ToInt(this->sec());
    CHECK(0 <= sec && sec <= 59);
  }
  if (weekday().IsSmi()) {
    int weekday = Smi::ToInt(this->weekday());
    CHECK(0 <= weekday && weekday <= 6);
  }
  if (cache_stamp().IsSmi()) {
    CHECK(Smi::ToInt(cache_stamp()) <=
          Smi::ToInt(isolate->date_cache()->stamp()));
  }
}

void JSMessageObject::JSMessageObjectVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::JSMessageObjectVerify(*this, isolate);
  VerifySmiField(kMessageTypeOffset);
  VerifySmiField(kStartPositionOffset);
  VerifySmiField(kEndPositionOffset);
  VerifySmiField(kErrorLevelOffset);
}

void String::StringVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::StringVerify(*this, isolate);
  CHECK(length() >= 0 && length() <= Smi::kMaxValue);
  CHECK_IMPLIES(length() == 0, *this == ReadOnlyRoots(isolate).empty_string());
  if (IsInternalizedString()) {
    CHECK(!ObjectInYoungGeneration(*this));
  }
}

void ConsString::ConsStringVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::ConsStringVerify(*this, isolate);
  CHECK_GE(this->length(), ConsString::kMinLength);
  CHECK(this->length() == this->first().length() + this->second().length());
  if (this->IsFlat()) {
    // A flat cons can only be created by String::SlowFlatten.
    // Afterwards, the first part may be externalized or internalized.
    CHECK(this->first().IsSeqString() || this->first().IsExternalString() ||
          this->first().IsThinString());
  }
}

void ThinString::ThinStringVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::ThinStringVerify(*this, isolate);
  CHECK(this->actual().IsInternalizedString());
  CHECK(this->actual().IsSeqString() || this->actual().IsExternalString());
}

void SlicedString::SlicedStringVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::SlicedStringVerify(*this, isolate);
  CHECK(!this->parent().IsConsString());
  CHECK(!this->parent().IsSlicedString());
  CHECK_GE(this->length(), SlicedString::kMinLength);
}

void JSBoundFunction::JSBoundFunctionVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::JSBoundFunctionVerify(*this, isolate);
  CHECK(IsCallable());

  if (!raw_bound_target_function().IsUndefined(isolate)) {
    CHECK(bound_target_function().IsCallable());
    CHECK_EQ(IsConstructor(), bound_target_function().IsConstructor());
  }
}

void JSFunction::JSFunctionVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::JSFunctionVerify(*this, isolate);
  CHECK(raw_feedback_cell().IsFeedbackCell());
  CHECK(code().IsCode());
  CHECK(map().is_callable());
  Handle<JSFunction> function(*this, isolate);
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
  TorqueGeneratedClassVerifiers::SharedFunctionInfoVerify(*this, isolate);

  Object value = name_or_scope_info();
  if (value.IsScopeInfo()) {
    CHECK_LT(0, ScopeInfo::cast(value).length());
    CHECK_NE(value, ReadOnlyRoots(isolate).empty_scope_info());
  }

  CHECK(HasWasmExportedFunctionData() || IsApiFunction() ||
        HasBytecodeArray() || HasAsmWasmData() || HasBuiltinId() ||
        HasUncompiledDataWithPreparseData() ||
        HasUncompiledDataWithoutPreparseData() || HasWasmJSFunctionData() ||
        HasWasmCapiFunctionData());

  CHECK(script_or_debug_info().IsUndefined(isolate) ||
        script_or_debug_info().IsScript() || HasDebugInfo());

  if (!is_compiled()) {
    CHECK(!HasFeedbackMetadata());
    CHECK(outer_scope_info().IsScopeInfo() ||
          outer_scope_info().IsTheHole(isolate));
  } else if (HasBytecodeArray() && HasFeedbackMetadata()) {
    CHECK(feedback_metadata().IsFeedbackMetadata());
  }

  int expected_map_index = Context::FunctionMapIndex(
      language_mode(), kind(), HasSharedName(), needs_home_object());
  CHECK_EQ(expected_map_index, function_map_index());

  if (scope_info().length() > 0) {
    ScopeInfo info = scope_info();
    CHECK(kind() == info.function_kind());
    CHECK_EQ(kind() == kModule, info.scope_type() == MODULE_SCOPE);
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

  // At this point we only support skipping arguments adaptor frames
  // for strict mode functions (see https://crbug.com/v8/8895).
  CHECK_IMPLIES(is_safe_to_skip_arguments_adaptor(),
                language_mode() == LanguageMode::kStrict);
}

void JSGlobalProxy::JSGlobalProxyVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::JSGlobalProxyVerify(*this, isolate);
  CHECK(map().is_access_check_needed());
  // Make sure that this object has no properties, elements.
  CHECK_EQ(0, FixedArray::cast(elements()).length());
}

void JSGlobalObject::JSGlobalObjectVerify(Isolate* isolate) {
  CHECK(IsJSGlobalObject());
  // Do not check the dummy global object for the builtins.
  if (global_dictionary().NumberOfElements() == 0 && elements().length() == 0) {
    return;
  }
  JSObjectVerify(isolate);
}

void Oddball::OddballVerify(Isolate* isolate) {
  TorqueGeneratedOddball::OddballVerify(isolate);
  Heap* heap = isolate->heap();
  Object number = to_number();
  if (number.IsHeapObject()) {
    CHECK(number == ReadOnlyRoots(heap).nan_value() ||
          number == ReadOnlyRoots(heap).hole_nan_value());
  } else {
    CHECK(number.IsSmi());
    int value = Smi::ToInt(number);
    // Hidden oddballs have negative smis.
    const int kLeastHiddenOddballNumber = -7;
    CHECK_LE(value, 1);
    CHECK_GE(value, kLeastHiddenOddballNumber);
  }

  ReadOnlyRoots roots(heap);
  if (map() == roots.undefined_map()) {
    CHECK(*this == roots.undefined_value());
  } else if (map() == roots.the_hole_map()) {
    CHECK(*this == roots.the_hole_value());
  } else if (map() == roots.null_map()) {
    CHECK(*this == roots.null_value());
  } else if (map() == roots.boolean_map()) {
    CHECK(*this == roots.true_value() || *this == roots.false_value());
  } else if (map() == roots.uninitialized_map()) {
    CHECK(*this == roots.uninitialized_value());
  } else if (map() == roots.arguments_marker_map()) {
    CHECK(*this == roots.arguments_marker());
  } else if (map() == roots.termination_exception_map()) {
    CHECK(*this == roots.termination_exception());
  } else if (map() == roots.exception_map()) {
    CHECK(*this == roots.exception());
  } else if (map() == roots.optimized_out_map()) {
    CHECK(*this == roots.optimized_out());
  } else if (map() == roots.stale_register_map()) {
    CHECK(*this == roots.stale_register());
  } else if (map() == roots.self_reference_marker_map()) {
    // Multiple instances of this oddball may exist at once.
    CHECK_EQ(kind(), Oddball::kSelfReferenceMarker);
  } else {
    UNREACHABLE();
  }
}

USE_TORQUE_VERIFIER(Cell)

USE_TORQUE_VERIFIER(PropertyCell)

void CodeDataContainer::CodeDataContainerVerify(Isolate* isolate) {
  CHECK(IsCodeDataContainer());
  VerifyObjectField(isolate, kNextCodeLinkOffset);
  CHECK(next_code_link().IsCode() || next_code_link().IsUndefined(isolate));
}

void Code::CodeVerify(Isolate* isolate) {
  CHECK_IMPLIES(
      has_safepoint_table(),
      IsAligned(safepoint_table_offset(), static_cast<unsigned>(kIntSize)));
  CHECK_LE(safepoint_table_offset(), handler_table_offset());
  CHECK_LE(handler_table_offset(), constant_pool_offset());
  CHECK_LE(constant_pool_offset(), code_comments_offset());
  CHECK_LE(code_comments_offset(), InstructionSize());
  CHECK(IsAligned(raw_instruction_start(), kCodeAlignment));
  relocation_info().ObjectVerify(isolate);
  CHECK(Code::SizeFor(body_size()) <= kMaxRegularHeapObjectSize ||
        isolate->heap()->InSpace(*this, CODE_LO_SPACE));
  Address last_gc_pc = kNullAddress;

  for (RelocIterator it(*this); !it.done(); it.next()) {
    it.rinfo()->Verify(isolate);
    // Ensure that GC will not iterate twice over the same pointer.
    if (RelocInfo::IsGCRelocMode(it.rinfo()->rmode())) {
      CHECK(it.rinfo()->pc() != last_gc_pc);
      last_gc_pc = it.rinfo()->pc();
    }
  }
}

void JSArray::JSArrayVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::JSArrayVerify(*this, isolate);
  // If a GC was caused while constructing this array, the elements
  // pointer may point to a one pointer filler map.
  if (!ElementsAreSafeToExamine()) return;
  if (elements().IsUndefined(isolate)) return;
  CHECK(elements().IsFixedArray() || elements().IsFixedDoubleArray());
  if (elements().length() == 0) {
    CHECK_EQ(elements(), ReadOnlyRoots(isolate).empty_fixed_array());
  }
  if (!length().IsNumber()) return;
  // Verify that the length and the elements backing store are in sync.
  if (length().IsSmi() && (HasFastElements() || HasFrozenOrSealedElements())) {
    if (elements().length() > 0) {
      CHECK_IMPLIES(HasDoubleElements(), elements().IsFixedDoubleArray());
      CHECK_IMPLIES(HasSmiOrObjectElements() || HasFrozenOrSealedElements(),
                    elements().IsFixedArray());
    }
    int size = Smi::ToInt(length());
    // Holey / Packed backing stores might have slack or might have not been
    // properly initialized yet.
    CHECK(size <= elements().length() ||
          elements() == ReadOnlyRoots(isolate).empty_fixed_array());
  } else {
    CHECK(HasDictionaryElements());
    uint32_t array_length;
    CHECK(length().ToArrayLength(&array_length));
    if (array_length == 0xFFFFFFFF) {
      CHECK(length().ToArrayLength(&array_length));
    }
    if (array_length != 0) {
      NumberDictionary dict = NumberDictionary::cast(elements());
      // The dictionary can never have more elements than the array length + 1.
      // If the backing store grows the verification might be triggered with
      // the old length in place.
      uint32_t nof_elements = static_cast<uint32_t>(dict.NumberOfElements());
      if (nof_elements != 0) nof_elements--;
      CHECK_LE(nof_elements, array_length);
    }
  }
}

void JSSet::JSSetVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::JSSetVerify(*this, isolate);
  VerifyHeapPointer(isolate, table());
  CHECK(table().IsOrderedHashSet() || table().IsUndefined(isolate));
  // TODO(arv): Verify OrderedHashTable too.
}

void JSMap::JSMapVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::JSMapVerify(*this, isolate);
  VerifyHeapPointer(isolate, table());
  CHECK(table().IsOrderedHashMap() || table().IsUndefined(isolate));
  // TODO(arv): Verify OrderedHashTable too.
}

void JSSetIterator::JSSetIteratorVerify(Isolate* isolate) {
  CHECK(IsJSSetIterator());
  JSObjectVerify(isolate);
  VerifyHeapPointer(isolate, table());
  CHECK(table().IsOrderedHashSet());
  CHECK(index().IsSmi());
}

void JSMapIterator::JSMapIteratorVerify(Isolate* isolate) {
  CHECK(IsJSMapIterator());
  JSObjectVerify(isolate);
  VerifyHeapPointer(isolate, table());
  CHECK(table().IsOrderedHashMap());
  CHECK(index().IsSmi());
}

void WeakCell::WeakCellVerify(Isolate* isolate) {
  CHECK(IsWeakCell());

  CHECK(target().IsJSReceiver() || target().IsUndefined(isolate));

  CHECK(prev().IsWeakCell() || prev().IsUndefined(isolate));
  if (prev().IsWeakCell()) {
    CHECK_EQ(WeakCell::cast(prev()).next(), *this);
  }

  CHECK(next().IsWeakCell() || next().IsUndefined(isolate));
  if (next().IsWeakCell()) {
    CHECK_EQ(WeakCell::cast(next()).prev(), *this);
  }

  CHECK_IMPLIES(key().IsUndefined(isolate),
                key_list_prev().IsUndefined(isolate));
  CHECK_IMPLIES(key().IsUndefined(isolate),
                key_list_next().IsUndefined(isolate));

  CHECK(key_list_prev().IsWeakCell() || key_list_prev().IsUndefined(isolate));
  if (key_list_prev().IsWeakCell()) {
    CHECK_EQ(WeakCell::cast(key_list_prev()).key_list_next(), *this);
  }

  CHECK(key_list_next().IsWeakCell() || key_list_next().IsUndefined(isolate));
  if (key_list_next().IsWeakCell()) {
    CHECK_EQ(WeakCell::cast(key_list_next()).key_list_prev(), *this);
  }

  CHECK(finalization_group().IsUndefined(isolate) ||
        finalization_group().IsJSFinalizationGroup());
}

void JSWeakRef::JSWeakRefVerify(Isolate* isolate) {
  CHECK(IsJSWeakRef());
  JSObjectVerify(isolate);
  CHECK(target().IsUndefined(isolate) || target().IsJSReceiver());
}

void JSFinalizationGroup::JSFinalizationGroupVerify(Isolate* isolate) {
  CHECK(IsJSFinalizationGroup());
  JSObjectVerify(isolate);
  VerifyHeapPointer(isolate, cleanup());
  CHECK(active_cells().IsUndefined(isolate) || active_cells().IsWeakCell());
  if (active_cells().IsWeakCell()) {
    CHECK(WeakCell::cast(active_cells()).prev().IsUndefined(isolate));
  }
  CHECK(cleared_cells().IsUndefined(isolate) || cleared_cells().IsWeakCell());
  if (cleared_cells().IsWeakCell()) {
    CHECK(WeakCell::cast(cleared_cells()).prev().IsUndefined(isolate));
  }
}

void JSFinalizationGroupCleanupIterator::
    JSFinalizationGroupCleanupIteratorVerify(Isolate* isolate) {
  CHECK(IsJSFinalizationGroupCleanupIterator());
  JSObjectVerify(isolate);
  VerifyHeapPointer(isolate, finalization_group());
}

void FinalizationGroupCleanupJobTask::FinalizationGroupCleanupJobTaskVerify(
    Isolate* isolate) {
  CHECK(IsFinalizationGroupCleanupJobTask());
  CHECK(finalization_group().IsJSFinalizationGroup());
}

void JSWeakMap::JSWeakMapVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::JSWeakMapVerify(*this, isolate);
  VerifyHeapPointer(isolate, table());
  CHECK(table().IsEphemeronHashTable() || table().IsUndefined(isolate));
}

void JSArrayIterator::JSArrayIteratorVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::JSArrayIteratorVerify(*this, isolate);
  CHECK(iterated_object().IsJSReceiver());

  CHECK_GE(next_index().Number(), 0);
  CHECK_LE(next_index().Number(), kMaxSafeInteger);

  if (iterated_object().IsJSTypedArray()) {
    // JSTypedArray::length is limited to Smi range.
    CHECK(next_index().IsSmi());
    CHECK_LE(next_index().Number(), Smi::kMaxValue);
  } else if (iterated_object().IsJSArray()) {
    // JSArray::length is limited to Uint32 range.
    CHECK_LE(next_index().Number(), kMaxUInt32);
  }
}

void JSStringIterator::JSStringIteratorVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::JSStringIteratorVerify(*this, isolate);
  CHECK(string().IsString());

  CHECK_GE(index(), 0);
  CHECK_LE(index(), String::kMaxLength);
}

USE_TORQUE_VERIFIER(JSAsyncFromSyncIterator)

void JSWeakSet::JSWeakSetVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::JSWeakSetVerify(*this, isolate);
  VerifyHeapPointer(isolate, table());
  CHECK(table().IsEphemeronHashTable() || table().IsUndefined(isolate));
}

USE_TORQUE_VERIFIER(Microtask)

void CallableTask::CallableTaskVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::CallableTaskVerify(*this, isolate);
  CHECK(callable().IsCallable());
}

USE_TORQUE_VERIFIER(CallbackTask)

void PromiseReactionJobTask::PromiseReactionJobTaskVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::PromiseReactionJobTaskVerify(*this, isolate);
  VerifyHeapPointer(isolate, handler());
  CHECK(handler().IsUndefined(isolate) || handler().IsCallable());
}

USE_TORQUE_VERIFIER(PromiseFulfillReactionJobTask)

USE_TORQUE_VERIFIER(PromiseRejectReactionJobTask)

USE_TORQUE_VERIFIER(PromiseResolveThenableJobTask)

USE_TORQUE_VERIFIER(PromiseCapability)

USE_TORQUE_VERIFIER(PromiseReaction)

void JSPromise::JSPromiseVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::JSPromiseVerify(*this, isolate);
  VerifySmiField(kFlagsOffset);
  if (status() == Promise::kPending) {
    CHECK(reactions().IsSmi() || reactions().IsPromiseReaction());
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
      Object val = GetDataEntry(entry, offset);
      VerifyPointer(isolate, val);
    }
  }

  for (int entry = NumberOfElements() + NumberOfDeletedElements();
       entry < Capacity(); entry++) {
    for (int offset = 0; offset < Derived::kEntrySize; offset++) {
      Object val = GetDataEntry(entry, offset);
      CHECK(val.IsTheHole(isolate));
    }
  }
}
void SmallOrderedHashMap::SmallOrderedHashMapVerify(Isolate* isolate) {
  SmallOrderedHashTable<SmallOrderedHashMap>::SmallOrderedHashTableVerify(
      isolate);
  for (int entry = NumberOfElements(); entry < NumberOfDeletedElements();
       entry++) {
    for (int offset = 0; offset < kEntrySize; offset++) {
      Object val = GetDataEntry(entry, offset);
      CHECK(val.IsTheHole(isolate));
    }
  }
}

void SmallOrderedHashSet::SmallOrderedHashSetVerify(Isolate* isolate) {
  SmallOrderedHashTable<SmallOrderedHashSet>::SmallOrderedHashTableVerify(
      isolate);
  for (int entry = NumberOfElements(); entry < NumberOfDeletedElements();
       entry++) {
    for (int offset = 0; offset < kEntrySize; offset++) {
      Object val = GetDataEntry(entry, offset);
      CHECK(val.IsTheHole(isolate));
    }
  }
}

void SmallOrderedNameDictionary::SmallOrderedNameDictionaryVerify(
    Isolate* isolate) {
  SmallOrderedHashTable<
      SmallOrderedNameDictionary>::SmallOrderedHashTableVerify(isolate);
  for (int entry = NumberOfElements(); entry < NumberOfDeletedElements();
       entry++) {
    for (int offset = 0; offset < kEntrySize; offset++) {
      Object val = GetDataEntry(entry, offset);
      CHECK(val.IsTheHole(isolate) ||
            (PropertyDetails::Empty().AsSmi() == Smi::cast(val)));
    }
  }
}

void JSRegExp::JSRegExpVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::JSRegExpVerify(*this, isolate);
  switch (TypeTag()) {
    case JSRegExp::ATOM: {
      FixedArray arr = FixedArray::cast(data());
      CHECK(arr.get(JSRegExp::kAtomPatternIndex).IsString());
      break;
    }
    case JSRegExp::IRREGEXP: {
      bool is_native = RegExpImpl::UsesNativeRegExp();

      FixedArray arr = FixedArray::cast(data());
      Object one_byte_data = arr.get(JSRegExp::kIrregexpLatin1CodeIndex);
      // Smi : Not compiled yet (-1).
      // Code/ByteArray: Compiled code.
      CHECK((one_byte_data.IsSmi() &&
             Smi::ToInt(one_byte_data) == JSRegExp::kUninitializedValue) ||
            (is_native ? one_byte_data.IsCode() : one_byte_data.IsByteArray()));
      Object uc16_data = arr.get(JSRegExp::kIrregexpUC16CodeIndex);
      CHECK((uc16_data.IsSmi() &&
             Smi::ToInt(uc16_data) == JSRegExp::kUninitializedValue) ||
            (is_native ? uc16_data.IsCode() : uc16_data.IsByteArray()));

      CHECK(arr.get(JSRegExp::kIrregexpCaptureCountIndex).IsSmi());
      CHECK(arr.get(JSRegExp::kIrregexpMaxRegisterCountIndex).IsSmi());
      break;
    }
    default:
      CHECK_EQ(JSRegExp::NOT_COMPILED, TypeTag());
      CHECK(data().IsUndefined(isolate));
      break;
  }
}

void JSRegExpStringIterator::JSRegExpStringIteratorVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::JSRegExpStringIteratorVerify(*this, isolate);
  CHECK(iterating_string().IsString());
  VerifySmiField(kFlagsOffset);
}

void JSProxy::JSProxyVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::JSProxyVerify(*this, isolate);
  CHECK(map().GetConstructor().IsJSFunction());
  if (!IsRevoked()) {
    CHECK_EQ(target().IsCallable(), map().is_callable());
    CHECK_EQ(target().IsConstructor(), map().is_constructor());
  }
  CHECK(map().prototype().IsNull(isolate));
  // There should be no properties on a Proxy.
  CHECK_EQ(0, map().NumberOfOwnDescriptors());
}

void JSArrayBuffer::JSArrayBufferVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::JSArrayBufferVerify(*this, isolate);
  if (FIELD_SIZE(kOptionalPaddingOffset) != 0) {
    CHECK_EQ(4, FIELD_SIZE(kOptionalPaddingOffset));
    CHECK_EQ(0,
             *reinterpret_cast<uint32_t*>(address() + kOptionalPaddingOffset));
  }
}

void JSArrayBufferView::JSArrayBufferViewVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::JSArrayBufferViewVerify(*this, isolate);
  CHECK_LE(byte_length(), JSArrayBuffer::kMaxByteLength);
  CHECK_LE(byte_offset(), JSArrayBuffer::kMaxByteLength);
}

void JSTypedArray::JSTypedArrayVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::JSTypedArrayVerify(*this, isolate);
  CHECK_LE(length(), JSTypedArray::kMaxLength);
}

void JSDataView::JSDataViewVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::JSDataViewVerify(*this, isolate);
  if (!WasDetached()) {
    CHECK_EQ(reinterpret_cast<uint8_t*>(
                 JSArrayBuffer::cast(buffer()).backing_store()) +
                 byte_offset(),
             data_pointer());
  }
}

USE_TORQUE_VERIFIER(Foreign)

void AsyncGeneratorRequest::AsyncGeneratorRequestVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::AsyncGeneratorRequestVerify(*this, isolate);
  CHECK_GE(resume_mode(), JSGeneratorObject::kNext);
  CHECK_LE(resume_mode(), JSGeneratorObject::kThrow);
  next().ObjectVerify(isolate);
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
  TorqueGeneratedClassVerifiers::ModuleInfoEntryVerify(*this, isolate);
  CHECK_IMPLIES(import_name().IsString(), module_request() >= 0);
  CHECK_IMPLIES(export_name().IsString() && import_name().IsString(),
                local_name().IsUndefined(isolate));
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

  CHECK((status() >= kEvaluating && code().IsModuleInfo()) ||
        (status() == kInstantiated && code().IsJSGeneratorObject()) ||
        (status() == kInstantiating && code().IsJSFunction()) ||
        (code().IsSharedFunctionInfo()));

  CHECK_EQ(status() == kErrored, !exception().IsTheHole(isolate));

  CHECK(module_namespace().IsUndefined(isolate) ||
        module_namespace().IsJSModuleNamespace());
  if (module_namespace().IsJSModuleNamespace()) {
    CHECK_LE(kInstantiating, status());
    CHECK_EQ(JSModuleNamespace::cast(module_namespace()).module(), *this);
  }

  CHECK_EQ(requested_modules().length(), info().module_requests().length());

  CHECK(import_meta().IsTheHole(isolate) || import_meta().IsJSObject());

  CHECK_NE(hash(), 0);
}

void PrototypeInfo::PrototypeInfoVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::PrototypeInfoVerify(*this, isolate);
  if (prototype_users().IsWeakArrayList()) {
    PrototypeUsers::Verify(WeakArrayList::cast(prototype_users()));
  } else {
    CHECK(prototype_users().IsSmi());
  }
}

void PrototypeUsers::Verify(WeakArrayList array) {
  if (array.length() == 0) {
    // Allow empty & uninitialized lists.
    return;
  }
  // Verify empty slot chain.
  int empty_slot = Smi::ToInt(empty_slot_index(array));
  int empty_slots_count = 0;
  while (empty_slot != kNoEmptySlotsMarker) {
    CHECK_GT(empty_slot, 0);
    CHECK_LT(empty_slot, array.length());
    empty_slot = array.Get(empty_slot).ToSmi().value();
    ++empty_slots_count;
  }

  // Verify that all elements are either weak pointers or SMIs marking empty
  // slots.
  int weak_maps_count = 0;
  for (int i = kFirstIndex; i < array.length(); ++i) {
    HeapObject heap_object;
    MaybeObject object = array.Get(i);
    if ((object->GetHeapObjectIfWeak(&heap_object) && heap_object.IsMap()) ||
        object->IsCleared()) {
      ++weak_maps_count;
    } else {
      CHECK(object->IsSmi());
    }
  }

  CHECK_EQ(weak_maps_count + empty_slots_count + 1, array.length());
}

USE_TORQUE_VERIFIER(TemplateObjectDescription)

void EnumCache::EnumCacheVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::EnumCacheVerify(*this, isolate);
  Heap* heap = isolate->heap();
  if (*this == ReadOnlyRoots(heap).empty_enum_cache()) {
    CHECK_EQ(ReadOnlyRoots(heap).empty_fixed_array(), keys());
    CHECK_EQ(ReadOnlyRoots(heap).empty_fixed_array(), indices());
  }
}

USE_TORQUE_VERIFIER(SourcePositionTableWithFrameCache)

USE_TORQUE_VERIFIER(ClassPositions)

void ObjectBoilerplateDescription::ObjectBoilerplateDescriptionVerify(
    Isolate* isolate) {
  CHECK(IsObjectBoilerplateDescription());
  CHECK_GE(this->length(),
           ObjectBoilerplateDescription::kDescriptionStartIndex);
  this->FixedArrayVerify(isolate);
}

USE_TORQUE_VERIFIER(ArrayBoilerplateDescription)

USE_TORQUE_VERIFIER(AsmWasmData)

USE_TORQUE_VERIFIER(WasmDebugInfo)

USE_TORQUE_VERIFIER(WasmExceptionTag)

void WasmInstanceObject::WasmInstanceObjectVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
  CHECK(IsWasmInstanceObject());

  // Just generically check all tagged fields. Don't check the untagged fields,
  // as some of them might still contain the "undefined" value if the
  // WasmInstanceObject is not fully set up yet.
  for (int offset = kHeaderSize; offset < kEndOfStrongFieldsOffset;
       offset += kTaggedSize) {
    VerifyObjectField(isolate, offset);
  }
}

void WasmExportedFunctionData::WasmExportedFunctionDataVerify(
    Isolate* isolate) {
  TorqueGeneratedClassVerifiers::WasmExportedFunctionDataVerify(*this, isolate);
  CHECK(wrapper_code().kind() == Code::JS_TO_WASM_FUNCTION ||
        wrapper_code().kind() == Code::C_WASM_ENTRY);
}

void WasmModuleObject::WasmModuleObjectVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::WasmModuleObjectVerify(*this, isolate);
  CHECK(managed_native_module().IsForeign());
  CHECK(export_wrappers().IsFixedArray());
  CHECK(script().IsScript());
}

void WasmTableObject::WasmTableObjectVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::WasmTableObjectVerify(*this, isolate);
  CHECK(elements().IsFixedArray());
  VerifySmiField(kRawTypeOffset);
}

void WasmMemoryObject::WasmMemoryObjectVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::WasmMemoryObjectVerify(*this, isolate);
  CHECK(array_buffer().IsJSArrayBuffer());
  VerifySmiField(kMaximumPagesOffset);
}

USE_TORQUE_VERIFIER(WasmGlobalObject)

void WasmExceptionObject::WasmExceptionObjectVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::WasmExceptionObjectVerify(*this, isolate);
  CHECK(serialized_signature().IsByteArray());
}

void DataHandler::DataHandlerVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::DataHandlerVerify(*this, isolate);
  CHECK_IMPLIES(!smi_handler().IsSmi(),
                smi_handler().IsCode() && IsStoreHandler());
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
  TorqueGeneratedClassVerifiers::AccessorInfoVerify(*this, isolate);
  VerifyForeignPointer(isolate, *this, getter());
  VerifyForeignPointer(isolate, *this, setter());
  VerifyForeignPointer(isolate, *this, js_getter());
}

USE_TORQUE_VERIFIER(AccessorPair)

USE_TORQUE_VERIFIER(AccessCheckInfo)

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
  TorqueGeneratedClassVerifiers::InterceptorInfoVerify(*this, isolate);
  VerifyForeignPointer(isolate, *this, getter());
  VerifyForeignPointer(isolate, *this, setter());
  VerifyForeignPointer(isolate, *this, query());
  VerifyForeignPointer(isolate, *this, descriptor());
  VerifyForeignPointer(isolate, *this, deleter());
  VerifyForeignPointer(isolate, *this, enumerator());
  VerifyForeignPointer(isolate, *this, definer());
}

USE_TORQUE_VERIFIER(TemplateInfo)

USE_TORQUE_VERIFIER(FunctionTemplateInfo)

USE_TORQUE_VERIFIER(FunctionTemplateRareData)

USE_TORQUE_VERIFIER(WasmCapiFunctionData)

USE_TORQUE_VERIFIER(WasmJSFunctionData)

USE_TORQUE_VERIFIER(ObjectTemplateInfo)

void AllocationSite::AllocationSiteVerify(Isolate* isolate) {
  CHECK(IsAllocationSite());
  CHECK(dependent_code().IsDependentCode());
  CHECK(transition_info_or_boilerplate().IsSmi() ||
        transition_info_or_boilerplate().IsJSObject());
  CHECK(nested_site().IsAllocationSite() || nested_site() == Smi::kZero);
}

void AllocationMemento::AllocationMementoVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::AllocationMementoVerify(*this, isolate);
  VerifyHeapPointer(isolate, allocation_site());
  CHECK(!IsValid() || GetAllocationSite().IsAllocationSite());
}

void Script::ScriptVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::ScriptVerify(*this, isolate);
  for (int i = 0; i < shared_function_infos().length(); ++i) {
    MaybeObject maybe_object = shared_function_infos().Get(i);
    HeapObject heap_object;
    CHECK(maybe_object->IsWeak() || maybe_object->IsCleared() ||
          (maybe_object->GetHeapObjectIfStrong(&heap_object) &&
           heap_object.IsUndefined(isolate)));
  }
}

void NormalizedMapCache::NormalizedMapCacheVerify(Isolate* isolate) {
  WeakFixedArray::cast(*this).WeakFixedArrayVerify(isolate);
  if (FLAG_enable_slow_asserts) {
    for (int i = 0; i < length(); i++) {
      MaybeObject e = WeakFixedArray::Get(i);
      HeapObject heap_object;
      if (e->GetHeapObjectIfWeak(&heap_object)) {
        Map::cast(heap_object).DictionaryMapVerify(isolate);
      } else {
        CHECK(e->IsCleared() || (e->GetHeapObjectIfStrong(&heap_object) &&
                                 heap_object.IsUndefined(isolate)));
      }
    }
  }
}

USE_TORQUE_VERIFIER(DebugInfo)

USE_TORQUE_VERIFIER(StackTraceFrame)

USE_TORQUE_VERIFIER(StackFrameInfo)

void PreparseData::PreparseDataVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::PreparseDataVerify(*this, isolate);
  CHECK_LE(0, data_length());
  CHECK_LE(0, children_length());

  for (int i = 0; i < children_length(); ++i) {
    Object child = get_child_raw(i);
    CHECK(child.IsNull() || child.IsPreparseData());
    VerifyPointer(isolate, child);
  }
}

void UncompiledDataWithPreparseData::UncompiledDataWithPreparseDataVerify(
    Isolate* isolate) {
  CHECK(IsUncompiledDataWithPreparseData());
  VerifyPointer(isolate, inferred_name());
  VerifyPointer(isolate, preparse_data());
}

void UncompiledDataWithoutPreparseData::UncompiledDataWithoutPreparseDataVerify(
    Isolate* isolate) {
  CHECK(IsUncompiledDataWithoutPreparseData());
  VerifyPointer(isolate, inferred_name());
}

USE_TORQUE_VERIFIER(InterpreterData)

#ifdef V8_INTL_SUPPORT
void JSV8BreakIterator::JSV8BreakIteratorVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
  VerifyObjectField(isolate, kLocaleOffset);
  VerifyObjectField(isolate, kTypeOffset);
  VerifyObjectField(isolate, kBreakIteratorOffset);
  VerifyObjectField(isolate, kUnicodeStringOffset);
  VerifyObjectField(isolate, kBoundAdoptTextOffset);
  VerifyObjectField(isolate, kBoundFirstOffset);
  VerifyObjectField(isolate, kBoundNextOffset);
  VerifyObjectField(isolate, kBoundCurrentOffset);
  VerifyObjectField(isolate, kBoundBreakTypeOffset);
}

void JSCollator::JSCollatorVerify(Isolate* isolate) {
  CHECK(IsJSCollator());
  JSObjectVerify(isolate);
  VerifyObjectField(isolate, kICUCollatorOffset);
  VerifyObjectField(isolate, kBoundCompareOffset);
}

void JSDateTimeFormat::JSDateTimeFormatVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::JSDateTimeFormatVerify(*this, isolate);
  VerifySmiField(kFlagsOffset);
}

void JSListFormat::JSListFormatVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::JSListFormatVerify(*this, isolate);
  VerifySmiField(kFlagsOffset);
}

USE_TORQUE_VERIFIER(JSLocale)

void JSNumberFormat::JSNumberFormatVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::JSNumberFormatVerify(*this, isolate);
  VerifySmiField(kFlagsOffset);
}

void JSPluralRules::JSPluralRulesVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::JSPluralRulesVerify(*this, isolate);
  VerifySmiField(kFlagsOffset);
}

void JSRelativeTimeFormat::JSRelativeTimeFormatVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::JSRelativeTimeFormatVerify(*this, isolate);
  VerifySmiField(kFlagsOffset);
}

void JSSegmentIterator::JSSegmentIteratorVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::JSSegmentIteratorVerify(*this, isolate);
  VerifySmiField(kFlagsOffset);
}

void JSSegmenter::JSSegmenterVerify(Isolate* isolate) {
  TorqueGeneratedClassVerifiers::JSSegmenterVerify(*this, isolate);
  VerifySmiField(kFlagsOffset);
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
    info->number_of_fast_used_fields_ += map().NextFreePropertyIndex();
    info->number_of_fast_unused_fields_ += map().UnusedPropertyFields();
  } else if (IsJSGlobalObject()) {
    GlobalDictionary dict = JSGlobalObject::cast(*this).global_dictionary();
    info->number_of_slow_used_properties_ += dict.NumberOfElements();
    info->number_of_slow_unused_properties_ +=
        dict.Capacity() - dict.NumberOfElements();
  } else {
    NameDictionary dict = property_dictionary();
    info->number_of_slow_used_properties_ += dict.NumberOfElements();
    info->number_of_slow_unused_properties_ +=
        dict.Capacity() - dict.NumberOfElements();
  }
  // Indexed properties
  switch (GetElementsKind()) {
    case HOLEY_SMI_ELEMENTS:
    case PACKED_SMI_ELEMENTS:
    case HOLEY_DOUBLE_ELEMENTS:
    case PACKED_DOUBLE_ELEMENTS:
    case HOLEY_ELEMENTS:
    case HOLEY_FROZEN_ELEMENTS:
    case HOLEY_SEALED_ELEMENTS:
    case PACKED_ELEMENTS:
    case PACKED_FROZEN_ELEMENTS:
    case PACKED_SEALED_ELEMENTS:
    case FAST_STRING_WRAPPER_ELEMENTS: {
      info->number_of_objects_with_fast_elements_++;
      int holes = 0;
      FixedArray e = FixedArray::cast(elements());
      int len = e.length();
      for (int i = 0; i < len; i++) {
        if (e.get(i).IsTheHole(isolate)) holes++;
      }
      info->number_of_fast_used_elements_ += len - holes;
      info->number_of_fast_unused_elements_ += holes;
      break;
    }

#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype) case TYPE##_ELEMENTS:

      TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
      {
        info->number_of_objects_with_fast_elements_++;
        FixedArrayBase e = FixedArrayBase::cast(elements());
        info->number_of_fast_used_elements_ += e.length();
        break;
      }
    case DICTIONARY_ELEMENTS:
    case SLOW_STRING_WRAPPER_ELEMENTS: {
      NumberDictionary dict = element_dictionary();
      info->number_of_slow_used_elements_ += dict.NumberOfElements();
      info->number_of_slow_unused_elements_ +=
          dict.Capacity() - dict.NumberOfElements();
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
         number_of_objects_with_fast_properties_, number_of_fast_used_fields_,
         number_of_fast_unused_fields_);

  PrintF("    - slow properties (#%d): %d (used) %d (unused)\n",
         number_of_objects_ - number_of_objects_with_fast_properties_,
         number_of_slow_used_properties_, number_of_slow_unused_properties_);

  PrintF("    - fast elements (#%d): %d (used) %d (unused)\n",
         number_of_objects_with_fast_elements_, number_of_fast_used_elements_,
         number_of_fast_unused_elements_);

  PrintF("    - slow elements (#%d): %d (used) %d (unused)\n",
         number_of_objects_ - number_of_objects_with_fast_elements_,
         number_of_slow_used_elements_, number_of_slow_unused_elements_);

  PrintF("\n");
}

bool DescriptorArray::IsSortedNoDuplicates(int valid_entries) {
  if (valid_entries == -1) valid_entries = number_of_descriptors();
  Name current_key;
  uint32_t current = 0;
  for (int i = 0; i < number_of_descriptors(); i++) {
    Name key = GetSortedKey(i);
    if (key == current_key) {
      Print();
      return false;
    }
    current_key = key;
    uint32_t hash = GetSortedKey(i).Hash();
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
  Name prev_key;
  PropertyKind prev_kind = kData;
  PropertyAttributes prev_attributes = NONE;
  uint32_t prev_hash = 0;

  for (int i = 0; i < number_of_transitions(); i++) {
    Name key = GetSortedKey(i);
    uint32_t hash = key.Hash();
    PropertyKind kind = kData;
    PropertyAttributes attributes = NONE;
    if (!TransitionsAccessor::IsSpecialTransition(key.GetReadOnlyRoots(),
                                                  key)) {
      Map target = GetTarget(i);
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
  return transitions().IsSortedNoDuplicates();
}

static bool CheckOneBackPointer(Map current_map, Object target) {
  return !target.IsMap() || Map::cast(target).GetBackPointer() == current_map;
}

bool TransitionsAccessor::IsConsistentWithBackPointers() {
  int num_transitions = NumberOfTransitions();
  for (int i = 0; i < num_transitions; i++) {
    Map target = GetTarget(i);
    if (!CheckOneBackPointer(map_, target)) return false;
  }
  return true;
}

#undef USE_TORQUE_VERIFIER

#endif  // DEBUG

}  // namespace internal
}  // namespace v8
