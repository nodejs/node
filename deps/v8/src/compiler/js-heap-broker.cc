// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-heap-broker.h"
#include "src/compiler/heap-refs.h"

#ifdef ENABLE_SLOW_DCHECKS
#include <algorithm>
#endif

#include "src/api/api-inl.h"
#include "src/ast/modules.h"
#include "src/codegen/code-factory.h"
#include "src/compiler/access-info.h"
#include "src/compiler/bytecode-analysis.h"
#include "src/compiler/graph-reducer.h"
#include "src/compiler/per-isolate-compiler-cache.h"
#include "src/execution/protectors-inl.h"
#include "src/init/bootstrapper.h"
#include "src/objects/allocation-site-inl.h"
#include "src/objects/api-callbacks.h"
#include "src/objects/cell-inl.h"
#include "src/objects/heap-number-inl.h"
#include "src/objects/instance-type-inl.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/js-regexp-inl.h"
#include "src/objects/literal-objects-inl.h"
#include "src/objects/module-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/template-objects-inl.h"
#include "src/objects/templates.h"
#include "src/utils/boxed-float.h"
#include "src/utils/utils.h"

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

#ifdef DEBUG
  enum class Usage{kUnused, kOnlyIdentityUsed, kDataUsed};
  mutable Usage used_status = Usage::kUnused;
#endif  // DEBUG

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

// TODO(mslekova): Once we have real-world usage data, we might want to
// reimplement this as sorted vector instead, to reduce the memory overhead.
typedef ZoneMap<MapData*, HolderLookupResult> KnownReceiversMap;

class FunctionTemplateInfoData : public HeapObjectData {
 public:
  FunctionTemplateInfoData(JSHeapBroker* broker, ObjectData** storage,
                           Handle<FunctionTemplateInfo> object);

  bool is_signature_undefined() const { return is_signature_undefined_; }
  bool accept_any_receiver() const { return accept_any_receiver_; }
  bool has_call_code() const { return has_call_code_; }

  void SerializeCallCode(JSHeapBroker* broker);
  CallHandlerInfoData* call_code() const { return call_code_; }
  KnownReceiversMap& known_receivers() { return known_receivers_; }

 private:
  bool serialized_call_code_ = false;
  CallHandlerInfoData* call_code_ = nullptr;
  bool is_signature_undefined_ = false;
  bool accept_any_receiver_ = false;
  bool has_call_code_ = false;

  KnownReceiversMap known_receivers_;
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
    : HeapObjectData(broker, storage, object),
      known_receivers_(broker->zone()) {
  auto function_template_info = Handle<FunctionTemplateInfo>::cast(object);
  is_signature_undefined_ =
      function_template_info->signature().IsUndefined(broker->isolate());
  accept_any_receiver_ = function_template_info->accept_any_receiver();

  CallOptimization call_optimization(broker->isolate(), object);
  has_call_code_ = call_optimization.is_simple_api_call();
}

CallHandlerInfoData::CallHandlerInfoData(JSHeapBroker* broker,
                                         ObjectData** storage,
                                         Handle<CallHandlerInfo> object)
    : HeapObjectData(broker, storage, object),
      callback_(v8::ToCData<Address>(object->callback())) {}

void JSHeapBroker::IncrementTracingIndentation() { ++trace_indentation_; }

void JSHeapBroker::DecrementTracingIndentation() { --trace_indentation_; }

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

void FunctionTemplateInfoData::SerializeCallCode(JSHeapBroker* broker) {
  if (serialized_call_code_) return;
  serialized_call_code_ = true;

  TraceScope tracer(broker, this,
                    "FunctionTemplateInfoData::SerializeCallCode");
  auto function_template_info = Handle<FunctionTemplateInfo>::cast(object());
  DCHECK_NULL(call_code_);
  call_code_ = broker->GetOrCreateData(function_template_info->call_code())
                   ->AsCallHandlerInfo();
  call_code_->Serialize(broker);
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
  uint64_t AsBitsOfDouble() const {
    CHECK(IsDouble());
    return number_bits_;
  }
  double AsDouble() const {
    CHECK(IsDouble());
    return bit_cast<double>(number_bits_);
  }

  bool IsObject() const { return object_ != nullptr; }
  ObjectData* AsObject() const {
    CHECK(IsObject());
    return object_;
  }

  explicit JSObjectField(uint64_t value_bits) : number_bits_(value_bits) {}
  explicit JSObjectField(ObjectData* value) : object_(value) {}

 private:
  ObjectData* object_ = nullptr;
  uint64_t number_bits_ = 0;
};

class JSReceiverData : public HeapObjectData {
 public:
  JSReceiverData(JSHeapBroker* broker, ObjectData** storage,
                 Handle<JSReceiver> object)
      : HeapObjectData(broker, storage, object) {}
};

class JSObjectData : public JSReceiverData {
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

  MapData* object_create_map(JSHeapBroker* broker) const {  // Can be nullptr.
    if (!serialized_object_create_map_) {
      DCHECK_NULL(object_create_map_);
      TRACE_MISSING(broker, "object_create_map on " << this);
    }
    return object_create_map_;
  }

  ObjectData* GetOwnConstantElement(
      JSHeapBroker* broker, uint32_t index,
      SerializationPolicy policy = SerializationPolicy::kAssumeSerialized);
  ObjectData* GetOwnDataProperty(
      JSHeapBroker* broker, Representation representation,
      FieldIndex field_index,
      SerializationPolicy policy = SerializationPolicy::kAssumeSerialized);

  // This method is only used to assert our invariants.
  bool cow_or_empty_elements_tenured() const;

 private:
  void SerializeRecursiveAsBoilerplate(JSHeapBroker* broker, int max_depths);

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
  // Properties that either:
  // (1) are known to exist directly on the object, or
  // (2) are known not to (possibly they don't exist at all).
  // In case (2), the second pair component is nullptr.
  // For simplicity, this may in theory overlap with inobject_fields_.
  // The keys of the map are the property_index() values of the
  // respective property FieldIndex'es.
  ZoneUnorderedMap<int, ObjectData*> own_properties_;
};

void JSObjectData::SerializeObjectCreateMap(JSHeapBroker* broker) {
  if (serialized_object_create_map_) return;
  serialized_object_create_map_ = true;

  TraceScope tracer(broker, this, "JSObjectData::SerializeObjectCreateMap");
  Handle<JSObject> jsobject = Handle<JSObject>::cast(object());

  if (jsobject->map().is_prototype_map()) {
    Handle<Object> maybe_proto_info(jsobject->map().prototype_info(),
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

ObjectRef GetOwnDataPropertyFromHeap(JSHeapBroker* broker,
                                     Handle<JSObject> receiver,
                                     Representation representation,
                                     FieldIndex field_index) {
  Handle<Object> constant =
      JSObject::FastPropertyAt(receiver, representation, field_index);
  return ObjectRef(broker, constant);
}

}  // namespace

ObjectData* JSObjectData::GetOwnConstantElement(JSHeapBroker* broker,
                                                uint32_t index,
                                                SerializationPolicy policy) {
  for (auto const& p : own_constant_elements_) {
    if (p.first == index) return p.second;
  }

  if (policy == SerializationPolicy::kAssumeSerialized) {
    TRACE_MISSING(broker, "knowledge about index " << index << " on " << this);
    return nullptr;
  }

  base::Optional<ObjectRef> element =
      GetOwnElementFromHeap(broker, object(), index, true);
  ObjectData* result = element.has_value() ? element->data() : nullptr;
  own_constant_elements_.push_back({index, result});
  return result;
}

ObjectData* JSObjectData::GetOwnDataProperty(JSHeapBroker* broker,
                                             Representation representation,
                                             FieldIndex field_index,
                                             SerializationPolicy policy) {
  auto p = own_properties_.find(field_index.property_index());
  if (p != own_properties_.end()) return p->second;

  if (policy == SerializationPolicy::kAssumeSerialized) {
    TRACE_MISSING(broker, "knowledge about property with index "
                              << field_index.property_index() << " on "
                              << this);
    return nullptr;
  }

  ObjectRef property = GetOwnDataPropertyFromHeap(
      broker, Handle<JSObject>::cast(object()), representation, field_index);
  ObjectData* result(property.data());
  own_properties_.insert(std::make_pair(field_index.property_index(), result));
  return result;
}

class JSTypedArrayData : public JSObjectData {
 public:
  JSTypedArrayData(JSHeapBroker* broker, ObjectData** storage,
                   Handle<JSTypedArray> object);

  bool is_on_heap() const { return is_on_heap_; }
  size_t length() const { return length_; }
  void* data_ptr() const { return data_ptr_; }

  void Serialize(JSHeapBroker* broker);
  bool serialized() const { return serialized_; }

  HeapObjectData* buffer() const { return buffer_; }

 private:
  bool const is_on_heap_;
  size_t const length_;
  void* const data_ptr_;

  bool serialized_ = false;
  HeapObjectData* buffer_ = nullptr;
};

JSTypedArrayData::JSTypedArrayData(JSHeapBroker* broker, ObjectData** storage,
                                   Handle<JSTypedArray> object)
    : JSObjectData(broker, storage, object),
      is_on_heap_(object->is_on_heap()),
      length_(object->length()),
      data_ptr_(object->DataPtr()) {}

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

class ArrayBoilerplateDescriptionData : public HeapObjectData {
 public:
  ArrayBoilerplateDescriptionData(JSHeapBroker* broker, ObjectData** storage,
                                  Handle<ArrayBoilerplateDescription> object)
      : HeapObjectData(broker, storage, object),
        constants_elements_length_(object->constant_elements().length()) {}

  int constants_elements_length() const { return constants_elements_length_; }

 private:
  int const constants_elements_length_;
};

class ObjectBoilerplateDescriptionData : public HeapObjectData {
 public:
  ObjectBoilerplateDescriptionData(JSHeapBroker* broker, ObjectData** storage,
                                   Handle<ObjectBoilerplateDescription> object)
      : HeapObjectData(broker, storage, object), size_(object->size()) {}

  int size() const { return size_; }

 private:
  int const size_;
};

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
  bool serialized() const { return serialized_; }

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

class ContextData : public HeapObjectData {
 public:
  ContextData(JSHeapBroker* broker, ObjectData** storage,
              Handle<Context> object);

  // {previous} will return the closest valid context possible to desired
  // {depth}, decrementing {depth} for each previous link successfully followed.
  ContextData* previous(
      JSHeapBroker* broker, size_t* depth,
      SerializationPolicy policy = SerializationPolicy::kAssumeSerialized);

  // Returns nullptr if the slot index isn't valid or wasn't serialized,
  // unless {policy} is {kSerializeIfNeeded}.
  ObjectData* GetSlot(
      JSHeapBroker* broker, int index,
      SerializationPolicy policy = SerializationPolicy::kAssumeSerialized);

 private:
  ZoneMap<int, ObjectData*> slots_;
  ContextData* previous_ = nullptr;
};

ContextData::ContextData(JSHeapBroker* broker, ObjectData** storage,
                         Handle<Context> object)
    : HeapObjectData(broker, storage, object), slots_(broker->zone()) {}

ContextData* ContextData::previous(JSHeapBroker* broker, size_t* depth,
                                   SerializationPolicy policy) {
  if (*depth == 0) return this;

  if (policy == SerializationPolicy::kSerializeIfNeeded &&
      previous_ == nullptr) {
    TraceScope tracer(broker, this, "ContextData::previous");
    Handle<Context> context = Handle<Context>::cast(object());
    Object prev = context->unchecked_previous();
    if (prev.IsContext()) {
      previous_ = broker->GetOrCreateData(prev)->AsContext();
    }
  }

  if (previous_ != nullptr) {
    *depth = *depth - 1;
    return previous_->previous(broker, depth, policy);
  }
  return this;
}

ObjectData* ContextData::GetSlot(JSHeapBroker* broker, int index,
                                 SerializationPolicy policy) {
  CHECK_GE(index, 0);
  auto search = slots_.find(index);
  if (search != slots_.end()) {
    return search->second;
  }

  if (policy == SerializationPolicy::kSerializeIfNeeded) {
    Handle<Context> context = Handle<Context>::cast(object());
    if (index < context->length()) {
      TraceScope tracer(broker, this, "ContextData::GetSlot");
      TRACE(broker, "Serializing context slot " << index);
      ObjectData* odata = broker->GetOrCreateData(context->get(index));
      slots_.insert(std::make_pair(index, odata));
      return odata;
    }
  }

  return nullptr;
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

  ScopeInfoData* scope_info() const {
    CHECK(serialized_);
    return scope_info_;
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
  ScopeInfoData* scope_info_ = nullptr;
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

  StringData* GetCharAsString(
      JSHeapBroker* broker, uint32_t index,
      SerializationPolicy policy = SerializationPolicy::kAssumeSerialized);

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
                                        SerializationPolicy policy) {
  if (index >= static_cast<uint32_t>(length())) return nullptr;

  for (auto const& p : chars_as_strings_) {
    if (p.first == index) return p.second;
  }

  if (policy == SerializationPolicy::kAssumeSerialized) {
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

  Isolate* const isolate = boilerplate->GetIsolate();

  // Make sure the boilerplate map is not deprecated.
  if (!JSObject::TryMigrateInstance(isolate, boilerplate)) return false;

  // Check for too deep nesting.
  if (max_depth == 0) return false;

  // Check the elements.
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
        boilerplate->property_array().length() == 0)) {
    return false;
  }

  // Check the in-object properties.
  Handle<DescriptorArray> descriptors(boilerplate->map().instance_descriptors(),
                                      isolate);
  for (InternalIndex i : boilerplate->map().IterateOwnDescriptors()) {
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

class AccessorInfoData : public HeapObjectData {
 public:
  AccessorInfoData(JSHeapBroker* broker, ObjectData** storage,
                   Handle<AccessorInfo> object);
};

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

class BigIntData : public HeapObjectData {
 public:
  BigIntData(JSHeapBroker* broker, ObjectData** storage, Handle<BigInt> object)
      : HeapObjectData(broker, storage, object),
        as_uint64_(object->AsUint64(nullptr)) {}

  uint64_t AsUint64() const { return as_uint64_; }

 private:
  const uint64_t as_uint64_;
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
  ObjectData* value = nullptr;
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
  bool is_abandoned_prototype_map() const {
    return is_abandoned_prototype_map_;
  }

  // Extra information.

  void SerializeElementsKindGeneralizations(JSHeapBroker* broker);
  const ZoneVector<MapData*>& elements_kind_generalizations() const {
    CHECK(serialized_elements_kind_generalizations_);
    return elements_kind_generalizations_;
  }

  // Serialize a single (or all) own slot(s) of the descriptor array and recurse
  // on field owner(s).
  void SerializeOwnDescriptor(JSHeapBroker* broker,
                              InternalIndex descriptor_index);
  void SerializeOwnDescriptors(JSHeapBroker* broker);
  ObjectData* GetStrongValue(InternalIndex descriptor_index) const;
  DescriptorArrayData* instance_descriptors() const {
    return instance_descriptors_;
  }

  void SerializeRootMap(JSHeapBroker* broker);
  MapData* FindRootMap() const;

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
  bool const is_abandoned_prototype_map_;

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

  bool serialized_root_map_ = false;
  MapData* root_map_ = nullptr;

  bool serialized_for_element_load_ = false;

  bool serialized_for_element_store_ = false;
};

AccessorInfoData::AccessorInfoData(JSHeapBroker* broker, ObjectData** storage,
                                   Handle<AccessorInfo> object)
    : HeapObjectData(broker, storage, object) {}

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
  // TODO(jkummerow): We could skip the search and hardcode number == 0.
  InternalIndex number = descriptors.Search(*length_string, *jsarray_map);
  DCHECK(number.is_found());
  return descriptors.GetDetails(number).IsReadOnly();
}

bool SupportsFastArrayIteration(Isolate* isolate, Handle<Map> map) {
  return map->instance_type() == JS_ARRAY_TYPE &&
         IsFastElementsKind(map->elements_kind()) &&
         map->prototype().IsJSArray() &&
         isolate->IsAnyInitialArrayPrototype(
             handle(JSArray::cast(map->prototype()), isolate)) &&
         Protectors::IsNoElementsIntact(isolate);
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
      is_abandoned_prototype_map_(object->is_abandoned_prototype_map()),
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

  ZoneMap<int, PropertyDescriptor>& contents() { return contents_; }

 private:
  ZoneMap<int, PropertyDescriptor> contents_;
};

class FeedbackCellData : public HeapObjectData {
 public:
  FeedbackCellData(JSHeapBroker* broker, ObjectData** storage,
                   Handle<FeedbackCell> object);

  HeapObjectData* value() const { return value_; }

 private:
  HeapObjectData* const value_;
};

FeedbackCellData::FeedbackCellData(JSHeapBroker* broker, ObjectData** storage,
                                   Handle<FeedbackCell> object)
    : HeapObjectData(broker, storage, object),
      value_(broker->GetOrCreateData(object->value())->AsHeapObject()) {}

class FeedbackVectorData : public HeapObjectData {
 public:
  FeedbackVectorData(JSHeapBroker* broker, ObjectData** storage,
                     Handle<FeedbackVector> object);

  double invocation_count() const { return invocation_count_; }

  void Serialize(JSHeapBroker* broker);
  FeedbackCellData* GetClosureFeedbackCell(JSHeapBroker* broker,
                                           int index) const;

 private:
  double const invocation_count_;

  bool serialized_ = false;
  ZoneVector<ObjectData*> closure_feedback_cell_array_;
};

FeedbackVectorData::FeedbackVectorData(JSHeapBroker* broker,
                                       ObjectData** storage,
                                       Handle<FeedbackVector> object)
    : HeapObjectData(broker, storage, object),
      invocation_count_(object->invocation_count()),
      closure_feedback_cell_array_(broker->zone()) {}

FeedbackCellData* FeedbackVectorData::GetClosureFeedbackCell(
    JSHeapBroker* broker, int index) const {
  CHECK_GE(index, 0);

  size_t cell_array_size = closure_feedback_cell_array_.size();
  if (!serialized_) {
    DCHECK_EQ(cell_array_size, 0);
    TRACE_BROKER_MISSING(broker,
                         " closure feedback cell array for vector " << this);
    return nullptr;
  }
  CHECK_LT(index, cell_array_size);
  return closure_feedback_cell_array_[index]->AsFeedbackCell();
}

void FeedbackVectorData::Serialize(JSHeapBroker* broker) {
  if (serialized_) return;
  serialized_ = true;

  TraceScope tracer(broker, this, "FeedbackVectorData::Serialize");
  Handle<FeedbackVector> vector = Handle<FeedbackVector>::cast(object());
  DCHECK(closure_feedback_cell_array_.empty());
  int length = vector->closure_feedback_cell_array().length();
  closure_feedback_cell_array_.reserve(length);
  for (int i = 0; i < length; ++i) {
    Handle<FeedbackCell> cell = vector->GetClosureFeedbackCell(i);
    ObjectData* cell_data = broker->GetOrCreateData(cell);
    closure_feedback_cell_array_.push_back(cell_data);
  }
  TRACE(broker, "Copied " << length << " feedback cells");
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
  bound_target_function_ =
      broker->GetOrCreateData(function->bound_target_function());
  if (bound_target_function_->IsJSBoundFunction()) {
    bound_target_function_->AsJSBoundFunction()->Serialize(broker);
  } else if (bound_target_function_->IsJSFunction()) {
    bound_target_function_->AsJSFunction()->Serialize(broker);
  }

  DCHECK_NULL(bound_arguments_);
  bound_arguments_ =
      broker->GetOrCreateData(function->bound_arguments())->AsFixedArray();
  bound_arguments_->SerializeContents(broker);

  DCHECK_NULL(bound_this_);
  bound_this_ = broker->GetOrCreateData(function->bound_this());
}

JSObjectData::JSObjectData(JSHeapBroker* broker, ObjectData** storage,
                           Handle<JSObject> object)
    : JSReceiverData(broker, storage, object),
      inobject_fields_(broker->zone()),
      own_constant_elements_(broker->zone()),
      own_properties_(broker->zone()) {}

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
  int parameter_count() const { return parameter_count_; }
  interpreter::Register incoming_new_target_or_generator_register() const {
    return incoming_new_target_or_generator_register_;
  }

  uint8_t get(int index) const {
    DCHECK(is_serialized_for_compilation_);
    return bytecodes_[index];
  }

  Address GetFirstBytecodeAddress() const {
    return reinterpret_cast<Address>(bytecodes_.data());
  }

  Handle<Object> GetConstantAtIndex(int index, Isolate* isolate) const {
    return constant_pool_[index]->object();
  }

  bool IsConstantAtIndexSmi(int index) const {
    return constant_pool_[index]->is_smi();
  }

  Smi GetConstantAtIndexAsSmi(int index) const {
    return *(Handle<Smi>::cast(constant_pool_[index]->object()));
  }

  void SerializeForCompilation(JSHeapBroker* broker) {
    if (is_serialized_for_compilation_) return;

    Handle<BytecodeArray> bytecode_array =
        Handle<BytecodeArray>::cast(object());

    DCHECK(bytecodes_.empty());
    bytecodes_.reserve(bytecode_array->length());
    for (int i = 0; i < bytecode_array->length(); i++) {
      bytecodes_.push_back(bytecode_array->get(i));
    }

    DCHECK(constant_pool_.empty());
    Handle<FixedArray> constant_pool(bytecode_array->constant_pool(),
                                     broker->isolate());
    constant_pool_.reserve(constant_pool->length());
    for (int i = 0; i < constant_pool->length(); i++) {
      constant_pool_.push_back(broker->GetOrCreateData(constant_pool->get(i)));
    }

    Handle<ByteArray> source_position_table(
        bytecode_array->SourcePositionTableIfCollected(), broker->isolate());
    source_positions_.reserve(source_position_table->length());
    for (int i = 0; i < source_position_table->length(); i++) {
      source_positions_.push_back(source_position_table->get(i));
    }

    Handle<ByteArray> handlers(bytecode_array->handler_table(),
                               broker->isolate());
    handler_table_.reserve(handlers->length());
    for (int i = 0; i < handlers->length(); i++) {
      handler_table_.push_back(handlers->get(i));
    }

    is_serialized_for_compilation_ = true;
  }

  const byte* source_positions_address() const {
    return source_positions_.data();
  }

  size_t source_positions_size() const { return source_positions_.size(); }

  Address handler_table_address() const {
    CHECK(is_serialized_for_compilation_);
    return reinterpret_cast<Address>(handler_table_.data());
  }

  int handler_table_size() const {
    CHECK(is_serialized_for_compilation_);
    return static_cast<int>(handler_table_.size());
  }

  BytecodeArrayData(JSHeapBroker* broker, ObjectData** storage,
                    Handle<BytecodeArray> object)
      : FixedArrayBaseData(broker, storage, object),
        register_count_(object->register_count()),
        parameter_count_(object->parameter_count()),
        incoming_new_target_or_generator_register_(
            object->incoming_new_target_or_generator_register()),
        bytecodes_(broker->zone()),
        source_positions_(broker->zone()),
        handler_table_(broker->zone()),
        constant_pool_(broker->zone()) {}

 private:
  int const register_count_;
  int const parameter_count_;
  interpreter::Register const incoming_new_target_or_generator_register_;

  bool is_serialized_for_compilation_ = false;
  ZoneVector<uint8_t> bytecodes_;
  ZoneVector<uint8_t> source_positions_;
  ZoneVector<uint8_t> handler_table_;
  ZoneVector<ObjectData*> constant_pool_;
};

class JSArrayData : public JSObjectData {
 public:
  JSArrayData(JSHeapBroker* broker, ObjectData** storage,
              Handle<JSArray> object);

  void Serialize(JSHeapBroker* broker);
  ObjectData* length() const { return length_; }

  ObjectData* GetOwnElement(
      JSHeapBroker* broker, uint32_t index,
      SerializationPolicy policy = SerializationPolicy::kAssumeSerialized);

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
                                       SerializationPolicy policy) {
  for (auto const& p : own_elements_) {
    if (p.first == index) return p.second;
  }

  if (policy == SerializationPolicy::kAssumeSerialized) {
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
  void SerializeFunctionTemplateInfo(JSHeapBroker* broker);
  FunctionTemplateInfoData* function_template_info() const {
    return function_template_info_;
  }
  JSArrayData* GetTemplateObject(FeedbackSlot slot) const {
    auto lookup_it = template_objects_.find(slot.ToInt());
    if (lookup_it != template_objects_.cend()) {
      return lookup_it->second;
    }
    return nullptr;
  }
  void SetTemplateObject(FeedbackSlot slot, JSArrayData* object) {
    CHECK(
        template_objects_.insert(std::make_pair(slot.ToInt(), object)).second);
  }

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
  FunctionTemplateInfoData* function_template_info_;
  ZoneMap<int, JSArrayData*> template_objects_;
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
      ,
      function_template_info_(nullptr),
      template_objects_(broker->zone()) {
  DCHECK_EQ(HasBuiltinId_, builtin_id_ != Builtins::kNoBuiltinId);
  DCHECK_EQ(HasBytecodeArray_, GetBytecodeArray_ != nullptr);
}

void SharedFunctionInfoData::SetSerializedForCompilation(
    JSHeapBroker* broker, FeedbackVectorRef feedback) {
  CHECK(serialized_for_compilation_.insert(feedback.object()).second);
  TRACE(broker, "Set function " << this << " with " << feedback
                                << " as serialized for compilation");
}

void SharedFunctionInfoData::SerializeFunctionTemplateInfo(
    JSHeapBroker* broker) {
  if (function_template_info_) return;

  function_template_info_ =
      broker
          ->GetOrCreateData(handle(
              Handle<SharedFunctionInfo>::cast(object())->function_data(),
              broker->isolate()))
          ->AsFunctionTemplateInfo();
}

bool SharedFunctionInfoData::IsSerializedForCompilation(
    FeedbackVectorRef feedback) const {
  return serialized_for_compilation_.find(feedback.object()) !=
         serialized_for_compilation_.end();
}

class SourceTextModuleData : public HeapObjectData {
 public:
  SourceTextModuleData(JSHeapBroker* broker, ObjectData** storage,
                       Handle<SourceTextModule> object);
  void Serialize(JSHeapBroker* broker);

  CellData* GetCell(JSHeapBroker* broker, int cell_index) const;

 private:
  bool serialized_ = false;
  ZoneVector<CellData*> imports_;
  ZoneVector<CellData*> exports_;
};

SourceTextModuleData::SourceTextModuleData(JSHeapBroker* broker,
                                           ObjectData** storage,
                                           Handle<SourceTextModule> object)
    : HeapObjectData(broker, storage, object),
      imports_(broker->zone()),
      exports_(broker->zone()) {}

CellData* SourceTextModuleData::GetCell(JSHeapBroker* broker,
                                        int cell_index) const {
  if (!serialized_) {
    DCHECK(imports_.empty());
    TRACE_BROKER_MISSING(broker,
                         "module cell " << cell_index << " on " << this);
    return nullptr;
  }
  CellData* cell;
  switch (SourceTextModuleDescriptor::GetCellIndexKind(cell_index)) {
    case SourceTextModuleDescriptor::kImport:
      cell = imports_.at(SourceTextModule::ImportIndex(cell_index));
      break;
    case SourceTextModuleDescriptor::kExport:
      cell = exports_.at(SourceTextModule::ExportIndex(cell_index));
      break;
    case SourceTextModuleDescriptor::kInvalid:
      UNREACHABLE();
  }
  CHECK_NOT_NULL(cell);
  return cell;
}

void SourceTextModuleData::Serialize(JSHeapBroker* broker) {
  if (serialized_) return;
  serialized_ = true;

  TraceScope tracer(broker, this, "SourceTextModuleData::Serialize");
  Handle<SourceTextModule> module = Handle<SourceTextModule>::cast(object());

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

class JSGlobalObjectData : public JSObjectData {
 public:
  JSGlobalObjectData(JSHeapBroker* broker, ObjectData** storage,
                     Handle<JSGlobalObject> object);
  bool IsDetached() const { return is_detached_; }

  PropertyCellData* GetPropertyCell(
      JSHeapBroker* broker, NameData* name,
      SerializationPolicy policy = SerializationPolicy::kAssumeSerialized);

 private:
  bool const is_detached_;

  // Properties that either
  // (1) are known to exist as property cells on the global object, or
  // (2) are known not to (possibly they don't exist at all).
  // In case (2), the second pair component is nullptr.
  ZoneVector<std::pair<NameData*, PropertyCellData*>> properties_;
};

JSGlobalObjectData::JSGlobalObjectData(JSHeapBroker* broker,
                                       ObjectData** storage,
                                       Handle<JSGlobalObject> object)
    : JSObjectData(broker, storage, object),
      is_detached_(object->IsDetached()),
      properties_(broker->zone()) {}

class JSGlobalProxyData : public JSObjectData {
 public:
  JSGlobalProxyData(JSHeapBroker* broker, ObjectData** storage,
                    Handle<JSGlobalProxy> object);
};

JSGlobalProxyData::JSGlobalProxyData(JSHeapBroker* broker, ObjectData** storage,
                                     Handle<JSGlobalProxy> object)
    : JSObjectData(broker, storage, object) {}

namespace {
base::Optional<PropertyCellRef> GetPropertyCellFromHeap(JSHeapBroker* broker,
                                                        Handle<Name> name) {
  LookupIterator it(
      broker->isolate(),
      handle(broker->target_native_context().object()->global_object(),
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

PropertyCellData* JSGlobalObjectData::GetPropertyCell(
    JSHeapBroker* broker, NameData* name, SerializationPolicy policy) {
  CHECK_NOT_NULL(name);
  for (auto const& p : properties_) {
    if (p.first == name) return p.second;
  }

  if (policy == SerializationPolicy::kAssumeSerialized) {
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

class TemplateObjectDescriptionData : public HeapObjectData {
 public:
  TemplateObjectDescriptionData(JSHeapBroker* broker, ObjectData** storage,
                                Handle<TemplateObjectDescription> object)
      : HeapObjectData(broker, storage, object) {}
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
  SerializeRecursiveAsBoilerplate(broker, kMaxFastLiteralDepth);
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

  int const number_of_own = map->NumberOfOwnDescriptors();
  for (InternalIndex i : InternalIndex::Range(number_of_own)) {
    SerializeOwnDescriptor(broker, i);
  }
}

ObjectData* MapData::GetStrongValue(InternalIndex descriptor_index) const {
  auto data = instance_descriptors_->contents().find(descriptor_index.as_int());
  if (data == instance_descriptors_->contents().end()) return nullptr;

  return data->second.value;
}

void MapData::SerializeOwnDescriptor(JSHeapBroker* broker,
                                     InternalIndex descriptor_index) {
  TraceScope tracer(broker, this, "MapData::SerializeOwnDescriptor");
  Handle<Map> map = Handle<Map>::cast(object());

  if (instance_descriptors_ == nullptr) {
    instance_descriptors_ = broker->GetOrCreateData(map->instance_descriptors())
                                ->AsDescriptorArray();
  }

  ZoneMap<int, PropertyDescriptor>& contents =
      instance_descriptors()->contents();
  CHECK_LT(descriptor_index.as_int(), map->NumberOfOwnDescriptors());
  if (contents.find(descriptor_index.as_int()) != contents.end()) return;

  Isolate* const isolate = broker->isolate();
  auto descriptors =
      Handle<DescriptorArray>::cast(instance_descriptors_->object());
  CHECK_EQ(*descriptors, map->instance_descriptors());

  PropertyDescriptor d;
  d.key =
      broker->GetOrCreateData(descriptors->GetKey(descriptor_index))->AsName();
  MaybeObject value = descriptors->GetValue(descriptor_index);
  HeapObject obj;
  if (value.GetHeapObjectIfStrong(&obj)) {
    d.value = broker->GetOrCreateData(handle(obj, broker->isolate()));
  }
  d.details = descriptors->GetDetails(descriptor_index);
  if (d.details.location() == kField) {
    d.field_index = FieldIndex::ForDescriptor(*map, descriptor_index);
    d.field_owner =
        broker->GetOrCreateData(map->FindFieldOwner(isolate, descriptor_index))
            ->AsMap();
    d.field_type =
        broker->GetOrCreateData(descriptors->GetFieldType(descriptor_index));
    d.is_unboxed_double_field = map->IsUnboxedDoubleField(d.field_index);
  }
  contents[descriptor_index.as_int()] = d;

  if (d.details.location() == kField) {
    // Recurse on the owner map.
    d.field_owner->SerializeOwnDescriptor(broker, descriptor_index);
  }

  TRACE(broker, "Copied descriptor " << descriptor_index.as_int() << " into "
                                     << instance_descriptors_ << " ("
                                     << contents.size() << " total)");
}

void MapData::SerializeRootMap(JSHeapBroker* broker) {
  if (serialized_root_map_) return;
  serialized_root_map_ = true;

  TraceScope tracer(broker, this, "MapData::SerializeRootMap");
  Handle<Map> map = Handle<Map>::cast(object());
  DCHECK_NULL(root_map_);
  root_map_ =
      broker->GetOrCreateData(map->FindRootMap(broker->isolate()))->AsMap();
}

MapData* MapData::FindRootMap() const { return root_map_; }

void JSObjectData::SerializeRecursiveAsBoilerplate(JSHeapBroker* broker,
                                                   int depth) {
  if (serialized_as_boilerplate_) return;
  serialized_as_boilerplate_ = true;

  TraceScope tracer(broker, this,
                    "JSObjectData::SerializeRecursiveAsBoilerplate");
  Handle<JSObject> boilerplate = Handle<JSObject>::cast(object());

  // We only serialize boilerplates that pass the IsInlinableFastLiteral
  // check, so we only do a sanity check on the depth here.
  CHECK_GT(depth, 0);
  CHECK(!boilerplate->map().is_deprecated());

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
        value_data->AsJSObject()->SerializeRecursiveAsBoilerplate(broker,
                                                                  depth - 1);
      }
    }
  } else {
    CHECK(boilerplate->HasDoubleElements());
    CHECK_LE(elements_object->Size(), kMaxRegularHeapObjectSize);
    elements_->AsFixedDoubleArray()->SerializeContents(broker);
  }

  // TODO(turbofan): Do we want to support out-of-object properties?
  CHECK(boilerplate->HasFastProperties() &&
        boilerplate->property_array().length() == 0);
  CHECK_EQ(inobject_fields_.size(), 0u);

  // Check the in-object properties.
  Handle<DescriptorArray> descriptors(boilerplate->map().instance_descriptors(),
                                      isolate);
  for (InternalIndex i : boilerplate->map().IterateOwnDescriptors()) {
    PropertyDetails details = descriptors->GetDetails(i);
    if (details.location() != kField) continue;
    DCHECK_EQ(kData, details.kind());

    FieldIndex field_index = FieldIndex::ForDescriptor(boilerplate->map(), i);
    // Make sure {field_index} agrees with {inobject_properties} on the index of
    // this field.
    DCHECK_EQ(field_index.property_index(),
              static_cast<int>(inobject_fields_.size()));
    if (boilerplate->IsUnboxedDoubleField(field_index)) {
      uint64_t value_bits =
          boilerplate->RawFastDoublePropertyAsBitsAt(field_index);
      inobject_fields_.push_back(JSObjectField{value_bits});
    } else {
      Handle<Object> value(boilerplate->RawFastPropertyAt(field_index),
                           isolate);
      // In case of double fields we use a sentinel NaN value to mark
      // uninitialized fields. A boilerplate value with such a field may migrate
      // from its double to a tagged representation. If the double is unboxed,
      // the raw double is converted to a heap number, otherwise the (boxed)
      // double ceases to be mutable, and becomes a normal heap number. The
      // sentinel value carries no special meaning when it occurs in a heap
      // number, so we would like to recover the uninitialized value. We check
      // for the sentinel here, specifically, since migrations might have been
      // triggered as part of boilerplate serialization.
      if (!details.representation().IsDouble() && value->IsHeapNumber() &&
          HeapNumber::cast(*value).value_as_bits() == kHoleNanInt64) {
        value = isolate->factory()->uninitialized_value();
      }
      ObjectData* value_data = broker->GetOrCreateData(value);
      if (value->IsJSObject()) {
        value_data->AsJSObject()->SerializeRecursiveAsBoilerplate(broker,
                                                                  depth - 1);
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
#ifdef DEBUG
  if (broker()->mode() == JSHeapBroker::kSerialized &&
      data_->used_status == ObjectData::Usage::kUnused) {
    data_->used_status = ObjectData::Usage::kOnlyIdentityUsed;
  }
#endif  // DEBUG
  return data_ == other.data_;
}

Isolate* ObjectRef::isolate() const { return broker()->isolate(); }

ContextRef ContextRef::previous(size_t* depth,
                                SerializationPolicy policy) const {
  DCHECK_NOT_NULL(depth);
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleAllocation handle_allocation;
    AllowHandleDereference handle_dereference;
    Context current = *object();
    while (*depth != 0 && current.unchecked_previous().IsContext()) {
      current = Context::cast(current.unchecked_previous());
      (*depth)--;
    }
    return ContextRef(broker(), handle(current, broker()->isolate()));
  }
  ContextData* current = this->data()->AsContext();
  return ContextRef(broker(), current->previous(broker(), depth, policy));
}

base::Optional<ObjectRef> ContextRef::get(int index,
                                          SerializationPolicy policy) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleAllocation handle_allocation;
    AllowHandleDereference handle_dereference;
    Handle<Object> value(object()->get(index), broker()->isolate());
    return ObjectRef(broker(), value);
  }
  ObjectData* optional_slot =
      data()->AsContext()->GetSlot(broker(), index, policy);
  if (optional_slot != nullptr) {
    return ObjectRef(broker(), optional_slot);
  }
  return base::nullopt;
}

JSHeapBroker::JSHeapBroker(Isolate* isolate, Zone* broker_zone,
                           bool tracing_enabled)
    : isolate_(isolate),
      broker_zone_(broker_zone),
      current_zone_(broker_zone),
      refs_(new (zone())
                RefsMap(kMinimalRefsBucketCount, AddressMatcher(), zone())),
      array_and_object_prototypes_(zone()),
      tracing_enabled_(tracing_enabled),
      feedback_(zone()),
      bytecode_analyses_(zone()),
      property_access_infos_(zone()),
      typed_array_string_tags_(zone()) {
  // Note that this initialization of {refs_} with the minimal initial capacity
  // is redundant in the normal use case (concurrent compilation enabled,
  // standard objects to be serialized), as the map is going to be replaced
  // immediately with a larger-capacity one.  It doesn't seem to affect the
  // performance in a noticeable way though.
  TRACE(this, "Constructing heap broker");
}

std::ostream& JSHeapBroker::Trace() const {
  return trace_out_ << "[" << this << "] "
                    << std::string(trace_indentation_ * 2, ' ');
}

void JSHeapBroker::StopSerializing() {
  CHECK_EQ(mode_, kSerializing);
  TRACE(this, "Stopping serialization");
  mode_ = kSerialized;
}

#ifdef DEBUG
void JSHeapBroker::PrintRefsAnalysis() const {
  // Usage counts
  size_t used_total = 0, unused_total = 0, identity_used_total = 0;
  for (RefsMap::Entry* ref = refs_->Start(); ref != nullptr;
       ref = refs_->Next(ref)) {
    switch (ref->value->used_status) {
      case ObjectData::Usage::kUnused:
        ++unused_total;
        break;
      case ObjectData::Usage::kOnlyIdentityUsed:
        ++identity_used_total;
        break;
      case ObjectData::Usage::kDataUsed:
        ++used_total;
        break;
    }
  }

  // Ref types analysis
  TRACE_BROKER_MEMORY(
      this, "Refs: " << refs_->occupancy() << "; data used: " << used_total
                     << "; only identity used: " << identity_used_total
                     << "; unused: " << unused_total);
  size_t used_smis = 0, unused_smis = 0, identity_used_smis = 0;
  size_t used[LAST_TYPE + 1] = {0};
  size_t unused[LAST_TYPE + 1] = {0};
  size_t identity_used[LAST_TYPE + 1] = {0};
  for (RefsMap::Entry* ref = refs_->Start(); ref != nullptr;
       ref = refs_->Next(ref)) {
    if (ref->value->is_smi()) {
      switch (ref->value->used_status) {
        case ObjectData::Usage::kUnused:
          ++unused_smis;
          break;
        case ObjectData::Usage::kOnlyIdentityUsed:
          ++identity_used_smis;
          break;
        case ObjectData::Usage::kDataUsed:
          ++used_smis;
          break;
      }
    } else {
      InstanceType instance_type =
          static_cast<const HeapObjectData*>(ref->value)
              ->map()
              ->instance_type();
      CHECK_LE(FIRST_TYPE, instance_type);
      CHECK_LE(instance_type, LAST_TYPE);
      switch (ref->value->used_status) {
        case ObjectData::Usage::kUnused:
          ++unused[instance_type];
          break;
        case ObjectData::Usage::kOnlyIdentityUsed:
          ++identity_used[instance_type];
          break;
        case ObjectData::Usage::kDataUsed:
          ++used[instance_type];
          break;
      }
    }
  }

  TRACE_BROKER_MEMORY(
      this, "Smis: " << used_smis + identity_used_smis + unused_smis
                     << "; data used: " << used_smis << "; only identity used: "
                     << identity_used_smis << "; unused: " << unused_smis);
  for (uint16_t i = FIRST_TYPE; i <= LAST_TYPE; ++i) {
    size_t total = used[i] + identity_used[i] + unused[i];
    if (total == 0) continue;
    TRACE_BROKER_MEMORY(
        this, InstanceType(i) << ": " << total << "; data used: " << used[i]
                              << "; only identity used: " << identity_used[i]
                              << "; unused: " << unused[i]);
  }
}
#endif  // DEBUG

void JSHeapBroker::Retire() {
  CHECK_EQ(mode_, kSerialized);
  TRACE(this, "Retiring");
  mode_ = kRetired;

#ifdef DEBUG
  PrintRefsAnalysis();
#endif  // DEBUG
}

bool JSHeapBroker::SerializingAllowed() const { return mode() == kSerializing; }

void JSHeapBroker::SetTargetNativeContextRef(
    Handle<NativeContext> native_context) {
  // The MapData constructor uses {target_native_context_}. This creates a
  // benign cycle that we break by setting {target_native_context_} right before
  // starting to serialize (thus creating dummy data), and then again properly
  // right after.
  DCHECK((mode() == kDisabled && !target_native_context_.has_value()) ||
         (mode() == kSerializing &&
          target_native_context_->object().equals(native_context) &&
          target_native_context_->data_->kind() == kUnserializedHeapObject));
  target_native_context_ = NativeContextRef(this, native_context);
}

bool IsShareable(Handle<Object> object, Isolate* isolate) {
  int index;
  RootIndex root_index;
  bool is_builtin_handle =
      object->IsHeapObject() && isolate->builtins()->IsBuiltinHandle(
                                    Handle<HeapObject>::cast(object), &index);
  return is_builtin_handle ||
         isolate->roots_table().IsRootHandle(object, &root_index);
}

void JSHeapBroker::InitializeRefsMap() {
  TraceScope tracer(this, "JSHeapBroker::InitializeRefsMap");

  DCHECK_NULL(compiler_cache_);
  PerIsolateCompilerCache::Setup(isolate());
  compiler_cache_ = isolate()->compiler_cache();

  if (compiler_cache_->HasSnapshot()) {
    TRACE(this, "Importing existing RefsMap snapshot");
    DCHECK_NULL(refs_);
    refs_ = new (zone()) RefsMap(compiler_cache_->GetSnapshot(), zone());
    return;
  }

  TRACE(this, "Building RefsMap snapshot");
  DCHECK_NULL(refs_);
  refs_ =
      new (zone()) RefsMap(kInitialRefsBucketCount, AddressMatcher(), zone());

  // Temporarily use the "compiler zone" for serialization, such that the
  // serialized data survives this compilation.
  DCHECK_EQ(current_zone_, broker_zone_);
  current_zone_ = compiler_cache_->zone();

  // Serialize various builtins.
  Builtins* const b = isolate()->builtins();
  {
    Builtins::Name builtins[] = {
        Builtins::kAllocateInYoungGeneration,
        Builtins::kAllocateRegularInYoungGeneration,
        Builtins::kAllocateInOldGeneration,
        Builtins::kAllocateRegularInOldGeneration,
        Builtins::kArgumentsAdaptorTrampoline,
        Builtins::kArrayConstructorImpl,
        Builtins::kArrayIncludesHoleyDoubles,
        Builtins::kArrayIncludesPackedDoubles,
        Builtins::kArrayIncludesSmiOrObject,
        Builtins::kArrayIndexOfHoleyDoubles,
        Builtins::kArrayIndexOfPackedDoubles,
        Builtins::kArrayIndexOfSmiOrObject,
        Builtins::kCallApiCallback,
        Builtins::kCallFunctionForwardVarargs,
        Builtins::kCallFunction_ReceiverIsAny,
        Builtins::kCallFunction_ReceiverIsNotNullOrUndefined,
        Builtins::kCallFunction_ReceiverIsNullOrUndefined,
        Builtins::kCloneFastJSArray,
        Builtins::kCompileLazy,
        Builtins::kConstructFunctionForwardVarargs,
        Builtins::kForInFilter,
        Builtins::kGetProperty,
        Builtins::kIncBlockCounter,
        Builtins::kJSBuiltinsConstructStub,
        Builtins::kJSConstructStubGeneric,
        Builtins::kStringAdd_CheckNone,
        Builtins::kStringAddConvertLeft,
        Builtins::kStringAddConvertRight,
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

  // TODO(mslekova): Serialize root objects (from factory).

  // Verify.
  for (RefsMap::Entry* p = refs_->Start(); p != nullptr; p = refs_->Next(p)) {
    CHECK(IsShareable(p->value->object(), isolate()));
  }

  compiler_cache()->SetSnapshot(refs_);
  current_zone_ = broker_zone_;
}

void JSHeapBroker::CollectArrayAndObjectPrototypes() {
  DisallowHeapAllocation no_gc;
  CHECK_EQ(mode(), kSerializing);
  CHECK(array_and_object_prototypes_.empty());

  Object maybe_context = isolate()->heap()->native_contexts_list();
  while (!maybe_context.IsUndefined(isolate())) {
    Context context = Context::cast(maybe_context);
    Object array_prot = context.get(Context::INITIAL_ARRAY_PROTOTYPE_INDEX);
    Object object_prot = context.get(Context::INITIAL_OBJECT_PROTOTYPE_INDEX);
    array_and_object_prototypes_.emplace(JSObject::cast(array_prot), isolate());
    array_and_object_prototypes_.emplace(JSObject::cast(object_prot),
                                         isolate());
    maybe_context = context.next_context_link();
  }

  CHECK(!array_and_object_prototypes_.empty());
}

void JSHeapBroker::SerializeTypedArrayStringTags() {
#define TYPED_ARRAY_STRING_TAG(Type, type, TYPE, ctype)              \
  do {                                                               \
    ObjectData* data = GetOrCreateData(                              \
        isolate()->factory()->InternalizeUtf8String(#Type "Array")); \
    typed_array_string_tags_.push_back(data);                        \
  } while (false);

  TYPED_ARRAYS(TYPED_ARRAY_STRING_TAG)
#undef TYPED_ARRAY_STRING_TAG
}

StringRef JSHeapBroker::GetTypedArrayStringTag(ElementsKind kind) {
  DCHECK(IsTypedArrayElementsKind(kind));
  size_t idx = kind - FIRST_FIXED_TYPED_ARRAY_ELEMENTS_KIND;
  CHECK_LT(idx, typed_array_string_tags_.size());
  return StringRef(this, typed_array_string_tags_[idx]);
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

void JSHeapBroker::InitializeAndStartSerializing(
    Handle<NativeContext> native_context) {
  TraceScope tracer(this, "JSHeapBroker::InitializeAndStartSerializing");

  CHECK_EQ(mode_, kDisabled);
  mode_ = kSerializing;

  // Throw away the dummy data that we created while disabled.
  refs_->Clear();
  refs_ = nullptr;

  InitializeRefsMap();

  SetTargetNativeContextRef(native_context);
  target_native_context().Serialize();

  CollectArrayAndObjectPrototypes();
  SerializeTypedArrayStringTags();

  // Serialize standard objects.
  //
  // - Maps, strings, oddballs
  Factory* const f = isolate()->factory();
  GetOrCreateData(f->arguments_marker_map());
  GetOrCreateData(f->bigint_string());
  GetOrCreateData(f->block_context_map());
  GetOrCreateData(f->boolean_map());
  GetOrCreateData(f->boolean_string());
  GetOrCreateData(f->catch_context_map());
  GetOrCreateData(f->empty_fixed_array());
  GetOrCreateData(f->empty_string());
  GetOrCreateData(f->eval_context_map());
  GetOrCreateData(f->exec_string());
  GetOrCreateData(f->false_string());
  GetOrCreateData(f->false_value());
  GetOrCreateData(f->fixed_array_map());
  GetOrCreateData(f->fixed_cow_array_map());
  GetOrCreateData(f->fixed_double_array_map());
  GetOrCreateData(f->function_context_map());
  GetOrCreateData(f->function_string());
  GetOrCreateData(f->has_instance_symbol());
  GetOrCreateData(f->heap_number_map());
  GetOrCreateData(f->length_string());
  GetOrCreateData(f->many_closures_cell_map());
  GetOrCreateData(f->minus_zero_value());
  GetOrCreateData(f->name_dictionary_map());
  GetOrCreateData(f->name_string());
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
  GetOrCreateData(f->prototype_string());
  GetOrCreateData(f->ReflectHas_string());
  GetOrCreateData(f->ReflectGet_string());
  GetOrCreateData(f->sloppy_arguments_elements_map());
  GetOrCreateData(f->stale_register());
  GetOrCreateData(f->stale_register_map());
  GetOrCreateData(f->string_string());
  GetOrCreateData(f->symbol_string());
  GetOrCreateData(f->termination_exception_map());
  GetOrCreateData(f->the_hole_map());
  GetOrCreateData(f->the_hole_value());
  GetOrCreateData(f->then_string());
  GetOrCreateData(f->true_string());
  GetOrCreateData(f->true_value());
  GetOrCreateData(f->undefined_map());
  GetOrCreateData(f->undefined_string());
  GetOrCreateData(f->undefined_value());
  GetOrCreateData(f->uninitialized_map());
  GetOrCreateData(f->with_context_map());
  GetOrCreateData(f->zero_string());
  // - Cells
  GetOrCreateData(f->array_buffer_detaching_protector())
      ->AsPropertyCell()
      ->Serialize(this);
  GetOrCreateData(f->array_constructor_protector())
      ->AsPropertyCell()
      ->Serialize(this);
  GetOrCreateData(f->array_iterator_protector())
      ->AsPropertyCell()
      ->Serialize(this);
  GetOrCreateData(f->array_species_protector())
      ->AsPropertyCell()
      ->Serialize(this);
  GetOrCreateData(f->many_closures_cell())->AsFeedbackCell();
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
  GetOrCreateData(f->string_length_protector())
      ->AsPropertyCell()
      ->Serialize(this);
  // - CEntry stub
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
  MapData* map_data = data()->AsJSObject()->object_create_map(broker());
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

FeedbackCellRef FeedbackVectorRef::GetClosureFeedbackCell(int index) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleAllocation handle_allocation;
    AllowHandleDereference handle_dereference;
    return FeedbackCellRef(broker(), object()->GetClosureFeedbackCell(index));
  }

  return FeedbackCellRef(
      broker(),
      data()->AsFeedbackVector()->GetClosureFeedbackCell(broker(), index));
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

uint64_t JSObjectRef::RawFastDoublePropertyAsBitsAt(FieldIndex index) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference handle_dereference;
    return object()->RawFastDoublePropertyAsBitsAt(index);
  }
  JSObjectData* object_data = data()->AsJSObject();
  CHECK(index.is_inobject());
  return object_data->GetInobjectField(index.property_index()).AsBitsOfDouble();
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

void AllocationSiteRef::SerializeBoilerplate() {
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsAllocationSite()->SerializeBoilerplate(broker());
}

void JSObjectRef::SerializeElements() {
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsJSObject()->SerializeElements(broker());
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

FieldIndex MapRef::GetFieldIndexFor(InternalIndex descriptor_index) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference allow_handle_dereference;
    return FieldIndex::ForDescriptor(*object(), descriptor_index);
  }
  DescriptorArrayData* descriptors = data()->AsMap()->instance_descriptors();
  return descriptors->contents().at(descriptor_index.as_int()).field_index;
}

int MapRef::GetInObjectPropertyOffset(int i) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference allow_handle_dereference;
    return object()->GetInObjectPropertyOffset(i);
  }
  return (GetInObjectPropertiesStartInWords() + i) * kTaggedSize;
}

PropertyDetails MapRef::GetPropertyDetails(
    InternalIndex descriptor_index) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference allow_handle_dereference;
    return object()->instance_descriptors().GetDetails(descriptor_index);
  }
  DescriptorArrayData* descriptors = data()->AsMap()->instance_descriptors();
  return descriptors->contents().at(descriptor_index.as_int()).details;
}

NameRef MapRef::GetPropertyKey(InternalIndex descriptor_index) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleAllocation handle_allocation;
    AllowHandleDereference allow_handle_dereference;
    return NameRef(
        broker(),
        handle(object()->instance_descriptors().GetKey(descriptor_index),
               broker()->isolate()));
  }
  DescriptorArrayData* descriptors = data()->AsMap()->instance_descriptors();
  return NameRef(broker(),
                 descriptors->contents().at(descriptor_index.as_int()).key);
}

bool MapRef::IsFixedCowArrayMap() const {
  Handle<Map> fixed_cow_array_map =
      ReadOnlyRoots(broker()->isolate()).fixed_cow_array_map_handle();
  return equals(MapRef(broker(), fixed_cow_array_map));
}

bool MapRef::IsPrimitiveMap() const {
  return instance_type() <= LAST_PRIMITIVE_HEAP_OBJECT_TYPE;
}

MapRef MapRef::FindFieldOwner(InternalIndex descriptor_index) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleAllocation handle_allocation;
    AllowHandleDereference allow_handle_dereference;
    Handle<Map> owner(
        object()->FindFieldOwner(broker()->isolate(), descriptor_index),
        broker()->isolate());
    return MapRef(broker(), owner);
  }
  DescriptorArrayData* descriptors = data()->AsMap()->instance_descriptors();
  return MapRef(
      broker(),
      descriptors->contents().at(descriptor_index.as_int()).field_owner);
}

ObjectRef MapRef::GetFieldType(InternalIndex descriptor_index) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleAllocation handle_allocation;
    AllowHandleDereference allow_handle_dereference;
    Handle<FieldType> field_type(
        object()->instance_descriptors().GetFieldType(descriptor_index),
        broker()->isolate());
    return ObjectRef(broker(), field_type);
  }
  DescriptorArrayData* descriptors = data()->AsMap()->instance_descriptors();
  return ObjectRef(
      broker(),
      descriptors->contents().at(descriptor_index.as_int()).field_type);
}

bool MapRef::IsUnboxedDoubleField(InternalIndex descriptor_index) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference allow_handle_dereference;
    return object()->IsUnboxedDoubleField(
        FieldIndex::ForDescriptor(*object(), descriptor_index));
  }
  DescriptorArrayData* descriptors = data()->AsMap()->instance_descriptors();
  return descriptors->contents()
      .at(descriptor_index.as_int())
      .is_unboxed_double_field;
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

int ArrayBoilerplateDescriptionRef::constants_elements_length() const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference allow_handle_dereference;
    return object()->constant_elements().length();
  }
  return data()->AsArrayBoilerplateDescription()->constants_elements_length();
}

int ObjectBoilerplateDescriptionRef::size() const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference allow_handle_dereference;
    return object()->size();
  }
  return data()->AsObjectBoilerplateDescription()->size();
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

uint8_t BytecodeArrayRef::get(int index) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleAllocation handle_allocation;
    AllowHandleDereference allow_handle_dereference;
    return object()->get(index);
  }
  return data()->AsBytecodeArray()->get(index);
}

Address BytecodeArrayRef::GetFirstBytecodeAddress() const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleAllocation handle_allocation;
    AllowHandleDereference allow_handle_dereference;
    return object()->GetFirstBytecodeAddress();
  }
  return data()->AsBytecodeArray()->GetFirstBytecodeAddress();
}

Handle<Object> BytecodeArrayRef::GetConstantAtIndex(int index) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleAllocation handle_allocation;
    AllowHandleDereference allow_handle_dereference;
    return handle(object()->constant_pool().get(index), broker()->isolate());
  }
  return data()->AsBytecodeArray()->GetConstantAtIndex(index,
                                                       broker()->isolate());
}

bool BytecodeArrayRef::IsConstantAtIndexSmi(int index) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleAllocation handle_allocation;
    AllowHandleDereference allow_handle_dereference;
    return object()->constant_pool().get(index).IsSmi();
  }
  return data()->AsBytecodeArray()->IsConstantAtIndexSmi(index);
}

Smi BytecodeArrayRef::GetConstantAtIndexAsSmi(int index) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleAllocation handle_allocation;
    AllowHandleDereference allow_handle_dereference;
    return Smi::cast(object()->constant_pool().get(index));
  }
  return data()->AsBytecodeArray()->GetConstantAtIndexAsSmi(index);
}

void BytecodeArrayRef::SerializeForCompilation() {
  if (broker()->mode() == JSHeapBroker::kDisabled) return;
  data()->AsBytecodeArray()->SerializeForCompilation(broker());
}

const byte* BytecodeArrayRef::source_positions_address() const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference allow_handle_dereference;
    return object()->SourcePositionTableIfCollected().GetDataStartAddress();
  }
  return data()->AsBytecodeArray()->source_positions_address();
}

int BytecodeArrayRef::source_positions_size() const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference allow_handle_dereference;
    return object()->SourcePositionTableIfCollected().length();
  }
  return static_cast<int>(data()->AsBytecodeArray()->source_positions_size());
}

Address BytecodeArrayRef::handler_table_address() const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference allow_handle_dereference;
    return reinterpret_cast<Address>(
        object()->handler_table().GetDataStartAddress());
  }
  return data()->AsBytecodeArray()->handler_table_address();
}

int BytecodeArrayRef::handler_table_size() const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference allow_handle_dereference;
    return object()->handler_table().length();
  }
  return data()->AsBytecodeArray()->handler_table_size();
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
BIMODAL_ACCESSOR_C(BytecodeArray, int, parameter_count)
BIMODAL_ACCESSOR_C(BytecodeArray, interpreter::Register,
                   incoming_new_target_or_generator_register)

BIMODAL_ACCESSOR(Cell, Object, value)

BIMODAL_ACCESSOR_C(FeedbackVector, double, invocation_count)

BIMODAL_ACCESSOR(HeapObject, Map, map)

BIMODAL_ACCESSOR(JSArray, Object, length)

BIMODAL_ACCESSOR(JSBoundFunction, JSReceiver, bound_target_function)
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

BIMODAL_ACCESSOR_C(JSGlobalObject, bool, IsDetached)

BIMODAL_ACCESSOR_C(JSTypedArray, bool, is_on_heap)
BIMODAL_ACCESSOR_C(JSTypedArray, size_t, length)
BIMODAL_ACCESSOR(JSTypedArray, HeapObject, buffer)

BIMODAL_ACCESSOR_B(Map, bit_field2, elements_kind, Map::ElementsKindBits)
BIMODAL_ACCESSOR_B(Map, bit_field3, is_dictionary_map, Map::IsDictionaryMapBit)
BIMODAL_ACCESSOR_B(Map, bit_field3, is_deprecated, Map::IsDeprecatedBit)
BIMODAL_ACCESSOR_B(Map, bit_field3, NumberOfOwnDescriptors,
                   Map::NumberOfOwnDescriptorsBits)
BIMODAL_ACCESSOR_B(Map, bit_field3, is_migration_target,
                   Map::IsMigrationTargetBit)
BIMODAL_ACCESSOR_B(Map, bit_field3, is_extensible, Map::IsExtensibleBit)
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
BIMODAL_ACCESSOR_C(Map, bool, is_abandoned_prototype_map)

#define DEF_NATIVE_CONTEXT_ACCESSOR(type, name) \
  BIMODAL_ACCESSOR(NativeContext, type, name)
BROKER_NATIVE_CONTEXT_FIELDS(DEF_NATIVE_CONTEXT_ACCESSOR)
#undef DEF_NATIVE_CONTEXT_ACCESSOR

BIMODAL_ACCESSOR(PropertyCell, Object, value)
BIMODAL_ACCESSOR_C(PropertyCell, PropertyDetails, property_details)

base::Optional<CallHandlerInfoRef> FunctionTemplateInfoRef::call_code() const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    return CallHandlerInfoRef(
        broker(), handle(object()->call_code(), broker()->isolate()));
  }
  CallHandlerInfoData* call_code =
      data()->AsFunctionTemplateInfo()->call_code();
  if (!call_code) return base::nullopt;
  return CallHandlerInfoRef(broker(), call_code);
}

bool FunctionTemplateInfoRef::is_signature_undefined() const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference allow_handle_dereference;
    AllowHandleAllocation allow_handle_allocation;

    return object()->signature().IsUndefined(broker()->isolate());
  }
  return data()->AsFunctionTemplateInfo()->is_signature_undefined();
}

bool FunctionTemplateInfoRef::has_call_code() const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference allow_handle_dereference;
    AllowHandleAllocation allow_handle_allocation;

    CallOptimization call_optimization(broker()->isolate(), object());
    return call_optimization.is_simple_api_call();
  }
  return data()->AsFunctionTemplateInfo()->has_call_code();
}

BIMODAL_ACCESSOR_C(FunctionTemplateInfo, bool, accept_any_receiver)

HolderLookupResult FunctionTemplateInfoRef::LookupHolderOfExpectedType(
    MapRef receiver_map, SerializationPolicy policy) {
  const HolderLookupResult not_found;

  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference allow_handle_dereference;
    AllowHandleAllocation allow_handle_allocation;

    CallOptimization call_optimization(broker()->isolate(), object());
    Handle<Map> receiver_map_ref(receiver_map.object());
    if (!receiver_map_ref->IsJSReceiverMap() ||
        (receiver_map_ref->is_access_check_needed() &&
         !object()->accept_any_receiver())) {
      return not_found;
    }

    HolderLookupResult result;
    Handle<JSObject> holder = call_optimization.LookupHolderOfExpectedType(
        receiver_map_ref, &result.lookup);

    switch (result.lookup) {
      case CallOptimization::kHolderFound:
        result.holder = JSObjectRef(broker(), holder);
        break;
      default:
        DCHECK_EQ(result.holder, base::nullopt);
        break;
    }
    return result;
  }

  FunctionTemplateInfoData* fti_data = data()->AsFunctionTemplateInfo();
  KnownReceiversMap::iterator lookup_it =
      fti_data->known_receivers().find(receiver_map.data()->AsMap());
  if (lookup_it != fti_data->known_receivers().cend()) {
    return lookup_it->second;
  }
  if (policy == SerializationPolicy::kAssumeSerialized) {
    TRACE_BROKER_MISSING(broker(),
                         "holder for receiver with map " << receiver_map);
    return not_found;
  }
  if (!receiver_map.IsJSReceiverMap() ||
      (receiver_map.is_access_check_needed() && !accept_any_receiver())) {
    fti_data->known_receivers().insert(
        {receiver_map.data()->AsMap(), not_found});
    return not_found;
  }

  HolderLookupResult result;
  CallOptimization call_optimization(broker()->isolate(), object());
  Handle<JSObject> holder = call_optimization.LookupHolderOfExpectedType(
      receiver_map.object(), &result.lookup);

  switch (result.lookup) {
    case CallOptimization::kHolderFound: {
      result.holder = JSObjectRef(broker(), holder);
      fti_data->known_receivers().insert(
          {receiver_map.data()->AsMap(), result});
      break;
    }
    default: {
      DCHECK_EQ(result.holder, base::nullopt);
      fti_data->known_receivers().insert(
          {receiver_map.data()->AsMap(), result});
    }
  }
  return result;
}

BIMODAL_ACCESSOR(CallHandlerInfo, Object, data)

BIMODAL_ACCESSOR_C(SharedFunctionInfo, int, builtin_id)
BIMODAL_ACCESSOR(SharedFunctionInfo, BytecodeArray, GetBytecodeArray)
#define DEF_SFI_ACCESSOR(type, name) \
  BIMODAL_ACCESSOR_C(SharedFunctionInfo, type, name)
BROKER_SFI_FIELDS(DEF_SFI_ACCESSOR)
#undef DEF_SFI_ACCESSOR

BIMODAL_ACCESSOR_C(String, int, length)

BIMODAL_ACCESSOR(FeedbackCell, HeapObject, value)

ObjectRef MapRef::GetStrongValue(InternalIndex descriptor_index) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference allow_handle_dereference;
    return ObjectRef(broker(),
                     handle(object()->instance_descriptors().GetStrongValue(
                                descriptor_index),
                            broker()->isolate()));
  }
  return ObjectRef(broker(), data()->AsMap()->GetStrongValue(descriptor_index));
}

void MapRef::SerializeRootMap() {
  if (broker()->mode() == JSHeapBroker::kDisabled) return;
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsMap()->SerializeRootMap(broker());
}

base::Optional<MapRef> MapRef::FindRootMap() const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference allow_handle_dereference;
    return MapRef(broker(), handle(object()->FindRootMap(broker()->isolate()),
                                   broker()->isolate()));
  }
  MapData* map_data = data()->AsMap()->FindRootMap();
  if (map_data) {
    return MapRef(broker(), map_data);
  }
  TRACE_BROKER_MISSING(broker(), "root map for object " << *this);
  return base::nullopt;
}

void* JSTypedArrayRef::data_ptr() const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleDereference allow_handle_dereference;
    return object()->DataPtr();
  }
  return data()->AsJSTypedArray()->data_ptr();
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

ScopeInfoRef NativeContextRef::scope_info() const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleAllocation handle_allocation;
    AllowHandleDereference handle_dereference;
    return ScopeInfoRef(broker(),
                        handle(object()->scope_info(), broker()->isolate()));
  }
  return ScopeInfoRef(broker(), data()->AsNativeContext()->scope_info());
}

MapRef NativeContextRef::GetFunctionMapFromIndex(int index) const {
  DCHECK_GE(index, Context::FIRST_FUNCTION_MAP_INDEX);
  DCHECK_LE(index, Context::LAST_FUNCTION_MAP_INDEX);
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    return get(index).value().AsMap();
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
    uint32_t index, SerializationPolicy policy) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    return (IsJSObject() || IsString())
               ? GetOwnElementFromHeap(broker(), object(), index, true)
               : base::nullopt;
  }
  ObjectData* element = nullptr;
  if (IsJSObject()) {
    element =
        data()->AsJSObject()->GetOwnConstantElement(broker(), index, policy);
  } else if (IsString()) {
    element = data()->AsString()->GetCharAsString(broker(), index, policy);
  }
  if (element == nullptr) return base::nullopt;
  return ObjectRef(broker(), element);
}

base::Optional<ObjectRef> JSObjectRef::GetOwnDataProperty(
    Representation field_representation, FieldIndex index,
    SerializationPolicy policy) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    return GetOwnDataPropertyFromHeap(broker(),
                                      Handle<JSObject>::cast(object()),
                                      field_representation, index);
  }
  ObjectData* property = data()->AsJSObject()->GetOwnDataProperty(
      broker(), field_representation, index, policy);
  if (property == nullptr) return base::nullopt;
  return ObjectRef(broker(), property);
}

base::Optional<ObjectRef> JSArrayRef::GetOwnCowElement(
    uint32_t index, SerializationPolicy policy) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    if (!object()->elements().IsCowArray()) return base::nullopt;
    return GetOwnElementFromHeap(broker(), object(), index, false);
  }

  if (policy == SerializationPolicy::kSerializeIfNeeded) {
    data()->AsJSObject()->SerializeElements(broker());
  } else if (!data()->AsJSObject()->serialized_elements()) {
    TRACE(broker(), "'elements' on " << this);
    return base::nullopt;
  }
  if (!elements().map().IsFixedCowArrayMap()) return base::nullopt;

  ObjectData* element =
      data()->AsJSArray()->GetOwnElement(broker(), index, policy);
  if (element == nullptr) return base::nullopt;
  return ObjectRef(broker(), element);
}

double HeapNumberRef::value() const {
  IF_BROKER_DISABLED_ACCESS_HANDLE_C(HeapNumber, value);
  return data()->AsHeapNumber()->value();
}

uint64_t BigIntRef::AsUint64() const {
  IF_BROKER_DISABLED_ACCESS_HANDLE_C(BigInt, AsUint64);
  return data()->AsBigInt()->AsUint64();
}

base::Optional<CellRef> SourceTextModuleRef::GetCell(int cell_index) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleAllocation handle_allocation;
    AllowHandleDereference allow_handle_dereference;
    return CellRef(broker(),
                   handle(object()->GetCell(cell_index), broker()->isolate()));
  }
  CellData* cell = data()->AsSourceTextModule()->GetCell(broker(), cell_index);
  if (cell == nullptr) return base::nullopt;
  return CellRef(broker(), cell);
}

ObjectRef::ObjectRef(JSHeapBroker* broker, Handle<Object> object,
                     bool check_type)
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
  if (!data_) {  // TODO(mslekova): Remove once we're on the background thread.
    AllowHandleDereference handle_dereference;
    object->Print();
  }
  CHECK_WITH_MSG(data_ != nullptr, "Object is not known to the heap broker");
}

namespace {
OddballType GetOddballType(Isolate* isolate, Map map) {
  if (map.instance_type() != ODDBALL_TYPE) {
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
    if (map.is_undetectable()) flags |= HeapObjectType::kUndetectable;
    if (map.is_callable()) flags |= HeapObjectType::kCallable;
    return HeapObjectType(map.instance_type(), flags,
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

void FeedbackVectorRef::Serialize() {
  data()->AsFeedbackVector()->Serialize(broker());
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

void JSRegExpRef::SerializeAsRegExpBoilerplate() {
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  JSObjectRef::data()->AsJSRegExp()->SerializeAsRegExpBoilerplate(broker());
}

Handle<Object> ObjectRef::object() const {
#ifdef DEBUG
  if (broker()->mode() == JSHeapBroker::kSerialized &&
      data_->used_status == ObjectData::Usage::kUnused) {
    data_->used_status = ObjectData::Usage::kOnlyIdentityUsed;
  }
#endif  // DEBUG
  return data_->object();
}

#ifdef DEBUG
#define DEF_OBJECT_GETTER(T)                                                 \
  Handle<T> T##Ref::object() const {                                         \
    if (broker()->mode() == JSHeapBroker::kSerialized &&                     \
        data_->used_status == ObjectData::Usage::kUnused) {                  \
      data_->used_status = ObjectData::Usage::kOnlyIdentityUsed;             \
    }                                                                        \
    return Handle<T>(reinterpret_cast<Address*>(data_->object().address())); \
  }
#else
#define DEF_OBJECT_GETTER(T)                                                 \
  Handle<T> T##Ref::object() const {                                         \
    return Handle<T>(reinterpret_cast<Address*>(data_->object().address())); \
  }
#endif  // DEBUG

HEAP_BROKER_OBJECT_LIST(DEF_OBJECT_GETTER)
#undef DEF_OBJECT_GETTER

JSHeapBroker* ObjectRef::broker() const { return broker_; }

ObjectData* ObjectRef::data() const {
  switch (broker()->mode()) {
    case JSHeapBroker::kDisabled:
      CHECK_NE(data_->kind(), kSerializedHeapObject);
      return data_;
    case JSHeapBroker::kSerializing:
      CHECK_NE(data_->kind(), kUnserializedHeapObject);
      return data_;
    case JSHeapBroker::kSerialized:
#ifdef DEBUG
      data_->used_status = ObjectData::Usage::kDataUsed;
#endif  // DEBUG
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

  scope_info_ = broker->GetOrCreateData(context->scope_info())->AsScopeInfo();
}

void JSFunctionRef::Serialize() {
  if (broker()->mode() == JSHeapBroker::kDisabled) return;
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsJSFunction()->Serialize(broker());
}

bool JSBoundFunctionRef::serialized() const {
  if (broker()->mode() == JSHeapBroker::kDisabled) return true;
  return data()->AsJSBoundFunction()->serialized();
}

bool JSFunctionRef::serialized() const {
  if (broker()->mode() == JSHeapBroker::kDisabled) return true;
  return data()->AsJSFunction()->serialized();
}

JSArrayRef SharedFunctionInfoRef::GetTemplateObject(
    TemplateObjectDescriptionRef description, FeedbackSource const& source,
    SerializationPolicy policy) {
  // First, see if we have processed feedback from the vector, respecting
  // the serialization policy.
  ProcessedFeedback const& feedback =
      policy == SerializationPolicy::kSerializeIfNeeded
          ? broker()->ProcessFeedbackForTemplateObject(source)
          : broker()->GetFeedbackForTemplateObject(source);

  if (!feedback.IsInsufficient()) {
    return feedback.AsTemplateObject().value();
  }

  if (broker()->mode() == JSHeapBroker::kDisabled) {
    AllowHandleAllocation handle_allocation;
    AllowHandleDereference allow_handle_dereference;
    Handle<JSArray> template_object =
        TemplateObjectDescription::GetTemplateObject(
            isolate(), broker()->target_native_context().object(),
            description.object(), object(), source.slot.ToInt());
    return JSArrayRef(broker(), template_object);
  }

  JSArrayData* array =
      data()->AsSharedFunctionInfo()->GetTemplateObject(source.slot);
  if (array != nullptr) return JSArrayRef(broker(), array);

  CHECK_EQ(policy, SerializationPolicy::kSerializeIfNeeded);
  CHECK(broker()->SerializingAllowed());

  Handle<JSArray> template_object =
      TemplateObjectDescription::GetTemplateObject(
          broker()->isolate(), broker()->target_native_context().object(),
          description.object(), object(), source.slot.ToInt());
  array = broker()->GetOrCreateData(template_object)->AsJSArray();
  data()->AsSharedFunctionInfo()->SetTemplateObject(source.slot, array);
  return JSArrayRef(broker(), array);
}

void SharedFunctionInfoRef::SetSerializedForCompilation(
    FeedbackVectorRef feedback) {
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  CHECK(HasBytecodeArray());
  data()->AsSharedFunctionInfo()->SetSerializedForCompilation(broker(),
                                                              feedback);
}

void SharedFunctionInfoRef::SerializeFunctionTemplateInfo() {
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsSharedFunctionInfo()->SerializeFunctionTemplateInfo(broker());
}

base::Optional<FunctionTemplateInfoRef>
SharedFunctionInfoRef::function_template_info() const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    if (object()->IsApiFunction()) {
      return FunctionTemplateInfoRef(
          broker(), handle(object()->function_data(), broker()->isolate()));
    }
    return base::nullopt;
  }
  FunctionTemplateInfoData* function_template_info =
      data()->AsSharedFunctionInfo()->function_template_info();
  if (!function_template_info) return base::nullopt;
  return FunctionTemplateInfoRef(broker(), function_template_info);
}

bool SharedFunctionInfoRef::IsSerializedForCompilation(
    FeedbackVectorRef feedback) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) return HasBytecodeArray();
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

void MapRef::SerializeOwnDescriptor(InternalIndex descriptor_index) {
  if (broker()->mode() == JSHeapBroker::kDisabled) return;
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsMap()->SerializeOwnDescriptor(broker(), descriptor_index);
}

bool MapRef::serialized_own_descriptor(InternalIndex descriptor_index) const {
  CHECK_LT(descriptor_index.as_int(), NumberOfOwnDescriptors());
  if (broker()->mode() == JSHeapBroker::kDisabled) return true;
  DescriptorArrayData* desc_array_data =
      data()->AsMap()->instance_descriptors();
  if (!desc_array_data) return false;
  return desc_array_data->contents().find(descriptor_index.as_int()) !=
         desc_array_data->contents().end();
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

void SourceTextModuleRef::Serialize() {
  if (broker()->mode() == JSHeapBroker::kDisabled) return;
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsSourceTextModule()->Serialize(broker());
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

void FunctionTemplateInfoRef::SerializeCallCode() {
  if (broker()->mode() == JSHeapBroker::kDisabled) return;
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsFunctionTemplateInfo()->SerializeCallCode(broker());
}

base::Optional<PropertyCellRef> JSGlobalObjectRef::GetPropertyCell(
    NameRef const& name, SerializationPolicy policy) const {
  if (broker()->mode() == JSHeapBroker::kDisabled) {
    return GetPropertyCellFromHeap(broker(), name.object());
  }
  PropertyCellData* property_cell_data =
      data()->AsJSGlobalObject()->GetPropertyCell(
          broker(), name.data()->AsName(), policy);
  if (property_cell_data == nullptr) return base::nullopt;
  return PropertyCellRef(broker(), property_cell_data);
}

bool CanInlineElementAccess(MapRef const& map) {
  if (!map.IsJSObjectMap()) return false;
  if (map.is_access_check_needed()) return false;
  if (map.has_indexed_interceptor()) return false;
  ElementsKind const elements_kind = map.elements_kind();
  if (IsFastElementsKind(elements_kind)) return true;
  if (IsTypedArrayElementsKind(elements_kind) &&
      elements_kind != BIGUINT64_ELEMENTS &&
      elements_kind != BIGINT64_ELEMENTS) {
    return true;
  }
  return false;
}

ProcessedFeedback::ProcessedFeedback(Kind kind, FeedbackSlotKind slot_kind)
    : kind_(kind), slot_kind_(slot_kind) {}

KeyedAccessMode ElementAccessFeedback::keyed_mode() const {
  return keyed_mode_;
}

ZoneVector<ElementAccessFeedback::TransitionGroup> const&
ElementAccessFeedback::transition_groups() const {
  return transition_groups_;
}

ElementAccessFeedback const& ElementAccessFeedback::Refine(
    ZoneVector<Handle<Map>> const& inferred_maps, Zone* zone) const {
  ElementAccessFeedback& refined_feedback =
      *new (zone) ElementAccessFeedback(zone, keyed_mode(), slot_kind());
  if (inferred_maps.empty()) return refined_feedback;

  ZoneUnorderedSet<Handle<Map>, Handle<Map>::hash, Handle<Map>::equal_to>
      inferred(zone);
  inferred.insert(inferred_maps.begin(), inferred_maps.end());

  for (auto const& group : transition_groups()) {
    DCHECK(!group.empty());
    TransitionGroup new_group(zone);
    for (size_t i = 1; i < group.size(); ++i) {
      Handle<Map> source = group[i];
      if (inferred.find(source) != inferred.end()) {
        new_group.push_back(source);
      }
    }

    Handle<Map> target = group.front();
    bool const keep_target =
        inferred.find(target) != inferred.end() || new_group.size() > 1;
    if (keep_target) {
      new_group.push_back(target);
      // The target must be at the front, the order of sources doesn't matter.
      std::swap(new_group[0], new_group[new_group.size() - 1]);
    }

    if (!new_group.empty()) {
      DCHECK(new_group.size() == 1 || new_group.front().equals(target));
      refined_feedback.transition_groups_.push_back(std::move(new_group));
    }
  }
  return refined_feedback;
}

InsufficientFeedback::InsufficientFeedback(FeedbackSlotKind slot_kind)
    : ProcessedFeedback(kInsufficient, slot_kind) {}

GlobalAccessFeedback::GlobalAccessFeedback(PropertyCellRef cell,
                                           FeedbackSlotKind slot_kind)
    : ProcessedFeedback(kGlobalAccess, slot_kind),
      cell_or_context_(cell),
      index_and_immutable_(0 /* doesn't matter */) {
  DCHECK(IsGlobalICKind(slot_kind));
}

GlobalAccessFeedback::GlobalAccessFeedback(FeedbackSlotKind slot_kind)
    : ProcessedFeedback(kGlobalAccess, slot_kind),
      index_and_immutable_(0 /* doesn't matter */) {
  DCHECK(IsGlobalICKind(slot_kind));
}

GlobalAccessFeedback::GlobalAccessFeedback(ContextRef script_context,
                                           int slot_index, bool immutable,
                                           FeedbackSlotKind slot_kind)
    : ProcessedFeedback(kGlobalAccess, slot_kind),
      cell_or_context_(script_context),
      index_and_immutable_(FeedbackNexus::SlotIndexBits::encode(slot_index) |
                           FeedbackNexus::ImmutabilityBit::encode(immutable)) {
  DCHECK_EQ(this->slot_index(), slot_index);
  DCHECK_EQ(this->immutable(), immutable);
  DCHECK(IsGlobalICKind(slot_kind));
}

bool GlobalAccessFeedback::IsMegamorphic() const {
  return !cell_or_context_.has_value();
}
bool GlobalAccessFeedback::IsPropertyCell() const {
  return cell_or_context_.has_value() && cell_or_context_->IsPropertyCell();
}
bool GlobalAccessFeedback::IsScriptContextSlot() const {
  return cell_or_context_.has_value() && cell_or_context_->IsContext();
}
PropertyCellRef GlobalAccessFeedback::property_cell() const {
  CHECK(IsPropertyCell());
  return cell_or_context_->AsPropertyCell();
}
ContextRef GlobalAccessFeedback::script_context() const {
  CHECK(IsScriptContextSlot());
  return cell_or_context_->AsContext();
}
int GlobalAccessFeedback::slot_index() const {
  DCHECK(IsScriptContextSlot());
  return FeedbackNexus::SlotIndexBits::decode(index_and_immutable_);
}
bool GlobalAccessFeedback::immutable() const {
  DCHECK(IsScriptContextSlot());
  return FeedbackNexus::ImmutabilityBit::decode(index_and_immutable_);
}

base::Optional<ObjectRef> GlobalAccessFeedback::GetConstantHint() const {
  if (IsPropertyCell()) {
    return property_cell().value();
  } else if (IsScriptContextSlot() && immutable()) {
    return script_context().get(slot_index());
  } else {
    return base::nullopt;
  }
}

KeyedAccessMode KeyedAccessMode::FromNexus(FeedbackNexus const& nexus) {
  FeedbackSlotKind kind = nexus.kind();
  if (IsKeyedLoadICKind(kind)) {
    return KeyedAccessMode(AccessMode::kLoad, nexus.GetKeyedAccessLoadMode());
  }
  if (IsKeyedHasICKind(kind)) {
    return KeyedAccessMode(AccessMode::kHas, nexus.GetKeyedAccessLoadMode());
  }
  if (IsKeyedStoreICKind(kind)) {
    return KeyedAccessMode(AccessMode::kStore, nexus.GetKeyedAccessStoreMode());
  }
  if (IsStoreInArrayLiteralICKind(kind) ||
      IsStoreDataPropertyInLiteralKind(kind)) {
    return KeyedAccessMode(AccessMode::kStoreInLiteral,
                           nexus.GetKeyedAccessStoreMode());
  }
  UNREACHABLE();
}

AccessMode KeyedAccessMode::access_mode() const { return access_mode_; }

bool KeyedAccessMode::IsLoad() const {
  return access_mode_ == AccessMode::kLoad || access_mode_ == AccessMode::kHas;
}
bool KeyedAccessMode::IsStore() const {
  return access_mode_ == AccessMode::kStore ||
         access_mode_ == AccessMode::kStoreInLiteral;
}

KeyedAccessLoadMode KeyedAccessMode::load_mode() const {
  CHECK(IsLoad());
  return load_store_mode_.load_mode;
}

KeyedAccessStoreMode KeyedAccessMode::store_mode() const {
  CHECK(IsStore());
  return load_store_mode_.store_mode;
}

KeyedAccessMode::LoadStoreMode::LoadStoreMode(KeyedAccessLoadMode load_mode)
    : load_mode(load_mode) {}
KeyedAccessMode::LoadStoreMode::LoadStoreMode(KeyedAccessStoreMode store_mode)
    : store_mode(store_mode) {}

KeyedAccessMode::KeyedAccessMode(AccessMode access_mode,
                                 KeyedAccessLoadMode load_mode)
    : access_mode_(access_mode), load_store_mode_(load_mode) {
  CHECK(!IsStore());
  CHECK(IsLoad());
}
KeyedAccessMode::KeyedAccessMode(AccessMode access_mode,
                                 KeyedAccessStoreMode store_mode)
    : access_mode_(access_mode), load_store_mode_(store_mode) {
  CHECK(!IsLoad());
  CHECK(IsStore());
}

ElementAccessFeedback::ElementAccessFeedback(Zone* zone,
                                             KeyedAccessMode const& keyed_mode,
                                             FeedbackSlotKind slot_kind)
    : ProcessedFeedback(kElementAccess, slot_kind),
      keyed_mode_(keyed_mode),
      transition_groups_(zone) {
  DCHECK(IsKeyedLoadICKind(slot_kind) || IsKeyedHasICKind(slot_kind) ||
         IsStoreDataPropertyInLiteralKind(slot_kind) ||
         IsKeyedStoreICKind(slot_kind) ||
         IsStoreInArrayLiteralICKind(slot_kind));
}

bool ElementAccessFeedback::HasOnlyStringMaps(JSHeapBroker* broker) const {
  for (auto const& group : transition_groups()) {
    for (Handle<Map> map : group) {
      if (!MapRef(broker, map).IsStringMap()) return false;
    }
  }
  return true;
}

NamedAccessFeedback::NamedAccessFeedback(NameRef const& name,
                                         ZoneVector<Handle<Map>> const& maps,
                                         FeedbackSlotKind slot_kind)
    : ProcessedFeedback(kNamedAccess, slot_kind), name_(name), maps_(maps) {
  DCHECK(IsLoadICKind(slot_kind) || IsStoreICKind(slot_kind) ||
         IsStoreOwnICKind(slot_kind) || IsKeyedLoadICKind(slot_kind) ||
         IsKeyedHasICKind(slot_kind) || IsKeyedStoreICKind(slot_kind) ||
         IsStoreInArrayLiteralICKind(slot_kind) ||
         IsStoreDataPropertyInLiteralKind(slot_kind));
}

void JSHeapBroker::SetFeedback(FeedbackSource const& source,
                               ProcessedFeedback const* feedback) {
  CHECK(source.IsValid());
  auto insertion = feedback_.insert({source, feedback});
  CHECK(insertion.second);
}

bool JSHeapBroker::HasFeedback(FeedbackSource const& source) const {
  DCHECK(source.IsValid());
  return feedback_.find(source) != feedback_.end();
}

ProcessedFeedback const& JSHeapBroker::GetFeedback(
    FeedbackSource const& source) const {
  DCHECK(source.IsValid());
  auto it = feedback_.find(source);
  CHECK_NE(it, feedback_.end());
  return *it->second;
}

FeedbackSlotKind JSHeapBroker::GetFeedbackSlotKind(
    FeedbackSource const& source) const {
  if (FLAG_concurrent_inlining) {
    ProcessedFeedback const& processed = GetFeedback(source);
    return processed.slot_kind();
  }
  FeedbackNexus nexus(source.vector, source.slot);
  return nexus.kind();
}

bool JSHeapBroker::FeedbackIsInsufficient(FeedbackSource const& source) const {
  return FLAG_concurrent_inlining
             ? GetFeedback(source).IsInsufficient()
             : FeedbackNexus(source.vector, source.slot).IsUninitialized();
}

namespace {
MapHandles GetRelevantReceiverMaps(Isolate* isolate, MapHandles const& maps) {
  MapHandles result;
  for (Handle<Map> map : maps) {
    if (Map::TryUpdate(isolate, map).ToHandle(&map) &&
        !map->is_abandoned_prototype_map()) {
      DCHECK(!map->is_deprecated());
      result.push_back(map);
    }
  }
  return result;
}  // namespace
}  // namespace

ProcessedFeedback const& JSHeapBroker::ReadFeedbackForPropertyAccess(
    FeedbackSource const& source, AccessMode mode,
    base::Optional<NameRef> static_name) {
  FeedbackNexus nexus(source.vector, source.slot);
  FeedbackSlotKind kind = nexus.kind();
  if (nexus.IsUninitialized()) return *new (zone()) InsufficientFeedback(kind);

  MapHandles maps;
  nexus.ExtractMaps(&maps);
  if (!maps.empty()) {
    maps = GetRelevantReceiverMaps(isolate(), maps);
    if (maps.empty()) return *new (zone()) InsufficientFeedback(kind);
  }

  base::Optional<NameRef> name =
      static_name.has_value() ? static_name : GetNameFeedback(nexus);
  if (name.has_value()) {
    return *new (zone()) NamedAccessFeedback(
        *name, ZoneVector<Handle<Map>>(maps.begin(), maps.end(), zone()), kind);
  } else if (nexus.GetKeyType() == ELEMENT && !maps.empty()) {
    return ProcessFeedbackMapsForElementAccess(
        maps, KeyedAccessMode::FromNexus(nexus), kind);
  } else {
    // No actionable feedback.
    DCHECK(maps.empty());
    // TODO(neis): Investigate if we really want to treat cleared the same as
    // megamorphic (also for global accesses).
    // TODO(neis): Using ElementAccessFeedback here is kind of an abuse.
    return *new (zone())
        ElementAccessFeedback(zone(), KeyedAccessMode::FromNexus(nexus), kind);
  }
}

ProcessedFeedback const& JSHeapBroker::ReadFeedbackForGlobalAccess(
    FeedbackSource const& source) {
  FeedbackNexus nexus(source.vector, source.slot);
  DCHECK(nexus.kind() == FeedbackSlotKind::kLoadGlobalInsideTypeof ||
         nexus.kind() == FeedbackSlotKind::kLoadGlobalNotInsideTypeof ||
         nexus.kind() == FeedbackSlotKind::kStoreGlobalSloppy ||
         nexus.kind() == FeedbackSlotKind::kStoreGlobalStrict);
  if (nexus.IsUninitialized()) {
    return *new (zone()) InsufficientFeedback(nexus.kind());
  }
  if (nexus.ic_state() != MONOMORPHIC || nexus.GetFeedback()->IsCleared()) {
    return *new (zone()) GlobalAccessFeedback(nexus.kind());
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
        isolate(), target_native_context().script_context_table().object(),
        script_context_index);
    {
      ObjectRef contents(this,
                         handle(context->get(context_slot_index), isolate()));
      CHECK(!contents.equals(
          ObjectRef(this, isolate()->factory()->the_hole_value())));
    }
    ContextRef context_ref(this, context);
    if (immutable) {
      context_ref.get(context_slot_index,
                      SerializationPolicy::kSerializeIfNeeded);
    }
    return *new (zone()) GlobalAccessFeedback(context_ref, context_slot_index,
                                              immutable, nexus.kind());
  }

  CHECK(feedback_value->IsPropertyCell());
  // The wanted name belongs (or did belong) to a property on the global
  // object and the feedback is the cell holding its value.
  PropertyCellRef cell(this, Handle<PropertyCell>::cast(feedback_value));
  cell.Serialize();
  return *new (zone()) GlobalAccessFeedback(cell, nexus.kind());
}

BinaryOperationHint JSHeapBroker::ReadFeedbackForBinaryOperation(
    FeedbackSource const& source) const {
  return FeedbackNexus(source.vector, source.slot).GetBinaryOperationFeedback();
}

CompareOperationHint JSHeapBroker::ReadFeedbackForCompareOperation(
    FeedbackSource const& source) const {
  return FeedbackNexus(source.vector, source.slot)
      .GetCompareOperationFeedback();
}

ForInHint JSHeapBroker::ReadFeedbackForForIn(
    FeedbackSource const& source) const {
  return FeedbackNexus(source.vector, source.slot).GetForInFeedback();
}

ProcessedFeedback const& JSHeapBroker::ReadFeedbackForInstanceOf(
    FeedbackSource const& source) {
  FeedbackNexus nexus(source.vector, source.slot);
  if (nexus.IsUninitialized())
    return *new (zone()) InsufficientFeedback(nexus.kind());

  base::Optional<JSObjectRef> optional_constructor;
  {
    MaybeHandle<JSObject> maybe_constructor = nexus.GetConstructorFeedback();
    Handle<JSObject> constructor;
    if (maybe_constructor.ToHandle(&constructor)) {
      optional_constructor = JSObjectRef(this, constructor);
    }
  }
  return *new (zone()) InstanceOfFeedback(optional_constructor, nexus.kind());
}

ProcessedFeedback const& JSHeapBroker::ReadFeedbackForArrayOrObjectLiteral(
    FeedbackSource const& source) {
  FeedbackNexus nexus(source.vector, source.slot);
  HeapObject object;
  if (nexus.IsUninitialized() || !nexus.GetFeedback()->GetHeapObject(&object)) {
    return *new (zone()) InsufficientFeedback(nexus.kind());
  }

  AllocationSiteRef site(this, handle(object, isolate()));
  if (site.IsFastLiteral()) {
    site.SerializeBoilerplate();
  }

  return *new (zone()) LiteralFeedback(site, nexus.kind());
}

ProcessedFeedback const& JSHeapBroker::ReadFeedbackForRegExpLiteral(
    FeedbackSource const& source) {
  FeedbackNexus nexus(source.vector, source.slot);
  HeapObject object;
  if (nexus.IsUninitialized() || !nexus.GetFeedback()->GetHeapObject(&object)) {
    return *new (zone()) InsufficientFeedback(nexus.kind());
  }

  JSRegExpRef regexp(this, handle(object, isolate()));
  regexp.SerializeAsRegExpBoilerplate();
  return *new (zone()) RegExpLiteralFeedback(regexp, nexus.kind());
}

ProcessedFeedback const& JSHeapBroker::ReadFeedbackForTemplateObject(
    FeedbackSource const& source) {
  FeedbackNexus nexus(source.vector, source.slot);
  HeapObject object;
  if (nexus.IsUninitialized() || !nexus.GetFeedback()->GetHeapObject(&object)) {
    return *new (zone()) InsufficientFeedback(nexus.kind());
  }

  JSArrayRef array(this, handle(object, isolate()));
  return *new (zone()) TemplateObjectFeedback(array, nexus.kind());
}

ProcessedFeedback const& JSHeapBroker::ReadFeedbackForCall(
    FeedbackSource const& source) {
  FeedbackNexus nexus(source.vector, source.slot);
  if (nexus.IsUninitialized())
    return *new (zone()) InsufficientFeedback(nexus.kind());

  base::Optional<HeapObjectRef> target_ref;
  {
    MaybeObject maybe_target = nexus.GetFeedback();
    HeapObject target_object;
    if (maybe_target->GetHeapObject(&target_object)) {
      target_ref = HeapObjectRef(this, handle(target_object, isolate()));
    }
  }
  float frequency = nexus.ComputeCallFrequency();
  SpeculationMode mode = nexus.GetSpeculationMode();
  return *new (zone()) CallFeedback(target_ref, frequency, mode, nexus.kind());
}

BinaryOperationHint JSHeapBroker::GetFeedbackForBinaryOperation(
    FeedbackSource const& source) {
  ProcessedFeedback const& feedback =
      FLAG_concurrent_inlining ? GetFeedback(source)
                               : ProcessFeedbackForBinaryOperation(source);
  return feedback.IsInsufficient() ? BinaryOperationHint::kNone
                                   : feedback.AsBinaryOperation().value();
}

CompareOperationHint JSHeapBroker::GetFeedbackForCompareOperation(
    FeedbackSource const& source) {
  ProcessedFeedback const& feedback =
      FLAG_concurrent_inlining ? GetFeedback(source)
                               : ProcessFeedbackForCompareOperation(source);
  return feedback.IsInsufficient() ? CompareOperationHint::kNone
                                   : feedback.AsCompareOperation().value();
}

ForInHint JSHeapBroker::GetFeedbackForForIn(FeedbackSource const& source) {
  ProcessedFeedback const& feedback = FLAG_concurrent_inlining
                                          ? GetFeedback(source)
                                          : ProcessFeedbackForForIn(source);
  return feedback.IsInsufficient() ? ForInHint::kNone
                                   : feedback.AsForIn().value();
}

ProcessedFeedback const& JSHeapBroker::GetFeedbackForPropertyAccess(
    FeedbackSource const& source, AccessMode mode,
    base::Optional<NameRef> static_name) {
  return FLAG_concurrent_inlining
             ? GetFeedback(source)
             : ProcessFeedbackForPropertyAccess(source, mode, static_name);
}

ProcessedFeedback const& JSHeapBroker::GetFeedbackForInstanceOf(
    FeedbackSource const& source) {
  return FLAG_concurrent_inlining ? GetFeedback(source)
                                  : ProcessFeedbackForInstanceOf(source);
}

ProcessedFeedback const& JSHeapBroker::GetFeedbackForCall(
    FeedbackSource const& source) {
  return FLAG_concurrent_inlining ? GetFeedback(source)
                                  : ProcessFeedbackForCall(source);
}

ProcessedFeedback const& JSHeapBroker::GetFeedbackForGlobalAccess(
    FeedbackSource const& source) {
  return FLAG_concurrent_inlining ? GetFeedback(source)
                                  : ProcessFeedbackForGlobalAccess(source);
}

ProcessedFeedback const& JSHeapBroker::GetFeedbackForArrayOrObjectLiteral(
    FeedbackSource const& source) {
  return FLAG_concurrent_inlining
             ? GetFeedback(source)
             : ProcessFeedbackForArrayOrObjectLiteral(source);
}

ProcessedFeedback const& JSHeapBroker::GetFeedbackForRegExpLiteral(
    FeedbackSource const& source) {
  return FLAG_concurrent_inlining ? GetFeedback(source)
                                  : ProcessFeedbackForRegExpLiteral(source);
}

ProcessedFeedback const& JSHeapBroker::GetFeedbackForTemplateObject(
    FeedbackSource const& source) {
  return FLAG_concurrent_inlining ? GetFeedback(source)
                                  : ProcessFeedbackForTemplateObject(source);
}

ProcessedFeedback const& JSHeapBroker::ProcessFeedbackForArrayOrObjectLiteral(
    FeedbackSource const& source) {
  if (HasFeedback(source)) return GetFeedback(source);
  ProcessedFeedback const& feedback =
      ReadFeedbackForArrayOrObjectLiteral(source);
  SetFeedback(source, &feedback);
  return feedback;
}

ProcessedFeedback const& JSHeapBroker::ProcessFeedbackForRegExpLiteral(
    FeedbackSource const& source) {
  if (HasFeedback(source)) return GetFeedback(source);
  ProcessedFeedback const& feedback = ReadFeedbackForRegExpLiteral(source);
  SetFeedback(source, &feedback);
  return feedback;
}

ProcessedFeedback const& JSHeapBroker::ProcessFeedbackForTemplateObject(
    FeedbackSource const& source) {
  if (HasFeedback(source)) return GetFeedback(source);
  ProcessedFeedback const& feedback = ReadFeedbackForTemplateObject(source);
  SetFeedback(source, &feedback);
  return feedback;
}

ProcessedFeedback const& JSHeapBroker::ProcessFeedbackForBinaryOperation(
    FeedbackSource const& source) {
  if (HasFeedback(source)) return GetFeedback(source);
  BinaryOperationHint hint = ReadFeedbackForBinaryOperation(source);
  ProcessedFeedback const* feedback;
  if (hint == BinaryOperationHint::kNone) {
    feedback =
        new (zone()) InsufficientFeedback(source.vector->GetKind(source.slot));
  } else {
    feedback = new (zone())
        BinaryOperationFeedback(hint, source.vector->GetKind(source.slot));
  }
  SetFeedback(source, feedback);
  return *feedback;
}

ProcessedFeedback const& JSHeapBroker::ProcessFeedbackForCompareOperation(
    FeedbackSource const& source) {
  if (HasFeedback(source)) return GetFeedback(source);
  CompareOperationHint hint = ReadFeedbackForCompareOperation(source);
  ProcessedFeedback const* feedback;
  if (hint == CompareOperationHint::kNone) {
    feedback =
        new (zone()) InsufficientFeedback(source.vector->GetKind(source.slot));
  } else {
    feedback = new (zone())
        CompareOperationFeedback(hint, source.vector->GetKind(source.slot));
  }
  SetFeedback(source, feedback);
  return *feedback;
}

ProcessedFeedback const& JSHeapBroker::ProcessFeedbackForForIn(
    FeedbackSource const& source) {
  if (HasFeedback(source)) return GetFeedback(source);
  ForInHint hint = ReadFeedbackForForIn(source);
  ProcessedFeedback const* feedback;
  if (hint == ForInHint::kNone) {
    feedback =
        new (zone()) InsufficientFeedback(source.vector->GetKind(source.slot));
  } else {
    feedback =
        new (zone()) ForInFeedback(hint, source.vector->GetKind(source.slot));
  }
  SetFeedback(source, feedback);
  return *feedback;
}

ProcessedFeedback const& JSHeapBroker::ProcessFeedbackForPropertyAccess(
    FeedbackSource const& source, AccessMode mode,
    base::Optional<NameRef> static_name) {
  if (HasFeedback(source)) return GetFeedback(source);
  ProcessedFeedback const& feedback =
      ReadFeedbackForPropertyAccess(source, mode, static_name);
  SetFeedback(source, &feedback);
  return feedback;
}

ProcessedFeedback const& JSHeapBroker::ProcessFeedbackForInstanceOf(
    FeedbackSource const& source) {
  if (HasFeedback(source)) return GetFeedback(source);
  ProcessedFeedback const& feedback = ReadFeedbackForInstanceOf(source);
  SetFeedback(source, &feedback);
  return feedback;
}

ProcessedFeedback const& JSHeapBroker::ProcessFeedbackForCall(
    FeedbackSource const& source) {
  if (HasFeedback(source)) return GetFeedback(source);
  ProcessedFeedback const& feedback = ReadFeedbackForCall(source);
  SetFeedback(source, &feedback);
  return feedback;
}

ProcessedFeedback const& JSHeapBroker::ProcessFeedbackForGlobalAccess(
    FeedbackSource const& source) {
  if (HasFeedback(source)) return GetFeedback(source);
  ProcessedFeedback const& feedback = ReadFeedbackForGlobalAccess(source);
  SetFeedback(source, &feedback);
  return feedback;
}

ElementAccessFeedback const& JSHeapBroker::ProcessFeedbackMapsForElementAccess(
    MapHandles const& maps, KeyedAccessMode const& keyed_mode,
    FeedbackSlotKind slot_kind) {
  DCHECK(!maps.empty());

  // Collect possible transition targets.
  MapHandles possible_transition_targets;
  possible_transition_targets.reserve(maps.size());
  for (Handle<Map> map : maps) {
    MapRef map_ref(this, map);
    map_ref.SerializeRootMap();

    if (CanInlineElementAccess(map_ref) &&
        IsFastElementsKind(map->elements_kind()) &&
        GetInitialFastElementsKind() != map->elements_kind()) {
      possible_transition_targets.push_back(map);
    }
  }

  using TransitionGroup = ElementAccessFeedback::TransitionGroup;
  ZoneUnorderedMap<Handle<Map>, TransitionGroup, Handle<Map>::hash,
                   Handle<Map>::equal_to>
      transition_groups(zone());

  // Separate the actual receiver maps and the possible transition sources.
  for (Handle<Map> map : maps) {
    // Don't generate elements kind transitions from stable maps.
    Map transition_target = map->is_stable()
                                ? Map()
                                : map->FindElementsKindTransitionedMap(
                                      isolate(), possible_transition_targets);
    if (transition_target.is_null()) {
      TransitionGroup group(1, map, zone());
      transition_groups.insert({map, group});
    } else {
      Handle<Map> target(transition_target, isolate());
      TransitionGroup new_group(1, target, zone());
      TransitionGroup& actual_group =
          transition_groups.insert({target, new_group}).first->second;
      actual_group.push_back(map);
    }
  }

  ElementAccessFeedback* result =
      new (zone()) ElementAccessFeedback(zone(), keyed_mode, slot_kind);
  for (auto entry : transition_groups) {
    result->AddGroup(std::move(entry.second));
  }

  CHECK(!result->transition_groups().empty());
  return *result;
}

void ElementAccessFeedback::AddGroup(TransitionGroup&& group) {
  CHECK(!group.empty());
  transition_groups_.push_back(std::move(group));

#ifdef ENABLE_SLOW_DCHECKS
  // Check that each of the group's maps occurs exactly once in the whole
  // feedback. This implies that "a source is not a target".
  for (Handle<Map> map : group) {
    int count = 0;
    for (TransitionGroup const& some_group : transition_groups()) {
      count += std::count_if(
          some_group.begin(), some_group.end(),
          [&](Handle<Map> some_map) { return some_map.equals(map); });
    }
    CHECK_EQ(count, 1);
  }
#endif
}

std::ostream& operator<<(std::ostream& os, const ObjectRef& ref) {
  if (ref.broker()->mode() == JSHeapBroker::kDisabled ||
      !FLAG_concurrent_recompilation) {
    // We cannot be in a background thread so it's safe to read the heap.
    AllowHandleDereference allow_handle_dereference;
    return os << ref.data() << " {" << ref.object() << "}";
  } else {
    return os << ref.data();
  }
}

base::Optional<NameRef> JSHeapBroker::GetNameFeedback(
    FeedbackNexus const& nexus) {
  Name raw_name = nexus.GetName();
  if (raw_name.is_null()) return base::nullopt;
  return NameRef(this, handle(raw_name, isolate()));
}

PropertyAccessInfo JSHeapBroker::GetPropertyAccessInfo(
    MapRef map, NameRef name, AccessMode access_mode,
    CompilationDependencies* dependencies, SerializationPolicy policy) {
  PropertyAccessTarget target({map, name, access_mode});
  auto it = property_access_infos_.find(target);
  if (it != property_access_infos_.end()) return it->second;

  if (policy == SerializationPolicy::kAssumeSerialized) {
    TRACE_BROKER_MISSING(this, "PropertyAccessInfo for "
                                   << access_mode << " of property " << name
                                   << " on map " << map);
    return PropertyAccessInfo::Invalid(zone());
  }

  CHECK_NOT_NULL(dependencies);
  AccessInfoFactory factory(this, dependencies, zone());
  PropertyAccessInfo access_info = factory.ComputePropertyAccessInfo(
      map.object(), name.object(), access_mode);
  if (FLAG_concurrent_inlining) {
    CHECK(SerializingAllowed());
    TRACE(this, "Storing PropertyAccessInfo for "
                    << access_mode << " of property " << name << " on map "
                    << map);
    property_access_infos_.insert({target, access_info});
  }
  return access_info;
}

BinaryOperationFeedback const& ProcessedFeedback::AsBinaryOperation() const {
  CHECK_EQ(kBinaryOperation, kind());
  return *static_cast<BinaryOperationFeedback const*>(this);
}

CallFeedback const& ProcessedFeedback::AsCall() const {
  CHECK_EQ(kCall, kind());
  return *static_cast<CallFeedback const*>(this);
}

CompareOperationFeedback const& ProcessedFeedback::AsCompareOperation() const {
  CHECK_EQ(kCompareOperation, kind());
  return *static_cast<CompareOperationFeedback const*>(this);
}

ElementAccessFeedback const& ProcessedFeedback::AsElementAccess() const {
  CHECK_EQ(kElementAccess, kind());
  return *static_cast<ElementAccessFeedback const*>(this);
}

ForInFeedback const& ProcessedFeedback::AsForIn() const {
  CHECK_EQ(kForIn, kind());
  return *static_cast<ForInFeedback const*>(this);
}

GlobalAccessFeedback const& ProcessedFeedback::AsGlobalAccess() const {
  CHECK_EQ(kGlobalAccess, kind());
  return *static_cast<GlobalAccessFeedback const*>(this);
}

InstanceOfFeedback const& ProcessedFeedback::AsInstanceOf() const {
  CHECK_EQ(kInstanceOf, kind());
  return *static_cast<InstanceOfFeedback const*>(this);
}

NamedAccessFeedback const& ProcessedFeedback::AsNamedAccess() const {
  CHECK_EQ(kNamedAccess, kind());
  return *static_cast<NamedAccessFeedback const*>(this);
}

LiteralFeedback const& ProcessedFeedback::AsLiteral() const {
  CHECK_EQ(kLiteral, kind());
  return *static_cast<LiteralFeedback const*>(this);
}

RegExpLiteralFeedback const& ProcessedFeedback::AsRegExpLiteral() const {
  CHECK_EQ(kRegExpLiteral, kind());
  return *static_cast<RegExpLiteralFeedback const*>(this);
}

TemplateObjectFeedback const& ProcessedFeedback::AsTemplateObject() const {
  CHECK_EQ(kTemplateObject, kind());
  return *static_cast<TemplateObjectFeedback const*>(this);
}

BytecodeAnalysis const& JSHeapBroker::GetBytecodeAnalysis(
    Handle<BytecodeArray> bytecode_array, BailoutId osr_bailout_id,
    bool analyze_liveness, SerializationPolicy policy) {
  ObjectData* bytecode_array_data = GetData(bytecode_array);
  CHECK_NOT_NULL(bytecode_array_data);

  auto it = bytecode_analyses_.find(bytecode_array_data);
  if (it != bytecode_analyses_.end()) {
    // Bytecode analysis can be run for OSR or for non-OSR. In the rare case
    // where we optimize for OSR and consider the top-level function itself for
    // inlining (because of recursion), we need both the OSR and the non-OSR
    // analysis. Fortunately, the only difference between the two lies in
    // whether the OSR entry offset gets computed (from the OSR bailout id).
    // Hence it's okay to reuse the OSR-version when asked for the non-OSR
    // version, such that we need to store at most one analysis result per
    // bytecode array.
    CHECK_IMPLIES(osr_bailout_id != it->second->osr_bailout_id(),
                  osr_bailout_id.IsNone());
    CHECK_EQ(analyze_liveness, it->second->liveness_analyzed());
    return *it->second;
  }

  CHECK_EQ(policy, SerializationPolicy::kSerializeIfNeeded);
  BytecodeAnalysis* analysis = new (zone()) BytecodeAnalysis(
      bytecode_array, zone(), osr_bailout_id, analyze_liveness);
  DCHECK_EQ(analysis->osr_bailout_id(), osr_bailout_id);
  bytecode_analyses_[bytecode_array_data] = analysis;
  return *analysis;
}

OffHeapBytecodeArray::OffHeapBytecodeArray(BytecodeArrayRef bytecode_array)
    : array_(bytecode_array) {}

int OffHeapBytecodeArray::length() const { return array_.length(); }

int OffHeapBytecodeArray::parameter_count() const {
  return array_.parameter_count();
}

uint8_t OffHeapBytecodeArray::get(int index) const { return array_.get(index); }

void OffHeapBytecodeArray::set(int index, uint8_t value) { UNREACHABLE(); }

Address OffHeapBytecodeArray::GetFirstBytecodeAddress() const {
  return array_.GetFirstBytecodeAddress();
}

Handle<Object> OffHeapBytecodeArray::GetConstantAtIndex(
    int index, Isolate* isolate) const {
  return array_.GetConstantAtIndex(index);
}

bool OffHeapBytecodeArray::IsConstantAtIndexSmi(int index) const {
  return array_.IsConstantAtIndexSmi(index);
}

Smi OffHeapBytecodeArray::GetConstantAtIndexAsSmi(int index) const {
  return array_.GetConstantAtIndexAsSmi(index);
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
