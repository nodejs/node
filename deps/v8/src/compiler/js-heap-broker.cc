// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-heap-broker.h"
#include "src/objects-inl.h"
#include "src/objects/js-regexp-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

HeapObjectRef::HeapObjectRef(Handle<Object> object) : ObjectRef(object) {
  AllowHandleDereference handle_dereference;
  DCHECK(object->IsHeapObject());
}

MapRef HeapObjectRef::map(const JSHeapBroker* broker) const {
  AllowHandleDereference allow_handle_dereference;
  return MapRef(handle(object<HeapObject>()->map(), broker->isolate()));
}

JSFunctionRef::JSFunctionRef(Handle<Object> object) : JSObjectRef(object) {
  AllowHandleDereference handle_dereference;
  DCHECK(object->IsJSFunction());
}

HeapNumberRef::HeapNumberRef(Handle<Object> object) : HeapObjectRef(object) {
  AllowHandleDereference handle_dereference;
  DCHECK(object->IsHeapNumber());
}

MutableHeapNumberRef::MutableHeapNumberRef(Handle<Object> object)
    : HeapObjectRef(object) {
  AllowHandleDereference handle_dereference;
  DCHECK(object->IsMutableHeapNumber());
}

double HeapNumberRef::value() const {
  AllowHandleDereference allow_handle_dereference;
  return object<HeapNumber>()->value();
}

double MutableHeapNumberRef::value() const {
  AllowHandleDereference allow_handle_dereference;
  return object<MutableHeapNumber>()->value();
}

ContextRef::ContextRef(Handle<Object> object) : HeapObjectRef(object) {
  AllowHandleDereference handle_dereference;
  DCHECK(object->IsContext());
}

NativeContextRef::NativeContextRef(Handle<Object> object) : ContextRef(object) {
  AllowHandleDereference handle_dereference;
  DCHECK(object->IsNativeContext());
}

bool ObjectRef::IsSmi() const {
  AllowHandleDereference allow_handle_dereference;
  return object_->IsSmi();
}

int ObjectRef::AsSmi() const { return object<Smi>()->value(); }

base::Optional<ContextRef> ContextRef::previous(
    const JSHeapBroker* broker) const {
  AllowHandleAllocation handle_allocation;
  AllowHandleDereference handle_dereference;
  Context* previous = object<Context>()->previous();
  if (previous == nullptr) return base::Optional<ContextRef>();
  return ContextRef(handle(previous, broker->isolate()));
}

ObjectRef ContextRef::get(const JSHeapBroker* broker, int index) const {
  AllowHandleAllocation handle_allocation;
  AllowHandleDereference handle_dereference;
  Handle<Object> value(object<Context>()->get(index), broker->isolate());
  return ObjectRef(value);
}

JSHeapBroker::JSHeapBroker(Isolate* isolate) : isolate_(isolate) {}

HeapObjectType JSHeapBroker::HeapObjectTypeFromMap(Map* map) const {
  AllowHandleDereference allow_handle_dereference;
  Heap* heap = isolate_->heap();
  OddballType oddball_type = OddballType::kNone;
  if (map->instance_type() == ODDBALL_TYPE) {
    if (map == heap->undefined_map()) {
      oddball_type = OddballType::kUndefined;
    } else if (map == heap->null_map()) {
      oddball_type = OddballType::kNull;
    } else if (map == heap->boolean_map()) {
      oddball_type = OddballType::kBoolean;
    } else if (map == heap->the_hole_map()) {
      oddball_type = OddballType::kHole;
    } else if (map == heap->uninitialized_map()) {
      oddball_type = OddballType::kUninitialized;
    } else {
      oddball_type = OddballType::kOther;
      DCHECK(map == heap->termination_exception_map() ||
             map == heap->arguments_marker_map() ||
             map == heap->optimized_out_map() ||
             map == heap->stale_register_map());
    }
  }
  HeapObjectType::Flags flags(0);
  if (map->is_undetectable()) flags |= HeapObjectType::kUndetectable;
  if (map->is_callable()) flags |= HeapObjectType::kCallable;

  return HeapObjectType(map->instance_type(), flags, oddball_type);
}

// static
base::Optional<int> JSHeapBroker::TryGetSmi(Handle<Object> object) {
  AllowHandleDereference allow_handle_dereference;
  if (!object->IsSmi()) return base::Optional<int>();
  return Smi::cast(*object)->value();
}

#define HEAP_KIND_FUNCTIONS_DEF(Name)                \
  bool ObjectRef::Is##Name() const {                 \
    AllowHandleDereference allow_handle_dereference; \
    return object<Object>()->Is##Name();             \
  }
HEAP_BROKER_KIND_LIST(HEAP_KIND_FUNCTIONS_DEF)
#undef HEAP_KIND_FUNCTIONS_DEF

#define HEAP_DATA_FUNCTIONS_DEF(Name)       \
  Name##Ref ObjectRef::As##Name() const {   \
    DCHECK(Is##Name());                     \
    return Name##Ref(object<HeapObject>()); \
  }
HEAP_BROKER_DATA_LIST(HEAP_DATA_FUNCTIONS_DEF)
#undef HEAP_DATA_FUNCTIONS_DEF

HeapObjectType HeapObjectRef::type(const JSHeapBroker* broker) const {
  AllowHandleDereference allow_handle_dereference;
  return broker->HeapObjectTypeFromMap(object<HeapObject>()->map());
}

bool JSFunctionRef::HasBuiltinFunctionId() const {
  AllowHandleDereference allow_handle_dereference;
  return object<JSFunction>()->shared()->HasBuiltinFunctionId();
}

BuiltinFunctionId JSFunctionRef::GetBuiltinFunctionId() const {
  AllowHandleDereference allow_handle_dereference;
  return object<JSFunction>()->shared()->builtin_function_id();
}

NameRef::NameRef(Handle<Object> object) : HeapObjectRef(object) {
  AllowHandleDereference handle_dereference;
  DCHECK(object->IsName());
}

ScriptContextTableRef::ScriptContextTableRef(Handle<Object> object)
    : HeapObjectRef(object) {
  AllowHandleDereference handle_dereference;
  DCHECK(object->IsScriptContextTable());
}

base::Optional<ScriptContextTableRef::LookupResult>
ScriptContextTableRef::lookup(const NameRef& name) const {
  AllowHandleDereference handle_dereference;
  if (!name.IsString()) return {};
  ScriptContextTable::LookupResult lookup_result;
  auto table = object<ScriptContextTable>();
  if (!ScriptContextTable::Lookup(table, name.object<String>(),
                                  &lookup_result)) {
    return {};
  }
  Handle<Context> script_context =
      ScriptContextTable::GetContext(table, lookup_result.context_index);
  LookupResult result{ContextRef(script_context),
                      lookup_result.mode == VariableMode::kConst,
                      lookup_result.slot_index};
  return result;
}

ScriptContextTableRef NativeContextRef::script_context_table(
    const JSHeapBroker* broker) const {
  AllowHandleDereference handle_dereference;
  return ScriptContextTableRef(
      handle(object<Context>()->script_context_table(), broker->isolate()));
}

OddballType ObjectRef::oddball_type(const JSHeapBroker* broker) const {
  return IsSmi() ? OddballType::kNone
                 : AsHeapObject().type(broker).oddball_type();
}

ObjectRef FeedbackVectorRef::get(const JSHeapBroker* broker,
                                 FeedbackSlot slot) const {
  AllowHandleDereference handle_dereference;
  Handle<Object> value(object<FeedbackVector>()->Get(slot)->ToObject(),
                       broker->isolate());
  return ObjectRef(value);
}

JSObjectRef AllocationSiteRef::boilerplate(const JSHeapBroker* broker) const {
  AllowHandleDereference handle_dereference;
  Handle<JSObject> value(object<AllocationSite>()->boilerplate(),
                         broker->isolate());
  return JSObjectRef(value);
}

bool JSObjectRef::IsUnboxedDoubleField(FieldIndex index) const {
  AllowHandleDereference handle_dereference;
  return object<JSObject>()->IsUnboxedDoubleField(index);
}

double JSObjectRef::RawFastDoublePropertyAt(FieldIndex index) const {
  AllowHandleDereference handle_dereference;
  return object<JSObject>()->RawFastDoublePropertyAt(index);
}

ObjectRef JSObjectRef::RawFastPropertyAt(const JSHeapBroker* broker,
                                         FieldIndex index) const {
  AllowHandleDereference handle_dereference;
  return ObjectRef(
      handle(object<JSObject>()->RawFastPropertyAt(index), broker->isolate()));
}

FixedArrayBaseRef JSObjectRef::elements(const JSHeapBroker* broker) const {
  AllowHandleDereference handle_dereference;
  return FixedArrayBaseRef(
      handle(object<JSObject>()->elements(), broker->isolate()));
}

namespace {

// Determines whether the given array or object literal boilerplate satisfies
// all limits to be considered for fast deep-copying and computes the total
// size of all objects that are part of the graph.
bool IsFastLiteralHelper(Handle<JSObject> boilerplate, int max_depth,
                         int* max_properties) {
  DCHECK_GE(max_depth, 0);
  DCHECK_GE(*max_properties, 0);

  // Make sure the boilerplate map is not deprecated.
  if (!JSObject::TryMigrateInstance(boilerplate)) return false;

  // Check for too deep nesting.
  if (max_depth == 0) return false;

  // Check the elements.
  Isolate* const isolate = boilerplate->GetIsolate();
  Handle<FixedArrayBase> elements(boilerplate->elements(), isolate);
  if (elements->length() > 0 &&
      elements->map() != isolate->heap()->fixed_cow_array_map()) {
    if (boilerplate->HasSmiOrObjectElements()) {
      Handle<FixedArray> fast_elements = Handle<FixedArray>::cast(elements);
      int length = elements->length();
      for (int i = 0; i < length; i++) {
        if ((*max_properties)-- == 0) return false;
        Handle<Object> value(fast_elements->get(i), isolate);
        if (value->IsJSObject()) {
          Handle<JSObject> value_object = Handle<JSObject>::cast(value);
          if (!IsFastLiteralHelper(value_object, max_depth - 1,
                                   max_properties)) {
            return false;
          }
        }
      }
    } else if (boilerplate->HasDoubleElements()) {
      if (elements->Size() > kMaxRegularHeapObjectSize) return false;
    } else {
      return false;
    }
  }

  // TODO(turbofan): Do we want to support out-of-object properties?
  if (!(boilerplate->HasFastProperties() &&
        boilerplate->property_array()->length() == 0)) {
    return false;
  }

  // Check the in-object properties.
  Handle<DescriptorArray> descriptors(
      boilerplate->map()->instance_descriptors(), isolate);
  int limit = boilerplate->map()->NumberOfOwnDescriptors();
  for (int i = 0; i < limit; i++) {
    PropertyDetails details = descriptors->GetDetails(i);
    if (details.location() != kField) continue;
    DCHECK_EQ(kData, details.kind());
    if ((*max_properties)-- == 0) return false;
    FieldIndex field_index = FieldIndex::ForDescriptor(boilerplate->map(), i);
    if (boilerplate->IsUnboxedDoubleField(field_index)) continue;
    Handle<Object> value(boilerplate->RawFastPropertyAt(field_index), isolate);
    if (value->IsJSObject()) {
      Handle<JSObject> value_object = Handle<JSObject>::cast(value);
      if (!IsFastLiteralHelper(value_object, max_depth - 1, max_properties)) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace

// Maximum depth and total number of elements and properties for literal
// graphs to be considered for fast deep-copying. The limit is chosen to
// match the maximum number of inobject properties, to ensure that the
// performance of using object literals is not worse than using constructor
// functions, see crbug.com/v8/6211 for details.
const int kMaxFastLiteralDepth = 3;
const int kMaxFastLiteralProperties = JSObject::kMaxInObjectProperties;

// Determines whether the given array or object literal boilerplate satisfies
// all limits to be considered for fast deep-copying and computes the total
// size of all objects that are part of the graph.
bool AllocationSiteRef::IsFastLiteral(const JSHeapBroker* broker) {
  AllowHandleDereference allow_handle_dereference;
  int max_properties = kMaxFastLiteralProperties;
  Handle<JSObject> boilerplate(object<AllocationSite>()->boilerplate(),
                               broker->isolate());
  return IsFastLiteralHelper(boilerplate, kMaxFastLiteralDepth,
                             &max_properties);
}

PretenureFlag AllocationSiteRef::GetPretenureMode() const {
  AllowHandleDereference allow_handle_dereference;
  return object<AllocationSite>()->GetPretenureMode();
}

void JSObjectRef::EnsureElementsTenured(const JSHeapBroker* broker) {
  // TODO(jarin) Eventually, we will pretenure the boilerplates before
  // the compilation job starts.
  AllowHandleDereference allow_handle_dereference;
  Handle<FixedArrayBase> object_elements =
      elements(broker).object<FixedArrayBase>();
  if (broker->isolate()->heap()->InNewSpace(*object_elements)) {
    // If we would like to pretenure a fixed cow array, we must ensure that
    // the array is already in old space, otherwise we'll create too many
    // old-to-new-space pointers (overflowing the store buffer).
    object_elements = Handle<FixedArrayBase>(
        broker->isolate()->factory()->CopyAndTenureFixedCOWArray(
            Handle<FixedArray>::cast(object_elements)));
    object<JSObject>()->set_elements(*object_elements);
  }
}

int MapRef::GetInObjectProperties() const {
  AllowHandleDereference allow_handle_dereference;
  return object<Map>()->GetInObjectProperties();
}

int MapRef::NumberOfOwnDescriptors() const {
  AllowHandleDereference allow_handle_dereference;
  return object<Map>()->NumberOfOwnDescriptors();
}

FieldIndex MapRef::GetFieldIndexFor(int i) const {
  AllowHandleDereference allow_handle_dereference;
  return FieldIndex::ForDescriptor(*object<Map>(), i);
}

int MapRef::instance_size() const {
  AllowHandleDereference allow_handle_dereference;
  return object<Map>()->instance_size();
}

InstanceType MapRef::instance_type() const {
  AllowHandleDereference allow_handle_dereference;
  return object<Map>()->instance_type();
}

PropertyDetails MapRef::GetPropertyDetails(int i) const {
  AllowHandleDereference allow_handle_dereference;
  return object<Map>()->instance_descriptors()->GetDetails(i);
}

NameRef MapRef::GetPropertyKey(const JSHeapBroker* broker, int i) const {
  AllowHandleDereference allow_handle_dereference;
  return NameRef(handle(object<Map>()->instance_descriptors()->GetKey(i),
                        broker->isolate()));
}

bool MapRef::IsJSArrayMap() const {
  AllowHandleDereference allow_handle_dereference;
  return object<Map>()->IsJSArrayMap();
}

bool MapRef::IsFixedCowArrayMap(const JSHeapBroker* broker) const {
  AllowHandleDereference allow_handle_dereference;
  return *object<Map>() == broker->isolate()->heap()->fixed_cow_array_map();
}

ElementsKind JSArrayRef::GetElementsKind() const {
  AllowHandleDereference allow_handle_dereference;
  return object<JSArray>()->GetElementsKind();
}

ObjectRef JSArrayRef::length(const JSHeapBroker* broker) const {
  AllowHandleDereference allow_handle_dereference;
  return ObjectRef(handle(object<JSArray>()->length(), broker->isolate()));
}

ObjectRef JSRegExpRef::raw_properties_or_hash(
    const JSHeapBroker* broker) const {
  AllowHandleDereference allow_handle_dereference;
  return ObjectRef(
      handle(object<JSRegExp>()->raw_properties_or_hash(), broker->isolate()));
}

ObjectRef JSRegExpRef::data(const JSHeapBroker* broker) const {
  AllowHandleDereference allow_handle_dereference;
  return ObjectRef(handle(object<JSRegExp>()->data(), broker->isolate()));
}

ObjectRef JSRegExpRef::source(const JSHeapBroker* broker) const {
  AllowHandleDereference allow_handle_dereference;
  return ObjectRef(handle(object<JSRegExp>()->source(), broker->isolate()));
}

ObjectRef JSRegExpRef::flags(const JSHeapBroker* broker) const {
  AllowHandleDereference allow_handle_dereference;
  return ObjectRef(handle(object<JSRegExp>()->flags(), broker->isolate()));
}

ObjectRef JSRegExpRef::last_index(const JSHeapBroker* broker) const {
  AllowHandleDereference allow_handle_dereference;
  return ObjectRef(handle(object<JSRegExp>()->last_index(), broker->isolate()));
}

int FixedArrayBaseRef::length() const {
  AllowHandleDereference allow_handle_dereference;
  return object<FixedArrayBase>()->length();
}

bool FixedArrayRef::is_the_hole(const JSHeapBroker* broker, int i) const {
  AllowHandleDereference allow_handle_dereference;
  return object<FixedArray>()->is_the_hole(broker->isolate(), i);
}

ObjectRef FixedArrayRef::get(const JSHeapBroker* broker, int i) const {
  AllowHandleDereference allow_handle_dereference;
  return ObjectRef(handle(object<FixedArray>()->get(i), broker->isolate()));
}

bool FixedDoubleArrayRef::is_the_hole(int i) const {
  AllowHandleDereference allow_handle_dereference;
  return object<FixedDoubleArray>()->is_the_hole(i);
}

double FixedDoubleArrayRef::get_scalar(int i) const {
  AllowHandleDereference allow_handle_dereference;
  return object<FixedDoubleArray>()->get_scalar(i);
}

int SharedFunctionInfoRef::internal_formal_parameter_count() const {
  AllowHandleDereference allow_handle_dereference;
  return object<SharedFunctionInfo>()->internal_formal_parameter_count();
}

bool SharedFunctionInfoRef::has_duplicate_parameters() const {
  AllowHandleDereference allow_handle_dereference;
  return object<SharedFunctionInfo>()->has_duplicate_parameters();
}

MapRef NativeContextRef::fast_aliased_arguments_map(
    const JSHeapBroker* broker) const {
  AllowHandleDereference allow_handle_dereference;
  return MapRef(handle(object<Context>()->fast_aliased_arguments_map(),
                       broker->isolate()));
}

MapRef NativeContextRef::sloppy_arguments_map(
    const JSHeapBroker* broker) const {
  AllowHandleDereference allow_handle_dereference;
  return MapRef(
      handle(object<Context>()->sloppy_arguments_map(), broker->isolate()));
}

MapRef NativeContextRef::strict_arguments_map(
    const JSHeapBroker* broker) const {
  AllowHandleDereference allow_handle_dereference;
  return MapRef(
      handle(object<Context>()->strict_arguments_map(), broker->isolate()));
}

MapRef NativeContextRef::js_array_fast_elements_map_index(
    const JSHeapBroker* broker) const {
  AllowHandleDereference allow_handle_dereference;
  return MapRef(handle(object<Context>()->js_array_fast_elements_map_index(),
                       broker->isolate()));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
