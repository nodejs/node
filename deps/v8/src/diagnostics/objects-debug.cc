// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/logging.h"
#include "src/codegen/assembler-inl.h"
#include "src/common/globals.h"
#include "src/date/date.h"
#include "src/diagnostics/disasm.h"
#include "src/diagnostics/disassembler.h"
#include "src/heap/combined-heap.h"
#include "src/heap/heap-layout-inl.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/heap/read-only-heap.h"
#include "src/ic/handler-configuration-inl.h"
#include "src/init/bootstrapper.h"
#include "src/logging/runtime-call-stats-scope.h"
#include "src/objects/allocation-site-inl.h"
#include "src/objects/arguments-inl.h"
#include "src/objects/bigint.h"
#include "src/objects/call-site-info-inl.h"
#include "src/objects/cell-inl.h"
#include "src/objects/code-inl.h"
#include "src/objects/contexts.h"
#include "src/objects/data-handler-inl.h"
#include "src/objects/debug-objects-inl.h"
#include "src/objects/elements.h"
#include "src/objects/embedder-data-array-inl.h"
#include "src/objects/embedder-data-slot-inl.h"
#include "src/objects/feedback-cell-inl.h"
#include "src/objects/field-type.h"
#include "src/objects/foreign-inl.h"
#include "src/objects/free-space-inl.h"
#include "src/objects/function-kind.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/instance-type.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/js-atomics-synchronization-inl.h"
#include "src/objects/js-disposable-stack.h"
#include "src/objects/literal-objects.h"
#include "src/objects/objects-inl.h"
#include "src/objects/objects.h"
#include "src/objects/property-details.h"
#include "src/objects/tagged-field.h"
#include "src/objects/trusted-object.h"
#include "src/objects/turbofan-types-inl.h"
#include "src/objects/turboshaft-types-inl.h"
#include "src/roots/roots.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/js-break-iterator-inl.h"
#include "src/objects/js-collator-inl.h"
#endif  // V8_INTL_SUPPORT
#include "src/objects/js-collection-inl.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/js-date-time-format-inl.h"
#include "src/objects/js-display-names-inl.h"
#include "src/objects/js-duration-format-inl.h"
#endif  // V8_INTL_SUPPORT
#include "src/objects/js-disposable-stack-inl.h"
#include "src/objects/js-generator-inl.h"
#include "src/objects/js-iterator-helpers-inl.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/js-list-format-inl.h"
#include "src/objects/js-locale-inl.h"
#include "src/objects/js-number-format-inl.h"
#include "src/objects/js-plural-rules-inl.h"
#endif  // V8_INTL_SUPPORT
#include "src/objects/js-regexp-inl.h"
#include "src/objects/js-regexp-string-iterator-inl.h"
#include "src/objects/js-shadow-realm-inl.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/js-relative-time-format-inl.h"
#include "src/objects/js-segment-iterator-inl.h"
#include "src/objects/js-segmenter-inl.h"
#include "src/objects/js-segments-inl.h"
#endif  // V8_INTL_SUPPORT
#include "src/objects/hole.h"
#include "src/objects/js-raw-json-inl.h"
#include "src/objects/js-shared-array-inl.h"
#include "src/objects/js-struct-inl.h"
#ifdef V8_TEMPORAL_SUPPORT
#include "src/objects/js-temporal-objects-inl.h"
#endif  // V8_TEMPORAL_SUPPORT
#include "src/objects/js-weak-refs-inl.h"
#include "src/objects/literal-objects-inl.h"
#include "src/objects/maybe-object.h"
#include "src/objects/megadom-handler-inl.h"
#include "src/objects/microtask-inl.h"
#include "src/objects/module-inl.h"
#include "src/objects/oddball-inl.h"
#include "src/objects/promise-inl.h"
#include "src/objects/property-descriptor-object-inl.h"
#include "src/objects/sort-state.h"
#include "src/objects/string-forwarding-table-inl.h"
#include "src/objects/struct-inl.h"
#include "src/objects/swiss-name-dictionary-inl.h"
#include "src/objects/synthetic-module-inl.h"
#include "src/objects/template-objects-inl.h"
#include "src/objects/torque-defined-classes-inl.h"
#include "src/objects/transitions-inl.h"
#include "src/regexp/regexp.h"
#include "src/sandbox/js-dispatch-table-inl.h"
#include "src/utils/ostreams.h"

#if V8_ENABLE_WEBASSEMBLY
#include "src/base/strings.h"
#include "src/debug/debug-wasm-objects-inl.h"
#include "src/wasm/canonical-types.h"
#include "src/wasm/wasm-objects-inl.h"
#endif  // V8_ENABLE_WEBASSEMBLY

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
// - In cases where the InstanceType is too generic (e.g. FixedArray) the
//   XXXVerify of the outer method has to do recursive verification.
// - If the corresponding objects have inheritance the parent's Verify method
//   is called as well.
// - For any field containing pointers VerifyPointer(...) should be called.
//
// Caveats
// -------
// - Assume that any of the verify methods is incomplete!
// - Some integrity checks are only partially done due to objects being in
//   partially initialized states when a gc happens, for instance when outer
//   objects are allocated before inner ones.
//

#ifdef VERIFY_HEAP

// static
void Object::ObjectVerify(Tagged<Object> obj, Isolate* isolate) {
  RCS_SCOPE(isolate, RuntimeCallCounterId::kObjectVerify);
  if (IsSmi(obj)) {
    Smi::SmiVerify(Cast<Smi>(obj), isolate);
  } else {
    Cast<HeapObject>(obj)->HeapObjectVerify(isolate);
  }
  CHECK(!IsConstructor(obj) || IsCallable(obj));
}

void Object::VerifyPointer(Isolate* isolate, Tagged<Object> p) {
  if (IsHeapObject(p)) {
    HeapObject::VerifyHeapPointer(isolate, p);
  } else {
    CHECK(IsSmi(p));
  }
}

void Object::VerifyAnyTagged(Isolate* isolate, Tagged<Object> p) {
  if (IsHeapObject(p)) {
    if (V8_EXTERNAL_CODE_SPACE_BOOL) {
      CHECK(IsValidHeapObject(isolate->heap(), Cast<HeapObject>(p)));
    } else {
      HeapObject::VerifyHeapPointer(isolate, p);
    }
  } else {
    CHECK(IsSmi(p));
  }
}

void Object::VerifyMaybeObjectPointer(Isolate* isolate, Tagged<MaybeObject> p) {
  Tagged<HeapObject> heap_object;
  if (p.GetHeapObject(&heap_object)) {
    HeapObject::VerifyHeapPointer(isolate, heap_object);
  } else {
    CHECK(p.IsSmi() || p.IsCleared() || MapWord::IsPacked(p.ptr()));
  }
}

// static
void Smi::SmiVerify(Tagged<Smi> obj, Isolate* isolate) {
  CHECK(IsSmi(Tagged<Object>(obj)));
  CHECK(!IsCallable(obj));
  CHECK(!IsConstructor(obj));
}

// static
void TaggedIndex::TaggedIndexVerify(Tagged<TaggedIndex> obj, Isolate* isolate) {
  CHECK(IsTaggedIndex(obj));
}

void HeapObject::HeapObjectVerify(Isolate* isolate) {
  // Cast away heapobject-ness so that the IsHeapObject test is non-trivial.
  CHECK(IsHeapObject(Tagged<Object>(this)));

  Object::VerifyPointer(isolate, map());
  CHECK(IsMap(map()));

  CHECK(CheckRequiredAlignment());

  // Only TrustedObjects live in trusted space. See also TrustedObjectVerify.
  CHECK_IMPLIES(!IsTrustedObject(this) && !IsFreeSpaceOrFiller(this),
                !TrustedHeapLayout::InTrustedSpace(this));

  switch (map()->instance_type()) {
#define STRING_TYPE_CASE(TYPE, size, name, CamelName) case TYPE:
    STRING_TYPE_LIST(STRING_TYPE_CASE)
#undef STRING_TYPE_CASE
    if (IsConsString(this)) {
      Cast<ConsString>(this)->ConsStringVerify(isolate);
    } else if (IsSlicedString(this)) {
      Cast<SlicedString>(this)->SlicedStringVerify(isolate);
    } else if (IsThinString(this)) {
      Cast<ThinString>(this)->ThinStringVerify(isolate);
    } else if (IsSeqString(this)) {
      Cast<SeqString>(this)->SeqStringVerify(isolate);
    } else if (IsExternalString(this)) {
      Cast<ExternalString>(this)->ExternalStringVerify(isolate);
    } else {
      Cast<String>(this)->StringVerify(isolate);
    }
    break;
    // FixedArray types
    case HASH_TABLE_TYPE:
    case ORDERED_HASH_MAP_TYPE:
    case ORDERED_HASH_SET_TYPE:
    case ORDERED_NAME_DICTIONARY_TYPE:
    case NAME_TO_INDEX_HASH_TABLE_TYPE:
    case REGISTERED_SYMBOL_TABLE_TYPE:
    case NAME_DICTIONARY_TYPE:
    case GLOBAL_DICTIONARY_TYPE:
    case NUMBER_DICTIONARY_TYPE:
    case SIMPLE_NAME_DICTIONARY_TYPE:
    case SIMPLE_NUMBER_DICTIONARY_TYPE:
    case EPHEMERON_HASH_TABLE_TYPE:
      Cast<FixedArray>(this)->FixedArrayVerify(isolate);
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
      Cast<Context>(this)->ContextVerify(isolate);
      break;
    case NATIVE_CONTEXT_TYPE:
      Cast<NativeContext>(this)->NativeContextVerify(isolate);
      break;
    case TRANSITION_ARRAY_TYPE:
      Cast<TransitionArray>(this)->TransitionArrayVerify(isolate);
      break;

    case INSTRUCTION_STREAM_TYPE:
      TrustedCast<InstructionStream>(this)->InstructionStreamVerify(isolate);
      break;
    case JS_API_OBJECT_TYPE:
    case JS_ARRAY_ITERATOR_PROTOTYPE_TYPE:
    case JS_CONTEXT_EXTENSION_OBJECT_TYPE:
    case JS_ERROR_TYPE:
    case JS_ITERATOR_PROTOTYPE_TYPE:
    case JS_MAP_ITERATOR_PROTOTYPE_TYPE:
    case JS_OBJECT_PROTOTYPE_TYPE:
    case JS_PROMISE_PROTOTYPE_TYPE:
    case JS_REG_EXP_PROTOTYPE_TYPE:
    case JS_SET_ITERATOR_PROTOTYPE_TYPE:
    case JS_SET_PROTOTYPE_TYPE:
    case JS_SPECIAL_API_OBJECT_TYPE:
    case JS_STRING_ITERATOR_PROTOTYPE_TYPE:
    case JS_TYPED_ARRAY_PROTOTYPE_TYPE:
      Cast<JSObject>(this)->JSObjectVerify(isolate);
      break;
#if V8_ENABLE_WEBASSEMBLY
    case WASM_TRUSTED_INSTANCE_DATA_TYPE:
      TrustedCast<WasmTrustedInstanceData>(this)->WasmTrustedInstanceDataVerify(
          isolate);
      break;
    case WASM_DISPATCH_TABLE_TYPE:
      TrustedCast<WasmDispatchTable>(this)->WasmDispatchTableVerify(isolate);
      break;
    case WASM_DISPATCH_TABLE_FOR_IMPORTS_TYPE:
      TrustedCast<WasmDispatchTableForImports>(this)
          ->WasmDispatchTableForImportsVerify(isolate);
      break;
    case WASM_VALUE_OBJECT_TYPE:
      Cast<WasmValueObject>(this)->WasmValueObjectVerify(isolate);
      break;
    case WASM_EXCEPTION_PACKAGE_TYPE:
      Cast<WasmExceptionPackage>(this)->WasmExceptionPackageVerify(isolate);
      break;
#endif  // V8_ENABLE_WEBASSEMBLY
    case JS_SET_KEY_VALUE_ITERATOR_TYPE:
    case JS_SET_VALUE_ITERATOR_TYPE:
      Cast<JSSetIterator>(this)->JSSetIteratorVerify(isolate);
      break;
    case JS_MAP_KEY_ITERATOR_TYPE:
    case JS_MAP_KEY_VALUE_ITERATOR_TYPE:
    case JS_MAP_VALUE_ITERATOR_TYPE:
      Cast<JSMapIterator>(this)->JSMapIteratorVerify(isolate);
      break;
    case FILLER_TYPE:
      break;
    case CODE_TYPE:
      TrustedCast<Code>(this)->CodeVerify(isolate);
      break;
    case CODE_WRAPPER_TYPE:
      Cast<CodeWrapper>(this)->CodeWrapperVerify(isolate);
      break;
    case DOUBLE_STRING_CACHE_TYPE:
      Cast<DoubleStringCache>(this)->DoubleStringCacheVerify(isolate);
      break;

#define MAKE_TORQUE_CASE(Name, TYPE)                \
  case TYPE:                                        \
    TrustedCast<Name>(this)->Name##Verify(isolate); \
    break;
      // Every class that has its fields defined in a .tq file and corresponds
      // to exactly one InstanceType value is included in the following list.
      TORQUE_INSTANCE_CHECKERS_SINGLE_FULLY_DEFINED(MAKE_TORQUE_CASE)
      TORQUE_INSTANCE_CHECKERS_MULTIPLE_FULLY_DEFINED(MAKE_TORQUE_CASE)
#undef MAKE_TORQUE_CASE

    case HOLE_TYPE:
      Cast<Hole>(this)->HoleVerify(isolate);
      break;

    case TUPLE2_TYPE:
      Cast<Tuple2>(this)->Tuple2Verify(isolate);
      break;

    case CLASS_POSITIONS_TYPE:
      Cast<ClassPositions>(this)->ClassPositionsVerify(isolate);
      break;

    case ACCESSOR_PAIR_TYPE:
      Cast<AccessorPair>(this)->AccessorPairVerify(isolate);
      break;

    case ALLOCATION_SITE_TYPE:
      Cast<AllocationSite>(this)->AllocationSiteVerify(isolate);
      break;

    case LOAD_HANDLER_TYPE:
      Cast<LoadHandler>(this)->LoadHandlerVerify(isolate);
      break;

    case STORE_HANDLER_TYPE:
      Cast<StoreHandler>(this)->StoreHandlerVerify(isolate);
      break;

    case BIG_INT_BASE_TYPE:
      Cast<BigIntBase>(this)->BigIntBaseVerify(isolate);
      break;

    case FREE_SPACE_TYPE:
      Cast<FreeSpace>(this)->FreeSpaceVerify(isolate);
      break;

    case JS_CLASS_CONSTRUCTOR_TYPE:
    case JS_PROMISE_CONSTRUCTOR_TYPE:
    case JS_REG_EXP_CONSTRUCTOR_TYPE:
    case JS_ARRAY_CONSTRUCTOR_TYPE:
#define TYPED_ARRAY_CONSTRUCTORS_SWITCH(Type, type, TYPE, Ctype) \
  case TYPE##_TYPED_ARRAY_CONSTRUCTOR_TYPE:
      TYPED_ARRAYS(TYPED_ARRAY_CONSTRUCTORS_SWITCH)
#undef TYPED_ARRAY_CONSTRUCTORS_SWITCH
      Cast<JSFunction>(this)->JSFunctionVerify(isolate);
      break;
    case JS_LAST_DUMMY_API_OBJECT_TYPE:
      UNREACHABLE();
  }
}

// static
void HeapObject::VerifyHeapPointer(Isolate* isolate, Tagged<Object> p) {
  CHECK(IsHeapObject(p));
  // If you crashed here and {isolate->is_shared()}, there is a bug causing the
  // host of {p} to point to a non-shared object.
  CHECK(IsValidHeapObject(isolate->heap(), Cast<HeapObject>(p)));
  CHECK_IMPLIES(V8_EXTERNAL_CODE_SPACE_BOOL, !IsInstructionStream(p));
}

// static
void HeapObject::VerifyCodePointer(Isolate* isolate, Tagged<Object> p) {
  CHECK(IsHeapObject(p));
  CHECK(IsValidCodeObject(isolate->heap(), Cast<HeapObject>(p)));
  CHECK(IsInstructionStream(Cast<HeapObject>(p)));
}

void FreeSpace::FreeSpaceVerify(Isolate* isolate) {
  CHECK(IsFreeSpace(this));
  {
    Tagged<Object> size_in_tagged = size_in_tagged_.Relaxed_Load();
    CHECK(IsSmi(size_in_tagged));
  }
}

void Name::NameVerify(Isolate* isolate) {
  PrimitiveHeapObjectVerify(isolate);
  CHECK(IsName(this));
}

void Symbol::SymbolVerify(Isolate* isolate) {
  NameVerify(isolate);
  CHECK(IsSymbol(this));
  uint32_t hash;
  const bool has_hash = TryGetHash(&hash);
  CHECK(has_hash);
  CHECK_GT(hash, 0);
  CHECK(IsUndefined(description(), isolate) || IsString(description()));
  CHECK_IMPLIES(IsAnyPrivateName(), IsAnyPrivate());
  CHECK_IMPLIES(IsPrivateBrand(), IsAnyPrivateName());
}

void BytecodeArray::BytecodeArrayVerify(Isolate* isolate) {
  ExposedTrustedObjectVerify(isolate);

  {
    Object::VerifyPointer(isolate, length_.load());
    CHECK(IsSmi(length_.load()));
    CHECK_LE(0, length());
    CHECK_LE(length(), kMaxLength);
  }
  {
    auto o = constant_pool();
    Object::VerifyPointer(isolate, o);
    CHECK(IsTrustedFixedArray(o));
  }
  {
    auto o = handler_table();
    Object::VerifyPointer(isolate, o);
    CHECK(IsTrustedByteArray(o));
  }
  {
    auto o = wrapper();
    Object::VerifyPointer(isolate, o);
    CHECK(IsBytecodeWrapper(o));
    if (o->has_bytecode()) {
      // If the wrapper is fully initialized, it must point back to us.
      CHECK_EQ(o->bytecode(isolate), Tagged(this));
    }
  }
  {
    // Use the raw accessor here as source positions may not be available.
    auto o = raw_source_position_table(kAcquireLoad);
    Object::VerifyPointer(isolate, o);
    CHECK(o == Smi::zero() || IsTrustedByteArray(o));
  }

  // TODO(oth): Walk bytecodes and immediate values to validate sanity.
  // - All bytecodes are known and well formed.
  // - Jumps must go to new instructions starts.
  // - No Illegal bytecodes.
  // - No consecutive sequences of prefix Wide / ExtraWide.
  // - String constants for loads should be internalized strings.
}

void BytecodeWrapper::BytecodeWrapperVerify(Isolate* isolate) {
  if (!this->has_bytecode()) return;
  auto bytecode = this->bytecode(isolate);
  Object::VerifyPointer(isolate, bytecode);
  CHECK_EQ(bytecode->wrapper(), this);
}

void JSReceiver::JSReceiverVerify(Isolate* isolate) {
  CHECK(IsJSReceiver(this));

  Tagged<JSReceiver::PropertiesOrHash> properties_or_hash =
      raw_properties_or_hash(kRelaxedLoad);
  Object::VerifyPointer(isolate, properties_or_hash);
  CHECK(IsFixedArrayBase(properties_or_hash) || IsSmi(properties_or_hash) ||
        IsPropertyArray(properties_or_hash) ||
        IsSwissNameDictionary(properties_or_hash) ||
        IsGlobalDictionary(properties_or_hash));
}

bool JSObject::ElementsAreSafeToExamine() const {
  // If a GC was caused while constructing this object, the elements
  // pointer may point to a one pointer filler map.
  return elements() != GetReadOnlyRoots().one_pointer_filler_map();
}

namespace {

void VerifyJSObjectElements(Isolate* isolate, Tagged<JSObject> object);
void VerifyJSObjectElements(Isolate* isolate, const JSObject* object) {
  VerifyJSObjectElements(isolate, Cast<JSObject>(object));
}
void VerifyJSObjectElements(Isolate* isolate, Tagged<JSObject> object) {
  // Only TypedArrays can have these specialized elements.
  if (IsJSTypedArray(object)) {
    // TODO(bmeurer,v8:4153): Fix CreateTypedArray to either not instantiate
    // the object or properly initialize it on errors during construction.
    /* CHECK(object->HasTypedArrayOrRabGsabTypedArrayElements()); */
    return;
  }
  CHECK(!IsByteArray(object->elements()));

  if (object->HasDoubleElements()) {
    if (object->elements()->ulength().value() > 0) {
      CHECK(IsFixedDoubleArray(object->elements()));
    }
    return;
  }

  if (object->HasSloppyArgumentsElements()) {
    CHECK(IsSloppyArgumentsElements(object->elements()));
    return;
  }

  Tagged<FixedArray> elements = Cast<FixedArray>(object->elements());
  uint32_t elements_len = elements->ulength().value();
  if (object->HasSmiElements()) {
    // We might have a partially initialized backing store, in which case we
    // allow the hole + smi values.
    for (uint32_t i = 0; i < elements_len; i++) {
      Tagged<Object> value = elements->get(i);
      CHECK(IsSmi(value) || IsTheHole(value, isolate));
    }
  } else if (object->HasObjectElements()) {
    for (uint32_t i = 0; i < elements_len; i++) {
      Tagged<Object> element = elements->get(i);
      CHECK(!HasWeakHeapObjectTag(element));
    }
  }
}
}  // namespace

void JSObjectWithEmbedderSlots::JSObjectWithEmbedderSlotsVerify(
    Isolate* isolate) {
  JSObjectVerify(isolate);
}

void JSRawJson::JSRawJsonVerify(Isolate* isolate) { JSObjectVerify(isolate); }

void JSExternalObject::JSExternalObjectVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
}

void JSObject::JSObjectVerify(Isolate* isolate) {
  Cast<JSReceiver>(this)->JSReceiverVerify(isolate);
  HeapObject::VerifyHeapPointer(isolate, elements());

  CHECK_IMPLIES(HasSloppyArgumentsElements(), IsJSArgumentsObject(this));
  if (HasFastProperties()) {
    FieldStorageLocation offset = map()->NextFreeFieldStorageLocation();
    const uint32_t property_array_len = property_array()->length().value();
    if (map()->HasOutOfObjectProperties()) {
      CHECK(!offset.is_in_object);
      int actual_first_unused_property_index =
          offset.offset_in_words -
          OFFSET_OF_DATA_START(FixedArray) / kTaggedSize;
      int expected_first_unused_property_index =
          property_array_len - map()->UnusedPropertyFields();
      // We expect actual_first_unused_property_index and
      // expected_first_unused_property_index to be equal, but we might be in
      // the middle of extending a property array, in which case either we have:
      //   1. New property array and old map --> UnusedPropertyFields is stale
      //      and we need to subtract the extension, or
      //   2. Old property array and new map --> property_array_len is stale
      //      and we need to add in the extension.
      CHECK(actual_first_unused_property_index ==
                expected_first_unused_property_index ||
            actual_first_unused_property_index ==
                expected_first_unused_property_index - JSObject::kFieldsAdded ||
            actual_first_unused_property_index ==
                expected_first_unused_property_index + JSObject::kFieldsAdded);
    } else {
      // We should have a 0 length property array, but we might be in the middle
      // of adding the first property array entry so we might have a fresh
      // property array.
      CHECK(property_array_len == 0 ||
            property_array_len ==
                static_cast<uint32_t>(JSObject::kFieldsAdded));
      if (!offset.is_in_object) {
        CHECK_EQ(map()->GetInObjectPropertiesStartInWords() +
                     map()->GetInObjectProperties(),
                 map()->instance_size_in_words());
        CHECK_EQ(map()->UnusedPropertyFields(), 0);
      } else {
        int actual_first_unused_property_index =
            offset.offset_in_words - map()->GetInObjectPropertiesStartInWords();
        int expected_first_unused_property_index =
            map()->GetInObjectProperties() - map()->UnusedPropertyFields();
        CHECK_EQ(actual_first_unused_property_index,
                 expected_first_unused_property_index);
      }
    }
    Tagged<DescriptorArray> descriptors = map()->instance_descriptors();
    map()->VerifyDescriptorInObjectBits(descriptors,
                                        map()->NumberOfOwnDescriptors());
    bool is_transitionable_fast_elements_kind =
        IsTransitionableFastElementsKind(map()->elements_kind());

    for (InternalIndex i : map()->IterateOwnDescriptors()) {
      PropertyDetails details = descriptors->GetDetails(i);
      if (details.location() == PropertyLocation::kField) {
        CHECK_EQ(PropertyKind::kData, details.kind());
        Representation r = details.representation();
        FieldIndex index = FieldIndex::ForDetails(map(), details);
        if (COMPRESS_POINTERS_BOOL && index.is_inobject()) {
          VerifyObjectField(isolate, index.offset());
        }
        if (!index.is_inobject() &&
            static_cast<uint32_t>(index.outobject_array_index()) >=
                property_array()->length().value()) {
          // We might be in the middle of property array extension with an old
          // property array, ignore OOB reads of the property array.
          continue;
        }
        Tagged<Object> value = RawFastPropertyAt(index);
        CHECK_IMPLIES(r.IsDouble(), IsHeapNumber(value));
        if (IsUninitializedHole(value, isolate)) continue;
        CHECK_IMPLIES(r.IsSmi(), IsSmi(value));
        CHECK_IMPLIES(r.IsHeapObject(), IsHeapObject(value));
        Tagged<FieldType> field_type = descriptors->GetFieldType(i);
        bool type_is_none = IsNone(field_type);
        bool type_is_any = IsAny(field_type);
        if (r.IsNone()) {
          CHECK(type_is_none);
        } else if (r.IsHeapObject()) {
          CHECK(!type_is_none);
          if (!type_is_any) {
            CHECK_IMPLIES(FieldType::NowStable(field_type),
                          map()->is_deprecated() ||
                              FieldType::NowContains(field_type, value));
          }
        } else {
          CHECK(type_is_any);
        }
        CHECK_IMPLIES(is_transitionable_fast_elements_kind,
                      Map::IsMostGeneralFieldType(r, field_type));
      }
    }

    if (map()->EnumLength() != kInvalidEnumCacheSentinel) {
      Tagged<EnumCache> enum_cache = descriptors->enum_cache();
      Tagged<FixedArray> keys = enum_cache->keys();
      Tagged<FixedArray> indices = enum_cache->indices();
      CHECK_LE(static_cast<uint32_t>(map()->EnumLength()),
               keys->ulength().value());
      CHECK_IMPLIES(indices != ReadOnlyRoots(isolate).empty_fixed_array(),
                    keys->length() == indices->length());
    }
  }

  // If a GC was caused while constructing this object, the elements
  // pointer may point to a one pointer filler map.
  if (ElementsAreSafeToExamine()) {
    CHECK_EQ((map()->has_fast_smi_or_object_elements() ||
              map()->has_any_nonextensible_elements() ||
              (elements() == GetReadOnlyRoots().empty_fixed_array()) ||
              HasFastStringWrapperElements()),
             (elements()->map() == GetReadOnlyRoots().fixed_array_map() ||
              elements()->map() == GetReadOnlyRoots().fixed_cow_array_map()));
    CHECK_EQ(map()->has_fast_object_elements(), HasObjectElements());
    VerifyJSObjectElements(isolate, this);
  }
}

void Map::MapVerify(Isolate* isolate) {
  CHECK(IsMap(this));
  Object::VerifyPointer(isolate, prototype());
  Object::VerifyPointer(isolate,
                        constructor_or_back_pointer_or_native_context_.load());
  Object::VerifyPointer(isolate, instance_descriptors_.load());
  Object::VerifyPointer(isolate, dependent_code_.load());
  Object::VerifyPointer(isolate, prototype_validity_cell_.load());
  Object::VerifyMaybeObjectPointer(isolate,
                                   transitions_or_prototype_info_.load());
  Heap* heap = isolate->heap();
  CHECK(!HeapLayout::InYoungGeneration(this));
  CHECK(FIRST_TYPE <= instance_type() && instance_type() <= LAST_TYPE);
  CHECK(instance_size() == kVariableSizeSentinel ||
        (kTaggedSize <= instance_size() &&
         static_cast<size_t>(instance_size()) < heap->Capacity()));

  if (is_extended_map()) {
    bool handled = false;
    switch (UncheckedCast<ExtendedMap>(this)->map_kind()) {
      case ExtendedMapKind::kJSInterceptorMap: {
        Tagged<JSInterceptorMap> self = UncheckedCast<JSInterceptorMap>(this);
        Object::VerifyPointer(isolate, self->named_interceptor());
        Object::VerifyPointer(isolate, self->indexed_interceptor());
        handled = true;
        break;
      }
    }
    CHECK(handled);
  }

#if V8_ENABLE_WEBASSEMBLY
  bool is_wasm_struct = InstanceTypeChecker::IsWasmStruct(instance_type());
#else
  constexpr bool is_wasm_struct = false;
#endif  // V8_ENABLE_WEBASSEMBLY
  if (IsContextMap(this)) {
    // The map for the NativeContext is allocated before the NativeContext
    // itself, so it may happen that during a GC the native_context() is still
    // null.
    CHECK(IsNull(native_context_or_null()) ||
          IsNativeContext(native_context_or_null()));
    // The context's meta map is tied to the same native context.
    CHECK_EQ(native_context_or_null(), map()->native_context_or_null());
  } else {
    if (IsUndefined(GetBackPointer())) {
      if (!is_wasm_struct) {
        // Root maps must not have descriptors in the descriptor array that do
        // not belong to the map.
        CHECK_EQ(NumberOfOwnDescriptors(),
                 instance_descriptors()->number_of_descriptors());
      }
    } else {
      // If there is a parent map it must be non-stable.
      Tagged<Map> parent = Cast<Map>(GetBackPointer());
      CHECK(!parent->is_stable());
      Tagged<DescriptorArray> descriptors = instance_descriptors();
      if (!is_deprecated() && !parent->is_deprecated()) {
        CHECK_EQ(IsInobjectSlackTrackingInProgress(),
                 parent->IsInobjectSlackTrackingInProgress());
      }
      if (descriptors == parent->instance_descriptors()) {
        if (NumberOfOwnDescriptors() == parent->NumberOfOwnDescriptors() + 1) {
          // Descriptors sharing through property transitions takes over
          // ownership from the parent map.
          CHECK(!parent->owns_descriptors());
        } else {
          CHECK_EQ(NumberOfOwnDescriptors(), parent->NumberOfOwnDescriptors());
          // Descriptors sharing through special transitions properly takes over
          // ownership from the parent map unless it uses the canonical empty
          // descriptor array.
          if (descriptors != ReadOnlyRoots(isolate).empty_descriptor_array()) {
            CHECK_IMPLIES(owns_descriptors(), !parent->owns_descriptors());
            CHECK_IMPLIES(parent->owns_descriptors(), !owns_descriptors());
          }
        }
      }
    }
  }
  if (!is_wasm_struct) {
    VerifyDescriptorInObjectBits(instance_descriptors(),
                                 NumberOfOwnDescriptors());
    SLOW_DCHECK(instance_descriptors()->IsSortedNoDuplicates());
  }
  SLOW_DCHECK(TransitionsAccessor(isolate, this).IsSortedNoDuplicates());
  SLOW_DCHECK(
      TransitionsAccessor(isolate, this).IsConsistentWithBackPointers());

  if (InstanceTypeChecker::IsNativeContextSpecific(instance_type())) {
    // Native context-specific objects must have their own contextful meta map
    // modulo the following exceptions.
    if (instance_type() == NATIVE_CONTEXT_TYPE ||
        instance_type() == JS_GLOBAL_PROXY_TYPE) {
      // 1) Diring creation of the NativeContext the native context field might
      //    be not be initialized yet.
      // 2) The same applies to the placeholder JSGlobalProxy object created by
      //    Factory::NewUninitializedJSGlobalProxy.
      CHECK(IsNull(map()->native_context_or_null()) ||
            IsNativeContext(map()->native_context_or_null()));

    } else if (instance_type() == JS_SPECIAL_API_OBJECT_TYPE) {
      // 3) Remote Api objects' maps have the RO meta map (and thus are not
      //    tied to any native context) while all the other Api objects are
      //    tied to a native context.
      CHECK_IMPLIES(map() != GetReadOnlyRoots().meta_map(),
                    IsNativeContext(map()->native_context_or_null()));

    } else {
      // For all the other objects native context specific objects the
      // native context field must already be initialized.
      CHECK(IsNativeContext(map()->native_context_or_null()));
    }
  } else if (InstanceTypeChecker::IsAlwaysSharedSpaceJSObject(
                 instance_type())) {
    // Shared objects' maps must use the RO meta map.
    CHECK_EQ(map(), GetReadOnlyRoots().meta_map());
  }

#if V8_ENABLE_WEBASSEMBLY
  if (instance_type() == WASM_STRUCT_TYPE ||
      instance_type() == WASM_ARRAY_TYPE) {
    // Wasm structs are sometimes shared. In this case, the meta map of this map
    // has to be the context-free RO meta map.
    if (HeapLayout::InAnySharedSpace(this)) {
      CHECK_EQ(map(), GetReadOnlyRoots().meta_map());
    }
    // Wasm maps must have a WasmTypeInfo, which must contain all of their
    // supertype maps.
    CHECK(IsWasmTypeInfo(wasm_type_info()));
    wasm::CanonicalTypeIndex index = wasm_type_info()->type_index();
    wasm::TypeCanonicalizer* types = wasm::GetTypeCanonicalizer();
    uint8_t subtyping_depth = types->GetSubtypingDepth_Slow(index);
    CHECK_GE(wasm_type_info()->supertypes_length(), subtyping_depth);
    // Wasm maps with custom descriptors additionally cache their immediate
    // supertype.
    // Note: for each static type that has a descriptor, there is also a
    // canonical RTT that does not have one (and is not used by any actual
    // objects).
    if (types->has_descriptor(index) && IsWasmStruct(custom_descriptor())) {
      CHECK_GT(wasm_type_info()->supertypes_length(), subtyping_depth);
      CHECK_EQ(immediate_supertype_map(),
               wasm_type_info()->supertypes(subtyping_depth));
    }
    // For the time being, installing prototypes requires the "js interop" flag,
    // not just "custom descriptors".
    CHECK_IMPLIES(!IsNull(prototype()), v8_flags.experimental_wasm_js_interop);
  }
#endif

  if (IsJSObjectMap(this)) {
    int header_end_offset = JSObject::GetHeaderSize(this);
    int inobject_fields_start_offset = GetInObjectPropertyOffset(0);
    // Ensure that embedder fields are located exactly between header and
    // inobject properties.
    CHECK_EQ(header_end_offset, JSObject::GetEmbedderFieldsStartOffset(this));
    CHECK_EQ(header_end_offset +
                 JSObject::GetEmbedderFieldCount(this) * kEmbedderDataSlotSize,
             inobject_fields_start_offset);

    if (IsJSSharedStructMap(this) || IsJSSharedArrayMap(this) ||
        IsJSAtomicsMutex(this) || IsJSAtomicsCondition(this)) {
      if (COMPRESS_POINTERS_IN_MULTIPLE_CAGES_BOOL) {
        // TODO(v8:14089): Verify what should be checked in this configuration
        // and again merge with the else-branch below.
        // CHECK(InSharedHeap());
        CHECK(IsUndefined(GetBackPointer(), isolate));
        // Object maybe_cell = prototype_validity_cell(kRelaxedLoad);
        // if (maybe_cell.IsCell()) CHECK(maybe_cell.InSharedHeap());
        CHECK(!is_extensible());
        CHECK(!is_prototype_map());
        CHECK(OnlyHasSimpleProperties());
        // CHECK(instance_descriptors().InSharedHeap());
        if (IsJSSharedArrayMap(this)) {
          CHECK(has_shared_array_elements());
        }
      } else {
        CHECK(HeapLayout::InAnySharedSpace(this));
        CHECK(IsUndefined(GetBackPointer(), isolate));
        Tagged<Object> maybe_cell = prototype_validity_cell(kRelaxedLoad);
        if (IsCell(maybe_cell)) {
          CHECK(HeapLayout::InAnySharedSpace(Cast<Cell>(maybe_cell)));
        }
        CHECK(!is_extensible());
        CHECK(!is_prototype_map());
        CHECK(OnlyHasSimpleProperties());
        CHECK(HeapLayout::InAnySharedSpace(instance_descriptors()));
        if (IsJSSharedArrayMap(this)) {
          CHECK(has_shared_array_elements());
        }
      }
    }

    // Check constructor value in JSFunction's maps.
    if (IsJSFunctionMap(this) && !IsMap(constructor_or_back_pointer())) {
      Tagged<Object> maybe_constructor = constructor_or_back_pointer();
      // Constructor field might still contain a tuple if this map used to
      // have non-instance prototype earlier.
      CHECK_IMPLIES(has_non_instance_prototype(), IsTuple2(maybe_constructor));
      if (IsTuple2(maybe_constructor)) {
        Tagged<Tuple2> tuple = Cast<Tuple2>(maybe_constructor);
        // Unwrap the {constructor, non-instance_prototype} pair.
        maybe_constructor = tuple->value1();
        CHECK(!IsJSReceiver(tuple->value2()));
      }
      CHECK(IsJSFunction(maybe_constructor) ||
            IsFunctionTemplateInfo(maybe_constructor) ||
            // The above check might fail until empty function setup is done.
            IsUndefined(isolate->raw_native_context()->GetNoCell(
                Context::EMPTY_FUNCTION_INDEX)));
    }
  }

  if (!may_have_interesting_properties() && !is_wasm_struct) {
    CHECK(!has_named_interceptor());
    CHECK(!is_dictionary_map());
    CHECK(!is_access_check_needed());
    Tagged<DescriptorArray> const descriptors = instance_descriptors();
    for (InternalIndex i : IterateOwnDescriptors()) {
      CHECK(!descriptors->GetKey(i)->IsInteresting(isolate));
    }
  }
  CHECK_IMPLIES(has_named_interceptor(), may_have_interesting_properties());
  CHECK_IMPLIES(is_dictionary_map(), may_have_interesting_properties());
  CHECK_IMPLIES(is_dictionary_map(), owns_descriptors());
  CHECK_IMPLIES(is_access_check_needed(), may_have_interesting_properties());
  CHECK_IMPLIES(IsJSObjectMap(this) && !CanHaveFastTransitionableElementsKind(),
                IsDictionaryElementsKind(elements_kind()) ||
                    IsTerminalElementsKind(elements_kind()) ||
                    IsAnyHoleyNonextensibleElementsKind(elements_kind()) ||
                    IsSharedArrayElementsKind(elements_kind()));
  CHECK_IMPLIES(is_deprecated(), !is_stable());
  if (is_prototype_map()) {
    CHECK(prototype_info() == Smi::zero() ||
          IsPrototypeInfo(prototype_info()) ||
          IsPrototypeSharedClosureInfo(prototype_info()));
  }
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

void EmbedderDataArray::EmbedderDataArrayVerify(Isolate* isolate) {
  CHECK(IsEmbedderDataArray(this));
  CHECK(length_.load().IsSmi());
  EmbedderDataSlot start(this, 0);
  EmbedderDataSlot end(this, length());
  for (EmbedderDataSlot slot = start; slot < end; ++slot) {
    Tagged<Object> e = slot.load_tagged();
    Object::VerifyPointer(isolate, e);
  }
}

void FixedArrayBase::FixedArrayBaseVerify(Isolate* isolate) {
  CHECK_LE(length_, FixedArrayBase::kMaxLength);
}

void FixedArray::FixedArrayVerify(Isolate* isolate) {
  CHECK_LE(length_, FixedArray::kMaxLength);

  uint32_t len = ulength().value();
  for (uint32_t i = 0; i < len; ++i) {
    Object::VerifyPointer(isolate, get(i));
  }

  if (this == ReadOnlyRoots(isolate).empty_fixed_array()) {
    CHECK_EQ(len, 0);
    CHECK_EQ(map(), ReadOnlyRoots(isolate).fixed_array_map());
  }
}

void TrustedFixedArray::TrustedFixedArrayVerify(Isolate* isolate) {
  TrustedObjectVerify(isolate);
  CHECK_LE(length_, TrustedFixedArray::kMaxLength);

  uint32_t len = ulength().value();
  for (uint32_t i = 0; i < len; ++i) {
    Object::VerifyPointer(isolate, get(i));
  }
}

void ProtectedFixedArray::ProtectedFixedArrayVerify(Isolate* isolate) {
  TrustedObjectVerify(isolate);

  CHECK_LE(length_, ProtectedFixedArray::kMaxLength);

  uint32_t len = ulength().value();
  for (uint32_t i = 0; i < len; ++i) {
    Tagged<Object> element = get(i);
    CHECK(IsSmi(element) || IsTrustedObject(element));
    Object::VerifyPointer(isolate, element);
  }
}

void RegExpMatchInfo::RegExpMatchInfoVerify(Isolate* isolate) {
  const uint32_t cap = capacity().value();
  const uint32_t capture_registers =
      static_cast<uint32_t>(number_of_capture_registers());
  CHECK_GE(cap, kMinCapacity);
  CHECK_LE(cap, kMaxCapacity);
  CHECK_GE(capture_registers, kMinCapacity);
  CHECK_LE(capture_registers, cap);
  CHECK(IsString(last_subject()));
  Object::VerifyPointer(isolate, last_input());
  for (uint32_t i = 0; i < cap; ++i) {
    CHECK(IsSmi(get(i)));
  }
}

void FeedbackCell::FeedbackCellVerify(Isolate* isolate) {
  Tagged<Object> v = value();
  Object::VerifyPointer(isolate, v);
  CHECK(IsUndefined(v) || IsClosureFeedbackCellArray(v) || IsFeedbackVector(v));

  JSDispatchHandle handle = dispatch_handle();
  if (handle == kNullJSDispatchHandle) return;

  JSDispatchTable& jdt = isolate->js_dispatch_table();
  Tagged<Code> code = jdt.GetCode(handle);
  CodeKind kind = code->kind();
  CHECK(kind == CodeKind::FOR_TESTING_JS || kind == CodeKind::BUILTIN ||
        kind == CodeKind::INTERPRETED_FUNCTION || kind == CodeKind::BASELINE ||
        kind == CodeKind::MAGLEV || kind == CodeKind::TURBOFAN_JS);
}

void ClosureFeedbackCellArray::ClosureFeedbackCellArrayVerify(
    Isolate* isolate) {
  CHECK_LE(length_, kMaxCapacity);
  uint32_t len = ulength().value();
  for (uint32_t i = 0; i < len; ++i) {
    Object::VerifyPointer(isolate, get(i));
  }
}

void FeedbackVector::FeedbackVectorVerify(Isolate* isolate) {
  CHECK(IsFeedbackVector(this));
  // Strong header slots.
  Object::VerifyPointer(isolate, shared_function_info());
  CHECK(IsSharedFunctionInfo(shared_function_info()));
  Object::VerifyPointer(isolate, closure_feedback_cell_array());
  CHECK(IsClosureFeedbackCellArray(closure_feedback_cell_array()));
  Object::VerifyPointer(isolate, parent_feedback_cell());
  CHECK(IsFeedbackCell(parent_feedback_cell()));
  // Variable-length maybe-weak tail.
  const int len = length();
  for (int i = 0; i < len; ++i) {
    Tagged<MaybeObject> value = raw_feedback_slots()[i].Relaxed_Load();
    Object::VerifyMaybeObjectPointer(isolate, value);
  }
}

void WeakFixedArray::WeakFixedArrayVerify(Isolate* isolate) {
  CHECK_LE(length_, kMaxCapacity);
  uint32_t len = ulength().value();
  for (uint32_t i = 0; i < len; ++i) {
    Object::VerifyMaybeObjectPointer(isolate, get(i));
  }
}

void WeakHomomorphicFixedArray::WeakHomomorphicFixedArrayVerify(
    Isolate* isolate) {
  CHECK_LE(length_, kMaxCapacity);
  uint32_t len = ulength().value();
  for (uint32_t i = 0; i < len; ++i) {
    Object::VerifyMaybeObjectPointer(isolate, get(i));
  }
}

void TrustedWeakFixedArray::TrustedWeakFixedArrayVerify(Isolate* isolate) {
  CHECK_LE(length_, kMaxCapacity);
  uint32_t len = ulength().value();
  for (uint32_t i = 0; i < len; ++i) {
    Object::VerifyMaybeObjectPointer(isolate, get(i));
  }
}

void ProtectedWeakFixedArray::ProtectedWeakFixedArrayVerify(Isolate* isolate) {
  TrustedObjectVerify(isolate);
  CHECK_LE(length_, kMaxCapacity);
  uint32_t len = ulength().value();
  for (uint32_t i = 0; i < len; ++i) {
    Tagged<UnionOf<MaybeWeak<TrustedObject>, Smi>> p = get(i);
    Tagged<HeapObject> heap_object;
    if (p.GetHeapObject(&heap_object)) {
      // We could relax this, but for now we assume that strong pointers in a
      // weak fixed array are unintentional and should be reported.
      CHECK(p.IsWeak());
      CHECK(IsTrustedObject(heap_object));
      HeapObject::VerifyHeapPointer(isolate, heap_object);
    } else {
      CHECK(p.IsSmi() || p.IsCleared());
    }
  }
}

void ScriptContextTable::ScriptContextTableVerify(Isolate* isolate) {
  const uint32_t len = length(kAcquireLoad).value();
  CHECK_LE(len, capacity().value());
  CHECK_LE(capacity_, kMaxCapacity);
  CHECK(IsNameToIndexHashTable(names_to_context_index()));
  for (uint32_t i = 0; i < len; ++i) {
    Tagged<Context> o = get(i);
    Object::VerifyPointer(isolate, o);
    CHECK(IsContext(o));
    CHECK(o->IsScriptContext());
  }
}

void ArrayList::ArrayListVerify(Isolate* isolate) {
  CHECK_LE(capacity_, kMaxCapacity);
  const uint32_t len = ulength().value();
  const uint32_t cap = capacity().value();
  CHECK_LE(len, cap);
  CHECK_IMPLIES(cap == 0, this == ReadOnlyRoots(isolate).empty_array_list());
  for (uint32_t i = 0; i < cap; ++i) {
    Object::VerifyPointer(isolate, get(i));
  }
}

void WeakArrayList::WeakArrayListVerify(Isolate* isolate) {
  CHECK_LE(capacity_, kMaxCapacity);
  const uint32_t len = length().value();
  const uint32_t cap = capacity().value();
  CHECK_LE(len, cap);
  CHECK_IMPLIES(cap == 0,
                this == ReadOnlyRoots(isolate).empty_weak_array_list());
  for (uint32_t i = 0; i < cap; ++i) {
    Object::VerifyMaybeObjectPointer(isolate, get(i));
  }
}

void PropertyArray::PropertyArrayVerify(Isolate* isolate) {
  CHECK(IsPropertyArray(this));
  CHECK(length_and_hash_.load().IsSmi());
  const uint32_t len = length().value();
  // There are no empty PropertyArrays.
  if (len == 0) {
    CHECK_EQ(this, ReadOnlyRoots(isolate).empty_property_array());
    return;
  }
  for (uint32_t i = 0; i < len; i++) {
    Tagged<Object> e = get(i);
    Object::VerifyPointer(isolate, e);
  }
}

void ScopeInfo::ScopeInfoVerify(Isolate* isolate) {
  CHECK(IsScopeInfo(this));
  CHECK(parameter_count_.load().IsSmi());
  CHECK(context_local_count_.load().IsSmi());
  CHECK(position_info_start_.load().IsSmi());
  CHECK(position_info_end_.load().IsSmi());

  const uint32_t flags = Flags();
  const bool is_module =
      ScopeTypeBits::decode(flags) == ScopeType::MODULE_SCOPE;
  const int local_count = context_local_count();
  const bool has_hashtable = local_count >= kScopeInfoMaxInlinedLocalNamesSize;

  if (is_module) {
    CHECK_LE(0, module_variable_count());
  }

  if (has_hashtable) {
    CHECK(IsNameToIndexHashTable(context_local_names_hashtable()));
  } else {
    for (int i = 0; i < local_count; ++i) {
      CHECK(IsString(context_local_names(i)));
    }
  }

  for (int i = 0; i < local_count; ++i) {
    // context_local_infos is stored as a Smi (SmiTagged<VariableProperties>);
    // the accessor returns the decoded int.
    USE(context_local_infos(i));
  }

  if (HasSavedClassVariableBit::decode(flags)) {
    Tagged<Union<Name, Smi>> v = saved_class_variable_info();
    CHECK(IsSmi(v) || IsName(v));
  }

  if (FunctionVariableBits::decode(flags) != VariableAllocationInfo::NONE) {
    Tagged<Union<Smi, String>> name = function_variable_info_name();
    CHECK(IsString(name) || IsZero(name));
    USE(function_variable_info_context_or_stack_slot_index());
  }

  if (HasInferredFunctionNameBit::decode(flags)) {
    Tagged<Union<String, Undefined>> n = inferred_function_name();
    CHECK(IsString(n) || IsUndefined(n));
  }

  if (HasOuterScopeInfoBit::decode(flags)) {
    CHECK(IsScopeInfo(outer_scope_info()));
  }

  if (is_module) {
    CHECK(IsSourceTextModuleInfo(module_info()));
    const int mod_var_count = module_variable_count();
    for (int i = 0; i < mod_var_count; ++i) {
      CHECK(IsString(module_variables_name(i)));
      USE(module_variables_index(i));
      USE(module_variables_properties(i));
    }
  }

  if (SloppyEvalCanExtendVarsBit::decode(flags)) {
    CHECK(IsDependentCode(dependent_code()));
  }

  if (ScopeTypeBits::decode(flags) == ScopeType::FUNCTION_SCOPE) {
    USE(unused_parameter_bits());
  }
}

void ByteArray::ByteArrayVerify(Isolate* isolate) {}

void TrustedByteArray::TrustedByteArrayVerify(Isolate* isolate) {
  TrustedObjectVerify(isolate);
}

void FixedDoubleArray::FixedDoubleArrayVerify(Isolate* isolate) {
  uint32_t len = ulength().value();
  for (uint32_t i = 0; i < len; ++i) {
    if (!is_the_hole(i)
#ifdef V8_ENABLE_UNDEFINED_DOUBLE
        && !is_undefined(i)
#endif  // V8_ENABLE_UNDEFINED_DOUBLE
    ) {
      uint64_t value = get_representation(i);
      uint64_t unexpected =
          base::bit_cast<uint64_t>(std::numeric_limits<double>::quiet_NaN()) &
          uint64_t{0x7FF8000000000000};
      // Create implementation specific sNaN by inverting relevant bit.
      unexpected ^= uint64_t{0x0008000000000000};
      CHECK((value & uint64_t{0x7FF8000000000000}) != unexpected ||
            (value & uint64_t{0x0007FFFFFFFFFFFF}) == uint64_t{0});
    }
  }
}

void Context::ContextVerify(Isolate* isolate) {
  if (has_extension()) VerifyExtensionSlot(extension());
  CHECK(IsContext(this));
  CHECK(length_.load().IsSmi());
  for (int i = 0; i < length(); i++) {
    Object::VerifyPointer(isolate, elements()[i].load());
  }
}

void NativeContext::NativeContextVerify(Isolate* isolate) {
  ContextVerify(isolate);
  CHECK(retained_maps() == Smi::zero() || IsWeakArrayList(retained_maps()));
  CHECK_EQ(length(), NativeContext::NATIVE_CONTEXT_SLOTS);
  CHECK_EQ(kVariableSizeSentinel, map()->instance_size());
}

void ContextCell::ContextCellVerify(Isolate* isolate) {
  Tagged<Object> dep_code = dependent_code();
  Tagged<JSAny> tagged = tagged_value();
  int state_as_int = static_cast<int>(state());
  CHECK_GE(state_as_int, static_cast<int>(kConst));
  CHECK_LE(state_as_int, static_cast<int>(kDetached));
  CHECK(IsDependentCode(dep_code));
  Object::VerifyPointer(isolate, dep_code);
  Object::VerifyPointer(isolate, tagged);
}

void DoubleStringCache::DoubleStringCacheVerify(Isolate* isolate) {
  for (auto& e : *this) {
    Object::VerifyPointer(isolate, e.value_.load());
  }
}

void FeedbackMetadata::FeedbackMetadataVerify(Isolate* isolate) {
  if (slot_count() == 0 && create_closure_slot_count() == 0) {
    CHECK_EQ(ReadOnlyRoots(isolate).empty_feedback_metadata(), this);
  } else {
    DisallowGarbageCollection no_gc;
    FeedbackMetadataIterator iter(Tagged<FeedbackMetadata>(this), no_gc);
    while (iter.HasNext()) {
      iter.Next();
      FeedbackSlotKind kind = iter.kind();
      CHECK_NE(FeedbackSlotKind::kInvalid, kind);
      CHECK_GT(kFeedbackSlotKindCount, kind);
    }
  }
}

void StrongDescriptorArray::StrongDescriptorArrayVerify(Isolate* isolate) {
  CHECK(IsStrongDescriptorArray(this));
  DescriptorArrayVerify(isolate);
}

void DescriptorArray::DescriptorArrayEntryTypesVerify(Isolate* isolate) {
  // Header: enum_cache_ is strong.
  Object::VerifyPointer(isolate, enum_cache());
  CHECK(IsEnumCache(enum_cache()));
  // Descriptors tail: verify each (key, details, value) triple. Key and
  // details slots are always strong; only the value slot may be weak.
  const int nof = number_of_all_descriptors();
  for (int i = 0; i < nof; ++i) {
    const DescriptorArray::Entry& entry = entries()[i];
    Tagged<Object> key = entry.key.Relaxed_Load();
    Object::VerifyPointer(isolate, key);
    CHECK(IsName(key) || IsUndefined(key));
    Tagged<Object> details = entry.details.Relaxed_Load();
    Object::VerifyPointer(isolate, details);
    CHECK(IsSmi(details) || IsUndefined(details));
    Tagged<MaybeObject> value = entry.value.Relaxed_Load();
    Object::VerifyMaybeObjectPointer(isolate, value);
    if (value.IsCleared()) continue;
    if (value.IsWeak()) {
      CHECK(IsMap(value.GetHeapObjectAssumeWeak()));
    } else {
      Tagged<Object> strong = value.GetHeapObjectOrSmi();
      CHECK(IsSmi(strong) || IsHeapNumber(strong) || IsBigInt(strong) ||
            IsString(strong) || IsSymbol(strong) || IsBoolean(strong) ||
            IsNull(strong) || IsUndefined(strong) || IsJSReceiver(strong) ||
            IsNumberDictionary(strong) || IsAccessorInfo(strong) ||
            IsAccessorPair(strong) || IsClassPositions(strong));
    }
  }
}

void DescriptorArray::DescriptorArrayVerify(Isolate* isolate) {
  CHECK(IsDescriptorArray(this));
  DescriptorArrayEntryTypesVerify(isolate);
  if (number_of_all_descriptors() == 0) {
    CHECK_EQ(ReadOnlyRoots(isolate).empty_descriptor_array(), this);
    CHECK_EQ(0, number_of_all_descriptors());
    CHECK_EQ(0, number_of_descriptors());
    CHECK_EQ(ReadOnlyRoots(isolate).empty_enum_cache(), enum_cache());
  } else {
    CHECK_LT(0, number_of_all_descriptors());
    CHECK_LE(number_of_descriptors(), number_of_all_descriptors());

    // Check that properties with private symbols names are non-enumerable, and
    // that fields are in order.
    for (InternalIndex descriptor :
         InternalIndex::Range(number_of_descriptors())) {
      Tagged<Object> key =
          *(GetDescriptorSlot(descriptor.as_int()) + kEntryKeyIndex);
      // number_of_descriptors() may be out of sync with the actual descriptors
      // written during descriptor array construction.
      if (IsUndefined(key, isolate)) continue;
      PropertyDetails details = GetDetails(descriptor);
      if (Cast<Name>(key)->IsAnyPrivate()) {
        CHECK_NE(details.attributes() & DONT_ENUM, 0);
      }
      Tagged<MaybeObject> value = GetValue(descriptor);
      if (details.location() == PropertyLocation::kField) {
        if (details.is_in_object()) {
          CHECK_GE(details.field_offset(), JSObject::kHeaderSize / kTaggedSize);
        } else {
          CHECK_GE(details.field_offset(),
                   PropertyArray::OffsetOfElementAt(0) / kTaggedSize);
        }
        CHECK(value == FieldType::None() || value == FieldType::Any() ||
              IsMap(value.GetHeapObjectAssumeWeak()));
      } else {
        CHECK(!value.IsWeakOrCleared());
        CHECK(!IsMap(Cast<Object>(value)));
      }
    }
  }
}

void TransitionArray::TransitionArrayVerify(Isolate* isolate) {
  WeakFixedArrayVerify(isolate);
  CHECK_LE(static_cast<uint32_t>(LengthFor(number_of_transitions())),
           ulength().value());

  ReadOnlyRoots roots(isolate);
  Tagged<Map> owner;

  // Check all entries have the same owner
  for (int i = 0; i < number_of_transitions(); ++i) {
    Tagged<Map> target = GetTarget(i);
    Tagged<Map> parent = Cast<Map>(target->constructor_or_back_pointer());
    if (owner.is_null()) {
      parent = owner;
    } else {
      CHECK_EQ(parent, owner);
    }
  }
  // Check all entries have the same owner
  if (HasPrototypeTransitions()) {
    Tagged<WeakFixedArray> proto_trans = GetPrototypeTransitions();
    const uint32_t length =
        TransitionArray::NumberOfPrototypeTransitions(proto_trans);
    for (uint32_t i = 0; i < length; ++i) {
      const uint32_t index = TransitionArray::kProtoTransitionHeaderSize + i;
      Tagged<MaybeObject> maybe_target = proto_trans->get(index);
      Tagged<HeapObject> target;
      if (maybe_target.GetHeapObjectIfWeak(&target)) {
        Tagged<Map> parent =
            Cast<Map>(Cast<Map>(target)->constructor_or_back_pointer());
        if (owner.is_null()) {
          parent = Cast<Map>(target);
        } else {
          CHECK_EQ(parent, owner);
        }
      }
    }
  }
  // Check all entries are valid
  if (HasSideStepTransitions()) {
    Tagged<WeakFixedArray> side_trans = GetSideStepTransitions();
    for (uint32_t index = SideStepTransition::kFirstMapIdx;
         index <= SideStepTransition::kLastMapIdx; ++index) {
      Tagged<MaybeObject> maybe_target = side_trans->get(index);
      Tagged<HeapObject> target;
      if (maybe_target.GetHeapObjectIfWeak(&target)) {
        CHECK(IsMap(target));
        if (!owner.is_null()) {
          CHECK_EQ(target->map(), owner->map());
        }
      } else {
        CHECK(maybe_target == SideStepTransition::Unreachable ||
              maybe_target == SideStepTransition::Empty ||
              maybe_target.IsCleared());
      }
    }
    Tagged<MaybeObject> maybe_cell =
        side_trans->get(SideStepTransition::index_of(
            SideStepTransition::Kind::kObjectAssignValidityCell));
    Tagged<HeapObject> cell;
    if (maybe_cell.GetHeapObjectIfWeak(&cell)) {
      CHECK(IsCell(cell));
    } else {
      CHECK(maybe_cell == SideStepTransition::Empty || maybe_cell.IsCleared());
    }
  }
}

namespace {
void SloppyArgumentsElementsVerify(Isolate* isolate,
                                   Tagged<SloppyArgumentsElements> elements,
                                   Tagged<JSObject> holder) {
  elements->SloppyArgumentsElementsVerify(isolate);
  ElementsKind kind = holder->GetElementsKind();
  bool is_fast = kind == FAST_SLOPPY_ARGUMENTS_ELEMENTS;
  Tagged<Context> context_object = elements->context();
  Tagged<FixedArray> arg_elements = elements->arguments();
  const uint32_t elements_len = elements->ulength().value();
  const uint32_t arg_elements_len = arg_elements->ulength().value();
  if (arg_elements_len == 0) {
    CHECK(arg_elements == ReadOnlyRoots(isolate).empty_fixed_array());
    return;
  }
  ElementsAccessor* accessor;
  if (is_fast) {
    accessor = ElementsAccessor::ForKind(HOLEY_ELEMENTS);
  } else {
    accessor = ElementsAccessor::ForKind(DICTIONARY_ELEMENTS);
  }
  // The value of elements->length() can be overshooted during optimization,
  // for fast sloppy arguments take the minimum between the elements length and
  // the argument count as done in the built-in NewSloppyArgumentsElements.
  uint32_t mapped_elements_length =
      is_fast ? std::min(elements_len, arg_elements_len) : elements_len;
  const int context_object_len = context_object->length();
  CHECK_GE(context_object_len, 0);
  uint32_t nofMappedParameters = 0;
  for (uint32_t i = 0; i < mapped_elements_length; i++) {
    // Verify that each context-mapped argument is either the hole or a valid
    // Smi within context length range.
    Tagged<Object> mapped = elements->mapped_entries(i, kRelaxedLoad);
    if (IsTheHole(mapped, isolate)) {
      // Both slow and fast sloppy arguments can be holey. Ensure that the fixed
      // array backing the fast sloppy arguments is large enough.
      CHECK(!is_fast || i < arg_elements_len);
      continue;
    }
    int mappedIndex = Smi::ToInt(mapped);
    nofMappedParameters++;
    CHECK_LT(mappedIndex, context_object_len);
    Tagged<Object> value = context_object->GetNoCell(mappedIndex);
    CHECK(IsObject(value));
    // None of the context-mapped entries should exist in the arguments
    // elements.
    CHECK(!accessor->HasElement(isolate, holder, i, arg_elements));
  }
  for (uint32_t i = mapped_elements_length; i < elements_len; ++i) {
    // Ensure that any overshooted element is the hole
    CHECK(IsTheHole(elements->mapped_entries(i, kRelaxedLoad), isolate));
  }
  CHECK_LE(nofMappedParameters, static_cast<uint32_t>(context_object_len));
  if (is_fast) {
    CHECK_LE(nofMappedParameters, arg_elements_len);
  } else {
    CHECK(IsNumberDictionary(elements->arguments()));
  }
}
}  // namespace

void JSArgumentsObject::JSArgumentsObjectVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
  CHECK(IsJSArgumentsObject(this));
  if (IsSloppyArgumentsElementsKind(GetElementsKind())) {
    SloppyArgumentsElementsVerify(isolate,
                                  Cast<SloppyArgumentsElements>(elements()),
                                  Cast<JSObject>(Tagged<HeapObject>(this)));
  }
  Tagged<NativeContext> native_context = map()->map()->native_context();
  if (map() == native_context->GetNoCell(Context::SLOPPY_ARGUMENTS_MAP_INDEX) ||
      map() == native_context->GetNoCell(
                   Context::SLOW_ALIASED_ARGUMENTS_MAP_INDEX) ||
      map() == native_context->GetNoCell(
                   Context::FAST_ALIASED_ARGUMENTS_MAP_INDEX)) {
    VerifyObjectField(isolate, JSSloppyArgumentsObject::kLengthOffset);
    VerifyObjectField(isolate, JSSloppyArgumentsObject::kCalleeOffset);
  } else if (map() ==
             native_context->GetNoCell(Context::STRICT_ARGUMENTS_MAP_INDEX)) {
    VerifyObjectField(isolate, JSStrictArgumentsObject::kLengthOffset);
  }
}

void JSGeneratorObject::JSGeneratorObjectVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
}

void JSAsyncFunctionObject::JSAsyncFunctionObjectVerify(Isolate* isolate) {
  JSGeneratorObjectVerify(isolate);
}

void JSAsyncGeneratorObject::JSAsyncGeneratorObjectVerify(Isolate* isolate) {
  JSGeneratorObjectVerify(isolate);
}

void JSDate::JSDateVerify(Isolate* isolate) {
  JSObjectVerify(isolate);

  auto IsValidCachedField = [](Tagged<UnionOf<Smi, HeapNumber>> f) {
    return IsSmi(f) || IsNaN(f);
  };
  CHECK(IsValidCachedField(year()));
  CHECK(IsValidCachedField(month()));
  CHECK(IsValidCachedField(day()));
  CHECK(IsValidCachedField(weekday()));
  CHECK(IsValidCachedField(hour()));
  CHECK(IsValidCachedField(min()));
  CHECK(IsValidCachedField(sec()));
  CHECK(IsValidCachedField(cache_stamp()));

  if (IsSmi(month())) {
    int month = Smi::ToInt(this->month());
    CHECK(0 <= month && month <= 11);
  }
  if (IsSmi(day())) {
    int day = Smi::ToInt(this->day());
    CHECK(1 <= day && day <= 31);
  }
  if (IsSmi(hour())) {
    int hour = Smi::ToInt(this->hour());
    CHECK(0 <= hour && hour <= 23);
  }
  if (IsSmi(min())) {
    int min = Smi::ToInt(this->min());
    CHECK(0 <= min && min <= 59);
  }
  if (IsSmi(sec())) {
    int sec = Smi::ToInt(this->sec());
    CHECK(0 <= sec && sec <= 59);
  }
  if (IsSmi(weekday())) {
    int weekday = Smi::ToInt(this->weekday());
    CHECK(0 <= weekday && weekday <= 6);
  }
  if (IsSmi(cache_stamp())) {
    CHECK_LE(Smi::ToInt(cache_stamp()),
             Smi::ToInt(isolate->date_cache_stamp()));
  }
}

void String::StringVerify(Isolate* isolate) {
  PrimitiveHeapObjectVerify(isolate);
  CHECK(IsString(this));
  CHECK(length() >= 0 && length() <= Smi::kMaxValue);
  CHECK_IMPLIES(length() == 0, this == ReadOnlyRoots(isolate).empty_string());
  if (IsInternalizedString(this)) {
    CHECK(HasHashCode());
    CHECK(!HeapLayout::InYoungGeneration(this));
  }
  const uint32_t raw_hash = raw_hash_field();
  if (IsForwardingIndex(raw_hash)) {
    const int forwarding_index =
        Name::ForwardingIndexValueBits::decode(raw_hash);
    CHECK_LT(forwarding_index, isolate->string_forwarding_table()->size());
  }
}

void ConsString::ConsStringVerify(Isolate* isolate) {
  StringVerify(isolate);
  CHECK(IsConsString(this));
  CHECK_GE(length(), ConsString::kMinLength);
  CHECK(length() == first()->length() + second()->length());
  if (IsFlat()) {
    // A flat cons can only be created by String::SlowFlatten.
    // Afterwards, the first part may be externalized or internalized.
    CHECK(IsSeqString(first()) || IsExternalString(first()) ||
          IsThinString(first()));
  }
}

void ThinString::ThinStringVerify(Isolate* isolate) {
  StringVerify(isolate);
  CHECK(IsThinString(this));
  CHECK(!HasForwardingIndex(kAcquireLoad));
  CHECK(IsInternalizedString(actual()));
  CHECK(IsSeqString(actual()) || IsExternalString(actual()));
}

void SlicedString::SlicedStringVerify(Isolate* isolate) {
  StringVerify(isolate);
  CHECK(IsSlicedString(this));
  CHECK(!IsConsString(parent()));
  CHECK(!IsSlicedString(parent()));
#ifdef DEBUG
  if (!isolate->has_turbofan_string_builders()) {
    // Turbofan's string builder optimization can introduce SlicedString that
    // are less than SlicedString::kMinLength characters. Their live range and
    // scope are pretty limited, but they can be visible to the GC, which
    // shouldn't treat them as invalid.
    CHECK_GE(length(), SlicedString::kMinLength);
  }
#endif
}

void ExternalString::ExternalStringVerify(Isolate* isolate) {
  StringVerify(isolate);
  CHECK(IsExternalString(this));
}

void JSBoundFunction::JSBoundFunctionVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
  CHECK(IsCallable(this));
  CHECK_EQ(IsConstructor(this), IsConstructor(bound_target_function()));
}

void JSFunction::JSFunctionVerify(Isolate* isolate) {
  // This assertion exists to encourage updating this verification function if
  // new fields are added in the Torque class layout definition.
  static_assert(JSFunctionWithoutPrototype::kHeaderSize == 7 * kTaggedSize);
  static_assert(JSFunctionWithPrototype::kHeaderSize == 8 * kTaggedSize);

  CHECK(IsJSFunction(this));
  Object::VerifyPointer(isolate, shared());
  CHECK(IsSharedFunctionInfo(shared()));
  Object::VerifyPointer(isolate, context(kRelaxedLoad));
  CHECK(IsContext(context(kRelaxedLoad)));
  Object::VerifyPointer(isolate, raw_feedback_cell());
  CHECK(IsFeedbackCell(raw_feedback_cell()));
  Object::VerifyPointer(isolate, code(isolate));
  CHECK(IsCode(code(isolate)));
  CHECK(map()->is_callable());
  // Ensure that the function's meta map belongs to the same native context.
  CHECK_EQ(map()->map()->native_context_or_null(), native_context());

  JSDispatchTable& jdt = isolate->js_dispatch_table();
  JSDispatchHandle handle = dispatch_handle();
  CHECK_NE(handle, kNullJSDispatchHandle);
  uint16_t parameter_count = jdt.GetParameterCount(handle);
  CHECK_EQ(parameter_count,
           shared()->internal_formal_parameter_count_with_receiver());
  Tagged<Code> code_from_table = jdt.GetCode(handle);
  CHECK(code_from_table->parameter_count() == kDontAdaptArgumentsSentinel ||
        code_from_table->parameter_count() == parameter_count);
  CHECK(!code_from_table->marked_for_deoptimization());
  CHECK_IMPLIES(code_from_table->is_optimized_code(),
                code_from_table->js_dispatch_handle() != kNullJSDispatchHandle);

  // Currently, a JSFunction must have the same dispatch entry as its
  // FeedbackCell, unless the FeedbackCell has no entry.
  JSDispatchHandle feedback_cell_handle =
      raw_feedback_cell()->dispatch_handle();
  CHECK_EQ(raw_feedback_cell() == *isolate->factory()->many_closures_cell(),
           feedback_cell_handle == kNullJSDispatchHandle);
  if (feedback_cell_handle != kNullJSDispatchHandle) {
    CHECK_EQ(feedback_cell_handle, handle);
  }
  if (code_from_table->is_context_specialized()) {
    CHECK_EQ(raw_feedback_cell()->map(),
             ReadOnlyRoots(isolate).one_closure_cell_map());
  }

  // Verify the entrypoint corresponds to the code or a tiering builtin.
  Address entrypoint = jdt.GetEntrypoint(handle);
#define CASE(name, ...) \
  entrypoint == BUILTIN_CODE(isolate, name)->instruction_start() ||
  CHECK(BUILTIN_LIST_BASE_TIERING(CASE)
            entrypoint == code_from_table->instruction_start());
#undef CASE

  DirectHandle<JSFunction> function(this, isolate);
  LookupIterator it(isolate, function, isolate->factory()->prototype_string(),
                    LookupIterator::OWN_SKIP_INTERCEPTOR);
  if (IsJSFunctionWithPrototype(this)) {
    Object::VerifyPointer(
        isolate, Cast<JSFunctionWithPrototype>(this)->prototype_or_initial_map(
                     kAcquireLoad));
  }

  if (has_prototype_property()) {
    CHECK(it.IsFound());
    CHECK_EQ(LookupIterator::ACCESSOR, it.state());
    CHECK(IsAccessorInfo(*it.GetAccessors()));
  } else {
    CHECK(!it.IsFound() || it.state() != LookupIterator::ACCESSOR ||
          !IsAccessorInfo(*it.GetAccessors()));
  }

  CHECK_IMPLIES(shared()->HasBuiltinId(),
                Builtins::CheckFormalParameterCount(
                    shared()->builtin_id(), shared()->length(),
                    shared()->internal_formal_parameter_count_with_receiver()));
}

void JSFunctionWithoutPrototype::JSFunctionWithoutPrototypeVerify(
    Isolate* isolate) {
  Cast<JSFunction>(this)->JSFunctionVerify(isolate);
}

void JSFunctionWithPrototype::JSFunctionWithPrototypeVerify(Isolate* isolate) {
  Cast<JSFunction>(this)->JSFunctionVerify(isolate);
}

void JSWrappedFunction::JSWrappedFunctionVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
  CHECK(IsCallable(this));
  // Ensure that the function's meta map belongs to the same native context.
  CHECK_EQ(map()->map()->native_context_or_null(), context());
}

namespace {

bool ShouldVerifySharedFunctionInfoFunctionIndex(
    Tagged<SharedFunctionInfo> sfi) {
  if (!sfi->HasBuiltinId()) return true;
  switch (sfi->builtin_id()) {
    case Builtin::kPromiseCapabilityDefaultReject:
    case Builtin::kPromiseCapabilityDefaultResolve:
      // For these we manually set custom function indices.
      return false;
    default:
      return true;
  }
  UNREACHABLE();
}

}  // namespace

void SharedFunctionInfo::SharedFunctionInfoVerify(LocalIsolate* isolate) {
  ReadOnlyRoots roots(isolate);

  // As we're iterating the heap, we might see inconsistent (and unreachable)
  // SFIs that reference unpublished trusted objects. This can for example
  // happen in case Wasm module instantiation fails, in which case we
  // "unpublish" the trusted objects as they'd be inconsistent.
  if (HasUnpublishedTrustedData(isolate)) return;

  Tagged<Object> value = name_or_scope_info(kAcquireLoad);
  if (IsScopeInfo(value)) {
    CHECK(!Cast<ScopeInfo>(value)->IsEmpty());
    CHECK_NE(value, roots.empty_scope_info());
  }

#if V8_ENABLE_WEBASSEMBLY
  bool is_wasm = HasWasmExportedFunctionData(isolate) || HasAsmWasmData() ||
                 HasWasmCapiFunctionData(isolate) || HasWasmResumeData();
#else
  bool is_wasm = false;
#endif  // V8_ENABLE_WEBASSEMBLY
  CHECK(is_wasm || IsApiFunction() || HasBytecodeArray() || HasBuiltinId() ||
        HasUncompiledDataWithPreparseData(isolate) ||
        HasUncompiledDataWithoutPreparseData(isolate));

  {
    Tagged<HeapObject> script = this->script(kAcquireLoad);
    CHECK(IsUndefined(script, roots) || IsScript(script));
  }

  if (!is_compiled()) {
    CHECK(!HasFeedbackMetadata());
    CHECK(IsTheHole(outer_scope_info()) || IsScopeInfo(outer_scope_info()));
  } else if (HasBytecodeArray() && HasFeedbackMetadata()) {
    CHECK(IsFeedbackMetadata(feedback_metadata()));
  }

  if (HasBytecodeArray()) {
    CHECK_EQ(GetBytecodeArray(isolate)->parameter_count(),
             internal_formal_parameter_count_with_receiver());
  }

  if (ShouldVerifySharedFunctionInfoFunctionIndex(this)) {
    int expected_map_index =
        Context::FunctionMapIndex(language_mode(), kind(), HasSharedName());
    CHECK_EQ(expected_map_index, function_map_index());
  }

  Tagged<ScopeInfo> info = EarlyScopeInfo(kAcquireLoad);
  if (!info->IsEmpty()) {
    CHECK(kind() == info->function_kind());
    CHECK_EQ(internal::IsModule(kind()), info->scope_type() == MODULE_SCOPE);
  }

  if (IsApiFunction()) {
    CHECK(construct_as_builtin());
  } else if (!HasBuiltinId()) {
    CHECK(!construct_as_builtin());
  } else {
    if (builtin_id() != Builtin::kCompileLazy &&
#if V8_ENABLE_WEBASSEMBLY
        builtin_id() != Builtin::kWasmMethodWrapper &&
#endif  // V8_ENABLE_WEBASSEMBLY
        builtin_id() != Builtin::kEmptyFunction) {
      CHECK(construct_as_builtin());
    } else {
      CHECK(!construct_as_builtin());
    }
  }
  CHECK_IMPLIES(HasBuiltinId(),
                Builtins::CheckFormalParameterCount(
                    builtin_id(), length(),
                    internal_formal_parameter_count_with_receiver()));
}

void SharedFunctionInfo::SharedFunctionInfoVerify(Isolate* isolate) {
  // TODO(leszeks): Add a TorqueGeneratedClassVerifier for LocalIsolate.
  SharedFunctionInfoVerify(isolate->AsLocalIsolate());
}

void SharedFunctionInfoWrapper::SharedFunctionInfoWrapperVerify(
    Isolate* isolate) {
  Object::VerifyPointer(isolate, shared_info());
}

void InterpreterData::InterpreterDataVerify(Isolate* isolate) {
  CHECK(IsInterpreterData(this));
}

void JSGlobalProxy::JSGlobalProxyVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
  CHECK(IsJSGlobalProxy(this));
  CHECK(map()->is_access_check_needed());
  // Make sure that this object has no properties, elements.
  CHECK_EQ(0, Cast<FixedArray>(elements())->ulength().value());
}

void JSGlobalObject::JSGlobalObjectVerify(Isolate* isolate) {
  Tagged<JSGlobalObject> self(this);
  CHECK(IsJSGlobalObject(self));
  // Do not check the dummy global object for the builtins.
  if (global_dictionary(kAcquireLoad)->NumberOfElements() == 0 &&
      elements()->ulength().value() == 0) {
    return;
  }
  Cast<JSObject>(self)->JSObjectVerify(isolate);
}

void PrimitiveHeapObject::PrimitiveHeapObjectVerify(Isolate* isolate) {
  CHECK(IsPrimitiveHeapObject(this));
}

void HeapNumber::HeapNumberVerify(Isolate* isolate) {
  PrimitiveHeapObjectVerify(isolate);
  CHECK(IsHeapNumber(this));
}

void Oddball::OddballVerify(Isolate* isolate) {
  PrimitiveHeapObjectVerify(isolate);
  CHECK(IsOddball(this));

  Heap* heap = isolate->heap();
  Tagged<Object> string = to_string();
  Object::VerifyPointer(isolate, string);
  CHECK(IsString(string));
  Tagged<Object> type = type_of();
  Object::VerifyPointer(isolate, type);
  CHECK(IsString(type));
  Tagged<Object> kind_value = kind_.load();
  Object::VerifyPointer(isolate, kind_value);
  CHECK(IsSmi(kind_value));

  Tagged<Object> number = to_number();
  Object::VerifyPointer(isolate, number);
  CHECK(IsSmi(number) || IsHeapNumber(number));
  if (IsHeapObject(number)) {
    CHECK(number == ReadOnlyRoots(heap).nan_value() ||
          number == ReadOnlyRoots(heap).hole_nan_value() ||
          number == ReadOnlyRoots(heap).undefined_nan_value());
  } else {
    CHECK(IsSmi(number));
    int value = Smi::ToInt(number);
    // Hidden oddballs have negative smis.
    const int kLeastHiddenOddballNumber = -7;
    CHECK_LE(value, 1);
    CHECK_GE(value, kLeastHiddenOddballNumber);
  }

  ReadOnlyRoots roots(heap);
  if (map() == roots.undefined_map()) {
    CHECK(this == roots.undefined_value());
  } else if (map() == roots.null_map()) {
    CHECK(this == roots.null_value());
  } else if (map() == roots.boolean_map()) {
    CHECK(this == roots.true_value() || this == roots.false_value());
  } else {
    UNREACHABLE();
  }
}

void Hole::HoleVerify(Isolate* isolate) {
  ReadOnlyRoots roots(isolate->heap());
  CHECK_EQ(map(), roots.hole_map());

#define COMPARE_ROOTS_VALUE(_, Value, __) \
  if (this == roots.Value()) {            \
    return;                               \
  }
  HOLE_LIST(COMPARE_ROOTS_VALUE);
#undef COMPARE_ROOTS_VALUE

  UNREACHABLE();
}

void Foreign::ForeignVerify(Isolate* isolate) { CHECK(IsForeign(this)); }

void TrustedForeign::TrustedForeignVerify(Isolate* isolate) {
  CHECK(IsTrustedForeign(this));
}

void Cell::CellVerify(Isolate* isolate) {
  CHECK(IsCell(this));
  Object::VerifyMaybeObjectPointer(isolate, maybe_value());
}

void MegaDomHandler::MegaDomHandlerVerify(Isolate* isolate) {
  CHECK(IsMegaDomHandler(this));
  Object::VerifyMaybeObjectPointer(isolate, accessor());
  Object::VerifyMaybeObjectPointer(isolate, context());
}

void PropertyCell::PropertyCellVerify(Isolate* isolate) {
  CHECK(IsPropertyCell(this));
  Object::VerifyPointer(isolate, name_.load());
  Object::VerifyPointer(isolate, property_details_raw_.load());
  Object::VerifyPointer(isolate, value_.load());
  Object::VerifyPointer(isolate, dependent_code_.load());
  CHECK(IsUniqueName(name()));
  CheckDataIsCompatible(property_details(), value());
}

void TrustedObject::TrustedObjectVerify(Isolate* isolate) {
#if defined(V8_ENABLE_SANDBOX)
  // All trusted objects must live in trusted space.
  // TODO(saelo): Some objects are trusted but do not yet live in trusted space.
  CHECK(TrustedHeapLayout::InTrustedSpace(this) || IsCode(this));
#endif
}

void ExposedTrustedObject::ExposedTrustedObjectVerify(Isolate* isolate) {
  TrustedObjectVerify(isolate);
#if defined(V8_ENABLE_SANDBOX)
  // Check that the self indirect pointer is consistent, i.e. points back to
  // this object.
  IndirectPointerTag tag;
  if (IsPublished(isolate)) {
    InstanceType instance_type = map()->instance_type();
    SharedFlag shared = SharedFlag(HeapLayout::InAnySharedSpace(this));
    tag = IndirectPointerTagFromInstanceType(instance_type, shared);
  } else {
    tag = kUnpublishedIndirectPointerTag;
  }
  // We can't use ReadIndirectPointerField here because the tag is not a
  // compile-time constant.
  IndirectPointerSlot slot = RawIndirectPointerField(
      offsetof(ExposedTrustedObject, self_indirect_pointer_), tag);
  Tagged<Object> self = slot.load(isolate);
  CHECK_EQ(self, this);
  // If the object is in the read-only space, the self indirect pointer entry
  // must be in the read-only segment, and vice versa.
  if (tag == kCodeIndirectPointerTag) {
    CodePointerTable::Space* space =
        IsolateForSandbox(isolate).GetCodePointerTableSpaceFor(slot.address());
    // During snapshot creation, the code pointer space of the read-only heap is
    // not marked as an internal read-only space.
    bool is_space_read_only =
        space == isolate->read_only_heap()->code_pointer_space();
    CHECK_EQ(is_space_read_only, HeapLayout::InReadOnlySpace(this));
  } else {
    CHECK(!HeapLayout::InReadOnlySpace(this));
  }
#endif
}

void Code::CodeVerify(Isolate* isolate) {
  ExposedTrustedObjectVerify(isolate);
  CHECK(IsCode(this));
  if (has_instruction_stream()) {
    Tagged<InstructionStream> istream = instruction_stream();
    CHECK_EQ(istream->code(kAcquireLoad), Tagged{this});
    CHECK_EQ(safepoint_table_offset(), 0);
    CHECK_LE(safepoint_table_offset(), handler_table_offset());
    CHECK_LE(handler_table_offset(), constant_pool_offset());
    CHECK_LE(constant_pool_offset(), code_comments_offset());
    CHECK_LE(code_comments_offset(), unwinding_info_offset());
    CHECK_LE(unwinding_info_offset(), metadata_size());

    // Ensure the cached code entry point corresponds to the InstructionStream
    // object associated with this Code.
#if defined(V8_COMPRESS_POINTERS) && defined(V8_SHORT_BUILTIN_CALLS)
    if (istream->instruction_start() == instruction_start()) {
      // Most common case, all good.
    } else {
      // When shared pointer compression cage is enabled and it has the
      // embedded code blob copy then the
      // InstructionStream::instruction_start() might return the address of
      // the remapped builtin regardless of whether the builtins copy existed
      // when the instruction_start value was cached in the Code (see
      // InstructionStream::OffHeapInstructionStart()).  So, do a reverse
      // Code object lookup via instruction_start value to ensure it
      // corresponds to this current Code object.
      Tagged<Code> lookup_result =
          isolate->heap()->FindCodeForInnerPointer(instruction_start());
      CHECK_EQ(lookup_result, Tagged{this});
    }
#else
    CHECK_EQ(istream->instruction_start(), instruction_start());
#endif  // V8_COMPRESS_POINTERS && V8_SHORT_BUILTIN_CALLS
  }

  // Our wrapper must point back to us.
  CHECK_EQ(wrapper()->code(isolate), Tagged{this});
}

void CodeWrapper::CodeWrapperVerify(Isolate* isolate) {
  if (!this->has_code()) return;
  auto code = this->code(isolate);
  Object::VerifyPointer(isolate, code);
  CHECK_EQ(code->wrapper(), this);
}

void InstructionStream::InstructionStreamVerify(Isolate* isolate) {
  TrustedObjectVerify(isolate);
  Tagged<Code> code;
  if (!TryGetCode(&code, kAcquireLoad)) return;
  CHECK(
      IsAligned(code->instruction_size(),
                static_cast<unsigned>(InstructionStream::kMetadataAlignment)));
  Tagged<InstructionStream> self(this);
#if (!defined(_MSC_VER) || defined(__clang__)) && !defined(V8_OS_ZOS)
  // See also: PlatformEmbeddedFileWriterWin::AlignToCodeAlignment
  //      and: PlatformEmbeddedFileWriterZOS::AlignToCodeAlignment
  CHECK_IMPLIES(!ReadOnlyHeap::Contains(self),
                IsAligned(instruction_start(), kCodeAlignment));
#endif  // (!defined(_MSC_VER) || defined(__clang__)) && !defined(V8_OS_ZOS)
  CHECK_IMPLIES(!ReadOnlyHeap::Contains(self),
                IsAligned(instruction_start(), kCodeAlignment));
  CHECK_EQ(self, code->instruction_stream());
  CHECK(Size() <= MemoryChunkLayout::MaxRegularCodeObjectSize() ||
        isolate->heap()->InSpace(self, CODE_LO_SPACE));
  Address last_gc_pc = kNullAddress;

  Object::ObjectVerify(relocation_info(), isolate);

  if (!code->marked_for_deoptimization()) {
    for (RelocIterator it(code); !it.done(); it.next()) {
      it.rinfo()->Verify(isolate);
      // Ensure that GC will not iterate twice over the same pointer.
      if (RelocInfo::IsGCRelocMode(it.rinfo()->rmode())) {
        CHECK(it.rinfo()->pc() != last_gc_pc);
        last_gc_pc = it.rinfo()->pc();
      }
    }
  }
}

void JSArray::JSArrayVerify(Isolate* isolate) {
  Tagged<JSObject> self = Cast<JSObject>(this);
  self->JSObjectVerify(isolate);
  CHECK(IsJSArray(this));
  Object::VerifyPointer(isolate, length());
  CHECK(IsSmi(length()) || IsHeapNumber(length()));
  // If a GC was caused while constructing this array, the elements
  // pointer may point to a one pointer filler map.
  if (!self->ElementsAreSafeToExamine()) return;
  if (IsUndefined(elements(), isolate)) return;
  CHECK(IsFixedArray(elements()) || IsFixedDoubleArray(elements()));
  uint32_t elements_len = elements()->ulength().value();
  if (elements_len == 0) {
    CHECK_EQ(elements(), ReadOnlyRoots(isolate).empty_fixed_array());
  }
  // Verify that the length and the elements backing store are in sync.
  if (IsSmi(length()) &&
      (self->HasFastElements() || self->HasAnyNonextensibleElements())) {
    if (elements_len > 0) {
      CHECK_IMPLIES(self->HasDoubleElements(), IsFixedDoubleArray(elements()));
      CHECK_IMPLIES(
          self->HasSmiOrObjectElements() || self->HasAnyNonextensibleElements(),
          IsFixedArray(elements()));
    }
    uint32_t size = Smi::ToUInt(length());
    // Holey / Packed backing stores might have slack or might have not been
    // properly initialized yet.
    CHECK(size <= elements_len ||
          elements() == ReadOnlyRoots(isolate).empty_fixed_array());
  } else {
    CHECK(self->HasDictionaryElements());
    uint32_t array_length;
    CHECK(Object::ToArrayLength(length(), &array_length));
    if (array_length == 0xFFFFFFFF) {
      CHECK(Object::ToArrayLength(length(), &array_length));
    }
    if (array_length != 0) {
      Tagged<NumberDictionary> dict = Cast<NumberDictionary>(elements());
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
  JSObjectVerify(isolate);
  CHECK(IsOrderedHashSet(table()) || IsUndefined(table(), isolate));
  // TODO(arv): Verify OrderedHashTable too.
}

void JSMap::JSMapVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
  CHECK(IsOrderedHashMap(table()) || IsUndefined(table(), isolate));
  // TODO(arv): Verify OrderedHashTable too.
}

void JSCollectionIterator::JSCollectionIteratorVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
  CHECK(IsJSCollectionIterator(Tagged<JSCollectionIterator>(this)));
  VerifyObjectField(isolate, offsetof(JSCollectionIterator, table_));
  VerifyObjectField(isolate, offsetof(JSCollectionIterator, index_));
}

void JSSetIterator::JSSetIteratorVerify(Isolate* isolate) {
  Tagged<JSSetIterator> self(this);
  CHECK(IsJSSetIterator(self));
  JSCollectionIteratorVerify(isolate);
  CHECK(IsOrderedHashSet(table()));
  CHECK(IsSmi(index()));
}

void JSMapIterator::JSMapIteratorVerify(Isolate* isolate) {
  Tagged<JSMapIterator> self(this);
  CHECK(IsJSMapIterator(self));
  JSCollectionIteratorVerify(isolate);
  CHECK(IsOrderedHashMap(table()));
  CHECK(IsSmi(index()));
}

void JSShadowRealm::JSShadowRealmVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
  CHECK(IsNativeContext(native_context()));
}

#ifdef V8_INTL_SUPPORT
void JSLocale::JSLocaleVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
  CHECK(IsForeign(icu_locale()));
}

void JSCollator::JSCollatorVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
  CHECK(IsForeign(icu_collator()));
  CHECK(IsUndefined(bound_compare()) || IsJSFunction(bound_compare()));
  CHECK(IsString(locale()));
}

void JSV8BreakIterator::JSV8BreakIteratorVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
  CHECK(IsString(locale()));
  CHECK(IsForeign(icu_iterator_with_text_.load()));
  CHECK(IsUndefined(bound_adopt_text()) || IsJSFunction(bound_adopt_text()));
  CHECK(IsUndefined(bound_first()) || IsJSFunction(bound_first()));
  CHECK(IsUndefined(bound_next()) || IsJSFunction(bound_next()));
  CHECK(IsUndefined(bound_current()) || IsJSFunction(bound_current()));
  CHECK(IsUndefined(bound_break_type()) || IsJSFunction(bound_break_type()));
}

void JSDateTimeFormat::JSDateTimeFormatVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
  CHECK(IsString(locale()));
  CHECK(IsForeign(icu_locale_.load()));
  CHECK(IsForeign(icu_simple_date_format_.load()));
  CHECK(IsForeign(icu_date_interval_format_.load()));
  CHECK(IsUndefined(bound_format()) || IsJSFunction(bound_format()));
  CHECK(IsSmi(flags_.load()));
}

void JSDisplayNames::JSDisplayNamesVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
  CHECK(IsForeign(internal_.load()));
  CHECK(IsSmi(flags_.load()));
}

void JSDurationFormat::JSDurationFormatVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
  CHECK(IsSmi(style_flags_.load()));
  CHECK(IsSmi(display_flags_.load()));
  CHECK(IsForeign(icu_locale_.load()));
  CHECK(IsForeign(icu_number_formatter_.load()));
}

void JSListFormat::JSListFormatVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
  CHECK(IsString(locale()));
  CHECK(IsForeign(icu_formatter_.load()));
  CHECK(IsSmi(flags_.load()));
}

void JSNumberFormat::JSNumberFormatVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
  CHECK(IsString(locale()));
  CHECK(IsForeign(icu_number_formatter_.load()));
  CHECK(IsUndefined(bound_format()) || IsJSFunction(bound_format()));
}

void JSPluralRules::JSPluralRulesVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
  CHECK(IsString(locale()));
  CHECK(IsSmi(flags_.load()));
  CHECK(IsForeign(icu_plural_rules_.load()));
  CHECK(IsForeign(icu_number_formatter_.load()));
}

void JSRelativeTimeFormat::JSRelativeTimeFormatVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
  CHECK(IsString(locale()));
  CHECK(IsString(numberingSystem()));
  CHECK(IsForeign(icu_formatter_.load()));
  CHECK(IsSmi(flags_.load()));
}

void JSSegmenter::JSSegmenterVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
  CHECK(IsString(locale()));
  CHECK(IsForeign(icu_break_iterator_.load()));
  CHECK(IsSmi(flags_.load()));
}

void JSSegments::JSSegmentsVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
  CHECK(IsForeign(icu_iterator_with_text_.load()));
  CHECK(IsString(raw_string()));
  CHECK(IsSmi(flags_.load()));
}

void JSSegmentIterator::JSSegmentIteratorVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
  CHECK(IsForeign(icu_iterator_with_text_.load()));
  CHECK(IsString(raw_string()));
  CHECK(IsSmi(flags_.load()));
}

void JSSegmentDataObject::JSSegmentDataObjectVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
  CHECK(IsString(segment()));
  CHECK(IsSmi(index_.load()) || IsHeapNumber(index_.load()));
  CHECK(IsString(input()));
}

void JSSegmentDataObjectWithIsWordLike::JSSegmentDataObjectWithIsWordLikeVerify(
    Isolate* isolate) {
  JSSegmentDataObjectVerify(isolate);
  CHECK(IsBoolean(is_word_like()));
}
#endif  // V8_INTL_SUPPORT

namespace {

void VerifyElementIsShared(Tagged<Object> element) {
  // Exception for ThinStrings:
  // When storing a ThinString in a shared object, we want to store the actual
  // string, which is shared when sharing the string table.
  // It is possible that a stored shared string migrates to a ThinString later
  // on, which is fine as the ThinString resides in shared space if the original
  // string was in shared space.
  if (IsThinString(element)) {
    CHECK(v8_flags.shared_string_table);
    CHECK(HeapLayout::InWritableSharedSpace(Cast<ThinString>(element)));
  } else {
    CHECK(IsShared(element));
  }
}

}  // namespace

void JSSharedStruct::JSSharedStructVerify(Isolate* isolate) {
  CHECK(IsJSSharedStruct(this));
  CHECK(HeapLayout::InWritableSharedSpace(Tagged<HeapObject>(this)));
  JSObjectVerify(isolate);
  CHECK(HasFastProperties());
  // Shared structs can only point to primitives or other shared HeapObjects,
  // even internally.
  Tagged<Map> struct_map = map();
  CHECK(HeapLayout::InAnySharedSpace(property_array()));
  Tagged<DescriptorArray> descriptors = struct_map->instance_descriptors();
  for (InternalIndex i : struct_map->IterateOwnDescriptors()) {
    PropertyDetails details = descriptors->GetDetails(i);
    CHECK_EQ(PropertyKind::kData, details.kind());

    if (JSSharedStruct::IsRegistryKeyDescriptor(isolate, struct_map, i)) {
      CHECK_EQ(PropertyLocation::kDescriptor, details.location());
      CHECK(IsInternalizedString(descriptors->GetStrongValue(i)));
    } else if (JSSharedStruct::IsElementsTemplateDescriptor(isolate, struct_map,
                                                            i)) {
      CHECK_EQ(PropertyLocation::kDescriptor, details.location());
      CHECK(IsNumberDictionary(descriptors->GetStrongValue(i)));
    } else {
      CHECK_EQ(PropertyLocation::kField, details.location());
      CHECK(details.representation().IsTagged());
      CHECK(!IsNumberDictionary(descriptors->GetStrongValue(i)));
      CHECK(!IsInternalizedString(descriptors->GetStrongValue(i)));
      FieldIndex field_index = FieldIndex::ForDetails(struct_map, details);
      VerifyElementIsShared(RawFastPropertyAt(field_index));
    }
  }
}

void JSAtomicsMutex::JSAtomicsMutexVerify(Isolate* isolate) {
  CHECK(IsJSAtomicsMutex(this));
  CHECK(HeapLayout::InWritableSharedSpace(Tagged<HeapObject>(this)));
  JSObjectVerify(isolate);
}

void JSAtomicsCondition::JSAtomicsConditionVerify(Isolate* isolate) {
  CHECK(IsJSAtomicsCondition(this));
  CHECK(HeapLayout::InAnySharedSpace(Tagged<HeapObject>(this)));
  JSObjectVerify(isolate);
}

void JSDisposableStackBase::JSDisposableStackBaseVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
  const int len = length();
  const uint32_t cap = stack()->capacity().value();
  CHECK_EQ(len % 3, 0);
  CHECK_GE(cap, static_cast<uint32_t>(len));
}

void JSSyncDisposableStack::JSSyncDisposableStackVerify(Isolate* isolate) {
  JSDisposableStackBaseVerify(isolate);
}

void JSAsyncDisposableStack::JSAsyncDisposableStackVerify(Isolate* isolate) {
  JSDisposableStackBaseVerify(isolate);
}

void JSSharedArray::JSSharedArrayVerify(Isolate* isolate) {
  CHECK(IsJSSharedArray(this));
  JSObjectVerify(isolate);
  CHECK(HasFastProperties());
  // Shared arrays can only point to primitives or other shared HeapObjects,
  // even internally.
  Tagged<FixedArray> storage = Cast<FixedArray>(elements());
  uint32_t length = storage->ulength().value();
  for (uint32_t j = 0; j < length; j++) {
    Tagged<Object> element_value = storage->get(j);
    VerifyElementIsShared(element_value);
  }
}

// TODO(42203505): Review and improve the verifiers for the hierarchy of
// iterator helpers.

void JSIteratorHelper::JSIteratorHelperVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
}

void JSIteratorHelperSimple::JSIteratorHelperSimpleVerify(Isolate* isolate) {
  JSIteratorHelperVerify(isolate);
}

void JSIteratorMapHelper::JSIteratorMapHelperVerify(Isolate* isolate) {
  JSIteratorHelperSimpleVerify(isolate);
  CHECK(IsCallable(mapper()));
  CHECK_GE(Object::NumberValue(counter()), 0);
}

void JSIteratorFilterHelper::JSIteratorFilterHelperVerify(Isolate* isolate) {
  JSIteratorHelperSimpleVerify(isolate);
  CHECK(IsCallable(predicate()));
  CHECK_GE(Object::NumberValue(counter()), 0);
}

void JSIteratorTakeHelper::JSIteratorTakeHelperVerify(Isolate* isolate) {
  JSIteratorHelperSimpleVerify(isolate);
  CHECK_GE(Object::NumberValue(remaining()), 0);
}

void JSIteratorDropHelper::JSIteratorDropHelperVerify(Isolate* isolate) {
  JSIteratorHelperSimpleVerify(isolate);
  CHECK_GE(Object::NumberValue(remaining()), 0);
}

void JSIteratorFlatMapHelper::JSIteratorFlatMapHelperVerify(Isolate* isolate) {
  JSIteratorHelperSimpleVerify(isolate);
  CHECK(IsCallable(mapper()));
  CHECK_GE(Object::NumberValue(counter()), 0);
}

void JSIteratorConcatHelper::JSIteratorConcatHelperVerify(Isolate* isolate) {
  JSIteratorHelperSimpleVerify(isolate);
}

void JSIteratorZipHelper::JSIteratorZipHelperVerify(Isolate* isolate) {
  JSIteratorHelperVerify(isolate);
}

void JSIteratorZipKeyedHelper::JSIteratorZipKeyedHelperVerify(
    Isolate* isolate) {
  JSIteratorZipHelperVerify(isolate);
}

void WeakCell::WeakCellVerify(Isolate* isolate) {
  CHECK(IsWeakCell(this));

  CHECK(IsUndefined(target(), isolate) || Object::CanBeHeldWeakly(target()));

  CHECK(IsWeakCell(prev()) || IsUndefined(prev(), isolate));
  if (IsWeakCell(prev())) {
    CHECK_EQ(Cast<WeakCell>(prev())->next(), this);
  }

  CHECK(IsWeakCell(next()) || IsUndefined(next(), isolate));
  if (IsWeakCell(next())) {
    CHECK_EQ(Cast<WeakCell>(next())->prev(), this);
  }

  CHECK_IMPLIES(IsUndefined(unregister_token(), isolate),
                IsUndefined(key_list_prev(), isolate));
  CHECK_IMPLIES(IsUndefined(unregister_token(), isolate),
                IsUndefined(key_list_next(), isolate));

  CHECK(IsWeakCell(key_list_prev()) || IsUndefined(key_list_prev(), isolate));

  CHECK(IsWeakCell(key_list_next()) || IsUndefined(key_list_next(), isolate));

  CHECK(IsUndefined(finalization_registry(), isolate) ||
        IsJSFinalizationRegistry(finalization_registry()));
}

void JSWeakRef::JSWeakRefVerify(Isolate* isolate) {
  CHECK(IsJSWeakRef(this));
  JSObjectVerify(isolate);
  CHECK(IsUndefined(target(), isolate) || Object::CanBeHeldWeakly(target()));
}

void JSFinalizationRegistry::JSFinalizationRegistryVerify(Isolate* isolate) {
  CHECK(IsJSFinalizationRegistry(this));
  JSObjectVerify(isolate);
  if (IsWeakCell(active_cells())) {
    CHECK(IsUndefined(Cast<WeakCell>(active_cells())->prev(), isolate));
  }
  if (IsWeakCell(cleared_cells())) {
    CHECK(IsUndefined(Cast<WeakCell>(cleared_cells())->prev(), isolate));
  }
}

void AccessCheckInfo::AccessCheckInfoVerify(Isolate* isolate) {
  Object::VerifyPointer(isolate, callback());
  Object::VerifyPointer(isolate, named_interceptor());
  Object::VerifyPointer(isolate, indexed_interceptor());
  Object::VerifyPointer(isolate, data());
}

void JSWeakMap::JSWeakMapVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
  CHECK(IsEphemeronHashTable(table()) || IsUndefined(table(), isolate));
}

void ScriptOrModule::ScriptOrModuleVerify(Isolate* isolate) {
  Object::VerifyPointer(isolate, resource_name());
  Object::VerifyPointer(isolate, host_defined_options());
}

void JSArrayIterator::JSArrayIteratorVerify(Isolate* isolate) {
  JSObjectVerify(isolate);

  CHECK_GE(Object::NumberValue(next_index()), 0);
  CHECK_LE(Object::NumberValue(next_index()), kMaxSafeInteger);

  if (IsJSTypedArray(iterated_object())) {
    // JSTypedArray::length is limited to Smi range.
    CHECK_LE(Object::NumberValue(next_index()), kMaxSafeInteger);
  } else if (IsJSArray(iterated_object())) {
    // JSArray::length is limited to Uint32 range.
    CHECK_LE(Object::NumberValue(next_index()), kMaxUInt32);
  }
}

void JSStringIterator::JSStringIteratorVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
  CHECK(IsString(string()));
  CHECK(index_.load().IsSmi());
  CHECK_GE(index(), 0);
  CHECK_LE(index(), String::kMaxLength);
}

void JSAsyncFromSyncIterator::JSAsyncFromSyncIteratorVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
  CHECK(IsJSReceiver(sync_iterator()));
}

void JSValidIteratorWrapper::JSValidIteratorWrapperVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
  CHECK(IsJSReceiver(underlying_object()));
}

void JSRegExpStringIterator::JSRegExpStringIteratorVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
  CHECK(IsJSReceiver(iterating_reg_exp()));
  CHECK(IsString(iterated_string()));
  CHECK(flags_.load().IsSmi());
}

void JSPrimitiveWrapper::JSPrimitiveWrapperVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
}

void JSWeakSet::JSWeakSetVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
  CHECK(IsEphemeronHashTable(table()) || IsUndefined(table(), isolate));
}

void CallbackTask::CallbackTaskVerify(Isolate* isolate) {
  MicrotaskVerify(isolate);
  Object::VerifyPointer(isolate, callback());
  Object::VerifyPointer(isolate, data());
}

void CallableTask::CallableTaskVerify(Isolate* isolate) {
  MicrotaskVerify(isolate);
  Object::VerifyPointer(isolate, callable());
  Object::VerifyPointer(isolate, context());
}

void AsyncResumeTask::AsyncResumeTaskVerify(Isolate* isolate) {
  MicrotaskVerify(isolate);
  Object::VerifyPointer(isolate, generator());
  Object::VerifyPointer(isolate, value());
  CHECK_LE(0, kind());
  CHECK_LE(kind(), Kind::kAsyncFunctionAwait);
}

void Microtask::MicrotaskVerify(Isolate* isolate) {
#ifdef V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA
  Object::VerifyPointer(isolate, continuation_preserved_embedder_data());
#endif
}

void PromiseReactionJobTask::PromiseReactionJobTaskVerify(Isolate* isolate) {
  MicrotaskVerify(isolate);
  Object::VerifyPointer(isolate, argument());
  Object::VerifyPointer(isolate, context());
  Object::VerifyPointer(isolate, handler());
  Object::VerifyPointer(isolate, promise_or_capability());
}

void PromiseFulfillReactionJobTask::PromiseFulfillReactionJobTaskVerify(
    Isolate* isolate) {
  PromiseReactionJobTaskVerify(isolate);
}

void PromiseRejectReactionJobTask::PromiseRejectReactionJobTaskVerify(
    Isolate* isolate) {
  PromiseReactionJobTaskVerify(isolate);
}

void PromiseResolveThenableJobTask::PromiseResolveThenableJobTaskVerify(
    Isolate* isolate) {
  MicrotaskVerify(isolate);
  Object::VerifyPointer(isolate, context());
  Object::VerifyPointer(isolate, promise_to_resolve());
  Object::VerifyPointer(isolate, thenable());
  Object::VerifyPointer(isolate, then());
}

void PromiseCapability::PromiseCapabilityVerify(Isolate* isolate) {
  Object::VerifyPointer(isolate, promise());
  Object::VerifyPointer(isolate, resolve());
  Object::VerifyPointer(isolate, reject());
}

void PromiseReaction::PromiseReactionVerify(Isolate* isolate) {
#ifdef V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA
  Object::VerifyPointer(isolate, continuation_preserved_embedder_data());
#endif
  Object::VerifyPointer(isolate, next());
  Object::VerifyPointer(isolate, reject_handler());
  Object::VerifyPointer(isolate, fulfill_handler());
  Object::VerifyPointer(isolate, promise_or_capability());
}

void JSPromise::JSPromiseVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
  CHECK(flags_.load().IsSmi());
  if (status() == Promise::kPending) {
    CHECK(IsSmi(reactions()) || IsPromiseReaction(reactions()));
  }
}

template <typename Derived>
void SmallOrderedHashTableImpl<Derived>::SmallOrderedHashTableVerify(
    Isolate* isolate) {
  CHECK(IsSmallOrderedHashTable(this));

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
      Tagged<Object> val = GetDataEntry(entry, offset);
      Object::VerifyPointer(isolate, val);
    }
  }

  for (int entry = NumberOfElements() + NumberOfDeletedElements();
       entry < Capacity(); entry++) {
    for (int offset = 0; offset < Derived::kEntrySize; offset++) {
      Tagged<Object> val = GetDataEntry(entry, offset);
      CHECK(IsTheHole(val, isolate));
    }
  }
}

void SmallOrderedHashMap::SmallOrderedHashMapVerify(Isolate* isolate) {
  CHECK(IsSmallOrderedHashMap(this));
  SmallOrderedHashTableImpl<SmallOrderedHashMap>::SmallOrderedHashTableVerify(
      isolate);
  for (int entry = NumberOfElements(); entry < NumberOfDeletedElements();
       entry++) {
    for (int offset = 0; offset < kEntrySize; offset++) {
      Tagged<Object> val = GetDataEntry(entry, offset);
      CHECK(IsTheHole(val, isolate));
    }
  }
}

void SmallOrderedHashSet::SmallOrderedHashSetVerify(Isolate* isolate) {
  CHECK(IsSmallOrderedHashSet(this));
  SmallOrderedHashTableImpl<SmallOrderedHashSet>::SmallOrderedHashTableVerify(
      isolate);
  for (int entry = NumberOfElements(); entry < NumberOfDeletedElements();
       entry++) {
    for (int offset = 0; offset < kEntrySize; offset++) {
      Tagged<Object> val = GetDataEntry(entry, offset);
      CHECK(IsTheHole(val, isolate));
    }
  }
}

void SmallOrderedNameDictionary::SmallOrderedNameDictionaryVerify(
    Isolate* isolate) {
  CHECK(IsSmallOrderedNameDictionary(this));
  SmallOrderedHashTableImpl<
      SmallOrderedNameDictionary>::SmallOrderedHashTableVerify(isolate);
  for (int entry = NumberOfElements(); entry < NumberOfDeletedElements();
       entry++) {
    for (int offset = 0; offset < kEntrySize; offset++) {
      Tagged<Object> val = GetDataEntry(entry, offset);
      CHECK(IsTheHole(val, isolate) ||
            (PropertyDetails::Empty().AsSmi() == Cast<Smi>(val)));
    }
  }
}

void SwissNameDictionary::SwissNameDictionaryVerify(Isolate* isolate) {
  this->SwissNameDictionaryVerify(isolate, false);
}

void SwissNameDictionary::SwissNameDictionaryVerify(Isolate* isolate,
                                                    bool slow_checks) {
  DisallowHeapAllocation no_gc;

  CHECK(IsValidCapacity(Capacity()));

  meta_table()->ByteArrayVerify(isolate);

  int seen_deleted = 0;
  int seen_present = 0;

  for (int i = 0; i < Capacity(); i++) {
    ctrl_t ctrl = GetCtrl(i);

    if (IsFull(ctrl) || slow_checks) {
      Tagged<Object> key = KeyAt(i);
      Tagged<Object> value = ValueAtRaw(i);

      if (IsFull(ctrl)) {
        ++seen_present;

        Tagged<Name> name = Cast<Name>(key);
        if (slow_checks) {
          CHECK_EQ(swiss_table::H2(name->hash()), ctrl);
        }

        CHECK(!IsTheHole(key));
        CHECK(!IsTheHole(value));
        name->NameVerify(isolate);
        Object::ObjectVerify(value, isolate);
      } else if (IsDeleted(ctrl)) {
        ++seen_deleted;
        CHECK(IsTheHole(key));
        CHECK(IsTheHole(value));
      } else if (IsEmpty(ctrl)) {
        CHECK(IsTheHole(key));
        CHECK(IsTheHole(value));
      } else {
        // Something unexpected. Note that we don't use kSentinel at the moment.
        UNREACHABLE();
      }
    }
  }

  CHECK_EQ(seen_present, NumberOfElements());
  if (slow_checks) {
    CHECK_EQ(seen_deleted, NumberOfDeletedElements());

    // Verify copy of first group at end (= after Capacity() slots) of control
    // table.
    for (int i = 0; i < std::min(static_cast<int>(Group::kWidth), Capacity());
         ++i) {
      CHECK_EQ(CtrlTable()[i], CtrlTable()[Capacity() + i]);
    }
    // If 2 * capacity is smaller than the capacity plus group width, the slots
    // after that must be empty.
    for (int i = 2 * Capacity(); i < Capacity() + kGroupWidth; ++i) {
      CHECK_EQ(Ctrl::kEmpty, CtrlTable()[i]);
    }

    for (int enum_index = 0; enum_index < UsedCapacity(); ++enum_index) {
      int entry = EntryForEnumerationIndex(enum_index);
      CHECK_LT(entry, Capacity());
      ctrl_t ctrl = GetCtrl(entry);

      // Enum table must not point to empty slots.
      CHECK(IsFull(ctrl) || IsDeleted(ctrl));
    }
  }
}

void JSMessageObject::JSMessageObjectVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
  CHECK(IsScript(script()));
}

void JSRegExp::JSRegExpVerify(Isolate* isolate) {
  Tagged<Object> flags =
      TaggedField<Object>::load(this, offsetof(JSRegExp, flags_));
  CHECK(IsSmi(flags) || IsUndefined(flags));
  if (!has_data()) return;

  Tagged<RegExpData> data = this->data(isolate);
  switch (data->type_tag()) {
    case RegExpData::Type::ATOM:
      CHECK(Is<AtomRegExpData>(data));
      return;
    case RegExpData::Type::EXPERIMENTAL:
    case RegExpData::Type::IRREGEXP:
      CHECK(Is<IrRegExpData>(data));
      return;
  }
  UNREACHABLE();
}

void RegExpData::RegExpDataVerify(Isolate* isolate) {
  CHECK(IsRegExpData(this));
  Object::VerifyPointer(isolate, original_source());
  Object::VerifyPointer(isolate, escaped_source());
  Object::VerifyPointer(isolate, wrapper());
}

#ifdef V8_TEMPORAL_SUPPORT
#define DEFINE_TEMPORAL_VERIFIER(JSType, field)   \
  void JSType::JSType##Verify(Isolate* isolate) { \
    JSObjectVerify(isolate);                      \
    CHECK(IsForeign(field##_.load()));            \
  }

DEFINE_TEMPORAL_VERIFIER(JSTemporalDuration, duration)
DEFINE_TEMPORAL_VERIFIER(JSTemporalInstant, instant)
DEFINE_TEMPORAL_VERIFIER(JSTemporalPlainDate, date)
DEFINE_TEMPORAL_VERIFIER(JSTemporalPlainDateTime, date_time)
DEFINE_TEMPORAL_VERIFIER(JSTemporalPlainMonthDay, month_day)
DEFINE_TEMPORAL_VERIFIER(JSTemporalPlainTime, time)
DEFINE_TEMPORAL_VERIFIER(JSTemporalPlainYearMonth, year_month)
DEFINE_TEMPORAL_VERIFIER(JSTemporalZonedDateTime, zoned_date_time)
#undef DEFINE_TEMPORAL_VERIFIER
#endif  // V8_TEMPORAL_SUPPORT

void AtomRegExpData::AtomRegExpDataVerify(Isolate* isolate) {
  CHECK(IsAtomRegExpData(this));
  RegExpDataVerify(isolate);
  Object::VerifyPointer(isolate, pattern());
}

void IrRegExpData::IrRegExpDataVerify(Isolate* isolate) {
  CHECK(IsIrRegExpData(this));
  RegExpDataVerify(isolate);

  Object::VerifyPointer(isolate, latin1_bytecode());
  Object::VerifyPointer(isolate, uc16_bytecode());

  CHECK_IMPLIES(!has_latin1_code(), !has_latin1_bytecode());
  CHECK_IMPLIES(!has_uc16_code(), !has_uc16_bytecode());

  CHECK_IMPLIES(has_latin1_code(), Is<Code>(latin1_code(isolate)));
  CHECK_IMPLIES(has_uc16_code(), Is<Code>(uc16_code(isolate)));
  CHECK_IMPLIES(has_latin1_bytecode(), Is<TrustedByteArray>(latin1_bytecode()));
  CHECK_IMPLIES(has_uc16_bytecode(), Is<TrustedByteArray>(uc16_bytecode()));

  CHECK_IMPLIES(has_capture_name_map(),
                Is<TrustedFixedArray>(capture_name_map()));

  switch (type_tag()) {
    case RegExpData::Type::EXPERIMENTAL: {
      if (has_latin1_code()) {
        CHECK_EQ(latin1_code(isolate)->builtin_id(),
                 Builtin::kRegExpExperimentalTrampoline);
        CHECK_EQ(latin1_code(isolate), uc16_code(isolate));
        CHECK(Is<TrustedByteArray>(latin1_bytecode()));
        CHECK_EQ(latin1_bytecode(), uc16_bytecode());
      } else {
        CHECK(!has_uc16_code());
        CHECK(!has_latin1_bytecode());
        CHECK(!has_uc16_bytecode());
      }

      CHECK_EQ(max_register_count(), JSRegExp::kUninitializedValue);
      CHECK_EQ(ticks_until_tier_up(), JSRegExp::kUninitializedValue);
      CHECK_EQ(backtrack_limit(), JSRegExp::kUninitializedValue);
      CHECK_EQ(bit_field(), 0);

      break;
    }
    case RegExpData::Type::IRREGEXP: {
      bool can_be_interpreted = RegExp::CanGenerateBytecode();
      CHECK_IMPLIES(has_latin1_bytecode(), can_be_interpreted);
      CHECK_IMPLIES(has_uc16_bytecode(), can_be_interpreted);

      static_assert(JSRegExp::kUninitializedValue == -1);
      CHECK_GE(max_register_count(), JSRegExp::kUninitializedValue);
      CHECK_GE(capture_count(), 0);
      if (v8_flags.regexp_tier_up) {
        // With tier-up enabled, ticks_until_tier_up should actually be >= 0.
        // However FlagScopes in unittests can modify the flag and verification
        // on Isolate deinitialization will fail.
        CHECK_GE(ticks_until_tier_up(), JSRegExp::kUninitializedValue);
        CHECK_LE(ticks_until_tier_up(), v8_flags.regexp_tier_up_ticks);
      } else {
        CHECK_EQ(ticks_until_tier_up(), JSRegExp::kUninitializedValue);
      }
      CHECK_GE(backtrack_limit(), 0);

      break;
    }
    default:
      UNREACHABLE();
  }
}

void RegExpDataWrapper::RegExpDataWrapperVerify(Isolate* isolate) {
  CHECK(IsRegExpDataWrapper(this));
  if (!this->has_data()) return;
  auto data = this->data(isolate);
  Object::VerifyPointer(isolate, data);
  CHECK_EQ(data->wrapper(), this);
}

void JSProxy::JSProxyVerify(Isolate* isolate) {
  Cast<JSReceiver>(this)->JSReceiverVerify(isolate);
  CHECK(IsJSProxy(this));
  Object::VerifyPointer(isolate, target());
  CHECK(IsNull(target()) || IsJSReceiver(target()));
  Object::VerifyPointer(isolate, handler());
  CHECK(IsNull(handler()) || IsJSReceiver(handler()));
  CHECK(IsJSFunction(map()->GetConstructor()));
  if (!IsRevoked()) {
    CHECK_EQ(IsCallable(target()), map()->is_callable());
    CHECK_EQ(IsConstructor(target()), map()->is_constructor());
  }
  CHECK(IsNull(map()->prototype(), isolate));
  // There should be no properties on a Proxy.
  CHECK_EQ(0, map()->NumberOfOwnDescriptors());
}

void JSArrayBuffer::JSArrayBufferVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
#if TAGGED_SIZE_8_BYTES
  CHECK_EQ(0u, optional_padding_);
#endif
  Tagged<MaybeObject> v = views_or_detach_key();
  if (is_shared() || !v8_flags.track_array_buffer_views) {
    CHECK(has_detach_key() || v == kNoView);
  } else {
    CHECK_EQ(!v.IsWeak() && !v.IsSmi() && !v.IsCleared(), has_detach_key());
    CHECK(v == kNoView || v == kManyViews || (v.IsStrong() && Is<Cell>(v)) ||
          (v.IsWeak() && IsJSArrayBufferView(v.GetHeapObjectAssumeWeak())) ||
          v.IsCleared());
  }
}

void JSArrayBufferView::JSArrayBufferViewVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
  CHECK(IsJSArrayBufferView(this));
  Object::VerifyPointer(isolate, buffer());
  CHECK(IsJSArrayBuffer(buffer()));
  CHECK_LE(byte_length(), JSArrayBuffer::kMaxByteLength);
  CHECK_LE(byte_offset(), JSArrayBuffer::kMaxByteLength);
}

void JSDetachedTypedArray::JSDetachedTypedArrayVerify(Isolate* isolate) {
  JSTypedArrayVerify(isolate);
  CHECK_LE(GetLength(), 0);
  CHECK(WasDetached());
}

namespace {

template <typename View>
void CheckArrayBufferViewTrackingConsistency(Tagged<JSArrayBuffer> ab,
                                             Tagged<View> view,
                                             Isolate* isolate) {
  if (ab->is_shared()) return;
  Tagged<MaybeObject> views = ab->views_or_detach_key();
  if (!IsUndefined(ab->DetachKey(isolate))) {
    CHECK_EQ(ab->views(), JSArrayBuffer::kManyViews);
    CHECK(!views.IsSmi() && views.IsStrong() && Is<Cell>(views));
    return;
  }
  if (!v8_flags.track_array_buffer_views) {
    CHECK_EQ(views, Smi::zero());
    return;
  }
  if (views == JSArrayBuffer::kNoView || views.IsCleared()) {
    CHECK(view->WasDetached());
  } else if (views != JSArrayBuffer::kManyViews) {
    CHECK_EQ(views.GetHeapObjectAssumeWeak(), view);
    CHECK(Is<JSArrayBufferView>(views.GetHeapObjectAssumeWeak()));
  }
}

}  // namespace

void JSTypedArray::JSTypedArrayVerify(Isolate* isolate) {
  JSArrayBufferViewVerify(isolate);
  Object::VerifyPointer(isolate, base_pointer());
  CHECK(IsSmi(base_pointer()) || IsByteArray(base_pointer()));
  CHECK_LE(GetLength(), JSTypedArray::kMaxByteLength / element_size());
  if (IsJSArrayBuffer(buffer())) {
    CheckArrayBufferViewTrackingConsistency(buffer(), Tagged{this}, isolate);
  }
}

void JSDataView::JSDataViewVerify(Isolate* isolate) {
  JSArrayBufferViewVerify(isolate);
  CHECK(!IsVariableLength());
  if (!WasDetached()) {
    CHECK_EQ(
        reinterpret_cast<uint8_t*>(buffer()->backing_store()) + byte_offset(),
        data_pointer());
  }

  if (IsJSArrayBuffer(buffer())) {
    CheckArrayBufferViewTrackingConsistency(buffer(), Tagged{this}, isolate);
  }
}

void JSRabGsabDataView::JSRabGsabDataViewVerify(Isolate* isolate) {
  JSArrayBufferViewVerify(isolate);
  CHECK(IsVariableLength());
  if (!WasDetached()) {
    CHECK_EQ(
        reinterpret_cast<uint8_t*>(buffer()->backing_store()) + byte_offset(),
        data_pointer());
  }

  if (IsJSArrayBuffer(buffer())) {
    CheckArrayBufferViewTrackingConsistency(buffer(), Tagged{this}, isolate);
  }
}

void AsyncGeneratorRequest::AsyncGeneratorRequestVerify(Isolate* isolate) {
  CHECK(IsStruct(this));
  CHECK(IsAsyncGeneratorRequest(this));
  Object::VerifyPointer(isolate, next());
  Object::VerifyPointer(isolate, value());
  Object::VerifyPointer(isolate, promise());
  CHECK_GE(resume_mode(), JSGeneratorObject::kNext);
  CHECK_LE(resume_mode(), JSGeneratorObject::kThrow);
}

void BigIntBase::BigIntBaseVerify(Isolate* isolate) {
  CHECK_GE(length(), 0);
  CHECK_IMPLIES(is_zero(), !sign());  // There is no -0n.
}

void CoverageInfo::CoverageInfoVerify(Isolate* isolate) {
  CHECK(IsCoverageInfo(this));
  CHECK_GE(slot_count(), 0);
}

void CppHeapExternalObject::CppHeapExternalObjectVerify(Isolate* isolate) {
  CHECK(IsCppHeapExternalObject(this));
}

void AccessorInfo::AccessorInfoVerify(Isolate* isolate) {
  CHECK(IsAccessorInfo(this));
  Object::VerifyPointer(isolate, data());
  Object::VerifyPointer(isolate, name());
}

void InterceptorInfo::InterceptorInfoVerify(Isolate* isolate) {
  CHECK(IsInterceptorInfo(this));
  Object::VerifyPointer(isolate, data());
}

void SourceTextModuleInfoEntry::SourceTextModuleInfoEntryVerify(
    Isolate* isolate) {
  CHECK(IsStruct(this));
  CHECK(IsSourceTextModuleInfoEntry(this));
  Object::VerifyPointer(isolate, export_name());
  Object::VerifyPointer(isolate, local_name());
  Object::VerifyPointer(isolate, import_name());
  CHECK_IMPLIES(IsString(import_name()), module_request() >= 0);
  CHECK_IMPLIES(IsString(export_name()) && IsString(import_name()),
                IsUndefined(local_name(), isolate));
}

void Module::ModuleVerify(Isolate* isolate) {
  CHECK_EQ(status() == Module::kErrored, !IsTheHole(exception(), isolate));

  CHECK(IsUndefined(module_namespace(), isolate) || IsCell(module_namespace()));
  if (IsCell(module_namespace())) {
    // Technically kLinking should be the correct check, however we can end up
    // here in SourceTextModule::FinishInstantiate before updating the status.
    CHECK_LE(Module::kPreLinking, status());
    auto cell = Cast<Cell>(module_namespace());
    CHECK(IsJSModuleNamespace(cell->value()) || IsUndefined(cell->value()));
    if (IsJSModuleNamespace(cell->value())) {
      CHECK_EQ(Cast<JSModuleNamespace>(cell->value())->module(), this);
    }
  }

  if (!(status() == kErrored || status() == kEvaluating ||
        status() == kEvaluatingAsync || status() == kEvaluated)) {
    CHECK(IsUndefined(top_level_capability()));
  }

  CHECK_NE(hash(), 0);
}

void ModuleRequest::ModuleRequestVerify(Isolate* isolate) {
  CHECK(IsStruct(this));
  CHECK(IsModuleRequest(this));
  Object::VerifyPointer(isolate, specifier());
  Object::VerifyPointer(isolate, import_attributes());
  uint32_t import_attributes_len = import_attributes()->ulength().value();
  CHECK_EQ(0, import_attributes_len % ModuleRequest::kAttributeEntrySize);

  for (uint32_t i = 0; i < import_attributes_len;
       i += ModuleRequest::kAttributeEntrySize) {
    CHECK(IsString(import_attributes()->get(i)));      // Attribute key
    CHECK(IsString(import_attributes()->get(i + 1)));  // Attribute value
    CHECK(IsSmi(import_attributes()->get(i + 2)));     // Attribute location
  }
}

void JSModuleNamespace::JSModuleNamespaceVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
  CHECK(IsJSModuleNamespace(this));
  Object::VerifyPointer(isolate, module());
  CHECK(IsModule(module()));
}

void JSDeferredModuleNamespace::JSDeferredModuleNamespaceVerify(
    Isolate* isolate) {
  JSModuleNamespaceVerify(isolate);
  CHECK(IsJSDeferredModuleNamespace(this));
}

void SourceTextModule::SourceTextModuleVerify(Isolate* isolate) {
  if (status() == kErrored) {
    CHECK(IsSharedFunctionInfo(code()));
  } else if (status() == kEvaluating || status() == kEvaluatingAsync ||
             status() == kEvaluated) {
    CHECK(IsJSGeneratorObject(code()));
  } else {
    if (status() == kLinked) {
      CHECK(IsJSGeneratorObject(code()));
    } else if (status() == kLinking) {
      CHECK(IsJSFunction(code()));
    } else if (status() == kPreLinking) {
      CHECK(IsSharedFunctionInfo(code()));
    } else if (status() == kUnlinked) {
      CHECK(IsSharedFunctionInfo(code()));
    }
    CHECK(!AsyncParentModuleCount());
    CHECK(!pending_async_dependencies());
    CHECK(!HasAsyncEvaluationOrdinal());
  }

  CHECK_EQ(requested_modules()->length(), info()->module_requests()->length());
}

void SyntheticModule::SyntheticModuleVerify(Isolate* isolate) {
  uint32_t export_names_len = export_names()->ulength().value();
  for (uint32_t i = 0; i < export_names_len; i++) {
    CHECK(IsString(export_names()->get(i)));
  }
}

void PrototypeInfo::PrototypeInfoVerify(Isolate* isolate) {
  CHECK(IsPrototypeInfo(this));
  Object::VerifyPointer(isolate, module_namespace());
  if (IsWeakArrayList(prototype_users())) {
    PrototypeUsers::Verify(Cast<WeakArrayList>(prototype_users()));
  } else {
    CHECK(IsSmi(prototype_users()));
  }
  Object::VerifyPointer(isolate, prototype_chain_enum_cache());
  Object::VerifyPointer(isolate, derived_maps());
  Object::VerifyPointer(isolate, prototype_shared_closure_info());
  for (int i = 0; i < kCachedHandlerCount; i++) {
    Object::VerifyPointer(isolate, cached_handler(i));
  }
  Tagged<UnionOf<WeakArrayList, Undefined>> derived = derived_maps();
  if (!IsUndefined(derived)) {
    auto derived_list = Cast<WeakArrayList>(derived);
    const int derived_length = derived_list->length().value();
    CHECK_GT(derived_length, 0);
    for (int i = 0; i < derived_length; ++i) {
      derived_list->Get(i).IsWeakOrCleared();
    }
  }
}

void PrototypeUsers::Verify(Tagged<WeakArrayList> array) {
  const uint32_t array_len = array->length().value();
  if (array_len == 0) {
    // Allow empty & uninitialized lists.
    return;
  }
  // Verify empty slot chain.
  int empty_slot = Smi::ToInt(empty_slot_index(array));
  uint32_t empty_slots_count = 0;
  while (empty_slot != kNoEmptySlotsMarker) {
    CHECK_GT(empty_slot, 0);
    CHECK_LT(empty_slot, array_len);
    empty_slot = array->Get(empty_slot).ToSmi().value();
    ++empty_slots_count;
  }

  // Verify that all elements are either weak pointers or SMIs marking empty
  // slots.
  uint32_t weak_maps_count = 0;
  for (uint32_t i = kFirstIndex; i < array_len; ++i) {
    Tagged<HeapObject> heap_object;
    Tagged<MaybeObject> object = array->Get(i);
    if ((object.GetHeapObjectIfWeak(&heap_object) && IsMap(heap_object)) ||
        object.IsCleared()) {
      ++weak_maps_count;
    } else {
      CHECK(IsSmi(object));
    }
  }

  CHECK_EQ(weak_maps_count + empty_slots_count + 1, array_len);
}

void EnumCache::EnumCacheVerify(Isolate* isolate) {
  Object::VerifyPointer(isolate, keys());
  Object::VerifyPointer(isolate, indices());

  ReadOnlyRoots roots(isolate);
  if (this == roots.empty_enum_cache()) {
    CHECK_EQ(roots.empty_fixed_array(), keys());
    CHECK_EQ(roots.empty_fixed_array(), indices());
  }
}

void ObjectBoilerplateDescription::ObjectBoilerplateDescriptionVerify(
    Isolate* isolate) {
  CHECK_LE(length_, kMaxCapacity);
  CHECK(IsSmi(backing_store_size_.load()));
  CHECK(IsSmi(flags_.load()));
  // The keys of the boilerplate should either be internalized strings, or
  // numbers. Exceptionally, during initialization, they can also be undefined.
  for (int i = 0; i < boilerplate_properties_count(); ++i) {
    CHECK((
        Is<UnionOf<InternalizedString, Number, Undefined>>(get(NameIndex(i)))));
  }
}

void ArrayBoilerplateDescription::ArrayBoilerplateDescriptionVerify(
    Isolate* isolate) {
  CHECK(IsSmi(flags_.load()));
  CHECK(IsFixedArrayBase(constant_elements_.load()));
  Object::VerifyPointer(isolate, constant_elements_.load());
}

void ClassBoilerplate::ClassBoilerplateVerify(Isolate* isolate) {
  CHECK(IsSmi(arguments_count_.load()));
  Object::VerifyPointer(isolate, static_properties_template_.load());
  Object::VerifyPointer(isolate, static_elements_template_.load());
  Object::VerifyPointer(isolate, static_computed_properties_.load());
  CHECK(IsFixedArray(static_computed_properties_.load()));
  Object::VerifyPointer(isolate, instance_properties_template_.load());
  Object::VerifyPointer(isolate, instance_elements_template_.load());
  Object::VerifyPointer(isolate, instance_computed_properties_.load());
  CHECK(IsFixedArray(instance_computed_properties_.load()));
}

void PropertyDescriptorObject::PropertyDescriptorObjectVerify(
    Isolate* isolate) {
  CHECK(IsSmi(flags_.load()));
  Object::VerifyPointer(isolate, value_.load());
  Object::VerifyPointer(isolate, get_.load());
  Object::VerifyPointer(isolate, set_.load());
}

void TemplateObjectDescription::TemplateObjectDescriptionVerify(
    Isolate* isolate) {
  Object::VerifyPointer(isolate, raw_strings_.load());
  CHECK(IsFixedArray(raw_strings_.load()));
  Object::VerifyPointer(isolate, cooked_strings_.load());
  CHECK(IsFixedArray(cooked_strings_.load()));
}

void RegExpBoilerplateDescription::RegExpBoilerplateDescriptionVerify(
    Isolate* isolate) {
  {
    auto o = data(isolate);
    Object::VerifyPointer(isolate, o);
    CHECK(IsRegExpData(o));
  }
  CHECK(IsSmi(flags_.load()));
}

#if V8_ENABLE_WEBASSEMBLY

void WasmTrustedInstanceData::WasmTrustedInstanceDataVerify(Isolate* isolate) {
  ExposedTrustedObjectVerify(isolate);

  // Check all tagged fields.
  for (uint16_t offset : kTaggedFieldOffsets) {
    VerifyObjectField(isolate, offset);
  }

  // Check all protected fields.
  for (uint16_t offset : kProtectedFieldOffsets) {
    VerifyProtectedPointerField(isolate, offset);
  }

  uint32_t num_dispatch_tables = dispatch_tables()->ulength().value();
  for (uint32_t i = 0; i < num_dispatch_tables; ++i) {
    Tagged<Object> table = dispatch_tables()->get(i);
    if (table == Smi::zero()) continue;
    CHECK(IsWasmDispatchTable(table));
    if (i == 0) CHECK_EQ(table, dispatch_table0());
  }
  if (num_dispatch_tables == 0) CHECK_EQ(0, dispatch_table0()->length());
}

void WasmDispatchTable::WasmDispatchTableVerify(Isolate* isolate) {
  TrustedObjectVerify(isolate);

  int len = length();
  CHECK_LE(len, capacity());
  for (int i = 0; i < len; ++i) {
    Tagged<Object> arg = implicit_arg(i);
    Object::VerifyPointer(isolate, arg);
    CHECK(IsWasmTrustedInstanceData(arg) || IsWasmImportData(arg) ||
          arg == Smi::zero());
    if (!v8_flags.wasm_jitless) {
      // call_target always null with the interpreter.
      CHECK_EQ(arg == Smi::zero(), target(i) == wasm::kInvalidWasmCodePointer);
    }
  }

  // Check invariants of the "uses" list (which are specific to
  // WasmDispatchTable, not inherent to any ProtectedWeakFixedArray).
  Tagged<ProtectedWeakFixedArray> uses = protected_uses();
  if (uses->ulength().value() > 0) {
    CHECK(IsSmi(uses->get(0)));
    uint32_t capacity = uses->ulength().value();
    CHECK(capacity & 1);  // Capacity is odd: reserved slot + 2*num_entries.
    int used_length = Cast<Smi>(uses->get(0)).value();
    CHECK_GE(used_length, 0);
    CHECK_LE(static_cast<uint32_t>(used_length), capacity);
    for (int i = 1; i < used_length; i += 2) {
      CHECK(uses->get(i).IsCleared() ||
            IsWasmTrustedInstanceData(uses->get(i).GetHeapObjectAssumeWeak()));
      CHECK(IsSmi(uses->get(i + 1)));
    }
  }
}

void WasmDispatchTableForImports::WasmDispatchTableForImportsVerify(
    Isolate* isolate) {
  TrustedObjectVerify(isolate);

  int len = length();
  for (int i = 0; i < len; ++i) {
    Tagged<Object> arg = implicit_arg(i);
    Object::VerifyPointer(isolate, arg);
    CHECK(IsWasmTrustedInstanceData(arg) || IsWasmImportData(arg) ||
          arg == Smi::zero());
    if (!v8_flags.wasm_jitless) {
      // call_target always null with the interpreter.
      CHECK_EQ(arg == Smi::zero(), target(i) == wasm::kInvalidWasmCodePointer);
    }
  }
}

void WasmTableObject::WasmTableObjectVerify(Isolate* isolate) {
  CHECK(IsWasmTableObject(this));
  Object::VerifyPointer(isolate, entries());
  CHECK(IsFixedArray(entries()));
  Object::VerifyPointer(isolate, maximum_length());
  // If Wasm instantiation fails, trusted objects are unpublished.
  // Orphaned JS objects might still point to them though.
  if (has_trusted_dispatch_table_unpublished(isolate)) return;
  bool is_function_table =
      unsafe_type().ref_type_kind() == wasm::RefTypeKind::kFunction;
  bool has_dispatch_table = trusted_dispatch_table(isolate) !=
                            *isolate->factory()->empty_wasm_dispatch_table();
  CHECK_EQ(is_function_table, has_dispatch_table);
  if (is_function_table) {
    CHECK_EQ(trusted_dispatch_table(isolate)->length(), current_length());
  }
}

void WasmValueObject::WasmValueObjectVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
  CHECK(IsWasmValueObject(this));
}

void WasmExceptionPackage::WasmExceptionPackageVerify(Isolate* isolate) {
  JSObjectVerify(isolate);
  CHECK(IsWasmExceptionPackage(this));
}

void WasmExceptionTag::WasmExceptionTagVerify(Isolate* isolate) {
  CHECK(IsStruct(this));
  CHECK(IsWasmExceptionTag(this));
  CHECK(IsSmi(Tagged<Object>(index_.load())));
}

void AsmWasmData::AsmWasmDataVerify(Isolate* isolate) {
  CHECK(IsAsmWasmData(this));
  ExposedTrustedObjectVerify(isolate);
  if (has_managed_native_module()) {
    Object::VerifyPointer(isolate, managed_native_module());
  }
}

void WasmFuncRef::WasmFuncRefVerify(Isolate* isolate) {
  CHECK(IsWasmFuncRef(this));
}

void WasmNull::WasmNullVerify(Isolate* isolate) { CHECK(IsWasmNull(this)); }

void WasmImportData::WasmImportDataVerify(Isolate* isolate) {
  CHECK(IsWasmImportData(this));
  Object::VerifyPointer(isolate, native_context_.load());
  CHECK(IsNativeContext(native_context_.load()));
  Object::VerifyPointer(isolate, callable_.load());
  CHECK(IsUndefined(callable_.load()) || IsJSReceiver(callable_.load()));
  Object::VerifyPointer(isolate, wrapper_budget_.load());
  CHECK(IsCell(wrapper_budget_.load()));
}

void WasmInternalFunction::WasmInternalFunctionVerify(Isolate* isolate) {
  CHECK(IsWasmInternalFunction(this));
  Object::VerifyPointer(isolate, external_.load());
  CHECK(IsUndefined(external_.load()) || IsJSFunction(external_.load()));
  Object::VerifyPointer(isolate, function_index_.load());
  CHECK(IsSmi(function_index_.load()));
}

void WasmFunctionData::WasmFunctionDataVerify(Isolate* isolate) {
  CHECK(IsWasmFunctionData(this));
  Object::VerifyPointer(isolate, func_ref_.load());
  CHECK(IsWasmFuncRef(func_ref_.load()));
  Object::VerifyPointer(isolate, js_promise_flags_.load());
  CHECK(IsSmi(js_promise_flags_.load()));
}

void WasmExportedFunctionData::WasmExportedFunctionDataVerify(
    Isolate* isolate) {
  CHECK(IsWasmExportedFunctionData(this));
  WasmFunctionDataVerify(isolate);
  Object::VerifyPointer(isolate, function_index_.load());
  CHECK(IsSmi(function_index_.load()));
  Object::VerifyPointer(isolate, wrapper_budget_.load());
  CHECK(IsCell(wrapper_budget_.load()));
  Object::VerifyPointer(isolate, packed_args_size_.load());
  CHECK(IsSmi(packed_args_size_.load()));
  Tagged<Code> wrapper = wrapper_code(isolate);
  CHECK(wrapper->kind() == CodeKind::JS_TO_WASM_FUNCTION ||
        wrapper->kind() == CodeKind::C_WASM_ENTRY ||
        (wrapper->is_builtin() &&
         (wrapper->builtin_id() == Builtin::kJSToWasmWrapper ||
#if V8_ENABLE_DRUMBRAKE
          wrapper->builtin_id() == Builtin::kJSToWasmInterpreterWrapper ||
          wrapper->builtin_id() == Builtin::kJSToWasmInterpreterWrapperAsm ||
#endif  // V8_ENABLE_DRUMBRAKE
          wrapper->builtin_id() == Builtin::kWasmPromising ||
          wrapper->builtin_id() == Builtin::kWasmStressSwitch)));
}

void WasmCapiFunctionData::WasmCapiFunctionDataVerify(Isolate* isolate) {
  CHECK(IsWasmCapiFunctionData(this));
  WasmFunctionDataVerify(isolate);
  Object::VerifyPointer(isolate, embedder_data_.load());
  CHECK(IsForeign(embedder_data_.load()));
}

void WasmSuspenderObject::WasmSuspenderObjectVerify(Isolate* isolate) {
  CHECK(IsWasmSuspenderObject(this));
  Object::VerifyPointer(isolate, promise_.load());
  CHECK(IsUndefined(promise_.load()) || IsJSPromise(promise_.load()));
  Object::VerifyPointer(isolate, resume_.load());
  CHECK(IsUndefined(resume_.load()) || IsJSObject(resume_.load()));
  Object::VerifyPointer(isolate, reject_.load());
  CHECK(IsUndefined(reject_.load()) || IsJSObject(reject_.load()));
}

void WasmTypeInfo::WasmTypeInfoVerify(Isolate* isolate) {
  CHECK(IsWasmTypeInfo(this));
  CHECK_GE(supertypes_length(), 0);
  for (int i = 0; i < supertypes_length(); i++) {
    Object::VerifyPointer(isolate, supertypes(i));
  }
}

void WasmContinuationObject::WasmContinuationObjectVerify(Isolate* isolate) {
  CHECK(IsWasmContinuationObject(this));
  Object::VerifyPointer(isolate, stack_obj());
}

void WasmStackObject::WasmStackObjectVerify(Isolate* isolate) {
  CHECK(IsWasmStackObject(this));
}

void WasmResumeData::WasmResumeDataVerify(Isolate* isolate) {
  CHECK(IsWasmResumeData(this));
  Object::VerifyPointer(isolate, on_resume_.load());
  CHECK(IsSmi(on_resume_.load()));
}

void WasmSuspendingObject::WasmSuspendingObjectVerify(Isolate* isolate) {
  CHECK(IsWasmSuspendingObject(this));
  Object::VerifyPointer(isolate, callable());
}

void WasmModuleObject::WasmModuleObjectVerify(Isolate* isolate) {
  CHECK(IsWasmModuleObject(this));
  Object::VerifyPointer(isolate, managed_native_module());
  Object::VerifyPointer(isolate, script());
}

void WasmInstanceObject::WasmInstanceObjectVerify(Isolate* isolate) {
  CHECK(IsWasmInstanceObject(this));
  Object::VerifyPointer(isolate, module_object());
  Object::VerifyPointer(isolate, exports_object());
}

void WasmTagObject::WasmTagObjectVerify(Isolate* isolate) {
  CHECK(IsWasmTagObject(this));
  Object::VerifyPointer(isolate, tag());
  CHECK(IsSmi(canonical_type_index_.load()));
}

void WasmStruct::WasmStructVerify(Isolate* isolate) {
  CHECK(IsWasmStruct(this));
}

void WasmArray::WasmArrayVerify(Isolate* isolate) { CHECK(IsWasmArray(this)); }

void WasmMemoryMapDescriptor::WasmMemoryMapDescriptorVerify(Isolate* isolate) {
  CHECK(IsWasmMemoryMapDescriptor(this));
}

void WasmMemoryObject::WasmMemoryObjectVerify(Isolate* isolate) {
  CHECK(IsWasmMemoryObject(this));
  Object::VerifyPointer(isolate, array_buffer());
  Object::VerifyPointer(isolate, managed_backing_store());
  Object::VerifyPointer(isolate, instances());
}

void WasmGlobalObject::WasmGlobalObjectVerify(Isolate* isolate) {
  CHECK(IsWasmGlobalObject(this));
  Object::VerifyPointer(isolate, buffer());
}

#endif  // V8_ENABLE_WEBASSEMBLY

void TurboshaftWord32Type::TurboshaftWord32TypeVerify(Isolate* isolate) {
  CHECK(IsTurboshaftType(this));
  CHECK(IsTurboshaftWord32Type(this));
}
void TurboshaftWord32RangeType::TurboshaftWord32RangeTypeVerify(
    Isolate* isolate) {
  TurboshaftWord32TypeVerify(isolate);
  CHECK(IsTurboshaftWord32RangeType(this));
}
void TurboshaftWord32SetType::TurboshaftWord32SetTypeVerify(Isolate* isolate) {
  TurboshaftWord32TypeVerify(isolate);
  CHECK(IsTurboshaftWord32SetType(this));
}
void TurboshaftWord64Type::TurboshaftWord64TypeVerify(Isolate* isolate) {
  CHECK(IsTurboshaftType(this));
  CHECK(IsTurboshaftWord64Type(this));
}
void TurboshaftWord64RangeType::TurboshaftWord64RangeTypeVerify(
    Isolate* isolate) {
  TurboshaftWord64TypeVerify(isolate);
  CHECK(IsTurboshaftWord64RangeType(this));
}
void TurboshaftWord64SetType::TurboshaftWord64SetTypeVerify(Isolate* isolate) {
  TurboshaftWord64TypeVerify(isolate);
  CHECK(IsTurboshaftWord64SetType(this));
}
void TurboshaftFloat64Type::TurboshaftFloat64TypeVerify(Isolate* isolate) {
  CHECK(IsTurboshaftType(this));
  CHECK(IsTurboshaftFloat64Type(this));
}
void TurboshaftFloat64RangeType::TurboshaftFloat64RangeTypeVerify(
    Isolate* isolate) {
  TurboshaftFloat64TypeVerify(isolate);
  CHECK(IsTurboshaftFloat64RangeType(this));
}
void TurboshaftFloat64SetType::TurboshaftFloat64SetTypeVerify(
    Isolate* isolate) {
  TurboshaftFloat64TypeVerify(isolate);
  CHECK(IsTurboshaftFloat64SetType(this));
}

void TurbofanBitsetType::TurbofanBitsetTypeVerify(Isolate* isolate) {
  CHECK(IsTurbofanType(this));
  CHECK(IsTurbofanBitsetType(this));
}
void TurbofanUnionType::TurbofanUnionTypeVerify(Isolate* isolate) {
  CHECK(IsTurbofanType(this));
  CHECK(IsTurbofanUnionType(this));
  Object::VerifyPointer(isolate, type1_.load());
  CHECK(IsTurbofanType(type1_.load()));
  Object::VerifyPointer(isolate, type2_.load());
  CHECK(IsTurbofanType(type2_.load()));
}
void TurbofanRangeType::TurbofanRangeTypeVerify(Isolate* isolate) {
  CHECK(IsTurbofanType(this));
  CHECK(IsTurbofanRangeType(this));
}
void TurbofanHeapConstantType::TurbofanHeapConstantTypeVerify(
    Isolate* isolate) {
  CHECK(IsTurbofanType(this));
  CHECK(IsTurbofanHeapConstantType(this));
  Object::VerifyPointer(isolate, constant_.load());
  CHECK(IsHeapObject(constant_.load()));
}
void TurbofanOtherNumberConstantType::TurbofanOtherNumberConstantTypeVerify(
    Isolate* isolate) {
  CHECK(IsTurbofanType(this));
  CHECK(IsTurbofanOtherNumberConstantType(this));
}

void SortState::SortStateVerify(Isolate* isolate) {
  CHECK(IsSortState(this));
  Object::VerifyPointer(isolate, receiver_.load());
  Object::VerifyPointer(isolate, initial_receiver_map_.load());
  Object::VerifyPointer(isolate, initial_receiver_length_.load());
  Object::VerifyPointer(isolate, user_cmp_fn_.load());
  Object::VerifyPointer(isolate, is_reset_to_generic_.load());
  Object::VerifyPointer(isolate, pending_runs_.load());
  Object::VerifyPointer(isolate, pending_powers_.load());
  Object::VerifyPointer(isolate, work_array_.load());
  Object::VerifyPointer(isolate, temp_array_.load());
}

void OnHeapBasicBlockProfilerData::OnHeapBasicBlockProfilerDataVerify(
    Isolate* isolate) {
  CHECK(IsOnHeapBasicBlockProfilerData(this));
  Object::VerifyPointer(isolate, block_ids());
  Object::VerifyPointer(isolate, counts());
  Object::VerifyPointer(isolate, branches());
  Object::VerifyPointer(isolate, name());
  Object::VerifyPointer(isolate, schedule());
  Object::VerifyPointer(isolate, code());
  CHECK(IsSmi(hash()));
}

#if V8_ENABLE_WEBASSEMBLY
void WasmFastApiCallData::WasmFastApiCallDataVerify(Isolate* isolate) {
  CHECK(IsWasmFastApiCallData(this));
  Object::VerifyPointer(isolate, signature());
  Object::VerifyPointer(isolate, callback_data());
  Object::VerifyMaybeObjectPointer(isolate, cached_map());
}

void WasmStringViewIter::WasmStringViewIterVerify(Isolate* isolate) {
  CHECK(IsWasmStringViewIter(this));
  Object::VerifyPointer(isolate, string());
  CHECK(IsString(string()));
}
#endif  // V8_ENABLE_WEBASSEMBLY

void FunctionTemplateRareData::FunctionTemplateRareDataVerify(
    Isolate* isolate) {
  CHECK(IsStruct(this));
  CHECK(IsFunctionTemplateRareData(this));
  Object::VerifyPointer(isolate, prototype_template());
  Object::VerifyPointer(isolate, prototype_provider_template());
  Object::VerifyPointer(isolate, parent_template());
  Object::VerifyPointer(isolate, named_property_handler());
  Object::VerifyPointer(isolate, indexed_property_handler());
  Object::VerifyPointer(isolate, instance_template());
  Object::VerifyPointer(isolate, instance_call_handler());
  Object::VerifyPointer(isolate, access_check_info());
  Object::VerifyPointer(isolate, c_function_overloads());
  CHECK(IsFixedArray(c_function_overloads()) ||
        IsUndefined(c_function_overloads(), isolate));
}

void PrototypeSharedClosureInfo::PrototypeSharedClosureInfoVerify(
    Isolate* isolate) {
  CHECK(IsStruct(this));
  CHECK(IsPrototypeSharedClosureInfo(this));
  Object::VerifyPointer(isolate, boilerplate_description());
  Object::VerifyPointer(isolate, closure_feedback_cell_array());
  Object::VerifyPointer(isolate, context());
}

void Tuple2::Tuple2Verify(Isolate* isolate) {
  CHECK(IsStruct(this));
  CHECK(IsTuple2(this));
  Object::VerifyPointer(isolate, value1_.load());
  Object::VerifyPointer(isolate, value2_.load());
}

void AliasedArgumentsEntry::AliasedArgumentsEntryVerify(Isolate* isolate) {
  CHECK(IsStruct(this));
  CHECK(IsAliasedArgumentsEntry(this));
}

void AccessorPair::AccessorPairVerify(Isolate* isolate) {
  CHECK(IsStruct(this));
  CHECK(IsAccessorPair(this));
  Object::VerifyPointer(isolate, getter_.load());
  Object::VerifyPointer(isolate, setter_.load());
}

void ClassPositions::ClassPositionsVerify(Isolate* isolate) {
  CHECK(IsStruct(this));
  CHECK(IsClassPositions(this));
  CHECK(IsSmi(Tagged<Object>(start_.load())));
  CHECK(IsSmi(Tagged<Object>(end_.load())));
}

void DebugInfo::DebugInfoVerify(Isolate* isolate) {
  CHECK(IsStruct(this));
  CHECK(IsDebugInfo(this));
  Object::VerifyPointer(isolate, shared());
  Object::VerifyPointer(isolate, break_points());
  Object::VerifyPointer(isolate, coverage_info());
  CHECK_IMPLIES(has_original_bytecode_array(),
                IsBytecodeArray(original_bytecode_array(isolate)));
  CHECK_IMPLIES(has_debug_bytecode_array(),
                IsBytecodeArray(debug_bytecode_array(isolate)));
}

void BreakPointInfo::BreakPointInfoVerify(Isolate* isolate) {
  CHECK(IsStruct(this));
  CHECK(IsBreakPointInfo(this));
  CHECK(IsSmi(Tagged<Object>(source_position_.load())));
  Object::VerifyPointer(isolate, break_points_.load());
}

void BreakPoint::BreakPointVerify(Isolate* isolate) {
  CHECK(IsStruct(this));
  CHECK(IsBreakPoint(this));
  CHECK(IsSmi(Tagged<Object>(id_.load())));
  Object::VerifyPointer(isolate, condition_.load());
}

void FunctionTemplateInfo::FunctionTemplateInfoVerify(Isolate* isolate) {
  CHECK(IsFunctionTemplateInfo(this));
  Object::VerifyPointer(isolate, class_name_.load());
  Object::VerifyPointer(isolate, interface_name_.load());
  Object::VerifyPointer(isolate, signature_.load());
  Object::VerifyPointer(isolate, rare_data_.load());
  Object::VerifyPointer(isolate, shared_function_info_.load());
  Object::VerifyPointer(isolate, cached_property_name_.load());
  Object::VerifyPointer(isolate, callback_data_.load());
}

void ObjectTemplateInfo::ObjectTemplateInfoVerify(Isolate* isolate) {
  CHECK(IsObjectTemplateInfo(this));
  Object::VerifyPointer(isolate, constructor_.load());
  CHECK(IsSmi(Tagged<Object>(data_.load())));
}

void DictionaryTemplateInfo::DictionaryTemplateInfoVerify(Isolate* isolate) {
  CHECK(IsDictionaryTemplateInfo(this));
  Object::VerifyPointer(isolate, property_names_.load());
}

void DataHandler::DataHandlerVerify(Isolate* isolate) {
  CHECK(IsStruct(this));
  CHECK(IsDataHandler(this));
  Object::VerifyPointer(isolate, smi_handler());
  CHECK_IMPLIES(!IsSmi(smi_handler()),
                IsStoreHandler(this) && IsCode(smi_handler()));
  Object::VerifyPointer(isolate, validity_cell());
  CHECK(IsSmi(validity_cell()) || IsCell(validity_cell()));
  for (int i = 0; i < data_field_count(); ++i) {
    Object::VerifyMaybeObjectPointer(isolate, data()[i].load());
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

void AllocationSite::AllocationSiteVerify(Isolate* isolate) {
  CHECK(IsAllocationSite(this));
  CHECK(IsDependentCode(dependent_code()));
  if (PointsToLiteral()) {
    CHECK(IsJSObject(transition_info_or_boilerplate_.load()));
  } else {
    CHECK(IsSmi(transition_info_or_boilerplate_.load()));
  }
  CHECK(IsAllocationSite(nested_site()) || nested_site() == Smi::zero());
}

void AllocationMemento::AllocationMementoVerify(Isolate* isolate) {
  CHECK(IsStruct(this));
  CHECK(IsAllocationMemento(this));
  CHECK(IsAllocationSite(allocation_site_.load()));
}

void Script::ScriptVerify(Isolate* isolate) {
  CHECK(IsScript(this));
  Object::VerifyPointer(isolate, source());
  Object::VerifyPointer(isolate, name());
  Object::VerifyPointer(isolate, context_data());
  Object::VerifyPointer(isolate, line_ends());
  Object::VerifyPointer(isolate, eval_from_shared_or_wrapped_arguments());
  Object::VerifyPointer(isolate, eval_from_position_.load());
  Object::VerifyPointer(isolate, eval_from_scope_info());
  Object::VerifyPointer(isolate, infos_.load());
  Object::VerifyPointer(isolate, compiled_lazy_function_positions());
  Object::VerifyPointer(isolate, source_url());
  Object::VerifyPointer(isolate, source_mapping_url());
  Object::VerifyPointer(isolate, debug_id());
  Object::VerifyPointer(isolate, host_defined_options());
#if V8_SCRIPTORMODULE_LEGACY_LIFETIME
  Object::VerifyPointer(isolate, script_or_modules());
#endif
  Object::VerifyPointer(isolate, source_hash());

#if V8_ENABLE_WEBASSEMBLY
  if (type() == Script::Type::kWasm) {
    CHECK_EQ(line_ends(), ReadOnlyRoots(isolate).empty_fixed_array());
  } else {
    CHECK(CanHaveLineEnds());
  }
#else   // V8_ENABLE_WEBASSEMBLY
  CHECK(CanHaveLineEnds());
#endif  // V8_ENABLE_WEBASSEMBLY
  uint32_t infos_len = infos()->ulength().value();
  for (uint32_t i = 0; i < infos_len; ++i) {
    Tagged<MaybeObject> maybe_object = infos()->get(i);
    Tagged<HeapObject> heap_object;
    CHECK(!maybe_object.GetHeapObjectIfWeak(isolate, &heap_object) ||
          (maybe_object.GetHeapObjectIfStrong(&heap_object) &&
           IsUndefined(heap_object, isolate)) ||
          Is<SharedFunctionInfo>(heap_object) || Is<ScopeInfo>(heap_object));
  }
}

void NormalizedMapCache::NormalizedMapCacheVerify(Isolate* isolate) {
  Cast<WeakFixedArray>(this)->WeakFixedArrayVerify(isolate);
  if (v8_flags.enable_slow_asserts) {
    uint32_t len = ulength().value();
    for (uint32_t i = 0; i < len; i++) {
      Tagged<MaybeObject> e = WeakFixedArray::get(i);
      Tagged<HeapObject> heap_object;
      if (e.GetHeapObjectIfWeak(&heap_object)) {
        Cast<Map>(heap_object)->DictionaryMapVerify(isolate);
      } else {
        CHECK(e.IsCleared() || (e.GetHeapObjectIfStrong(&heap_object) &&
                                IsUndefined(heap_object, isolate)));
      }
    }
  }
}

void PreparseData::PreparseDataVerify(Isolate* isolate) {
  CHECK(IsPreparseData(this));
  CHECK_LE(0, data_length());
  CHECK_LE(0, children_length());

  for (int i = 0; i < children_length(); ++i) {
    Tagged<Object> child = children()[i].Relaxed_Load();
    // Preparse data is created recursively, depth first, so if there is a heap
    // verification during preparse data tree creation, it will observe a
    // partially initialized preparse data with null values.
    CHECK(IsNull(child) || IsPreparseData(child));
    Object::VerifyPointer(isolate, child);
  }
}

void UncompiledData::UncompiledDataVerify(Isolate* isolate) {
  CHECK(IsUncompiledData(this));
  {
    Tagged<Object> inferred_name = this->inferred_name();
    Object::VerifyPointer(isolate, inferred_name);
    CHECK(IsString(inferred_name));
  }
}
void UncompiledDataWithoutPreparseData::UncompiledDataWithoutPreparseDataVerify(
    Isolate* isolate) {
  UncompiledDataVerify(isolate);
  CHECK(IsUncompiledDataWithoutPreparseData(this));
}
void UncompiledDataWithPreparseData::UncompiledDataWithPreparseDataVerify(
    Isolate* isolate) {
  UncompiledDataVerify(isolate);
  CHECK(IsUncompiledDataWithPreparseData(this));
  {
    Tagged<Object> preparse_data = this->preparse_data();
    Object::VerifyPointer(isolate, preparse_data);
    CHECK(IsPreparseData(preparse_data));
  }
}
void UncompiledDataWithoutPreparseDataWithJob::
    UncompiledDataWithoutPreparseDataWithJobVerify(Isolate* isolate) {
  UncompiledDataWithoutPreparseDataVerify(isolate);
  CHECK(IsUncompiledDataWithoutPreparseDataWithJob(this));
}
void UncompiledDataWithPreparseDataAndJob::
    UncompiledDataWithPreparseDataAndJobVerify(Isolate* isolate) {
  UncompiledDataWithPreparseDataVerify(isolate);
  CHECK(IsUncompiledDataWithPreparseDataAndJob(this));
}

void CallSiteInfo::CallSiteInfoVerify(Isolate* isolate) {
  CHECK(IsSmi(flags_.load()));
  CHECK(IsSmi(code_offset_or_source_position_.load()));
  Object::VerifyPointer(isolate, receiver_or_instance_.load());
  Object::VerifyPointer(isolate, function_.load());

  Tagged<Object> code = code_object_.load_maybe_empty(isolate);
  CHECK(IsCode(code) || IsBytecodeArray(code) || code == Smi::zero());

#if V8_ENABLE_WEBASSEMBLY
  CHECK_IMPLIES(IsAsmJsWasm(), IsWasm());
  CHECK_IMPLIES(IsWasm(), IsWasmInstanceObject(receiver_or_instance()));
  CHECK_IMPLIES(IsWasm() || IsBuiltin(), IsSmi(function()));
  CHECK_IMPLIES(!IsWasm() && !IsBuiltin(), IsJSFunction(function()));
  CHECK_IMPLIES(IsAsync(), !IsWasm());
  CHECK_IMPLIES(IsConstructor(), !IsWasm());
#endif  // V8_ENABLE_WEBASSEMBLY
}

void StackFrameInfo::StackFrameInfoVerify(Isolate* isolate) {
  CHECK(IsStruct(this));
  CHECK(IsStackFrameInfo(this));
  Object::VerifyPointer(isolate, shared_or_script_.load());
  Object::VerifyPointer(isolate, function_name_.load());
  CHECK(IsSmi(Tagged<Object>(flags_.load())));
}

void StackTraceInfo::StackTraceInfoVerify(Isolate* isolate) {
  CHECK(IsStruct(this));
  CHECK(IsStackTraceInfo(this));
  CHECK(IsSmi(Tagged<Object>(id_.load())));
  Object::VerifyPointer(isolate, frames_.load());
}

void ErrorStackData::ErrorStackDataVerify(Isolate* isolate) {
  CHECK(IsStruct(this));
  CHECK(IsErrorStackData(this));
  Object::VerifyPointer(
      isolate, raw_data_for_call_site_infos_or_formatted_stack_.load());
  Object::VerifyPointer(isolate, stack_trace_.load());
}

void SloppyArgumentsElements::SloppyArgumentsElementsVerify(Isolate* isolate) {
  CHECK_LE(length_, kMaxCapacity);
  {
    auto o = context();
    Object::VerifyPointer(isolate, o);
    CHECK(IsContext(o));
  }
  {
    auto o = arguments();
    Object::VerifyPointer(isolate, o);
    CHECK(IsFixedArray(o));
  }
  uint32_t len = ulength().value();
  for (uint32_t i = 0; i < len; ++i) {
    auto o = mapped_entries(i, kRelaxedLoad);
    CHECK(IsSmi(o) || IsTheHole(o));
  }
}

// Helper class for verifying the string table.
class StringTableVerifier : public RootVisitor {
 public:
  explicit StringTableVerifier(Isolate* isolate) : isolate_(isolate) {}

  void VisitRootPointers(Root root, const char* description,
                         FullObjectSlot start, FullObjectSlot end) override {
    UNREACHABLE();
  }
  void VisitCompressedRootPointers(Root root, const char* description,
                                   OffHeapObjectSlot start,
                                   OffHeapObjectSlot end) override {
    // Visit all HeapObject pointers in [start, end).
    for (OffHeapObjectSlot p = start; p < end; ++p) {
      Tagged<Object> o = p.load(isolate_);
      CHECK(!HasWeakHeapObjectTag(o));
      if (IsHeapObject(o)) {
        Tagged<HeapObject> object = Cast<HeapObject>(o);
        // Check that the string is actually internalized.
        CHECK(IsInternalizedString(object));
        CHECK_IMPLIES(v8_flags.shared_string_table,
                      HeapLayout::InAnySharedSpace(object));
        // Internalized forwarding indices are never allowed in the string
        // table.
        uint32_t raw_hash = Cast<Name>(object)->raw_hash_field();
        CHECK(!Name::IsInternalizedForwardingIndex(raw_hash));
      }
    }
  }

 private:
  Isolate* isolate_;
};

void StringTable::VerifyIfOwnedBy(Isolate* isolate) {
  CHECK_EQ(isolate->string_table(), this);
  if (!isolate->OwnsStringTables()) return;
  StringTableVerifier verifier(isolate);
  IterateElements(&verifier);
}

#endif  // VERIFY_HEAP

#ifdef DEBUG

void JSObject::IncrementSpillStatistics(Isolate* isolate,
                                        SpillInformation* info) {
  info->number_of_objects_++;
  // Named properties
  if (HasFastProperties()) {
    info->number_of_objects_with_fast_properties_++;
    info->number_of_fast_used_fields_ +=
        map()->GetInObjectProperties() + property_array()->length().value();
    info->number_of_fast_unused_fields_ += map()->UnusedPropertyFields();
  } else if (IsJSGlobalObject(this)) {
    Tagged<GlobalDictionary> dict =
        Cast<JSGlobalObject>(this)->global_dictionary(kAcquireLoad);
    info->number_of_slow_used_properties_ += dict->NumberOfElements();
    info->number_of_slow_unused_properties_ +=
        dict->Capacity() - dict->NumberOfElements();
  } else if (V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
    Tagged<SwissNameDictionary> dict = property_dictionary_swiss();
    info->number_of_slow_used_properties_ += dict->NumberOfElements();
    info->number_of_slow_unused_properties_ +=
        dict->Capacity() - dict->NumberOfElements();
  } else {
    Tagged<NameDictionary> dict = property_dictionary();
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
    case HOLEY_FROZEN_ELEMENTS:
    case HOLEY_SEALED_ELEMENTS:
    case HOLEY_NONEXTENSIBLE_ELEMENTS:
    case PACKED_ELEMENTS:
    case PACKED_FROZEN_ELEMENTS:
    case PACKED_SEALED_ELEMENTS:
    case PACKED_NONEXTENSIBLE_ELEMENTS:
    case FAST_STRING_WRAPPER_ELEMENTS:
    case SHARED_ARRAY_ELEMENTS: {
      info->number_of_objects_with_fast_elements_++;
      int holes = 0;
      Tagged<FixedArray> e = Cast<FixedArray>(elements());
      uint32_t len = e->ulength().value();
      for (uint32_t i = 0; i < len; i++) {
        if (IsTheHole(e->get(i), isolate)) holes++;
      }
      info->number_of_fast_used_elements_ += len - holes;
      info->number_of_fast_unused_elements_ += holes;
      break;
    }

#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype) case TYPE##_ELEMENTS:

      TYPED_ARRAYS(TYPED_ARRAY_CASE)
      RAB_GSAB_TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
      {
        info->number_of_objects_with_fast_elements_++;
        Tagged<FixedArrayBase> e = Cast<FixedArrayBase>(elements());
        info->number_of_fast_used_elements_ +=
            static_cast<int>(e->ulength().value());
        break;
      }
    case DICTIONARY_ELEMENTS:
    case SLOW_STRING_WRAPPER_ELEMENTS: {
      Tagged<NumberDictionary> dict = element_dictionary();
      info->number_of_slow_used_elements_ += dict->NumberOfElements();
      info->number_of_slow_unused_elements_ +=
          dict->Capacity() - dict->NumberOfElements();
      break;
    }
    case FAST_SLOPPY_ARGUMENTS_ELEMENTS:
    case SLOW_SLOPPY_ARGUMENTS_ELEMENTS:
    case WASM_ARRAY_ELEMENTS:
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

bool DescriptorArray::IsSortedNoDuplicates() {
  // Up to the linear search limit the array is not sorted and that's fine.
  if (number_of_descriptors() <= kMaxElementsForLinearSearch) return true;
  Tagged<Name> current_key;
  uint32_t current = 0;
  for (int i = 0; i < number_of_descriptors(); i++) {
    Tagged<Name> key = GetSortedKey(i);
    uint32_t hash;
    const bool has_hash = key->TryGetHash(&hash);
    CHECK(has_hash);
    if (key == current_key) {
      Print(this);
      return false;
    }
    current_key = key;
    if (hash < current) {
      Print(this);
      return false;
    }
    current = hash;
  }
  return true;
}

bool TransitionArray::IsSortedNoDuplicates() {
  // Up to the linear search limit the array is not sorted and that's fine.
  if (number_of_transitions() <= kMaxElementsForLinearSearch) return true;
  Tagged<Name> prev_key;
  PropertyKind prev_kind = PropertyKind::kData;
  PropertyAttributes prev_attributes = NONE;
  uint32_t prev_hash = 0;

  for (int i = 0; i < number_of_transitions(); i++) {
    Tagged<Name> key = GetKey(i);
    uint32_t hash;
    const bool has_hash = key->TryGetHash(&hash);
    CHECK(has_hash);
    PropertyKind kind = PropertyKind::kData;
    PropertyAttributes attributes = NONE;
    if (!TransitionsAccessor::IsSpecialTransition(GetReadOnlyRoots(), key)) {
      Tagged<Map> target = GetTarget(i);
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
      Print(this);
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

static bool CheckOneBackPointer(Tagged<Map> current_map, Tagged<Map> target) {
  return target->GetBackPointer() == current_map;
}

bool TransitionsAccessor::IsConsistentWithBackPointers() {
  DisallowGarbageCollection no_gc;
  bool success = true;
  ReadOnlyRoots roots(isolate_);
  DCHECK_IMPLIES(map_->IsInobjectSlackTrackingInProgress(),
                 !HasSideStepTransitions());
  auto CheckTarget =
      [&](Tagged<Map> target) {
#ifdef DEBUG
        if (!map_->is_deprecated() && !target->is_deprecated()) {
          DCHECK_EQ(map_->IsInobjectSlackTrackingInProgress(),
                    target->IsInobjectSlackTrackingInProgress());
          // Check prototype transitions are first.
          DCHECK_IMPLIES(map_->prototype() != target->prototype(),
                         IsUndefined(map_->GetBackPointer()));
        }
        DCHECK_EQ(target->map(), map_->map());
#endif  // DEBUG
        if (!CheckOneBackPointer(map_, target)) {
          success = false;
        }
      };
  ForEachTransition(
      &no_gc, [&](Tagged<Map> target) { CheckTarget(target); },
      [&](Tagged<Object> side_step) {
        if (!side_step.IsSmi()) {
          DCHECK_EQ(Cast<Map>(side_step)->map(), map_->map());
          DCHECK(!Cast<Map>(side_step)->IsInobjectSlackTrackingInProgress());
          DCHECK_EQ(
              Cast<Map>(side_step)->GetInObjectProperties() -
                  Cast<Map>(side_step)->UnusedInObjectProperties(),
              map_->GetInObjectProperties() - map_->UnusedInObjectProperties());
        }
      });
  return success;
}

#endif  // DEBUG

}  // namespace internal
}  // namespace v8
