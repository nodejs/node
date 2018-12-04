// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-heap-broker.h"

#include "src/ast/modules.h"
#include "src/bootstrapper.h"
#include "src/boxed-float.h"
#include "src/code-factory.h"
#include "src/compiler/graph-reducer.h"
#include "src/compiler/per-isolate-compiler-cache.h"
#include "src/objects-inl.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/js-regexp-inl.h"
#include "src/objects/module-inl.h"
#include "src/utils.h"

namespace v8 {
namespace internal {
namespace compiler {

#define FORWARD_DECL(Name) class Name##Data;
HEAP_BROKER_OBJECT_LIST(FORWARD_DECL)
#undef FORWARD_DECL

// There are three kinds of ObjectData values.
//
// kSmi: The underlying V8 object is a Smi and the data is an instance of the
//   base class (ObjectData), i.e. it's basically just the handle.  Because the
//   object is a Smi, it's safe to access the handle in order to extract the
//   number value, and AsSmi() does exactly that.
//
// kSerializedHeapObject: The underlying V8 object is a HeapObject and the
//   data is an instance of the corresponding (most-specific) subclass, e.g.
//   JSFunctionData, which provides serialized information about the object.
//
// kUnserializedHeapObject: The underlying V8 object is a HeapObject and the
//   data is an instance of the base class (ObjectData), i.e. it basically
//   carries no information other than the handle.
//
enum ObjectDataKind { kSmi, kSerializedHeapObject, kUnserializedHeapObject };

class ObjectData : public ZoneObject {
 public:
  ObjectData(JSHeapBroker* broker, ObjectData** storage, Handle<Object> object,
             ObjectDataKind kind)
      : object_(object), kind_(kind) {
    // This assignment ensures we don't end up inserting the same object
    // in an endless recursion.
    *storage = this;

    broker->Trace("Creating data %p for handle %" V8PRIuPTR " (", this,
                  object.address());
    if (FLAG_trace_heap_broker) {
      object->ShortPrint();
      PrintF(")\n");
    }
    CHECK_NOT_NULL(broker->isolate()->handle_scope_data()->canonical_scope);
  }

#define DECLARE_IS_AND_AS(Name) \
  bool Is##Name() const;        \
  Name##Data* As##Name();
  HEAP_BROKER_OBJECT_LIST(DECLARE_IS_AND_AS)
#undef DECLARE_IS_AND_AS

  Handle<Object> object() const { return object_; }
  ObjectDataKind kind() const { return kind_; }
  bool is_smi() const { return kind_ == kSmi; }

 private:
  Handle<Object> const object_;
  ObjectDataKind const kind_;
};

class HeapObjectData : public ObjectData {
 public:
  HeapObjectData(JSHeapBroker* broker, ObjectData** storage,
                 Handle<HeapObject> object);

  bool boolean_value() const { return boolean_value_; }
  MapData* map() const { return map_; }

  static HeapObjectData* Serialize(JSHeapBroker* broker,
                                   Handle<HeapObject> object);

 private:
  bool const boolean_value_;
  MapData* const map_;
};

class PropertyCellData : public HeapObjectData {
 public:
  PropertyCellData(JSHeapBroker* broker, ObjectData** storage,
                   Handle<PropertyCell> object);

  PropertyDetails property_details() const { return property_details_; }

  void Serialize(JSHeapBroker* broker);
  ObjectData* value() { return value_; }

 private:
  PropertyDetails const property_details_;

  bool serialized_ = false;
  ObjectData* value_ = nullptr;
};

void JSHeapBroker::IncrementTracingIndentation() { ++tracing_indentation_; }

void JSHeapBroker::DecrementTracingIndentation() { --tracing_indentation_; }

class TraceScope {
 public:
  TraceScope(JSHeapBroker* broker, const char* label)
      : TraceScope(broker, static_cast<void*>(broker), label) {}

  TraceScope(JSHeapBroker* broker, ObjectData* data, const char* label)
      : TraceScope(broker, static_cast<void*>(data), label) {}

  ~TraceScope() { broker_->DecrementTracingIndentation(); }

 private:
  JSHeapBroker* const broker_;

  TraceScope(JSHeapBroker* broker, void* self, const char* label)
      : broker_(broker) {
    broker_->Trace("Running %s on %p.\n", label, self);
    broker_->IncrementTracingIndentation();
  }
};

PropertyCellData::PropertyCellData(JSHeapBroker* broker, ObjectData** storage,
                                   Handle<PropertyCell> object)
    : HeapObjectData(broker, storage, object),
      property_details_(object->property_details()) {}

void PropertyCellData::Serialize(JSHeapBroker* broker) {
  if (serialized_) return;
  serialized_ = true;

  TraceScope tracer(broker, this, "PropertyCellData::Serialize");
  auto cell = Handle<PropertyCell>::cast(object());
  DCHECK_NULL(value_);
  value_ = broker->GetOrCreateData(cell->value());
}

class JSObjectField {
 public:
  bool IsDouble() const { return object_ == nullptr; }
  double AsDouble() const {
    CHECK(IsDouble());
    return number_;
  }

  bool IsObject() const { return object_ != nullptr; }
  ObjectData* AsObject() const {
    CHECK(IsObject());
    return object_;
  }

  explicit JSObjectField(double value) : number_(value) {}
  explicit JSObjectField(ObjectData* value) : object_(value) {}

 private:
  ObjectData* object_ = nullptr;
  double number_ = 0;
};

class JSObjectData : public HeapObjectData {
 public:
  JSObjectData(JSHeapBroker* broker, ObjectData** storage,
               Handle<JSObject> object);

  // Recursively serializes all reachable JSObjects.
  void SerializeAsBoilerplate(JSHeapBroker* broker);
  // Shallow serialization of {elements}.
  void SerializeElements(JSHeapBroker* broker);

  const JSObjectField& GetInobjectField(int property_index) const;
  FixedArrayBaseData* elements() const;

  // This method is only used to assert our invariants.
  bool cow_or_empty_elements_tenured() const;

  void SerializeObjectCreateMap(JSHeapBroker* broker);
  MapData* object_create_map() const {  // Can be nullptr.
    CHECK(serialized_object_create_map_);
    return object_create_map_;
  }

 private:
  void SerializeRecursive(JSHeapBroker* broker, int max_depths);

  FixedArrayBaseData* elements_ = nullptr;
  bool cow_or_empty_elements_tenured_ = false;
  // The {serialized_as_boilerplate} flag is set when all recursively
  // reachable JSObjects are serialized.
  bool serialized_as_boilerplate_ = false;
  bool serialized_elements_ = false;

  ZoneVector<JSObjectField> inobject_fields_;

  bool serialized_object_create_map_ = false;
  MapData* object_create_map_ = nullptr;
};

void JSObjectData::SerializeObjectCreateMap(JSHeapBroker* broker) {
  if (serialized_object_create_map_) return;
  serialized_object_create_map_ = true;

  TraceScope tracer(broker, this, "JSObjectData::SerializeObjectCreateMap");
  Handle<JSObject> jsobject = Handle<JSObject>::cast(object());

  if (jsobject->map()->is_prototype_map()) {
    Handle<Object> maybe_proto_info(jsobject->map()->prototype_info(),
                                    broker->isolate());
    if (maybe_proto_info->IsPrototypeInfo()) {
      auto proto_info = Handle<PrototypeInfo>::cast(maybe_proto_info);
      if (proto_info->HasObjectCreateMap()) {
        DCHECK_NULL(object_create_map_);
        object_create_map_ =
            broker->GetOrCreateData(proto_info->ObjectCreateMap())->AsMap();
      }
    }
  }
}

class JSFunctionData : public JSObjectData {
 public:
  JSFunctionData(JSHeapBroker* broker, ObjectData** storage,
                 Handle<JSFunction> object);

  bool has_initial_map() const { return has_initial_map_; }
  bool has_prototype() const { return has_prototype_; }
  bool PrototypeRequiresRuntimeLookup() const {
    return PrototypeRequiresRuntimeLookup_;
  }

  void Serialize(JSHeapBroker* broker);

  JSGlobalProxyData* global_proxy() const { return global_proxy_; }
  MapData* initial_map() const { return initial_map_; }
  ObjectData* prototype() const { return prototype_; }
  SharedFunctionInfoData* shared() const { return shared_; }
  int initial_map_instance_size_with_min_slack() const {
    CHECK(serialized_);
    return initial_map_instance_size_with_min_slack_;
  }

 private:
  bool has_initial_map_;
  bool has_prototype_;
  bool PrototypeRequiresRuntimeLookup_;

  bool serialized_ = false;

  JSGlobalProxyData* global_proxy_ = nullptr;
  MapData* initial_map_ = nullptr;
  ObjectData* prototype_ = nullptr;
  SharedFunctionInfoData* shared_ = nullptr;
  int initial_map_instance_size_with_min_slack_;
};

class JSRegExpData : public JSObjectData {
 public:
  JSRegExpData(JSHeapBroker* broker, ObjectData** storage,
               Handle<JSRegExp> object)
      : JSObjectData(broker, storage, object) {}

  void SerializeAsRegExpBoilerplate(JSHeapBroker* broker);

  ObjectData* raw_properties_or_hash() const { return raw_properties_or_hash_; }
  ObjectData* data() const { return data_; }
  ObjectData* source() const { return source_; }
  ObjectData* flags() const { return flags_; }
  ObjectData* last_index() const { return last_index_; }

 private:
  bool serialized_as_reg_exp_boilerplate_ = false;

  ObjectData* raw_properties_or_hash_ = nullptr;
  ObjectData* data_ = nullptr;
  ObjectData* source_ = nullptr;
  ObjectData* flags_ = nullptr;
  ObjectData* last_index_ = nullptr;
};

class HeapNumberData : public HeapObjectData {
 public:
  HeapNumberData(JSHeapBroker* broker, ObjectData** storage,
                 Handle<HeapNumber> object)
      : HeapObjectData(broker, storage, object), value_(object->value()) {}

  double value() const { return value_; }

 private:
  double const value_;
};

class MutableHeapNumberData : public HeapObjectData {
 public:
  MutableHeapNumberData(JSHeapBroker* broker, ObjectData** storage,
                        Handle<MutableHeapNumber> object)
      : HeapObjectData(broker, storage, object), value_(object->value()) {}

  double value() const { return value_; }

 private:
  double const value_;
};

class ContextData : public HeapObjectData {
 public:
  ContextData(JSHeapBroker* broker, ObjectData** storage,
              Handle<Context> object);
  void Serialize(JSHeapBroker* broker);

  ContextData* previous() const {
    CHECK(serialized_);
    return previous_;
  }

 private:
  bool serialized_ = false;
  ContextData* previous_ = nullptr;
};

ContextData::ContextData(JSHeapBroker* broker, ObjectData** storage,
                         Handle<Context> object)
    : HeapObjectData(broker, storage, object) {}

void ContextData::Serialize(JSHeapBroker* broker) {
  if (serialized_) return;
  serialized_ = true;

  TraceScope tracer(broker, this, "ContextData::Serialize");
  Handle<Context> context = Handle<Context>::cast(object());

  DCHECK_NULL(previous_);
  // Context::previous DCHECK-fails when called on the native context.
  if (!context->IsNativeContext()) {
    previous_ = broker->GetOrCreateData(context->previous())->AsContext();
    previous_->Serialize(broker);
  }
}

class NativeContextData : public ContextData {
 public:
#define DECL_ACCESSOR(type, name) \
  type##Data* name() const { return name##_; }
  BROKER_NATIVE_CONTEXT_FIELDS(DECL_ACCESSOR)
#undef DECL_ACCESSOR

  const ZoneVector<MapData*>& function_maps() const {
    CHECK(serialized_);
    return function_maps_;
  }

  NativeContextData(JSHeapBroker* broker, ObjectData** storage,
                    Handle<NativeContext> object);
  void Serialize(JSHeapBroker* broker);

 private:
  bool serialized_ = false;
#define DECL_MEMBER(type, name) type##Data* name##_ = nullptr;
  BROKER_NATIVE_CONTEXT_FIELDS(DECL_MEMBER)
#undef DECL_MEMBER
  ZoneVector<MapData*> function_maps_;
};

class NameData : public HeapObjectData {
 public:
  NameData(JSHeapBroker* broker, ObjectData** storage, Handle<Name> object)
      : HeapObjectData(broker, storage, object) {}
};

class StringData : public NameData {
 public:
  StringData(JSHeapBroker* broker, ObjectData** storage, Handle<String> object);

  int length() const { return length_; }
  uint16_t first_char() const { return first_char_; }
  base::Optional<double> to_number() const { return to_number_; }
  bool is_external_string() const { return is_external_string_; }
  bool is_seq_string() const { return is_seq_string_; }

 private:
  int const length_;
  uint16_t const first_char_;
  base::Optional<double> to_number_;
  bool const is_external_string_;
  bool const is_seq_string_;

  static constexpr int kMaxLengthForDoubleConversion = 23;
};

StringData::StringData(JSHeapBroker* broker, ObjectData** storage,
                       Handle<String> object)
    : NameData(broker, storage, object),
      length_(object->length()),
      first_char_(length_ > 0 ? object->Get(0) : 0),
      is_external_string_(object->IsExternalString()),
      is_seq_string_(object->IsSeqString()) {
  int flags = ALLOW_HEX | ALLOW_OCTAL | ALLOW_BINARY;
  if (length_ <= kMaxLengthForDoubleConversion) {
    to_number_ = StringToDouble(
        broker->isolate(), broker->isolate()->unicode_cache(), object, flags);
  }
}

class InternalizedStringData : public StringData {
 public:
  InternalizedStringData(JSHeapBroker* broker, ObjectData** storage,
                         Handle<InternalizedString> object)
      : StringData(broker, storage, object) {}
};

namespace {

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
      elements->map() != ReadOnlyRoots(isolate).fixed_cow_array_map()) {
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
bool IsInlinableFastLiteral(Handle<JSObject> boilerplate) {
  int max_properties = kMaxFastLiteralProperties;
  return IsFastLiteralHelper(boilerplate, kMaxFastLiteralDepth,
                             &max_properties);
}

}  // namespace

class AllocationSiteData : public HeapObjectData {
 public:
  AllocationSiteData(JSHeapBroker* broker, ObjectData** storage,
                     Handle<AllocationSite> object);
  void SerializeBoilerplate(JSHeapBroker* broker);

  bool PointsToLiteral() const { return PointsToLiteral_; }
  PretenureFlag GetPretenureMode() const { return GetPretenureMode_; }
  ObjectData* nested_site() const { return nested_site_; }
  bool IsFastLiteral() const { return IsFastLiteral_; }
  JSObjectData* boilerplate() const { return boilerplate_; }

  // These are only valid if PointsToLiteral is false.
  ElementsKind GetElementsKind() const { return GetElementsKind_; }
  bool CanInlineCall() const { return CanInlineCall_; }

 private:
  bool const PointsToLiteral_;
  PretenureFlag const GetPretenureMode_;
  ObjectData* nested_site_ = nullptr;
  bool IsFastLiteral_ = false;
  JSObjectData* boilerplate_ = nullptr;
  ElementsKind GetElementsKind_ = NO_ELEMENTS;
  bool CanInlineCall_ = false;
  bool serialized_boilerplate_ = false;
};

// Only used in JSNativeContextSpecialization.
class ScriptContextTableData : public HeapObjectData {
 public:
  ScriptContextTableData(JSHeapBroker* broker, ObjectData** storage,
                         Handle<ScriptContextTable> object)
      : HeapObjectData(broker, storage, object) {}
};

struct PropertyDescriptor {
  NameData* key = nullptr;
  PropertyDetails details = PropertyDetails::Empty();
  FieldIndex field_index;
  MapData* field_owner = nullptr;
  ObjectData* field_type = nullptr;
  bool is_unboxed_double_field = false;
};

class MapData : public HeapObjectData {
 public:
  MapData(JSHeapBroker* broker, ObjectData** storage, Handle<Map> object);

  InstanceType instance_type() const { return instance_type_; }
  int instance_size() const { return instance_size_; }
  byte bit_field() const { return bit_field_; }
  byte bit_field2() const { return bit_field2_; }
  uint32_t bit_field3() const { return bit_field3_; }
  bool can_be_deprecated() const { return can_be_deprecated_; }
  bool can_transition() const { return can_transition_; }
  int in_object_properties_start_in_words() const {
    CHECK(InstanceTypeChecker::IsJSObject(instance_type()));
    return in_object_properties_start_in_words_;
  }
  int in_object_properties() const {
    CHECK(InstanceTypeChecker::IsJSObject(instance_type()));
    return in_object_properties_;
  }

  // Extra information.

  void SerializeElementsKindGeneralizations(JSHeapBroker* broker);
  const ZoneVector<MapData*>& elements_kind_generalizations() const {
    CHECK(serialized_elements_kind_generalizations_);
    return elements_kind_generalizations_;
  }

  // Serialize the own part of the descriptor array and, recursively, that of
  // any field owner.
  void SerializeOwnDescriptors(JSHeapBroker* broker);
  DescriptorArrayData* instance_descriptors() const {
    CHECK(serialized_own_descriptors_);
    return instance_descriptors_;
  }

  void SerializeConstructorOrBackpointer(JSHeapBroker* broker);
  ObjectData* constructor_or_backpointer() const {
    CHECK(serialized_constructor_or_backpointer_);
    return constructor_or_backpointer_;
  }

  void SerializePrototype(JSHeapBroker* broker);
  ObjectData* prototype() const {
    CHECK(serialized_prototype_);
    return prototype_;
  }

 private:
  InstanceType const instance_type_;
  int const instance_size_;
  byte const bit_field_;
  byte const bit_field2_;
  uint32_t const bit_field3_;
  bool const can_be_deprecated_;
  bool const can_transition_;
  int const in_object_properties_start_in_words_;
  int const in_object_properties_;

  bool serialized_elements_kind_generalizations_ = false;
  ZoneVector<MapData*> elements_kind_generalizations_;

  bool serialized_own_descriptors_ = false;
  DescriptorArrayData* instance_descriptors_ = nullptr;

  bool serialized_constructor_or_backpointer_ = false;
  ObjectData* constructor_or_backpointer_ = nullptr;

  bool serialized_prototype_ = false;
  ObjectData* prototype_ = nullptr;
};

AllocationSiteData::AllocationSiteData(JSHeapBroker* broker,
                                       ObjectData** storage,
                                       Handle<AllocationSite> object)
    : HeapObjectData(broker, storage, object),
      PointsToLiteral_(object->PointsToLiteral()),
      GetPretenureMode_(object->GetPretenureMode()) {
  if (PointsToLiteral_) {
    IsFastLiteral_ = IsInlinableFastLiteral(
        handle(object->boilerplate(), broker->isolate()));
  } else {
    GetElementsKind_ = object->GetElementsKind();
    CanInlineCall_ = object->CanInlineCall();
  }
}

void AllocationSiteData::SerializeBoilerplate(JSHeapBroker* broker) {
  if (serialized_boilerplate_) return;
  serialized_boilerplate_ = true;

  TraceScope tracer(broker, this, "AllocationSiteData::SerializeBoilerplate");
  Handle<AllocationSite> site = Handle<AllocationSite>::cast(object());

  CHECK(IsFastLiteral_);
  DCHECK_NULL(boilerplate_);
  boilerplate_ = broker->GetOrCreateData(site->boilerplate())->AsJSObject();
  boilerplate_->SerializeAsBoilerplate(broker);

  DCHECK_NULL(nested_site_);
  nested_site_ = broker->GetOrCreateData(site->nested_site());
  if (nested_site_->IsAllocationSite()) {
    nested_site_->AsAllocationSite()->SerializeBoilerplate(broker);
  }
}

HeapObjectData::HeapObjectData(JSHeapBroker* broker, ObjectData** storage,
                               Handle<HeapObject> object)
    : ObjectData(broker, storage, object, kSerializedHeapObject),
      boolean_value_(object->BooleanValue(broker->isolate())),
      // We have to use a raw cast below instead of AsMap() because of
      // recursion. AsMap() would call IsMap(), which accesses the
      // instance_type_ member. In the case of constructing the MapData for the
      // meta map (whose map is itself), this member has not yet been
      // initialized.
      map_(static_cast<MapData*>(broker->GetOrCreateData(object->map()))) {
  CHECK(broker->SerializingAllowed());
}

MapData::MapData(JSHeapBroker* broker, ObjectData** storage, Handle<Map> object)
    : HeapObjectData(broker, storage, object),
      instance_type_(object->instance_type()),
      instance_size_(object->instance_size()),
      bit_field_(object->bit_field()),
      bit_field2_(object->bit_field2()),
      bit_field3_(object->bit_field3()),
      can_be_deprecated_(object->NumberOfOwnDescriptors() > 0
                             ? object->CanBeDeprecated()
                             : false),
      can_transition_(object->CanTransition()),
      in_object_properties_start_in_words_(
          object->IsJSObjectMap() ? object->GetInObjectPropertiesStartInWords()
                                  : 0),
      in_object_properties_(
          object->IsJSObjectMap() ? object->GetInObjectProperties() : 0),
      elements_kind_generalizations_(broker->zone()) {}

JSFunctionData::JSFunctionData(JSHeapBroker* broker, ObjectData** storage,
                               Handle<JSFunction> object)
    : JSObjectData(broker, storage, object),
      has_initial_map_(object->has_prototype_slot() &&
                       object->has_initial_map()),
      has_prototype_(object->has_prototype_slot() && object->has_prototype()),
      PrototypeRequiresRuntimeLookup_(
          object->PrototypeRequiresRuntimeLookup()) {}

void JSFunctionData::Serialize(JSHeapBroker* broker) {
  if (serialized_) return;
  serialized_ = true;

  TraceScope tracer(broker, this, "JSFunctionData::Serialize");
  Handle<JSFunction> function = Handle<JSFunction>::cast(object());

  DCHECK_NULL(global_proxy_);
  DCHECK_NULL(initial_map_);
  DCHECK_NULL(prototype_);
  DCHECK_NULL(shared_);

  global_proxy_ =
      broker->GetOrCreateData(function->global_proxy())->AsJSGlobalProxy();
  shared_ = broker->GetOrCreateData(function->shared())->AsSharedFunctionInfo();
  initial_map_ = has_initial_map()
                     ? broker->GetOrCreateData(function->initial_map())->AsMap()
                     : nullptr;
  prototype_ = has_prototype() ? broker->GetOrCreateData(function->prototype())
                               : nullptr;

  if (initial_map_ != nullptr) {
    initial_map_instance_size_with_min_slack_ =
        function->ComputeInstanceSizeWithMinSlack(broker->isolate());
    if (initial_map_->instance_type() == JS_ARRAY_TYPE) {
      initial_map_->SerializeElementsKindGeneralizations(broker);
    }
    initial_map_->SerializeConstructorOrBackpointer(broker);
    // TODO(neis): This is currently only needed for native_context's
    // object_function, as used by GetObjectCreateMap. If no further use sites
    // show up, we should move this into NativeContextData::Serialize.
    initial_map_->SerializePrototype(broker);
  }
}

void MapData::SerializeElementsKindGeneralizations(JSHeapBroker* broker) {
  if (serialized_elements_kind_generalizations_) return;
  serialized_elements_kind_generalizations_ = true;

  TraceScope tracer(broker, this,
                    "MapData::SerializeElementsKindGeneralizations");
  DCHECK_EQ(instance_type(), JS_ARRAY_TYPE);
  MapRef self(broker, this);
  ElementsKind from_kind = self.elements_kind();
  DCHECK(elements_kind_generalizations_.empty());
  for (int i = FIRST_FAST_ELEMENTS_KIND; i <= LAST_FAST_ELEMENTS_KIND; i++) {
    ElementsKind to_kind = static_cast<ElementsKind>(i);
    if (IsMoreGeneralElementsKindTransition(from_kind, to_kind)) {
      Handle<Map> target =
          Map::AsElementsKind(broker->isolate(), self.object<Map>(), to_kind);
      elements_kind_generalizations_.push_back(
          broker->GetOrCreateData(target)->AsMap());
    }
  }
}

class DescriptorArrayData : public HeapObjectData {
 public:
  DescriptorArrayData(JSHeapBroker* broker, ObjectData** storage,
                      Handle<DescriptorArray> object)
      : HeapObjectData(broker, storage, object), contents_(broker->zone()) {}

  ZoneVector<PropertyDescriptor>& contents() { return contents_; }

 private:
  ZoneVector<PropertyDescriptor> contents_;
};

class FeedbackVectorData : public HeapObjectData {
 public:
  const ZoneVector<ObjectData*>& feedback() { return feedback_; }

  FeedbackVectorData(JSHeapBroker* broker, ObjectData** storage,
                     Handle<FeedbackVector> object);

  void SerializeSlots(JSHeapBroker* broker);

 private:
  bool serialized_ = false;
  ZoneVector<ObjectData*> feedback_;
};

FeedbackVectorData::FeedbackVectorData(JSHeapBroker* broker,
                                       ObjectData** storage,
                                       Handle<FeedbackVector> object)
    : HeapObjectData(broker, storage, object), feedback_(broker->zone()) {}

void FeedbackVectorData::SerializeSlots(JSHeapBroker* broker) {
  if (serialized_) return;
  serialized_ = true;

  TraceScope tracer(broker, this, "FeedbackVectorData::SerializeSlots");
  Handle<FeedbackVector> vector = Handle<FeedbackVector>::cast(object());
  DCHECK(feedback_.empty());
  feedback_.reserve(vector->length());
  for (int i = 0; i < vector->length(); ++i) {
    MaybeObject* value = vector->get(i);
    ObjectData* slot_value =
        value->IsObject() ? broker->GetOrCreateData(value->cast<Object>())
                          : nullptr;
    feedback_.push_back(slot_value);
    if (slot_value == nullptr) continue;

    if (slot_value->IsAllocationSite() &&
        slot_value->AsAllocationSite()->IsFastLiteral()) {
      slot_value->AsAllocationSite()->SerializeBoilerplate(broker);
    } else if (slot_value->IsJSRegExp()) {
      slot_value->AsJSRegExp()->SerializeAsRegExpBoilerplate(broker);
    }
  }
  DCHECK_EQ(vector->length(), feedback_.size());
  broker->Trace("Copied %zu slots.\n", feedback_.size());
}

class FixedArrayBaseData : public HeapObjectData {
 public:
  FixedArrayBaseData(JSHeapBroker* broker, ObjectData** storage,
                     Handle<FixedArrayBase> object)
      : HeapObjectData(broker, storage, object), length_(object->length()) {}

  int length() const { return length_; }

 private:
  int const length_;
};

JSObjectData::JSObjectData(JSHeapBroker* broker, ObjectData** storage,
                           Handle<JSObject> object)
    : HeapObjectData(broker, storage, object),
      inobject_fields_(broker->zone()) {}

class FixedArrayData : public FixedArrayBaseData {
 public:
  FixedArrayData(JSHeapBroker* broker, ObjectData** storage,
                 Handle<FixedArray> object);

  // Creates all elements of the fixed array.
  void SerializeContents(JSHeapBroker* broker);

  ObjectData* Get(int i) const;

 private:
  bool serialized_contents_ = false;
  ZoneVector<ObjectData*> contents_;
};

void FixedArrayData::SerializeContents(JSHeapBroker* broker) {
  if (serialized_contents_) return;
  serialized_contents_ = true;

  TraceScope tracer(broker, this, "FixedArrayData::SerializeContents");
  Handle<FixedArray> array = Handle<FixedArray>::cast(object());
  CHECK_EQ(array->length(), length());
  CHECK(contents_.empty());
  contents_.reserve(static_cast<size_t>(length()));

  for (int i = 0; i < length(); i++) {
    Handle<Object> value(array->get(i), broker->isolate());
    contents_.push_back(broker->GetOrCreateData(value));
  }
  broker->Trace("Copied %zu elements.\n", contents_.size());
}

FixedArrayData::FixedArrayData(JSHeapBroker* broker, ObjectData** storage,
                               Handle<FixedArray> object)
    : FixedArrayBaseData(broker, storage, object), contents_(broker->zone()) {}

class FixedDoubleArrayData : public FixedArrayBaseData {
 public:
  FixedDoubleArrayData(JSHeapBroker* broker, ObjectData** storage,
                       Handle<FixedDoubleArray> object);

  // Serializes all elements of the fixed array.
  void SerializeContents(JSHeapBroker* broker);

  Float64 Get(int i) const;

 private:
  bool serialized_contents_ = false;
  ZoneVector<Float64> contents_;
};

FixedDoubleArrayData::FixedDoubleArrayData(JSHeapBroker* broker,
                                           ObjectData** storage,
                                           Handle<FixedDoubleArray> object)
    : FixedArrayBaseData(broker, storage, object), contents_(broker->zone()) {}

void FixedDoubleArrayData::SerializeContents(JSHeapBroker* broker) {
  if (serialized_contents_) return;
  serialized_contents_ = true;

  TraceScope tracer(broker, this, "FixedDoubleArrayData::SerializeContents");
  Handle<FixedDoubleArray> self = Handle<FixedDoubleArray>::cast(object());
  CHECK_EQ(self->length(), length());
  CHECK(contents_.empty());
  contents_.reserve(static_cast<size_t>(length()));

  for (int i = 0; i < length(); i++) {
    contents_.push_back(Float64::FromBits(self->get_representation(i)));
  }
  broker->Trace("Copied %zu elements.\n", contents_.size());
}

class BytecodeArrayData : public FixedArrayBaseData {
 public:
  int register_count() const { return register_count_; }

  BytecodeArrayData(JSHeapBroker* broker, ObjectData** storage,
                    Handle<BytecodeArray> object)
      : FixedArrayBaseData(broker, storage, object),
        register_count_(object->register_count()) {}

 private:
  int const register_count_;
};

class JSArrayData : public JSObjectData {
 public:
  JSArrayData(JSHeapBroker* broker, ObjectData** storage,
              Handle<JSArray> object);
  void Serialize(JSHeapBroker* broker);

  ObjectData* length() const { return length_; }

 private:
  bool serialized_ = false;
  ObjectData* length_ = nullptr;
};

JSArrayData::JSArrayData(JSHeapBroker* broker, ObjectData** storage,
                         Handle<JSArray> object)
    : JSObjectData(broker, storage, object) {}

void JSArrayData::Serialize(JSHeapBroker* broker) {
  if (serialized_) return;
  serialized_ = true;

  TraceScope tracer(broker, this, "JSArrayData::Serialize");
  Handle<JSArray> jsarray = Handle<JSArray>::cast(object());
  DCHECK_NULL(length_);
  length_ = broker->GetOrCreateData(jsarray->length());
}

class ScopeInfoData : public HeapObjectData {
 public:
  ScopeInfoData(JSHeapBroker* broker, ObjectData** storage,
                Handle<ScopeInfo> object);

  int context_length() const { return context_length_; }

 private:
  int const context_length_;
};

ScopeInfoData::ScopeInfoData(JSHeapBroker* broker, ObjectData** storage,
                             Handle<ScopeInfo> object)
    : HeapObjectData(broker, storage, object),
      context_length_(object->ContextLength()) {}

class SharedFunctionInfoData : public HeapObjectData {
 public:
  int builtin_id() const { return builtin_id_; }
  BytecodeArrayData* GetBytecodeArray() const { return GetBytecodeArray_; }
#define DECL_ACCESSOR(type, name) \
  type name() const { return name##_; }
  BROKER_SFI_FIELDS(DECL_ACCESSOR)
#undef DECL_ACCESSOR

  SharedFunctionInfoData(JSHeapBroker* broker, ObjectData** storage,
                         Handle<SharedFunctionInfo> object)
      : HeapObjectData(broker, storage, object),
        builtin_id_(object->HasBuiltinId() ? object->builtin_id()
                                           : Builtins::kNoBuiltinId),
        GetBytecodeArray_(
            object->HasBytecodeArray()
                ? broker->GetOrCreateData(object->GetBytecodeArray())
                      ->AsBytecodeArray()
                : nullptr)
#define INIT_MEMBER(type, name) , name##_(object->name())
            BROKER_SFI_FIELDS(INIT_MEMBER)
#undef INIT_MEMBER
  {
    DCHECK_EQ(HasBuiltinId_, builtin_id_ != Builtins::kNoBuiltinId);
    DCHECK_EQ(HasBytecodeArray_, GetBytecodeArray_ != nullptr);
  }

 private:
  int const builtin_id_;
  BytecodeArrayData* const GetBytecodeArray_;
#define DECL_MEMBER(type, name) type const name##_;
  BROKER_SFI_FIELDS(DECL_MEMBER)
#undef DECL_MEMBER
};

class ModuleData : public HeapObjectData {
 public:
  ModuleData(JSHeapBroker* broker, ObjectData** storage, Handle<Module> object);
  void Serialize(JSHeapBroker* broker);

  CellData* GetCell(int cell_index) const;

 private:
  bool serialized_ = false;
  ZoneVector<CellData*> imports_;
  ZoneVector<CellData*> exports_;
};

ModuleData::ModuleData(JSHeapBroker* broker, ObjectData** storage,
                       Handle<Module> object)
    : HeapObjectData(broker, storage, object),
      imports_(broker->zone()),
      exports_(broker->zone()) {}

CellData* ModuleData::GetCell(int cell_index) const {
  CHECK(serialized_);
  CellData* cell;
  switch (ModuleDescriptor::GetCellIndexKind(cell_index)) {
    case ModuleDescriptor::kImport:
      cell = imports_.at(Module::ImportIndex(cell_index));
      break;
    case ModuleDescriptor::kExport:
      cell = exports_.at(Module::ExportIndex(cell_index));
      break;
    case ModuleDescriptor::kInvalid:
      UNREACHABLE();
      break;
  }
  CHECK_NOT_NULL(cell);
  return cell;
}

void ModuleData::Serialize(JSHeapBroker* broker) {
  if (serialized_) return;
  serialized_ = true;

  TraceScope tracer(broker, this, "ModuleData::Serialize");
  Handle<Module> module = Handle<Module>::cast(object());

  // TODO(neis): We could be smarter and only serialize the cells we care about.
  // TODO(neis): Define a helper for serializing a FixedArray into a ZoneVector.

  DCHECK(imports_.empty());
  Handle<FixedArray> imports(module->regular_imports(), broker->isolate());
  int const imports_length = imports->length();
  imports_.reserve(imports_length);
  for (int i = 0; i < imports_length; ++i) {
    imports_.push_back(broker->GetOrCreateData(imports->get(i))->AsCell());
  }
  broker->Trace("Copied %zu imports.\n", imports_.size());

  DCHECK(exports_.empty());
  Handle<FixedArray> exports(module->regular_exports(), broker->isolate());
  int const exports_length = exports->length();
  exports_.reserve(exports_length);
  for (int i = 0; i < exports_length; ++i) {
    exports_.push_back(broker->GetOrCreateData(exports->get(i))->AsCell());
  }
  broker->Trace("Copied %zu exports.\n", exports_.size());
}

class CellData : public HeapObjectData {
 public:
  CellData(JSHeapBroker* broker, ObjectData** storage, Handle<Cell> object)
      : HeapObjectData(broker, storage, object) {}
};

class JSGlobalProxyData : public JSObjectData {
 public:
  JSGlobalProxyData(JSHeapBroker* broker, ObjectData** storage,
                    Handle<JSGlobalProxy> object)
      : JSObjectData(broker, storage, object) {}
};

class CodeData : public HeapObjectData {
 public:
  CodeData(JSHeapBroker* broker, ObjectData** storage, Handle<Code> object)
      : HeapObjectData(broker, storage, object) {}
};

#define DEFINE_IS_AND_AS(Name)                                            \
  bool ObjectData::Is##Name() const {                                     \
    if (kind() == kUnserializedHeapObject) {                              \
      AllowHandleDereference allow_handle_dereference;                    \
      return object()->Is##Name();                                        \
    }                                                                     \
    if (is_smi()) return false;                                           \
    InstanceType instance_type =                                          \
        static_cast<const HeapObjectData*>(this)->map()->instance_type(); \
    return InstanceTypeChecker::Is##Name(instance_type);                  \
  }                                                                       \
  Name##Data* ObjectData::As##Name() {                                    \
    CHECK_EQ(kind(), kSerializedHeapObject);                              \
    CHECK(Is##Name());                                                    \
    return static_cast<Name##Data*>(this);                                \
  }
HEAP_BROKER_OBJECT_LIST(DEFINE_IS_AND_AS)
#undef DEFINE_IS_AND_AS

const JSObjectField& JSObjectData::GetInobjectField(int property_index) const {
  CHECK_LT(static_cast<size_t>(property_index), inobject_fields_.size());
  return inobject_fields_[property_index];
}

bool JSObjectData::cow_or_empty_elements_tenured() const {
  return cow_or_empty_elements_tenured_;
}

FixedArrayBaseData* JSObjectData::elements() const { return elements_; }

void JSObjectData::SerializeAsBoilerplate(JSHeapBroker* broker) {
  SerializeRecursive(broker, kMaxFastLiteralDepth);
}

void JSObjectData::SerializeElements(JSHeapBroker* broker) {
  if (serialized_elements_) return;
  serialized_elements_ = true;

  TraceScope tracer(broker, this, "JSObjectData::SerializeElements");
  Handle<JSObject> boilerplate = Handle<JSObject>::cast(object());
  Handle<FixedArrayBase> elements_object(boilerplate->elements(),
                                         broker->isolate());
  DCHECK_NULL(elements_);
  elements_ = broker->GetOrCreateData(elements_object)->AsFixedArrayBase();
}

void MapData::SerializeConstructorOrBackpointer(JSHeapBroker* broker) {
  if (serialized_constructor_or_backpointer_) return;
  serialized_constructor_or_backpointer_ = true;

  TraceScope tracer(broker, this, "MapData::SerializeConstructorOrBackpointer");
  Handle<Map> map = Handle<Map>::cast(object());
  DCHECK_NULL(constructor_or_backpointer_);
  constructor_or_backpointer_ =
      broker->GetOrCreateData(map->constructor_or_backpointer());
}

void MapData::SerializePrototype(JSHeapBroker* broker) {
  if (serialized_prototype_) return;
  serialized_prototype_ = true;

  TraceScope tracer(broker, this, "MapData::SerializePrototype");
  Handle<Map> map = Handle<Map>::cast(object());
  DCHECK_NULL(prototype_);
  prototype_ = broker->GetOrCreateData(map->prototype());
}

void MapData::SerializeOwnDescriptors(JSHeapBroker* broker) {
  if (serialized_own_descriptors_) return;
  serialized_own_descriptors_ = true;

  TraceScope tracer(broker, this, "MapData::SerializeOwnDescriptors");
  Handle<Map> map = Handle<Map>::cast(object());

  DCHECK_NULL(instance_descriptors_);
  instance_descriptors_ =
      broker->GetOrCreateData(map->instance_descriptors())->AsDescriptorArray();

  int const number_of_own = map->NumberOfOwnDescriptors();
  ZoneVector<PropertyDescriptor>& contents = instance_descriptors_->contents();
  int const current_size = static_cast<int>(contents.size());
  if (number_of_own <= current_size) return;

  Isolate* const isolate = broker->isolate();
  auto descriptors =
      Handle<DescriptorArray>::cast(instance_descriptors_->object());
  CHECK_EQ(*descriptors, map->instance_descriptors());
  contents.reserve(number_of_own);

  // Copy the new descriptors.
  for (int i = current_size; i < number_of_own; ++i) {
    PropertyDescriptor d;
    d.key = broker->GetOrCreateData(descriptors->GetKey(i))->AsName();
    d.details = descriptors->GetDetails(i);
    if (d.details.location() == kField) {
      d.field_index = FieldIndex::ForDescriptor(*map, i);
      d.field_owner =
          broker->GetOrCreateData(map->FindFieldOwner(isolate, i))->AsMap();
      d.field_type = broker->GetOrCreateData(descriptors->GetFieldType(i));
      d.is_unboxed_double_field = map->IsUnboxedDoubleField(d.field_index);
      // Recurse.
    }
    contents.push_back(d);
  }
  CHECK_EQ(number_of_own, contents.size());

  // Recurse on the new owner maps.
  for (int i = current_size; i < number_of_own; ++i) {
    const PropertyDescriptor& d = contents[i];
    if (d.details.location() == kField) {
      CHECK_LE(
          Handle<Map>::cast(d.field_owner->object())->NumberOfOwnDescriptors(),
          number_of_own);
      d.field_owner->SerializeOwnDescriptors(broker);
    }
  }

  broker->Trace("Copied %zu descriptors into %p (%zu total).\n",
                number_of_own - current_size, instance_descriptors_,
                number_of_own);
}

void JSObjectData::SerializeRecursive(JSHeapBroker* broker, int depth) {
  if (serialized_as_boilerplate_) return;
  serialized_as_boilerplate_ = true;

  TraceScope tracer(broker, this, "JSObjectData::SerializeRecursive");
  Handle<JSObject> boilerplate = Handle<JSObject>::cast(object());

  // We only serialize boilerplates that pass the IsInlinableFastLiteral
  // check, so we only do a sanity check on the depth here.
  CHECK_GT(depth, 0);
  CHECK(!boilerplate->map()->is_deprecated());

  // Serialize the elements.
  Isolate* const isolate = broker->isolate();
  Handle<FixedArrayBase> elements_object(boilerplate->elements(), isolate);

  // Boilerplates need special serialization - we need to make sure COW arrays
  // are tenured. Boilerplate objects should only be reachable from their
  // allocation site, so it is safe to assume that the elements have not been
  // serialized yet.

  bool const empty_or_cow =
      elements_object->length() == 0 ||
      elements_object->map() == ReadOnlyRoots(isolate).fixed_cow_array_map();
  if (empty_or_cow) {
    // We need to make sure copy-on-write elements are tenured.
    if (Heap::InNewSpace(*elements_object)) {
      elements_object = isolate->factory()->CopyAndTenureFixedCOWArray(
          Handle<FixedArray>::cast(elements_object));
      boilerplate->set_elements(*elements_object);
    }
    cow_or_empty_elements_tenured_ = true;
  }

  DCHECK_NULL(elements_);
  elements_ = broker->GetOrCreateData(elements_object)->AsFixedArrayBase();

  if (empty_or_cow) {
    // No need to do anything here. Empty or copy-on-write elements
    // do not need to be serialized because we only need to store the elements
    // reference to the allocated object.
  } else if (boilerplate->HasSmiOrObjectElements()) {
    elements_->AsFixedArray()->SerializeContents(broker);
    Handle<FixedArray> fast_elements =
        Handle<FixedArray>::cast(elements_object);
    int length = elements_object->length();
    for (int i = 0; i < length; i++) {
      Handle<Object> value(fast_elements->get(i), isolate);
      if (value->IsJSObject()) {
        ObjectData* value_data = broker->GetOrCreateData(value);
        value_data->AsJSObject()->SerializeRecursive(broker, depth - 1);
      }
    }
  } else {
    CHECK(boilerplate->HasDoubleElements());
    CHECK_LE(elements_object->Size(), kMaxRegularHeapObjectSize);
    elements_->AsFixedDoubleArray()->SerializeContents(broker);
  }

  // TODO(turbofan): Do we want to support out-of-object properties?
  CHECK(boilerplate->HasFastProperties() &&
        boilerplate->property_array()->length() == 0);
  CHECK_EQ(inobject_fields_.size(), 0u);

  // Check the in-object properties.
  Handle<DescriptorArray> descriptors(
      boilerplate->map()->instance_descriptors(), isolate);
  int const limit = boilerplate->map()->NumberOfOwnDescriptors();
  for (int i = 0; i < limit; i++) {
    PropertyDetails details = descriptors->GetDetails(i);
    if (details.location() != kField) continue;
    DCHECK_EQ(kData, details.kind());

    FieldIndex field_index = FieldIndex::ForDescriptor(boilerplate->map(), i);
    // Make sure {field_index} agrees with {inobject_properties} on the index of
    // this field.
    DCHECK_EQ(field_index.property_index(),
              static_cast<int>(inobject_fields_.size()));
    if (boilerplate->IsUnboxedDoubleField(field_index)) {
      double value = boilerplate->RawFastDoublePropertyAt(field_index);
      inobject_fields_.push_back(JSObjectField{value});
    } else {
      Handle<Object> value(boilerplate->RawFastPropertyAt(field_index),
                           isolate);
      ObjectData* value_data = broker->GetOrCreateData(value);
      if (value->IsJSObject()) {
        value_data->AsJSObject()->SerializeRecursive(broker, depth - 1);
      }
      inobject_fields_.push_back(JSObjectField{value_data});
    }
  }
  broker->Trace("Copied %zu in-object fields.\n", inobject_fields_.size());

  map()->SerializeOwnDescriptors(broker);

  if (IsJSArray()) AsJSArray()->Serialize(broker);
}

void JSRegExpData::SerializeAsRegExpBoilerplate(JSHeapBroker* broker) {
  if (serialized_as_reg_exp_boilerplate_) return;
  serialized_as_reg_exp_boilerplate_ = true;

  TraceScope tracer(broker, this, "JSRegExpData::SerializeAsRegExpBoilerplate");
  Handle<JSRegExp> boilerplate = Handle<JSRegExp>::cast(object());

  SerializeElements(broker);

  raw_properties_or_hash_ =
      broker->GetOrCreateData(boilerplate->raw_properties_or_hash());
  data_ = broker->GetOrCreateData(boilerplate->data());
  source_ = broker->GetOrCreateData(boilerplate->source());
  flags_ = broker->GetOrCreateData(boilerplate->flags());
  last_index_ = broker->GetOrCreateData(boilerplate->last_index());
}

bool ObjectRef::equals(const ObjectRef& other) const {
  return data_ == other.data_;
}

Isolate* ObjectRef::isolate() const { return broker()->isolate(); }

ContextRef ContextRef::previous() const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleAllocation handle_allocation;
    AllowHandleDereference handle_dereference;
    return ContextRef(
        broker(), handle(object<Context>()->previous(), broker()->isolate()));
  }
  return ContextRef(broker(), data()->AsContext()->previous());
}

// Not needed for TypedLowering.
ObjectRef ContextRef::get(int index) const {
  AllowHandleAllocation handle_allocation;
  AllowHandleDereference handle_dereference;
  Handle<Object> value(object<Context>()->get(index), broker()->isolate());
  return ObjectRef(broker(), value);
}

JSHeapBroker::JSHeapBroker(Isolate* isolate, Zone* broker_zone)
    : isolate_(isolate),
      broker_zone_(broker_zone),
      current_zone_(broker_zone),
      refs_(new (zone())
                RefsMap(kMinimalRefsBucketCount, AddressMatcher(), zone())) {
  // Note that this initialization of the refs_ pointer with the minimal
  // initial capacity is redundant in the normal use case (concurrent
  // compilation enabled, standard objects to be serialized), as the map
  // is going to be replaced immediatelly with a larger capacity one.
  // It doesn't seem to affect the performance in a noticeable way though.
  Trace("Constructing heap broker.\n");
}

void JSHeapBroker::Trace(const char* format, ...) const {
  if (FLAG_trace_heap_broker) {
    PrintF("[%p] ", this);
    for (unsigned i = 0; i < tracing_indentation_; ++i) PrintF("  ");
    va_list arguments;
    va_start(arguments, format);
    base::OS::VPrint(format, arguments);
    va_end(arguments);
  }
}

void JSHeapBroker::StartSerializing() {
  CHECK_EQ(mode_, kDisabled);
  Trace("Starting serialization.\n");
  mode_ = kSerializing;
  refs_->Clear();
}

void JSHeapBroker::StopSerializing() {
  CHECK_EQ(mode_, kSerializing);
  Trace("Stopping serialization.\n");
  mode_ = kSerialized;
}

void JSHeapBroker::Retire() {
  CHECK_EQ(mode_, kSerialized);
  Trace("Retiring.\n");
  mode_ = kRetired;
}

bool JSHeapBroker::SerializingAllowed() const {
  return mode() == kSerializing ||
         (!FLAG_strict_heap_broker && mode() == kSerialized);
}

void JSHeapBroker::SetNativeContextRef() {
  native_context_ = NativeContextRef(this, isolate()->native_context());
}

bool IsShareable(Handle<Object> object, Isolate* isolate) {
  Builtins* const b = isolate->builtins();

  int index;
  RootIndex root_index;
  return (object->IsHeapObject() &&
          b->IsBuiltinHandle(Handle<HeapObject>::cast(object), &index)) ||
         isolate->heap()->IsRootHandle(object, &root_index);
}

void JSHeapBroker::SerializeShareableObjects() {
  PerIsolateCompilerCache::Setup(isolate());
  compiler_cache_ = isolate()->compiler_cache();

  if (compiler_cache_->HasSnapshot()) {
    RefsMap* snapshot = compiler_cache_->GetSnapshot();

    refs_ = new (zone()) RefsMap(snapshot, zone());
    return;
  }

  TraceScope tracer(
      this, "JSHeapBroker::SerializeShareableObjects (building snapshot)");

  refs_ =
      new (zone()) RefsMap(kInitialRefsBucketCount, AddressMatcher(), zone());

  current_zone_ = compiler_cache_->zone();

  Builtins* const b = isolate()->builtins();
  {
    Builtins::Name builtins[] = {
        Builtins::kAllocateInNewSpace,
        Builtins::kAllocateInOldSpace,
        Builtins::kArgumentsAdaptorTrampoline,
        Builtins::kArrayConstructorImpl,
        Builtins::kCallFunctionForwardVarargs,
        Builtins::kCallFunction_ReceiverIsAny,
        Builtins::kCallFunction_ReceiverIsNotNullOrUndefined,
        Builtins::kCallFunction_ReceiverIsNullOrUndefined,
        Builtins::kConstructFunctionForwardVarargs,
        Builtins::kForInFilter,
        Builtins::kJSBuiltinsConstructStub,
        Builtins::kJSConstructStubGeneric,
        Builtins::kStringAdd_CheckNone,
        Builtins::kStringAdd_ConvertLeft,
        Builtins::kStringAdd_ConvertRight,
        Builtins::kToNumber,
        Builtins::kToObject,
    };
    for (auto id : builtins) {
      GetOrCreateData(b->builtin_handle(id));
    }
  }
  for (int32_t id = 0; id < Builtins::builtin_count; ++id) {
    if (Builtins::KindOf(id) == Builtins::TFJ) {
      GetOrCreateData(b->builtin_handle(id));
    }
  }

  for (RefsMap::Entry* p = refs_->Start(); p != nullptr; p = refs_->Next(p)) {
    CHECK(IsShareable(p->value->object(), isolate()));
  }

  // TODO(mslekova):
  // Serialize root objects (from factory).
  compiler_cache()->SetSnapshot(refs_);
  current_zone_ = broker_zone_;
}

void JSHeapBroker::SerializeStandardObjects() {
  if (mode() == kDisabled) return;
  CHECK_EQ(mode(), kSerializing);

  SerializeShareableObjects();

  TraceScope tracer(this, "JSHeapBroker::SerializeStandardObjects");

  SetNativeContextRef();
  native_context().Serialize();

  Factory* const f = isolate()->factory();

  // Maps, strings, oddballs
  GetOrCreateData(f->arguments_marker_map());
  GetOrCreateData(f->bigint_string());
  GetOrCreateData(f->block_context_map());
  GetOrCreateData(f->boolean_map());
  GetOrCreateData(f->boolean_string());
  GetOrCreateData(f->catch_context_map());
  GetOrCreateData(f->empty_fixed_array());
  GetOrCreateData(f->empty_string());
  GetOrCreateData(f->eval_context_map());
  GetOrCreateData(f->false_string());
  GetOrCreateData(f->false_value());
  GetOrCreateData(f->fixed_array_map());
  GetOrCreateData(f->fixed_cow_array_map());
  GetOrCreateData(f->fixed_double_array_map());
  GetOrCreateData(f->function_context_map());
  GetOrCreateData(f->function_string());
  GetOrCreateData(f->heap_number_map());
  GetOrCreateData(f->length_string());
  GetOrCreateData(f->many_closures_cell_map());
  GetOrCreateData(f->minus_zero_value());
  GetOrCreateData(f->mutable_heap_number_map());
  GetOrCreateData(f->name_dictionary_map());
  GetOrCreateData(f->NaN_string());
  GetOrCreateData(f->null_map());
  GetOrCreateData(f->null_string());
  GetOrCreateData(f->null_value());
  GetOrCreateData(f->number_string());
  GetOrCreateData(f->object_string());
  GetOrCreateData(f->one_pointer_filler_map());
  GetOrCreateData(f->optimized_out());
  GetOrCreateData(f->optimized_out_map());
  GetOrCreateData(f->property_array_map());
  GetOrCreateData(f->sloppy_arguments_elements_map());
  GetOrCreateData(f->stale_register());
  GetOrCreateData(f->stale_register_map());
  GetOrCreateData(f->string_string());
  GetOrCreateData(f->symbol_string());
  GetOrCreateData(f->termination_exception_map());
  GetOrCreateData(f->the_hole_map());
  GetOrCreateData(f->the_hole_value());
  GetOrCreateData(f->true_string());
  GetOrCreateData(f->true_value());
  GetOrCreateData(f->undefined_map());
  GetOrCreateData(f->undefined_string());
  GetOrCreateData(f->undefined_value());
  GetOrCreateData(f->uninitialized_map());
  GetOrCreateData(f->with_context_map());
  GetOrCreateData(f->zero_string());

  // Property cells
  GetOrCreateData(f->array_buffer_neutering_protector())
      ->AsPropertyCell()
      ->Serialize(this);
  GetOrCreateData(f->array_iterator_protector())
      ->AsPropertyCell()
      ->Serialize(this);
  GetOrCreateData(f->array_species_protector())
      ->AsPropertyCell()
      ->Serialize(this);
  GetOrCreateData(f->no_elements_protector())
      ->AsPropertyCell()
      ->Serialize(this);
  GetOrCreateData(f->promise_hook_protector())
      ->AsPropertyCell()
      ->Serialize(this);
  GetOrCreateData(f->promise_species_protector())
      ->AsPropertyCell()
      ->Serialize(this);
  GetOrCreateData(f->promise_then_protector())
      ->AsPropertyCell()
      ->Serialize(this);

  // CEntry stub
  GetOrCreateData(
      CodeFactory::CEntry(isolate(), 1, kDontSaveFPRegs, kArgvOnStack, true));

  Trace("Finished serializing standard objects.\n");
}

ObjectData* JSHeapBroker::GetData(Handle<Object> object) const {
  RefsMap::Entry* entry = refs_->Lookup(object.address());
  return entry ? entry->value : nullptr;
}

// clang-format off
ObjectData* JSHeapBroker::GetOrCreateData(Handle<Object> object) {
  CHECK(SerializingAllowed());
  RefsMap::Entry* entry = refs_->LookupOrInsert(object.address(), zone());
  ObjectData** data_storage = &(entry->value);
  if (*data_storage == nullptr) {
    // TODO(neis): Remove these Allow* once we serialize everything upfront.
    AllowHandleAllocation handle_allocation;
    AllowHandleDereference handle_dereference;
    if (object->IsSmi()) {
      new (zone()) ObjectData(this, data_storage, object, kSmi);
#define CREATE_DATA_IF_MATCH(name)                                             \
    } else if (object->Is##name()) {                                           \
      new (zone()) name##Data(this, data_storage, Handle<name>::cast(object));
    HEAP_BROKER_OBJECT_LIST(CREATE_DATA_IF_MATCH)
#undef CREATE_DATA_IF_MATCH
    } else {
      UNREACHABLE();
    }
  }
  CHECK_NOT_NULL(*data_storage);
  return (*data_storage);
}
// clang-format on

ObjectData* JSHeapBroker::GetOrCreateData(Object* object) {
  return GetOrCreateData(handle(object, isolate()));
}

#define DEFINE_IS_AND_AS(Name)                                    \
  bool ObjectRef::Is##Name() const { return data()->Is##Name(); } \
  Name##Ref ObjectRef::As##Name() const {                         \
    DCHECK(Is##Name());                                           \
    return Name##Ref(broker(), data());                           \
  }
HEAP_BROKER_OBJECT_LIST(DEFINE_IS_AND_AS)
#undef DEFINE_IS_AND_AS

bool ObjectRef::IsSmi() const { return data()->is_smi(); }

int ObjectRef::AsSmi() const {
  DCHECK(IsSmi());
  // Handle-dereference is always allowed for Handle<Smi>.
  return object<Smi>()->value();
}

base::Optional<MapRef> JSObjectRef::GetObjectCreateMap() const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleAllocation handle_allocation;
    AllowHandleDereference allow_handle_dereference;
    AllowHeapAllocation heap_allocation;
    Handle<Map> instance_map;
    if (Map::TryGetObjectCreateMap(broker()->isolate(), object<HeapObject>())
            .ToHandle(&instance_map)) {
      return MapRef(broker(), instance_map);
    } else {
      return base::Optional<MapRef>();
    }
  }
  MapData* map_data = data()->AsJSObject()->object_create_map();
  return map_data != nullptr ? MapRef(broker(), map_data)
                             : base::Optional<MapRef>();
}

base::Optional<MapRef> MapRef::AsElementsKind(ElementsKind kind) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleAllocation handle_allocation;
    AllowHeapAllocation heap_allocation;
    AllowHandleDereference allow_handle_dereference;
    return MapRef(broker(), Map::AsElementsKind(broker()->isolate(),
                                                object<Map>(), kind));
  }
  if (kind == elements_kind()) return *this;
  const ZoneVector<MapData*>& elements_kind_generalizations =
      data()->AsMap()->elements_kind_generalizations();
  for (auto data : elements_kind_generalizations) {
    MapRef map(broker(), data);
    if (map.elements_kind() == kind) return map;
  }
  return base::Optional<MapRef>();
}

int JSFunctionRef::InitialMapInstanceSizeWithMinSlack() const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference allow_handle_dereference;
    AllowHandleAllocation handle_allocation;
    return object<JSFunction>()->ComputeInstanceSizeWithMinSlack(
        broker()->isolate());
  }
  return data()->AsJSFunction()->initial_map_instance_size_with_min_slack();
}

// Not needed for TypedLowering.
base::Optional<ScriptContextTableRef::LookupResult>
ScriptContextTableRef::lookup(const NameRef& name) const {
  AllowHandleAllocation handle_allocation;
  AllowHandleDereference handle_dereference;
  if (!name.IsString()) return {};
  ScriptContextTable::LookupResult lookup_result;
  auto table = object<ScriptContextTable>();
  if (!ScriptContextTable::Lookup(broker()->isolate(), table,
                                  name.object<String>(), &lookup_result)) {
    return {};
  }
  Handle<Context> script_context = ScriptContextTable::GetContext(
      broker()->isolate(), table, lookup_result.context_index);
  LookupResult result{ContextRef(broker(), script_context),
                      lookup_result.mode == VariableMode::kConst,
                      lookup_result.slot_index};
  return result;
}

OddballType MapRef::oddball_type() const {
  if (instance_type() != ODDBALL_TYPE) {
    return OddballType::kNone;
  }
  Factory* f = broker()->isolate()->factory();
  if (equals(MapRef(broker(), f->undefined_map()))) {
    return OddballType::kUndefined;
  }
  if (equals(MapRef(broker(), f->null_map()))) {
    return OddballType::kNull;
  }
  if (equals(MapRef(broker(), f->boolean_map()))) {
    return OddballType::kBoolean;
  }
  if (equals(MapRef(broker(), f->the_hole_map()))) {
    return OddballType::kHole;
  }
  if (equals(MapRef(broker(), f->uninitialized_map()))) {
    return OddballType::kUninitialized;
  }
  DCHECK(equals(MapRef(broker(), f->termination_exception_map())) ||
         equals(MapRef(broker(), f->arguments_marker_map())) ||
         equals(MapRef(broker(), f->optimized_out_map())) ||
         equals(MapRef(broker(), f->stale_register_map())));
  return OddballType::kOther;
}

ObjectRef FeedbackVectorRef::get(FeedbackSlot slot) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleAllocation handle_allocation;
    AllowHandleDereference handle_dereference;
    Handle<Object> value(object<FeedbackVector>()->Get(slot)->cast<Object>(),
                         broker()->isolate());
    return ObjectRef(broker(), value);
  }
  int i = FeedbackVector::GetIndex(slot);
  return ObjectRef(broker(), data()->AsFeedbackVector()->feedback().at(i));
}

double JSObjectRef::RawFastDoublePropertyAt(FieldIndex index) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference handle_dereference;
    return object<JSObject>()->RawFastDoublePropertyAt(index);
  }
  JSObjectData* object_data = data()->AsJSObject();
  CHECK(index.is_inobject());
  return object_data->GetInobjectField(index.property_index()).AsDouble();
}

ObjectRef JSObjectRef::RawFastPropertyAt(FieldIndex index) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleAllocation handle_allocation;
    AllowHandleDereference handle_dereference;
    return ObjectRef(broker(),
                     handle(object<JSObject>()->RawFastPropertyAt(index),
                            broker()->isolate()));
  }
  JSObjectData* object_data = data()->AsJSObject();
  CHECK(index.is_inobject());
  return ObjectRef(
      broker(),
      object_data->GetInobjectField(index.property_index()).AsObject());
}

bool AllocationSiteRef::IsFastLiteral() const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHeapAllocation allow_heap_allocation;  // For TryMigrateInstance.
    AllowHandleAllocation allow_handle_allocation;
    AllowHandleDereference allow_handle_dereference;
    return IsInlinableFastLiteral(
        handle(object<AllocationSite>()->boilerplate(), broker()->isolate()));
  }
  return data()->AsAllocationSite()->IsFastLiteral();
}

void JSObjectRef::EnsureElementsTenured() {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleAllocation allow_handle_allocation;
    AllowHandleDereference allow_handle_dereference;
    AllowHeapAllocation allow_heap_allocation;

    Handle<FixedArrayBase> object_elements =
        elements().object<FixedArrayBase>();
    if (Heap::InNewSpace(*object_elements)) {
      // If we would like to pretenure a fixed cow array, we must ensure that
      // the array is already in old space, otherwise we'll create too many
      // old-to-new-space pointers (overflowing the store buffer).
      object_elements =
          broker()->isolate()->factory()->CopyAndTenureFixedCOWArray(
              Handle<FixedArray>::cast(object_elements));
      object<JSObject>()->set_elements(*object_elements);
    }
    return;
  }
  CHECK(data()->AsJSObject()->cow_or_empty_elements_tenured());
}

FieldIndex MapRef::GetFieldIndexFor(int descriptor_index) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference allow_handle_dereference;
    return FieldIndex::ForDescriptor(*object<Map>(), descriptor_index);
  }
  DescriptorArrayData* descriptors = data()->AsMap()->instance_descriptors();
  return descriptors->contents().at(descriptor_index).field_index;
}

int MapRef::GetInObjectPropertyOffset(int i) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference allow_handle_dereference;
    return object<Map>()->GetInObjectPropertyOffset(i);
  }
  return (GetInObjectPropertiesStartInWords() + i) * kPointerSize;
}

PropertyDetails MapRef::GetPropertyDetails(int descriptor_index) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference allow_handle_dereference;
    return object<Map>()->instance_descriptors()->GetDetails(descriptor_index);
  }
  DescriptorArrayData* descriptors = data()->AsMap()->instance_descriptors();
  return descriptors->contents().at(descriptor_index).details;
}

NameRef MapRef::GetPropertyKey(int descriptor_index) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleAllocation handle_allocation;
    AllowHandleDereference allow_handle_dereference;
    return NameRef(
        broker(),
        handle(object<Map>()->instance_descriptors()->GetKey(descriptor_index),
               broker()->isolate()));
  }
  DescriptorArrayData* descriptors = data()->AsMap()->instance_descriptors();
  return NameRef(broker(), descriptors->contents().at(descriptor_index).key);
}

bool MapRef::IsFixedCowArrayMap() const {
  Handle<Map> fixed_cow_array_map =
      ReadOnlyRoots(broker()->isolate()).fixed_cow_array_map_handle();
  return equals(MapRef(broker(), fixed_cow_array_map));
}

MapRef MapRef::FindFieldOwner(int descriptor_index) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleAllocation handle_allocation;
    AllowHandleDereference allow_handle_dereference;
    Handle<Map> owner(
        object<Map>()->FindFieldOwner(broker()->isolate(), descriptor_index),
        broker()->isolate());
    return MapRef(broker(), owner);
  }
  DescriptorArrayData* descriptors = data()->AsMap()->instance_descriptors();
  return MapRef(broker(),
                descriptors->contents().at(descriptor_index).field_owner);
}

ObjectRef MapRef::GetFieldType(int descriptor_index) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleAllocation handle_allocation;
    AllowHandleDereference allow_handle_dereference;
    Handle<FieldType> field_type(
        object<Map>()->instance_descriptors()->GetFieldType(descriptor_index),
        broker()->isolate());
    return ObjectRef(broker(), field_type);
  }
  DescriptorArrayData* descriptors = data()->AsMap()->instance_descriptors();
  return ObjectRef(broker(),
                   descriptors->contents().at(descriptor_index).field_type);
}

bool MapRef::IsUnboxedDoubleField(int descriptor_index) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference allow_handle_dereference;
    return object<Map>()->IsUnboxedDoubleField(
        FieldIndex::ForDescriptor(*object<Map>(), descriptor_index));
  }
  DescriptorArrayData* descriptors = data()->AsMap()->instance_descriptors();
  return descriptors->contents().at(descriptor_index).is_unboxed_double_field;
}

uint16_t StringRef::GetFirstChar() {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference allow_handle_dereference;
    return object<String>()->Get(0);
  }
  return data()->AsString()->first_char();
}

base::Optional<double> StringRef::ToNumber() {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference allow_handle_dereference;
    AllowHandleAllocation allow_handle_allocation;
    AllowHeapAllocation allow_heap_allocation;
    int flags = ALLOW_HEX | ALLOW_OCTAL | ALLOW_BINARY;
    return StringToDouble(broker()->isolate(),
                          broker()->isolate()->unicode_cache(),
                          object<String>(), flags);
  }
  return data()->AsString()->to_number();
}

ObjectRef FixedArrayRef::get(int i) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleAllocation handle_allocation;
    AllowHandleDereference allow_handle_dereference;
    return ObjectRef(broker(),
                     handle(object<FixedArray>()->get(i), broker()->isolate()));
  }
  return ObjectRef(broker(), data()->AsFixedArray()->Get(i));
}

bool FixedDoubleArrayRef::is_the_hole(int i) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference allow_handle_dereference;
    return object<FixedDoubleArray>()->is_the_hole(i);
  }
  return data()->AsFixedDoubleArray()->Get(i).is_hole_nan();
}

double FixedDoubleArrayRef::get_scalar(int i) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference allow_handle_dereference;
    return object<FixedDoubleArray>()->get_scalar(i);
  }
  CHECK(!data()->AsFixedDoubleArray()->Get(i).is_hole_nan());
  return data()->AsFixedDoubleArray()->Get(i).get_scalar();
}

#define IF_BROKER_DISABLED_ACCESS_HANDLE_C(holder, name) \
  if (broker()->mode() == JSHeapBroker::kDisabled) {     \
    AllowHandleAllocation handle_allocation;             \
    AllowHandleDereference allow_handle_dereference;     \
    return object<holder>()->name();                     \
  }

#define IF_BROKER_DISABLED_ACCESS_HANDLE(holder, result, name)                 \
  if (broker()->mode() == JSHeapBroker::kDisabled) {                           \
    AllowHandleAllocation handle_allocation;                                   \
    AllowHandleDereference allow_handle_dereference;                           \
    return result##Ref(broker(),                                               \
                       handle(object<holder>()->name(), broker()->isolate())); \
  }

// Macros for definining a const getter that, depending on the broker mode,
// either looks into the handle or into the serialized data.
#define BIMODAL_ACCESSOR(holder, result, name)                             \
  result##Ref holder##Ref::name() const {                                  \
    IF_BROKER_DISABLED_ACCESS_HANDLE(holder, result, name);                \
    return result##Ref(broker(), ObjectRef::data()->As##holder()->name()); \
  }

// Like above except that the result type is not an XYZRef.
#define BIMODAL_ACCESSOR_C(holder, result, name)      \
  result holder##Ref::name() const {                  \
    IF_BROKER_DISABLED_ACCESS_HANDLE_C(holder, name); \
    return ObjectRef::data()->As##holder()->name();   \
  }

// Like above but for BitFields.
#define BIMODAL_ACCESSOR_B(holder, field, name, BitField)              \
  typename BitField::FieldType holder##Ref::name() const {             \
    IF_BROKER_DISABLED_ACCESS_HANDLE_C(holder, name);                  \
    return BitField::decode(ObjectRef::data()->As##holder()->field()); \
  }

BIMODAL_ACCESSOR(AllocationSite, Object, nested_site)
BIMODAL_ACCESSOR_C(AllocationSite, bool, CanInlineCall)
BIMODAL_ACCESSOR_C(AllocationSite, bool, PointsToLiteral)
BIMODAL_ACCESSOR_C(AllocationSite, ElementsKind, GetElementsKind)
BIMODAL_ACCESSOR_C(AllocationSite, PretenureFlag, GetPretenureMode)

BIMODAL_ACCESSOR_C(BytecodeArray, int, register_count)

BIMODAL_ACCESSOR(HeapObject, Map, map)

BIMODAL_ACCESSOR(JSArray, Object, length)

BIMODAL_ACCESSOR_C(JSFunction, bool, has_prototype)
BIMODAL_ACCESSOR_C(JSFunction, bool, has_initial_map)
BIMODAL_ACCESSOR_C(JSFunction, bool, PrototypeRequiresRuntimeLookup)
BIMODAL_ACCESSOR(JSFunction, JSGlobalProxy, global_proxy)
BIMODAL_ACCESSOR(JSFunction, Map, initial_map)
BIMODAL_ACCESSOR(JSFunction, Object, prototype)
BIMODAL_ACCESSOR(JSFunction, SharedFunctionInfo, shared)

BIMODAL_ACCESSOR_B(Map, bit_field2, elements_kind, Map::ElementsKindBits)
BIMODAL_ACCESSOR_B(Map, bit_field3, is_deprecated, Map::IsDeprecatedBit)
BIMODAL_ACCESSOR_B(Map, bit_field3, is_dictionary_map, Map::IsDictionaryMapBit)
BIMODAL_ACCESSOR_B(Map, bit_field3, NumberOfOwnDescriptors,
                   Map::NumberOfOwnDescriptorsBits)
BIMODAL_ACCESSOR_B(Map, bit_field, has_prototype_slot, Map::HasPrototypeSlotBit)
BIMODAL_ACCESSOR_B(Map, bit_field, is_callable, Map::IsCallableBit)
BIMODAL_ACCESSOR_B(Map, bit_field, is_constructor, Map::IsConstructorBit)
BIMODAL_ACCESSOR_B(Map, bit_field, is_undetectable, Map::IsUndetectableBit)
BIMODAL_ACCESSOR_C(Map, int, instance_size)
BIMODAL_ACCESSOR(Map, Object, prototype)
BIMODAL_ACCESSOR_C(Map, InstanceType, instance_type)
BIMODAL_ACCESSOR(Map, Object, constructor_or_backpointer)

#define DEF_NATIVE_CONTEXT_ACCESSOR(type, name) \
  BIMODAL_ACCESSOR(NativeContext, type, name)
BROKER_NATIVE_CONTEXT_FIELDS(DEF_NATIVE_CONTEXT_ACCESSOR)
#undef DEF_NATIVE_CONTEXT_ACCESSOR

BIMODAL_ACCESSOR(PropertyCell, Object, value)
BIMODAL_ACCESSOR_C(PropertyCell, PropertyDetails, property_details)

BIMODAL_ACCESSOR_C(SharedFunctionInfo, int, builtin_id)
BIMODAL_ACCESSOR(SharedFunctionInfo, BytecodeArray, GetBytecodeArray)
#define DEF_SFI_ACCESSOR(type, name) \
  BIMODAL_ACCESSOR_C(SharedFunctionInfo, type, name)
BROKER_SFI_FIELDS(DEF_SFI_ACCESSOR)
#undef DEF_SFI_ACCESSOR

BIMODAL_ACCESSOR_C(String, int, length)

bool MapRef::IsInobjectSlackTrackingInProgress() const {
  IF_BROKER_DISABLED_ACCESS_HANDLE_C(Map, IsInobjectSlackTrackingInProgress);
  return Map::ConstructionCounterBits::decode(data()->AsMap()->bit_field3()) !=
         Map::kNoSlackTracking;
}

bool MapRef::is_stable() const {
  IF_BROKER_DISABLED_ACCESS_HANDLE_C(Map, is_stable);
  return !Map::IsUnstableBit::decode(data()->AsMap()->bit_field3());
}

bool MapRef::CanBeDeprecated() const {
  IF_BROKER_DISABLED_ACCESS_HANDLE_C(Map, CanBeDeprecated);
  CHECK_GT(NumberOfOwnDescriptors(), 0);
  return data()->AsMap()->can_be_deprecated();
}

bool MapRef::CanTransition() const {
  IF_BROKER_DISABLED_ACCESS_HANDLE_C(Map, CanTransition);
  return data()->AsMap()->can_transition();
}

int MapRef::GetInObjectPropertiesStartInWords() const {
  IF_BROKER_DISABLED_ACCESS_HANDLE_C(Map, GetInObjectPropertiesStartInWords);
  return data()->AsMap()->in_object_properties_start_in_words();
}

int MapRef::GetInObjectProperties() const {
  IF_BROKER_DISABLED_ACCESS_HANDLE_C(Map, GetInObjectProperties);
  return data()->AsMap()->in_object_properties();
}

int ScopeInfoRef::ContextLength() const {
  IF_BROKER_DISABLED_ACCESS_HANDLE_C(ScopeInfo, ContextLength);
  return data()->AsScopeInfo()->context_length();
}

bool StringRef::IsExternalString() const {
  IF_BROKER_DISABLED_ACCESS_HANDLE_C(String, IsExternalString);
  return data()->AsString()->is_external_string();
}

bool StringRef::IsSeqString() const {
  IF_BROKER_DISABLED_ACCESS_HANDLE_C(String, IsSeqString);
  return data()->AsString()->is_seq_string();
}

MapRef NativeContextRef::GetFunctionMapFromIndex(int index) const {
  DCHECK_GE(index, Context::FIRST_FUNCTION_MAP_INDEX);
  DCHECK_LE(index, Context::LAST_FUNCTION_MAP_INDEX);
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    return get(index).AsMap();
  }
  return MapRef(broker(), data()->AsNativeContext()->function_maps().at(
                              index - Context::FIRST_FUNCTION_MAP_INDEX));
}

MapRef NativeContextRef::GetInitialJSArrayMap(ElementsKind kind) const {
  switch (kind) {
    case PACKED_SMI_ELEMENTS:
      return js_array_packed_smi_elements_map();
    case HOLEY_SMI_ELEMENTS:
      return js_array_holey_smi_elements_map();
    case PACKED_DOUBLE_ELEMENTS:
      return js_array_packed_double_elements_map();
    case HOLEY_DOUBLE_ELEMENTS:
      return js_array_holey_double_elements_map();
    case PACKED_ELEMENTS:
      return js_array_packed_elements_map();
    case HOLEY_ELEMENTS:
      return js_array_holey_elements_map();
    default:
      UNREACHABLE();
  }
}

bool ObjectRef::BooleanValue() const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference allow_handle_dereference;
    return object<Object>()->BooleanValue(broker()->isolate());
  }
  return IsSmi() ? (AsSmi() != 0) : data()->AsHeapObject()->boolean_value();
}

double ObjectRef::OddballToNumber() const {
  OddballType type = AsHeapObject().map().oddball_type();

  switch (type) {
    case OddballType::kBoolean: {
      ObjectRef true_ref(broker(),
                         broker()->isolate()->factory()->true_value());
      return this->equals(true_ref) ? 1 : 0;
      break;
    }
    case OddballType::kUndefined: {
      return std::numeric_limits<double>::quiet_NaN();
      break;
    }
    case OddballType::kNull: {
      return 0;
      break;
    }
    default: {
      UNREACHABLE();
      break;
    }
  }
}

double HeapNumberRef::value() const {
  IF_BROKER_DISABLED_ACCESS_HANDLE_C(HeapNumber, value);
  return data()->AsHeapNumber()->value();
}

double MutableHeapNumberRef::value() const {
  IF_BROKER_DISABLED_ACCESS_HANDLE_C(MutableHeapNumber, value);
  return data()->AsMutableHeapNumber()->value();
}

CellRef ModuleRef::GetCell(int cell_index) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleAllocation handle_allocation;
    AllowHandleDereference allow_handle_dereference;
    return CellRef(broker(), handle(object<Module>()->GetCell(cell_index),
                                    broker()->isolate()));
  }
  return CellRef(broker(), data()->AsModule()->GetCell(cell_index));
}

ObjectRef::ObjectRef(JSHeapBroker* broker, Handle<Object> object)
    : broker_(broker) {
  switch (broker->mode()) {
    case JSHeapBroker::kSerialized:
      data_ = FLAG_strict_heap_broker ? broker->GetData(object)
                                      : broker->GetOrCreateData(object);
      break;
    case JSHeapBroker::kSerializing:
      data_ = broker->GetOrCreateData(object);
      break;
    case JSHeapBroker::kDisabled: {
      RefsMap::Entry* entry =
          broker->refs_->LookupOrInsert(object.address(), broker->zone());
      ObjectData** storage = &(entry->value);
      if (*storage == nullptr) {
        AllowHandleDereference handle_dereference;
        entry->value = new (broker->zone())
            ObjectData(broker, storage, object,
                       object->IsSmi() ? kSmi : kUnserializedHeapObject);
      }
      data_ = *storage;
      break;
    }
    case JSHeapBroker::kRetired:
      UNREACHABLE();
  }
  CHECK_NOT_NULL(data_);
}

namespace {
OddballType GetOddballType(Isolate* isolate, Map* map) {
  if (map->instance_type() != ODDBALL_TYPE) {
    return OddballType::kNone;
  }
  ReadOnlyRoots roots(isolate);
  if (map == roots.undefined_map()) {
    return OddballType::kUndefined;
  }
  if (map == roots.null_map()) {
    return OddballType::kNull;
  }
  if (map == roots.boolean_map()) {
    return OddballType::kBoolean;
  }
  if (map == roots.the_hole_map()) {
    return OddballType::kHole;
  }
  if (map == roots.uninitialized_map()) {
    return OddballType::kUninitialized;
  }
  DCHECK(map == roots.termination_exception_map() ||
         map == roots.arguments_marker_map() ||
         map == roots.optimized_out_map() || map == roots.stale_register_map());
  return OddballType::kOther;
}
}  // namespace

HeapObjectType HeapObjectRef::GetHeapObjectType() const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference handle_dereference;
    Map* map = Handle<HeapObject>::cast(object())->map();
    HeapObjectType::Flags flags(0);
    if (map->is_undetectable()) flags |= HeapObjectType::kUndetectable;
    if (map->is_callable()) flags |= HeapObjectType::kCallable;
    return HeapObjectType(map->instance_type(), flags,
                          GetOddballType(broker()->isolate(), map));
  }
  HeapObjectType::Flags flags(0);
  if (map().is_undetectable()) flags |= HeapObjectType::kUndetectable;
  if (map().is_callable()) flags |= HeapObjectType::kCallable;
  return HeapObjectType(map().instance_type(), flags, map().oddball_type());
}
base::Optional<JSObjectRef> AllocationSiteRef::boilerplate() const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleAllocation handle_allocation;
    AllowHandleDereference allow_handle_dereference;
    return JSObjectRef(broker(), handle(object<AllocationSite>()->boilerplate(),
                                        broker()->isolate()));
  }
  JSObjectData* boilerplate = data()->AsAllocationSite()->boilerplate();
  if (boilerplate) {
    return JSObjectRef(broker(), boilerplate);
  } else {
    return base::nullopt;
  }
}

ElementsKind JSObjectRef::GetElementsKind() const {
  return map().elements_kind();
}

FixedArrayBaseRef JSObjectRef::elements() const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleAllocation handle_allocation;
    AllowHandleDereference allow_handle_dereference;
    return FixedArrayBaseRef(
        broker(), handle(object<JSObject>()->elements(), broker()->isolate()));
  }
  return FixedArrayBaseRef(broker(), data()->AsJSObject()->elements());
}

int FixedArrayBaseRef::length() const {
  IF_BROKER_DISABLED_ACCESS_HANDLE_C(FixedArrayBase, length);
  return data()->AsFixedArrayBase()->length();
}

ObjectData* FixedArrayData::Get(int i) const {
  CHECK_LT(i, static_cast<int>(contents_.size()));
  CHECK_NOT_NULL(contents_[i]);
  return contents_[i];
}

Float64 FixedDoubleArrayData::Get(int i) const {
  CHECK_LT(i, static_cast<int>(contents_.size()));
  return contents_[i];
}

void FeedbackVectorRef::SerializeSlots() {
  data()->AsFeedbackVector()->SerializeSlots(broker());
}

ObjectRef JSRegExpRef::data() const {
  IF_BROKER_DISABLED_ACCESS_HANDLE(JSRegExp, Object, data);
  return ObjectRef(broker(), ObjectRef::data()->AsJSRegExp()->data());
}

ObjectRef JSRegExpRef::flags() const {
  IF_BROKER_DISABLED_ACCESS_HANDLE(JSRegExp, Object, flags);
  return ObjectRef(broker(), ObjectRef::data()->AsJSRegExp()->flags());
}

ObjectRef JSRegExpRef::last_index() const {
  IF_BROKER_DISABLED_ACCESS_HANDLE(JSRegExp, Object, last_index);
  return ObjectRef(broker(), ObjectRef::data()->AsJSRegExp()->last_index());
}

ObjectRef JSRegExpRef::raw_properties_or_hash() const {
  IF_BROKER_DISABLED_ACCESS_HANDLE(JSRegExp, Object, raw_properties_or_hash);
  return ObjectRef(broker(),
                   ObjectRef::data()->AsJSRegExp()->raw_properties_or_hash());
}

ObjectRef JSRegExpRef::source() const {
  IF_BROKER_DISABLED_ACCESS_HANDLE(JSRegExp, Object, source);
  return ObjectRef(broker(), ObjectRef::data()->AsJSRegExp()->source());
}

Handle<Object> ObjectRef::object() const { return data_->object(); }

JSHeapBroker* ObjectRef::broker() const { return broker_; }

ObjectData* ObjectRef::data() const {
  switch (broker()->mode()) {
    case JSHeapBroker::kDisabled:
      CHECK_NE(data_->kind(), kSerializedHeapObject);
      return data_;
    case JSHeapBroker::kSerializing:
    case JSHeapBroker::kSerialized:
      CHECK_NE(data_->kind(), kUnserializedHeapObject);
      return data_;
    case JSHeapBroker::kRetired:
      UNREACHABLE();
  }
}

Reduction NoChangeBecauseOfMissingData(JSHeapBroker* broker,
                                       const char* function, int line) {
  if (FLAG_trace_heap_broker) {
    PrintF("[%p] Skipping optimization in %s at line %d due to missing data\n",
           broker, function, line);
  }
  return AdvancedReducer::NoChange();
}

NativeContextData::NativeContextData(JSHeapBroker* broker, ObjectData** storage,
                                     Handle<NativeContext> object)
    : ContextData(broker, storage, object), function_maps_(broker->zone()) {}

void NativeContextData::Serialize(JSHeapBroker* broker) {
  if (serialized_) return;
  serialized_ = true;

  TraceScope tracer(broker, this, "NativeContextData::Serialize");
  Handle<NativeContext> context = Handle<NativeContext>::cast(object());

#define SERIALIZE_MEMBER(type, name)                              \
  DCHECK_NULL(name##_);                                           \
  name##_ = broker->GetOrCreateData(context->name())->As##type(); \
  if (name##_->IsJSFunction()) name##_->AsJSFunction()->Serialize(broker);
  BROKER_COMPULSORY_NATIVE_CONTEXT_FIELDS(SERIALIZE_MEMBER)
  if (!broker->isolate()->bootstrapper()->IsActive()) {
    BROKER_OPTIONAL_NATIVE_CONTEXT_FIELDS(SERIALIZE_MEMBER)
  }
#undef SERIALIZE_MEMBER

  DCHECK(function_maps_.empty());
  int const first = Context::FIRST_FUNCTION_MAP_INDEX;
  int const last = Context::LAST_FUNCTION_MAP_INDEX;
  function_maps_.reserve(last + 1 - first);
  for (int i = first; i <= last; ++i) {
    function_maps_.push_back(broker->GetOrCreateData(context->get(i))->AsMap());
  }
}

void JSFunctionRef::Serialize() {
  if (broker()->mode() == JSHeapBroker::kDisabled) return;
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsJSFunction()->Serialize(broker());
}

void JSObjectRef::SerializeObjectCreateMap() {
  if (broker()->mode() == JSHeapBroker::kDisabled) return;
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsJSObject()->SerializeObjectCreateMap(broker());
}

void MapRef::SerializeOwnDescriptors() {
  if (broker()->mode() == JSHeapBroker::kDisabled) return;
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsMap()->SerializeOwnDescriptors(broker());
}

void ModuleRef::Serialize() {
  if (broker()->mode() == JSHeapBroker::kDisabled) return;
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsModule()->Serialize(broker());
}

void ContextRef::Serialize() {
  if (broker()->mode() == JSHeapBroker::kDisabled) return;
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsContext()->Serialize(broker());
}

void NativeContextRef::Serialize() {
  if (broker()->mode() == JSHeapBroker::kDisabled) return;
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsNativeContext()->Serialize(broker());
}

#undef BIMODAL_ACCESSOR
#undef BIMODAL_ACCESSOR_B
#undef BIMODAL_ACCESSOR_C
#undef IF_BROKER_DISABLED_ACCESS_HANDLE
#undef IF_BROKER_DISABLED_ACCESS_HANDLE_C

}  // namespace compiler
}  // namespace internal
}  // namespace v8
