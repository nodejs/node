// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-heap-broker.h"

#ifdef ENABLE_SLOW_DCHECKS
#include <algorithm>
#endif

#include "src/api-inl.h"
#include "src/ast/modules.h"
#include "src/bootstrapper.h"
#include "src/boxed-float.h"
#include "src/code-factory.h"
#include "src/compiler/graph-reducer.h"
#include "src/compiler/per-isolate-compiler-cache.h"
#include "src/objects-inl.h"
#include "src/objects/allocation-site-inl.h"
#include "src/objects/api-callbacks.h"
#include "src/objects/cell-inl.h"
#include "src/objects/heap-number-inl.h"
#include "src/objects/instance-type-inl.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/js-regexp-inl.h"
#include "src/objects/module-inl.h"
#include "src/objects/templates.h"
#include "src/utils.h"
#include "src/vector-slot-pair.h"

namespace v8 {
namespace internal {
namespace compiler {

#define TRACE(broker, x) TRACE_BROKER(broker, x)
#define TRACE_MISSING(broker, x) TRACE_BROKER_MISSING(broker, x)

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

    TRACE(broker, "Creating data " << this << " for handle " << object.address()
                                   << " (" << Brief(*object) << ")");

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
  ObjectData* value() const { return value_; }

 private:
  PropertyDetails const property_details_;

  bool serialized_ = false;
  ObjectData* value_ = nullptr;
};

class FunctionTemplateInfoData : public HeapObjectData {
 public:
  FunctionTemplateInfoData(JSHeapBroker* broker, ObjectData** storage,
                           Handle<FunctionTemplateInfo> object);

  void Serialize(JSHeapBroker* broker);
  ObjectData* call_code() const { return call_code_; }

 private:
  bool serialized_ = false;
  ObjectData* call_code_ = nullptr;
};

class CallHandlerInfoData : public HeapObjectData {
 public:
  CallHandlerInfoData(JSHeapBroker* broker, ObjectData** storage,
                      Handle<CallHandlerInfo> object);

  Address callback() const { return callback_; }

  void Serialize(JSHeapBroker* broker);
  ObjectData* data() const { return data_; }

 private:
  Address const callback_;

  bool serialized_ = false;
  ObjectData* data_ = nullptr;
};

FunctionTemplateInfoData::FunctionTemplateInfoData(
    JSHeapBroker* broker, ObjectData** storage,
    Handle<FunctionTemplateInfo> object)
    : HeapObjectData(broker, storage, object) {}

CallHandlerInfoData::CallHandlerInfoData(JSHeapBroker* broker,
                                         ObjectData** storage,
                                         Handle<CallHandlerInfo> object)
    : HeapObjectData(broker, storage, object),
      callback_(v8::ToCData<Address>(object->callback())) {}

void JSHeapBroker::IncrementTracingIndentation() { ++trace_indentation_; }

void JSHeapBroker::DecrementTracingIndentation() { --trace_indentation_; }

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
    TRACE(broker_, "Running " << label << " on " << self);
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

void FunctionTemplateInfoData::Serialize(JSHeapBroker* broker) {
  if (serialized_) return;
  serialized_ = true;

  TraceScope tracer(broker, this, "FunctionTemplateInfoData::Serialize");
  auto function_template_info = Handle<FunctionTemplateInfo>::cast(object());
  DCHECK_NULL(call_code_);
  call_code_ = broker->GetOrCreateData(function_template_info->call_code());

  if (call_code_->IsCallHandlerInfo()) {
    call_code_->AsCallHandlerInfo()->Serialize(broker);
  }
}

void CallHandlerInfoData::Serialize(JSHeapBroker* broker) {
  if (serialized_) return;
  serialized_ = true;

  TraceScope tracer(broker, this, "CallHandlerInfoData::Serialize");
  auto call_handler_info = Handle<CallHandlerInfo>::cast(object());
  DCHECK_NULL(data_);
  data_ = broker->GetOrCreateData(call_handler_info->data());
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

  // Recursive serialization of all reachable JSObjects.
  void SerializeAsBoilerplate(JSHeapBroker* broker);
  const JSObjectField& GetInobjectField(int property_index) const;

  // Shallow serialization of {elements}.
  void SerializeElements(JSHeapBroker* broker);
  bool serialized_elements() const { return serialized_elements_; }
  FixedArrayBaseData* elements() const;

  void SerializeObjectCreateMap(JSHeapBroker* broker);
  MapData* object_create_map() const {  // Can be nullptr.
    CHECK(serialized_object_create_map_);
    return object_create_map_;
  }

  ObjectData* GetOwnConstantElement(JSHeapBroker* broker, uint32_t index,
                                    bool serialize);

  // This method is only used to assert our invariants.
  bool cow_or_empty_elements_tenured() const;

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

  // Elements (indexed properties) that either
  // (1) are known to exist directly on the object as non-writable and
  // non-configurable, or (2) are known not to (possibly they don't exist at
  // all). In case (2), the second pair component is nullptr.
  ZoneVector<std::pair<uint32_t, ObjectData*>> own_constant_elements_;
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

namespace {
base::Optional<ObjectRef> GetOwnElementFromHeap(JSHeapBroker* broker,
                                                Handle<Object> receiver,
                                                uint32_t index,
                                                bool constant_only) {
  LookupIterator it(broker->isolate(), receiver, index, LookupIterator::OWN);
  if (it.state() == LookupIterator::DATA &&
      (!constant_only || (it.IsReadOnly() && !it.IsConfigurable()))) {
    return ObjectRef(broker, it.GetDataValue());
  }
  return base::nullopt;
}
}  // namespace

ObjectData* JSObjectData::GetOwnConstantElement(JSHeapBroker* broker,
                                                uint32_t index,
                                                bool serialize) {
  for (auto const& p : own_constant_elements_) {
    if (p.first == index) return p.second;
  }

  if (!serialize) {
    TRACE_MISSING(broker, "knowledge about index " << index << " on " << this);
    return nullptr;
  }

  base::Optional<ObjectRef> element =
      GetOwnElementFromHeap(broker, object(), index, true);
  ObjectData* result = element.has_value() ? element->data() : nullptr;
  own_constant_elements_.push_back({index, result});
  return result;
}

class JSTypedArrayData : public JSObjectData {
 public:
  JSTypedArrayData(JSHeapBroker* broker, ObjectData** storage,
                   Handle<JSTypedArray> object);

  bool is_on_heap() const { return is_on_heap_; }
  size_t length_value() const { return length_value_; }
  void* elements_external_pointer() const { return elements_external_pointer_; }

  void Serialize(JSHeapBroker* broker);
  bool serialized() const { return serialized_; }

  HeapObjectData* buffer() const { return buffer_; }

 private:
  bool const is_on_heap_;
  size_t const length_value_;
  void* const elements_external_pointer_;

  bool serialized_ = false;
  HeapObjectData* buffer_ = nullptr;
};

JSTypedArrayData::JSTypedArrayData(JSHeapBroker* broker, ObjectData** storage,
                                   Handle<JSTypedArray> object)
    : JSObjectData(broker, storage, object),
      is_on_heap_(object->is_on_heap()),
      length_value_(object->length_value()),
      elements_external_pointer_(
          FixedTypedArrayBase::cast(object->elements())->external_pointer()) {}

void JSTypedArrayData::Serialize(JSHeapBroker* broker) {
  if (serialized_) return;
  serialized_ = true;

  TraceScope tracer(broker, this, "JSTypedArrayData::Serialize");
  Handle<JSTypedArray> typed_array = Handle<JSTypedArray>::cast(object());

  if (!is_on_heap()) {
    DCHECK_NULL(buffer_);
    buffer_ = broker->GetOrCreateData(typed_array->buffer())->AsHeapObject();
  }
}

class JSDataViewData : public JSObjectData {
 public:
  JSDataViewData(JSHeapBroker* broker, ObjectData** storage,
                 Handle<JSDataView> object);

  size_t byte_length() const { return byte_length_; }
  size_t byte_offset() const { return byte_offset_; }

 private:
  size_t const byte_length_;
  size_t const byte_offset_;
};

class JSBoundFunctionData : public JSObjectData {
 public:
  JSBoundFunctionData(JSHeapBroker* broker, ObjectData** storage,
                      Handle<JSBoundFunction> object);

  void Serialize(JSHeapBroker* broker);

  ObjectData* bound_target_function() const { return bound_target_function_; }
  ObjectData* bound_this() const { return bound_this_; }
  FixedArrayData* bound_arguments() const { return bound_arguments_; }

 private:
  bool serialized_ = false;

  ObjectData* bound_target_function_ = nullptr;
  ObjectData* bound_this_ = nullptr;
  FixedArrayData* bound_arguments_ = nullptr;
};

class JSFunctionData : public JSObjectData {
 public:
  JSFunctionData(JSHeapBroker* broker, ObjectData** storage,
                 Handle<JSFunction> object);

  bool has_feedback_vector() const { return has_feedback_vector_; }
  bool has_initial_map() const { return has_initial_map_; }
  bool has_prototype() const { return has_prototype_; }
  bool PrototypeRequiresRuntimeLookup() const {
    return PrototypeRequiresRuntimeLookup_;
  }

  void Serialize(JSHeapBroker* broker);
  bool serialized() const { return serialized_; }

  ContextData* context() const { return context_; }
  NativeContextData* native_context() const { return native_context_; }
  MapData* initial_map() const { return initial_map_; }
  ObjectData* prototype() const { return prototype_; }
  SharedFunctionInfoData* shared() const { return shared_; }
  FeedbackVectorData* feedback_vector() const { return feedback_vector_; }
  int initial_map_instance_size_with_min_slack() const {
    CHECK(serialized_);
    return initial_map_instance_size_with_min_slack_;
  }

 private:
  bool has_feedback_vector_;
  bool has_initial_map_;
  bool has_prototype_;
  bool PrototypeRequiresRuntimeLookup_;

  bool serialized_ = false;

  ContextData* context_ = nullptr;
  NativeContextData* native_context_ = nullptr;
  MapData* initial_map_ = nullptr;
  ObjectData* prototype_ = nullptr;
  SharedFunctionInfoData* shared_ = nullptr;
  FeedbackVectorData* feedback_vector_ = nullptr;
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
  void SerializeContextChain(JSHeapBroker* broker);

  ContextData* previous() const {
    CHECK(serialized_context_chain_);
    return previous_;
  }

  void SerializeSlot(JSHeapBroker* broker, int index);

  ObjectData* GetSlot(int index) {
    auto search = slots_.find(index);
    CHECK(search != slots_.end());
    return search->second;
  }

 private:
  ZoneMap<int, ObjectData*> slots_;
  bool serialized_context_chain_ = false;
  ContextData* previous_ = nullptr;
};

ContextData::ContextData(JSHeapBroker* broker, ObjectData** storage,
                         Handle<Context> object)
    : HeapObjectData(broker, storage, object), slots_(broker->zone()) {}

void ContextData::SerializeContextChain(JSHeapBroker* broker) {
  if (serialized_context_chain_) return;
  serialized_context_chain_ = true;

  TraceScope tracer(broker, this, "ContextData::SerializeContextChain");
  Handle<Context> context = Handle<Context>::cast(object());

  DCHECK_NULL(previous_);
  // Context::previous DCHECK-fails when called on the native context.
  if (!context->IsNativeContext()) {
    previous_ = broker->GetOrCreateData(context->previous())->AsContext();
    previous_->SerializeContextChain(broker);
  }
}

void ContextData::SerializeSlot(JSHeapBroker* broker, int index) {
  TraceScope tracer(broker, this, "ContextData::SerializeSlot");
  TRACE(broker, "Serializing script context slot " << index);
  Handle<Context> context = Handle<Context>::cast(object());
  CHECK(index >= 0 && index < context->length());
  ObjectData* odata = broker->GetOrCreateData(context->get(index));
  slots_.insert(std::make_pair(index, odata));
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

  StringData* GetCharAsString(JSHeapBroker* broker, uint32_t index,
                              bool serialize);

 private:
  int const length_;
  uint16_t const first_char_;
  base::Optional<double> to_number_;
  bool const is_external_string_;
  bool const is_seq_string_;

  // Known individual characters as strings, corresponding to the semantics of
  // element access (s[i]). The first pair component is always less than
  // {length_}. The second component is never nullptr.
  ZoneVector<std::pair<uint32_t, StringData*>> chars_as_strings_;

  static constexpr int kMaxLengthForDoubleConversion = 23;
};

class SymbolData : public NameData {
 public:
  SymbolData(JSHeapBroker* broker, ObjectData** storage, Handle<Symbol> object)
      : NameData(broker, storage, object) {}
};

StringData::StringData(JSHeapBroker* broker, ObjectData** storage,
                       Handle<String> object)
    : NameData(broker, storage, object),
      length_(object->length()),
      first_char_(length_ > 0 ? object->Get(0) : 0),
      is_external_string_(object->IsExternalString()),
      is_seq_string_(object->IsSeqString()),
      chars_as_strings_(broker->zone()) {
  int flags = ALLOW_HEX | ALLOW_OCTAL | ALLOW_BINARY;
  if (length_ <= kMaxLengthForDoubleConversion) {
    to_number_ = StringToDouble(broker->isolate(), object, flags);
  }
}

class InternalizedStringData : public StringData {
 public:
  InternalizedStringData(JSHeapBroker* broker, ObjectData** storage,
                         Handle<InternalizedString> object);

  uint32_t array_index() const { return array_index_; }

 private:
  uint32_t array_index_;
};

StringData* StringData::GetCharAsString(JSHeapBroker* broker, uint32_t index,
                                        bool serialize) {
  if (index >= static_cast<uint32_t>(length())) return nullptr;

  for (auto const& p : chars_as_strings_) {
    if (p.first == index) return p.second;
  }

  if (!serialize) {
    TRACE_MISSING(broker, "knowledge about index " << index << " on " << this);
    return nullptr;
  }

  base::Optional<ObjectRef> element =
      GetOwnElementFromHeap(broker, object(), index, true);
  StringData* result =
      element.has_value() ? element->data()->AsString() : nullptr;
  chars_as_strings_.push_back({index, result});
  return result;
}

InternalizedStringData::InternalizedStringData(
    JSHeapBroker* broker, ObjectData** storage,
    Handle<InternalizedString> object)
    : StringData(broker, storage, object) {}

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
  AllocationType GetAllocationType() const { return GetAllocationType_; }
  ObjectData* nested_site() const { return nested_site_; }
  bool IsFastLiteral() const { return IsFastLiteral_; }
  JSObjectData* boilerplate() const { return boilerplate_; }

  // These are only valid if PointsToLiteral is false.
  ElementsKind GetElementsKind() const { return GetElementsKind_; }
  bool CanInlineCall() const { return CanInlineCall_; }

 private:
  bool const PointsToLiteral_;
  AllocationType const GetAllocationType_;
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
  int constructor_function_index() const { return constructor_function_index_; }
  int NextFreePropertyIndex() const { return next_free_property_index_; }
  int UnusedPropertyFields() const { return unused_property_fields_; }
  bool supports_fast_array_iteration() const {
    return supports_fast_array_iteration_;
  }
  bool supports_fast_array_resize() const {
    return supports_fast_array_resize_;
  }
  bool IsMapOfCurrentGlobalProxy() const {
    return is_map_of_current_global_proxy_;
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

  void SerializeConstructor(JSHeapBroker* broker);
  ObjectData* GetConstructor() const {
    CHECK(serialized_constructor_);
    return constructor_;
  }

  void SerializeBackPointer(JSHeapBroker* broker);
  HeapObjectData* GetBackPointer() const {
    CHECK(serialized_backpointer_);
    return backpointer_;
  }

  void SerializePrototype(JSHeapBroker* broker);
  bool serialized_prototype() const { return serialized_prototype_; }
  ObjectData* prototype() const {
    CHECK(serialized_prototype_);
    return prototype_;
  }

  void SerializeForElementLoad(JSHeapBroker* broker);

  void SerializeForElementStore(JSHeapBroker* broker);

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
  int const constructor_function_index_;
  int const next_free_property_index_;
  int const unused_property_fields_;
  bool const supports_fast_array_iteration_;
  bool const supports_fast_array_resize_;
  bool const is_map_of_current_global_proxy_;

  bool serialized_elements_kind_generalizations_ = false;
  ZoneVector<MapData*> elements_kind_generalizations_;

  bool serialized_own_descriptors_ = false;
  DescriptorArrayData* instance_descriptors_ = nullptr;

  bool serialized_constructor_ = false;
  ObjectData* constructor_ = nullptr;

  bool serialized_backpointer_ = false;
  HeapObjectData* backpointer_ = nullptr;

  bool serialized_prototype_ = false;
  ObjectData* prototype_ = nullptr;

  bool serialized_for_element_load_ = false;

  bool serialized_for_element_store_ = false;
};

AllocationSiteData::AllocationSiteData(JSHeapBroker* broker,
                                       ObjectData** storage,
                                       Handle<AllocationSite> object)
    : HeapObjectData(broker, storage, object),
      PointsToLiteral_(object->PointsToLiteral()),
      GetAllocationType_(object->GetAllocationType()) {
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

namespace {
bool IsReadOnlyLengthDescriptor(Isolate* isolate, Handle<Map> jsarray_map) {
  DCHECK(!jsarray_map->is_dictionary_map());
  Handle<Name> length_string = isolate->factory()->length_string();
  DescriptorArray descriptors = jsarray_map->instance_descriptors();
  int number = descriptors->Search(*length_string, *jsarray_map);
  DCHECK_NE(DescriptorArray::kNotFound, number);
  return descriptors->GetDetails(number).IsReadOnly();
}

bool SupportsFastArrayIteration(Isolate* isolate, Handle<Map> map) {
  return map->instance_type() == JS_ARRAY_TYPE &&
         IsFastElementsKind(map->elements_kind()) &&
         map->prototype()->IsJSArray() &&
         isolate->IsAnyInitialArrayPrototype(
             handle(JSArray::cast(map->prototype()), isolate)) &&
         isolate->IsNoElementsProtectorIntact();
}

bool SupportsFastArrayResize(Isolate* isolate, Handle<Map> map) {
  return SupportsFastArrayIteration(isolate, map) && map->is_extensible() &&
         !map->is_dictionary_map() && !IsReadOnlyLengthDescriptor(isolate, map);
}
}  // namespace

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
      constructor_function_index_(object->IsPrimitiveMap()
                                      ? object->GetConstructorFunctionIndex()
                                      : Map::kNoConstructorFunctionIndex),
      next_free_property_index_(object->NextFreePropertyIndex()),
      unused_property_fields_(object->UnusedPropertyFields()),
      supports_fast_array_iteration_(
          SupportsFastArrayIteration(broker->isolate(), object)),
      supports_fast_array_resize_(
          SupportsFastArrayResize(broker->isolate(), object)),
      is_map_of_current_global_proxy_(
          object->IsMapOfGlobalProxy(broker->isolate()->native_context())),
      elements_kind_generalizations_(broker->zone()) {}

JSFunctionData::JSFunctionData(JSHeapBroker* broker, ObjectData** storage,
                               Handle<JSFunction> object)
    : JSObjectData(broker, storage, object),
      has_feedback_vector_(object->has_feedback_vector()),
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

  DCHECK_NULL(context_);
  DCHECK_NULL(native_context_);
  DCHECK_NULL(initial_map_);
  DCHECK_NULL(prototype_);
  DCHECK_NULL(shared_);
  DCHECK_NULL(feedback_vector_);

  context_ = broker->GetOrCreateData(function->context())->AsContext();
  native_context_ =
      broker->GetOrCreateData(function->native_context())->AsNativeContext();
  shared_ = broker->GetOrCreateData(function->shared())->AsSharedFunctionInfo();
  feedback_vector_ = has_feedback_vector()
                         ? broker->GetOrCreateData(function->feedback_vector())
                               ->AsFeedbackVector()
                         : nullptr;
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
    initial_map_->SerializeConstructor(broker);
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
          Map::AsElementsKind(broker->isolate(), self.object(), to_kind);
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
    MaybeObject value = vector->get(i);
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
  TRACE(broker, "Copied " << feedback_.size() << " slots");
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

JSDataViewData::JSDataViewData(JSHeapBroker* broker, ObjectData** storage,
                               Handle<JSDataView> object)
    : JSObjectData(broker, storage, object),
      byte_length_(object->byte_length()),
      byte_offset_(object->byte_offset()) {}

JSBoundFunctionData::JSBoundFunctionData(JSHeapBroker* broker,
                                         ObjectData** storage,
                                         Handle<JSBoundFunction> object)
    : JSObjectData(broker, storage, object) {}

void JSBoundFunctionData::Serialize(JSHeapBroker* broker) {
  if (serialized_) return;
  serialized_ = true;

  TraceScope tracer(broker, this, "JSBoundFunctionData::Serialize");
  Handle<JSBoundFunction> function = Handle<JSBoundFunction>::cast(object());

  DCHECK_NULL(bound_target_function_);
  DCHECK_NULL(bound_this_);
  DCHECK_NULL(bound_arguments_);

  bound_target_function_ =
      broker->GetOrCreateData(function->bound_target_function());
  bound_this_ = broker->GetOrCreateData(function->bound_this());
  bound_arguments_ =
      broker->GetOrCreateData(function->bound_arguments())->AsFixedArray();

  bound_arguments_->SerializeContents(broker);
}

JSObjectData::JSObjectData(JSHeapBroker* broker, ObjectData** storage,
                           Handle<JSObject> object)
    : HeapObjectData(broker, storage, object),
      inobject_fields_(broker->zone()),
      own_constant_elements_(broker->zone()) {}

FixedArrayData::FixedArrayData(JSHeapBroker* broker, ObjectData** storage,
                               Handle<FixedArray> object)
    : FixedArrayBaseData(broker, storage, object), contents_(broker->zone()) {}

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
  TRACE(broker, "Copied " << contents_.size() << " elements");
}

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
  TRACE(broker, "Copied " << contents_.size() << " elements");
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

  ObjectData* GetOwnElement(JSHeapBroker* broker, uint32_t index,
                            bool serialize);

 private:
  bool serialized_ = false;
  ObjectData* length_ = nullptr;

  // Elements (indexed properties) that either
  // (1) are known to exist directly on the object, or
  // (2) are known not to (possibly they don't exist at all).
  // In case (2), the second pair component is nullptr.
  ZoneVector<std::pair<uint32_t, ObjectData*>> own_elements_;
};

JSArrayData::JSArrayData(JSHeapBroker* broker, ObjectData** storage,
                         Handle<JSArray> object)
    : JSObjectData(broker, storage, object), own_elements_(broker->zone()) {}

void JSArrayData::Serialize(JSHeapBroker* broker) {
  if (serialized_) return;
  serialized_ = true;

  TraceScope tracer(broker, this, "JSArrayData::Serialize");
  Handle<JSArray> jsarray = Handle<JSArray>::cast(object());

  DCHECK_NULL(length_);
  length_ = broker->GetOrCreateData(jsarray->length());
}

ObjectData* JSArrayData::GetOwnElement(JSHeapBroker* broker, uint32_t index,
                                       bool serialize) {
  for (auto const& p : own_elements_) {
    if (p.first == index) return p.second;
  }

  if (!serialize) {
    TRACE_MISSING(broker, "knowledge about index " << index << " on " << this);
    return nullptr;
  }

  base::Optional<ObjectRef> element =
      GetOwnElementFromHeap(broker, object(), index, false);
  ObjectData* result = element.has_value() ? element->data() : nullptr;
  own_elements_.push_back({index, result});
  return result;
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
  SharedFunctionInfoData(JSHeapBroker* broker, ObjectData** storage,
                         Handle<SharedFunctionInfo> object);

  int builtin_id() const { return builtin_id_; }
  BytecodeArrayData* GetBytecodeArray() const { return GetBytecodeArray_; }
  void SetSerializedForCompilation(JSHeapBroker* broker,
                                   FeedbackVectorRef feedback);
  bool IsSerializedForCompilation(FeedbackVectorRef feedback) const;
#define DECL_ACCESSOR(type, name) \
  type name() const { return name##_; }
  BROKER_SFI_FIELDS(DECL_ACCESSOR)
#undef DECL_ACCESSOR

 private:
  int const builtin_id_;
  BytecodeArrayData* const GetBytecodeArray_;
  ZoneUnorderedSet<Handle<FeedbackVector>, Handle<FeedbackVector>::hash,
                   Handle<FeedbackVector>::equal_to>
      serialized_for_compilation_;
#define DECL_MEMBER(type, name) type const name##_;
  BROKER_SFI_FIELDS(DECL_MEMBER)
#undef DECL_MEMBER
};

SharedFunctionInfoData::SharedFunctionInfoData(
    JSHeapBroker* broker, ObjectData** storage,
    Handle<SharedFunctionInfo> object)
    : HeapObjectData(broker, storage, object),
      builtin_id_(object->HasBuiltinId() ? object->builtin_id()
                                         : Builtins::kNoBuiltinId),
      GetBytecodeArray_(
          object->HasBytecodeArray()
              ? broker->GetOrCreateData(object->GetBytecodeArray())
                    ->AsBytecodeArray()
              : nullptr),
      serialized_for_compilation_(broker->zone())
#define INIT_MEMBER(type, name) , name##_(object->name())
          BROKER_SFI_FIELDS(INIT_MEMBER)
#undef INIT_MEMBER
{
  DCHECK_EQ(HasBuiltinId_, builtin_id_ != Builtins::kNoBuiltinId);
  DCHECK_EQ(HasBytecodeArray_, GetBytecodeArray_ != nullptr);
}

void SharedFunctionInfoData::SetSerializedForCompilation(
    JSHeapBroker* broker, FeedbackVectorRef feedback) {
  CHECK(serialized_for_compilation_.insert(feedback.object()).second);
  TRACE(broker, "Set function " << object() << " with " << feedback.object()
                                << " as serialized for compilation");
}

bool SharedFunctionInfoData::IsSerializedForCompilation(
    FeedbackVectorRef feedback) const {
  return serialized_for_compilation_.find(feedback.object()) !=
         serialized_for_compilation_.end();
}

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
  TRACE(broker, "Copied " << imports_.size() << " imports");

  DCHECK(exports_.empty());
  Handle<FixedArray> exports(module->regular_exports(), broker->isolate());
  int const exports_length = exports->length();
  exports_.reserve(exports_length);
  for (int i = 0; i < exports_length; ++i) {
    exports_.push_back(broker->GetOrCreateData(exports->get(i))->AsCell());
  }
  TRACE(broker, "Copied " << exports_.size() << " exports");
}

class CellData : public HeapObjectData {
 public:
  CellData(JSHeapBroker* broker, ObjectData** storage, Handle<Cell> object);

  void Serialize(JSHeapBroker* broker);
  ObjectData* value() { return value_; }

 private:
  bool serialized_ = false;
  ObjectData* value_ = nullptr;
};

CellData::CellData(JSHeapBroker* broker, ObjectData** storage,
                   Handle<Cell> object)
    : HeapObjectData(broker, storage, object) {}

void CellData::Serialize(JSHeapBroker* broker) {
  if (serialized_) return;
  serialized_ = true;

  TraceScope tracer(broker, this, "CellData::Serialize");
  auto cell = Handle<Cell>::cast(object());
  DCHECK_NULL(value_);
  value_ = broker->GetOrCreateData(cell->value());
}

class JSGlobalProxyData : public JSObjectData {
 public:
  JSGlobalProxyData(JSHeapBroker* broker, ObjectData** storage,
                    Handle<JSGlobalProxy> object);

  PropertyCellData* GetPropertyCell(JSHeapBroker* broker, NameData* name,
                                    bool serialize);

 private:
  // Properties that either
  // (1) are known to exist as property cells on the global object, or
  // (2) are known not to (possibly they don't exist at all).
  // In case (2), the second pair component is nullptr.
  ZoneVector<std::pair<NameData*, PropertyCellData*>> properties_;
};

JSGlobalProxyData::JSGlobalProxyData(JSHeapBroker* broker, ObjectData** storage,
                                     Handle<JSGlobalProxy> object)
    : JSObjectData(broker, storage, object), properties_(broker->zone()) {}

namespace {
base::Optional<PropertyCellRef> GetPropertyCellFromHeap(JSHeapBroker* broker,
                                                        Handle<Name> name) {
  LookupIterator it(broker->isolate(),
                    handle(broker->native_context().object()->global_object(),
                           broker->isolate()),
                    name, LookupIterator::OWN);
  it.TryLookupCachedProperty();
  if (it.state() == LookupIterator::DATA &&
      it.GetHolder<JSObject>()->IsJSGlobalObject()) {
    return PropertyCellRef(broker, it.GetPropertyCell());
  }
  return base::nullopt;
}
}  // namespace

PropertyCellData* JSGlobalProxyData::GetPropertyCell(JSHeapBroker* broker,
                                                     NameData* name,
                                                     bool serialize) {
  CHECK_NOT_NULL(name);
  for (auto const& p : properties_) {
    if (p.first == name) return p.second;
  }

  if (!serialize) {
    TRACE_MISSING(broker, "knowledge about global property " << name);
    return nullptr;
  }

  PropertyCellData* result = nullptr;
  base::Optional<PropertyCellRef> cell =
      GetPropertyCellFromHeap(broker, Handle<Name>::cast(name->object()));
  if (cell.has_value()) {
    cell->Serialize();
    result = cell->data()->AsPropertyCell();
  }
  properties_.push_back({name, result});
  return result;
}

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

void MapData::SerializeConstructor(JSHeapBroker* broker) {
  if (serialized_constructor_) return;
  serialized_constructor_ = true;

  TraceScope tracer(broker, this, "MapData::SerializeConstructor");
  Handle<Map> map = Handle<Map>::cast(object());
  DCHECK_NULL(constructor_);
  constructor_ = broker->GetOrCreateData(map->GetConstructor());
}

void MapData::SerializeBackPointer(JSHeapBroker* broker) {
  if (serialized_backpointer_) return;
  serialized_backpointer_ = true;

  TraceScope tracer(broker, this, "MapData::SerializeBackPointer");
  Handle<Map> map = Handle<Map>::cast(object());
  DCHECK_NULL(backpointer_);
  backpointer_ = broker->GetOrCreateData(map->GetBackPointer())->AsHeapObject();
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

  TRACE(broker, "Copied " << number_of_own - current_size
                          << " descriptors into " << instance_descriptors_
                          << " (" << number_of_own << " total)");
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
    if (ObjectInYoungGeneration(*elements_object)) {
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
  TRACE(broker, "Copied " << inobject_fields_.size() << " in-object fields");

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
    return ContextRef(broker(),
                      handle(object()->previous(), broker()->isolate()));
  }
  return ContextRef(broker(), data()->AsContext()->previous());
}

// Not needed for TypedLowering.
ObjectRef ContextRef::get(int index) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleAllocation handle_allocation;
    AllowHandleDereference handle_dereference;
    Handle<Object> value(object()->get(index), broker()->isolate());
    return ObjectRef(broker(), value);
  }
  return ObjectRef(broker(), data()->AsContext()->GetSlot(index));
}

JSHeapBroker::JSHeapBroker(Isolate* isolate, Zone* broker_zone)
    : isolate_(isolate),
      broker_zone_(broker_zone),
      current_zone_(broker_zone),
      refs_(new (zone())
                RefsMap(kMinimalRefsBucketCount, AddressMatcher(), zone())),
      array_and_object_prototypes_(zone()),
      feedback_(zone()) {
  // Note that this initialization of the refs_ pointer with the minimal
  // initial capacity is redundant in the normal use case (concurrent
  // compilation enabled, standard objects to be serialized), as the map
  // is going to be replaced immediatelly with a larger capacity one.
  // It doesn't seem to affect the performance in a noticeable way though.
  TRACE(this, "Constructing heap broker");
}

std::ostream& JSHeapBroker::Trace() {
  return trace_out_ << "[" << this << "] "
                    << std::string(trace_indentation_ * 2, ' ');
}

void JSHeapBroker::StartSerializing() {
  CHECK_EQ(mode_, kDisabled);
  TRACE(this, "Starting serialization");
  mode_ = kSerializing;
  refs_->Clear();
}

void JSHeapBroker::StopSerializing() {
  CHECK_EQ(mode_, kSerializing);
  TRACE(this, "Stopping serialization");
  mode_ = kSerialized;
}

void JSHeapBroker::Retire() {
  CHECK_EQ(mode_, kSerialized);
  TRACE(this, "Retiring");
  mode_ = kRetired;
}

bool JSHeapBroker::SerializingAllowed() const { return mode() == kSerializing; }

void JSHeapBroker::SetNativeContextRef() {
  native_context_ = NativeContextRef(this, isolate()->native_context());
}

bool IsShareable(Handle<Object> object, Isolate* isolate) {
  Builtins* const b = isolate->builtins();

  int index;
  RootIndex root_index;
  return (object->IsHeapObject() &&
          b->IsBuiltinHandle(Handle<HeapObject>::cast(object), &index)) ||
         isolate->roots_table().IsRootHandle(object, &root_index);
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
        Builtins::kAllocateInYoungGeneration,
        Builtins::kAllocateInOldGeneration,
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

void JSHeapBroker::CollectArrayAndObjectPrototypes() {
  DisallowHeapAllocation no_gc;
  CHECK_EQ(mode(), kSerializing);
  CHECK(array_and_object_prototypes_.empty());

  Object maybe_context = isolate()->heap()->native_contexts_list();
  while (!maybe_context->IsUndefined(isolate())) {
    Context context = Context::cast(maybe_context);
    Object array_prot = context->get(Context::INITIAL_ARRAY_PROTOTYPE_INDEX);
    Object object_prot = context->get(Context::INITIAL_OBJECT_PROTOTYPE_INDEX);
    array_and_object_prototypes_.emplace(JSObject::cast(array_prot), isolate());
    array_and_object_prototypes_.emplace(JSObject::cast(object_prot),
                                         isolate());
    maybe_context = context->next_context_link();
  }

  CHECK(!array_and_object_prototypes_.empty());
}

bool JSHeapBroker::IsArrayOrObjectPrototype(const JSObjectRef& object) const {
  if (mode() == kDisabled) {
    return isolate()->IsInAnyContext(*object.object(),
                                     Context::INITIAL_ARRAY_PROTOTYPE_INDEX) ||
           isolate()->IsInAnyContext(*object.object(),
                                     Context::INITIAL_OBJECT_PROTOTYPE_INDEX);
  }
  CHECK(!array_and_object_prototypes_.empty());
  return array_and_object_prototypes_.find(object.object()) !=
         array_and_object_prototypes_.end();
}

void JSHeapBroker::SerializeStandardObjects() {
  if (mode() == kDisabled) return;
  CHECK_EQ(mode(), kSerializing);

  SerializeShareableObjects();

  TraceScope tracer(this, "JSHeapBroker::SerializeStandardObjects");

  CollectArrayAndObjectPrototypes();

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

  // Protector cells
  GetOrCreateData(f->array_buffer_detaching_protector())
      ->AsPropertyCell()
      ->Serialize(this);
  GetOrCreateData(f->array_constructor_protector())->AsCell()->Serialize(this);
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
  GetOrCreateData(f->string_length_protector())->AsCell()->Serialize(this);

  // CEntry stub
  GetOrCreateData(
      CodeFactory::CEntry(isolate(), 1, kDontSaveFPRegs, kArgvOnStack, true));

  TRACE(this, "Finished serializing standard objects");
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

ObjectData* JSHeapBroker::GetOrCreateData(Object object) {
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
  return Handle<Smi>::cast(object())->value();
}

base::Optional<MapRef> JSObjectRef::GetObjectCreateMap() const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleAllocation handle_allocation;
    AllowHandleDereference allow_handle_dereference;
    AllowHeapAllocation heap_allocation;
    Handle<Map> instance_map;
    if (Map::TryGetObjectCreateMap(broker()->isolate(), object())
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

#define DEF_TESTER(Type, ...)                              \
  bool MapRef::Is##Type##Map() const {                     \
    return InstanceTypeChecker::Is##Type(instance_type()); \
  }
INSTANCE_TYPE_CHECKERS(DEF_TESTER)
#undef DEF_TESTER

base::Optional<MapRef> MapRef::AsElementsKind(ElementsKind kind) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleAllocation handle_allocation;
    AllowHeapAllocation heap_allocation;
    AllowHandleDereference allow_handle_dereference;
    return MapRef(broker(),
                  Map::AsElementsKind(broker()->isolate(), object(), kind));
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

void MapRef::SerializeForElementLoad() {
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsMap()->SerializeForElementLoad(broker());
}

void MapRef::SerializeForElementStore() {
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsMap()->SerializeForElementStore(broker());
}

namespace {
// This helper function has two modes. If {prototype_maps} is nullptr, the
// prototype chain is serialized as necessary to determine the result.
// Otherwise, the heap is untouched and the encountered prototypes are pushed
// onto {prototype_maps}.
bool HasOnlyStablePrototypesWithFastElementsHelper(
    JSHeapBroker* broker, MapRef const& map,
    ZoneVector<MapRef>* prototype_maps) {
  for (MapRef prototype_map = map;;) {
    if (prototype_maps == nullptr) prototype_map.SerializePrototype();
    prototype_map = prototype_map.prototype().AsHeapObject().map();
    if (prototype_map.oddball_type() == OddballType::kNull) return true;
    if (!map.prototype().IsJSObject() || !prototype_map.is_stable() ||
        !IsFastElementsKind(prototype_map.elements_kind())) {
      return false;
    }
    if (prototype_maps != nullptr) prototype_maps->push_back(prototype_map);
  }
}
}  // namespace

void MapData::SerializeForElementLoad(JSHeapBroker* broker) {
  if (serialized_for_element_load_) return;
  serialized_for_element_load_ = true;

  TraceScope tracer(broker, this, "MapData::SerializeForElementLoad");
  SerializePrototype(broker);
}

void MapData::SerializeForElementStore(JSHeapBroker* broker) {
  if (serialized_for_element_store_) return;
  serialized_for_element_store_ = true;

  TraceScope tracer(broker, this, "MapData::SerializeForElementStore");
  HasOnlyStablePrototypesWithFastElementsHelper(broker, MapRef(broker, this),
                                                nullptr);
}

bool MapRef::HasOnlyStablePrototypesWithFastElements(
    ZoneVector<MapRef>* prototype_maps) {
  for (MapRef prototype_map = *this;;) {
    if (prototype_maps == nullptr) prototype_map.SerializePrototype();
    prototype_map = prototype_map.prototype().AsHeapObject().map();
    if (prototype_map.oddball_type() == OddballType::kNull) return true;
    if (!prototype().IsJSObject() || !prototype_map.is_stable() ||
        !IsFastElementsKind(prototype_map.elements_kind())) {
      return false;
    }
    if (prototype_maps != nullptr) prototype_maps->push_back(prototype_map);
  }
}

bool MapRef::supports_fast_array_iteration() const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference allow_handle_dereference;
    AllowHandleAllocation handle_allocation;
    return SupportsFastArrayIteration(broker()->isolate(), object());
  }
  return data()->AsMap()->supports_fast_array_iteration();
}

bool MapRef::supports_fast_array_resize() const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference allow_handle_dereference;
    AllowHandleAllocation handle_allocation;
    return SupportsFastArrayResize(broker()->isolate(), object());
  }
  return data()->AsMap()->supports_fast_array_resize();
}

bool MapRef::IsMapOfCurrentGlobalProxy() const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference allow_handle_dereference;
    AllowHandleAllocation handle_allocation;
    return object()->IsMapOfGlobalProxy(broker()->isolate()->native_context());
  }
  return data()->AsMap()->IsMapOfCurrentGlobalProxy();
}

int JSFunctionRef::InitialMapInstanceSizeWithMinSlack() const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference allow_handle_dereference;
    AllowHandleAllocation handle_allocation;
    return object()->ComputeInstanceSizeWithMinSlack(broker()->isolate());
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
  auto table = object();
  if (!ScriptContextTable::Lookup(broker()->isolate(), *table,
                                  *name.AsString().object(), &lookup_result)) {
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
    Handle<Object> value(object()->Get(slot)->cast<Object>(),
                         broker()->isolate());
    return ObjectRef(broker(), value);
  }
  int i = FeedbackVector::GetIndex(slot);
  return ObjectRef(broker(), data()->AsFeedbackVector()->feedback().at(i));
}

double JSObjectRef::RawFastDoublePropertyAt(FieldIndex index) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference handle_dereference;
    return object()->RawFastDoublePropertyAt(index);
  }
  JSObjectData* object_data = data()->AsJSObject();
  CHECK(index.is_inobject());
  return object_data->GetInobjectField(index.property_index()).AsDouble();
}

ObjectRef JSObjectRef::RawFastPropertyAt(FieldIndex index) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleAllocation handle_allocation;
    AllowHandleDereference handle_dereference;
    return ObjectRef(broker(), handle(object()->RawFastPropertyAt(index),
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
        handle(object()->boilerplate(), broker()->isolate()));
  }
  return data()->AsAllocationSite()->IsFastLiteral();
}

void JSObjectRef::EnsureElementsTenured() {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleAllocation allow_handle_allocation;
    AllowHandleDereference allow_handle_dereference;
    AllowHeapAllocation allow_heap_allocation;

    Handle<FixedArrayBase> object_elements = elements().object();
    if (ObjectInYoungGeneration(*object_elements)) {
      // If we would like to pretenure a fixed cow array, we must ensure that
      // the array is already in old space, otherwise we'll create too many
      // old-to-new-space pointers (overflowing the store buffer).
      object_elements =
          broker()->isolate()->factory()->CopyAndTenureFixedCOWArray(
              Handle<FixedArray>::cast(object_elements));
      object()->set_elements(*object_elements);
    }
    return;
  }
  CHECK(data()->AsJSObject()->cow_or_empty_elements_tenured());
}

FieldIndex MapRef::GetFieldIndexFor(int descriptor_index) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference allow_handle_dereference;
    return FieldIndex::ForDescriptor(*object(), descriptor_index);
  }
  DescriptorArrayData* descriptors = data()->AsMap()->instance_descriptors();
  return descriptors->contents().at(descriptor_index).field_index;
}

int MapRef::GetInObjectPropertyOffset(int i) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference allow_handle_dereference;
    return object()->GetInObjectPropertyOffset(i);
  }
  return (GetInObjectPropertiesStartInWords() + i) * kTaggedSize;
}

PropertyDetails MapRef::GetPropertyDetails(int descriptor_index) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference allow_handle_dereference;
    return object()->instance_descriptors()->GetDetails(descriptor_index);
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
        handle(object()->instance_descriptors()->GetKey(descriptor_index),
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

bool MapRef::IsPrimitiveMap() const {
  return instance_type() <= LAST_PRIMITIVE_TYPE;
}

MapRef MapRef::FindFieldOwner(int descriptor_index) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleAllocation handle_allocation;
    AllowHandleDereference allow_handle_dereference;
    Handle<Map> owner(
        object()->FindFieldOwner(broker()->isolate(), descriptor_index),
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
        object()->instance_descriptors()->GetFieldType(descriptor_index),
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
    return object()->IsUnboxedDoubleField(
        FieldIndex::ForDescriptor(*object(), descriptor_index));
  }
  DescriptorArrayData* descriptors = data()->AsMap()->instance_descriptors();
  return descriptors->contents().at(descriptor_index).is_unboxed_double_field;
}

uint16_t StringRef::GetFirstChar() {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference allow_handle_dereference;
    return object()->Get(0);
  }
  return data()->AsString()->first_char();
}

base::Optional<double> StringRef::ToNumber() {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference allow_handle_dereference;
    AllowHandleAllocation allow_handle_allocation;
    AllowHeapAllocation allow_heap_allocation;
    int flags = ALLOW_HEX | ALLOW_OCTAL | ALLOW_BINARY;
    return StringToDouble(broker()->isolate(), object(), flags);
  }
  return data()->AsString()->to_number();
}

ObjectRef FixedArrayRef::get(int i) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleAllocation handle_allocation;
    AllowHandleDereference allow_handle_dereference;
    return ObjectRef(broker(), handle(object()->get(i), broker()->isolate()));
  }
  return ObjectRef(broker(), data()->AsFixedArray()->Get(i));
}

bool FixedDoubleArrayRef::is_the_hole(int i) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference allow_handle_dereference;
    return object()->is_the_hole(i);
  }
  return data()->AsFixedDoubleArray()->Get(i).is_hole_nan();
}

double FixedDoubleArrayRef::get_scalar(int i) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference allow_handle_dereference;
    return object()->get_scalar(i);
  }
  CHECK(!data()->AsFixedDoubleArray()->Get(i).is_hole_nan());
  return data()->AsFixedDoubleArray()->Get(i).get_scalar();
}

#define IF_BROKER_DISABLED_ACCESS_HANDLE_C(holder, name) \
  if (broker()->mode() == JSHeapBroker::kDisabled) {     \
    AllowHandleAllocation handle_allocation;             \
    AllowHandleDereference allow_handle_dereference;     \
    return object()->name();                             \
  }

#define IF_BROKER_DISABLED_ACCESS_HANDLE(holder, result, name)         \
  if (broker()->mode() == JSHeapBroker::kDisabled) {                   \
    AllowHandleAllocation handle_allocation;                           \
    AllowHandleDereference allow_handle_dereference;                   \
    return result##Ref(broker(),                                       \
                       handle(object()->name(), broker()->isolate())); \
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
BIMODAL_ACCESSOR_C(AllocationSite, AllocationType, GetAllocationType)

BIMODAL_ACCESSOR_C(BytecodeArray, int, register_count)

BIMODAL_ACCESSOR(Cell, Object, value)

BIMODAL_ACCESSOR(HeapObject, Map, map)

BIMODAL_ACCESSOR(JSArray, Object, length)

BIMODAL_ACCESSOR(JSBoundFunction, Object, bound_target_function)
BIMODAL_ACCESSOR(JSBoundFunction, Object, bound_this)
BIMODAL_ACCESSOR(JSBoundFunction, FixedArray, bound_arguments)

BIMODAL_ACCESSOR_C(JSDataView, size_t, byte_length)
BIMODAL_ACCESSOR_C(JSDataView, size_t, byte_offset)

BIMODAL_ACCESSOR_C(JSFunction, bool, has_feedback_vector)
BIMODAL_ACCESSOR_C(JSFunction, bool, has_initial_map)
BIMODAL_ACCESSOR_C(JSFunction, bool, has_prototype)
BIMODAL_ACCESSOR_C(JSFunction, bool, PrototypeRequiresRuntimeLookup)
BIMODAL_ACCESSOR(JSFunction, Context, context)
BIMODAL_ACCESSOR(JSFunction, NativeContext, native_context)
BIMODAL_ACCESSOR(JSFunction, Map, initial_map)
BIMODAL_ACCESSOR(JSFunction, Object, prototype)
BIMODAL_ACCESSOR(JSFunction, SharedFunctionInfo, shared)
BIMODAL_ACCESSOR(JSFunction, FeedbackVector, feedback_vector)

BIMODAL_ACCESSOR_C(JSTypedArray, bool, is_on_heap)
BIMODAL_ACCESSOR_C(JSTypedArray, size_t, length_value)
BIMODAL_ACCESSOR(JSTypedArray, HeapObject, buffer)

BIMODAL_ACCESSOR_B(Map, bit_field2, elements_kind, Map::ElementsKindBits)
BIMODAL_ACCESSOR_B(Map, bit_field2, is_extensible, Map::IsExtensibleBit)
BIMODAL_ACCESSOR_B(Map, bit_field3, is_deprecated, Map::IsDeprecatedBit)
BIMODAL_ACCESSOR_B(Map, bit_field3, is_dictionary_map, Map::IsDictionaryMapBit)
BIMODAL_ACCESSOR_B(Map, bit_field3, NumberOfOwnDescriptors,
                   Map::NumberOfOwnDescriptorsBits)
BIMODAL_ACCESSOR_B(Map, bit_field3, has_hidden_prototype,
                   Map::HasHiddenPrototypeBit)
BIMODAL_ACCESSOR_B(Map, bit_field3, is_migration_target,
                   Map::IsMigrationTargetBit)
BIMODAL_ACCESSOR_B(Map, bit_field, has_prototype_slot, Map::HasPrototypeSlotBit)
BIMODAL_ACCESSOR_B(Map, bit_field, is_access_check_needed,
                   Map::IsAccessCheckNeededBit)
BIMODAL_ACCESSOR_B(Map, bit_field, is_callable, Map::IsCallableBit)
BIMODAL_ACCESSOR_B(Map, bit_field, has_indexed_interceptor,
                   Map::HasIndexedInterceptorBit)
BIMODAL_ACCESSOR_B(Map, bit_field, is_constructor, Map::IsConstructorBit)
BIMODAL_ACCESSOR_B(Map, bit_field, is_undetectable, Map::IsUndetectableBit)
BIMODAL_ACCESSOR_C(Map, int, instance_size)
BIMODAL_ACCESSOR_C(Map, int, NextFreePropertyIndex)
BIMODAL_ACCESSOR_C(Map, int, UnusedPropertyFields)
BIMODAL_ACCESSOR(Map, HeapObject, prototype)
BIMODAL_ACCESSOR_C(Map, InstanceType, instance_type)
BIMODAL_ACCESSOR(Map, Object, GetConstructor)
BIMODAL_ACCESSOR(Map, HeapObject, GetBackPointer)

#define DEF_NATIVE_CONTEXT_ACCESSOR(type, name) \
  BIMODAL_ACCESSOR(NativeContext, type, name)
BROKER_NATIVE_CONTEXT_FIELDS(DEF_NATIVE_CONTEXT_ACCESSOR)
#undef DEF_NATIVE_CONTEXT_ACCESSOR

BIMODAL_ACCESSOR(PropertyCell, Object, value)
BIMODAL_ACCESSOR_C(PropertyCell, PropertyDetails, property_details)

BIMODAL_ACCESSOR(FunctionTemplateInfo, Object, call_code)

BIMODAL_ACCESSOR(CallHandlerInfo, Object, data)

BIMODAL_ACCESSOR_C(SharedFunctionInfo, int, builtin_id)
BIMODAL_ACCESSOR(SharedFunctionInfo, BytecodeArray, GetBytecodeArray)
#define DEF_SFI_ACCESSOR(type, name) \
  BIMODAL_ACCESSOR_C(SharedFunctionInfo, type, name)
BROKER_SFI_FIELDS(DEF_SFI_ACCESSOR)
#undef DEF_SFI_ACCESSOR

BIMODAL_ACCESSOR_C(String, int, length)

void* JSTypedArrayRef::elements_external_pointer() const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference allow_handle_dereference;
    return FixedTypedArrayBase::cast(object()->elements())->external_pointer();
  }
  return data()->AsJSTypedArray()->elements_external_pointer();
}

bool MapRef::IsInobjectSlackTrackingInProgress() const {
  IF_BROKER_DISABLED_ACCESS_HANDLE_C(Map, IsInobjectSlackTrackingInProgress);
  return Map::ConstructionCounterBits::decode(data()->AsMap()->bit_field3()) !=
         Map::kNoSlackTracking;
}

int MapRef::constructor_function_index() const {
  IF_BROKER_DISABLED_ACCESS_HANDLE_C(Map, GetConstructorFunctionIndex);
  CHECK(IsPrimitiveMap());
  return data()->AsMap()->constructor_function_index();
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

Address CallHandlerInfoRef::callback() const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    return v8::ToCData<Address>(object()->callback());
  }
  return HeapObjectRef::data()->AsCallHandlerInfo()->callback();
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

base::Optional<JSFunctionRef> NativeContextRef::GetConstructorFunction(
    const MapRef& map) const {
  CHECK(map.IsPrimitiveMap());
  switch (map.constructor_function_index()) {
    case Map::kNoConstructorFunctionIndex:
      return base::nullopt;
    case Context::BIGINT_FUNCTION_INDEX:
      return bigint_function();
    case Context::BOOLEAN_FUNCTION_INDEX:
      return boolean_function();
    case Context::NUMBER_FUNCTION_INDEX:
      return number_function();
    case Context::STRING_FUNCTION_INDEX:
      return string_function();
    case Context::SYMBOL_FUNCTION_INDEX:
      return symbol_function();
    default:
      UNREACHABLE();
  }
}

bool ObjectRef::IsNullOrUndefined() const {
  if (IsSmi()) return false;
  OddballType type = AsHeapObject().map().oddball_type();
  return type == OddballType::kNull || type == OddballType::kUndefined;
}

bool ObjectRef::BooleanValue() const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference allow_handle_dereference;
    return object()->BooleanValue(broker()->isolate());
  }
  return IsSmi() ? (AsSmi() != 0) : data()->AsHeapObject()->boolean_value();
}

Maybe<double> ObjectRef::OddballToNumber() const {
  OddballType type = AsHeapObject().map().oddball_type();

  switch (type) {
    case OddballType::kBoolean: {
      ObjectRef true_ref(broker(),
                         broker()->isolate()->factory()->true_value());
      return this->equals(true_ref) ? Just(1.0) : Just(0.0);
      break;
    }
    case OddballType::kUndefined: {
      return Just(std::numeric_limits<double>::quiet_NaN());
      break;
    }
    case OddballType::kNull: {
      return Just(0.0);
      break;
    }
    default: {
      return Nothing<double>();
      break;
    }
  }
}

base::Optional<ObjectRef> ObjectRef::GetOwnConstantElement(
    uint32_t index, bool serialize) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    return (IsJSObject() || IsString())
               ? GetOwnElementFromHeap(broker(), object(), index, true)
               : base::nullopt;
  }
  ObjectData* element = nullptr;
  if (IsJSObject()) {
    element =
        data()->AsJSObject()->GetOwnConstantElement(broker(), index, serialize);
  } else if (IsString()) {
    element = data()->AsString()->GetCharAsString(broker(), index, serialize);
  }
  if (element == nullptr) return base::nullopt;
  return ObjectRef(broker(), element);
}

base::Optional<ObjectRef> JSArrayRef::GetOwnCowElement(uint32_t index,
                                                       bool serialize) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    if (!object()->elements()->IsCowArray()) return base::nullopt;
    return GetOwnElementFromHeap(broker(), object(), index, false);
  }

  if (serialize) {
    data()->AsJSObject()->SerializeElements(broker());
  } else if (!data()->AsJSObject()->serialized_elements()) {
    TRACE(broker(), "'elements' on data " << this);
    return base::nullopt;
  }
  if (!elements().map().IsFixedCowArrayMap()) return base::nullopt;

  ObjectData* element =
      data()->AsJSArray()->GetOwnElement(broker(), index, serialize);
  if (element == nullptr) return base::nullopt;
  return ObjectRef(broker(), element);
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
    return CellRef(broker(),
                   handle(object()->GetCell(cell_index), broker()->isolate()));
  }
  return CellRef(broker(), data()->AsModule()->GetCell(cell_index));
}

ObjectRef::ObjectRef(JSHeapBroker* broker, Handle<Object> object)
    : broker_(broker) {
  switch (broker->mode()) {
    case JSHeapBroker::kSerialized:
      data_ = broker->GetData(object);
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
  CHECK_WITH_MSG(data_ != nullptr, "Object is not known to the heap broker");
}

namespace {
OddballType GetOddballType(Isolate* isolate, Map map) {
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
    Map map = Handle<HeapObject>::cast(object())->map();
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
    return JSObjectRef(broker(),
                       handle(object()->boilerplate(), broker()->isolate()));
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
    return FixedArrayBaseRef(broker(),
                             handle(object()->elements(), broker()->isolate()));
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

bool NameRef::IsUniqueName() const {
  // Must match Name::IsUniqueName.
  return IsInternalizedString() || IsSymbol();
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

#define DEF_OBJECT_GETTER(T)                                                 \
  Handle<T> T##Ref::object() const {                                         \
    return Handle<T>(reinterpret_cast<Address*>(data_->object().address())); \
  }
HEAP_BROKER_OBJECT_LIST(DEF_OBJECT_GETTER)
#undef DEF_OBJECT_GETTER

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
  TRACE_MISSING(broker, "data in function " << function << " at line " << line);
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

#define SERIALIZE_MEMBER(type, name)                                       \
  DCHECK_NULL(name##_);                                                    \
  name##_ = broker->GetOrCreateData(context->name())->As##type();          \
  if (name##_->IsJSFunction()) name##_->AsJSFunction()->Serialize(broker); \
  if (name##_->IsMap()) name##_->AsMap()->SerializeConstructor(broker);
  BROKER_COMPULSORY_NATIVE_CONTEXT_FIELDS(SERIALIZE_MEMBER)
  if (!broker->isolate()->bootstrapper()->IsActive()) {
    BROKER_OPTIONAL_NATIVE_CONTEXT_FIELDS(SERIALIZE_MEMBER)
  }
#undef SERIALIZE_MEMBER

  bound_function_with_constructor_map_->SerializePrototype(broker);
  bound_function_without_constructor_map_->SerializePrototype(broker);

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

bool JSFunctionRef::serialized() const {
  CHECK_NE(broker()->mode(), JSHeapBroker::kDisabled);
  return data()->AsJSFunction()->serialized();
}

bool JSFunctionRef::IsSerializedForCompilation() const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    return handle(object()->shared(), broker()->isolate())->HasBytecodeArray();
  }

  // We get a crash if we try to access the shared() getter without
  // checking for `serialized` first. Also it's possible to have a
  // JSFunctionRef without a feedback vector.
  return serialized() && has_feedback_vector() &&
         shared().IsSerializedForCompilation(feedback_vector());
}

void SharedFunctionInfoRef::SetSerializedForCompilation(
    FeedbackVectorRef feedback) {
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsSharedFunctionInfo()->SetSerializedForCompilation(broker(),
                                                              feedback);
}

bool SharedFunctionInfoRef::IsSerializedForCompilation(
    FeedbackVectorRef feedback) const {
  CHECK_NE(broker()->mode(), JSHeapBroker::kDisabled);
  return data()->AsSharedFunctionInfo()->IsSerializedForCompilation(feedback);
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

void MapRef::SerializeBackPointer() {
  if (broker()->mode() == JSHeapBroker::kDisabled) return;
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsMap()->SerializeBackPointer(broker());
}

void MapRef::SerializePrototype() {
  if (broker()->mode() == JSHeapBroker::kDisabled) return;
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsMap()->SerializePrototype(broker());
}

bool MapRef::serialized_prototype() const {
  CHECK_NE(broker()->mode(), JSHeapBroker::kDisabled);
  return data()->AsMap()->serialized_prototype();
}

void ModuleRef::Serialize() {
  if (broker()->mode() == JSHeapBroker::kDisabled) return;
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsModule()->Serialize(broker());
}

void ContextRef::SerializeContextChain() {
  if (broker()->mode() == JSHeapBroker::kDisabled) return;
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsContext()->SerializeContextChain(broker());
}

void ContextRef::SerializeSlot(int index) {
  if (broker()->mode() == JSHeapBroker::kDisabled) return;
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsContext()->SerializeSlot(broker(), index);
}

void NativeContextRef::Serialize() {
  if (broker()->mode() == JSHeapBroker::kDisabled) return;
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsNativeContext()->Serialize(broker());
}

void JSTypedArrayRef::Serialize() {
  if (broker()->mode() == JSHeapBroker::kDisabled) return;
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsJSTypedArray()->Serialize(broker());
}

bool JSTypedArrayRef::serialized() const {
  CHECK_NE(broker()->mode(), JSHeapBroker::kDisabled);
  return data()->AsJSTypedArray()->serialized();
}

void JSBoundFunctionRef::Serialize() {
  if (broker()->mode() == JSHeapBroker::kDisabled) return;
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsJSBoundFunction()->Serialize(broker());
}

void PropertyCellRef::Serialize() {
  if (broker()->mode() == JSHeapBroker::kDisabled) return;
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsPropertyCell()->Serialize(broker());
}

void FunctionTemplateInfoRef::Serialize() {
  if (broker()->mode() == JSHeapBroker::kDisabled) return;
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsFunctionTemplateInfo()->Serialize(broker());
}

base::Optional<PropertyCellRef> JSGlobalProxyRef::GetPropertyCell(
    NameRef const& name, bool serialize) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    return GetPropertyCellFromHeap(broker(), name.object());
  }
  PropertyCellData* property_cell_data =
      data()->AsJSGlobalProxy()->GetPropertyCell(
          broker(), name.data()->AsName(), serialize);
  if (property_cell_data == nullptr) return base::nullopt;
  return PropertyCellRef(broker(), property_cell_data);
}

bool CanInlineElementAccess(MapRef const& map) {
  if (!map.IsJSObjectMap()) return false;
  if (map.is_access_check_needed()) return false;
  if (map.has_indexed_interceptor()) return false;
  ElementsKind const elements_kind = map.elements_kind();
  if (IsFastElementsKind(elements_kind)) return true;
  if (IsFixedTypedArrayElementsKind(elements_kind) &&
      elements_kind != BIGUINT64_ELEMENTS &&
      elements_kind != BIGINT64_ELEMENTS) {
    return true;
  }
  return false;
}

GlobalAccessFeedback::GlobalAccessFeedback(PropertyCellRef cell)
    : ProcessedFeedback(kGlobalAccess),
      cell_or_context_(cell),
      index_and_immutable_(0 /* doesn't matter */) {}

GlobalAccessFeedback::GlobalAccessFeedback(ContextRef script_context,
                                           int slot_index, bool immutable)
    : ProcessedFeedback(kGlobalAccess),
      cell_or_context_(script_context),
      index_and_immutable_(FeedbackNexus::SlotIndexBits::encode(slot_index) |
                           FeedbackNexus::ImmutabilityBit::encode(immutable)) {
  DCHECK_EQ(this->slot_index(), slot_index);
  DCHECK_EQ(this->immutable(), immutable);
}

bool GlobalAccessFeedback::IsPropertyCell() const {
  return cell_or_context_.IsPropertyCell();
}
PropertyCellRef GlobalAccessFeedback::property_cell() const {
  DCHECK(IsPropertyCell());
  return cell_or_context_.AsPropertyCell();
}
ContextRef GlobalAccessFeedback::script_context() const {
  DCHECK(IsScriptContextSlot());
  return cell_or_context_.AsContext();
}
int GlobalAccessFeedback::slot_index() const {
  CHECK(IsScriptContextSlot());
  return FeedbackNexus::SlotIndexBits::decode(index_and_immutable_);
}
bool GlobalAccessFeedback::immutable() const {
  CHECK(IsScriptContextSlot());
  return FeedbackNexus::ImmutabilityBit::decode(index_and_immutable_);
}

base::Optional<ObjectRef> GlobalAccessFeedback::GetConstantHint() const {
  if (IsScriptContextSlot()) {
    if (immutable()) return script_context().get(slot_index());
  } else {
    return property_cell().value();
  }
  return {};
}

ElementAccessFeedback::ElementAccessFeedback(Zone* zone)
    : ProcessedFeedback(kElementAccess),
      receiver_maps(zone),
      transitions(zone) {}

ElementAccessFeedback::MapIterator::MapIterator(
    ElementAccessFeedback const& processed, JSHeapBroker* broker)
    : processed_(processed), broker_(broker) {
  CHECK_LT(processed.receiver_maps.size(),
           std::numeric_limits<size_t>::max() - processed.transitions.size());
}

bool ElementAccessFeedback::MapIterator::done() const {
  return index_ >=
         processed_.receiver_maps.size() + processed_.transitions.size();
}

void ElementAccessFeedback::MapIterator::advance() { index_++; }

MapRef ElementAccessFeedback::MapIterator::current() const {
  CHECK(!done());
  size_t receiver_maps_size = processed_.receiver_maps.size();
  Handle<Map> map;
  if (index_ < receiver_maps_size) {
    map = processed_.receiver_maps[index_];
  } else {
    map = processed_.transitions[index_ - receiver_maps_size].first;
  }
  return MapRef(broker_, map);
}

ElementAccessFeedback::MapIterator ElementAccessFeedback::all_maps(
    JSHeapBroker* broker) const {
  return MapIterator(*this, broker);
}

FeedbackSource::FeedbackSource(FeedbackNexus const& nexus)
    : vector(nexus.vector_handle()), slot(nexus.slot()) {}

FeedbackSource::FeedbackSource(VectorSlotPair const& pair)
    : vector(pair.vector()), slot(pair.slot()) {}

void JSHeapBroker::SetFeedback(FeedbackSource const& source,
                               ProcessedFeedback const* feedback) {
  auto insertion = feedback_.insert({source, feedback});
  CHECK(insertion.second);
}

bool JSHeapBroker::HasFeedback(FeedbackSource const& source) const {
  return feedback_.find(source) != feedback_.end();
}

ProcessedFeedback const* JSHeapBroker::GetFeedback(
    FeedbackSource const& source) const {
  auto it = feedback_.find(source);
  CHECK_NE(it, feedback_.end());
  return it->second;
}

ElementAccessFeedback const* JSHeapBroker::GetElementAccessFeedback(
    FeedbackSource const& source) const {
  ProcessedFeedback const* feedback = GetFeedback(source);
  if (feedback == nullptr) return nullptr;
  CHECK_EQ(feedback->kind(), ProcessedFeedback::kElementAccess);
  return static_cast<ElementAccessFeedback const*>(feedback);
}

GlobalAccessFeedback const* JSHeapBroker::GetGlobalAccessFeedback(
    FeedbackSource const& source) const {
  ProcessedFeedback const* feedback = GetFeedback(source);
  if (feedback == nullptr) return nullptr;
  CHECK_EQ(feedback->kind(), ProcessedFeedback::kGlobalAccess);
  return static_cast<GlobalAccessFeedback const*>(feedback);
}

ElementAccessFeedback const* JSHeapBroker::ProcessFeedbackMapsForElementAccess(
    MapHandles const& maps) {
  // Collect possible transition targets.
  MapHandles possible_transition_targets;
  possible_transition_targets.reserve(maps.size());
  for (Handle<Map> map : maps) {
    if (CanInlineElementAccess(MapRef(this, map)) &&
        IsFastElementsKind(map->elements_kind()) &&
        GetInitialFastElementsKind() != map->elements_kind()) {
      possible_transition_targets.push_back(map);
    }
  }

  if (maps.empty()) return nullptr;

  ElementAccessFeedback* result = new (zone()) ElementAccessFeedback(zone());

  // Separate the actual receiver maps and the possible transition sources.
  for (Handle<Map> map : maps) {
    // Don't generate elements kind transitions from stable maps.
    Map transition_target = map->is_stable()
                                ? Map()
                                : map->FindElementsKindTransitionedMap(
                                      isolate(), possible_transition_targets);
    if (transition_target.is_null()) {
      result->receiver_maps.push_back(map);
    } else {
      result->transitions.emplace_back(map,
                                       handle(transition_target, isolate()));
    }
  }

#ifdef ENABLE_SLOW_DCHECKS
  // No transition sources appear in {receiver_maps}.
  // All transition targets appear in {receiver_maps}.
  for (auto& transition : result->transitions) {
    CHECK(std::none_of(
        result->receiver_maps.cbegin(), result->receiver_maps.cend(),
        [&](Handle<Map> map) { return map.equals(transition.first); }));
    CHECK(std::any_of(
        result->receiver_maps.cbegin(), result->receiver_maps.cend(),
        [&](Handle<Map> map) { return map.equals(transition.second); }));
  }
#endif
  CHECK(!result->receiver_maps.empty());

  return result;
}

GlobalAccessFeedback const* JSHeapBroker::ProcessFeedbackForGlobalAccess(
    FeedbackSource const& source) {
  FeedbackNexus nexus(source.vector, source.slot);
  DCHECK(nexus.kind() == FeedbackSlotKind::kLoadGlobalInsideTypeof ||
         nexus.kind() == FeedbackSlotKind::kLoadGlobalNotInsideTypeof ||
         nexus.kind() == FeedbackSlotKind::kStoreGlobalSloppy ||
         nexus.kind() == FeedbackSlotKind::kStoreGlobalStrict);
  if (nexus.ic_state() != MONOMORPHIC || nexus.GetFeedback()->IsCleared()) {
    return nullptr;
  }

  Handle<Object> feedback_value(nexus.GetFeedback()->GetHeapObjectOrSmi(),
                                isolate());

  if (feedback_value->IsSmi()) {
    // The wanted name belongs to a script-scope variable and the feedback tells
    // us where to find its value.
    int number = feedback_value->Number();
    int const script_context_index =
        FeedbackNexus::ContextIndexBits::decode(number);
    int const context_slot_index = FeedbackNexus::SlotIndexBits::decode(number);
    bool const immutable = FeedbackNexus::ImmutabilityBit::decode(number);
    Handle<Context> context = ScriptContextTable::GetContext(
        isolate(), native_context().script_context_table().object(),
        script_context_index);
    {
      ObjectRef contents(this,
                         handle(context->get(context_slot_index), isolate()));
      CHECK(!contents.equals(
          ObjectRef(this, isolate()->factory()->the_hole_value())));
    }
    ContextRef context_ref(this, context);
    if (immutable) {
      context_ref.SerializeSlot(context_slot_index);
    }
    return new (zone())
        GlobalAccessFeedback(context_ref, context_slot_index, immutable);
  }

  CHECK(feedback_value->IsPropertyCell());
  // The wanted name belongs (or did belong) to a property on the global
  // object and the feedback is the cell holding its value.
  PropertyCellRef cell(this, Handle<PropertyCell>::cast(feedback_value));
  cell.Serialize();
  return new (zone()) GlobalAccessFeedback(cell);
}

std::ostream& operator<<(std::ostream& os, const ObjectRef& ref) {
  return os << ref.data();
}

#undef BIMODAL_ACCESSOR
#undef BIMODAL_ACCESSOR_B
#undef BIMODAL_ACCESSOR_C
#undef IF_BROKER_DISABLED_ACCESS_HANDLE
#undef IF_BROKER_DISABLED_ACCESS_HANDLE_C
#undef TRACE
#undef TRACE_MISSING

}  // namespace compiler
}  // namespace internal
}  // namespace v8
