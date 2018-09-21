// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-heap-broker.h"

#include "src/compiler/graph-reducer.h"
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

// TODO(neis): It would be nice to share the serialized data for read-only
// objects.

class ObjectData : public ZoneObject {
 public:
  static ObjectData* Serialize(JSHeapBroker* broker, Handle<Object> object);

  ObjectData(JSHeapBroker* broker_, Handle<Object> object_, bool is_smi_)
      : broker(broker_), object(object_), is_smi(is_smi_) {
    broker->AddData(object, this);
  }

#define DECLARE_IS_AND_AS(Name) \
  bool Is##Name() const;        \
  Name##Data* As##Name();
  HEAP_BROKER_OBJECT_LIST(DECLARE_IS_AND_AS)
#undef DECLARE_IS_AND_AS

  JSHeapBroker* const broker;
  Handle<Object> const object;
  bool const is_smi;
};

// TODO(neis): Perhaps add a boolean that indicates whether serialization of an
// object has completed. That could be used to add safety checks.

#define GET_OR_CREATE(name) \
  broker->GetOrCreateData(handle(object_->name(), broker->isolate()))

class HeapObjectData : public ObjectData {
 public:
  static HeapObjectData* Serialize(JSHeapBroker* broker,
                                   Handle<HeapObject> object);

  HeapObjectType const type;
  MapData* const map;

  HeapObjectData(JSHeapBroker* broker_, Handle<HeapObject> object_,
                 HeapObjectType type_)
      : ObjectData(broker_, object_, false),
        type(type_),
        map(GET_OR_CREATE(map)->AsMap()) {
    CHECK(broker_->SerializingAllowed());
  }
};

class PropertyCellData : public HeapObjectData {
 public:
  PropertyCellData(JSHeapBroker* broker_, Handle<PropertyCell> object_,
                   HeapObjectType type_)
      : HeapObjectData(broker_, object_, type_) {}
};

class JSObjectData : public HeapObjectData {
 public:
  JSObjectData(JSHeapBroker* broker_, Handle<JSObject> object_,
               HeapObjectType type_)
      : HeapObjectData(broker_, object_, type_) {}
};

class JSFunctionData : public JSObjectData {
 public:
  JSGlobalProxyData* const global_proxy;
  MapData* const initial_map;  // Can be nullptr.
  bool const has_prototype;
  ObjectData* const prototype;  // Can be nullptr.
  bool const PrototypeRequiresRuntimeLookup;
  SharedFunctionInfoData* const shared;

  JSFunctionData(JSHeapBroker* broker_, Handle<JSFunction> object_,
                 HeapObjectType type_);
};

class JSRegExpData : public JSObjectData {
 public:
  JSRegExpData(JSHeapBroker* broker_, Handle<JSRegExp> object_,
               HeapObjectType type_)
      : JSObjectData(broker_, object_, type_) {}
};

class HeapNumberData : public HeapObjectData {
 public:
  HeapNumberData(JSHeapBroker* broker_, Handle<HeapNumber> object_,
                 HeapObjectType type_)
      : HeapObjectData(broker_, object_, type_) {}
};

class MutableHeapNumberData : public HeapObjectData {
 public:
  MutableHeapNumberData(JSHeapBroker* broker_,
                        Handle<MutableHeapNumber> object_, HeapObjectType type_)
      : HeapObjectData(broker_, object_, type_) {}
};

class ContextData : public HeapObjectData {
 public:
  ContextData(JSHeapBroker* broker_, Handle<Context> object_,
              HeapObjectType type_)
      : HeapObjectData(broker_, object_, type_) {}
};

class NativeContextData : public ContextData {
 public:
#define DECL_MEMBER(type, name) type##Data* const name;
  BROKER_NATIVE_CONTEXT_FIELDS(DECL_MEMBER)
#undef DECL_MEMBER

  NativeContextData(JSHeapBroker* broker_, Handle<NativeContext> object_,
                    HeapObjectType type_)
      : ContextData(broker_, object_, type_)
#define INIT_MEMBER(type, name) , name(GET_OR_CREATE(name)->As##type())
            BROKER_NATIVE_CONTEXT_FIELDS(INIT_MEMBER)
#undef INIT_MEMBER
  {
  }
};

class NameData : public HeapObjectData {
 public:
  NameData(JSHeapBroker* broker, Handle<Name> object, HeapObjectType type)
      : HeapObjectData(broker, object, type) {}
};

class StringData : public NameData {
 public:
  StringData(JSHeapBroker* broker, Handle<String> object, HeapObjectType type)
      : NameData(broker, object, type),
        length(object->length()),
        first_char(length > 0 ? object->Get(0) : 0) {
    int flags = ALLOW_HEX | ALLOW_OCTAL | ALLOW_BINARY;
    if (length <= kMaxLengthForDoubleConversion) {
      to_number = StringToDouble(
          broker->isolate(), broker->isolate()->unicode_cache(), object, flags);
    }
  }

  int const length;
  uint16_t const first_char;
  base::Optional<double> to_number;

 private:
  static constexpr int kMaxLengthForDoubleConversion = 23;
};

class InternalizedStringData : public StringData {
 public:
  InternalizedStringData(JSHeapBroker* broker,
                         Handle<InternalizedString> object, HeapObjectType type)
      : StringData(broker, object, type) {}
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
  AllocationSiteData(JSHeapBroker* broker, Handle<AllocationSite> object_,
                     HeapObjectType type_)
      : HeapObjectData(broker, object_, type_),
        PointsToLiteral(object_->PointsToLiteral()),
        GetPretenureMode(object_->GetPretenureMode()),
        nested_site(GET_OR_CREATE(nested_site)) {
    if (PointsToLiteral) {
      if (IsInlinableFastLiteral(
              handle(object_->boilerplate(), broker->isolate()))) {
        boilerplate = GET_OR_CREATE(boilerplate)->AsJSObject();
      }
    } else {
      GetElementsKind = object_->GetElementsKind();
      CanInlineCall = object_->CanInlineCall();
    }
  }

  bool const PointsToLiteral;
  PretenureFlag const GetPretenureMode;
  ObjectData* const nested_site;
  JSObjectData* boilerplate = nullptr;

  // These are only valid if PointsToLiteral is false.
  ElementsKind GetElementsKind = NO_ELEMENTS;
  bool CanInlineCall = false;
};

// Only used in JSNativeContextSpecialization.
class ScriptContextTableData : public HeapObjectData {
 public:
  ScriptContextTableData(JSHeapBroker* broker_,
                         Handle<ScriptContextTable> object_,
                         HeapObjectType type_)
      : HeapObjectData(broker_, object_, type_) {}
};

class MapData : public HeapObjectData {
 public:
  InstanceType const instance_type;
  int const instance_size;
  byte const bit_field;
  byte const bit_field2;
  uint32_t const bit_field3;

  MapData(JSHeapBroker* broker_, Handle<Map> object_, HeapObjectType type_);

  // Extra information.
  void SerializeElementsKindGeneralizations();
  const ZoneVector<MapData*>& elements_kind_generalizations() {
    return elements_kind_generalizations_;
  }

 private:
  ZoneVector<MapData*> elements_kind_generalizations_;
};

MapData::MapData(JSHeapBroker* broker_, Handle<Map> object_,
                 HeapObjectType type_)
    : HeapObjectData(broker_, object_, type_),
      instance_type(object_->instance_type()),
      instance_size(object_->instance_size()),
      bit_field(object_->bit_field()),
      bit_field2(object_->bit_field2()),
      bit_field3(object_->bit_field3()),
      elements_kind_generalizations_(broker->zone()) {}

JSFunctionData::JSFunctionData(JSHeapBroker* broker_,
                               Handle<JSFunction> object_, HeapObjectType type_)
    : JSObjectData(broker_, object_, type_),
      global_proxy(GET_OR_CREATE(global_proxy)->AsJSGlobalProxy()),
      initial_map(object_->has_prototype_slot() && object_->has_initial_map()
                      ? GET_OR_CREATE(initial_map)->AsMap()
                      : nullptr),
      has_prototype(object_->has_prototype_slot() && object_->has_prototype()),
      prototype(has_prototype ? GET_OR_CREATE(prototype) : nullptr),
      PrototypeRequiresRuntimeLookup(object_->PrototypeRequiresRuntimeLookup()),
      shared(GET_OR_CREATE(shared)->AsSharedFunctionInfo()) {
  if (initial_map != nullptr && initial_map->instance_type == JS_ARRAY_TYPE) {
    initial_map->SerializeElementsKindGeneralizations();
  }
}

void MapData::SerializeElementsKindGeneralizations() {
  broker->Trace("Computing ElementsKind generalizations of %p.\n", *object);
  DCHECK_EQ(instance_type, JS_ARRAY_TYPE);
  MapRef self(this);
  ElementsKind from_kind = self.elements_kind();
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

class FeedbackVectorData : public HeapObjectData {
 public:
  const ZoneVector<ObjectData*>& feedback() { return feedback_; }

  FeedbackVectorData(JSHeapBroker* broker_, Handle<FeedbackVector> object_,
                     HeapObjectType type_);

 private:
  ZoneVector<ObjectData*> feedback_;
};

FeedbackVectorData::FeedbackVectorData(JSHeapBroker* broker_,
                                       Handle<FeedbackVector> object_,
                                       HeapObjectType type_)
    : HeapObjectData(broker_, object_, type_), feedback_(broker_->zone()) {
  feedback_.reserve(object_->length());
  for (int i = 0; i < object_->length(); ++i) {
    MaybeObject* value = object_->get(i);
    feedback_.push_back(value->IsObject()
                            ? broker->GetOrCreateData(
                                  handle(value->ToObject(), broker->isolate()))
                            : nullptr);
  }
  DCHECK_EQ(object_->length(), feedback_.size());
}

class FixedArrayBaseData : public HeapObjectData {
 public:
  int const length;

  FixedArrayBaseData(JSHeapBroker* broker_, Handle<FixedArrayBase> object_,
                     HeapObjectType type_)
      : HeapObjectData(broker_, object_, type_), length(object_->length()) {}
};

class FixedArrayData : public FixedArrayBaseData {
 public:
  FixedArrayData(JSHeapBroker* broker_, Handle<FixedArray> object_,
                 HeapObjectType type_)
      : FixedArrayBaseData(broker_, object_, type_) {}
};

class FixedDoubleArrayData : public FixedArrayBaseData {
 public:
  FixedDoubleArrayData(JSHeapBroker* broker_, Handle<FixedDoubleArray> object_,
                       HeapObjectType type_)
      : FixedArrayBaseData(broker_, object_, type_) {}
};

class BytecodeArrayData : public FixedArrayBaseData {
 public:
  int const register_count;

  BytecodeArrayData(JSHeapBroker* broker_, Handle<BytecodeArray> object_,
                    HeapObjectType type_)
      : FixedArrayBaseData(broker_, object_, type_),
        register_count(object_->register_count()) {}
};

class JSArrayData : public JSObjectData {
 public:
  JSArrayData(JSHeapBroker* broker_, Handle<JSArray> object_,
              HeapObjectType type_)
      : JSObjectData(broker_, object_, type_) {}
};

class ScopeInfoData : public HeapObjectData {
 public:
  ScopeInfoData(JSHeapBroker* broker_, Handle<ScopeInfo> object_,
                HeapObjectType type_)
      : HeapObjectData(broker_, object_, type_) {}
};

class SharedFunctionInfoData : public HeapObjectData {
 public:
  int const builtin_id;
  BytecodeArrayData* const GetBytecodeArray;  // Can be nullptr.
#define DECL_MEMBER(type, name) type const name;
  BROKER_SFI_FIELDS(DECL_MEMBER)
#undef DECL_MEMBER

  SharedFunctionInfoData(JSHeapBroker* broker_,
                         Handle<SharedFunctionInfo> object_,
                         HeapObjectType type_)
      : HeapObjectData(broker_, object_, type_),
        builtin_id(object_->HasBuiltinId() ? object_->builtin_id()
                                           : Builtins::kNoBuiltinId),
        GetBytecodeArray(
            object_->HasBytecodeArray()
                ? GET_OR_CREATE(GetBytecodeArray)->AsBytecodeArray()
                : nullptr)
#define INIT_MEMBER(type, name) , name(object_->name())
            BROKER_SFI_FIELDS(INIT_MEMBER)
#undef INIT_MEMBER
  {
    DCHECK_EQ(HasBuiltinId, builtin_id != Builtins::kNoBuiltinId);
    DCHECK_EQ(HasBytecodeArray, GetBytecodeArray != nullptr);
  }
};

class ModuleData : public HeapObjectData {
 public:
  ModuleData(JSHeapBroker* broker_, Handle<Module> object_,
             HeapObjectType type_)
      : HeapObjectData(broker_, object_, type_) {}
};

class CellData : public HeapObjectData {
 public:
  CellData(JSHeapBroker* broker_, Handle<Cell> object_, HeapObjectType type_)
      : HeapObjectData(broker_, object_, type_) {}
};

class JSGlobalProxyData : public JSObjectData {
 public:
  JSGlobalProxyData(JSHeapBroker* broker_, Handle<JSGlobalProxy> object_,
                    HeapObjectType type_)
      : JSObjectData(broker_, object_, type_) {}
};

class CodeData : public HeapObjectData {
 public:
  CodeData(JSHeapBroker* broker_, Handle<Code> object_, HeapObjectType type_)
      : HeapObjectData(broker_, object_, type_) {}
};

#define DEFINE_IS_AND_AS(Name)                                          \
  bool ObjectData::Is##Name() const {                                   \
    if (broker->mode() == JSHeapBroker::kDisabled) {                    \
      AllowHandleDereference allow_handle_dereference;                  \
      return object->Is##Name();                                        \
    }                                                                   \
    if (is_smi) return false;                                           \
    InstanceType instance_type =                                        \
        static_cast<const HeapObjectData*>(this)->type.instance_type(); \
    return InstanceTypeChecker::Is##Name(instance_type);                \
  }                                                                     \
  Name##Data* ObjectData::As##Name() {                                  \
    CHECK_NE(broker->mode(), JSHeapBroker::kDisabled);                  \
    CHECK(Is##Name());                                                  \
    return static_cast<Name##Data*>(this);                              \
  }
HEAP_BROKER_OBJECT_LIST(DEFINE_IS_AND_AS)
#undef DEFINE_IS_AND_AS

ObjectData* ObjectData::Serialize(JSHeapBroker* broker, Handle<Object> object) {
  CHECK(broker->SerializingAllowed());
  return object->IsSmi() ? new (broker->zone()) ObjectData(broker, object, true)
                         : HeapObjectData::Serialize(
                               broker, Handle<HeapObject>::cast(object));
}

HeapObjectData* HeapObjectData::Serialize(JSHeapBroker* broker,
                                          Handle<HeapObject> object) {
  CHECK(broker->SerializingAllowed());
  Handle<Map> map(object->map(), broker->isolate());
  HeapObjectType type = broker->HeapObjectTypeFromMap(map);

#define RETURN_CREATE_DATA_IF_MATCH(name)                     \
  if (object->Is##name()) {                                   \
    return new (broker->zone())                               \
        name##Data(broker, Handle<name>::cast(object), type); \
  }
  HEAP_BROKER_OBJECT_LIST(RETURN_CREATE_DATA_IF_MATCH)
#undef RETURN_CREATE_DATA_IF_MATCH
  UNREACHABLE();
}

bool ObjectRef::equals(const ObjectRef& other) const {
  return data_ == other.data_;
}

StringRef ObjectRef::TypeOf() const {
  AllowHandleAllocation handle_allocation;
  AllowHandleDereference handle_dereference;
  return StringRef(broker(),
                   Object::TypeOf(broker()->isolate(), object<Object>()));
}

Isolate* ObjectRef::isolate() const { return broker()->isolate(); }

base::Optional<ContextRef> ContextRef::previous() const {
  AllowHandleAllocation handle_allocation;
  AllowHandleDereference handle_dereference;
  Context* previous = object<Context>()->previous();
  if (previous == nullptr) return base::Optional<ContextRef>();
  return ContextRef(broker(), handle(previous, broker()->isolate()));
}

ObjectRef ContextRef::get(int index) const {
  AllowHandleAllocation handle_allocation;
  AllowHandleDereference handle_dereference;
  Handle<Object> value(object<Context>()->get(index), broker()->isolate());
  return ObjectRef(broker(), value);
}

JSHeapBroker::JSHeapBroker(Isolate* isolate, Zone* zone)
    : isolate_(isolate),
      zone_(zone),
      refs_(zone),
      mode_(FLAG_concurrent_compiler_frontend ? kSerializing : kDisabled) {
  Trace("%s", "Constructing heap broker.\n");
}

void JSHeapBroker::Trace(const char* format, ...) const {
  if (FLAG_trace_heap_broker) {
    PrintF("[%p] ", this);
    va_list arguments;
    va_start(arguments, format);
    base::OS::VPrint(format, arguments);
    va_end(arguments);
  }
}

bool JSHeapBroker::SerializingAllowed() const {
  return mode() == kSerializing ||
         (!FLAG_strict_heap_broker && mode() == kSerialized);
}

void JSHeapBroker::SerializeStandardObjects() {
  Trace("Serializing standard objects.\n");

  Builtins* const b = isolate()->builtins();
  Factory* const f = isolate()->factory();

  // Stuff used by JSGraph:
  GetOrCreateData(f->empty_fixed_array());

  // Stuff used by JSCreateLowering:
  GetOrCreateData(f->block_context_map());
  GetOrCreateData(f->catch_context_map());
  GetOrCreateData(f->eval_context_map());
  GetOrCreateData(f->fixed_array_map());
  GetOrCreateData(f->fixed_double_array_map());
  GetOrCreateData(f->function_context_map());
  GetOrCreateData(f->many_closures_cell_map());
  GetOrCreateData(f->mutable_heap_number_map());
  GetOrCreateData(f->name_dictionary_map());
  GetOrCreateData(f->one_pointer_filler_map());
  GetOrCreateData(f->sloppy_arguments_elements_map());
  GetOrCreateData(f->with_context_map());

  // Stuff used by TypedOptimization:
  // Strings produced by typeof:
  GetOrCreateData(f->boolean_string());
  GetOrCreateData(f->number_string());
  GetOrCreateData(f->string_string());
  GetOrCreateData(f->bigint_string());
  GetOrCreateData(f->symbol_string());
  GetOrCreateData(f->undefined_string());
  GetOrCreateData(f->object_string());
  GetOrCreateData(f->function_string());

  // Stuff used by JSTypedLowering:
  GetOrCreateData(f->length_string());
  Builtins::Name builtins[] = {
      Builtins::kArgumentsAdaptorTrampoline,
      Builtins::kCallFunctionForwardVarargs,
      Builtins::kStringAdd_CheckNone_NotTenured,
      Builtins::kStringAdd_CheckNone_Tenured,
      Builtins::kStringAdd_ConvertLeft_NotTenured,
      Builtins::kStringAdd_ConvertRight_NotTenured,
  };
  for (auto id : builtins) {
    GetOrCreateData(b->builtin_handle(id));
  }
  for (int32_t id = 0; id < Builtins::builtin_count; ++id) {
    if (Builtins::KindOf(id) == Builtins::TFJ) {
      GetOrCreateData(b->builtin_handle(id));
    }
  }

  Trace("Finished serializing standard objects.\n");
}

HeapObjectType JSHeapBroker::HeapObjectTypeFromMap(Map* map) const {
  AllowHandleDereference allow_handle_dereference;
  OddballType oddball_type = OddballType::kNone;
  if (map->instance_type() == ODDBALL_TYPE) {
    ReadOnlyRoots roots(isolate_);
    if (map == roots.undefined_map()) {
      oddball_type = OddballType::kUndefined;
    } else if (map == roots.null_map()) {
      oddball_type = OddballType::kNull;
    } else if (map == roots.boolean_map()) {
      oddball_type = OddballType::kBoolean;
    } else if (map == roots.the_hole_map()) {
      oddball_type = OddballType::kHole;
    } else if (map == roots.uninitialized_map()) {
      oddball_type = OddballType::kUninitialized;
    } else {
      oddball_type = OddballType::kOther;
      DCHECK(map == roots.termination_exception_map() ||
             map == roots.arguments_marker_map() ||
             map == roots.optimized_out_map() ||
             map == roots.stale_register_map());
    }
  }
  HeapObjectType::Flags flags(0);
  if (map->is_undetectable()) flags |= HeapObjectType::kUndetectable;
  if (map->is_callable()) flags |= HeapObjectType::kCallable;

  return HeapObjectType(map->instance_type(), flags, oddball_type);
}

ObjectData* JSHeapBroker::GetData(Handle<Object> object) const {
  auto it = refs_.find(object.address());
  return it != refs_.end() ? it->second : nullptr;
}

ObjectData* JSHeapBroker::GetOrCreateData(Handle<Object> object) {
  CHECK(SerializingAllowed());
  ObjectData* data = GetData(object);
  if (data == nullptr) {
    // TODO(neis): Remove these Allow* once we serialize everything upfront.
    AllowHandleAllocation handle_allocation;
    AllowHandleDereference handle_dereference;
    data = ObjectData::Serialize(this, object);
  }
  CHECK_NOT_NULL(data);
  return data;
}

void JSHeapBroker::AddData(Handle<Object> object, ObjectData* data) {
  Trace("Creating data %p for handle %" V8PRIuPTR " (", data, object.address());
  if (FLAG_trace_heap_broker) {
    object->ShortPrint();
    PrintF(")\n");
  }
  CHECK_NOT_NULL(isolate()->handle_scope_data()->canonical_scope);
  CHECK(refs_.insert({object.address(), data}).second);
}

#define DEFINE_IS_AND_AS(Name)                                    \
  bool ObjectRef::Is##Name() const { return data()->Is##Name(); } \
  Name##Ref ObjectRef::As##Name() const {                         \
    DCHECK(Is##Name());                                           \
    return Name##Ref(data());                                     \
  }
HEAP_BROKER_OBJECT_LIST(DEFINE_IS_AND_AS)
#undef DEFINE_IS_AND_AS

bool ObjectRef::IsSmi() const { return data()->is_smi; }

int ObjectRef::AsSmi() const {
  DCHECK(IsSmi());
  // Handle-dereference is always allowed for Handle<Smi>.
  return object<Smi>()->value();
}

HeapObjectType HeapObjectRef::type() const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference allow_handle_dereference;
    return broker()->HeapObjectTypeFromMap(object<HeapObject>()->map());
  } else {
    return data()->AsHeapObject()->type;
  }
}

base::Optional<MapRef> HeapObjectRef::TryGetObjectCreateMap() const {
  AllowHandleAllocation handle_allocation;
  AllowHandleDereference allow_handle_dereference;
  Handle<Map> instance_map;
  if (Map::TryGetObjectCreateMap(broker()->isolate(), object<HeapObject>())
          .ToHandle(&instance_map)) {
    return MapRef(broker(), instance_map);
  } else {
    return base::Optional<MapRef>();
  }
}

base::Optional<MapRef> MapRef::AsElementsKind(ElementsKind kind) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleAllocation handle_allocation;
    AllowHeapAllocation heap_allocation;
    AllowHandleDereference allow_handle_dereference;
    return MapRef(broker(), Map::AsElementsKind(broker()->isolate(),
                                                object<Map>(), kind));
  } else {
    if (kind == elements_kind()) return *this;
    const ZoneVector<MapData*>& elements_kind_generalizations =
        data()->AsMap()->elements_kind_generalizations();
    for (auto data : elements_kind_generalizations) {
      MapRef map(data);
      if (map.elements_kind() == kind) return map;
    }
    return base::Optional<MapRef>();
  }
}

int JSFunctionRef::InitialMapInstanceSizeWithMinSlack() const {
  AllowHandleDereference allow_handle_dereference;
  AllowHandleAllocation handle_allocation;

  return object<JSFunction>()->ComputeInstanceSizeWithMinSlack(
      broker()->isolate());
}

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

OddballType ObjectRef::oddball_type() const {
  return IsSmi() ? OddballType::kNone : AsHeapObject().type().oddball_type();
}

ObjectRef FeedbackVectorRef::get(FeedbackSlot slot) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleAllocation handle_allocation;
    AllowHandleDereference handle_dereference;
    Handle<Object> value(object<FeedbackVector>()->Get(slot)->ToObject(),
                         broker()->isolate());
    return ObjectRef(broker(), value);
  }
  int i = FeedbackVector::GetIndex(slot);
  return ObjectRef(data()->AsFeedbackVector()->feedback().at(i));
}

bool JSObjectRef::IsUnboxedDoubleField(FieldIndex index) const {
  AllowHandleDereference handle_dereference;
  return object<JSObject>()->IsUnboxedDoubleField(index);
}

double JSObjectRef::RawFastDoublePropertyAt(FieldIndex index) const {
  AllowHandleDereference handle_dereference;
  return object<JSObject>()->RawFastDoublePropertyAt(index);
}

ObjectRef JSObjectRef::RawFastPropertyAt(FieldIndex index) const {
  AllowHandleAllocation handle_allocation;
  AllowHandleDereference handle_dereference;
  return ObjectRef(broker(),
                   handle(object<JSObject>()->RawFastPropertyAt(index),
                          broker()->isolate()));
}


bool AllocationSiteRef::IsFastLiteral() const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHeapAllocation
        allow_heap_allocation;  // This is needed for TryMigrateInstance.
    AllowHandleAllocation allow_handle_allocation;
    AllowHandleDereference allow_handle_dereference;
    return IsInlinableFastLiteral(
        handle(object<AllocationSite>()->boilerplate(), broker()->isolate()));
  } else {
    return data()->AsAllocationSite()->boilerplate != nullptr;
  }
}

void JSObjectRef::EnsureElementsTenured() {
  // TODO(jarin) Eventually, we will pretenure the boilerplates before
  // the compilation job starts.
  AllowHandleAllocation allow_handle_allocation;
  AllowHandleDereference allow_handle_dereference;
  AllowHeapAllocation allow_heap_allocation;

  Handle<FixedArrayBase> object_elements = elements().object<FixedArrayBase>();
  if (Heap::InNewSpace(*object_elements)) {
    // If we would like to pretenure a fixed cow array, we must ensure that
    // the array is already in old space, otherwise we'll create too many
    // old-to-new-space pointers (overflowing the store buffer).
    object_elements =
        broker()->isolate()->factory()->CopyAndTenureFixedCOWArray(
            Handle<FixedArray>::cast(object_elements));
    object<JSObject>()->set_elements(*object_elements);
  }
}

FieldIndex MapRef::GetFieldIndexFor(int i) const {
  AllowHandleDereference allow_handle_dereference;
  return FieldIndex::ForDescriptor(*object<Map>(), i);
}

int MapRef::GetInObjectPropertyOffset(int i) const {
  AllowHandleDereference allow_handle_dereference;
  return object<Map>()->GetInObjectPropertyOffset(i);
}

PropertyDetails MapRef::GetPropertyDetails(int i) const {
  AllowHandleDereference allow_handle_dereference;
  return object<Map>()->instance_descriptors()->GetDetails(i);
}

NameRef MapRef::GetPropertyKey(int i) const {
  AllowHandleAllocation handle_allocation;
  AllowHandleDereference allow_handle_dereference;
  return NameRef(broker(),
                 handle(object<Map>()->instance_descriptors()->GetKey(i),
                        broker()->isolate()));
}

bool MapRef::IsFixedCowArrayMap() const {
  AllowHandleDereference allow_handle_dereference;
  return *object<Map>() ==
         ReadOnlyRoots(broker()->isolate()).fixed_cow_array_map();
}

MapRef MapRef::FindFieldOwner(int descriptor) const {
  AllowHandleAllocation handle_allocation;
  AllowHandleDereference allow_handle_dereference;
  Handle<Map> owner(
      object<Map>()->FindFieldOwner(broker()->isolate(), descriptor),
      broker()->isolate());
  return MapRef(broker(), owner);
}

ObjectRef MapRef::GetFieldType(int descriptor) const {
  AllowHandleAllocation handle_allocation;
  AllowHandleDereference allow_handle_dereference;
  Handle<FieldType> field_type(
      object<Map>()->instance_descriptors()->GetFieldType(descriptor),
      broker()->isolate());
  return ObjectRef(broker(), field_type);
}

uint16_t StringRef::GetFirstChar() {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference allow_handle_dereference;
    return object<String>()->Get(0);
  } else {
    return data()->AsString()->first_char;
  }
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
  } else {
    return data()->AsString()->to_number;
  }
}

bool FixedArrayRef::is_the_hole(int i) const {
  AllowHandleDereference allow_handle_dereference;
  return object<FixedArray>()->is_the_hole(broker()->isolate(), i);
}

ObjectRef FixedArrayRef::get(int i) const {
  AllowHandleAllocation handle_allocation;
  AllowHandleDereference allow_handle_dereference;
  return ObjectRef(broker(),
                   handle(object<FixedArray>()->get(i), broker()->isolate()));
}

bool FixedDoubleArrayRef::is_the_hole(int i) const {
  AllowHandleDereference allow_handle_dereference;
  return object<FixedDoubleArray>()->is_the_hole(i);
}

double FixedDoubleArrayRef::get_scalar(int i) const {
  AllowHandleDereference allow_handle_dereference;
  return object<FixedDoubleArray>()->get_scalar(i);
}

#define IF_BROKER_DISABLED_ACCESS_HANDLE_C(holder, name) \
  if (broker()->mode() == JSHeapBroker::kDisabled) {     \
    AllowHandleAllocation handle_allocation;             \
    AllowHandleDereference allow_handle_dereference;     \
    return object<holder>()->name();                     \
  }

// Macros for definining a const getter that, depending on the broker mode,
// either looks into the handle or into the serialized data. The first one is
// used for the rare case of a XYZRef class that does not have a corresponding
// XYZ class in objects.h. The second one is used otherwise.
#define BIMODAL_ACCESSOR(holder, result, name)                              \
  result##Ref holder##Ref::name() const {                                   \
    if (broker()->mode() == JSHeapBroker::kDisabled) {                      \
      AllowHandleAllocation handle_allocation;                              \
      AllowHandleDereference allow_handle_dereference;                      \
      return result##Ref(                                                   \
          broker(), handle(object<holder>()->name(), broker()->isolate())); \
    } else {                                                                \
      return result##Ref(data()->As##holder()->name);                       \
    }                                                                       \
  }

// Like HANDLE_ACCESSOR except that the result type is not an XYZRef.
#define BIMODAL_ACCESSOR_C(holder, result, name)      \
  result holder##Ref::name() const {                  \
    IF_BROKER_DISABLED_ACCESS_HANDLE_C(holder, name); \
    return data()->As##holder()->name;                \
  }

// Like HANDLE_ACCESSOR_C but for BitFields.
#define BIMODAL_ACCESSOR_B(holder, field, name, BitField)  \
  typename BitField::FieldType holder##Ref::name() const { \
    IF_BROKER_DISABLED_ACCESS_HANDLE_C(holder, name);      \
    return BitField::decode(data()->As##holder()->field);  \
  }

// Macros for definining a const getter that always looks into the handle.
// (These will go away once we serialize everything.) The first one is used for
// the rare case of a XYZRef class that does not have a corresponding XYZ class
// in objects.h. The second one is used otherwise.
#define HANDLE_ACCESSOR(holder, result, name)                                  \
  result##Ref holder##Ref::name() const {                                      \
    AllowHandleAllocation handle_allocation;                                   \
    AllowHandleDereference allow_handle_dereference;                           \
    return result##Ref(broker(),                                               \
                       handle(object<holder>()->name(), broker()->isolate())); \
  }

// Like HANDLE_ACCESSOR except that the result type is not an XYZRef.
#define HANDLE_ACCESSOR_C(holder, result, name)      \
  result holder##Ref::name() const {                 \
    AllowHandleAllocation handle_allocation;         \
    AllowHandleDereference allow_handle_dereference; \
    return object<holder>()->name();                 \
  }

BIMODAL_ACCESSOR(AllocationSite, Object, nested_site)
BIMODAL_ACCESSOR_C(AllocationSite, bool, CanInlineCall)
BIMODAL_ACCESSOR_C(AllocationSite, bool, PointsToLiteral)
BIMODAL_ACCESSOR_C(AllocationSite, ElementsKind, GetElementsKind)
BIMODAL_ACCESSOR_C(AllocationSite, PretenureFlag, GetPretenureMode)

BIMODAL_ACCESSOR_C(BytecodeArray, int, register_count)

BIMODAL_ACCESSOR_C(FixedArrayBase, int, length)

BIMODAL_ACCESSOR(HeapObject, Map, map)
HANDLE_ACCESSOR_C(HeapObject, bool, IsExternalString)
HANDLE_ACCESSOR_C(HeapObject, bool, IsSeqString)

HANDLE_ACCESSOR_C(HeapNumber, double, value)

HANDLE_ACCESSOR(JSArray, Object, length)

BIMODAL_ACCESSOR_C(JSFunction, bool, has_prototype)
BIMODAL_ACCESSOR_C(JSFunction, bool, PrototypeRequiresRuntimeLookup)
BIMODAL_ACCESSOR(JSFunction, Map, initial_map)
BIMODAL_ACCESSOR(JSFunction, Object, prototype)
HANDLE_ACCESSOR_C(JSFunction, bool, IsConstructor)
HANDLE_ACCESSOR(JSFunction, JSGlobalProxy, global_proxy)
HANDLE_ACCESSOR(JSFunction, SharedFunctionInfo, shared)

HANDLE_ACCESSOR(JSObject, FixedArrayBase, elements)

HANDLE_ACCESSOR(JSRegExp, Object, data)
HANDLE_ACCESSOR(JSRegExp, Object, flags)
HANDLE_ACCESSOR(JSRegExp, Object, last_index)
HANDLE_ACCESSOR(JSRegExp, Object, raw_properties_or_hash)
HANDLE_ACCESSOR(JSRegExp, Object, source)

BIMODAL_ACCESSOR_B(Map, bit_field2, elements_kind, Map::ElementsKindBits)
BIMODAL_ACCESSOR_B(Map, bit_field3, is_deprecated, Map::IsDeprecatedBit)
BIMODAL_ACCESSOR_B(Map, bit_field3, is_dictionary_map, Map::IsDictionaryMapBit)
BIMODAL_ACCESSOR_B(Map, bit_field, has_prototype_slot, Map::HasPrototypeSlotBit)
BIMODAL_ACCESSOR_C(Map, int, instance_size)
HANDLE_ACCESSOR_C(Map, bool, CanBeDeprecated)
HANDLE_ACCESSOR_C(Map, bool, CanTransition)
HANDLE_ACCESSOR_C(Map, bool, IsInobjectSlackTrackingInProgress)
HANDLE_ACCESSOR_C(Map, bool, IsJSArrayMap)
HANDLE_ACCESSOR_C(Map, bool, is_stable)
HANDLE_ACCESSOR_C(Map, InstanceType, instance_type)
HANDLE_ACCESSOR_C(Map, int, GetInObjectProperties)
HANDLE_ACCESSOR_C(Map, int, GetInObjectPropertiesStartInWords)
HANDLE_ACCESSOR_C(Map, int, NumberOfOwnDescriptors)
HANDLE_ACCESSOR(Map, Object, constructor_or_backpointer)

HANDLE_ACCESSOR_C(MutableHeapNumber, double, value)

#define DEF_NATIVE_CONTEXT_ACCESSOR(type, name) \
  BIMODAL_ACCESSOR(NativeContext, type, name)
BROKER_NATIVE_CONTEXT_FIELDS(DEF_NATIVE_CONTEXT_ACCESSOR)
#undef DEF_NATIVE_CONTEXT_ACCESSOR

HANDLE_ACCESSOR(PropertyCell, Object, value)
HANDLE_ACCESSOR_C(PropertyCell, PropertyDetails, property_details)

HANDLE_ACCESSOR_C(ScopeInfo, int, ContextLength)

BIMODAL_ACCESSOR_C(SharedFunctionInfo, int, builtin_id)
BIMODAL_ACCESSOR(SharedFunctionInfo, BytecodeArray, GetBytecodeArray)
#define DEF_SFI_ACCESSOR(type, name) \
  BIMODAL_ACCESSOR_C(SharedFunctionInfo, type, name)
BROKER_SFI_FIELDS(DEF_SFI_ACCESSOR)
#undef DEF_SFI_ACCESSOR

BIMODAL_ACCESSOR_C(String, int, length)

// TODO(neis): Provide StringShape() on StringRef.

bool JSFunctionRef::has_initial_map() const {
  IF_BROKER_DISABLED_ACCESS_HANDLE_C(JSFunction, has_initial_map);
  return data()->AsJSFunction()->initial_map != nullptr;
}

MapRef NativeContextRef::GetFunctionMapFromIndex(int index) const {
  DCHECK_LE(index, Context::LAST_FUNCTION_MAP_INDEX);
  DCHECK_GE(index, Context::FIRST_FUNCTION_MAP_INDEX);
  return get(index).AsMap();
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

bool ObjectRef::BooleanValue() {
  AllowHandleDereference allow_handle_dereference;
  return object<Object>()->BooleanValue(broker()->isolate());
}

double ObjectRef::OddballToNumber() const {
  OddballType type = oddball_type();

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

CellRef ModuleRef::GetCell(int cell_index) {
  AllowHandleAllocation handle_allocation;
  AllowHandleDereference allow_handle_dereference;
  return CellRef(broker(), handle(object<Module>()->GetCell(cell_index),
                                  broker()->isolate()));
}

ObjectRef::ObjectRef(JSHeapBroker* broker, Handle<Object> object) {
  switch (broker->mode()) {
    case JSHeapBroker::kSerialized:
      data_ = FLAG_strict_heap_broker ? broker->GetData(object)
                                      : broker->GetOrCreateData(object);
      break;
    case JSHeapBroker::kSerializing:
      data_ = broker->GetOrCreateData(object);
      break;
    case JSHeapBroker::kDisabled:
      data_ = broker->GetData(object);
      if (data_ == nullptr) {
        AllowHandleDereference handle_dereference;
        data_ =
            new (broker->zone()) ObjectData(broker, object, object->IsSmi());
      }
      break;
  }
  CHECK_NOT_NULL(data_);
}

base::Optional<JSObjectRef> AllocationSiteRef::boilerplate() const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleAllocation handle_allocation;
    AllowHandleDereference allow_handle_dereference;
    return JSObjectRef(broker(), handle(object<AllocationSite>()->boilerplate(),
                                        broker()->isolate()));
  } else {
    JSObjectData* boilerplate = data()->AsAllocationSite()->boilerplate;
    if (boilerplate) {
      return JSObjectRef(boilerplate);
    } else {
      return base::nullopt;
    }
  }
}

ElementsKind JSObjectRef::GetElementsKind() const {
  return map().elements_kind();
}

Handle<Object> ObjectRef::object() const { return data_->object; }

JSHeapBroker* ObjectRef::broker() const { return data_->broker; }

ObjectData* ObjectRef::data() const { return data_; }

Reduction NoChangeBecauseOfMissingData(JSHeapBroker* broker,
                                       const char* function, int line) {
  if (FLAG_trace_heap_broker) {
    PrintF("[%p] Skipping optimization in %s at line %d due to missing data\n",
           broker, function, line);
  }
  return AdvancedReducer::NoChange();
}

#undef BIMODAL_ACCESSOR
#undef BIMODAL_ACCESSOR_B
#undef BIMODAL_ACCESSOR_C
#undef GET_OR_CREATE
#undef HANDLE_ACCESSOR
#undef HANDLE_ACCESSOR_C
#undef IF_BROKER_DISABLED_ACCESS_HANDLE_C

}  // namespace compiler
}  // namespace internal
}  // namespace v8
