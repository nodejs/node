// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/map.h"

#include "src/execution/frames.h"
#include "src/execution/isolate.h"
#include "src/handles/handles-inl.h"
#include "src/handles/maybe-handles.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/init/bootstrapper.h"
#include "src/logging/counters-inl.h"
#include "src/logging/log.h"
#include "src/objects/arguments-inl.h"
#include "src/objects/descriptor-array.h"
#include "src/objects/elements-kind.h"
#include "src/objects/field-type.h"
#include "src/objects/js-objects.h"
#include "src/objects/map-updater.h"
#include "src/objects/maybe-object.h"
#include "src/objects/oddball.h"
#include "src/objects/property.h"
#include "src/objects/transitions-inl.h"
#include "src/roots/roots.h"
#include "src/utils/ostreams.h"
#include "src/zone/zone-containers.h"
#include "torque-generated/field-offsets.h"

namespace v8 {
namespace internal {

Map Map::GetPrototypeChainRootMap(Isolate* isolate) const {
  DisallowGarbageCollection no_alloc;
  if (IsJSReceiverMap()) {
    return *this;
  }
  int constructor_function_index = GetConstructorFunctionIndex();
  if (constructor_function_index != Map::kNoConstructorFunctionIndex) {
    Context native_context = isolate->context().native_context();
    JSFunction constructor_function =
        JSFunction::cast(native_context.get(constructor_function_index));
    return constructor_function.initial_map();
  }
  return ReadOnlyRoots(isolate).null_value().map();
}

// static
MaybeHandle<JSFunction> Map::GetConstructorFunction(
    Handle<Map> map, Handle<Context> native_context) {
  if (map->IsPrimitiveMap()) {
    int const constructor_function_index = map->GetConstructorFunctionIndex();
    if (constructor_function_index != kNoConstructorFunctionIndex) {
      return handle(
          JSFunction::cast(native_context->get(constructor_function_index)),
          native_context->GetIsolate());
    }
  }
  return MaybeHandle<JSFunction>();
}

void Map::PrintReconfiguration(Isolate* isolate, FILE* file,
                               InternalIndex modify_index, PropertyKind kind,
                               PropertyAttributes attributes) {
  OFStream os(file);
  os << "[reconfiguring]";
  Name name = instance_descriptors(isolate).GetKey(modify_index);
  if (name.IsString()) {
    String::cast(name).PrintOn(file);
  } else {
    os << "{symbol " << reinterpret_cast<void*>(name.ptr()) << "}";
  }
  os << ": " << (kind == kData ? "kData" : "ACCESSORS") << ", attrs: ";
  os << attributes << " [";
  JavaScriptFrame::PrintTop(isolate, file, false, true);
  os << "]\n";
}

Map Map::GetInstanceTypeMap(ReadOnlyRoots roots, InstanceType type) {
  Map map;
  switch (type) {
#define MAKE_CASE(TYPE, Name, name) \
  case TYPE:                        \
    map = roots.name##_map();       \
    break;
    STRUCT_LIST(MAKE_CASE)
#undef MAKE_CASE
#define MAKE_CASE(TYPE, Name, name) \
  case TYPE:                        \
    map = roots.name##_map();       \
    break;
    TORQUE_DEFINED_INSTANCE_TYPE_LIST(MAKE_CASE)
#undef MAKE_CASE
    default:
      UNREACHABLE();
  }
  return map;
}

VisitorId Map::GetVisitorId(Map map) {
  STATIC_ASSERT(kVisitorIdCount <= 256);

  const int instance_type = map.instance_type();

  if (instance_type < FIRST_NONSTRING_TYPE) {
    switch (instance_type & kStringRepresentationMask) {
      case kSeqStringTag:
        if ((instance_type & kStringEncodingMask) == kOneByteStringTag) {
          return kVisitSeqOneByteString;
        } else {
          return kVisitSeqTwoByteString;
        }

      case kConsStringTag:
        if (IsShortcutCandidate(instance_type)) {
          return kVisitShortcutCandidate;
        } else {
          return kVisitConsString;
        }

      case kSlicedStringTag:
        return kVisitSlicedString;

      case kExternalStringTag:
        return kVisitDataObject;

      case kThinStringTag:
        return kVisitThinString;
    }
    UNREACHABLE();
  }

  switch (instance_type) {
    case BYTE_ARRAY_TYPE:
      return kVisitByteArray;

    case BYTECODE_ARRAY_TYPE:
      return kVisitBytecodeArray;

    case FREE_SPACE_TYPE:
      return kVisitFreeSpace;

    case EMBEDDER_DATA_ARRAY_TYPE:
      return kVisitEmbedderDataArray;

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
    case SCRIPT_CONTEXT_TABLE_TYPE:
      return kVisitFixedArray;

    case AWAIT_CONTEXT_TYPE:
    case BLOCK_CONTEXT_TYPE:
    case CATCH_CONTEXT_TYPE:
    case DEBUG_EVALUATE_CONTEXT_TYPE:
    case EVAL_CONTEXT_TYPE:
    case FUNCTION_CONTEXT_TYPE:
    case MODULE_CONTEXT_TYPE:
    case SCRIPT_CONTEXT_TYPE:
    case WITH_CONTEXT_TYPE:
      return kVisitContext;

    case NATIVE_CONTEXT_TYPE:
      return kVisitNativeContext;

    case EPHEMERON_HASH_TABLE_TYPE:
      return kVisitEphemeronHashTable;

    case FIXED_DOUBLE_ARRAY_TYPE:
      return kVisitFixedDoubleArray;

    case PROPERTY_ARRAY_TYPE:
      return kVisitPropertyArray;

    case FEEDBACK_CELL_TYPE:
      return kVisitFeedbackCell;

    case FEEDBACK_METADATA_TYPE:
      return kVisitFeedbackMetadata;

    case MAP_TYPE:
      return kVisitMap;

    case CODE_TYPE:
      return kVisitCode;

    case CELL_TYPE:
      return kVisitCell;

    case PROPERTY_CELL_TYPE:
      return kVisitPropertyCell;

    case TRANSITION_ARRAY_TYPE:
      return kVisitTransitionArray;

    case JS_WEAK_MAP_TYPE:
    case JS_WEAK_SET_TYPE:
      return kVisitJSWeakCollection;

    case CALL_HANDLER_INFO_TYPE:
      return kVisitStruct;

    case JS_PROXY_TYPE:
      return kVisitStruct;

    case SYMBOL_TYPE:
      return kVisitSymbol;

    case JS_ARRAY_BUFFER_TYPE:
      return kVisitJSArrayBuffer;

    case JS_DATA_VIEW_TYPE:
      return kVisitJSDataView;

    case JS_FUNCTION_TYPE:
    case JS_PROMISE_CONSTRUCTOR_TYPE:
    case JS_REG_EXP_CONSTRUCTOR_TYPE:
    case JS_ARRAY_CONSTRUCTOR_TYPE:
#define TYPED_ARRAY_CONSTRUCTORS_SWITCH(Type, type, TYPE, Ctype) \
  case TYPE##_TYPED_ARRAY_CONSTRUCTOR_TYPE:
      TYPED_ARRAYS(TYPED_ARRAY_CONSTRUCTORS_SWITCH)
#undef TYPED_ARRAY_CONSTRUCTORS_SWITCH
      return kVisitJSFunction;

    case JS_TYPED_ARRAY_TYPE:
      return kVisitJSTypedArray;

    case SMALL_ORDERED_HASH_MAP_TYPE:
      return kVisitSmallOrderedHashMap;

    case SMALL_ORDERED_HASH_SET_TYPE:
      return kVisitSmallOrderedHashSet;

    case SMALL_ORDERED_NAME_DICTIONARY_TYPE:
      return kVisitSmallOrderedNameDictionary;

    case SWISS_NAME_DICTIONARY_TYPE:
      return kVisitSwissNameDictionary;

    case CODE_DATA_CONTAINER_TYPE:
      return kVisitCodeDataContainer;

    case PREPARSE_DATA_TYPE:
      return kVisitPreparseData;

    case COVERAGE_INFO_TYPE:
      return kVisitCoverageInfo;

    case JS_ARGUMENTS_OBJECT_TYPE:
    case JS_ARRAY_ITERATOR_PROTOTYPE_TYPE:
    case JS_ARRAY_ITERATOR_TYPE:
    case JS_ARRAY_TYPE:
    case JS_ASYNC_FROM_SYNC_ITERATOR_TYPE:
    case JS_ASYNC_FUNCTION_OBJECT_TYPE:
    case JS_ASYNC_GENERATOR_OBJECT_TYPE:
    case JS_CONTEXT_EXTENSION_OBJECT_TYPE:
    case JS_DATE_TYPE:
    case JS_ERROR_TYPE:
    case JS_FINALIZATION_REGISTRY_TYPE:
    case JS_GENERATOR_OBJECT_TYPE:
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
    case JS_STRING_ITERATOR_PROTOTYPE_TYPE:
    case JS_STRING_ITERATOR_TYPE:
    case JS_TYPED_ARRAY_PROTOTYPE_TYPE:
#ifdef V8_INTL_SUPPORT
    case JS_V8_BREAK_ITERATOR_TYPE:
    case JS_COLLATOR_TYPE:
    case JS_DATE_TIME_FORMAT_TYPE:
    case JS_DISPLAY_NAMES_TYPE:
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
    case WASM_EXCEPTION_OBJECT_TYPE:
    case WASM_GLOBAL_OBJECT_TYPE:
    case WASM_MEMORY_OBJECT_TYPE:
    case WASM_MODULE_OBJECT_TYPE:
    case WASM_TABLE_OBJECT_TYPE:
    case WASM_VALUE_OBJECT_TYPE:
#endif  // V8_ENABLE_WEBASSEMBLY
    case JS_BOUND_FUNCTION_TYPE: {
      const bool has_raw_data_fields =
          COMPRESS_POINTERS_BOOL && JSObject::GetEmbedderFieldCount(map) > 0;
      return has_raw_data_fields ? kVisitJSObject : kVisitJSObjectFast;
    }
    case JS_API_OBJECT_TYPE:
    case JS_GLOBAL_PROXY_TYPE:
    case JS_GLOBAL_OBJECT_TYPE:
    case JS_SPECIAL_API_OBJECT_TYPE:
      return kVisitJSApiObject;

    case JS_WEAK_REF_TYPE:
      return kVisitJSWeakRef;

    case WEAK_CELL_TYPE:
      return kVisitWeakCell;

    case FILLER_TYPE:
    case FOREIGN_TYPE:
    case HEAP_NUMBER_TYPE:
      return kVisitDataObject;

    case BIGINT_TYPE:
      return kVisitBigInt;

    case ALLOCATION_SITE_TYPE:
      return kVisitAllocationSite;

#define MAKE_STRUCT_CASE(TYPE, Name, name) case TYPE:
      STRUCT_LIST(MAKE_STRUCT_CASE)
#undef MAKE_STRUCT_CASE
      if (instance_type == PROTOTYPE_INFO_TYPE) {
        return kVisitPrototypeInfo;
      }
#if V8_ENABLE_WEBASSEMBLY
      if (instance_type == WASM_CAPI_FUNCTION_DATA_TYPE) {
        return kVisitWasmCapiFunctionData;
      }
      if (instance_type == WASM_INDIRECT_FUNCTION_TABLE_TYPE) {
        return kVisitWasmIndirectFunctionTable;
      }
#endif  // V8_ENABLE_WEBASSEMBLY
      return kVisitStruct;

    case LOAD_HANDLER_TYPE:
    case STORE_HANDLER_TYPE:
      return kVisitDataHandler;

    case SOURCE_TEXT_MODULE_TYPE:
      return kVisitSourceTextModule;
    case SYNTHETIC_MODULE_TYPE:
      return kVisitSyntheticModule;

#if V8_ENABLE_WEBASSEMBLY
    case WASM_INSTANCE_OBJECT_TYPE:
      return kVisitWasmInstanceObject;
    case WASM_ARRAY_TYPE:
      return kVisitWasmArray;
    case WASM_STRUCT_TYPE:
      return kVisitWasmStruct;
    case WASM_TYPE_INFO_TYPE:
      return kVisitWasmTypeInfo;
#endif  // V8_ENABLE_WEBASSEMBLY

#define MAKE_TQ_CASE(TYPE, Name) \
  case TYPE:                     \
    return kVisit##Name;
      TORQUE_INSTANCE_TYPE_TO_BODY_DESCRIPTOR_LIST(MAKE_TQ_CASE)
#undef MAKE_TQ_CASE

    default:
      UNREACHABLE();
  }
}

void Map::PrintGeneralization(
    Isolate* isolate, FILE* file, const char* reason,
    InternalIndex modify_index, int split, int descriptors,
    bool descriptor_to_field, Representation old_representation,
    Representation new_representation, PropertyConstness old_constness,
    PropertyConstness new_constness, MaybeHandle<FieldType> old_field_type,
    MaybeHandle<Object> old_value, MaybeHandle<FieldType> new_field_type,
    MaybeHandle<Object> new_value) {
  OFStream os(file);
  os << "[generalizing]";
  Name name = instance_descriptors(isolate).GetKey(modify_index);
  if (name.IsString()) {
    String::cast(name).PrintOn(file);
  } else {
    os << "{symbol " << reinterpret_cast<void*>(name.ptr()) << "}";
  }
  os << ":";
  if (descriptor_to_field) {
    os << "c";
  } else {
    os << old_representation.Mnemonic() << "{";
    if (old_field_type.is_null()) {
      os << Brief(*(old_value.ToHandleChecked()));
    } else {
      old_field_type.ToHandleChecked()->PrintTo(os);
    }
    os << ";" << old_constness << "}";
  }
  os << "->" << new_representation.Mnemonic() << "{";
  if (new_field_type.is_null()) {
    os << Brief(*(new_value.ToHandleChecked()));
  } else {
    new_field_type.ToHandleChecked()->PrintTo(os);
  }
  os << ";" << new_constness << "} (";
  if (strlen(reason) > 0) {
    os << reason;
  } else {
    os << "+" << (descriptors - split) << " maps";
  }
  os << ") [";
  JavaScriptFrame::PrintTop(isolate, file, false, true);
  os << "]\n";
}

// static
MaybeObjectHandle Map::WrapFieldType(Isolate* isolate, Handle<FieldType> type) {
  if (type->IsClass()) {
    return MaybeObjectHandle::Weak(type->AsClass(), isolate);
  }
  return MaybeObjectHandle(type);
}

// static
FieldType Map::UnwrapFieldType(MaybeObject wrapped_type) {
  if (wrapped_type->IsCleared()) {
    return FieldType::None();
  }
  HeapObject heap_object;
  if (wrapped_type->GetHeapObjectIfWeak(&heap_object)) {
    return FieldType::cast(heap_object);
  }
  return wrapped_type->cast<FieldType>();
}

MaybeHandle<Map> Map::CopyWithField(Isolate* isolate, Handle<Map> map,
                                    Handle<Name> name, Handle<FieldType> type,
                                    PropertyAttributes attributes,
                                    PropertyConstness constness,
                                    Representation representation,
                                    TransitionFlag flag) {
  DCHECK(map->instance_descriptors(isolate)
             .Search(*name, map->NumberOfOwnDescriptors())
             .is_not_found());

  // Ensure the descriptor array does not get too big.
  if (map->NumberOfOwnDescriptors() >= kMaxNumberOfDescriptors) {
    return MaybeHandle<Map>();
  }

  // Compute the new index for new field.
  int index = map->NextFreePropertyIndex();

  if (map->instance_type() == JS_CONTEXT_EXTENSION_OBJECT_TYPE) {
    constness = PropertyConstness::kMutable;
    representation = Representation::Tagged();
    type = FieldType::Any(isolate);
  } else {
    Map::GeneralizeIfCanHaveTransitionableFastElementsKind(
        isolate, map->instance_type(), &representation, &type);
  }

  MaybeObjectHandle wrapped_type = WrapFieldType(isolate, type);

  Descriptor d = Descriptor::DataField(name, index, attributes, constness,
                                       representation, wrapped_type);
  Handle<Map> new_map = Map::CopyAddDescriptor(isolate, map, &d, flag);
  new_map->AccountAddedPropertyField();
  return new_map;
}

MaybeHandle<Map> Map::CopyWithConstant(Isolate* isolate, Handle<Map> map,
                                       Handle<Name> name,
                                       Handle<Object> constant,
                                       PropertyAttributes attributes,
                                       TransitionFlag flag) {
  // Ensure the descriptor array does not get too big.
  if (map->NumberOfOwnDescriptors() >= kMaxNumberOfDescriptors) {
    return MaybeHandle<Map>();
  }

  Representation representation = constant->OptimalRepresentation(isolate);
  Handle<FieldType> type = constant->OptimalType(isolate, representation);
  return CopyWithField(isolate, map, name, type, attributes,
                       PropertyConstness::kConst, representation, flag);
}

bool Map::InstancesNeedRewriting(Map target) const {
  int target_number_of_fields = target.NumberOfFields();
  int target_inobject = target.GetInObjectProperties();
  int target_unused = target.UnusedPropertyFields();
  int old_number_of_fields;

  return InstancesNeedRewriting(target, target_number_of_fields,
                                target_inobject, target_unused,
                                &old_number_of_fields);
}

bool Map::InstancesNeedRewriting(Map target, int target_number_of_fields,
                                 int target_inobject, int target_unused,
                                 int* old_number_of_fields) const {
  // If fields were added (or removed), rewrite the instance.
  *old_number_of_fields = NumberOfFields();
  DCHECK(target_number_of_fields >= *old_number_of_fields);
  if (target_number_of_fields != *old_number_of_fields) return true;

  // If smi descriptors were replaced by double descriptors, rewrite.
  DescriptorArray old_desc = instance_descriptors();
  DescriptorArray new_desc = target.instance_descriptors();
  for (InternalIndex i : IterateOwnDescriptors()) {
    if (new_desc.GetDetails(i).representation().IsDouble() !=
        old_desc.GetDetails(i).representation().IsDouble()) {
      return true;
    }
  }

  // If no fields were added, and no inobject properties were removed, setting
  // the map is sufficient.
  if (target_inobject == GetInObjectProperties()) return false;
  // In-object slack tracking may have reduced the object size of the new map.
  // In that case, succeed if all existing fields were inobject, and they still
  // fit within the new inobject size.
  DCHECK(target_inobject < GetInObjectProperties());
  if (target_number_of_fields <= target_inobject) {
    DCHECK(target_number_of_fields + target_unused == target_inobject);
    return false;
  }
  // Otherwise, properties will need to be moved to the backing store.
  return true;
}

int Map::NumberOfFields() const {
  DescriptorArray descriptors = instance_descriptors();
  int result = 0;
  for (InternalIndex i : IterateOwnDescriptors()) {
    if (descriptors.GetDetails(i).location() == kField) result++;
  }
  return result;
}

Map::FieldCounts Map::GetFieldCounts() const {
  DescriptorArray descriptors = instance_descriptors();
  int mutable_count = 0;
  int const_count = 0;
  for (InternalIndex i : IterateOwnDescriptors()) {
    PropertyDetails details = descriptors.GetDetails(i);
    if (details.location() == kField) {
      switch (details.constness()) {
        case PropertyConstness::kMutable:
          mutable_count++;
          break;
        case PropertyConstness::kConst:
          const_count++;
          break;
      }
    }
  }
  return FieldCounts(mutable_count, const_count);
}

bool Map::HasOutOfObjectProperties() const {
  return GetInObjectProperties() < NumberOfFields();
}

void Map::DeprecateTransitionTree(Isolate* isolate) {
  if (is_deprecated()) return;
  DisallowGarbageCollection no_gc;
  TransitionsAccessor transitions(isolate, *this, &no_gc);
  int num_transitions = transitions.NumberOfTransitions();
  for (int i = 0; i < num_transitions; ++i) {
    transitions.GetTarget(i).DeprecateTransitionTree(isolate);
  }
  DCHECK(!constructor_or_back_pointer().IsFunctionTemplateInfo());
  DCHECK(CanBeDeprecated());
  set_is_deprecated(true);
  if (FLAG_log_maps) {
    LOG(isolate, MapEvent("Deprecate", handle(*this, isolate), Handle<Map>()));
  }
  dependent_code().DeoptimizeDependentCodeGroup(
      DependentCode::kTransitionGroup);
  NotifyLeafMapLayoutChange(isolate);
}

// Installs |new_descriptors| over the current instance_descriptors to ensure
// proper sharing of descriptor arrays.
void Map::ReplaceDescriptors(Isolate* isolate,
                             DescriptorArray new_descriptors) {
  // Don't overwrite the empty descriptor array or initial map's descriptors.
  if (NumberOfOwnDescriptors() == 0 ||
      GetBackPointer(isolate).IsUndefined(isolate)) {
    return;
  }

  DescriptorArray to_replace = instance_descriptors(isolate);
  // Replace descriptors by new_descriptors in all maps that share it. The old
  // descriptors will not be trimmed in the mark-compactor, we need to mark
  // all its elements.
  Map current = *this;
#ifndef V8_DISABLE_WRITE_BARRIERS
  WriteBarrier::Marking(to_replace, to_replace.number_of_descriptors());
#endif
  while (current.instance_descriptors(isolate) == to_replace) {
    Object next = current.GetBackPointer(isolate);
    if (next.IsUndefined(isolate)) break;  // Stop overwriting at initial map.
    current.SetEnumLength(kInvalidEnumCacheSentinel);
    current.UpdateDescriptors(isolate, new_descriptors,
                              current.NumberOfOwnDescriptors());
    current = Map::cast(next);
  }
  set_owns_descriptors(false);
}

Map Map::FindRootMap(Isolate* isolate) const {
  DisallowGarbageCollection no_gc;
  Map result = *this;
  while (true) {
    Object back = result.GetBackPointer(isolate);
    if (back.IsUndefined(isolate)) {
      // Initial map must not contain descriptors in the descriptors array
      // that do not belong to the map.
      DCHECK_LE(result.NumberOfOwnDescriptors(),
                result.instance_descriptors(isolate, kRelaxedLoad)
                    .number_of_descriptors());
      return result;
    }
    result = Map::cast(back);
  }
}

Map Map::FindFieldOwner(Isolate* isolate, InternalIndex descriptor) const {
  DisallowGarbageCollection no_gc;
  DCHECK_EQ(kField, instance_descriptors(isolate, kRelaxedLoad)
                        .GetDetails(descriptor)
                        .location());
  Map result = *this;
  while (true) {
    Object back = result.GetBackPointer(isolate);
    if (back.IsUndefined(isolate)) break;
    const Map parent = Map::cast(back);
    if (parent.NumberOfOwnDescriptors() <= descriptor.as_int()) break;
    result = parent;
  }
  return result;
}

void Map::UpdateFieldType(Isolate* isolate, InternalIndex descriptor,
                          Handle<Name> name, PropertyConstness new_constness,
                          Representation new_representation,
                          const MaybeObjectHandle& new_wrapped_type) {
  DCHECK(new_wrapped_type->IsSmi() || new_wrapped_type->IsWeak());
  // We store raw pointers in the queue, so no allocations are allowed.
  DisallowGarbageCollection no_gc;
  PropertyDetails details =
      instance_descriptors(isolate).GetDetails(descriptor);
  if (details.location() != kField) return;
  DCHECK_EQ(kData, details.kind());

  if (new_constness != details.constness() && is_prototype_map()) {
    JSObject::InvalidatePrototypeChains(*this);
  }

  Zone zone(isolate->allocator(), ZONE_NAME);
  ZoneQueue<Map> backlog(&zone);
  backlog.push(*this);

  while (!backlog.empty()) {
    Map current = backlog.front();
    backlog.pop();

    TransitionsAccessor transitions(isolate, current, &no_gc);
    int num_transitions = transitions.NumberOfTransitions();
    for (int i = 0; i < num_transitions; ++i) {
      Map target = transitions.GetTarget(i);
      backlog.push(target);
    }
    DescriptorArray descriptors = current.instance_descriptors(isolate);
    PropertyDetails details = descriptors.GetDetails(descriptor);

    // It is allowed to change representation here only from None
    // to something or from Smi or HeapObject to Tagged.
    DCHECK(details.representation().Equals(new_representation) ||
           details.representation().CanBeInPlaceChangedTo(new_representation));

    // Skip if already updated the shared descriptor.
    if (new_constness != details.constness() ||
        !new_representation.Equals(details.representation()) ||
        descriptors.GetFieldType(descriptor) != *new_wrapped_type.object()) {
      Descriptor d = Descriptor::DataField(
          name, descriptors.GetFieldIndex(descriptor), details.attributes(),
          new_constness, new_representation, new_wrapped_type);
      descriptors.Replace(descriptor, &d);
    }
  }
}

bool FieldTypeIsCleared(Representation rep, FieldType type) {
  return type.IsNone() && rep.IsHeapObject();
}

// static
Handle<FieldType> Map::GeneralizeFieldType(Representation rep1,
                                           Handle<FieldType> type1,
                                           Representation rep2,
                                           Handle<FieldType> type2,
                                           Isolate* isolate) {
  // Cleared field types need special treatment. They represent lost knowledge,
  // so we must be conservative, so their generalization with any other type
  // is "Any".
  if (FieldTypeIsCleared(rep1, *type1) || FieldTypeIsCleared(rep2, *type2)) {
    return FieldType::Any(isolate);
  }
  if (type1->NowIs(type2)) return type2;
  if (type2->NowIs(type1)) return type1;
  return FieldType::Any(isolate);
}

// static
void Map::GeneralizeField(Isolate* isolate, Handle<Map> map,
                          InternalIndex modify_index,
                          PropertyConstness new_constness,
                          Representation new_representation,
                          Handle<FieldType> new_field_type) {
  // Check if we actually need to generalize the field type at all.
  Handle<DescriptorArray> old_descriptors(map->instance_descriptors(isolate),
                                          isolate);
  PropertyDetails old_details = old_descriptors->GetDetails(modify_index);
  PropertyConstness old_constness = old_details.constness();
  Representation old_representation = old_details.representation();
  Handle<FieldType> old_field_type(old_descriptors->GetFieldType(modify_index),
                                   isolate);

  // Return if the current map is general enough to hold requested constness and
  // representation/field type.
  if (IsGeneralizableTo(new_constness, old_constness) &&
      old_representation.Equals(new_representation) &&
      !FieldTypeIsCleared(new_representation, *new_field_type) &&
      // Checking old_field_type for being cleared is not necessary because
      // the NowIs check below would fail anyway in that case.
      new_field_type->NowIs(old_field_type)) {
    DCHECK(GeneralizeFieldType(old_representation, old_field_type,
                               new_representation, new_field_type, isolate)
               ->NowIs(old_field_type));
    return;
  }

  // Determine the field owner.
  Handle<Map> field_owner(map->FindFieldOwner(isolate, modify_index), isolate);
  Handle<DescriptorArray> descriptors(
      field_owner->instance_descriptors(isolate), isolate);
  DCHECK_EQ(*old_field_type, descriptors->GetFieldType(modify_index));

  new_field_type =
      Map::GeneralizeFieldType(old_representation, old_field_type,
                               new_representation, new_field_type, isolate);

  new_constness = GeneralizeConstness(old_constness, new_constness);

  PropertyDetails details = descriptors->GetDetails(modify_index);
  Handle<Name> name(descriptors->GetKey(modify_index), isolate);

  MaybeObjectHandle wrapped_type(WrapFieldType(isolate, new_field_type));
  field_owner->UpdateFieldType(isolate, modify_index, name, new_constness,
                               new_representation, wrapped_type);

  if (new_constness != old_constness) {
    field_owner->dependent_code().DeoptimizeDependentCodeGroup(
        DependentCode::kFieldConstGroup);
  }

  if (!new_field_type->Equals(*old_field_type)) {
    field_owner->dependent_code().DeoptimizeDependentCodeGroup(
        DependentCode::kFieldTypeGroup);
  }

  if (!new_representation.Equals(old_representation)) {
    field_owner->dependent_code().DeoptimizeDependentCodeGroup(
        DependentCode::kFieldRepresentationGroup);
  }

  if (FLAG_trace_generalization) {
    map->PrintGeneralization(
        isolate, stdout, "field type generalization", modify_index,
        map->NumberOfOwnDescriptors(), map->NumberOfOwnDescriptors(), false,
        details.representation(),
        descriptors->GetDetails(modify_index).representation(), old_constness,
        new_constness, old_field_type, MaybeHandle<Object>(), new_field_type,
        MaybeHandle<Object>());
  }
}

namespace {

Map SearchMigrationTarget(Isolate* isolate, Map old_map) {
  DisallowGarbageCollection no_gc;
  DisallowDeoptimization no_deoptimization(isolate);

  Map target = old_map;
  do {
    target = TransitionsAccessor(isolate, target, &no_gc).GetMigrationTarget();
  } while (!target.is_null() && target.is_deprecated());
  if (target.is_null()) return Map();

  // TODO(ishell): if this validation ever become a bottleneck consider adding a
  // bit to the Map telling whether it contains fields whose field types may be
  // cleared.
  // TODO(ishell): revisit handling of cleared field types in
  // TryReplayPropertyTransitions() and consider checking the target map's field
  // types instead of old_map's types.
  // Go to slow map updating if the old_map has fast properties with cleared
  // field types.
  DescriptorArray old_descriptors = old_map.instance_descriptors(isolate);
  for (InternalIndex i : old_map.IterateOwnDescriptors()) {
    PropertyDetails old_details = old_descriptors.GetDetails(i);
    if (old_details.location() == kField && old_details.kind() == kData) {
      FieldType old_type = old_descriptors.GetFieldType(i);
      if (FieldTypeIsCleared(old_details.representation(), old_type)) {
        return Map();
      }
    }
  }

  SLOW_DCHECK(Map::TryUpdateSlow(isolate, old_map) == target);
  return target;
}
}  // namespace

// TODO(ishell): Move TryUpdate() and friends to MapUpdater
// static
MaybeHandle<Map> Map::TryUpdate(Isolate* isolate, Handle<Map> old_map) {
  DisallowGarbageCollection no_gc;
  DisallowDeoptimization no_deoptimization(isolate);

  if (!old_map->is_deprecated()) return old_map;

  if (FLAG_fast_map_update) {
    Map target_map = SearchMigrationTarget(isolate, *old_map);
    if (!target_map.is_null()) {
      return handle(target_map, isolate);
    }
  }

  Map new_map = TryUpdateSlow(isolate, *old_map);
  if (new_map.is_null()) return MaybeHandle<Map>();
  if (FLAG_fast_map_update) {
    TransitionsAccessor(isolate, *old_map, &no_gc).SetMigrationTarget(new_map);
  }
  return handle(new_map, isolate);
}

namespace {

struct IntegrityLevelTransitionInfo {
  explicit IntegrityLevelTransitionInfo(Map map)
      : integrity_level_source_map(map) {}

  bool has_integrity_level_transition = false;
  PropertyAttributes integrity_level = NONE;
  Map integrity_level_source_map;
  Symbol integrity_level_symbol;
};

IntegrityLevelTransitionInfo DetectIntegrityLevelTransitions(
    Map map, Isolate* isolate, DisallowGarbageCollection* no_gc) {
  IntegrityLevelTransitionInfo info(map);

  // Figure out the most restrictive integrity level transition (it should
  // be the last one in the transition tree).
  DCHECK(!map.is_extensible());
  Map previous = Map::cast(map.GetBackPointer(isolate));
  TransitionsAccessor last_transitions(isolate, previous, no_gc);
  if (!last_transitions.HasIntegrityLevelTransitionTo(
          map, &(info.integrity_level_symbol), &(info.integrity_level))) {
    // The last transition was not integrity level transition - just bail out.
    // This can happen in the following cases:
    // - there are private symbol transitions following the integrity level
    //   transitions (see crbug.com/v8/8854).
    // - there is a getter added in addition to an existing setter (or a setter
    //   in addition to an existing getter).
    return info;
  }

  Map source_map = previous;
  // Now walk up the back pointer chain and skip all integrity level
  // transitions. If we encounter any non-integrity level transition interleaved
  // with integrity level transitions, just bail out.
  while (!source_map.is_extensible()) {
    previous = Map::cast(source_map.GetBackPointer(isolate));
    TransitionsAccessor transitions(isolate, previous, no_gc);
    if (!transitions.HasIntegrityLevelTransitionTo(source_map)) {
      return info;
    }
    source_map = previous;
  }

  // Integrity-level transitions never change number of descriptors.
  CHECK_EQ(map.NumberOfOwnDescriptors(), source_map.NumberOfOwnDescriptors());

  info.has_integrity_level_transition = true;
  info.integrity_level_source_map = source_map;
  return info;
}

}  // namespace

Map Map::TryUpdateSlow(Isolate* isolate, Map old_map) {
  DisallowGarbageCollection no_gc;
  DisallowDeoptimization no_deoptimization(isolate);

  // Check the state of the root map.
  Map root_map = old_map.FindRootMap(isolate);
  if (root_map.is_deprecated()) {
    JSFunction constructor = JSFunction::cast(root_map.GetConstructor());
    DCHECK(constructor.has_initial_map());
    DCHECK(constructor.initial_map().is_dictionary_map());
    if (constructor.initial_map().elements_kind() != old_map.elements_kind()) {
      return Map();
    }
    return constructor.initial_map();
  }
  if (!old_map.EquivalentToForTransition(root_map)) return Map();

  ElementsKind from_kind = root_map.elements_kind();
  ElementsKind to_kind = old_map.elements_kind();

  IntegrityLevelTransitionInfo info(old_map);
  if (root_map.is_extensible() != old_map.is_extensible()) {
    DCHECK(!old_map.is_extensible());
    DCHECK(root_map.is_extensible());
    info = DetectIntegrityLevelTransitions(old_map, isolate, &no_gc);
    // Bail out if there were some private symbol transitions mixed up
    // with the integrity level transitions.
    if (!info.has_integrity_level_transition) return Map();
    // Make sure to replay the original elements kind transitions, before
    // the integrity level transition sets the elements to dictionary mode.
    DCHECK(to_kind == DICTIONARY_ELEMENTS ||
           to_kind == SLOW_STRING_WRAPPER_ELEMENTS ||
           IsTypedArrayElementsKind(to_kind) ||
           IsAnyHoleyNonextensibleElementsKind(to_kind));
    to_kind = info.integrity_level_source_map.elements_kind();
  }
  if (from_kind != to_kind) {
    // Try to follow existing elements kind transitions.
    root_map = root_map.LookupElementsTransitionMap(isolate, to_kind);
    if (root_map.is_null()) return Map();
    // From here on, use the map with correct elements kind as root map.
  }

  // Replay the transitions as they were before the integrity level transition.
  Map result = root_map.TryReplayPropertyTransitions(
      isolate, info.integrity_level_source_map);
  if (result.is_null()) return Map();

  if (info.has_integrity_level_transition) {
    // Now replay the integrity level transition.
    result = TransitionsAccessor(isolate, result, &no_gc)
                 .SearchSpecial(info.integrity_level_symbol);
  }

  DCHECK_IMPLIES(!result.is_null(),
                 old_map.elements_kind() == result.elements_kind());
  DCHECK_IMPLIES(!result.is_null(),
                 old_map.instance_type() == result.instance_type());
  return result;
}

Map Map::TryReplayPropertyTransitions(Isolate* isolate, Map old_map) {
  DisallowGarbageCollection no_gc;
  DisallowDeoptimization no_deoptimization(isolate);

  int root_nof = NumberOfOwnDescriptors();

  int old_nof = old_map.NumberOfOwnDescriptors();
  DescriptorArray old_descriptors = old_map.instance_descriptors(isolate);

  Map new_map = *this;
  for (InternalIndex i : InternalIndex::Range(root_nof, old_nof)) {
    PropertyDetails old_details = old_descriptors.GetDetails(i);
    Map transition =
        TransitionsAccessor(isolate, new_map, &no_gc)
            .SearchTransition(old_descriptors.GetKey(i), old_details.kind(),
                              old_details.attributes());
    if (transition.is_null()) return Map();
    new_map = transition;
    DescriptorArray new_descriptors = new_map.instance_descriptors(isolate);

    PropertyDetails new_details = new_descriptors.GetDetails(i);
    DCHECK_EQ(old_details.kind(), new_details.kind());
    DCHECK_EQ(old_details.attributes(), new_details.attributes());
    if (!IsGeneralizableTo(old_details.constness(), new_details.constness())) {
      return Map();
    }
    DCHECK(IsGeneralizableTo(old_details.location(), new_details.location()));
    if (!old_details.representation().fits_into(new_details.representation())) {
      return Map();
    }
    if (new_details.location() == kField) {
      if (new_details.kind() == kData) {
        FieldType new_type = new_descriptors.GetFieldType(i);
        // Cleared field types need special treatment. They represent lost
        // knowledge, so we must first generalize the new_type to "Any".
        if (FieldTypeIsCleared(new_details.representation(), new_type)) {
          return Map();
        }
        DCHECK_EQ(kData, old_details.kind());
        DCHECK_EQ(kField, old_details.location());
        FieldType old_type = old_descriptors.GetFieldType(i);
        if (FieldTypeIsCleared(old_details.representation(), old_type) ||
            !old_type.NowIs(new_type)) {
          return Map();
        }
      } else {
        DCHECK_EQ(kAccessor, new_details.kind());
#ifdef DEBUG
        FieldType new_type = new_descriptors.GetFieldType(i);
        DCHECK(new_type.IsAny());
#endif
        UNREACHABLE();
      }
    } else {
      DCHECK_EQ(kDescriptor, new_details.location());
      if (old_details.location() == kField ||
          old_descriptors.GetStrongValue(i) !=
              new_descriptors.GetStrongValue(i)) {
        return Map();
      }
    }
  }
  if (new_map.NumberOfOwnDescriptors() != old_nof) return Map();
  return new_map;
}

// static
Handle<Map> Map::Update(Isolate* isolate, Handle<Map> map) {
  if (!map->is_deprecated()) return map;
  if (FLAG_fast_map_update) {
    Map target_map = SearchMigrationTarget(isolate, *map);
    if (!target_map.is_null()) {
      return handle(target_map, isolate);
    }
  }
  MapUpdater mu(isolate, map);
  return mu.Update();
}

void Map::EnsureDescriptorSlack(Isolate* isolate, Handle<Map> map, int slack) {
  // Only supports adding slack to owned descriptors.
  DCHECK(map->owns_descriptors());

  Handle<DescriptorArray> descriptors(map->instance_descriptors(isolate),
                                      isolate);
  int old_size = map->NumberOfOwnDescriptors();
  if (slack <= descriptors->number_of_slack_descriptors()) return;

  Handle<DescriptorArray> new_descriptors =
      DescriptorArray::CopyUpTo(isolate, descriptors, old_size, slack);

  DisallowGarbageCollection no_gc;
  if (old_size == 0) {
    map->UpdateDescriptors(isolate, *new_descriptors,
                           map->NumberOfOwnDescriptors());
    return;
  }

  // If the source descriptors had an enum cache we copy it. This ensures
  // that the maps to which we push the new descriptor array back can rely
  // on a cache always being available once it is set. If the map has more
  // enumerated descriptors than available in the original cache, the cache
  // will be lazily replaced by the extended cache when needed.
  new_descriptors->CopyEnumCacheFrom(*descriptors);

  // Replace descriptors by new_descriptors in all maps that share it. The old
  // descriptors will not be trimmed in the mark-compactor, we need to mark
  // all its elements.
#ifndef V8_DISABLE_WRITE_BARRIERS
  WriteBarrier::Marking(*descriptors, descriptors->number_of_descriptors());
#endif

  // Update the descriptors from {map} (inclusive) until the initial map
  // (exclusive). In the case that {map} is the initial map, update it.
  map->UpdateDescriptors(isolate, *new_descriptors,
                         map->NumberOfOwnDescriptors());
  Object next = map->GetBackPointer();
  if (next.IsUndefined(isolate)) return;

  Map current = Map::cast(next);
  while (current.instance_descriptors(isolate) == *descriptors) {
    next = current.GetBackPointer();
    if (next.IsUndefined(isolate)) break;
    current.UpdateDescriptors(isolate, *new_descriptors,
                              current.NumberOfOwnDescriptors());
    current = Map::cast(next);
  }
}

// static
Handle<Map> Map::GetObjectCreateMap(Isolate* isolate,
                                    Handle<HeapObject> prototype) {
  Handle<Map> map(isolate->native_context()->object_function().initial_map(),
                  isolate);
  if (map->prototype() == *prototype) return map;
  if (prototype->IsNull(isolate)) {
    return isolate->slow_object_with_null_prototype_map();
  }
  if (prototype->IsJSObject()) {
    Handle<JSObject> js_prototype = Handle<JSObject>::cast(prototype);
    if (!js_prototype->map().is_prototype_map()) {
      JSObject::OptimizeAsPrototype(js_prototype);
    }
    Handle<PrototypeInfo> info =
        Map::GetOrCreatePrototypeInfo(js_prototype, isolate);
    // TODO(verwaest): Use inobject slack tracking for this map.
    if (info->HasObjectCreateMap()) {
      map = handle(info->ObjectCreateMap(), isolate);
    } else {
      map = Map::CopyInitialMap(isolate, map);
      Map::SetPrototype(isolate, map, prototype);
      PrototypeInfo::SetObjectCreateMap(info, map);
    }
    return map;
  }

  return Map::TransitionToPrototype(isolate, map, prototype);
}

// static
MaybeHandle<Map> Map::TryGetObjectCreateMap(Isolate* isolate,
                                            Handle<HeapObject> prototype) {
  Handle<Map> map(isolate->native_context()->object_function().initial_map(),
                  isolate);
  if (map->prototype() == *prototype) return map;
  if (prototype->IsNull(isolate)) {
    return isolate->slow_object_with_null_prototype_map();
  }
  if (!prototype->IsJSObject()) return MaybeHandle<Map>();
  Handle<JSObject> js_prototype = Handle<JSObject>::cast(prototype);
  if (!js_prototype->map().is_prototype_map()) return MaybeHandle<Map>();
  Handle<PrototypeInfo> info =
      Map::GetOrCreatePrototypeInfo(js_prototype, isolate);
  if (!info->HasObjectCreateMap()) return MaybeHandle<Map>();
  return handle(info->ObjectCreateMap(), isolate);
}

static bool ContainsMap(MapHandles const& maps, Map map) {
  DCHECK(!map.is_null());
  for (Handle<Map> current : maps) {
    if (!current.is_null() && *current == map) return true;
  }
  return false;
}

static bool HasElementsKind(MapHandles const& maps,
                            ElementsKind elements_kind) {
  for (Handle<Map> current : maps) {
    if (!current.is_null() && current->elements_kind() == elements_kind)
      return true;
  }
  return false;
}

Map Map::FindElementsKindTransitionedMap(Isolate* isolate,
                                         MapHandles const& candidates) {
  DisallowGarbageCollection no_gc;
  DisallowDeoptimization no_deoptimization(isolate);

  if (IsDetached(isolate)) return Map();

  ElementsKind kind = elements_kind();
  bool packed = IsFastPackedElementsKind(kind);

  Map transition;
  if (IsTransitionableFastElementsKind(kind)) {
    // Check the state of the root map.
    Map root_map = FindRootMap(isolate);
    if (!EquivalentToForElementsKindTransition(root_map)) return Map();
    root_map = root_map.LookupElementsTransitionMap(isolate, kind);
    DCHECK(!root_map.is_null());
    // Starting from the next existing elements kind transition try to
    // replay the property transitions that does not involve instance rewriting
    // (ElementsTransitionAndStoreStub does not support that).
    for (root_map = root_map.ElementsTransitionMap(isolate);
         !root_map.is_null() && root_map.has_fast_elements();
         root_map = root_map.ElementsTransitionMap(isolate)) {
      // If root_map's elements kind doesn't match any of the elements kind in
      // the candidates there is no need to do any additional work.
      if (!HasElementsKind(candidates, root_map.elements_kind())) continue;
      Map current = root_map.TryReplayPropertyTransitions(isolate, *this);
      if (current.is_null()) continue;
      if (InstancesNeedRewriting(current)) continue;

      if (ContainsMap(candidates, current) &&
          (packed || !IsFastPackedElementsKind(current.elements_kind()))) {
        transition = current;
        packed = packed && IsFastPackedElementsKind(current.elements_kind());
      }
    }
  }
  return transition;
}

static Map FindClosestElementsTransition(Isolate* isolate, Map map,
                                         ElementsKind to_kind) {
  // Ensure we are requested to search elements kind transition "near the root".
  DCHECK_EQ(map.FindRootMap(isolate).NumberOfOwnDescriptors(),
            map.NumberOfOwnDescriptors());
  Map current_map = map;

  ElementsKind kind = map.elements_kind();
  while (kind != to_kind) {
    Map next_map = current_map.ElementsTransitionMap(isolate);
    if (next_map.is_null()) return current_map;
    kind = next_map.elements_kind();
    current_map = next_map;
  }

  DCHECK_EQ(to_kind, current_map.elements_kind());
  return current_map;
}

Map Map::LookupElementsTransitionMap(Isolate* isolate, ElementsKind to_kind) {
  Map to_map = FindClosestElementsTransition(isolate, *this, to_kind);
  if (to_map.elements_kind() == to_kind) return to_map;
  return Map();
}

bool Map::IsMapInArrayPrototypeChain(Isolate* isolate) const {
  if (isolate->initial_array_prototype()->map() == *this) {
    return true;
  }

  if (isolate->initial_object_prototype()->map() == *this) {
    return true;
  }

  return false;
}

Handle<Map> Map::TransitionElementsTo(Isolate* isolate, Handle<Map> map,
                                      ElementsKind to_kind) {
  ElementsKind from_kind = map->elements_kind();
  if (from_kind == to_kind) return map;

  Context native_context = isolate->context().native_context();
  if (from_kind == FAST_SLOPPY_ARGUMENTS_ELEMENTS) {
    if (*map == native_context.fast_aliased_arguments_map()) {
      DCHECK_EQ(SLOW_SLOPPY_ARGUMENTS_ELEMENTS, to_kind);
      return handle(native_context.slow_aliased_arguments_map(), isolate);
    }
  } else if (from_kind == SLOW_SLOPPY_ARGUMENTS_ELEMENTS) {
    if (*map == native_context.slow_aliased_arguments_map()) {
      DCHECK_EQ(FAST_SLOPPY_ARGUMENTS_ELEMENTS, to_kind);
      return handle(native_context.fast_aliased_arguments_map(), isolate);
    }
  } else if (IsFastElementsKind(from_kind) && IsFastElementsKind(to_kind)) {
    // Reuse map transitions for JSArrays.
    DisallowGarbageCollection no_gc;
    if (native_context.GetInitialJSArrayMap(from_kind) == *map) {
      Object maybe_transitioned_map =
          native_context.get(Context::ArrayMapIndex(to_kind));
      if (maybe_transitioned_map.IsMap()) {
        return handle(Map::cast(maybe_transitioned_map), isolate);
      }
    }
  }

  DCHECK(!map->IsUndefined(isolate));
  // Check if we can go back in the elements kind transition chain.
  if (IsHoleyElementsKind(from_kind) &&
      to_kind == GetPackedElementsKind(from_kind) &&
      map->GetBackPointer().IsMap() &&
      Map::cast(map->GetBackPointer()).elements_kind() == to_kind) {
    return handle(Map::cast(map->GetBackPointer()), isolate);
  }

  bool allow_store_transition = IsTransitionElementsKind(from_kind);
  // Only store fast element maps in ascending generality.
  if (IsFastElementsKind(to_kind)) {
    allow_store_transition =
        allow_store_transition && IsTransitionableFastElementsKind(from_kind) &&
        IsMoreGeneralElementsKindTransition(from_kind, to_kind);
  }

  if (!allow_store_transition) {
    return Map::CopyAsElementsKind(isolate, map, to_kind, OMIT_TRANSITION);
  }

  return MapUpdater{isolate, map}.ReconfigureElementsKind(to_kind);
}

static Handle<Map> AddMissingElementsTransitions(Isolate* isolate,
                                                 Handle<Map> map,
                                                 ElementsKind to_kind) {
  DCHECK(IsTransitionElementsKind(map->elements_kind()));

  Handle<Map> current_map = map;

  ElementsKind kind = map->elements_kind();
  TransitionFlag flag;
  if (map->IsDetached(isolate)) {
    flag = OMIT_TRANSITION;
  } else {
    flag = INSERT_TRANSITION;
    if (IsFastElementsKind(kind)) {
      while (kind != to_kind && !IsTerminalElementsKind(kind)) {
        kind = GetNextTransitionElementsKind(kind);
        current_map = Map::CopyAsElementsKind(isolate, current_map, kind, flag);
      }
    }
  }

  // In case we are exiting the fast elements kind system, just add the map in
  // the end.
  if (kind != to_kind) {
    current_map = Map::CopyAsElementsKind(isolate, current_map, to_kind, flag);
  }

  DCHECK(current_map->elements_kind() == to_kind);
  return current_map;
}

// static
Handle<Map> Map::AsElementsKind(Isolate* isolate, Handle<Map> map,
                                ElementsKind kind) {
  Handle<Map> closest_map(FindClosestElementsTransition(isolate, *map, kind),
                          isolate);

  if (closest_map->elements_kind() == kind) {
    return closest_map;
  }

  return AddMissingElementsTransitions(isolate, closest_map, kind);
}

int Map::NumberOfEnumerableProperties() const {
  int result = 0;
  DescriptorArray descs = instance_descriptors(kRelaxedLoad);
  for (InternalIndex i : IterateOwnDescriptors()) {
    if ((descs.GetDetails(i).attributes() & ONLY_ENUMERABLE) == 0 &&
        !descs.GetKey(i).FilterKey(ENUMERABLE_STRINGS)) {
      result++;
    }
  }
  return result;
}

int Map::NextFreePropertyIndex() const {
  int number_of_own_descriptors = NumberOfOwnDescriptors();
  DescriptorArray descs = instance_descriptors();
  // Search properties backwards to find the last field.
  for (int i = number_of_own_descriptors - 1; i >= 0; --i) {
    PropertyDetails details = descs.GetDetails(InternalIndex(i));
    if (details.location() == kField) {
      return details.field_index() + details.field_width_in_words();
    }
  }
  return 0;
}

bool Map::OnlyHasSimpleProperties() const {
  // Wrapped string elements aren't explicitly stored in the elements backing
  // store, but are loaded indirectly from the underlying string.
  return !IsStringWrapperElementsKind(elements_kind()) &&
         !IsSpecialReceiverMap() && !is_dictionary_map();
}

bool Map::MayHaveReadOnlyElementsInPrototypeChain(Isolate* isolate) {
  for (PrototypeIterator iter(isolate, *this); !iter.IsAtEnd();
       iter.Advance()) {
    // Be conservative, don't look into any JSReceivers that may have custom
    // elements. For example, into JSProxies, String wrappers (which have have
    // non-configurable, non-writable elements), API objects, etc.
    if (iter.GetCurrent().map().IsCustomElementsReceiverMap()) return true;

    JSObject current = iter.GetCurrent<JSObject>();
    ElementsKind elements_kind = current.GetElementsKind(isolate);
    if (IsFrozenElementsKind(elements_kind)) return true;

    if (IsDictionaryElementsKind(elements_kind) &&
        current.element_dictionary(isolate).requires_slow_elements()) {
      return true;
    }

    if (IsSlowArgumentsElementsKind(elements_kind)) {
      SloppyArgumentsElements elements =
          SloppyArgumentsElements::cast(current.elements(isolate));
      Object arguments = elements.arguments();
      if (NumberDictionary::cast(arguments).requires_slow_elements()) {
        return true;
      }
    }
  }

  return false;
}

Handle<Map> Map::RawCopy(Isolate* isolate, Handle<Map> map, int instance_size,
                         int inobject_properties) {
  Handle<Map> result = isolate->factory()->NewMap(
      map->instance_type(), instance_size, TERMINAL_FAST_ELEMENTS_KIND,
      inobject_properties);
  Handle<HeapObject> prototype(map->prototype(), isolate);
  Map::SetPrototype(isolate, result, prototype);
  result->set_constructor_or_back_pointer(map->GetConstructor());
  // TODO(solanes, v8:7790, v8:11353): set_relaxed_bit_field could be an atomic
  // set if TSAN could see the transitions happening in StoreIC.
  result->set_relaxed_bit_field(map->bit_field());
  result->set_bit_field2(map->bit_field2());
  int new_bit_field3 = map->bit_field3();
  new_bit_field3 = Bits3::OwnsDescriptorsBit::update(new_bit_field3, true);
  new_bit_field3 = Bits3::NumberOfOwnDescriptorsBits::update(new_bit_field3, 0);
  new_bit_field3 =
      Bits3::EnumLengthBits::update(new_bit_field3, kInvalidEnumCacheSentinel);
  new_bit_field3 = Bits3::IsDeprecatedBit::update(new_bit_field3, false);
  new_bit_field3 = Bits3::IsInRetainedMapListBit::update(new_bit_field3, false);
  if (!map->is_dictionary_map()) {
    new_bit_field3 = Bits3::IsUnstableBit::update(new_bit_field3, false);
  }
  result->set_bit_field3(new_bit_field3);
  result->clear_padding();
  return result;
}

Handle<Map> Map::Normalize(Isolate* isolate, Handle<Map> fast_map,
                           ElementsKind new_elements_kind,
                           PropertyNormalizationMode mode, const char* reason) {
  DCHECK(!fast_map->is_dictionary_map());

  Handle<Object> maybe_cache(isolate->native_context()->normalized_map_cache(),
                             isolate);
  bool use_cache =
      !fast_map->is_prototype_map() && !maybe_cache->IsUndefined(isolate);
  Handle<NormalizedMapCache> cache;
  if (use_cache) cache = Handle<NormalizedMapCache>::cast(maybe_cache);

  Handle<Map> new_map;
  if (use_cache &&
      cache->Get(fast_map, new_elements_kind, mode).ToHandle(&new_map)) {
#ifdef VERIFY_HEAP
    if (FLAG_verify_heap) new_map->DictionaryMapVerify(isolate);
#endif
#ifdef ENABLE_SLOW_DCHECKS
    if (FLAG_enable_slow_asserts) {
      // The cached map should match newly created normalized map bit-by-bit,
      // except for the code cache, which can contain some ICs which can be
      // applied to the shared map, dependent code and weak cell cache.
      Handle<Map> fresh = Map::CopyNormalized(isolate, fast_map, mode);
      fresh->set_elements_kind(new_elements_kind);

      STATIC_ASSERT(Map::kPrototypeValidityCellOffset ==
                    Map::kDependentCodeOffset + kTaggedSize);
      DCHECK_EQ(0, memcmp(reinterpret_cast<void*>(fresh->address()),
                          reinterpret_cast<void*>(new_map->address()),
                          Map::kBitField3Offset));
      // The IsInRetainedMapListBit might be different if the {new_map}
      // that we got from the {cache} was already embedded into optimized
      // code somewhere.
      // The IsMigrationTargetBit might be different if the {new_map} from
      // {cache} has already been marked as a migration target.
      constexpr int ignored_bit_field3_bits =
          Bits3::IsInRetainedMapListBit::kMask |
          Bits3::IsMigrationTargetBit::kMask;
      DCHECK_EQ(fresh->bit_field3() & ~ignored_bit_field3_bits,
                new_map->bit_field3() & ~ignored_bit_field3_bits);
      int offset = Map::kBitField3Offset + kInt32Size;
      DCHECK_EQ(0, memcmp(reinterpret_cast<void*>(fresh->address() + offset),
                          reinterpret_cast<void*>(new_map->address() + offset),
                          Map::kDependentCodeOffset - offset));
      offset = Map::kPrototypeValidityCellOffset + kTaggedSize;
      if (new_map->is_prototype_map()) {
        // For prototype maps, the PrototypeInfo is not copied.
        STATIC_ASSERT(Map::kTransitionsOrPrototypeInfoOffset ==
                      Map::kPrototypeValidityCellOffset + kTaggedSize);
        offset = kTransitionsOrPrototypeInfoOffset + kTaggedSize;
        DCHECK_EQ(fresh->raw_transitions(),
                  MaybeObject::FromObject(Smi::zero()));
      }
      DCHECK_EQ(0, memcmp(reinterpret_cast<void*>(fresh->address() + offset),
                          reinterpret_cast<void*>(new_map->address() + offset),
                          Map::kSize - offset));
    }
#endif
    if (FLAG_log_maps) {
      LOG(isolate, MapEvent("NormalizeCached", fast_map, new_map, reason));
    }
  } else {
    new_map = Map::CopyNormalized(isolate, fast_map, mode);
    new_map->set_elements_kind(new_elements_kind);
    if (use_cache) {
      cache->Set(fast_map, new_map);
      isolate->counters()->maps_normalized()->Increment();
    }
    if (FLAG_log_maps) {
      LOG(isolate, MapEvent("Normalize", fast_map, new_map, reason));
    }
  }
  fast_map->NotifyLeafMapLayoutChange(isolate);
  return new_map;
}

Handle<Map> Map::CopyNormalized(Isolate* isolate, Handle<Map> map,
                                PropertyNormalizationMode mode) {
  int new_instance_size = map->instance_size();
  if (mode == CLEAR_INOBJECT_PROPERTIES) {
    new_instance_size -= map->GetInObjectProperties() * kTaggedSize;
  }

  Handle<Map> result = RawCopy(
      isolate, map, new_instance_size,
      mode == CLEAR_INOBJECT_PROPERTIES ? 0 : map->GetInObjectProperties());
  // Clear the unused_property_fields explicitly as this field should not
  // be accessed for normalized maps.
  result->SetInObjectUnusedPropertyFields(0);
  result->set_is_dictionary_map(true);
  result->set_is_migration_target(false);
  result->set_may_have_interesting_symbols(true);
  result->set_construction_counter(kNoSlackTracking);

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) result->DictionaryMapVerify(isolate);
#endif

  return result;
}

// Return an immutable prototype exotic object version of the input map.
// Never even try to cache it in the transition tree, as it is intended
// for the global object and its prototype chain, and excluding it saves
// memory on the map transition tree.

// static
Handle<Map> Map::TransitionToImmutableProto(Isolate* isolate, Handle<Map> map) {
  Handle<Map> new_map = Map::Copy(isolate, map, "ImmutablePrototype");
  new_map->set_is_immutable_proto(true);
  return new_map;
}

namespace {
void EnsureInitialMap(Isolate* isolate, Handle<Map> map) {
#ifdef DEBUG
  Object maybe_constructor = map->GetConstructor();
  DCHECK((maybe_constructor.IsJSFunction() &&
          *map == JSFunction::cast(maybe_constructor).initial_map()) ||
         // Below are the exceptions to the check above.
         // Strict function maps have Function as a constructor but the
         // Function's initial map is a sloppy function map.
         *map == *isolate->strict_function_map() ||
         *map == *isolate->strict_function_with_name_map() ||
         // Same holds for GeneratorFunction and its initial map.
         *map == *isolate->generator_function_map() ||
         *map == *isolate->generator_function_with_name_map() ||
         // AsyncFunction has Null as a constructor.
         *map == *isolate->async_function_map() ||
         *map == *isolate->async_function_with_name_map());
#endif
  // Initial maps must not contain descriptors in the descriptors array
  // that do not belong to the map.
  DCHECK_EQ(map->NumberOfOwnDescriptors(),
            map->instance_descriptors(isolate).number_of_descriptors());
}
}  // namespace

// static
Handle<Map> Map::CopyInitialMapNormalized(Isolate* isolate, Handle<Map> map,
                                          PropertyNormalizationMode mode) {
  EnsureInitialMap(isolate, map);
  return CopyNormalized(isolate, map, mode);
}

// static
Handle<Map> Map::CopyInitialMap(Isolate* isolate, Handle<Map> map,
                                int instance_size, int inobject_properties,
                                int unused_property_fields) {
  EnsureInitialMap(isolate, map);

  Handle<Map> result =
      RawCopy(isolate, map, instance_size, inobject_properties);

  // Please note instance_type and instance_size are set when allocated.
  result->SetInObjectUnusedPropertyFields(unused_property_fields);

  int number_of_own_descriptors = map->NumberOfOwnDescriptors();
  if (number_of_own_descriptors > 0) {
    // The copy will use the same descriptors array without ownership.
    DescriptorArray descriptors = map->instance_descriptors(isolate);
    result->set_owns_descriptors(false);
    result->UpdateDescriptors(isolate, descriptors, number_of_own_descriptors);

    DCHECK_EQ(result->NumberOfFields(),
              result->GetInObjectProperties() - result->UnusedPropertyFields());
  }

  return result;
}

Handle<Map> Map::CopyDropDescriptors(Isolate* isolate, Handle<Map> map) {
  Handle<Map> result =
      RawCopy(isolate, map, map->instance_size(),
              map->IsJSObjectMap() ? map->GetInObjectProperties() : 0);

  // Please note instance_type and instance_size are set when allocated.
  if (map->IsJSObjectMap()) {
    result->CopyUnusedPropertyFields(*map);
  }
  map->NotifyLeafMapLayoutChange(isolate);
  return result;
}

Handle<Map> Map::ShareDescriptor(Isolate* isolate, Handle<Map> map,
                                 Handle<DescriptorArray> descriptors,
                                 Descriptor* descriptor) {
  // Sanity check. This path is only to be taken if the map owns its descriptor
  // array, implying that its NumberOfOwnDescriptors equals the number of
  // descriptors in the descriptor array.
  DCHECK_EQ(map->NumberOfOwnDescriptors(),
            map->instance_descriptors(isolate).number_of_descriptors());

  Handle<Map> result = CopyDropDescriptors(isolate, map);
  Handle<Name> name = descriptor->GetKey();

  // Properly mark the {result} if the {name} is an "interesting symbol".
  if (name->IsInterestingSymbol()) {
    result->set_may_have_interesting_symbols(true);
  }

  // Ensure there's space for the new descriptor in the shared descriptor array.
  if (descriptors->number_of_slack_descriptors() == 0) {
    int old_size = descriptors->number_of_descriptors();
    if (old_size == 0) {
      descriptors = DescriptorArray::Allocate(isolate, 0, 1);
    } else {
      int slack = SlackForArraySize(old_size, kMaxNumberOfDescriptors);
      EnsureDescriptorSlack(isolate, map, slack);
      descriptors = handle(map->instance_descriptors(isolate), isolate);
    }
  }

  {
    DisallowGarbageCollection no_gc;
    descriptors->Append(descriptor);
    result->InitializeDescriptors(isolate, *descriptors);
  }

  DCHECK(result->NumberOfOwnDescriptors() == map->NumberOfOwnDescriptors() + 1);
  ConnectTransition(isolate, map, result, name, SIMPLE_PROPERTY_TRANSITION);

  return result;
}

void Map::ConnectTransition(Isolate* isolate, Handle<Map> parent,
                            Handle<Map> child, Handle<Name> name,
                            SimpleTransitionFlag flag) {
  DCHECK_IMPLIES(name->IsInterestingSymbol(),
                 child->may_have_interesting_symbols());
  DCHECK_IMPLIES(parent->may_have_interesting_symbols(),
                 child->may_have_interesting_symbols());
  if (!parent->GetBackPointer().IsUndefined(isolate)) {
    parent->set_owns_descriptors(false);
  } else if (!parent->IsDetached(isolate)) {
    // |parent| is initial map and it must not contain descriptors in the
    // descriptors array that do not belong to the map.
    DCHECK_EQ(parent->NumberOfOwnDescriptors(),
              parent->instance_descriptors(isolate).number_of_descriptors());
  }
  if (parent->IsDetached(isolate)) {
    DCHECK(child->IsDetached(isolate));
    if (FLAG_log_maps) {
      LOG(isolate, MapEvent("Transition", parent, child, "prototype", name));
    }
  } else {
    TransitionsAccessor(isolate, parent).Insert(name, child, flag);
    if (FLAG_log_maps) {
      LOG(isolate, MapEvent("Transition", parent, child, "", name));
    }
  }
}

Handle<Map> Map::CopyReplaceDescriptors(Isolate* isolate, Handle<Map> map,
                                        Handle<DescriptorArray> descriptors,
                                        TransitionFlag flag,
                                        MaybeHandle<Name> maybe_name,
                                        const char* reason,
                                        SimpleTransitionFlag simple_flag) {
  DCHECK(descriptors->IsSortedNoDuplicates());

  Handle<Map> result = CopyDropDescriptors(isolate, map);
  bool is_connected = false;

  // Properly mark the {result} if the {name} is an "interesting symbol".
  Handle<Name> name;
  if (maybe_name.ToHandle(&name) && name->IsInterestingSymbol()) {
    result->set_may_have_interesting_symbols(true);
  }

  if (map->is_prototype_map()) {
    result->InitializeDescriptors(isolate, *descriptors);
  } else {
    if (flag == INSERT_TRANSITION &&
        TransitionsAccessor(isolate, map).CanHaveMoreTransitions()) {
      result->InitializeDescriptors(isolate, *descriptors);

      DCHECK(!maybe_name.is_null());
      ConnectTransition(isolate, map, result, name, simple_flag);
      is_connected = true;
    } else {
      descriptors->GeneralizeAllFields();
      result->InitializeDescriptors(isolate, *descriptors);
    }
  }
  if (FLAG_log_maps && !is_connected) {
    LOG(isolate, MapEvent("ReplaceDescriptors", map, result, reason,
                          maybe_name.is_null() ? Handle<HeapObject>() : name));
  }
  return result;
}

// Creates transition tree starting from |split_map| and adding all descriptors
// starting from descriptor with index |split_map|.NumberOfOwnDescriptors().
// The way how it is done is tricky because of GC and special descriptors
// marking logic.
Handle<Map> Map::AddMissingTransitions(Isolate* isolate, Handle<Map> split_map,
                                       Handle<DescriptorArray> descriptors) {
  DCHECK(descriptors->IsSortedNoDuplicates());
  int split_nof = split_map->NumberOfOwnDescriptors();
  int nof_descriptors = descriptors->number_of_descriptors();
  DCHECK_LT(split_nof, nof_descriptors);

  // Start with creating last map which will own full descriptors array.
  // This is necessary to guarantee that GC will mark the whole descriptor
  // array if any of the allocations happening below fail.
  // Number of unused properties is temporarily incorrect and the layout
  // descriptor could unnecessarily be in slow mode but we will fix after
  // all the other intermediate maps are created.
  // Also the last map might have interesting symbols, we temporarily set
  // the flag and clear it right before the descriptors are installed. This
  // makes heap verification happy and ensures the flag ends up accurate.
  Handle<Map> last_map = CopyDropDescriptors(isolate, split_map);
  last_map->InitializeDescriptors(isolate, *descriptors);
  last_map->SetInObjectUnusedPropertyFields(0);
  last_map->set_may_have_interesting_symbols(true);

  // During creation of intermediate maps we violate descriptors sharing
  // invariant since the last map is not yet connected to the transition tree
  // we create here. But it is safe because GC never trims map's descriptors
  // if there are no dead transitions from that map and this is exactly the
  // case for all the intermediate maps we create here.
  Handle<Map> map = split_map;
  for (InternalIndex i : InternalIndex::Range(split_nof, nof_descriptors - 1)) {
    Handle<Map> new_map = CopyDropDescriptors(isolate, map);
    InstallDescriptors(isolate, map, new_map, i, descriptors);

    map = new_map;
  }
  map->NotifyLeafMapLayoutChange(isolate);
  last_map->set_may_have_interesting_symbols(false);
  InstallDescriptors(isolate, map, last_map, InternalIndex(nof_descriptors - 1),
                     descriptors);
  return last_map;
}

// Since this method is used to rewrite an existing transition tree, it can
// always insert transitions without checking.
void Map::InstallDescriptors(Isolate* isolate, Handle<Map> parent,
                             Handle<Map> child, InternalIndex new_descriptor,
                             Handle<DescriptorArray> descriptors) {
  DCHECK(descriptors->IsSortedNoDuplicates());

  child->SetInstanceDescriptors(isolate, *descriptors,
                                new_descriptor.as_int() + 1);
  child->CopyUnusedPropertyFields(*parent);
  PropertyDetails details = descriptors->GetDetails(new_descriptor);
  if (details.location() == kField) {
    child->AccountAddedPropertyField();
  }

  Handle<Name> name = handle(descriptors->GetKey(new_descriptor), isolate);
  if (parent->may_have_interesting_symbols() || name->IsInterestingSymbol()) {
    child->set_may_have_interesting_symbols(true);
  }
  ConnectTransition(isolate, parent, child, name, SIMPLE_PROPERTY_TRANSITION);
}

Handle<Map> Map::CopyAsElementsKind(Isolate* isolate, Handle<Map> map,
                                    ElementsKind kind, TransitionFlag flag) {
  // Only certain objects are allowed to have non-terminal fast transitional
  // elements kinds.
  DCHECK(map->IsJSObjectMap());
  DCHECK_IMPLIES(
      !map->CanHaveFastTransitionableElementsKind(),
      IsDictionaryElementsKind(kind) || IsTerminalElementsKind(kind));

  Map maybe_elements_transition_map;
  if (flag == INSERT_TRANSITION) {
    // Ensure we are requested to add elements kind transition "near the root".
    DCHECK_EQ(map->FindRootMap(isolate).NumberOfOwnDescriptors(),
              map->NumberOfOwnDescriptors());

    maybe_elements_transition_map = map->ElementsTransitionMap(isolate);
    DCHECK(
        maybe_elements_transition_map.is_null() ||
        (maybe_elements_transition_map.elements_kind() == DICTIONARY_ELEMENTS &&
         kind == DICTIONARY_ELEMENTS));
    DCHECK(!IsFastElementsKind(kind) ||
           IsMoreGeneralElementsKindTransition(map->elements_kind(), kind));
    DCHECK(kind != map->elements_kind());
  }

  bool insert_transition =
      flag == INSERT_TRANSITION &&
      TransitionsAccessor(isolate, map).CanHaveMoreTransitions() &&
      maybe_elements_transition_map.is_null();

  if (insert_transition) {
    Handle<Map> new_map = CopyForElementsTransition(isolate, map);
    new_map->set_elements_kind(kind);

    Handle<Name> name = isolate->factory()->elements_transition_symbol();
    ConnectTransition(isolate, map, new_map, name, SPECIAL_TRANSITION);
    return new_map;
  }

  // Create a new free-floating map only if we are not allowed to store it.
  Handle<Map> new_map = Copy(isolate, map, "CopyAsElementsKind");
  new_map->set_elements_kind(kind);
  return new_map;
}

Handle<Map> Map::AsLanguageMode(Isolate* isolate, Handle<Map> initial_map,
                                Handle<SharedFunctionInfo> shared_info) {
  DCHECK(InstanceTypeChecker::IsJSFunction(initial_map->instance_type()));
  // Initial map for sloppy mode function is stored in the function
  // constructor. Initial maps for strict mode are cached as special transitions
  // using |strict_function_transition_symbol| as a key.
  if (is_sloppy(shared_info->language_mode())) return initial_map;

  Handle<Map> function_map(Map::cast(isolate->native_context()->get(
                               shared_info->function_map_index())),
                           isolate);

  STATIC_ASSERT(LanguageModeSize == 2);
  DCHECK_EQ(LanguageMode::kStrict, shared_info->language_mode());
  Handle<Symbol> transition_symbol =
      isolate->factory()->strict_function_transition_symbol();
  Map maybe_transition = TransitionsAccessor(isolate, initial_map)
                             .SearchSpecial(*transition_symbol);
  if (!maybe_transition.is_null()) {
    return handle(maybe_transition, isolate);
  }
  initial_map->NotifyLeafMapLayoutChange(isolate);

  // Create new map taking descriptors from the |function_map| and all
  // the other details from the |initial_map|.
  Handle<Map> map =
      Map::CopyInitialMap(isolate, function_map, initial_map->instance_size(),
                          initial_map->GetInObjectProperties(),
                          initial_map->UnusedPropertyFields());
  map->SetConstructor(initial_map->GetConstructor());
  map->set_prototype(initial_map->prototype());
  map->set_construction_counter(initial_map->construction_counter());

  if (TransitionsAccessor(isolate, initial_map).CanHaveMoreTransitions()) {
    Map::ConnectTransition(isolate, initial_map, map, transition_symbol,
                           SPECIAL_TRANSITION);
  }
  return map;
}

Handle<Map> Map::CopyForElementsTransition(Isolate* isolate, Handle<Map> map) {
  DCHECK(!map->IsDetached(isolate));
  Handle<Map> new_map = CopyDropDescriptors(isolate, map);

  if (map->owns_descriptors()) {
    // In case the map owned its own descriptors, share the descriptors and
    // transfer ownership to the new map.
    // The properties did not change, so reuse descriptors.
    map->set_owns_descriptors(false);
    new_map->InitializeDescriptors(isolate, map->instance_descriptors(isolate));
  } else {
    // In case the map did not own its own descriptors, a split is forced by
    // copying the map; creating a new descriptor array cell.
    Handle<DescriptorArray> descriptors(map->instance_descriptors(isolate),
                                        isolate);
    int number_of_own_descriptors = map->NumberOfOwnDescriptors();
    Handle<DescriptorArray> new_descriptors = DescriptorArray::CopyUpTo(
        isolate, descriptors, number_of_own_descriptors);
    new_map->InitializeDescriptors(isolate, *new_descriptors);
  }
  return new_map;
}

Handle<Map> Map::Copy(Isolate* isolate, Handle<Map> map, const char* reason) {
  Handle<DescriptorArray> descriptors(map->instance_descriptors(isolate),
                                      isolate);
  int number_of_own_descriptors = map->NumberOfOwnDescriptors();
  Handle<DescriptorArray> new_descriptors = DescriptorArray::CopyUpTo(
      isolate, descriptors, number_of_own_descriptors);
  return CopyReplaceDescriptors(isolate, map, new_descriptors, OMIT_TRANSITION,
                                MaybeHandle<Name>(), reason,
                                SPECIAL_TRANSITION);
}

Handle<Map> Map::Create(Isolate* isolate, int inobject_properties) {
  Handle<Map> copy =
      Copy(isolate, handle(isolate->object_function()->initial_map(), isolate),
           "MapCreate");

  // Check that we do not overflow the instance size when adding the extra
  // inobject properties. If the instance size overflows, we allocate as many
  // properties as we can as inobject properties.
  if (inobject_properties > JSObject::kMaxInObjectProperties) {
    inobject_properties = JSObject::kMaxInObjectProperties;
  }

  int new_instance_size =
      JSObject::kHeaderSize + kTaggedSize * inobject_properties;

  // Adjust the map with the extra inobject properties.
  copy->set_instance_size(new_instance_size);
  copy->SetInObjectPropertiesStartInWords(JSObject::kHeaderSize / kTaggedSize);
  DCHECK_EQ(copy->GetInObjectProperties(), inobject_properties);
  copy->SetInObjectUnusedPropertyFields(inobject_properties);
  copy->set_visitor_id(Map::GetVisitorId(*copy));
  return copy;
}

Handle<Map> Map::CopyForPreventExtensions(
    Isolate* isolate, Handle<Map> map, PropertyAttributes attrs_to_add,
    Handle<Symbol> transition_marker, const char* reason,
    bool old_map_is_dictionary_elements_kind) {
  int num_descriptors = map->NumberOfOwnDescriptors();
  Handle<DescriptorArray> new_desc = DescriptorArray::CopyUpToAddAttributes(
      isolate, handle(map->instance_descriptors(isolate), isolate),
      num_descriptors, attrs_to_add);
  // Do not track transitions during bootstrapping.
  TransitionFlag flag =
      isolate->bootstrapper()->IsActive() ? OMIT_TRANSITION : INSERT_TRANSITION;
  Handle<Map> new_map =
      CopyReplaceDescriptors(isolate, map, new_desc, flag, transition_marker,
                             reason, SPECIAL_TRANSITION);
  new_map->set_is_extensible(false);
  if (!IsTypedArrayElementsKind(map->elements_kind())) {
    ElementsKind new_kind = IsStringWrapperElementsKind(map->elements_kind())
                                ? SLOW_STRING_WRAPPER_ELEMENTS
                                : DICTIONARY_ELEMENTS;
    if (FLAG_enable_sealed_frozen_elements_kind &&
        !old_map_is_dictionary_elements_kind) {
      switch (map->elements_kind()) {
        case PACKED_ELEMENTS:
          if (attrs_to_add == SEALED) {
            new_kind = PACKED_SEALED_ELEMENTS;
          } else if (attrs_to_add == FROZEN) {
            new_kind = PACKED_FROZEN_ELEMENTS;
          } else {
            new_kind = PACKED_NONEXTENSIBLE_ELEMENTS;
          }
          break;
        case PACKED_NONEXTENSIBLE_ELEMENTS:
          if (attrs_to_add == SEALED) {
            new_kind = PACKED_SEALED_ELEMENTS;
          } else if (attrs_to_add == FROZEN) {
            new_kind = PACKED_FROZEN_ELEMENTS;
          }
          break;
        case PACKED_SEALED_ELEMENTS:
          if (attrs_to_add == FROZEN) {
            new_kind = PACKED_FROZEN_ELEMENTS;
          }
          break;
        case HOLEY_ELEMENTS:
          if (attrs_to_add == SEALED) {
            new_kind = HOLEY_SEALED_ELEMENTS;
          } else if (attrs_to_add == FROZEN) {
            new_kind = HOLEY_FROZEN_ELEMENTS;
          } else {
            new_kind = HOLEY_NONEXTENSIBLE_ELEMENTS;
          }
          break;
        case HOLEY_NONEXTENSIBLE_ELEMENTS:
          if (attrs_to_add == SEALED) {
            new_kind = HOLEY_SEALED_ELEMENTS;
          } else if (attrs_to_add == FROZEN) {
            new_kind = HOLEY_FROZEN_ELEMENTS;
          }
          break;
        case HOLEY_SEALED_ELEMENTS:
          if (attrs_to_add == FROZEN) {
            new_kind = HOLEY_FROZEN_ELEMENTS;
          }
          break;
        default:
          break;
      }
    }
    new_map->set_elements_kind(new_kind);
  }
  return new_map;
}

namespace {

bool CanHoldValue(DescriptorArray descriptors, InternalIndex descriptor,
                  PropertyConstness constness, Object value) {
  PropertyDetails details = descriptors.GetDetails(descriptor);
  if (details.location() == kField) {
    if (details.kind() == kData) {
      return IsGeneralizableTo(constness, details.constness()) &&
             value.FitsRepresentation(details.representation()) &&
             descriptors.GetFieldType(descriptor).NowContains(value);
    } else {
      DCHECK_EQ(kAccessor, details.kind());
      return false;
    }

  } else {
    DCHECK_EQ(kDescriptor, details.location());
    DCHECK_EQ(PropertyConstness::kConst, details.constness());
    DCHECK_EQ(kAccessor, details.kind());
    return false;
  }
  UNREACHABLE();
}

Handle<Map> UpdateDescriptorForValue(Isolate* isolate, Handle<Map> map,
                                     InternalIndex descriptor,
                                     PropertyConstness constness,
                                     Handle<Object> value) {
  if (CanHoldValue(map->instance_descriptors(isolate), descriptor, constness,
                   *value)) {
    return map;
  }

  PropertyAttributes attributes =
      map->instance_descriptors(isolate).GetDetails(descriptor).attributes();
  Representation representation = value->OptimalRepresentation(isolate);
  Handle<FieldType> type = value->OptimalType(isolate, representation);

  MapUpdater mu(isolate, map);
  return mu.ReconfigureToDataField(descriptor, attributes, constness,
                                   representation, type);
}

}  // namespace

// static
Handle<Map> Map::PrepareForDataProperty(Isolate* isolate, Handle<Map> map,
                                        InternalIndex descriptor,
                                        PropertyConstness constness,
                                        Handle<Object> value) {
  // Update to the newest map before storing the property.
  map = Update(isolate, map);
  // Dictionaries can store any property value.
  DCHECK(!map->is_dictionary_map());
  return UpdateDescriptorForValue(isolate, map, descriptor, constness, value);
}

Handle<Map> Map::TransitionToDataProperty(Isolate* isolate, Handle<Map> map,
                                          Handle<Name> name,
                                          Handle<Object> value,
                                          PropertyAttributes attributes,
                                          PropertyConstness constness,
                                          StoreOrigin store_origin) {
  RuntimeCallTimerScope stats_scope(
      isolate,
      map->IsDetached(isolate)
          ? RuntimeCallCounterId::kPrototypeMap_TransitionToDataProperty
          : RuntimeCallCounterId::kMap_TransitionToDataProperty);

  DCHECK(name->IsUniqueName());
  DCHECK(!map->is_dictionary_map());

  // Migrate to the newest map before storing the property.
  map = Update(isolate, map);

  Map maybe_transition = TransitionsAccessor(isolate, map)
                             .SearchTransition(*name, kData, attributes);
  if (!maybe_transition.is_null()) {
    Handle<Map> transition(maybe_transition, isolate);
    InternalIndex descriptor = transition->LastAdded();

    DCHECK_EQ(attributes, transition->instance_descriptors(isolate)
                              .GetDetails(descriptor)
                              .attributes());

    return UpdateDescriptorForValue(isolate, transition, descriptor, constness,
                                    value);
  }

  // Do not track transitions during bootstrapping.
  TransitionFlag flag =
      isolate->bootstrapper()->IsActive() ? OMIT_TRANSITION : INSERT_TRANSITION;
  MaybeHandle<Map> maybe_map;
  if (!map->TooManyFastProperties(store_origin)) {
    Representation representation = value->OptimalRepresentation(isolate);
    Handle<FieldType> type = value->OptimalType(isolate, representation);
    maybe_map = Map::CopyWithField(isolate, map, name, type, attributes,
                                   constness, representation, flag);
  }

  Handle<Map> result;
  if (!maybe_map.ToHandle(&result)) {
    const char* reason = "TooManyFastProperties";
#if V8_TRACE_MAPS
    std::unique_ptr<ScopedVector<char>> buffer;
    if (FLAG_log_maps) {
      ScopedVector<char> name_buffer(100);
      name->NameShortPrint(name_buffer);
      buffer.reset(new ScopedVector<char>(128));
      SNPrintF(*buffer, "TooManyFastProperties %s", name_buffer.begin());
      reason = buffer->begin();
    }
#endif
    Handle<Object> maybe_constructor(map->GetConstructor(), isolate);
    if (FLAG_feedback_normalization && map->new_target_is_base() &&
        maybe_constructor->IsJSFunction() &&
        !JSFunction::cast(*maybe_constructor).shared().native()) {
      Handle<JSFunction> constructor =
          Handle<JSFunction>::cast(maybe_constructor);
      DCHECK_NE(*constructor,
                constructor->context().native_context().object_function());
      Handle<Map> initial_map(constructor->initial_map(), isolate);
      result = Map::Normalize(isolate, initial_map, CLEAR_INOBJECT_PROPERTIES,
                              reason);
      initial_map->DeprecateTransitionTree(isolate);
      Handle<HeapObject> prototype(result->prototype(), isolate);
      JSFunction::SetInitialMap(isolate, constructor, result, prototype);

      // Deoptimize all code that embeds the previous initial map.
      initial_map->dependent_code().DeoptimizeDependentCodeGroup(
          DependentCode::kInitialMapChangedGroup);
      if (!result->EquivalentToForNormalization(*map,
                                                CLEAR_INOBJECT_PROPERTIES)) {
        result =
            Map::Normalize(isolate, map, CLEAR_INOBJECT_PROPERTIES, reason);
      }
    } else {
      result = Map::Normalize(isolate, map, CLEAR_INOBJECT_PROPERTIES, reason);
    }
  }

  return result;
}

Handle<Map> Map::ReconfigureExistingProperty(Isolate* isolate, Handle<Map> map,
                                             InternalIndex descriptor,
                                             PropertyKind kind,
                                             PropertyAttributes attributes,
                                             PropertyConstness constness) {
  // Dictionaries have to be reconfigured in-place.
  DCHECK(!map->is_dictionary_map());

  if (!map->GetBackPointer().IsMap()) {
    // There is no benefit from reconstructing transition tree for maps without
    // back pointers, normalize and try to hit the map cache instead.
    return Map::Normalize(isolate, map, CLEAR_INOBJECT_PROPERTIES,
                          "Normalize_AttributesMismatchProtoMap");
  }

  if (FLAG_trace_generalization) {
    map->PrintReconfiguration(isolate, stdout, descriptor, kind, attributes);
  }

  MapUpdater mu(isolate, map);
  DCHECK_EQ(kData, kind);  // Only kData case is supported so far.
  Handle<Map> new_map = mu.ReconfigureToDataField(
      descriptor, attributes, constness, Representation::None(),
      FieldType::None(isolate));
  return new_map;
}

Handle<Map> Map::TransitionToAccessorProperty(Isolate* isolate, Handle<Map> map,
                                              Handle<Name> name,
                                              InternalIndex descriptor,
                                              Handle<Object> getter,
                                              Handle<Object> setter,
                                              PropertyAttributes attributes) {
  RuntimeCallTimerScope stats_scope(
      isolate,
      map->IsDetached(isolate)
          ? RuntimeCallCounterId::kPrototypeMap_TransitionToAccessorProperty
          : RuntimeCallCounterId::kMap_TransitionToAccessorProperty);

  // At least one of the accessors needs to be a new value.
  DCHECK(!getter->IsNull(isolate) || !setter->IsNull(isolate));
  DCHECK(name->IsUniqueName());

  // Migrate to the newest map before transitioning to the new property.
  map = Update(isolate, map);

  // Dictionary maps can always have additional data properties.
  if (map->is_dictionary_map()) return map;

  PropertyNormalizationMode mode = map->is_prototype_map()
                                       ? KEEP_INOBJECT_PROPERTIES
                                       : CLEAR_INOBJECT_PROPERTIES;

  Map maybe_transition = TransitionsAccessor(isolate, map)
                             .SearchTransition(*name, kAccessor, attributes);
  if (!maybe_transition.is_null()) {
    Handle<Map> transition(maybe_transition, isolate);
    DescriptorArray descriptors = transition->instance_descriptors(isolate);
    InternalIndex descriptor = transition->LastAdded();
    DCHECK(descriptors.GetKey(descriptor).Equals(*name));

    DCHECK_EQ(kAccessor, descriptors.GetDetails(descriptor).kind());
    DCHECK_EQ(attributes, descriptors.GetDetails(descriptor).attributes());

    Handle<Object> maybe_pair(descriptors.GetStrongValue(descriptor), isolate);
    if (!maybe_pair->IsAccessorPair()) {
      return Map::Normalize(isolate, map, mode,
                            "TransitionToAccessorFromNonPair");
    }

    Handle<AccessorPair> pair = Handle<AccessorPair>::cast(maybe_pair);
    if (!pair->Equals(*getter, *setter)) {
      return Map::Normalize(isolate, map, mode,
                            "TransitionToDifferentAccessor");
    }

    return transition;
  }

  Handle<AccessorPair> pair;
  DescriptorArray old_descriptors = map->instance_descriptors(isolate);
  if (descriptor.is_found()) {
    if (descriptor != map->LastAdded()) {
      return Map::Normalize(isolate, map, mode, "AccessorsOverwritingNonLast");
    }
    PropertyDetails old_details = old_descriptors.GetDetails(descriptor);
    if (old_details.kind() != kAccessor) {
      return Map::Normalize(isolate, map, mode,
                            "AccessorsOverwritingNonAccessors");
    }

    if (old_details.attributes() != attributes) {
      return Map::Normalize(isolate, map, mode, "AccessorsWithAttributes");
    }

    Handle<Object> maybe_pair(old_descriptors.GetStrongValue(descriptor),
                              isolate);
    if (!maybe_pair->IsAccessorPair()) {
      return Map::Normalize(isolate, map, mode, "AccessorsOverwritingNonPair");
    }

    Handle<AccessorPair> current_pair = Handle<AccessorPair>::cast(maybe_pair);
    if (current_pair->Equals(*getter, *setter)) return map;

    bool overwriting_accessor = false;
    if (!getter->IsNull(isolate) &&
        !current_pair->get(ACCESSOR_GETTER).IsNull(isolate) &&
        current_pair->get(ACCESSOR_GETTER) != *getter) {
      overwriting_accessor = true;
    }
    if (!setter->IsNull(isolate) &&
        !current_pair->get(ACCESSOR_SETTER).IsNull(isolate) &&
        current_pair->get(ACCESSOR_SETTER) != *setter) {
      overwriting_accessor = true;
    }
    if (overwriting_accessor) {
      return Map::Normalize(isolate, map, mode,
                            "AccessorsOverwritingAccessors");
    }

    pair = AccessorPair::Copy(isolate, Handle<AccessorPair>::cast(maybe_pair));
  } else if (map->NumberOfOwnDescriptors() >= kMaxNumberOfDescriptors ||
             map->TooManyFastProperties(StoreOrigin::kNamed)) {
    return Map::Normalize(isolate, map, CLEAR_INOBJECT_PROPERTIES,
                          "TooManyAccessors");
  } else {
    pair = isolate->factory()->NewAccessorPair();
  }

  pair->SetComponents(*getter, *setter);

  // Do not track transitions during bootstrapping.
  TransitionFlag flag =
      isolate->bootstrapper()->IsActive() ? OMIT_TRANSITION : INSERT_TRANSITION;
  Descriptor d = Descriptor::AccessorConstant(name, pair, attributes);
  return Map::CopyInsertDescriptor(isolate, map, &d, flag);
}

Handle<Map> Map::CopyAddDescriptor(Isolate* isolate, Handle<Map> map,
                                   Descriptor* descriptor,
                                   TransitionFlag flag) {
  Handle<DescriptorArray> descriptors(map->instance_descriptors(isolate),
                                      isolate);

  // Share descriptors only if map owns descriptors and it not an initial map.
  if (flag == INSERT_TRANSITION && map->owns_descriptors() &&
      !map->GetBackPointer().IsUndefined(isolate) &&
      TransitionsAccessor(isolate, map).CanHaveMoreTransitions()) {
    return ShareDescriptor(isolate, map, descriptors, descriptor);
  }

  int nof = map->NumberOfOwnDescriptors();
  Handle<DescriptorArray> new_descriptors =
      DescriptorArray::CopyUpTo(isolate, descriptors, nof, 1);
  new_descriptors->Append(descriptor);

  return CopyReplaceDescriptors(isolate, map, new_descriptors, flag,
                                descriptor->GetKey(), "CopyAddDescriptor",
                                SIMPLE_PROPERTY_TRANSITION);
}

Handle<Map> Map::CopyInsertDescriptor(Isolate* isolate, Handle<Map> map,
                                      Descriptor* descriptor,
                                      TransitionFlag flag) {
  Handle<DescriptorArray> old_descriptors(map->instance_descriptors(isolate),
                                          isolate);

  // We replace the key if it is already present.
  InternalIndex index =
      old_descriptors->SearchWithCache(isolate, *descriptor->GetKey(), *map);
  if (index.is_found()) {
    return CopyReplaceDescriptor(isolate, map, old_descriptors, descriptor,
                                 index, flag);
  }
  return CopyAddDescriptor(isolate, map, descriptor, flag);
}

Handle<Map> Map::CopyReplaceDescriptor(Isolate* isolate, Handle<Map> map,
                                       Handle<DescriptorArray> descriptors,
                                       Descriptor* descriptor,
                                       InternalIndex insertion_index,
                                       TransitionFlag flag) {
  Handle<Name> key = descriptor->GetKey();
  DCHECK_EQ(*key, descriptors->GetKey(insertion_index));
  // This function does not support replacing property fields as
  // that would break property field counters.
  DCHECK_NE(kField, descriptor->GetDetails().location());
  DCHECK_NE(kField, descriptors->GetDetails(insertion_index).location());

  Handle<DescriptorArray> new_descriptors = DescriptorArray::CopyUpTo(
      isolate, descriptors, map->NumberOfOwnDescriptors());

  new_descriptors->Replace(insertion_index, descriptor);

  SimpleTransitionFlag simple_flag =
      (insertion_index.as_int() == descriptors->number_of_descriptors() - 1)
          ? SIMPLE_PROPERTY_TRANSITION
          : PROPERTY_TRANSITION;
  return CopyReplaceDescriptors(isolate, map, new_descriptors, flag, key,
                                "CopyReplaceDescriptor", simple_flag);
}

int Map::Hash() {
  // For performance reasons we only hash the 2 most variable fields of a map:
  // prototype map and bit_field2. For predictability reasons  we use objects'
  // offsets in respective pages for hashing instead of raw addresses. We use
  // the map of  the prototype because the prototype itself could be compacted,
  // whereas the map will not be moved.
  // NOTE: If we want to compact maps, this hash function won't work as intended
  // anymore.

  // Shift away the tag.
  int hash = ObjectAddressForHashing(prototype().map().ptr()) >> 2;
  return hash ^ bit_field2();
}

namespace {

bool CheckEquivalent(const Map first, const Map second) {
  return first.GetConstructor() == second.GetConstructor() &&
         first.prototype() == second.prototype() &&
         first.instance_type() == second.instance_type() &&
         first.bit_field() == second.bit_field() &&
         first.is_extensible() == second.is_extensible() &&
         first.new_target_is_base() == second.new_target_is_base();
}

}  // namespace

bool Map::EquivalentToForTransition(const Map other) const {
  CHECK_EQ(GetConstructor(), other.GetConstructor());
  CHECK_EQ(instance_type(), other.instance_type());

  if (bit_field() != other.bit_field()) return false;
  if (new_target_is_base() != other.new_target_is_base()) return false;
  if (prototype() != other.prototype()) return false;
  if (InstanceTypeChecker::IsJSFunction(instance_type())) {
    // JSFunctions require more checks to ensure that sloppy function is
    // not equivalent to strict function.
    int nof =
        std::min(NumberOfOwnDescriptors(), other.NumberOfOwnDescriptors());
    return instance_descriptors().IsEqualUpTo(other.instance_descriptors(),
                                              nof);
  }
  return true;
}

bool Map::EquivalentToForElementsKindTransition(const Map other) const {
  if (!EquivalentToForTransition(other)) return false;
#ifdef DEBUG
  // Ensure that we don't try to generate elements kind transitions from maps
  // with fields that may be generalized in-place. This must already be handled
  // during addition of a new field.
  DescriptorArray descriptors = instance_descriptors();
  for (InternalIndex i : IterateOwnDescriptors()) {
    PropertyDetails details = descriptors.GetDetails(i);
    if (details.location() == kField) {
      DCHECK(IsMostGeneralFieldType(details.representation(),
                                    descriptors.GetFieldType(i)));
    }
  }
#endif
  return true;
}

bool Map::EquivalentToForNormalization(const Map other,
                                       ElementsKind elements_kind,
                                       PropertyNormalizationMode mode) const {
  int properties =
      mode == CLEAR_INOBJECT_PROPERTIES ? 0 : other.GetInObjectProperties();
  // Make sure the elements_kind bits are in bit_field2.
  DCHECK_EQ(this->elements_kind(),
            Map::Bits2::ElementsKindBits::decode(bit_field2()));
  int adjusted_other_bit_field2 =
      Map::Bits2::ElementsKindBits::update(other.bit_field2(), elements_kind);
  return CheckEquivalent(*this, other) &&
         bit_field2() == adjusted_other_bit_field2 &&
         GetInObjectProperties() == properties &&
         JSObject::GetEmbedderFieldCount(*this) ==
             JSObject::GetEmbedderFieldCount(other);
}

static void GetMinInobjectSlack(Map map, void* data) {
  int slack = map.UnusedPropertyFields();
  if (*reinterpret_cast<int*>(data) > slack) {
    *reinterpret_cast<int*>(data) = slack;
  }
}

int Map::ComputeMinObjectSlack(Isolate* isolate) {
  DisallowGarbageCollection no_gc;
  // Has to be an initial map.
  DCHECK(GetBackPointer().IsUndefined(isolate));

  int slack = UnusedPropertyFields();
  TransitionsAccessor transitions(isolate, *this, &no_gc);
  transitions.TraverseTransitionTree(&GetMinInobjectSlack, &slack);
  return slack;
}

static void ShrinkInstanceSize(Map map, void* data) {
  int slack = *reinterpret_cast<int*>(data);
  DCHECK_GE(slack, 0);
#ifdef DEBUG
  int old_visitor_id = Map::GetVisitorId(map);
  int new_unused = map.UnusedPropertyFields() - slack;
#endif
  map.set_instance_size(map.InstanceSizeFromSlack(slack));
  map.set_construction_counter(Map::kNoSlackTracking);
  DCHECK_EQ(old_visitor_id, Map::GetVisitorId(map));
  DCHECK_EQ(new_unused, map.UnusedPropertyFields());
}

static void StopSlackTracking(Map map, void* data) {
  map.set_construction_counter(Map::kNoSlackTracking);
}

void Map::CompleteInobjectSlackTracking(Isolate* isolate) {
  DisallowGarbageCollection no_gc;
  // Has to be an initial map.
  DCHECK(GetBackPointer().IsUndefined(isolate));

  int slack = ComputeMinObjectSlack(isolate);
  TransitionsAccessor transitions(isolate, *this, &no_gc);
  if (slack != 0) {
    // Resize the initial map and all maps in its transition tree.
    transitions.TraverseTransitionTree(&ShrinkInstanceSize, &slack);
  } else {
    transitions.TraverseTransitionTree(&StopSlackTracking, nullptr);
  }
}

void Map::SetInstanceDescriptors(Isolate* isolate, DescriptorArray descriptors,
                                 int number_of_own_descriptors) {
  set_instance_descriptors(descriptors, kReleaseStore);
  SetNumberOfOwnDescriptors(number_of_own_descriptors);
#ifndef V8_DISABLE_WRITE_BARRIERS
  WriteBarrier::Marking(descriptors, number_of_own_descriptors);
#endif
}

// static
Handle<PrototypeInfo> Map::GetOrCreatePrototypeInfo(Handle<JSObject> prototype,
                                                    Isolate* isolate) {
  Object maybe_proto_info = prototype->map().prototype_info();
  if (maybe_proto_info.IsPrototypeInfo()) {
    return handle(PrototypeInfo::cast(maybe_proto_info), isolate);
  }
  Handle<PrototypeInfo> proto_info = isolate->factory()->NewPrototypeInfo();
  prototype->map().set_prototype_info(*proto_info);
  return proto_info;
}

// static
Handle<PrototypeInfo> Map::GetOrCreatePrototypeInfo(Handle<Map> prototype_map,
                                                    Isolate* isolate) {
  Object maybe_proto_info = prototype_map->prototype_info();
  if (maybe_proto_info.IsPrototypeInfo()) {
    return handle(PrototypeInfo::cast(maybe_proto_info), isolate);
  }
  Handle<PrototypeInfo> proto_info = isolate->factory()->NewPrototypeInfo();
  prototype_map->set_prototype_info(*proto_info);
  return proto_info;
}

// static
void Map::SetShouldBeFastPrototypeMap(Handle<Map> map, bool value,
                                      Isolate* isolate) {
  if (value == false && !map->prototype_info().IsPrototypeInfo()) {
    // "False" is the implicit default value, so there's nothing to do.
    return;
  }
  GetOrCreatePrototypeInfo(map, isolate)->set_should_be_fast_map(value);
}

// static
Handle<Object> Map::GetOrCreatePrototypeChainValidityCell(Handle<Map> map,
                                                          Isolate* isolate) {
  Handle<Object> maybe_prototype;
  if (map->IsJSGlobalObjectMap()) {
    DCHECK(map->is_prototype_map());
    // Global object is prototype of a global proxy and therefore we can
    // use its validity cell for guarding global object's prototype change.
    maybe_prototype = isolate->global_object();
  } else {
    maybe_prototype =
        handle(map->GetPrototypeChainRootMap(isolate).prototype(), isolate);
  }
  if (!maybe_prototype->IsJSObject()) {
    return handle(Smi::FromInt(Map::kPrototypeChainValid), isolate);
  }
  Handle<JSObject> prototype = Handle<JSObject>::cast(maybe_prototype);
  // Ensure the prototype is registered with its own prototypes so its cell
  // will be invalidated when necessary.
  JSObject::LazyRegisterPrototypeUser(handle(prototype->map(), isolate),
                                      isolate);

  Object maybe_cell = prototype->map().prototype_validity_cell();
  // Return existing cell if it's still valid.
  if (maybe_cell.IsCell()) {
    Handle<Cell> cell(Cell::cast(maybe_cell), isolate);
    if (cell->value() == Smi::FromInt(Map::kPrototypeChainValid)) {
      return cell;
    }
  }
  // Otherwise create a new cell.
  Handle<Cell> cell = isolate->factory()->NewCell(
      handle(Smi::FromInt(Map::kPrototypeChainValid), isolate));
  prototype->map().set_prototype_validity_cell(*cell);
  return cell;
}

// static
bool Map::IsPrototypeChainInvalidated(Map map) {
  DCHECK(map.is_prototype_map());
  Object maybe_cell = map.prototype_validity_cell();
  if (maybe_cell.IsCell()) {
    Cell cell = Cell::cast(maybe_cell);
    return cell.value() != Smi::FromInt(Map::kPrototypeChainValid);
  }
  return true;
}

// static
void Map::SetPrototype(Isolate* isolate, Handle<Map> map,
                       Handle<HeapObject> prototype,
                       bool enable_prototype_setup_mode) {
  RuntimeCallTimerScope stats_scope(isolate,
                                    RuntimeCallCounterId::kMap_SetPrototype);

  if (prototype->IsJSObject()) {
    Handle<JSObject> prototype_jsobj = Handle<JSObject>::cast(prototype);
    JSObject::OptimizeAsPrototype(prototype_jsobj, enable_prototype_setup_mode);
  } else {
    DCHECK(prototype->IsNull(isolate) || prototype->IsJSProxy());
  }

  WriteBarrierMode wb_mode =
      prototype->IsNull(isolate) ? SKIP_WRITE_BARRIER : UPDATE_WRITE_BARRIER;
  map->set_prototype(*prototype, wb_mode);
}

void Map::StartInobjectSlackTracking() {
  DCHECK(!IsInobjectSlackTrackingInProgress());
  if (UnusedPropertyFields() == 0) return;
  set_construction_counter(Map::kSlackTrackingCounterStart);
}

Handle<Map> Map::TransitionToPrototype(Isolate* isolate, Handle<Map> map,
                                       Handle<HeapObject> prototype) {
  Handle<Map> new_map =
      TransitionsAccessor(isolate, map).GetPrototypeTransition(prototype);
  if (new_map.is_null()) {
    new_map = Copy(isolate, map, "TransitionToPrototype");
    TransitionsAccessor(isolate, map)
        .PutPrototypeTransition(prototype, new_map);
    Map::SetPrototype(isolate, new_map, prototype);
  }
  return new_map;
}

Handle<NormalizedMapCache> NormalizedMapCache::New(Isolate* isolate) {
  Handle<WeakFixedArray> array(
      isolate->factory()->NewWeakFixedArray(kEntries, AllocationType::kOld));
  return Handle<NormalizedMapCache>::cast(array);
}

MaybeHandle<Map> NormalizedMapCache::Get(Handle<Map> fast_map,
                                         ElementsKind elements_kind,
                                         PropertyNormalizationMode mode) {
  DisallowGarbageCollection no_gc;
  MaybeObject value = WeakFixedArray::Get(GetIndex(fast_map));
  HeapObject heap_object;
  if (!value->GetHeapObjectIfWeak(&heap_object)) {
    return MaybeHandle<Map>();
  }

  Map normalized_map = Map::cast(heap_object);
  if (!normalized_map.EquivalentToForNormalization(*fast_map, elements_kind,
                                                   mode)) {
    return MaybeHandle<Map>();
  }
  return handle(normalized_map, GetIsolate());
}

void NormalizedMapCache::Set(Handle<Map> fast_map, Handle<Map> normalized_map) {
  DisallowGarbageCollection no_gc;
  DCHECK(normalized_map->is_dictionary_map());
  WeakFixedArray::Set(GetIndex(fast_map),
                      HeapObjectReference::Weak(*normalized_map));
}

}  // namespace internal
}  // namespace v8
