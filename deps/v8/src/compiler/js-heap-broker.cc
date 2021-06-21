// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-heap-broker.h"

#include "src/common/globals.h"
#include "src/compiler/heap-refs.h"

#ifdef ENABLE_SLOW_DCHECKS
#include <algorithm>
#endif

#include "include/v8-fast-api-calls.h"
#include "src/api/api-inl.h"
#include "src/ast/modules.h"
#include "src/codegen/code-factory.h"
#include "src/codegen/optimized-compilation-info.h"
#include "src/compiler/access-info.h"
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
#include "src/objects/property-details.h"
#include "src/objects/template-objects-inl.h"
#include "src/objects/templates.h"
#include "src/utils/utils.h"

namespace v8 {
namespace internal {
namespace compiler {

#define TRACE(broker, x) TRACE_BROKER(broker, x)
#define TRACE_MISSING(broker, x) TRACE_BROKER_MISSING(broker, x)

#define FORWARD_DECL(Name) class Name##Data;
HEAP_BROKER_SERIALIZED_OBJECT_LIST(FORWARD_DECL)
HEAP_BROKER_POSSIBLY_BACKGROUND_SERIALIZED_OBJECT_LIST(FORWARD_DECL)
HEAP_BROKER_BACKGROUND_SERIALIZED_OBJECT_LIST(FORWARD_DECL)
HEAP_BROKER_NEVER_SERIALIZED_OBJECT_LIST(FORWARD_DECL)
#undef FORWARD_DECL

// There are several kinds of ObjectData values.
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
// kBackgroundSerializedHeapObject: Like kSerializedHeapObject, but
//   allows serialization from the background thread.
//
// kUnserializedHeapObject: The underlying V8 object is a HeapObject and the
//   data is an instance of the base class (ObjectData), i.e. it basically
//   carries no information other than the handle.
//
// kNeverSerializedHeapObject: The underlying V8 object is a (potentially
//   mutable) HeapObject and the data is an instance of ObjectData. Its handle
//   must be persistent so that the GC can update it at a safepoint. Via this
//   handle, the object can be accessed concurrently to the main thread. To be
//   used the flag --turbo-direct-heap-access must be on.
//
// kUnserializedReadOnlyHeapObject: The underlying V8 object is a read-only
//   HeapObject and the data is an instance of ObjectData. For
//   ReadOnlyHeapObjects, it is OK to access heap even from off-thread, so
//   these objects need not be serialized.
enum ObjectDataKind {
  kSmi,
  kSerializedHeapObject,
  kBackgroundSerializedHeapObject,
  kUnserializedHeapObject,
  kNeverSerializedHeapObject,
  kUnserializedReadOnlyHeapObject
};

namespace {
bool IsReadOnlyHeapObject(Object object) {
  DisallowGarbageCollection no_gc;
  return (object.IsCode() && Code::cast(object).is_builtin()) ||
         (object.IsHeapObject() &&
          ReadOnlyHeap::Contains(HeapObject::cast(object)));
}

template <class T>
constexpr bool IsSerializedHeapObject() {
  return false;
}

#define DEFINE_MARKER(Name)                       \
  template <>                                     \
  constexpr bool IsSerializedHeapObject<Name>() { \
    return true;                                  \
  }                                               \
  STATIC_ASSERT(IsSerializedHeapObject<Name>());
HEAP_BROKER_SERIALIZED_OBJECT_LIST(DEFINE_MARKER)
#undef DEFINE_MARKER
}  // namespace

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

    // It is safe to access read only heap objects and builtins from a
    // background thread. When we read fileds of these objects, we may create
    // ObjectData on the background thread even without a canonical handle
    // scope. This is safe too since we don't create handles but just get
    // handles from read only root table or builtins table which is what
    // canonical scope uses as well. For all other objects we should have
    // created ObjectData in canonical handle scope on the main thread.
    CHECK_IMPLIES(
        broker->mode() == JSHeapBroker::kDisabled ||
            broker->mode() == JSHeapBroker::kSerializing,
        broker->isolate()->handle_scope_data()->canonical_scope != nullptr);
    CHECK_IMPLIES(broker->mode() == JSHeapBroker::kSerialized,
                  kind == kUnserializedReadOnlyHeapObject || kind == kSmi ||
                      kind == kNeverSerializedHeapObject ||
                      kind == kBackgroundSerializedHeapObject);
    CHECK_IMPLIES(kind == kUnserializedReadOnlyHeapObject,
                  IsReadOnlyHeapObject(*object));
  }

#define DECLARE_IS(Name) bool Is##Name() const;
  HEAP_BROKER_SERIALIZED_OBJECT_LIST(DECLARE_IS)
  HEAP_BROKER_POSSIBLY_BACKGROUND_SERIALIZED_OBJECT_LIST(DECLARE_IS)
  HEAP_BROKER_BACKGROUND_SERIALIZED_OBJECT_LIST(DECLARE_IS)
  HEAP_BROKER_NEVER_SERIALIZED_OBJECT_LIST(DECLARE_IS)
#undef DECLARE_IS

#define DECLARE_AS(Name) Name##Data* As##Name();
  HEAP_BROKER_SERIALIZED_OBJECT_LIST(DECLARE_AS)
  HEAP_BROKER_POSSIBLY_BACKGROUND_SERIALIZED_OBJECT_LIST(DECLARE_AS)
  HEAP_BROKER_BACKGROUND_SERIALIZED_OBJECT_LIST(DECLARE_AS)
  HEAP_BROKER_NEVER_SERIALIZED_OBJECT_LIST(DECLARE_AS)
#undef DECLARE_AS

  Handle<Object> object() const { return object_; }
  ObjectDataKind kind() const { return kind_; }
  bool is_smi() const { return kind_ == kSmi; }
  bool should_access_heap() const {
    return kind_ == kUnserializedHeapObject ||
           kind_ == kNeverSerializedHeapObject ||
           kind_ == kUnserializedReadOnlyHeapObject;
  }
  bool IsNull() const { return object_->IsNull(); }

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
                 Handle<HeapObject> object,
                 ObjectDataKind kind = ObjectDataKind::kSerializedHeapObject);

  bool boolean_value() const { return boolean_value_; }
  ObjectData* map() const { return map_; }
  InstanceType GetMapInstanceType() const;

  static HeapObjectData* Serialize(JSHeapBroker* broker,
                                   Handle<HeapObject> object);

 private:
  bool const boolean_value_;
  ObjectData* const map_;
};

class PropertyCellData : public HeapObjectData {
 public:
  PropertyCellData(JSHeapBroker* broker, ObjectData** storage,
                   Handle<PropertyCell> object,
                   ObjectDataKind kind = ObjectDataKind::kSerializedHeapObject);

  bool Serialize(JSHeapBroker* broker);

  PropertyDetails property_details() const {
    CHECK(serialized());
    return property_details_;
  }

  ObjectData* value() const {
    DCHECK(serialized());
    return value_;
  }

 private:
  PropertyDetails property_details_ = PropertyDetails::Empty();
  ObjectData* value_ = nullptr;

  bool serialized() const { return value_ != nullptr; }
};

// TODO(mslekova): Once we have real-world usage data, we might want to
// reimplement this as sorted vector instead, to reduce the memory overhead.
typedef ZoneMap<ObjectData*, HolderLookupResult> KnownReceiversMap;

class FunctionTemplateInfoData : public HeapObjectData {
 public:
  FunctionTemplateInfoData(JSHeapBroker* broker, ObjectData** storage,
                           Handle<FunctionTemplateInfo> object);

  bool is_signature_undefined() const { return is_signature_undefined_; }
  bool accept_any_receiver() const { return accept_any_receiver_; }
  bool has_call_code() const { return has_call_code_; }

  void SerializeCallCode(JSHeapBroker* broker);
  ObjectData* call_code() const { return call_code_; }
  Address c_function() const { return c_function_; }
  const CFunctionInfo* c_signature() const { return c_signature_; }
  KnownReceiversMap& known_receivers() { return known_receivers_; }

 private:
  bool is_signature_undefined_ = false;
  bool accept_any_receiver_ = false;
  bool has_call_code_ = false;

  ObjectData* call_code_ = nullptr;
  const Address c_function_;
  const CFunctionInfo* const c_signature_;
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

  ObjectData* data_ = nullptr;
};

FunctionTemplateInfoData::FunctionTemplateInfoData(
    JSHeapBroker* broker, ObjectData** storage,
    Handle<FunctionTemplateInfo> object)
    : HeapObjectData(broker, storage, object),
      c_function_(v8::ToCData<Address>(object->GetCFunction())),
      c_signature_(v8::ToCData<CFunctionInfo*>(object->GetCSignature())),
      known_receivers_(broker->zone()) {
  DCHECK(!FLAG_turbo_direct_heap_access);
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
      callback_(v8::ToCData<Address>(object->callback())) {
  DCHECK(!FLAG_turbo_direct_heap_access);
}

// These definitions are here in order to please the linker, which in debug mode
// sometimes requires static constants to be defined in .cc files.
const uint32_t JSHeapBroker::kMinimalRefsBucketCount;
const uint32_t JSHeapBroker::kInitialRefsBucketCount;

void JSHeapBroker::IncrementTracingIndentation() { ++trace_indentation_; }

void JSHeapBroker::DecrementTracingIndentation() { --trace_indentation_; }

PropertyCellData::PropertyCellData(JSHeapBroker* broker, ObjectData** storage,
                                   Handle<PropertyCell> object,
                                   ObjectDataKind kind)
    : HeapObjectData(broker, storage, object, kind) {}

bool PropertyCellData::Serialize(JSHeapBroker* broker) {
  if (serialized()) return true;

  TraceScope tracer(broker, this, "PropertyCellData::Serialize");
  auto cell = Handle<PropertyCell>::cast(object());

  // While this code runs on a background thread, the property cell might
  // undergo state transitions via calls to PropertyCell::Transition. These
  // transitions follow a certain protocol on which we rely here to ensure that
  // we only report success when we can guarantee consistent data. A key
  // property is that after transitioning from cell type A to B (A != B), there
  // will never be a transition back to A, unless A is kConstant and the new
  // value is the hole (i.e. the property cell was invalidated, which is a final
  // state).

  PropertyDetails property_details = cell->property_details(kAcquireLoad);

  Handle<Object> value =
      broker->CanonicalPersistentHandle(cell->value(kAcquireLoad));
  if (broker->ObjectMayBeUninitialized(value)) {
    DCHECK(!broker->IsMainThread());
    return false;
  }

  {
    PropertyDetails property_details_again =
        cell->property_details(kAcquireLoad);
    if (property_details != property_details_again) {
      DCHECK(!broker->IsMainThread());
      return false;
    }
  }

  if (property_details.cell_type() == PropertyCellType::kConstant) {
    Handle<Object> value_again =
        broker->CanonicalPersistentHandle(cell->value(kAcquireLoad));
    if (*value != *value_again) {
      DCHECK(!broker->IsMainThread());
      return false;
    }
  }

  ObjectData* value_data = broker->TryGetOrCreateData(value, false);
  if (value_data == nullptr) {
    DCHECK(!broker->IsMainThread());
    return false;
  }

  PropertyCell::CheckDataIsCompatible(property_details, *value);

  DCHECK(!serialized());
  property_details_ = property_details;
  value_ = value_data;
  DCHECK(serialized());
  return true;
}

void FunctionTemplateInfoData::SerializeCallCode(JSHeapBroker* broker) {
  if (call_code_ != nullptr) return;

  TraceScope tracer(broker, this,
                    "FunctionTemplateInfoData::SerializeCallCode");
  auto function_template_info = Handle<FunctionTemplateInfo>::cast(object());
  call_code_ =
      broker->GetOrCreateData(function_template_info->call_code(kAcquireLoad));
  if (call_code_->should_access_heap()) {
    // TODO(mvstanton): When ObjectRef is in the never serialized list, this
    // code can be removed.
    broker->GetOrCreateData(
        Handle<CallHandlerInfo>::cast(call_code_->object())->data());
  } else {
    call_code_->AsCallHandlerInfo()->Serialize(broker);
  }
}

void CallHandlerInfoData::Serialize(JSHeapBroker* broker) {
  if (data_ != nullptr) return;

  TraceScope tracer(broker, this, "CallHandlerInfoData::Serialize");
  auto call_handler_info = Handle<CallHandlerInfo>::cast(object());
  data_ = broker->GetOrCreateData(call_handler_info->data());
}

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
  ObjectData* GetInobjectField(int property_index) const;

  // Shallow serialization of {elements}.
  void SerializeElements(JSHeapBroker* broker);
  bool serialized_elements() const { return serialized_elements_; }
  ObjectData* elements() const;

  void SerializeObjectCreateMap(JSHeapBroker* broker);

  ObjectData* object_create_map(
      JSHeapBroker* broker) const {  // Can be nullptr.
    if (!serialized_object_create_map_) {
      DCHECK_NULL(object_create_map_);
      TRACE_MISSING(broker, "object_create_map on " << this);
    }
    return object_create_map_;
  }

  ObjectData* GetOwnConstantElement(
      JSHeapBroker* broker, uint32_t index,
      SerializationPolicy policy = SerializationPolicy::kAssumeSerialized);
  ObjectData* GetOwnFastDataProperty(
      JSHeapBroker* broker, Representation representation,
      FieldIndex field_index,
      SerializationPolicy policy = SerializationPolicy::kAssumeSerialized);
  ObjectData* GetOwnDictionaryProperty(JSHeapBroker* broker,
                                       InternalIndex dict_index,
                                       SerializationPolicy policy);

  // This method is only used to assert our invariants.
  bool cow_or_empty_elements_tenured() const;

 private:
  void SerializeRecursiveAsBoilerplate(JSHeapBroker* broker, int max_depths);

  ObjectData* elements_ = nullptr;
  bool cow_or_empty_elements_tenured_ = false;
  // The {serialized_as_boilerplate} flag is set when all recursively
  // reachable JSObjects are serialized.
  bool serialized_as_boilerplate_ = false;
  bool serialized_elements_ = false;

  ZoneVector<ObjectData*> inobject_fields_;

  bool serialized_object_create_map_ = false;
  ObjectData* object_create_map_ = nullptr;

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
  // For fast mode objects, the keys of the map are the property_index() values
  // of the respective property FieldIndex'es. For slow mode objects, the keys
  // are the dictionary indicies.
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
            broker->GetOrCreateData(proto_info->ObjectCreateMap());
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
    return ObjectRef(broker,
                     broker->CanonicalPersistentHandle(it.GetDataValue()));
  }
  return base::nullopt;
}

ObjectRef GetOwnFastDataPropertyFromHeap(JSHeapBroker* broker,
                                         Handle<JSObject> receiver,
                                         Representation representation,
                                         FieldIndex field_index) {
  Handle<Object> constant =
      JSObject::FastPropertyAt(receiver, representation, field_index);
  return ObjectRef(broker, constant);
}

ObjectRef GetOwnDictionaryPropertyFromHeap(JSHeapBroker* broker,
                                           Handle<JSObject> receiver,
                                           InternalIndex dict_index) {
  Handle<Object> constant =
      JSObject::DictionaryPropertyAt(receiver, dict_index);
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

ObjectData* JSObjectData::GetOwnFastDataProperty(JSHeapBroker* broker,
                                                 Representation representation,
                                                 FieldIndex field_index,
                                                 SerializationPolicy policy) {
  auto p = own_properties_.find(field_index.property_index());
  if (p != own_properties_.end()) return p->second;

  if (policy == SerializationPolicy::kAssumeSerialized) {
    TRACE_MISSING(broker, "knowledge about fast property with index "
                              << field_index.property_index() << " on "
                              << this);
    return nullptr;
  }

  ObjectRef property = GetOwnFastDataPropertyFromHeap(
      broker, Handle<JSObject>::cast(object()), representation, field_index);
  ObjectData* result(property.data());
  own_properties_.insert(std::make_pair(field_index.property_index(), result));
  return result;
}

ObjectData* JSObjectData::GetOwnDictionaryProperty(JSHeapBroker* broker,
                                                   InternalIndex dict_index,
                                                   SerializationPolicy policy) {
  auto p = own_properties_.find(dict_index.as_int());
  if (p != own_properties_.end()) return p->second;

  if (policy == SerializationPolicy::kAssumeSerialized) {
    TRACE_MISSING(broker, "knowledge about dictionary property with index "
                              << dict_index.as_int() << " on " << this);
    return nullptr;
  }

  ObjectRef property = GetOwnDictionaryPropertyFromHeap(
      broker, Handle<JSObject>::cast(object()), dict_index);
  ObjectData* result(property.data());
  own_properties_.insert(std::make_pair(dict_index.as_int(), result));
  return result;
}

class JSTypedArrayData : public JSObjectData {
 public:
  JSTypedArrayData(JSHeapBroker* broker, ObjectData** storage,
                   Handle<JSTypedArray> object)
      : JSObjectData(broker, storage, object) {}

  // TODO(v8:7790): Once JSObject is no longer serialized, also make
  // JSTypedArrayRef never-serialized.
  STATIC_ASSERT(IsSerializedHeapObject<JSObject>());

  void Serialize(JSHeapBroker* broker);
  bool serialized() const { return serialized_; }

  bool is_on_heap() const { return is_on_heap_; }
  size_t length() const { return length_; }
  void* data_ptr() const { return data_ptr_; }

  ObjectData* buffer() const { return buffer_; }

 private:
  bool serialized_ = false;
  bool is_on_heap_ = false;
  size_t length_ = 0;
  void* data_ptr_ = nullptr;
  ObjectData* buffer_ = nullptr;
};

void JSTypedArrayData::Serialize(JSHeapBroker* broker) {
  if (serialized_) return;
  serialized_ = true;

  TraceScope tracer(broker, this, "JSTypedArrayData::Serialize");
  Handle<JSTypedArray> typed_array = Handle<JSTypedArray>::cast(object());

  is_on_heap_ = typed_array->is_on_heap();
  length_ = typed_array->length();
  data_ptr_ = typed_array->DataPtr();

  if (!is_on_heap()) {
    DCHECK_NULL(buffer_);
    buffer_ = broker->GetOrCreateData(typed_array->buffer());
  }
}

class ArrayBoilerplateDescriptionData : public HeapObjectData {
 public:
  ArrayBoilerplateDescriptionData(JSHeapBroker* broker, ObjectData** storage,
                                  Handle<ArrayBoilerplateDescription> object)
      : HeapObjectData(broker, storage, object),
        constants_elements_length_(object->constant_elements().length()) {
    DCHECK(!FLAG_turbo_direct_heap_access);
  }

  int constants_elements_length() const { return constants_elements_length_; }

 private:
  int const constants_elements_length_;
};

class ObjectBoilerplateDescriptionData : public HeapObjectData {
 public:
  ObjectBoilerplateDescriptionData(JSHeapBroker* broker, ObjectData** storage,
                                   Handle<ObjectBoilerplateDescription> object)
      : HeapObjectData(broker, storage, object), size_(object->size()) {
    DCHECK(!FLAG_turbo_direct_heap_access);
  }

  int size() const { return size_; }

 private:
  int const size_;
};

class JSDataViewData : public JSObjectData {
 public:
  JSDataViewData(JSHeapBroker* broker, ObjectData** storage,
                 Handle<JSDataView> object);

  size_t byte_length() const { return byte_length_; }

 private:
  size_t const byte_length_;
};

class JSBoundFunctionData : public JSObjectData {
 public:
  JSBoundFunctionData(JSHeapBroker* broker, ObjectData** storage,
                      Handle<JSBoundFunction> object);

  bool Serialize(JSHeapBroker* broker);
  bool serialized() const { return serialized_; }

  ObjectData* bound_target_function() const {
    DCHECK(!FLAG_turbo_direct_heap_access);
    return bound_target_function_;
  }
  ObjectData* bound_this() const {
    DCHECK(!FLAG_turbo_direct_heap_access);
    return bound_this_;
  }
  ObjectData* bound_arguments() const {
    DCHECK(!FLAG_turbo_direct_heap_access);
    return bound_arguments_;
  }

 private:
  bool serialized_ = false;

  ObjectData* bound_target_function_ = nullptr;
  ObjectData* bound_this_ = nullptr;
  ObjectData* bound_arguments_ = nullptr;
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

  void SerializeCodeAndFeedback(JSHeapBroker* broker);
  bool serialized_code_and_feedback() const {
    return serialized_code_and_feedback_;
  }

  ObjectData* context() const { return context_; }
  ObjectData* native_context() const { return native_context_; }
  ObjectData* initial_map() const { return initial_map_; }
  ObjectData* prototype() const { return prototype_; }
  ObjectData* shared() const { return shared_; }
  ObjectData* raw_feedback_cell() const {
    DCHECK(serialized_code_and_feedback());
    return feedback_cell_;
  }
  ObjectData* feedback_vector() const {
    DCHECK(serialized_code_and_feedback());
    return feedback_vector_;
  }
  ObjectData* code() const {
    DCHECK(serialized_code_and_feedback());
    DCHECK(!FLAG_turbo_direct_heap_access);
    return code_;
  }
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
  bool serialized_code_and_feedback_ = false;

  ObjectData* context_ = nullptr;
  ObjectData* native_context_ = nullptr;
  ObjectData* initial_map_ = nullptr;
  ObjectData* prototype_ = nullptr;
  ObjectData* shared_ = nullptr;
  ObjectData* feedback_vector_ = nullptr;
  ObjectData* feedback_cell_ = nullptr;
  ObjectData* code_ = nullptr;
  int initial_map_instance_size_with_min_slack_;
};

class RegExpBoilerplateDescriptionData : public HeapObjectData {
 public:
  RegExpBoilerplateDescriptionData(JSHeapBroker* broker, ObjectData** storage,
                                   Handle<RegExpBoilerplateDescription> object)
      : HeapObjectData(broker, storage, object) {}

  void Serialize(JSHeapBroker* broker);
  ObjectData* data() const {
    CHECK(serialized_);
    return data_;
  }
  ObjectData* source() const {
    CHECK(serialized_);
    return source_;
  }
  int flags() const {
    CHECK(serialized_);
    return flags_;
  }

 private:
  bool serialized_ = false;
  ObjectData* data_ = nullptr;
  ObjectData* source_ = nullptr;
  int flags_;
};

class HeapNumberData : public HeapObjectData {
 public:
  HeapNumberData(JSHeapBroker* broker, ObjectData** storage,
                 Handle<HeapNumber> object,
                 ObjectDataKind kind = ObjectDataKind::kSerializedHeapObject)
      : HeapObjectData(broker, storage, object, kind),
        value_(object->value()) {}

  double value() const { return value_; }

 private:
  double const value_;
};

class ContextData : public HeapObjectData {
 public:
  ContextData(JSHeapBroker* broker, ObjectData** storage,
              Handle<Context> object);

  ObjectData* previous(
      JSHeapBroker* broker,
      SerializationPolicy policy = SerializationPolicy::kAssumeSerialized);

  // Returns nullptr if the slot index isn't valid or wasn't serialized,
  // unless {policy} is {kSerializeIfNeeded}.
  ObjectData* GetSlot(
      JSHeapBroker* broker, int index,
      SerializationPolicy policy = SerializationPolicy::kAssumeSerialized);

 private:
  ZoneMap<int, ObjectData*> slots_;
  ObjectData* previous_ = nullptr;
};

ContextData::ContextData(JSHeapBroker* broker, ObjectData** storage,
                         Handle<Context> object)
    : HeapObjectData(broker, storage, object), slots_(broker->zone()) {}

ObjectData* ContextData::previous(JSHeapBroker* broker,
                                  SerializationPolicy policy) {
  if (policy == SerializationPolicy::kSerializeIfNeeded &&
      previous_ == nullptr) {
    TraceScope tracer(broker, this, "ContextData::previous");
    Handle<Context> context = Handle<Context>::cast(object());
    previous_ = broker->GetOrCreateData(context->unchecked_previous());
  }
  return previous_;
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
  ObjectData* name() const { return name##_; }
  BROKER_NATIVE_CONTEXT_FIELDS(DECL_ACCESSOR)
#undef DECL_ACCESSOR

  const ZoneVector<ObjectData*>& function_maps() const {
    CHECK_NE(state_, State::kUnserialized);
    return function_maps_;
  }

  ObjectData* scope_info() const {
    CHECK_NE(state_, State::kUnserialized);
    return scope_info_;
  }

  NativeContextData(JSHeapBroker* broker, ObjectData** storage,
                    Handle<NativeContext> object);
  void Serialize(JSHeapBroker* broker);
  void SerializeOnBackground(JSHeapBroker* broker);

 private:
  // After Serialize is called the class is partially serialized and it the
  // kSerializedOnMainThread state. It then becomes kFullySerialized once
  // SerializeOnBackground is called.
  enum class State { kUnserialized, kSerializedOnMainThread, kFullySerialized };
  State state_;

#define DECL_MEMBER(type, name) ObjectData* name##_ = nullptr;
  BROKER_NATIVE_CONTEXT_FIELDS(DECL_MEMBER)
#undef DECL_MEMBER
  ZoneVector<ObjectData*> function_maps_;
  ObjectData* scope_info_ = nullptr;
};

class NameData : public HeapObjectData {
 public:
  NameData(JSHeapBroker* broker, ObjectData** storage, Handle<Name> object)
      : HeapObjectData(broker, storage, object) {
    DCHECK(!FLAG_turbo_direct_heap_access);
  }
};

class StringData : public NameData {
 public:
  StringData(JSHeapBroker* broker, ObjectData** storage, Handle<String> object);

  int length() const { return length_; }
  uint16_t first_char() const { return first_char_; }
  base::Optional<double> to_number() const { return to_number_; }
  bool is_external_string() const { return is_external_string_; }
  bool is_seq_string() const { return is_seq_string_; }

  ObjectData* GetCharAsStringOrUndefined(
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
  ZoneVector<std::pair<uint32_t, ObjectData*>> chars_as_strings_;
};

class SymbolData : public NameData {
 public:
  SymbolData(JSHeapBroker* broker, ObjectData** storage, Handle<Symbol> object)
      : NameData(broker, storage, object) {
    DCHECK(!FLAG_turbo_direct_heap_access);
  }
};

StringData::StringData(JSHeapBroker* broker, ObjectData** storage,
                       Handle<String> object)
    : NameData(broker, storage, object),
      length_(object->length()),
      first_char_(length_ > 0 ? object->Get(0) : 0),
      to_number_(TryStringToDouble(broker->local_isolate(), object)),
      is_external_string_(object->IsExternalString()),
      is_seq_string_(object->IsSeqString()),
      chars_as_strings_(broker->zone()) {
  DCHECK(!FLAG_turbo_direct_heap_access);
}

class InternalizedStringData : public StringData {
 public:
  InternalizedStringData(JSHeapBroker* broker, ObjectData** storage,
                         Handle<InternalizedString> object)
      : StringData(broker, storage, object) {
    DCHECK(!FLAG_turbo_direct_heap_access);
  }
};

ObjectData* StringData::GetCharAsStringOrUndefined(JSHeapBroker* broker,
                                                   uint32_t index,
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
  ObjectData* result = element.has_value() ? element->data() : nullptr;
  chars_as_strings_.push_back({index, result});
  return result;
}

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
  Handle<DescriptorArray> descriptors(
      boilerplate->map().instance_descriptors(isolate, kRelaxedLoad), isolate);
  for (InternalIndex i : boilerplate->map().IterateOwnDescriptors()) {
    PropertyDetails details = descriptors->GetDetails(i);
    if (details.location() != kField) continue;
    DCHECK_EQ(kData, details.kind());
    if ((*max_properties)-- == 0) return false;
    FieldIndex field_index = FieldIndex::ForDescriptor(boilerplate->map(), i);
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
  ObjectData* boilerplate() const { return boilerplate_; }

  // These are only valid if PointsToLiteral is false.
  ElementsKind GetElementsKind() const { return GetElementsKind_; }
  bool CanInlineCall() const { return CanInlineCall_; }

 private:
  bool const PointsToLiteral_;
  AllocationType const GetAllocationType_;
  ObjectData* nested_site_ = nullptr;
  bool IsFastLiteral_ = false;
  ObjectData* boilerplate_ = nullptr;
  ElementsKind GetElementsKind_ = NO_ELEMENTS;
  bool CanInlineCall_ = false;
  bool serialized_boilerplate_ = false;
};

class BigIntData : public HeapObjectData {
 public:
  BigIntData(JSHeapBroker* broker, ObjectData** storage, Handle<BigInt> object,
             ObjectDataKind kind = ObjectDataKind::kSerializedHeapObject)
      : HeapObjectData(broker, storage, object, kind),
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
  ObjectData* key = nullptr;
  ObjectData* value = nullptr;
  PropertyDetails details = PropertyDetails::Empty();
  FieldIndex field_index;
  ObjectData* field_owner = nullptr;
  ObjectData* field_type = nullptr;
};

class MapData : public HeapObjectData {
 public:
  MapData(JSHeapBroker* broker, ObjectData** storage, Handle<Map> object,
          ObjectDataKind kind = ObjectDataKind::kSerializedHeapObject);

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
  const ZoneVector<ObjectData*>& elements_kind_generalizations() const {
    CHECK(serialized_elements_kind_generalizations_);
    return elements_kind_generalizations_;
  }

  // Serialize a single (or all) own slot(s) of the descriptor array and recurse
  // on field owner(s).
  void SerializeOwnDescriptor(JSHeapBroker* broker,
                              InternalIndex descriptor_index);
  void SerializeOwnDescriptors(JSHeapBroker* broker);
  ObjectData* GetStrongValue(InternalIndex descriptor_index) const;
  ObjectData* instance_descriptors() const { return instance_descriptors_; }

  void SerializeRootMap(JSHeapBroker* broker);
  ObjectData* FindRootMap() const;

  void SerializeConstructor(JSHeapBroker* broker);
  ObjectData* GetConstructor() const {
    CHECK(serialized_constructor_);
    return constructor_;
  }

  void SerializeBackPointer(JSHeapBroker* broker);
  ObjectData* GetBackPointer() const {
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
  ZoneVector<ObjectData*> elements_kind_generalizations_;

  bool serialized_own_descriptors_ = false;
  ObjectData* instance_descriptors_ = nullptr;

  bool serialized_constructor_ = false;
  ObjectData* constructor_ = nullptr;

  bool serialized_backpointer_ = false;
  ObjectData* backpointer_ = nullptr;

  bool serialized_prototype_ = false;
  ObjectData* prototype_ = nullptr;

  bool serialized_root_map_ = false;
  ObjectData* root_map_ = nullptr;

  bool serialized_for_element_load_ = false;

  bool serialized_for_element_store_ = false;
};

AccessorInfoData::AccessorInfoData(JSHeapBroker* broker, ObjectData** storage,
                                   Handle<AccessorInfo> object)
    : HeapObjectData(broker, storage, object) {
  DCHECK(!FLAG_turbo_direct_heap_access);
}

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
  boilerplate_ = broker->GetOrCreateData(site->boilerplate());
  if (!boilerplate_->should_access_heap()) {
    boilerplate_->AsJSObject()->SerializeAsBoilerplate(broker);
  }

  DCHECK_NULL(nested_site_);
  nested_site_ = broker->GetOrCreateData(site->nested_site());
  if (nested_site_->IsAllocationSite() && !nested_site_->should_access_heap()) {
    nested_site_->AsAllocationSite()->SerializeBoilerplate(broker);
  }
}

HeapObjectData::HeapObjectData(JSHeapBroker* broker, ObjectData** storage,
                               Handle<HeapObject> object, ObjectDataKind kind)
    : ObjectData(broker, storage, object, kind),
      boolean_value_(object->BooleanValue(broker->isolate())),
      // We have to use a raw cast below instead of AsMap() because of
      // recursion. AsMap() would call IsMap(), which accesses the
      // instance_type_ member. In the case of constructing the MapData for the
      // meta map (whose map is itself), this member has not yet been
      // initialized.
      map_(broker->GetOrCreateData(object->map())) {
  CHECK_IMPLIES(kind == kSerializedHeapObject,
                broker->mode() == JSHeapBroker::kSerializing);
  CHECK_IMPLIES(broker->mode() == JSHeapBroker::kSerialized,
                    kind == kBackgroundSerializedHeapObject);
}

InstanceType HeapObjectData::GetMapInstanceType() const {
  ObjectData* map_data = map();
  if (map_data->should_access_heap()) {
    return Handle<Map>::cast(map_data->object())->instance_type();
  }
  return map_data->AsMap()->instance_type();
}

namespace {
bool IsReadOnlyLengthDescriptor(Isolate* isolate, Handle<Map> jsarray_map) {
  DCHECK(!jsarray_map->is_dictionary_map());
  Handle<Name> length_string = isolate->factory()->length_string();
  DescriptorArray descriptors =
      jsarray_map->instance_descriptors(isolate, kRelaxedLoad);
  // TODO(jkummerow): We could skip the search and hardcode number == 0.
  InternalIndex number = descriptors.Search(*length_string, *jsarray_map);
  DCHECK(number.is_found());
  return descriptors.GetDetails(number).IsReadOnly();
}

bool SupportsFastArrayIteration(Isolate* isolate, Handle<Map> map) {
  return map->instance_type() == JS_ARRAY_TYPE &&
         IsFastElementsKind(map->elements_kind()) &&
         map->prototype().IsJSArray() &&
         isolate->IsAnyInitialArrayPrototype(JSArray::cast(map->prototype())) &&
         Protectors::IsNoElementsIntact(isolate);
}

bool SupportsFastArrayResize(Isolate* isolate, Handle<Map> map) {
  return SupportsFastArrayIteration(isolate, map) && map->is_extensible() &&
         !map->is_dictionary_map() && !IsReadOnlyLengthDescriptor(isolate, map);
}
}  // namespace

MapData::MapData(JSHeapBroker* broker, ObjectData** storage, Handle<Map> object,
                 ObjectDataKind kind)
    : HeapObjectData(broker, storage, object, kind),
      instance_type_(object->instance_type()),
      instance_size_(object->instance_size()),
      // We read the bit_field as relaxed since `has_non_instance_prototype` can
      // be modified in live objects, and because we serialize some maps on the
      // background. Those background-serialized maps are the native context's
      // maps for which this bit is "set" but it doesn't change value (i.e. it
      // is set to false when it was already false).
      bit_field_(object->relaxed_bit_field()),
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

  context_ = broker->GetOrCreateData(function->context());
  native_context_ = broker->GetOrCreateData(function->native_context());
  shared_ = broker->GetOrCreateData(function->shared());

  initial_map_ = has_initial_map()
                     ? broker->GetOrCreateData(function->initial_map())
                     : nullptr;
  prototype_ = has_prototype() ? broker->GetOrCreateData(function->prototype())
                               : nullptr;

  if (initial_map_ != nullptr) {
    initial_map_instance_size_with_min_slack_ =
        function->ComputeInstanceSizeWithMinSlack(broker->isolate());
  }
  if (initial_map_ != nullptr && !initial_map_->should_access_heap()) {
    if (initial_map_->AsMap()->instance_type() == JS_ARRAY_TYPE) {
      initial_map_->AsMap()->SerializeElementsKindGeneralizations(broker);
    }
    initial_map_->AsMap()->SerializeConstructor(broker);
    // TODO(neis): This is currently only needed for native_context's
    // object_function, as used by GetObjectCreateMap. If no further use sites
    // show up, we should move this into NativeContextData::Serialize.
    initial_map_->AsMap()->SerializePrototype(broker);
  }
}

void JSFunctionData::SerializeCodeAndFeedback(JSHeapBroker* broker) {
  DCHECK(serialized_);
  if (serialized_code_and_feedback_) return;
  serialized_code_and_feedback_ = true;

  TraceScope tracer(broker, this, "JSFunctionData::SerializeCodeAndFeedback");
  Handle<JSFunction> function = Handle<JSFunction>::cast(object());

  DCHECK_NULL(feedback_cell_);
  DCHECK_NULL(feedback_vector_);
  DCHECK_NULL(code_);
  if (!FLAG_turbo_direct_heap_access) {
    // This is conditionalized because Code objects are never serialized now.
    // We only need to represent the code object in serialized data when
    // we're unable to perform direct heap accesses.
    code_ = broker->GetOrCreateData(function->code(kAcquireLoad));
  }
  feedback_cell_ = broker->GetOrCreateData(function->raw_feedback_cell());
  feedback_vector_ = has_feedback_vector()
                         ? broker->GetOrCreateData(function->feedback_vector())
                         : nullptr;
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
      elements_kind_generalizations_.push_back(broker->GetOrCreateData(target));
    }
  }
}

class DescriptorArrayData : public HeapObjectData {
 public:
  DescriptorArrayData(JSHeapBroker* broker, ObjectData** storage,
                      Handle<DescriptorArray> object)
      : HeapObjectData(broker, storage, object), contents_(broker->zone()) {
    DCHECK(!FLAG_turbo_direct_heap_access);
  }

  ObjectData* FindFieldOwner(InternalIndex descriptor_index) const {
    return contents_.at(descriptor_index.as_int()).field_owner;
  }

  PropertyDetails GetPropertyDetails(InternalIndex descriptor_index) const {
    return contents_.at(descriptor_index.as_int()).details;
  }

  ObjectData* GetPropertyKey(InternalIndex descriptor_index) const {
    return contents_.at(descriptor_index.as_int()).key;
  }

  FieldIndex GetFieldIndexFor(InternalIndex descriptor_index) const {
    return contents_.at(descriptor_index.as_int()).field_index;
  }

  ObjectData* GetFieldType(InternalIndex descriptor_index) const {
    return contents_.at(descriptor_index.as_int()).field_type;
  }

  ObjectData* GetStrongValue(InternalIndex descriptor_index) const {
    return contents_.at(descriptor_index.as_int()).value;
  }

  bool serialized_descriptor(InternalIndex descriptor_index) const {
    return contents_.find(descriptor_index.as_int()) != contents_.end();
  }

  void SerializeDescriptor(JSHeapBroker* broker, Handle<Map> map,
                           InternalIndex descriptor_index);

 private:
  ZoneMap<int, PropertyDescriptor> contents_;
};

void DescriptorArrayData::SerializeDescriptor(JSHeapBroker* broker,
                                              Handle<Map> map,
                                              InternalIndex descriptor_index) {
  CHECK_LT(descriptor_index.as_int(), map->NumberOfOwnDescriptors());
  if (contents_.find(descriptor_index.as_int()) != contents_.end()) return;

  Isolate* const isolate = broker->isolate();
  auto descriptors = Handle<DescriptorArray>::cast(object());
  CHECK_EQ(*descriptors, map->instance_descriptors(isolate));

  PropertyDescriptor d;
  d.key = broker->GetOrCreateData(descriptors->GetKey(descriptor_index));
  MaybeObject value = descriptors->GetValue(descriptor_index);
  HeapObject obj;
  if (value.GetHeapObjectIfStrong(&obj)) {
    d.value = broker->GetOrCreateData(obj);
  }
  d.details = descriptors->GetDetails(descriptor_index);
  if (d.details.location() == kField) {
    d.field_index = FieldIndex::ForDescriptor(*map, descriptor_index);
    d.field_owner =
        broker->GetOrCreateData(map->FindFieldOwner(isolate, descriptor_index));
    d.field_type =
        broker->GetOrCreateData(descriptors->GetFieldType(descriptor_index));
  }
  contents_[descriptor_index.as_int()] = d;

  if (d.details.location() == kField && !d.field_owner->should_access_heap()) {
    // Recurse on the owner map.
    d.field_owner->AsMap()->SerializeOwnDescriptor(broker, descriptor_index);
  }

  TRACE(broker, "Copied descriptor " << descriptor_index.as_int() << " into "
                                     << this << " (" << contents_.size()
                                     << " total)");
}

class FeedbackCellData : public HeapObjectData {
 public:
  FeedbackCellData(JSHeapBroker* broker, ObjectData** storage,
                   Handle<FeedbackCell> object);

  ObjectData* value() const { return value_; }

 private:
  ObjectData* const value_;
};

FeedbackCellData::FeedbackCellData(JSHeapBroker* broker, ObjectData** storage,
                                   Handle<FeedbackCell> object)
    : HeapObjectData(broker, storage, object),
      value_(object->value().IsFeedbackVector()
                 ? broker->GetOrCreateData(object->value())
                 : nullptr) {
  DCHECK(!FLAG_turbo_direct_heap_access);
}

class FeedbackVectorData : public HeapObjectData {
 public:
  FeedbackVectorData(JSHeapBroker* broker, ObjectData** storage,
                     Handle<FeedbackVector> object);

  double invocation_count() const { return invocation_count_; }

  ObjectData* shared_function_info() {
    CHECK(serialized_);
    return shared_function_info_;
  }

  void Serialize(JSHeapBroker* broker);
  bool serialized() const { return serialized_; }
  ObjectData* GetClosureFeedbackCell(JSHeapBroker* broker, int index) const;

 private:
  double const invocation_count_;

  bool serialized_ = false;
  ObjectData* shared_function_info_;
  ZoneVector<ObjectData*> closure_feedback_cell_array_;
};

FeedbackVectorData::FeedbackVectorData(JSHeapBroker* broker,
                                       ObjectData** storage,
                                       Handle<FeedbackVector> object)
    : HeapObjectData(broker, storage, object),
      invocation_count_(object->invocation_count()),
      closure_feedback_cell_array_(broker->zone()) {
  DCHECK(!FLAG_turbo_direct_heap_access);
}

ObjectData* FeedbackVectorData::GetClosureFeedbackCell(JSHeapBroker* broker,
                                                       int index) const {
  CHECK_GE(index, 0);

  size_t cell_array_size = closure_feedback_cell_array_.size();
  if (!serialized_) {
    DCHECK_EQ(cell_array_size, 0);
    TRACE_BROKER_MISSING(broker,
                         " closure feedback cell array for vector " << this);
    return nullptr;
  }
  CHECK_LT(index, cell_array_size);
  return closure_feedback_cell_array_[index];
}

void FeedbackVectorData::Serialize(JSHeapBroker* broker) {
  if (serialized_) return;
  serialized_ = true;

  TraceScope tracer(broker, this, "FeedbackVectorData::Serialize");
  Handle<FeedbackVector> vector = Handle<FeedbackVector>::cast(object());
  Handle<SharedFunctionInfo> sfi(vector->shared_function_info(),
                                 broker->isolate());
  shared_function_info_ = broker->GetOrCreateData(sfi);
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
      byte_length_(object->byte_length()) {}

JSBoundFunctionData::JSBoundFunctionData(JSHeapBroker* broker,
                                         ObjectData** storage,
                                         Handle<JSBoundFunction> object)
    : JSObjectData(broker, storage, object) {}

bool JSBoundFunctionData::Serialize(JSHeapBroker* broker) {
  if (serialized_) return true;
  if (broker->StackHasOverflowed()) return false;

  TraceScope tracer(broker, this, "JSBoundFunctionData::Serialize");
  Handle<JSBoundFunction> function = Handle<JSBoundFunction>::cast(object());

  // We don't immediately set {serialized_} in order to correctly handle the
  // case where a recursive call to this method reaches the stack limit.

  DCHECK_NULL(bound_target_function_);
  bound_target_function_ =
      broker->GetOrCreateData(function->bound_target_function());
  bool serialized_nested = true;
  if (!bound_target_function_->should_access_heap()) {
    if (bound_target_function_->IsJSBoundFunction()) {
      serialized_nested =
          bound_target_function_->AsJSBoundFunction()->Serialize(broker);
    } else if (bound_target_function_->IsJSFunction()) {
      bound_target_function_->AsJSFunction()->Serialize(broker);
    }
  }
  if (!serialized_nested) {
    // We couldn't serialize all nested bound functions due to stack
    // overflow. Give up.
    DCHECK(!serialized_);
    bound_target_function_ = nullptr;  // Reset to sync with serialized_.
    return false;
  }

  serialized_ = true;

  DCHECK_NULL(bound_arguments_);
  bound_arguments_ = broker->GetOrCreateData(function->bound_arguments());
  if (!bound_arguments_->should_access_heap()) {
    bound_arguments_->AsFixedArray()->SerializeContents(broker);
  }

  DCHECK_NULL(bound_this_);
  bound_this_ = broker->GetOrCreateData(function->bound_this());

  return true;
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

  BytecodeArrayData(JSHeapBroker* broker, ObjectData** storage,
                    Handle<BytecodeArray> object)
      : FixedArrayBaseData(broker, storage, object),
        register_count_(object->register_count()),
        parameter_count_(object->parameter_count()),
        incoming_new_target_or_generator_register_(
            object->incoming_new_target_or_generator_register()) {}

 private:
  int const register_count_;
  int const parameter_count_;
  interpreter::Register const incoming_new_target_or_generator_register_;
};

class JSArrayData : public JSObjectData {
 public:
  JSArrayData(JSHeapBroker* broker, ObjectData** storage,
              Handle<JSArray> object);

  void Serialize(JSHeapBroker* broker);
  ObjectData* length() const {
    CHECK(serialized_);
    return length_;
  }

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
  CHECK(!FLAG_turbo_direct_heap_access);

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

  int ContextLength() const { return context_length_; }
  bool HasContextExtensionSlot() const { return has_context_extension_slot_; }
  bool HasOuterScopeInfo() const { return has_outer_scope_info_; }

  ObjectData* OuterScopeInfo() const { return outer_scope_info_; }
  void SerializeScopeInfoChain(JSHeapBroker* broker);

 private:
  int const context_length_;
  bool const has_context_extension_slot_;
  bool const has_outer_scope_info_;

  // Only serialized via SerializeScopeInfoChain.
  ObjectData* outer_scope_info_;
};

ScopeInfoData::ScopeInfoData(JSHeapBroker* broker, ObjectData** storage,
                             Handle<ScopeInfo> object)
    : HeapObjectData(broker, storage, object),
      context_length_(object->ContextLength()),
      has_context_extension_slot_(object->HasContextExtensionSlot()),
      has_outer_scope_info_(object->HasOuterScopeInfo()),
      outer_scope_info_(nullptr) {
  DCHECK(!FLAG_turbo_direct_heap_access);
}

void ScopeInfoData::SerializeScopeInfoChain(JSHeapBroker* broker) {
  if (outer_scope_info_) return;
  if (!has_outer_scope_info_) return;
  outer_scope_info_ = broker->GetOrCreateData(
      Handle<ScopeInfo>::cast(object())->OuterScopeInfo());
  if (!outer_scope_info_->should_access_heap()) {
    outer_scope_info_->AsScopeInfo()->SerializeScopeInfoChain(broker);
  }
}

class SharedFunctionInfoData : public HeapObjectData {
 public:
  SharedFunctionInfoData(JSHeapBroker* broker, ObjectData** storage,
                         Handle<SharedFunctionInfo> object);

  int builtin_id() const { return builtin_id_; }
  int context_header_size() const { return context_header_size_; }
  ObjectData* GetBytecodeArray() const { return GetBytecodeArray_; }
  SharedFunctionInfo::Inlineability GetInlineability() const {
    return inlineability_;
  }
  void SerializeFunctionTemplateInfo(JSHeapBroker* broker);
  ObjectData* scope_info() const { return scope_info_; }
  void SerializeScopeInfoChain(JSHeapBroker* broker);
  ObjectData* function_template_info() const { return function_template_info_; }
  ObjectData* GetTemplateObject(FeedbackSlot slot) const {
    auto lookup_it = template_objects_.find(slot.ToInt());
    if (lookup_it != template_objects_.cend()) {
      return lookup_it->second;
    }
    return nullptr;
  }
  void SetTemplateObject(FeedbackSlot slot, ObjectData* object) {
    CHECK(
        template_objects_.insert(std::make_pair(slot.ToInt(), object)).second);
  }

#define DECL_ACCESSOR(type, name) \
  type name() const { return name##_; }
  BROKER_SFI_FIELDS(DECL_ACCESSOR)
#undef DECL_ACCESSOR

 private:
  int const builtin_id_;
  int const context_header_size_;
  ObjectData* const GetBytecodeArray_;
#define DECL_MEMBER(type, name) type const name##_;
  BROKER_SFI_FIELDS(DECL_MEMBER)
#undef DECL_MEMBER
  SharedFunctionInfo::Inlineability const inlineability_;
  ObjectData* function_template_info_;
  ZoneMap<int, ObjectData*> template_objects_;
  ObjectData* scope_info_;
};

SharedFunctionInfoData::SharedFunctionInfoData(
    JSHeapBroker* broker, ObjectData** storage,
    Handle<SharedFunctionInfo> object)
    : HeapObjectData(broker, storage, object),
      builtin_id_(object->HasBuiltinId() ? object->builtin_id()
                                         : Builtins::kNoBuiltinId),
      context_header_size_(object->scope_info().ContextHeaderLength()),
      GetBytecodeArray_(object->HasBytecodeArray()
                            ? broker->GetOrCreateData(
                                  object->GetBytecodeArray(broker->isolate()))
                            : nullptr)
#define INIT_MEMBER(type, name) , name##_(object->name())
          BROKER_SFI_FIELDS(INIT_MEMBER)
#undef INIT_MEMBER
      ,
      inlineability_(object->GetInlineability(broker->isolate())),
      function_template_info_(nullptr),
      template_objects_(broker->zone()),
      scope_info_(nullptr) {
  DCHECK_EQ(HasBuiltinId_, builtin_id_ != Builtins::kNoBuiltinId);
  DCHECK_EQ(HasBytecodeArray_, GetBytecodeArray_ != nullptr);
}

void SharedFunctionInfoData::SerializeFunctionTemplateInfo(
    JSHeapBroker* broker) {
  if (function_template_info_) return;
  function_template_info_ = broker->GetOrCreateData(
      Handle<SharedFunctionInfo>::cast(object())->function_data(kAcquireLoad));
}

void SharedFunctionInfoData::SerializeScopeInfoChain(JSHeapBroker* broker) {
  if (scope_info_) return;
  scope_info_ = broker->GetOrCreateData(
      Handle<SharedFunctionInfo>::cast(object())->scope_info());
  if (!scope_info_->should_access_heap()) {
    scope_info_->AsScopeInfo()->SerializeScopeInfoChain(broker);
  }
}

class SourceTextModuleData : public HeapObjectData {
 public:
  SourceTextModuleData(JSHeapBroker* broker, ObjectData** storage,
                       Handle<SourceTextModule> object);
  void Serialize(JSHeapBroker* broker);

  ObjectData* GetCell(JSHeapBroker* broker, int cell_index) const;
  ObjectData* GetImportMeta(JSHeapBroker* broker) const;

 private:
  bool serialized_ = false;
  ZoneVector<ObjectData*> imports_;
  ZoneVector<ObjectData*> exports_;
  ObjectData* import_meta_;
};

SourceTextModuleData::SourceTextModuleData(JSHeapBroker* broker,
                                           ObjectData** storage,
                                           Handle<SourceTextModule> object)
    : HeapObjectData(broker, storage, object),
      imports_(broker->zone()),
      exports_(broker->zone()),
      import_meta_(nullptr) {}

ObjectData* SourceTextModuleData::GetCell(JSHeapBroker* broker,
                                          int cell_index) const {
  if (!serialized_) {
    DCHECK(imports_.empty());
    TRACE_BROKER_MISSING(broker,
                         "module cell " << cell_index << " on " << this);
    return nullptr;
  }
  ObjectData* cell;
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

ObjectData* SourceTextModuleData::GetImportMeta(JSHeapBroker* broker) const {
  CHECK(serialized_);
  return import_meta_;
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
    imports_.push_back(broker->GetOrCreateData(imports->get(i)));
  }
  TRACE(broker, "Copied " << imports_.size() << " imports");

  DCHECK(exports_.empty());
  Handle<FixedArray> exports(module->regular_exports(), broker->isolate());
  int const exports_length = exports->length();
  exports_.reserve(exports_length);
  for (int i = 0; i < exports_length; ++i) {
    exports_.push_back(broker->GetOrCreateData(exports->get(i)));
  }
  TRACE(broker, "Copied " << exports_.size() << " exports");

  DCHECK_NULL(import_meta_);
  import_meta_ = broker->GetOrCreateData(module->import_meta());
  TRACE(broker, "Copied import_meta");
}

class CellData : public HeapObjectData {
 public:
  CellData(JSHeapBroker* broker, ObjectData** storage, Handle<Cell> object)
      : HeapObjectData(broker, storage, object) {
    DCHECK(!FLAG_turbo_direct_heap_access);
  }
};

class JSGlobalObjectData : public JSObjectData {
 public:
  JSGlobalObjectData(JSHeapBroker* broker, ObjectData** storage,
                     Handle<JSGlobalObject> object);
  bool IsDetached() const { return is_detached_; }

  ObjectData* GetPropertyCell(
      JSHeapBroker* broker, ObjectData* name,
      SerializationPolicy policy = SerializationPolicy::kAssumeSerialized);

 private:
  bool const is_detached_;

  // Properties that either
  // (1) are known to exist as property cells on the global object, or
  // (2) are known not to (possibly they don't exist at all).
  // In case (2), the second pair component is nullptr.
  ZoneVector<std::pair<ObjectData*, ObjectData*>> properties_;
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

ObjectData* JSGlobalObjectData::GetPropertyCell(JSHeapBroker* broker,
                                                ObjectData* name,
                                                SerializationPolicy policy) {
  CHECK_NOT_NULL(name);
  for (auto const& p : properties_) {
    if (p.first == name) return p.second;
  }

  if (policy == SerializationPolicy::kAssumeSerialized) {
    TRACE_MISSING(broker, "knowledge about global property " << name);
    return nullptr;
  }

  ObjectData* result = nullptr;
  base::Optional<PropertyCellRef> cell =
      GetPropertyCellFromHeap(broker, Handle<Name>::cast(name->object()));
  if (cell.has_value()) {
    result = cell->data();
    if (!result->should_access_heap()) {
      result->AsPropertyCell()->Serialize(broker);
    }
  }
  properties_.push_back({name, result});
  return result;
}

class TemplateObjectDescriptionData : public HeapObjectData {
 public:
  TemplateObjectDescriptionData(JSHeapBroker* broker, ObjectData** storage,
                                Handle<TemplateObjectDescription> object)
      : HeapObjectData(broker, storage, object) {
    DCHECK(!FLAG_turbo_direct_heap_access);
  }
};

class CodeData : public HeapObjectData {
 public:
  CodeData(JSHeapBroker* broker, ObjectData** storage, Handle<Code> object)
      : HeapObjectData(broker, storage, object),
        inlined_bytecode_size_(object->inlined_bytecode_size() > 0 &&
                                       !object->marked_for_deoptimization()
                                   ? object->inlined_bytecode_size()
                                   : 0) {
    DCHECK(!FLAG_turbo_direct_heap_access);
  }

  unsigned inlined_bytecode_size() const { return inlined_bytecode_size_; }

 private:
  unsigned const inlined_bytecode_size_;
};

#define DEFINE_IS(Name)                                                 \
  bool ObjectData::Is##Name() const {                                   \
    if (should_access_heap()) {                                         \
      return object()->Is##Name();                                      \
    }                                                                   \
    if (is_smi()) return false;                                         \
    InstanceType instance_type =                                        \
        static_cast<const HeapObjectData*>(this)->GetMapInstanceType(); \
    return InstanceTypeChecker::Is##Name(instance_type);                \
  }
HEAP_BROKER_SERIALIZED_OBJECT_LIST(DEFINE_IS)
HEAP_BROKER_POSSIBLY_BACKGROUND_SERIALIZED_OBJECT_LIST(DEFINE_IS)
HEAP_BROKER_BACKGROUND_SERIALIZED_OBJECT_LIST(DEFINE_IS)
HEAP_BROKER_NEVER_SERIALIZED_OBJECT_LIST(DEFINE_IS)
#undef DEFINE_IS

#define DEFINE_AS(Name)                                       \
  Name##Data* ObjectData::As##Name() {                        \
    CHECK(Is##Name());                                        \
    CHECK(kind_ == kSerializedHeapObject ||                   \
          kind_ == kBackgroundSerializedHeapObject);          \
    return static_cast<Name##Data*>(this);                    \
  }
HEAP_BROKER_SERIALIZED_OBJECT_LIST(DEFINE_AS)
HEAP_BROKER_POSSIBLY_BACKGROUND_SERIALIZED_OBJECT_LIST(DEFINE_AS)
HEAP_BROKER_BACKGROUND_SERIALIZED_OBJECT_LIST(DEFINE_AS)
#undef DEFINE_AS

// TODO(solanes, v8:10866): Remove once FLAG_turbo_direct_heap_access is
// removed.
// This macro defines the Asxxx methods for NeverSerialized objects, which
// should only be used with direct heap access off.
#define DEFINE_AS(Name)                     \
  Name##Data* ObjectData::As##Name() {      \
    DCHECK(!FLAG_turbo_direct_heap_access); \
    CHECK(Is##Name());                      \
    CHECK_EQ(kind_, kSerializedHeapObject); \
    return static_cast<Name##Data*>(this);  \
  }
HEAP_BROKER_NEVER_SERIALIZED_OBJECT_LIST(DEFINE_AS)
#undef DEFINE_AS

ObjectData* JSObjectData::GetInobjectField(int property_index) const {
  CHECK_LT(static_cast<size_t>(property_index), inobject_fields_.size());
  return inobject_fields_[property_index];
}

bool JSObjectData::cow_or_empty_elements_tenured() const {
  return cow_or_empty_elements_tenured_;
}

ObjectData* JSObjectData::elements() const {
  CHECK(serialized_elements_);
  return elements_;
}

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
  elements_ = broker->GetOrCreateData(elements_object);
  DCHECK(elements_->IsFixedArrayBase());
}

void MapData::SerializeConstructor(JSHeapBroker* broker) {
  if (serialized_constructor_) return;
  serialized_constructor_ = true;

  TraceScope tracer(broker, this, "MapData::SerializeConstructor");
  Handle<Map> map = Handle<Map>::cast(object());
  DCHECK(!map->IsContextMap());
  DCHECK_NULL(constructor_);
  constructor_ = broker->GetOrCreateData(map->GetConstructor());
}

void MapData::SerializeBackPointer(JSHeapBroker* broker) {
  if (serialized_backpointer_) return;
  serialized_backpointer_ = true;

  TraceScope tracer(broker, this, "MapData::SerializeBackPointer");
  Handle<Map> map = Handle<Map>::cast(object());
  DCHECK_NULL(backpointer_);
  DCHECK(!map->IsContextMap());
  backpointer_ = broker->GetOrCreateData(map->GetBackPointer());
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

  for (InternalIndex i : map->IterateOwnDescriptors()) {
    SerializeOwnDescriptor(broker, i);
  }
}

void MapData::SerializeOwnDescriptor(JSHeapBroker* broker,
                                     InternalIndex descriptor_index) {
  TraceScope tracer(broker, this, "MapData::SerializeOwnDescriptor");
  Handle<Map> map = Handle<Map>::cast(object());
  Isolate* isolate = broker->isolate();

  if (instance_descriptors_ == nullptr) {
    instance_descriptors_ =
        broker->GetOrCreateData(map->instance_descriptors(isolate));
  }

  if (instance_descriptors()->should_access_heap()) {
    // When accessing the fields concurrently, we still have to recurse on the
    // owner map if it is different than the current map. This is because
    // {instance_descriptors_} gets set on SerializeOwnDescriptor and otherwise
    // we risk the field owner having a null {instance_descriptors_}.
    Handle<DescriptorArray> descriptors(map->instance_descriptors(isolate),
                                        isolate);
    if (descriptors->GetDetails(descriptor_index).location() == kField) {
      Handle<Map> owner(map->FindFieldOwner(isolate, descriptor_index),
                        isolate);
      if (!owner.equals(map)) {
        broker->GetOrCreateData(owner)->AsMap()->SerializeOwnDescriptor(
            broker, descriptor_index);
      }
    }
  } else {
    DescriptorArrayData* descriptors =
        instance_descriptors()->AsDescriptorArray();
    descriptors->SerializeDescriptor(broker, map, descriptor_index);
  }
}

void MapData::SerializeRootMap(JSHeapBroker* broker) {
  if (serialized_root_map_) return;
  serialized_root_map_ = true;

  TraceScope tracer(broker, this, "MapData::SerializeRootMap");
  Handle<Map> map = Handle<Map>::cast(object());
  DCHECK_NULL(root_map_);
  root_map_ = broker->GetOrCreateData(map->FindRootMap(broker->isolate()));
}

ObjectData* MapData::FindRootMap() const { return root_map_; }

void JSObjectData::SerializeRecursiveAsBoilerplate(JSHeapBroker* broker,
                                                   int depth) {
  if (serialized_as_boilerplate_) return;
  serialized_as_boilerplate_ = true;

  TraceScope tracer(broker, this,
                    "JSObjectData::SerializeRecursiveAsBoilerplate");
  Handle<JSObject> boilerplate = Handle<JSObject>::cast(object());

  // We only serialize boilerplates that pass the IsInlinableFastLiteral
  // check, so we only do a check on the depth here.
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
  DCHECK(!serialized_elements_);
  serialized_elements_ = true;
  elements_ = broker->GetOrCreateData(elements_object);
  DCHECK(elements_->IsFixedArrayBase());

  if (empty_or_cow || elements_->should_access_heap()) {
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
        if (!value_data->should_access_heap()) {
          value_data->AsJSObject()->SerializeRecursiveAsBoilerplate(broker,
                                                                    depth - 1);
        }
      }
    }
  } else {
    CHECK(boilerplate->HasDoubleElements());
    CHECK_LE(elements_object->Size(), kMaxRegularHeapObjectSize);
    DCHECK_EQ(elements_->kind(), ObjectDataKind::kSerializedHeapObject);
    elements_->AsFixedDoubleArray()->SerializeContents(broker);
  }

  // TODO(turbofan): Do we want to support out-of-object properties?
  CHECK(boilerplate->HasFastProperties() &&
        boilerplate->property_array().length() == 0);
  CHECK_EQ(inobject_fields_.size(), 0u);

  // Check the in-object properties.
  Handle<DescriptorArray> descriptors(
      boilerplate->map().instance_descriptors(isolate), isolate);
  for (InternalIndex i : boilerplate->map().IterateOwnDescriptors()) {
    PropertyDetails details = descriptors->GetDetails(i);
    if (details.location() != kField) continue;
    DCHECK_EQ(kData, details.kind());

    FieldIndex field_index = FieldIndex::ForDescriptor(boilerplate->map(), i);
    // Make sure {field_index} agrees with {inobject_properties} on the index of
    // this field.
    DCHECK_EQ(field_index.property_index(),
              static_cast<int>(inobject_fields_.size()));
    Handle<Object> value(boilerplate->RawFastPropertyAt(field_index), isolate);
    // In case of double fields we use a sentinel NaN value to mark
    // uninitialized fields. A boilerplate value with such a field may migrate
    // from its double to a tagged representation. The sentinel value carries
    // no special meaning when it occurs in a heap number, so we would like to
    // recover the uninitialized value. We check for the sentinel here,
    // specifically, since migrations might have been triggered as part of
    // boilerplate serialization.
    if (!details.representation().IsDouble() && value->IsHeapNumber() &&
        HeapNumber::cast(*value).value_as_bits() == kHoleNanInt64) {
      value = isolate->factory()->uninitialized_value();
    }
    ObjectData* value_data = broker->GetOrCreateData(value);
    if (value_data->IsJSObject() && !value_data->should_access_heap()) {
      value_data->AsJSObject()->SerializeRecursiveAsBoilerplate(broker,
                                                                depth - 1);
    }
    inobject_fields_.push_back(value_data);
  }
  TRACE(broker, "Copied " << inobject_fields_.size() << " in-object fields");

  if (!map()->should_access_heap()) {
    map()->AsMap()->SerializeOwnDescriptors(broker);
  }

  if (IsJSArray() && !FLAG_turbo_direct_heap_access) {
    AsJSArray()->Serialize(broker);
  }
}

void RegExpBoilerplateDescriptionData::Serialize(JSHeapBroker* broker) {
  if (serialized_) return;  // Only serialize once.
  serialized_ = true;

  TraceScope tracer(broker, this,
                    "RegExpBoilerplateDescriptionData::Serialize");
  auto boilerplate = Handle<RegExpBoilerplateDescription>::cast(object());

  data_ = broker->GetOrCreateData(boilerplate->data());
  source_ = broker->GetOrCreateData(boilerplate->source());
  flags_ = boilerplate->flags();
}

#ifdef DEBUG
bool ObjectRef::IsNeverSerializedHeapObject() const {
  return data_->kind() == ObjectDataKind::kNeverSerializedHeapObject;
}
#endif  // DEBUG

bool ObjectRef::equals(const ObjectRef& other) const {
#ifdef DEBUG
  if (broker()->mode() == JSHeapBroker::kSerialized &&
      data_->used_status == ObjectData::Usage::kUnused) {
    data_->used_status = ObjectData::Usage::kOnlyIdentityUsed;
  }
#endif  // DEBUG
  return data_ == other.data_;
}

bool ObjectRef::ShouldHaveBeenSerialized() const {
  return broker()->mode() == JSHeapBroker::kSerialized &&
         data()->kind() == kSerializedHeapObject;
}

Isolate* ObjectRef::isolate() const { return broker()->isolate(); }

ContextRef ContextRef::previous(size_t* depth,
                                SerializationPolicy policy) const {
  DCHECK_NOT_NULL(depth);

  if (data_->should_access_heap()) {
    Context current = *object();
    while (*depth != 0 && current.unchecked_previous().IsContext()) {
      current = Context::cast(current.unchecked_previous());
      (*depth)--;
    }
    return ContextRef(broker(), broker()->CanonicalPersistentHandle(current));
  }

  if (*depth == 0) return *this;

  ObjectData* previous_data = data()->AsContext()->previous(broker(), policy);
  if (previous_data == nullptr || !previous_data->IsContext()) return *this;

  *depth = *depth - 1;
  return ContextRef(broker(), previous_data).previous(depth, policy);
}

base::Optional<ObjectRef> ContextRef::get(int index,
                                          SerializationPolicy policy) const {
  if (data_->should_access_heap()) {
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

SourceTextModuleRef ContextRef::GetModule(SerializationPolicy policy) const {
  ContextRef current = *this;
  while (current.map().instance_type() != MODULE_CONTEXT_TYPE) {
    size_t depth = 1;
    current = current.previous(&depth, policy);
    CHECK_EQ(depth, 0);
  }
  return current.get(Context::EXTENSION_INDEX, policy)
      .value()
      .AsSourceTextModule();
}

JSHeapBroker::JSHeapBroker(Isolate* isolate, Zone* broker_zone,
                           bool tracing_enabled, bool is_concurrent_inlining,
                           CodeKind code_kind)
    : isolate_(isolate),
      zone_(broker_zone),
      refs_(zone()->New<RefsMap>(kMinimalRefsBucketCount, AddressMatcher(),
                                 zone())),
      root_index_map_(isolate),
      array_and_object_prototypes_(zone()),
      tracing_enabled_(tracing_enabled),
      is_concurrent_inlining_(is_concurrent_inlining),
      is_isolate_bootstrapping_(isolate->bootstrapper()->IsActive()),
      code_kind_(code_kind),
      feedback_(zone()),
      property_access_infos_(zone()),
      minimorphic_property_access_infos_(zone()),
      typed_array_string_tags_(zone()),
      serialized_functions_(zone()) {
  // Note that this initialization of {refs_} with the minimal initial capacity
  // is redundant in the normal use case (concurrent compilation enabled,
  // standard objects to be serialized), as the map is going to be replaced
  // immediately with a larger-capacity one.  It doesn't seem to affect the
  // performance in a noticeable way though.
  TRACE(this, "Constructing heap broker");
  DCHECK_IMPLIES(is_concurrent_inlining_, FLAG_turbo_direct_heap_access);
}

JSHeapBroker::~JSHeapBroker() { DCHECK_NULL(local_isolate_); }

void JSHeapBroker::SetPersistentAndCopyCanonicalHandlesForTesting(
    std::unique_ptr<PersistentHandles> persistent_handles,
    std::unique_ptr<CanonicalHandlesMap> canonical_handles) {
  set_persistent_handles(std::move(persistent_handles));
  CopyCanonicalHandlesForTesting(std::move(canonical_handles));
}

void JSHeapBroker::CopyCanonicalHandlesForTesting(
    std::unique_ptr<CanonicalHandlesMap> canonical_handles) {
  DCHECK_NULL(canonical_handles_);
  canonical_handles_ = std::make_unique<CanonicalHandlesMap>(
      isolate_->heap(), ZoneAllocationPolicy(zone()));

  CanonicalHandlesMap::IteratableScope it_scope(canonical_handles.get());
  for (auto it = it_scope.begin(); it != it_scope.end(); ++it) {
    Address* entry = *it.entry();
    Object key = it.key();
    canonical_handles_->Insert(key, entry);
  }
}

std::string JSHeapBroker::Trace() const {
  std::ostringstream oss;
  oss << "[" << this << "] ";
  for (unsigned i = 0; i < trace_indentation_ * 2; ++i) oss.put(' ');
  return oss.str();
}

void JSHeapBroker::AttachLocalIsolate(OptimizedCompilationInfo* info,
                                      LocalIsolate* local_isolate) {
  set_canonical_handles(info->DetachCanonicalHandles());
  DCHECK_NULL(local_isolate_);
  local_isolate_ = local_isolate;
  DCHECK_NOT_NULL(local_isolate_);
  local_isolate_->heap()->AttachPersistentHandles(
      info->DetachPersistentHandles());

  if (FLAG_turbo_direct_heap_access) {
    // Ensure any serialization that happens on the background has been
    // performed.
    target_native_context().SerializeOnBackground();
  }
}

void JSHeapBroker::DetachLocalIsolate(OptimizedCompilationInfo* info) {
  DCHECK_NULL(ph_);
  DCHECK_NOT_NULL(local_isolate_);
  std::unique_ptr<PersistentHandles> ph =
      local_isolate_->heap()->DetachPersistentHandles();
  local_isolate_ = nullptr;
  info->set_canonical_handles(DetachCanonicalHandles());
  info->set_persistent_handles(std::move(ph));
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
      InstanceType instance_type;
      if (ref->value->should_access_heap()) {
        instance_type = Handle<HeapObject>::cast(ref->value->object())
                            ->map()
                            .instance_type();
      } else {
        instance_type = ref->value->AsHeapObject()->GetMapInstanceType();
      }
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
  target_native_context_ =
      NativeContextRef(this, CanonicalPersistentHandle(*native_context));
}

void JSHeapBroker::CollectArrayAndObjectPrototypes() {
  DisallowGarbageCollection no_gc;
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

StringRef JSHeapBroker::GetTypedArrayStringTag(ElementsKind kind) {
  DCHECK(IsTypedArrayElementsKind(kind));
  switch (kind) {
#define TYPED_ARRAY_STRING_TAG(Type, type, TYPE, ctype) \
  case ElementsKind::TYPE##_ELEMENTS:                   \
    return StringRef(this, isolate()->factory()->Type##Array_string());
    TYPED_ARRAYS(TYPED_ARRAY_STRING_TAG)
#undef TYPED_ARRAY_STRING_TAG
    default:
      UNREACHABLE();
  }
}

bool JSHeapBroker::ShouldBeSerializedForCompilation(
    const SharedFunctionInfoRef& shared, const FeedbackVectorRef& feedback,
    const HintsVector& arguments) const {
  if (serialized_functions_.size() >= kMaxSerializedFunctionsCacheSize) {
    TRACE_BROKER_MISSING(this,
                         "opportunity - serialized functions cache is full.");
    return false;
  }
  SerializedFunction function{shared, feedback};
  auto matching_functions = serialized_functions_.equal_range(function);
  return std::find_if(matching_functions.first, matching_functions.second,
                      [&arguments](const auto& entry) {
                        return entry.second == arguments;
                      }) == matching_functions.second;
}

void JSHeapBroker::SetSerializedForCompilation(
    const SharedFunctionInfoRef& shared, const FeedbackVectorRef& feedback,
    const HintsVector& arguments) {
  SerializedFunction function{shared, feedback};
  serialized_functions_.insert({function, arguments});
  TRACE(this, "Set function " << shared << " with " << feedback
                              << " as serialized for compilation");
}

bool JSHeapBroker::IsSerializedForCompilation(
    const SharedFunctionInfoRef& shared,
    const FeedbackVectorRef& feedback) const {
  if (mode() == kDisabled) return true;

  SerializedFunction function = {shared, feedback};
  return serialized_functions_.find(function) != serialized_functions_.end();
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
  refs_ =
      zone()->New<RefsMap>(kInitialRefsBucketCount, AddressMatcher(), zone());

  SetTargetNativeContextRef(native_context);
  target_native_context().Serialize();
  if (!FLAG_turbo_direct_heap_access) {
    // Perform full native context serialization now if we can't do it later on
    // the background thread.
    target_native_context().SerializeOnBackground();
  }

  CollectArrayAndObjectPrototypes();

  Factory* const f = isolate()->factory();
  if (!FLAG_turbo_direct_heap_access) {
    ObjectData* data;
    data = GetOrCreateData(f->array_buffer_detaching_protector());
    if (!data->should_access_heap()) data->AsPropertyCell()->Serialize(this);
    data = GetOrCreateData(f->array_constructor_protector());
    if (!data->should_access_heap()) data->AsPropertyCell()->Serialize(this);
    data = GetOrCreateData(f->array_iterator_protector());
    if (!data->should_access_heap()) data->AsPropertyCell()->Serialize(this);
    data = GetOrCreateData(f->array_species_protector());
    if (!data->should_access_heap()) data->AsPropertyCell()->Serialize(this);
    data = GetOrCreateData(f->no_elements_protector());
    if (!data->should_access_heap()) data->AsPropertyCell()->Serialize(this);
    data = GetOrCreateData(f->promise_hook_protector());
    if (!data->should_access_heap()) data->AsPropertyCell()->Serialize(this);
    data = GetOrCreateData(f->promise_species_protector());
    if (!data->should_access_heap()) data->AsPropertyCell()->Serialize(this);
    data = GetOrCreateData(f->promise_then_protector());
    if (!data->should_access_heap()) data->AsPropertyCell()->Serialize(this);
    data = GetOrCreateData(f->string_length_protector());
    if (!data->should_access_heap()) data->AsPropertyCell()->Serialize(this);
  }
  GetOrCreateData(f->many_closures_cell());
  GetOrCreateData(
      CodeFactory::CEntry(isolate(), 1, kDontSaveFPRegs, kArgvOnStack, true));

  TRACE(this, "Finished serializing standard objects");
}

ObjectData* JSHeapBroker::GetOrCreateData(Handle<Object> object,
    ObjectRef::BackgroundSerialization background_serialization) {
  ObjectData* return_value =
      TryGetOrCreateData(object, true, background_serialization);
  DCHECK_NOT_NULL(return_value);
  return return_value;
}

// clang-format off
ObjectData* JSHeapBroker::TryGetOrCreateData(Handle<Object> object,
    bool crash_on_error,
    ObjectRef::BackgroundSerialization background_serialization) {
  RefsMap::Entry* entry = refs_->Lookup(object.address());
  if (entry != nullptr) return entry->value;

  if (mode() == JSHeapBroker::kDisabled) {
    entry = refs_->LookupOrInsert(object.address());
    ObjectData** storage = &(entry->value);
    if (*storage == nullptr) {
      entry->value = zone()->New<ObjectData>(
          this, storage, object,
          object->IsSmi() ? kSmi : kUnserializedHeapObject);
    }
    return *storage;
  }

  CHECK(mode() == JSHeapBroker::kSerializing ||
        mode() == JSHeapBroker::kSerialized);

  ObjectData* object_data;
  if (object->IsSmi()) {
    entry = refs_->LookupOrInsert(object.address());
    object_data = zone()->New<ObjectData>(this, &(entry->value), object, kSmi);
  } else if (IsReadOnlyHeapObject(*object)) {
    entry = refs_->LookupOrInsert(object.address());
    object_data = zone()->New<ObjectData>(this, &(entry->value), object,
                                          kUnserializedReadOnlyHeapObject);
// TODO(solanes, v8:10866): Remove the if/else in this macro once we remove the
// FLAG_turbo_direct_heap_access.
#define CREATE_DATA_FOR_DIRECT_READ(name)                                 \
  } else if (object->Is##name()) {                                        \
    if (FLAG_turbo_direct_heap_access) {                                  \
      entry = refs_->LookupOrInsert(object.address());                    \
      object_data = zone()->New<ObjectData>(                              \
        this, &(entry->value), object, kNeverSerializedHeapObject);       \
    } else if (mode() == kSerializing) {                                  \
      entry = refs_->LookupOrInsert(object.address());                    \
      object_data = zone()->New<name##Data>(this, &(entry->value),        \
                                            Handle<name>::cast(object));  \
    } else {                                                              \
      CHECK(!crash_on_error);                                             \
      return nullptr;                                                     \
    }
  HEAP_BROKER_NEVER_SERIALIZED_OBJECT_LIST(CREATE_DATA_FOR_DIRECT_READ)
#undef CREATE_DATA_FOR_DIRECT_READ
#define CREATE_DATA_FOR_POSSIBLE_SERIALIZATION(name)                      \
    } else if (object->Is##name()) {                                      \
      if (mode() == kSerialized &&                                        \
          background_serialization !=                                     \
            ObjectRef::BackgroundSerialization::kAllowed) {               \
        CHECK(!crash_on_error);                                           \
        return nullptr;                                                   \
      }                                                                   \
      entry = refs_->LookupOrInsert(object.address());                    \
      ObjectDataKind kind = (background_serialization ==                      \
                         ObjectRef::BackgroundSerialization::kAllowed)    \
                        ? kBackgroundSerializedHeapObject                 \
                        : kSerializedHeapObject;                          \
      object_data = zone()->New<name##Data>(this, &(entry->value),        \
                                            Handle<name>::cast(object),   \
                                            kind);
  HEAP_BROKER_POSSIBLY_BACKGROUND_SERIALIZED_OBJECT_LIST(
      CREATE_DATA_FOR_POSSIBLE_SERIALIZATION)
#undef CREATE_DATA_FOR_POSSIBLE_SERIALIZATION
#define CREATE_DATA_FOR_BACKGROUND_SERIALIZATION(name)                    \
  } else if (object->Is##name()) {                                        \
    if (FLAG_turbo_direct_heap_access) {                                  \
      entry = refs_->LookupOrInsert(object.address());                    \
      object_data = zone()->New<name##Data>(                              \
          this, &(entry->value), Handle<name>::cast(object),              \
          kBackgroundSerializedHeapObject);                               \
    } else if (mode() == kSerializing) {                                  \
      entry = refs_->LookupOrInsert(object.address());                    \
      object_data = zone()->New<name##Data>(this, &(entry->value),        \
                                            Handle<name>::cast(object));  \
    } else {                                                              \
      CHECK(!crash_on_error);                                             \
      return nullptr;                                                     \
    }
  HEAP_BROKER_BACKGROUND_SERIALIZED_OBJECT_LIST(
      CREATE_DATA_FOR_BACKGROUND_SERIALIZATION)
#undef CREATE_DATA_FOR_SERIALIZATION
#define CREATE_DATA_FOR_SERIALIZATION(name)                               \
  } else if (object->Is##name()) {                                        \
    if (mode() == kSerializing) {                                         \
      entry = refs_->LookupOrInsert(object.address());                    \
      object_data = zone()->New<name##Data>(this, &(entry->value),        \
                                            Handle<name>::cast(object));  \
    } else {                                                              \
      CHECK(!crash_on_error);                                             \
      return nullptr;                                                     \
    }
  HEAP_BROKER_SERIALIZED_OBJECT_LIST(CREATE_DATA_FOR_SERIALIZATION)
#undef CREATE_DATA_FOR_SERIALIZATION
  } else {
    UNREACHABLE();
  }
  // At this point the entry pointer is not guaranteed to be valid as
  // the refs_ hash hable could be resized by one of the constructors above.
  DCHECK_EQ(object_data, refs_->Lookup(object.address())->value);
  return object_data;
}
// clang-format on

ObjectData* JSHeapBroker::GetOrCreateData(
    Object object,
    ObjectRef::BackgroundSerialization background_serialization) {
  return GetOrCreateData(CanonicalPersistentHandle(object),
                         background_serialization);
}

#define DEFINE_IS_AND_AS(Name)                                    \
  bool ObjectRef::Is##Name() const { return data()->Is##Name(); } \
  Name##Ref ObjectRef::As##Name() const {                         \
    DCHECK(Is##Name());                                           \
    return Name##Ref(broker(), data());                           \
  }
HEAP_BROKER_SERIALIZED_OBJECT_LIST(DEFINE_IS_AND_AS)
HEAP_BROKER_POSSIBLY_BACKGROUND_SERIALIZED_OBJECT_LIST(DEFINE_IS_AND_AS)
HEAP_BROKER_BACKGROUND_SERIALIZED_OBJECT_LIST(DEFINE_IS_AND_AS)
HEAP_BROKER_NEVER_SERIALIZED_OBJECT_LIST(DEFINE_IS_AND_AS)
#undef DEFINE_IS_AND_AS

bool JSHeapBroker::StackHasOverflowed() const {
  DCHECK_IMPLIES(local_isolate_ == nullptr,
                 ThreadId::Current() == isolate_->thread_id());
  return (local_isolate_ != nullptr)
             ? StackLimitCheck::HasOverflowed(local_isolate_)
             : StackLimitCheck(isolate_).HasOverflowed();
}

bool JSHeapBroker::ObjectMayBeUninitialized(Handle<Object> object) const {
  return !IsMainThread() && object->IsHeapObject() &&
         isolate()->heap()->IsPendingAllocation(HeapObject::cast(*object));
}

bool ObjectRef::IsSmi() const { return data()->is_smi(); }

int ObjectRef::AsSmi() const {
  DCHECK(IsSmi());
  // Handle-dereference is always allowed for Handle<Smi>.
  return Handle<Smi>::cast(object())->value();
}

base::Optional<MapRef> JSObjectRef::GetObjectCreateMap() const {
  if (data_->should_access_heap()) {
    Handle<Map> instance_map;
    if (Map::TryGetObjectCreateMap(broker()->isolate(), object())
            .ToHandle(&instance_map)) {
      return MapRef(broker(), instance_map);
    } else {
      return base::Optional<MapRef>();
    }
  }
  ObjectData* map_data = data()->AsJSObject()->object_create_map(broker());
  if (map_data == nullptr) return base::Optional<MapRef>();
  if (map_data->should_access_heap()) {
    return MapRef(broker(), map_data->object());
  }
  return MapRef(broker(), map_data->AsMap());
}

#define DEF_TESTER(Type, ...)                              \
  bool MapRef::Is##Type##Map() const {                     \
    return InstanceTypeChecker::Is##Type(instance_type()); \
  }
INSTANCE_TYPE_CHECKERS(DEF_TESTER)
#undef DEF_TESTER

base::Optional<MapRef> MapRef::AsElementsKind(ElementsKind kind) const {
  if (data_->should_access_heap()) {
    return MapRef(broker(),
                  Map::AsElementsKind(broker()->isolate(), object(), kind));
  }
  if (kind == elements_kind()) return *this;
  const ZoneVector<ObjectData*>& elements_kind_generalizations =
      data()->AsMap()->elements_kind_generalizations();
  for (auto data : elements_kind_generalizations) {
    MapRef map(broker(), data);
    if (map.elements_kind() == kind) return map;
  }
  return base::Optional<MapRef>();
}

void MapRef::SerializeForElementLoad() {
  if (data()->should_access_heap()) return;
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsMap()->SerializeForElementLoad(broker());
}

void MapRef::SerializeForElementStore() {
  if (data()->should_access_heap()) return;
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsMap()->SerializeForElementStore(broker());
}

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
  // TODO(solanes, v8:7790): This should use MapData methods rather than
  // constructing MapRefs, but it involves non-trivial refactoring and this
  // method should go away anyway once the compiler is fully concurrent.
  MapRef map(broker, this);
  for (MapRef prototype_map = map;;) {
    prototype_map.SerializePrototype();
    prototype_map = prototype_map.prototype().map();
    if (prototype_map.oddball_type() == OddballType::kNull ||
        !map.prototype().IsJSObject() || !prototype_map.is_stable() ||
        !IsFastElementsKind(prototype_map.elements_kind())) {
      return;
    }
  }
}

bool MapRef::HasOnlyStablePrototypesWithFastElements(
    ZoneVector<MapRef>* prototype_maps) {
  DCHECK_NOT_NULL(prototype_maps);
  MapRef prototype_map = prototype().map();
  while (prototype_map.oddball_type() != OddballType::kNull) {
    if (!prototype().IsJSObject() || !prototype_map.is_stable() ||
        !IsFastElementsKind(prototype_map.elements_kind())) {
      return false;
    }
    prototype_maps->push_back(prototype_map);
    prototype_map = prototype_map.prototype().map();
  }
  return true;
}

bool MapRef::supports_fast_array_iteration() const {
  if (data_->should_access_heap()) {
    return SupportsFastArrayIteration(broker()->isolate(), object());
  }
  return data()->AsMap()->supports_fast_array_iteration();
}

bool MapRef::supports_fast_array_resize() const {
  if (data_->should_access_heap()) {
    return SupportsFastArrayResize(broker()->isolate(), object());
  }
  return data()->AsMap()->supports_fast_array_resize();
}

int JSFunctionRef::InitialMapInstanceSizeWithMinSlack() const {
  if (data_->should_access_heap()) {
    return object()->ComputeInstanceSizeWithMinSlack(broker()->isolate());
  }
  return data()->AsJSFunction()->initial_map_instance_size_with_min_slack();
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
  if (data_->should_access_heap()) {
    FeedbackCell cell = object()->closure_feedback_cell(index);
    Handle<FeedbackCell> cell_handle =
        broker()->CanonicalPersistentHandle(cell);
    // These should all be available because we request the cell for each
    // CreateClosure bytecode.
    ObjectData* cell_data = broker()->GetOrCreateData(cell_handle);
    return FeedbackCellRef(broker(), cell_data);
  }

  return FeedbackCellRef(
      broker(),
      data()->AsFeedbackVector()->GetClosureFeedbackCell(broker(), index));
}

ObjectRef JSObjectRef::RawFastPropertyAt(FieldIndex index) const {
  CHECK(index.is_inobject());
  if (data_->should_access_heap()) {
    return ObjectRef(broker(), broker()->CanonicalPersistentHandle(
                                   object()->RawFastPropertyAt(index)));
  }
  JSObjectData* object_data = data()->AsJSObject();
  return ObjectRef(broker(),
                   object_data->GetInobjectField(index.property_index()));
}

bool AllocationSiteRef::IsFastLiteral() const {
  if (data_->should_access_heap()) {
    CHECK_NE(data_->kind(), ObjectDataKind::kNeverSerializedHeapObject);
    return IsInlinableFastLiteral(
        handle(object()->boilerplate(), broker()->isolate()));
  }
  return data()->AsAllocationSite()->IsFastLiteral();
}

void AllocationSiteRef::SerializeBoilerplate() {
  if (data_->should_access_heap()) return;
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsAllocationSite()->SerializeBoilerplate(broker());
}

void JSObjectRef::SerializeElements() {
  if (data_->should_access_heap()) return;
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsJSObject()->SerializeElements(broker());
}

void JSObjectRef::EnsureElementsTenured() {
  if (data_->should_access_heap()) {
    Handle<FixedArrayBase> object_elements = elements().value().object();
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
  CHECK_LT(descriptor_index.as_int(), NumberOfOwnDescriptors());
  if (data_->should_access_heap() || FLAG_turbo_direct_heap_access) {
    FieldIndex result = FieldIndex::ForDescriptor(*object(), descriptor_index);
    DCHECK(result.is_inobject());
    return result;
  }
  DescriptorArrayData* descriptors =
      data()->AsMap()->instance_descriptors()->AsDescriptorArray();
  FieldIndex result = descriptors->GetFieldIndexFor(descriptor_index);
  DCHECK(result.is_inobject());
  return result;
}

int MapRef::GetInObjectPropertyOffset(int i) const {
  if (data_->should_access_heap()) {
    return object()->GetInObjectPropertyOffset(i);
  }
  return (GetInObjectPropertiesStartInWords() + i) * kTaggedSize;
}

PropertyDetails MapRef::GetPropertyDetails(
    InternalIndex descriptor_index) const {
  CHECK_LT(descriptor_index.as_int(), NumberOfOwnDescriptors());
  return instance_descriptors().GetPropertyDetails(descriptor_index);
}

NameRef MapRef::GetPropertyKey(InternalIndex descriptor_index) const {
  CHECK_LT(descriptor_index.as_int(), NumberOfOwnDescriptors());
  return instance_descriptors().GetPropertyKey(descriptor_index);
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
  CHECK_LT(descriptor_index.as_int(), NumberOfOwnDescriptors());
  if (data_->should_access_heap() || FLAG_turbo_direct_heap_access) {
    // TODO(solanes, v8:7790): Consider caching the result of the field owner on
    // the descriptor array. It would be useful for same map as well as any
    // other map sharing that descriptor array.
    return MapRef(broker(), broker()->GetOrCreateData(object()->FindFieldOwner(
                                broker()->isolate(), descriptor_index)));
  }
  DescriptorArrayData* descriptors =
      data()->AsMap()->instance_descriptors()->AsDescriptorArray();
  return MapRef(broker(), descriptors->FindFieldOwner(descriptor_index));
}

ObjectRef MapRef::GetFieldType(InternalIndex descriptor_index) const {
  CHECK_LT(descriptor_index.as_int(), NumberOfOwnDescriptors());
  return instance_descriptors().GetFieldType(descriptor_index);
}

base::Optional<ObjectRef> StringRef::GetCharAsStringOrUndefined(
    uint32_t index, SerializationPolicy policy) const {
  if (data_->should_access_heap()) {
    // TODO(solanes, neis, v8:7790, v8:11012): Re-enable this optimization for
    // concurrent inlining when we have the infrastructure to safely do so.
    if (broker()->is_concurrent_inlining()) return base::nullopt;
    CHECK_EQ(data_->kind(), ObjectDataKind::kUnserializedHeapObject);
    return GetOwnElementFromHeap(broker(), object(), index, true);
  }
  ObjectData* element =
      data()->AsString()->GetCharAsStringOrUndefined(broker(), index, policy);
  if (element == nullptr) return base::nullopt;
  return ObjectRef(broker(), element);
}

base::Optional<int> StringRef::length() const {
  if (data_->should_access_heap()) {
    if (data_->kind() == kNeverSerializedHeapObject &&
        !this->IsInternalizedString()) {
      TRACE_BROKER_MISSING(
          broker(),
          "length for kNeverSerialized non-internalized string " << *this);
      return base::nullopt;
    } else {
      return object()->synchronized_length();
    }
  }
  return data()->AsString()->length();
}

base::Optional<uint16_t> StringRef::GetFirstChar() {
  if (data_->should_access_heap()) {
    if (data_->kind() == kNeverSerializedHeapObject &&
        !this->IsInternalizedString()) {
      TRACE_BROKER_MISSING(
          broker(),
          "first char for kNeverSerialized non-internalized string " << *this);
      return base::nullopt;
    }

    if (!broker()->IsMainThread()) {
      return object()->Get(0, broker()->local_isolate());
    } else {
      // TODO(solanes, v8:7790): Remove this case once the inlining phase is
      // done concurrently all the time.
      return object()->Get(0);
    }
  }
  return data()->AsString()->first_char();
}

base::Optional<double> StringRef::ToNumber() {
  if (data_->should_access_heap()) {
    if (data_->kind() == kNeverSerializedHeapObject &&
        !this->IsInternalizedString()) {
      TRACE_BROKER_MISSING(
          broker(),
          "number for kNeverSerialized non-internalized string " << *this);
      return base::nullopt;
    }

    return TryStringToDouble(broker()->local_isolate(), object());
  }
  return data()->AsString()->to_number();
}

int ArrayBoilerplateDescriptionRef::constants_elements_length() const {
  if (data_->should_access_heap()) {
    return object()->constant_elements().length();
  }
  return data()->AsArrayBoilerplateDescription()->constants_elements_length();
}

ObjectRef FixedArrayRef::get(int i) const {
  if (data_->should_access_heap()) {
    return ObjectRef(broker(),
                     broker()->CanonicalPersistentHandle(object()->get(i)));
  }
  return ObjectRef(broker(), data()->AsFixedArray()->Get(i));
}

Float64 FixedDoubleArrayRef::get(int i) const {
  if (data_->should_access_heap()) {
    return Float64::FromBits(object()->get_representation(i));
  } else {
    return data()->AsFixedDoubleArray()->Get(i);
  }
}

Handle<ByteArray> BytecodeArrayRef::SourcePositionTable() const {
  return broker()->CanonicalPersistentHandle(object()->SourcePositionTable());
}

Address BytecodeArrayRef::handler_table_address() const {
  return reinterpret_cast<Address>(
      object()->handler_table().GetDataStartAddress());
}

int BytecodeArrayRef::handler_table_size() const {
  return object()->handler_table().length();
}

#define IF_ACCESS_FROM_HEAP_C(name)  \
  if (data_->should_access_heap()) { \
    return object()->name();         \
  }

#define IF_ACCESS_FROM_HEAP(result, name)                                      \
  if (data_->should_access_heap()) {                                           \
    return result##Ref(broker(),                                               \
                       broker()->CanonicalPersistentHandle(object()->name())); \
  }

// Macros for definining a const getter that, depending on the data kind,
// either looks into the heap or into the serialized data.
#define BIMODAL_ACCESSOR(holder, result, name)                             \
  result##Ref holder##Ref::name() const {                                  \
    IF_ACCESS_FROM_HEAP(result, name);                                     \
    return result##Ref(broker(), ObjectRef::data()->As##holder()->name()); \
  }

// Like above except that the result type is not an XYZRef.
#define BIMODAL_ACCESSOR_C(holder, result, name)    \
  result holder##Ref::name() const {                \
    IF_ACCESS_FROM_HEAP_C(name);                    \
    return ObjectRef::data()->As##holder()->name(); \
  }

// Like above but for BitFields.
#define BIMODAL_ACCESSOR_B(holder, field, name, BitField)              \
  typename BitField::FieldType holder##Ref::name() const {             \
    IF_ACCESS_FROM_HEAP_C(name);                                       \
    return BitField::decode(ObjectRef::data()->As##holder()->field()); \
  }

// Like IF_ACCESS_FROM_HEAP[_C] but we also allow direct heap access for
// kSerialized only for methods that we identified to be safe.
#define IF_ACCESS_FROM_HEAP_WITH_FLAG(result, name)                            \
  if (data_->should_access_heap() || FLAG_turbo_direct_heap_access) {          \
    return result##Ref(broker(),                                               \
                       broker()->CanonicalPersistentHandle(object()->name())); \
  }
#define IF_ACCESS_FROM_HEAP_WITH_FLAG_C(name)                         \
  if (data_->should_access_heap() || FLAG_turbo_direct_heap_access) { \
    return object()->name();                                          \
  }

// Like BIMODAL_ACCESSOR[_C] except that we force a direct heap access if
// FLAG_turbo_direct_heap_access is true (even for kSerialized). This is because
// we identified the method to be safe to use direct heap access, but the
// holder##Data class still needs to be serialized.
#define BIMODAL_ACCESSOR_WITH_FLAG(holder, result, name)                   \
  result##Ref holder##Ref::name() const {                                  \
    IF_ACCESS_FROM_HEAP_WITH_FLAG(result, name);                           \
    return result##Ref(broker(), ObjectRef::data()->As##holder()->name()); \
  }
#define BIMODAL_ACCESSOR_WITH_FLAG_C(holder, result, name) \
  result holder##Ref::name() const {                       \
    IF_ACCESS_FROM_HEAP_WITH_FLAG_C(name);                 \
    return ObjectRef::data()->As##holder()->name();        \
  }
#define BIMODAL_ACCESSOR_WITH_FLAG_B(holder, field, name, BitField)    \
  typename BitField::FieldType holder##Ref::name() const {             \
    IF_ACCESS_FROM_HEAP_WITH_FLAG_C(name);                             \
    return BitField::decode(ObjectRef::data()->As##holder()->field()); \
  }

BIMODAL_ACCESSOR(AllocationSite, Object, nested_site)
BIMODAL_ACCESSOR_C(AllocationSite, bool, CanInlineCall)
BIMODAL_ACCESSOR_C(AllocationSite, bool, PointsToLiteral)
BIMODAL_ACCESSOR_C(AllocationSite, ElementsKind, GetElementsKind)
BIMODAL_ACCESSOR_C(AllocationSite, AllocationType, GetAllocationType)

BIMODAL_ACCESSOR_C(BigInt, uint64_t, AsUint64)

BIMODAL_ACCESSOR_C(BytecodeArray, int, register_count)
BIMODAL_ACCESSOR_C(BytecodeArray, int, parameter_count)
BIMODAL_ACCESSOR_C(BytecodeArray, interpreter::Register,
                   incoming_new_target_or_generator_register)

BIMODAL_ACCESSOR_C(FeedbackVector, double, invocation_count)

BIMODAL_ACCESSOR(HeapObject, Map, map)

BIMODAL_ACCESSOR_C(HeapNumber, double, value)

// These JSBoundFunction fields are immutable after initialization. Moreover,
// as long as JSObjects are still serialized on the main thread, all
// JSBoundFunctionRefs are created at a time when the underlying objects are
// guaranteed to be fully initialized.
BIMODAL_ACCESSOR_WITH_FLAG(JSBoundFunction, JSReceiver, bound_target_function)
BIMODAL_ACCESSOR_WITH_FLAG(JSBoundFunction, Object, bound_this)
BIMODAL_ACCESSOR_WITH_FLAG(JSBoundFunction, FixedArray, bound_arguments)

BIMODAL_ACCESSOR_C(JSDataView, size_t, byte_length)

BIMODAL_ACCESSOR_C(JSFunction, bool, has_feedback_vector)
BIMODAL_ACCESSOR_C(JSFunction, bool, has_initial_map)
BIMODAL_ACCESSOR_C(JSFunction, bool, has_prototype)
BIMODAL_ACCESSOR_C(JSFunction, bool, PrototypeRequiresRuntimeLookup)
BIMODAL_ACCESSOR(JSFunction, Context, context)
BIMODAL_ACCESSOR(JSFunction, NativeContext, native_context)
BIMODAL_ACCESSOR(JSFunction, Map, initial_map)
BIMODAL_ACCESSOR(JSFunction, Object, prototype)
BIMODAL_ACCESSOR(JSFunction, SharedFunctionInfo, shared)
BIMODAL_ACCESSOR(JSFunction, FeedbackCell, raw_feedback_cell)
BIMODAL_ACCESSOR(JSFunction, FeedbackVector, feedback_vector)

BIMODAL_ACCESSOR_C(JSGlobalObject, bool, IsDetached)

BIMODAL_ACCESSOR_WITH_FLAG_B(Map, bit_field2, elements_kind,
                             Map::Bits2::ElementsKindBits)
BIMODAL_ACCESSOR_WITH_FLAG_B(Map, bit_field3, is_dictionary_map,
                             Map::Bits3::IsDictionaryMapBit)
BIMODAL_ACCESSOR_WITH_FLAG_B(Map, bit_field3, is_deprecated,
                             Map::Bits3::IsDeprecatedBit)
BIMODAL_ACCESSOR_WITH_FLAG_B(Map, bit_field3, NumberOfOwnDescriptors,
                             Map::Bits3::NumberOfOwnDescriptorsBits)
BIMODAL_ACCESSOR_WITH_FLAG_B(Map, bit_field3, is_migration_target,
                             Map::Bits3::IsMigrationTargetBit)
BIMODAL_ACCESSOR_WITH_FLAG_B(Map, bit_field, has_prototype_slot,
                             Map::Bits1::HasPrototypeSlotBit)
BIMODAL_ACCESSOR_WITH_FLAG_B(Map, bit_field, is_access_check_needed,
                             Map::Bits1::IsAccessCheckNeededBit)
BIMODAL_ACCESSOR_WITH_FLAG_B(Map, bit_field, is_callable,
                             Map::Bits1::IsCallableBit)
BIMODAL_ACCESSOR_WITH_FLAG_B(Map, bit_field, has_indexed_interceptor,
                             Map::Bits1::HasIndexedInterceptorBit)
BIMODAL_ACCESSOR_WITH_FLAG_B(Map, bit_field, is_constructor,
                             Map::Bits1::IsConstructorBit)
BIMODAL_ACCESSOR_WITH_FLAG_B(Map, bit_field, is_undetectable,
                             Map::Bits1::IsUndetectableBit)
BIMODAL_ACCESSOR_C(Map, int, instance_size)
BIMODAL_ACCESSOR_C(Map, int, NextFreePropertyIndex)
BIMODAL_ACCESSOR_C(Map, int, UnusedPropertyFields)
BIMODAL_ACCESSOR_WITH_FLAG(Map, HeapObject, prototype)
BIMODAL_ACCESSOR_WITH_FLAG_C(Map, InstanceType, instance_type)
BIMODAL_ACCESSOR_WITH_FLAG(Map, Object, GetConstructor)
BIMODAL_ACCESSOR_WITH_FLAG(Map, HeapObject, GetBackPointer)
BIMODAL_ACCESSOR_C(Map, bool, is_abandoned_prototype_map)

#define DEF_NATIVE_CONTEXT_ACCESSOR(type, name) \
  BIMODAL_ACCESSOR(NativeContext, type, name)
BROKER_NATIVE_CONTEXT_FIELDS(DEF_NATIVE_CONTEXT_ACCESSOR)
#undef DEF_NATIVE_CONTEXT_ACCESSOR

BIMODAL_ACCESSOR_C(ObjectBoilerplateDescription, int, size)

BIMODAL_ACCESSOR(PropertyCell, Object, value)
BIMODAL_ACCESSOR_C(PropertyCell, PropertyDetails, property_details)

BIMODAL_ACCESSOR(RegExpBoilerplateDescription, FixedArray, data)
BIMODAL_ACCESSOR(RegExpBoilerplateDescription, String, source)
BIMODAL_ACCESSOR_C(RegExpBoilerplateDescription, int, flags)

base::Optional<CallHandlerInfoRef> FunctionTemplateInfoRef::call_code() const {
  if (data_->should_access_heap()) {
    HeapObject call_code = object()->call_code(kAcquireLoad);
    if (call_code.IsUndefined()) return base::nullopt;
    return CallHandlerInfoRef(broker(),
                              broker()->CanonicalPersistentHandle(call_code));
  }
  ObjectData* call_code = data()->AsFunctionTemplateInfo()->call_code();
  if (!call_code) return base::nullopt;
  return CallHandlerInfoRef(broker(), call_code);
}

bool FunctionTemplateInfoRef::is_signature_undefined() const {
  if (data_->should_access_heap()) {
    return object()->signature().IsUndefined(broker()->isolate());
  }
  return data()->AsFunctionTemplateInfo()->is_signature_undefined();
}

bool FunctionTemplateInfoRef::has_call_code() const {
  if (data_->should_access_heap()) {
    HeapObject call_code = object()->call_code(kAcquireLoad);
    return !call_code.IsUndefined();
  }
  return data()->AsFunctionTemplateInfo()->has_call_code();
}

bool FunctionTemplateInfoRef ::accept_any_receiver() const {
  if (data_->should_access_heap()) {
    return object()->accept_any_receiver();
  }
  return ObjectRef ::data()->AsFunctionTemplateInfo()->accept_any_receiver();
}

HolderLookupResult FunctionTemplateInfoRef::LookupHolderOfExpectedType(
    MapRef receiver_map, SerializationPolicy policy) {
  const HolderLookupResult not_found;

  if (data_->should_access_heap()) {
    // There are currently two ways we can see a FunctionTemplateInfo on the
    // background thread: 1.) As part of a SharedFunctionInfo and 2.) in an
    // AccessorPair. In both cases, the FTI is fully constructed on the main
    // thread before.
    // TODO(nicohartmann@, v8:7790): Once the above no longer holds, we might
    // have to use the GC predicate to check whether objects are fully
    // initialized and safe to read.
    if (!receiver_map.IsJSReceiverMap() ||
        (receiver_map.is_access_check_needed() &&
         !object()->accept_any_receiver())) {
      return not_found;
    }

    if (!receiver_map.IsJSObjectMap()) return not_found;

    DCHECK(has_call_code());

    DisallowGarbageCollection no_gc;
    HeapObject signature = object()->signature();
    if (signature.IsUndefined()) {
      return HolderLookupResult(CallOptimization::kHolderIsReceiver);
    }
    auto expected_receiver_type = FunctionTemplateInfo::cast(signature);
    if (expected_receiver_type.IsTemplateFor(*receiver_map.object())) {
      return HolderLookupResult(CallOptimization::kHolderIsReceiver);
    }

    if (!receiver_map.IsJSGlobalProxyMap()) return not_found;
    if (policy == SerializationPolicy::kSerializeIfNeeded) {
      receiver_map.SerializePrototype();
    }
    if (!receiver_map.serialized_prototype()) return not_found;
    if (receiver_map.prototype().IsNull()) return not_found;

    JSObject raw_prototype = JSObject::cast(*receiver_map.prototype().object());
    if (!expected_receiver_type.IsTemplateFor(raw_prototype.map())) {
      return not_found;
    }
    Handle<JSObject> prototype =
        broker()->CanonicalPersistentHandle(raw_prototype);
    if (ObjectData* data = broker()->TryGetOrCreateData(prototype)) {
      return HolderLookupResult(CallOptimization::kHolderFound,
                                JSObjectRef(broker(), data));
    }

    TRACE_BROKER_MISSING(broker(),
                         "holder for receiver with map " << receiver_map);
    return not_found;
  }

  FunctionTemplateInfoData* fti_data = data()->AsFunctionTemplateInfo();
  KnownReceiversMap::iterator lookup_it =
      fti_data->known_receivers().find(receiver_map.data());
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
    fti_data->known_receivers().insert({receiver_map.data(), not_found});
    return not_found;
  }

  HolderLookupResult result;
  CallOptimization call_optimization(broker()->isolate(), object());
  Handle<JSObject> holder = call_optimization.LookupHolderOfExpectedType(
      receiver_map.object(), &result.lookup);

  switch (result.lookup) {
    case CallOptimization::kHolderFound: {
      result.holder = JSObjectRef(broker(), holder);
      fti_data->known_receivers().insert({receiver_map.data(), result});
      break;
    }
    default: {
      DCHECK_EQ(result.holder, base::nullopt);
      fti_data->known_receivers().insert({receiver_map.data(), result});
    }
  }
  return result;
}

BIMODAL_ACCESSOR(CallHandlerInfo, Object, data)

BIMODAL_ACCESSOR_C(ScopeInfo, int, ContextLength)
BIMODAL_ACCESSOR_C(ScopeInfo, bool, HasContextExtensionSlot)
BIMODAL_ACCESSOR_C(ScopeInfo, bool, HasOuterScopeInfo)
BIMODAL_ACCESSOR(ScopeInfo, ScopeInfo, OuterScopeInfo)

BIMODAL_ACCESSOR_C(SharedFunctionInfo, int, builtin_id)
BytecodeArrayRef SharedFunctionInfoRef::GetBytecodeArray() const {
  if (data_->should_access_heap() || FLAG_turbo_direct_heap_access) {
    BytecodeArray bytecode_array;
    if (!broker()->IsMainThread()) {
      bytecode_array = object()->GetBytecodeArray(broker()->local_isolate());
    } else {
      bytecode_array = object()->GetBytecodeArray(broker()->isolate());
    }
    return BytecodeArrayRef(
        broker(), broker()->CanonicalPersistentHandle(bytecode_array));
  }
  return BytecodeArrayRef(
      broker(), ObjectRef ::data()->AsSharedFunctionInfo()->GetBytecodeArray());
}
#define DEF_SFI_ACCESSOR(type, name) \
  BIMODAL_ACCESSOR_WITH_FLAG_C(SharedFunctionInfo, type, name)
BROKER_SFI_FIELDS(DEF_SFI_ACCESSOR)
#undef DEF_SFI_ACCESSOR
SharedFunctionInfo::Inlineability SharedFunctionInfoRef::GetInlineability()
    const {
  if (data_->should_access_heap()) {
    if (!broker()->IsMainThread()) {
      return object()->GetInlineability(broker()->local_isolate());
    } else {
      return object()->GetInlineability(broker()->isolate());
    }
  }
  return ObjectRef ::data()->AsSharedFunctionInfo()->GetInlineability();
}

base::Optional<FeedbackVectorRef> FeedbackCellRef::value() const {
  if (data_->should_access_heap()) {
    // Note that we use the synchronized accessor.
    Object value = object()->value(kAcquireLoad);
    if (!value.IsFeedbackVector()) return base::nullopt;
    auto vector_handle = broker()->CanonicalPersistentHandle(value);
    ObjectData* vector = broker()->TryGetOrCreateData(vector_handle);
    if (vector) {
      return FeedbackVectorRef(broker(), vector);
    }
    TRACE_BROKER_MISSING(
        broker(),
        "Unable to retrieve FeedbackVector from FeedbackCellRef " << *this);
    return base::nullopt;
  }
  ObjectData* vector = ObjectRef::data()->AsFeedbackCell()->value();
  return FeedbackVectorRef(broker(), vector->AsFeedbackVector());
}

base::Optional<ObjectRef> MapRef::GetStrongValue(
    InternalIndex descriptor_index) const {
  CHECK_LT(descriptor_index.as_int(), NumberOfOwnDescriptors());
  return instance_descriptors().GetStrongValue(descriptor_index);
}

DescriptorArrayRef MapRef::instance_descriptors() const {
  if (data_->should_access_heap() || FLAG_turbo_direct_heap_access) {
    return DescriptorArrayRef(
        broker(),
        broker()->CanonicalPersistentHandle(
            object()->instance_descriptors(broker()->isolate(), kRelaxedLoad)));
  }

  return DescriptorArrayRef(broker(), data()->AsMap()->instance_descriptors());
}

void MapRef::SerializeRootMap() {
  if (data_->should_access_heap()) return;
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsMap()->SerializeRootMap(broker());
}

// TODO(solanes, v8:7790): Remove base::Optional from the return type when
// deleting serialization.
base::Optional<MapRef> MapRef::FindRootMap() const {
  if (data_->should_access_heap() || FLAG_turbo_direct_heap_access) {
    // TODO(solanes): Remove the TryGetOrCreateData part when Map is moved to
    // kNeverSerialized.
    ObjectData* root_map =
        broker()->TryGetOrCreateData(broker()->CanonicalPersistentHandle(
            object()->FindRootMap(broker()->isolate())));
    if (root_map) {
      // TODO(solanes, v8:7790): Consider caching the result of the root map.
      return MapRef(broker(), root_map);
    }
    TRACE_BROKER_MISSING(broker(), "root map for object " << *this);
    return base::nullopt;
  }
  ObjectData* map_data = data()->AsMap()->FindRootMap();
  if (map_data != nullptr) {
    return MapRef(broker(), map_data);
  }
  TRACE_BROKER_MISSING(broker(), "root map for object " << *this);
  return base::nullopt;
}

bool JSTypedArrayRef::is_on_heap() const {
  if (data_->should_access_heap() || FLAG_turbo_direct_heap_access) {
    // Safe to read concurrently because:
    // - host object seen by serializer.
    // - underlying field written 1. during initialization or 2. with
    //   release-store.
    return object()->is_on_heap(kAcquireLoad);
  }
  return data()->AsJSTypedArray()->data_ptr();
}

size_t JSTypedArrayRef::length() const {
  CHECK(!is_on_heap());
  if (data_->should_access_heap() || FLAG_turbo_direct_heap_access) {
    // Safe to read concurrently because:
    // - immutable after initialization.
    // - host object seen by serializer.
    return object()->length();
  }
  return data()->AsJSTypedArray()->length();
}

HeapObjectRef JSTypedArrayRef::buffer() const {
  CHECK(!is_on_heap());
  if (data_->should_access_heap() || FLAG_turbo_direct_heap_access) {
    // Safe to read concurrently because:
    // - immutable after initialization.
    // - host object seen by serializer.
    Handle<JSArrayBuffer> value =
        broker()->CanonicalPersistentHandle(object()->buffer());
    return JSObjectRef{broker(), value};
  }
  return HeapObjectRef{broker(), data()->AsJSTypedArray()->buffer()};
}

void* JSTypedArrayRef::data_ptr() const {
  CHECK(!is_on_heap());
  if (data_->should_access_heap() || FLAG_turbo_direct_heap_access) {
    // Safe to read concurrently because:
    // - host object seen by serializer.
    // - underlying field written 1. during initialization or 2. protected by
    //   the is_on_heap release/acquire semantics (external_pointer store
    //   happens-before base_pointer store, and this external_pointer load
    //   happens-after base_pointer load).
    STATIC_ASSERT(JSTypedArray::kOffHeapDataPtrEqualsExternalPointer);
    return object()->DataPtr();
  }
  return data()->AsJSTypedArray()->data_ptr();
}

bool MapRef::IsInobjectSlackTrackingInProgress() const {
  IF_ACCESS_FROM_HEAP_WITH_FLAG_C(IsInobjectSlackTrackingInProgress);
  return Map::Bits3::ConstructionCounterBits::decode(
             data()->AsMap()->bit_field3()) != Map::kNoSlackTracking;
}

int MapRef::constructor_function_index() const {
  IF_ACCESS_FROM_HEAP_C(GetConstructorFunctionIndex);
  CHECK(IsPrimitiveMap());
  return data()->AsMap()->constructor_function_index();
}

bool MapRef::is_stable() const {
  IF_ACCESS_FROM_HEAP_C(is_stable);
  return !Map::Bits3::IsUnstableBit::decode(data()->AsMap()->bit_field3());
}

bool MapRef::CanBeDeprecated() const {
  IF_ACCESS_FROM_HEAP_C(CanBeDeprecated);
  CHECK_GT(NumberOfOwnDescriptors(), 0);
  return data()->AsMap()->can_be_deprecated();
}

bool MapRef::CanTransition() const {
  IF_ACCESS_FROM_HEAP_C(CanTransition);
  return data()->AsMap()->can_transition();
}

int MapRef::GetInObjectPropertiesStartInWords() const {
  IF_ACCESS_FROM_HEAP_C(GetInObjectPropertiesStartInWords);
  return data()->AsMap()->in_object_properties_start_in_words();
}

int MapRef::GetInObjectProperties() const {
  IF_ACCESS_FROM_HEAP_C(GetInObjectProperties);
  return data()->AsMap()->in_object_properties();
}

void ScopeInfoRef::SerializeScopeInfoChain() {
  if (data_->should_access_heap()) return;
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsScopeInfo()->SerializeScopeInfoChain(broker());
}

bool StringRef::IsExternalString() const {
  IF_ACCESS_FROM_HEAP_C(IsExternalString);
  return data()->AsString()->is_external_string();
}

Address CallHandlerInfoRef::callback() const {
  if (data_->should_access_heap()) {
    return v8::ToCData<Address>(object()->callback());
  }
  return HeapObjectRef::data()->AsCallHandlerInfo()->callback();
}

Address FunctionTemplateInfoRef::c_function() const {
  if (data_->should_access_heap()) {
    return v8::ToCData<Address>(object()->GetCFunction());
  }
  return HeapObjectRef::data()->AsFunctionTemplateInfo()->c_function();
}

const CFunctionInfo* FunctionTemplateInfoRef::c_signature() const {
  if (data_->should_access_heap()) {
    return v8::ToCData<CFunctionInfo*>(object()->GetCSignature());
  }
  return HeapObjectRef::data()->AsFunctionTemplateInfo()->c_signature();
}

bool StringRef::IsSeqString() const {
  IF_ACCESS_FROM_HEAP_C(IsSeqString);
  return data()->AsString()->is_seq_string();
}

ScopeInfoRef NativeContextRef::scope_info() const {
  if (data_->should_access_heap()) {
    return ScopeInfoRef(
        broker(), broker()->CanonicalPersistentHandle(object()->scope_info()));
  }
  return ScopeInfoRef(broker(), data()->AsNativeContext()->scope_info());
}

SharedFunctionInfoRef FeedbackVectorRef::shared_function_info() const {
  if (data_->should_access_heap()) {
    return SharedFunctionInfoRef(
        broker(),
        broker()->CanonicalPersistentHandle(object()->shared_function_info()));
  }

  return SharedFunctionInfoRef(
      broker(), data()->AsFeedbackVector()->shared_function_info());
}

MapRef NativeContextRef::GetFunctionMapFromIndex(int index) const {
  DCHECK_GE(index, Context::FIRST_FUNCTION_MAP_INDEX);
  DCHECK_LE(index, Context::LAST_FUNCTION_MAP_INDEX);
  if (data_->should_access_heap()) {
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

bool ObjectRef::IsNull() const { return object()->IsNull(); }

bool ObjectRef::IsNullOrUndefined() const {
  if (IsSmi()) return false;
  OddballType type = AsHeapObject().map().oddball_type();
  return type == OddballType::kNull || type == OddballType::kUndefined;
}

bool ObjectRef::IsTheHole() const {
  return IsHeapObject() &&
         AsHeapObject().map().oddball_type() == OddballType::kHole;
}

bool ObjectRef::BooleanValue() const {
  if (data_->should_access_heap()) {
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

base::Optional<ObjectRef> JSObjectRef::GetOwnConstantElement(
    uint32_t index, SerializationPolicy policy) const {
  if (data_->should_access_heap() || FLAG_turbo_direct_heap_access) {
    // `elements` are currently still serialized as members of JSObjectRef.
    // TODO(jgruber,v8:7790): Once JSObject is no longer serialized, we must
    // guarantee consistency between `object`, `elements_kind` and `elements`
    // through other means (store/load order? locks? storing elements_kind in
    // elements.map?).
    STATIC_ASSERT(IsSerializedHeapObject<JSObject>());

    base::Optional<FixedArrayBaseRef> maybe_elements_ref = elements();
    if (!maybe_elements_ref.has_value()) {
      TRACE_BROKER_MISSING(broker(), "JSObject::elements" << *this);
      return {};
    }

    FixedArrayBaseRef elements_ref = maybe_elements_ref.value();
    ElementsKind elements_kind = GetElementsKind();

    DCHECK_LE(index, JSObject::kMaxElementIndex);

    // See also ElementsAccessorBase::GetMaxIndex.
    if (IsJSArray()) {
      // For JSArrays we additionally need to check against JSArray::length.
      // Length_unsafe is safe to use in this case since:
      // - GetOwnConstantElement only detects a constant for JSArray holders if
      //   the array is frozen/sealed.
      // - Frozen/sealed arrays can't change length.
      // - We've already seen a map with frozen/sealed elements_kinds (above);
      // - The release-load of that map ensures we read the newest value
      //   of `length` below.
      uint32_t array_length;
      if (!AsJSArray().length_unsafe().object()->ToArrayLength(&array_length)) {
        return {};
      }
      if (index >= array_length) return {};
    }

    Object maybe_element;
    auto result = ConcurrentLookupIterator::TryGetOwnConstantElement(
        &maybe_element, broker()->isolate(), broker()->local_isolate(),
        *object(), *elements_ref.object(), elements_kind, index);

    if (result == ConcurrentLookupIterator::kGaveUp) {
      TRACE_BROKER_MISSING(broker(), "JSObject::GetOwnConstantElement on "
                                         << *this << " at index " << index);
      return {};
    } else if (result == ConcurrentLookupIterator::kNotPresent) {
      return {};
    }

    DCHECK_EQ(result, ConcurrentLookupIterator::kPresent);
    return ObjectRef{broker(),
                     broker()->CanonicalPersistentHandle(maybe_element)};
  } else {
    ObjectData* element =
        data()->AsJSObject()->GetOwnConstantElement(broker(), index, policy);
    if (element == nullptr) return base::nullopt;
    return ObjectRef(broker(), element);
  }
}

base::Optional<ObjectRef> JSObjectRef::GetOwnFastDataProperty(
    Representation field_representation, FieldIndex index,
    SerializationPolicy policy) const {
  if (data_->should_access_heap()) {
    return GetOwnFastDataPropertyFromHeap(broker(),
                                          Handle<JSObject>::cast(object()),
                                          field_representation, index);
  }
  ObjectData* property = data()->AsJSObject()->GetOwnFastDataProperty(
      broker(), field_representation, index, policy);
  if (property == nullptr) return base::nullopt;
  return ObjectRef(broker(), property);
}

ObjectRef JSObjectRef::GetOwnDictionaryProperty(
    InternalIndex index, SerializationPolicy policy) const {
  CHECK(index.is_found());
  if (data_->should_access_heap()) {
    return GetOwnDictionaryPropertyFromHeap(
        broker(), Handle<JSObject>::cast(object()), index);
  }
  ObjectData* property =
      data()->AsJSObject()->GetOwnDictionaryProperty(broker(), index, policy);
  CHECK_NE(property, nullptr);
  return ObjectRef(broker(), property);
}

ObjectRef JSArrayRef::GetBoilerplateLength() const {
  // Safe to read concurrently because:
  // - boilerplates are immutable after initialization.
  // - boilerplates are published into the feedback vector.
  return length_unsafe();
}

ObjectRef JSArrayRef::length_unsafe() const {
  if (data_->should_access_heap() || FLAG_turbo_direct_heap_access) {
    Object o = object()->length(broker()->isolate(), kRelaxedLoad);
    return ObjectRef{broker(), broker()->CanonicalPersistentHandle(o)};
  } else {
    return ObjectRef{broker(), data()->AsJSArray()->length()};
  }
}

base::Optional<ObjectRef> JSArrayRef::GetOwnCowElement(
    FixedArrayBaseRef elements_ref, uint32_t index,
    SerializationPolicy policy) const {
  if (data_->should_access_heap() || FLAG_turbo_direct_heap_access) {
    // `elements` are currently still serialized as members of JSObjectRef.
    // TODO(jgruber,v8:7790): Remove the elements equality DCHECK below once
    // JSObject is no longer serialized.
    static_assert(std::is_base_of<JSObject, JSArray>::value, "");
    STATIC_ASSERT(IsSerializedHeapObject<JSObject>());

    // The elements_ref is passed in by callers to make explicit that it is
    // also used outside of this function, and must match the `elements` used
    // inside this function.
    DCHECK(elements_ref.equals(elements().value()));

    // Due to concurrency, the kind read here may not be consistent with
    // `elements_ref`. But consistency is guaranteed at runtime due to the
    // `elements` equality check in the caller.
    ElementsKind elements_kind = GetElementsKind();

    // We only inspect fixed COW arrays, which may only occur for fast
    // smi/objects elements kinds.
    if (!IsSmiOrObjectElementsKind(elements_kind)) return {};
    DCHECK(IsFastElementsKind(elements_kind));
    if (!elements_ref.map().IsFixedCowArrayMap()) return {};

    // As the name says, the `length` read here is unsafe and may not match
    // `elements`. We rely on the invariant that any `length` change will
    // also result in an `elements` change to make this safe. The `elements`
    // equality check in the caller thus also guards the value of `length`.
    ObjectRef length_ref = length_unsafe();

    // Likewise we only deal with smi lengths.
    if (!length_ref.IsSmi()) return {};

    base::Optional<Object> result =
        ConcurrentLookupIterator::TryGetOwnCowElement(
            broker()->isolate(), *elements_ref.AsFixedArray().object(),
            elements_kind, length_ref.AsSmi(), index);

    if (!result.has_value()) return {};

    return ObjectRef{broker(),
                     broker()->CanonicalPersistentHandle(result.value())};
  } else {
    DCHECK(!data_->should_access_heap());
    DCHECK(!FLAG_turbo_direct_heap_access);

    // Just to clarify that `elements_ref` is not used on this path.
    // GetOwnElement accesses the serialized `elements` field on its own.
    USE(elements_ref);

    if (!elements().value().map().IsFixedCowArrayMap()) return base::nullopt;

    ObjectData* element =
        data()->AsJSArray()->GetOwnElement(broker(), index, policy);
    if (element == nullptr) return base::nullopt;
    return ObjectRef(broker(), element);
  }
}

base::Optional<CellRef> SourceTextModuleRef::GetCell(int cell_index) const {
  if (data_->should_access_heap() || FLAG_turbo_direct_heap_access) {
    return CellRef(broker(), broker()->CanonicalPersistentHandle(
                                 object()->GetCell(cell_index)));
  }
  ObjectData* cell =
      data()->AsSourceTextModule()->GetCell(broker(), cell_index);
  if (cell == nullptr) return base::nullopt;
  return CellRef(broker(), cell);
}

ObjectRef SourceTextModuleRef::import_meta() const {
  if (data_->should_access_heap()) {
    return ObjectRef(
        broker(), broker()->CanonicalPersistentHandle(object()->import_meta()));
  }
  return ObjectRef(broker(),
                   data()->AsSourceTextModule()->GetImportMeta(broker()));
}

ObjectRef::ObjectRef(JSHeapBroker* broker, Handle<Object> object,
                     BackgroundSerialization background_serialization,
                     bool check_type)
    : broker_(broker) {
  CHECK_NE(broker->mode(), JSHeapBroker::kRetired);

  data_ = broker->GetOrCreateData(object, background_serialization);
  if (!data_) {  // TODO(mslekova): Remove once we're on the background thread.
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
  if (data_->should_access_heap()) {
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
  if (data_->should_access_heap()) {
    return JSObjectRef(
        broker(), broker()->CanonicalPersistentHandle(object()->boilerplate()));
  }
  ObjectData* boilerplate = data()->AsAllocationSite()->boilerplate();
  if (boilerplate) {
    return JSObjectRef(broker(), boilerplate);
  } else {
    return base::nullopt;
  }
}

ElementsKind JSObjectRef::GetElementsKind() const {
  return map().elements_kind();
}

base::Optional<FixedArrayBaseRef> JSObjectRef::elements() const {
  if (data_->should_access_heap()) {
    return FixedArrayBaseRef(
        broker(), broker()->CanonicalPersistentHandle(object()->elements()));
  }
  const JSObjectData* d = data()->AsJSObject();
  if (!d->serialized_elements()) {
    TRACE(broker(), "'elements' on " << this);
    return base::nullopt;
  }
  return FixedArrayBaseRef(broker(), d->elements());
}

int FixedArrayBaseRef::length() const {
  IF_ACCESS_FROM_HEAP_C(length);
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

PropertyDetails DescriptorArrayRef::GetPropertyDetails(
    InternalIndex descriptor_index) const {
  if (data_->should_access_heap()) {
    return object()->GetDetails(descriptor_index);
  }
  return data()->AsDescriptorArray()->GetPropertyDetails(descriptor_index);
}

NameRef DescriptorArrayRef::GetPropertyKey(
    InternalIndex descriptor_index) const {
  if (data_->should_access_heap()) {
    NameRef result(broker(), broker()->CanonicalPersistentHandle(
                                 object()->GetKey(descriptor_index)));
    CHECK(result.IsUniqueName());
    return result;
  }
  return NameRef(broker(),
                 data()->AsDescriptorArray()->GetPropertyKey(descriptor_index));
}

ObjectRef DescriptorArrayRef::GetFieldType(
    InternalIndex descriptor_index) const {
  if (data_->should_access_heap()) {
    // This method only gets called for the creation of FieldTypeDependencies.
    // These calls happen when the broker is either disabled or serializing,
    // which means that GetOrCreateData would be able to successfully create the
    // ObjectRef for the cases where we haven't seen the FieldType before.
    DCHECK(broker()->mode() == JSHeapBroker::kDisabled ||
           broker()->mode() == JSHeapBroker::kSerializing);
    return ObjectRef(broker(), broker()->CanonicalPersistentHandle(
                                   object()->GetFieldType(descriptor_index)));
  }
  return ObjectRef(broker(),
                   data()->AsDescriptorArray()->GetFieldType(descriptor_index));
}

base::Optional<ObjectRef> DescriptorArrayRef::GetStrongValue(
    InternalIndex descriptor_index) const {
  if (data_->should_access_heap()) {
    HeapObject heap_object;
    if (object()
            ->GetValue(descriptor_index)
            .GetHeapObjectIfStrong(&heap_object)) {
      // Since the descriptors in the descriptor array can be changed in-place
      // via DescriptorArray::Replace, we might get a value that we haven't seen
      // before.
      ObjectData* data = broker()->TryGetOrCreateData(
          broker()->CanonicalPersistentHandle(heap_object));
      if (data) return ObjectRef(broker(), data);

      TRACE_BROKER_MISSING(broker(), "strong value for descriptor array "
                                         << *this << " at index "
                                         << descriptor_index.as_int());
      // Fall through to the base::nullopt below.
    }
    return base::nullopt;
  }
  ObjectData* value =
      data()->AsDescriptorArray()->GetStrongValue(descriptor_index);
  if (!value) return base::nullopt;
  return ObjectRef(broker(), value);
}

base::Optional<SharedFunctionInfoRef> FeedbackCellRef::shared_function_info()
    const {
  if (value()) {
    FeedbackVectorRef vector = *value();
    if (vector.serialized()) {
      return vector.shared_function_info();
    }
  }
  return base::nullopt;
}

void FeedbackVectorRef::Serialize() {
  if (data_->should_access_heap()) return;
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsFeedbackVector()->Serialize(broker());
}

bool FeedbackVectorRef::serialized() const {
  if (data_->should_access_heap()) return true;
  return data()->AsFeedbackVector()->serialized();
}

bool NameRef::IsUniqueName() const {
  // Must match Name::IsUniqueName.
  return IsInternalizedString() || IsSymbol();
}

void RegExpBoilerplateDescriptionRef::Serialize() {
  if (data_->should_access_heap()) {
    // Even if the regexp boilerplate object itself is no longer serialized,
    // the `data` still is and thus we need to make sure to visit it.
    // TODO(jgruber,v8:7790): Remove once it is no longer a serialized type.
    STATIC_ASSERT(IsSerializedHeapObject<FixedArray>());
    FixedArrayRef data_ref{
        broker(), broker()->CanonicalPersistentHandle(object()->data())};
  } else {
    CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
    HeapObjectRef::data()->AsRegExpBoilerplateDescription()->Serialize(
        broker());
  }
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

HEAP_BROKER_SERIALIZED_OBJECT_LIST(DEF_OBJECT_GETTER)
HEAP_BROKER_POSSIBLY_BACKGROUND_SERIALIZED_OBJECT_LIST(DEF_OBJECT_GETTER)
HEAP_BROKER_BACKGROUND_SERIALIZED_OBJECT_LIST(DEF_OBJECT_GETTER)
HEAP_BROKER_NEVER_SERIALIZED_OBJECT_LIST(DEF_OBJECT_GETTER)
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
    : ContextData(broker, storage, object),
      state_(State::kUnserialized),
      function_maps_(broker->zone()) {}

void NativeContextData::Serialize(JSHeapBroker* broker) {
  if (state_ != State::kUnserialized) return;
  state_ = State::kSerializedOnMainThread;

  TraceScope tracer(broker, this, "NativeContextData::Serialize");
  Handle<NativeContext> context = Handle<NativeContext>::cast(object());

#define SERIALIZE_MEMBER(type, name)                                          \
  DCHECK_NULL(name##_);                                                       \
  name##_ = broker->GetOrCreateData(context->name());                         \
  if (!name##_->should_access_heap()) {                                       \
    if (name##_->IsMap() &&                                                   \
        !InstanceTypeChecker::IsContext(name##_->AsMap()->instance_type())) { \
      name##_->AsMap()->SerializeConstructor(broker);                         \
    }                                                                         \
    if (name##_->IsJSFunction()) {                                            \
      name##_->AsJSFunction()->Serialize(broker);                             \
    }                                                                         \
  }
  BROKER_COMPULSORY_NATIVE_CONTEXT_FIELDS(SERIALIZE_MEMBER)
  if (!broker->is_isolate_bootstrapping()) {
    BROKER_OPTIONAL_NATIVE_CONTEXT_FIELDS(SERIALIZE_MEMBER)
  }
#undef SERIALIZE_MEMBER

  if (!bound_function_with_constructor_map_->should_access_heap()) {
    bound_function_with_constructor_map_->AsMap()->SerializePrototype(broker);
  }
  if (!bound_function_without_constructor_map_->should_access_heap()) {
    bound_function_without_constructor_map_->AsMap()->SerializePrototype(
        broker);
  }

  scope_info_ = broker->GetOrCreateData(context->scope_info());
}

void NativeContextData::SerializeOnBackground(JSHeapBroker* broker) {
  if (state_ == State::kFullySerialized) return;
  DCHECK_EQ(state_, State::kSerializedOnMainThread);
  state_ = State::kSerializedOnMainThread;

  UnparkedScopeIfNeeded unparked_scope(broker);
  TraceScope tracer(broker, this, "NativeContextData::SerializeOnBackground");
  Handle<NativeContext> context = Handle<NativeContext>::cast(object());

  constexpr auto kAllowed = ObjectRef::BackgroundSerialization::kAllowed;
#define SERIALIZE_MEMBER(type, name)                            \
  DCHECK_NULL(name##_);                                         \
  name##_ = broker->GetOrCreateData(context->name(), kAllowed); \
  if (!name##_->should_access_heap()) {                         \
    DCHECK(!name##_->IsJSFunction());                           \
  }
  BROKER_COMPULSORY_BACKGROUND_NATIVE_CONTEXT_FIELDS(SERIALIZE_MEMBER)
  if (!broker->is_isolate_bootstrapping()) {
    BROKER_OPTIONAL_BACKGROUND_NATIVE_CONTEXT_FIELDS(SERIALIZE_MEMBER)
  }
#undef SERIALIZE_MEMBER

  DCHECK(function_maps_.empty());
  int const first = Context::FIRST_FUNCTION_MAP_INDEX;
  int const last = Context::LAST_FUNCTION_MAP_INDEX;
  function_maps_.reserve(last + 1 - first);
  for (int i = first; i <= last; ++i) {
    function_maps_.push_back(
        broker->GetOrCreateData(context->get(i), kAllowed));
  }
}

void JSFunctionRef::Serialize() {
  if (data_->should_access_heap()) return;
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsJSFunction()->Serialize(broker());
}

void JSFunctionRef::SerializeCodeAndFeedback() {
  if (data_->should_access_heap()) return;
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsJSFunction()->SerializeCodeAndFeedback(broker());
}

bool JSBoundFunctionRef::serialized() const {
  if (data_->should_access_heap()) return true;
  return data()->AsJSBoundFunction()->serialized();
}

bool JSFunctionRef::serialized() const {
  if (data_->should_access_heap()) return true;
  return data()->AsJSFunction()->serialized();
}

bool JSFunctionRef::serialized_code_and_feedback() const {
  if (data_->should_access_heap()) return true;
  return data()->AsJSFunction()->serialized_code_and_feedback();
}

CodeRef JSFunctionRef::code() const {
  if (data_->should_access_heap() || FLAG_turbo_direct_heap_access) {
    return CodeRef(broker(), broker()->CanonicalPersistentHandle(
                                 object()->code(kAcquireLoad)));
  }

  return CodeRef(broker(), ObjectRef::data()->AsJSFunction()->code());
}

void SharedFunctionInfoRef::SerializeFunctionTemplateInfo() {
  if (data_->should_access_heap()) return;
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsSharedFunctionInfo()->SerializeFunctionTemplateInfo(broker());
}

void SharedFunctionInfoRef::SerializeScopeInfoChain() {
  if (data_->should_access_heap()) return;
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsSharedFunctionInfo()->SerializeScopeInfoChain(broker());
}

base::Optional<FunctionTemplateInfoRef>
SharedFunctionInfoRef::function_template_info() const {
  if (data_->should_access_heap()) {
    if (object()->IsApiFunction()) {
      ObjectData* data =
          broker()->TryGetOrCreateData(broker()->CanonicalPersistentHandle(
              object()->function_data(kAcquireLoad)));
      if (data == nullptr) return base::nullopt;
      return FunctionTemplateInfoRef(broker(), data, true);
    }
    return base::nullopt;
  }
  ObjectData* function_template_info =
      data()->AsSharedFunctionInfo()->function_template_info();
  if (!function_template_info) return base::nullopt;
  return FunctionTemplateInfoRef(broker(), function_template_info);
}

int SharedFunctionInfoRef::context_header_size() const {
  IF_ACCESS_FROM_HEAP_C(scope_info().ContextHeaderLength);
  return data()->AsSharedFunctionInfo()->context_header_size();
}

ScopeInfoRef SharedFunctionInfoRef::scope_info() const {
  if (data_->should_access_heap()) {
    return ScopeInfoRef(
        broker(), broker()->CanonicalPersistentHandle(object()->scope_info()));
  }
  return ScopeInfoRef(broker(), data()->AsSharedFunctionInfo()->scope_info());
}

void JSObjectRef::SerializeObjectCreateMap() {
  if (data_->should_access_heap()) return;
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsJSObject()->SerializeObjectCreateMap(broker());
}

void MapRef::SerializeOwnDescriptor(InternalIndex descriptor_index) {
  CHECK_LT(descriptor_index.as_int(), NumberOfOwnDescriptors());
  if (data_->should_access_heap()) return;
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsMap()->SerializeOwnDescriptor(broker(), descriptor_index);
}

bool MapRef::serialized_own_descriptor(InternalIndex descriptor_index) const {
  CHECK_LT(descriptor_index.as_int(), NumberOfOwnDescriptors());
  if (data_->should_access_heap()) return true;
  ObjectData* maybe_desc_array_data = data()->AsMap()->instance_descriptors();
  if (!maybe_desc_array_data) return false;
  if (maybe_desc_array_data->should_access_heap()) return true;
  DescriptorArrayData* desc_array_data =
      maybe_desc_array_data->AsDescriptorArray();
  return desc_array_data->serialized_descriptor(descriptor_index);
}

void MapRef::SerializeBackPointer() {
  if (data_->should_access_heap()) return;
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsMap()->SerializeBackPointer(broker());
}

void MapRef::SerializePrototype() {
  if (data_->should_access_heap()) return;
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsMap()->SerializePrototype(broker());
}

bool MapRef::serialized_prototype() const {
  if (data_->should_access_heap()) return true;
  CHECK_NE(broker()->mode(), JSHeapBroker::kDisabled);
  return data()->AsMap()->serialized_prototype();
}

void SourceTextModuleRef::Serialize() {
  if (data_->should_access_heap()) return;
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsSourceTextModule()->Serialize(broker());
}

void NativeContextRef::Serialize() {
  if (data_->should_access_heap()) return;
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsNativeContext()->Serialize(broker());
}

void NativeContextRef::SerializeOnBackground() {
  if (data_->should_access_heap()) return;
  CHECK(broker()->mode() == JSHeapBroker::kSerializing ||
        broker()->mode() == JSHeapBroker::kSerialized);
  data()->AsNativeContext()->SerializeOnBackground(broker());
}

void JSTypedArrayRef::Serialize() {
  if (data_->should_access_heap() || FLAG_turbo_direct_heap_access) {
    // Even if the typed array object itself is no longer serialized (besides
    // the JSObject parts), the `buffer` field still is and thus we need to
    // make sure to visit it.
    // TODO(jgruber,v8:7790): Remove once JSObject is no longer serialized.
    static_assert(
        std::is_base_of<JSObject, decltype(object()->buffer())>::value, "");
    STATIC_ASSERT(IsSerializedHeapObject<JSObject>());
    JSObjectRef data_ref{
        broker(), broker()->CanonicalPersistentHandle(object()->buffer())};
  } else {
    CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
    data()->AsJSTypedArray()->Serialize(broker());
  }
}

bool JSTypedArrayRef::serialized() const {
  CHECK_NE(broker()->mode(), JSHeapBroker::kDisabled);
  return data()->AsJSTypedArray()->serialized();
}

bool JSTypedArrayRef::ShouldHaveBeenSerialized() const {
  if (FLAG_turbo_direct_heap_access) return false;
  return ObjectRef::ShouldHaveBeenSerialized();
}

bool JSBoundFunctionRef::Serialize() {
  if (data_->should_access_heap()) return true;
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  return data()->AsJSBoundFunction()->Serialize(broker());
}

bool PropertyCellRef::Serialize() const {
  if (data_->should_access_heap()) return true;
  CHECK(broker()->mode() == JSHeapBroker::kSerializing ||
        broker()->mode() == JSHeapBroker::kSerialized);
  return data()->AsPropertyCell()->Serialize(broker());
}

void FunctionTemplateInfoRef::SerializeCallCode() {
  if (data_->should_access_heap()) {
    CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
    // CallHandlerInfo::data may still hold a serialized heap object, so we
    // have to make the broker aware of it.
    // TODO(v8:7790): Remove this case once ObjectRef is never serialized.
    Handle<HeapObject> call_code(object()->call_code(kAcquireLoad),
                                 broker()->isolate());
    if (call_code->IsCallHandlerInfo()) {
      broker()->GetOrCreateData(
          Handle<CallHandlerInfo>::cast(call_code)->data());
    }
    return;
  }
  CHECK_EQ(broker()->mode(), JSHeapBroker::kSerializing);
  data()->AsFunctionTemplateInfo()->SerializeCallCode(broker());
}

base::Optional<PropertyCellRef> JSGlobalObjectRef::GetPropertyCell(
    NameRef const& name, SerializationPolicy policy) const {
  if (data_->should_access_heap()) {
    return GetPropertyCellFromHeap(broker(), name.object());
  }
  ObjectData* property_cell_data = data()->AsJSGlobalObject()->GetPropertyCell(
      broker(), name.data(), policy);
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
      *zone->New<ElementAccessFeedback>(zone, keyed_mode(), slot_kind());
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
    bool cell_serialized = property_cell().Serialize();
    CHECK(cell_serialized);  // Can't fail on the main thread.
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

MinimorphicLoadPropertyAccessFeedback::MinimorphicLoadPropertyAccessFeedback(
    NameRef const& name, FeedbackSlotKind slot_kind, Handle<Object> handler,
    ZoneVector<Handle<Map>> const& maps, bool has_migration_target_maps)
    : ProcessedFeedback(kMinimorphicPropertyAccess, slot_kind),
      name_(name),
      handler_(handler),
      maps_(maps),
      has_migration_target_maps_(has_migration_target_maps) {
  DCHECK(IsLoadICKind(slot_kind));
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
  if (is_concurrent_inlining_) {
    ProcessedFeedback const& processed = GetFeedback(source);
    return processed.slot_kind();
  }
  FeedbackNexus nexus(source.vector, source.slot, feedback_nexus_config());
  return nexus.kind();
}

bool JSHeapBroker::FeedbackIsInsufficient(FeedbackSource const& source) const {
  return is_concurrent_inlining_ ? GetFeedback(source).IsInsufficient()
                                 : FeedbackNexus(source.vector, source.slot,
                                                 feedback_nexus_config())
                                       .IsUninitialized();
}

namespace {
// Remove unupdatable and abandoned prototype maps in-place.
void FilterRelevantReceiverMaps(Isolate* isolate, MapHandles* maps) {
  auto in = maps->begin();
  auto out = in;
  auto end = maps->end();

  for (; in != end; ++in) {
    Handle<Map> map = *in;
    if (Map::TryUpdate(isolate, map).ToHandle(&map) &&
        !map->is_abandoned_prototype_map()) {
      DCHECK(!map->is_deprecated());
      *out = *in;
      ++out;
    }
  }

  // Remove everything between the last valid map and the end of the vector.
  maps->erase(out, end);
}

MaybeObjectHandle TryGetMinimorphicHandler(
    std::vector<MapAndHandler> const& maps_and_handlers, FeedbackSlotKind kind,
    Handle<NativeContext> native_context, bool is_turboprop) {
  if (!is_turboprop || !FLAG_turbo_dynamic_map_checks || !IsLoadICKind(kind)) {
    return MaybeObjectHandle();
  }

  // Don't use dynamic map checks when loading properties from Array.prototype.
  // Using dynamic map checks prevents constant folding and hence does not
  // inline the array builtins. We only care about monomorphic cases here. For
  // polymorphic loads currently we don't inline the builtins even without
  // dynamic map checks.
  if (maps_and_handlers.size() == 1 &&
      *maps_and_handlers[0].first ==
          native_context->initial_array_prototype().map()) {
    return MaybeObjectHandle();
  }

  MaybeObjectHandle initial_handler;
  for (MapAndHandler map_and_handler : maps_and_handlers) {
    auto map = map_and_handler.first;
    MaybeObjectHandle handler = map_and_handler.second;
    if (handler.is_null()) return MaybeObjectHandle();
    DCHECK(!handler->IsCleared());
    // TODO(mythria): extend this to DataHandlers too
    if (!handler.object()->IsSmi()) return MaybeObjectHandle();
    if (LoadHandler::GetHandlerKind(handler.object()->ToSmi()) !=
        LoadHandler::Kind::kField) {
      return MaybeObjectHandle();
    }
    CHECK(!map->IsJSGlobalProxyMap());
    if (initial_handler.is_null()) {
      initial_handler = handler;
    } else if (!handler.is_identical_to(initial_handler)) {
      return MaybeObjectHandle();
    }
  }
  return initial_handler;
}

bool HasMigrationTargets(const MapHandles& maps) {
  for (Handle<Map> map : maps) {
    if (map->is_migration_target()) return true;
  }
  return false;
}
}  // namespace

bool JSHeapBroker::CanUseFeedback(const FeedbackNexus& nexus) const {
  // TODO(jgruber,v8:8888): Currently, nci code does not use any
  // feedback. This restriction will be relaxed in the future.
  return !is_native_context_independent() && !nexus.IsUninitialized();
}

const ProcessedFeedback& JSHeapBroker::NewInsufficientFeedback(
    FeedbackSlotKind kind) const {
  return *zone()->New<InsufficientFeedback>(kind);
}

ProcessedFeedback const& JSHeapBroker::ReadFeedbackForPropertyAccess(
    FeedbackSource const& source, AccessMode mode,
    base::Optional<NameRef> static_name) {
  FeedbackNexus nexus(source.vector, source.slot, feedback_nexus_config());
  FeedbackSlotKind kind = nexus.kind();
  if (!CanUseFeedback(nexus)) return NewInsufficientFeedback(kind);

  std::vector<MapAndHandler> maps_and_handlers;
  nexus.ExtractMapsAndFeedback(&maps_and_handlers);
  MapHandles maps;
  for (auto const& entry : maps_and_handlers) {
    maps.push_back(entry.first);
  }

  base::Optional<NameRef> name =
      static_name.has_value() ? static_name : GetNameFeedback(nexus);
  MaybeObjectHandle handler = TryGetMinimorphicHandler(
      maps_and_handlers, kind, target_native_context().object(),
      is_turboprop());
  if (!handler.is_null()) {
    return *zone()->New<MinimorphicLoadPropertyAccessFeedback>(
        *name, kind, handler.object(),
        ZoneVector<Handle<Map>>(maps.begin(), maps.end(), zone()),
        HasMigrationTargets(maps));
  }

  FilterRelevantReceiverMaps(isolate(), &maps);

  // If no maps were found for a non-megamorphic access, then our maps died
  // and we should soft-deopt.
  if (maps.empty() && nexus.ic_state() != MEGAMORPHIC) {
    return NewInsufficientFeedback(kind);
  }

  if (name.has_value()) {
    // We rely on this invariant in JSGenericLowering.
    DCHECK_IMPLIES(maps.empty(), nexus.ic_state() == MEGAMORPHIC);
    return *zone()->New<NamedAccessFeedback>(
        *name, ZoneVector<Handle<Map>>(maps.begin(), maps.end(), zone()), kind);
  } else if (nexus.GetKeyType() == ELEMENT && !maps.empty()) {
    return ProcessFeedbackMapsForElementAccess(
        maps, KeyedAccessMode::FromNexus(nexus), kind);
  } else {
    // No actionable feedback.
    DCHECK(maps.empty());
    DCHECK_EQ(nexus.ic_state(), MEGAMORPHIC);
    // TODO(neis): Using ElementAccessFeedback here is kind of an abuse.
    return *zone()->New<ElementAccessFeedback>(
        zone(), KeyedAccessMode::FromNexus(nexus), kind);
  }
}

ProcessedFeedback const& JSHeapBroker::ReadFeedbackForGlobalAccess(
    FeedbackSource const& source) {
  FeedbackNexus nexus(source.vector, source.slot);
  DCHECK(nexus.kind() == FeedbackSlotKind::kLoadGlobalInsideTypeof ||
         nexus.kind() == FeedbackSlotKind::kLoadGlobalNotInsideTypeof ||
         nexus.kind() == FeedbackSlotKind::kStoreGlobalSloppy ||
         nexus.kind() == FeedbackSlotKind::kStoreGlobalStrict);
  if (!CanUseFeedback(nexus)) return NewInsufficientFeedback(nexus.kind());
  if (nexus.ic_state() != MONOMORPHIC || nexus.GetFeedback()->IsCleared()) {
    return *zone()->New<GlobalAccessFeedback>(nexus.kind());
  }

  Handle<Object> feedback_value(nexus.GetFeedback()->GetHeapObjectOrSmi(),
                                isolate());

  if (feedback_value->IsSmi()) {
    // The wanted name belongs to a script-scope variable and the feedback
    // tells us where to find its value.
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
    return *zone()->New<GlobalAccessFeedback>(context_ref, context_slot_index,
                                              immutable, nexus.kind());
  }

  CHECK(feedback_value->IsPropertyCell());
  // The wanted name belongs (or did belong) to a property on the global
  // object and the feedback is the cell holding its value.
  PropertyCellRef cell(this, Handle<PropertyCell>::cast(feedback_value));
  ObjectRef(
      this,
      CanonicalPersistentHandle(
          Handle<PropertyCell>::cast(feedback_value)->value(kAcquireLoad)));
  return *zone()->New<GlobalAccessFeedback>(cell, nexus.kind());
}

ProcessedFeedback const& JSHeapBroker::ReadFeedbackForBinaryOperation(
    FeedbackSource const& source) const {
  FeedbackNexus nexus(source.vector, source.slot, feedback_nexus_config());
  if (!CanUseFeedback(nexus)) return NewInsufficientFeedback(nexus.kind());
  BinaryOperationHint hint = nexus.GetBinaryOperationFeedback();
  DCHECK_NE(hint, BinaryOperationHint::kNone);  // Not uninitialized.
  return *zone()->New<BinaryOperationFeedback>(hint, nexus.kind());
}

ProcessedFeedback const& JSHeapBroker::ReadFeedbackForCompareOperation(
    FeedbackSource const& source) const {
  FeedbackNexus nexus(source.vector, source.slot, feedback_nexus_config());
  if (!CanUseFeedback(nexus)) return NewInsufficientFeedback(nexus.kind());
  CompareOperationHint hint = nexus.GetCompareOperationFeedback();
  DCHECK_NE(hint, CompareOperationHint::kNone);  // Not uninitialized.
  return *zone()->New<CompareOperationFeedback>(hint, nexus.kind());
}

ProcessedFeedback const& JSHeapBroker::ReadFeedbackForForIn(
    FeedbackSource const& source) const {
  FeedbackNexus nexus(source.vector, source.slot, feedback_nexus_config());
  if (!CanUseFeedback(nexus)) return NewInsufficientFeedback(nexus.kind());
  ForInHint hint = nexus.GetForInFeedback();
  DCHECK_NE(hint, ForInHint::kNone);  // Not uninitialized.
  return *zone()->New<ForInFeedback>(hint, nexus.kind());
}

ProcessedFeedback const& JSHeapBroker::ReadFeedbackForInstanceOf(
    FeedbackSource const& source) {
  FeedbackNexus nexus(source.vector, source.slot, feedback_nexus_config());
  if (!CanUseFeedback(nexus)) return NewInsufficientFeedback(nexus.kind());

  base::Optional<JSObjectRef> optional_constructor;
  {
    MaybeHandle<JSObject> maybe_constructor = nexus.GetConstructorFeedback();
    Handle<JSObject> constructor;
    if (maybe_constructor.ToHandle(&constructor)) {
      optional_constructor = JSObjectRef(this, constructor);
    }
  }
  return *zone()->New<InstanceOfFeedback>(optional_constructor, nexus.kind());
}

ProcessedFeedback const& JSHeapBroker::ReadFeedbackForArrayOrObjectLiteral(
    FeedbackSource const& source) {
  FeedbackNexus nexus(source.vector, source.slot, feedback_nexus_config());
  if (!CanUseFeedback(nexus)) return NewInsufficientFeedback(nexus.kind());

  HeapObject object;
  if (!nexus.GetFeedback()->GetHeapObject(&object)) {
    return NewInsufficientFeedback(nexus.kind());
  }

  AllocationSiteRef site(this, handle(object, isolate()));
  if (site.IsFastLiteral()) {
    site.SerializeBoilerplate();
  }

  return *zone()->New<LiteralFeedback>(site, nexus.kind());
}

ProcessedFeedback const& JSHeapBroker::ReadFeedbackForRegExpLiteral(
    FeedbackSource const& source) {
  FeedbackNexus nexus(source.vector, source.slot, feedback_nexus_config());
  if (!CanUseFeedback(nexus)) return NewInsufficientFeedback(nexus.kind());

  HeapObject object;
  if (!nexus.GetFeedback()->GetHeapObject(&object)) {
    return NewInsufficientFeedback(nexus.kind());
  }

  RegExpBoilerplateDescriptionRef boilerplate(this, handle(object, isolate()));
  boilerplate.Serialize();
  return *zone()->New<RegExpLiteralFeedback>(boilerplate, nexus.kind());
}

ProcessedFeedback const& JSHeapBroker::ReadFeedbackForTemplateObject(
    FeedbackSource const& source) {
  FeedbackNexus nexus(source.vector, source.slot, feedback_nexus_config());
  if (!CanUseFeedback(nexus)) return NewInsufficientFeedback(nexus.kind());

  HeapObject object;
  if (!nexus.GetFeedback()->GetHeapObject(&object)) {
    return NewInsufficientFeedback(nexus.kind());
  }

  JSArrayRef array(this, handle(object, isolate()));
  return *zone()->New<TemplateObjectFeedback>(array, nexus.kind());
}

ProcessedFeedback const& JSHeapBroker::ReadFeedbackForCall(
    FeedbackSource const& source) {
  FeedbackNexus nexus(source.vector, source.slot, feedback_nexus_config());
  if (!CanUseFeedback(nexus)) return NewInsufficientFeedback(nexus.kind());

  base::Optional<HeapObjectRef> target_ref;
  {
    // TODO(mvstanton): this read has a special danger when done on the
    // background thread, because the CallIC has a site in generated code
    // where a JSFunction is installed in this slot without store ordering.
    // Therefore, we will need to check {maybe_target} to ensure that it
    // has been store ordered by the heap's mechanism for store-ordering
    // batches of new objects.
    MaybeObject maybe_target = nexus.GetFeedback();
    HeapObject target_object;
    if (maybe_target->GetHeapObject(&target_object)) {
      target_ref = HeapObjectRef(this, handle(target_object, isolate()));
    }
  }
  float frequency = nexus.ComputeCallFrequency();
  SpeculationMode mode = nexus.GetSpeculationMode();
  return *zone()->New<CallFeedback>(target_ref, frequency, mode, nexus.kind());
}

BinaryOperationHint JSHeapBroker::GetFeedbackForBinaryOperation(
    FeedbackSource const& source) {
  ProcessedFeedback const& feedback =
      is_concurrent_inlining_ ? GetFeedback(source)
                              : ProcessFeedbackForBinaryOperation(source);
  return feedback.IsInsufficient() ? BinaryOperationHint::kNone
                                   : feedback.AsBinaryOperation().value();
}

CompareOperationHint JSHeapBroker::GetFeedbackForCompareOperation(
    FeedbackSource const& source) {
  ProcessedFeedback const& feedback =
      is_concurrent_inlining_ ? GetFeedback(source)
                              : ProcessFeedbackForCompareOperation(source);
  return feedback.IsInsufficient() ? CompareOperationHint::kNone
                                   : feedback.AsCompareOperation().value();
}

ForInHint JSHeapBroker::GetFeedbackForForIn(FeedbackSource const& source) {
  ProcessedFeedback const& feedback = is_concurrent_inlining_
                                          ? GetFeedback(source)
                                          : ProcessFeedbackForForIn(source);
  return feedback.IsInsufficient() ? ForInHint::kNone
                                   : feedback.AsForIn().value();
}

ProcessedFeedback const& JSHeapBroker::GetFeedbackForPropertyAccess(
    FeedbackSource const& source, AccessMode mode,
    base::Optional<NameRef> static_name) {
  return is_concurrent_inlining_
             ? GetFeedback(source)
             : ProcessFeedbackForPropertyAccess(source, mode, static_name);
}

ProcessedFeedback const& JSHeapBroker::GetFeedbackForInstanceOf(
    FeedbackSource const& source) {
  return is_concurrent_inlining_ ? GetFeedback(source)
                                 : ProcessFeedbackForInstanceOf(source);
}

ProcessedFeedback const& JSHeapBroker::GetFeedbackForCall(
    FeedbackSource const& source) {
  return is_concurrent_inlining_ ? GetFeedback(source)
                                 : ProcessFeedbackForCall(source);
}

ProcessedFeedback const& JSHeapBroker::GetFeedbackForGlobalAccess(
    FeedbackSource const& source) {
  return is_concurrent_inlining_ ? GetFeedback(source)
                                 : ProcessFeedbackForGlobalAccess(source);
}

ProcessedFeedback const& JSHeapBroker::GetFeedbackForArrayOrObjectLiteral(
    FeedbackSource const& source) {
  return is_concurrent_inlining_
             ? GetFeedback(source)
             : ProcessFeedbackForArrayOrObjectLiteral(source);
}

ProcessedFeedback const& JSHeapBroker::GetFeedbackForRegExpLiteral(
    FeedbackSource const& source) {
  return is_concurrent_inlining_ ? GetFeedback(source)
                                 : ProcessFeedbackForRegExpLiteral(source);
}

ProcessedFeedback const& JSHeapBroker::GetFeedbackForTemplateObject(
    FeedbackSource const& source) {
  return is_concurrent_inlining_ ? GetFeedback(source)
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
  ProcessedFeedback const& feedback = ReadFeedbackForBinaryOperation(source);
  SetFeedback(source, &feedback);
  return feedback;
}

ProcessedFeedback const& JSHeapBroker::ProcessFeedbackForCompareOperation(
    FeedbackSource const& source) {
  if (HasFeedback(source)) return GetFeedback(source);
  ProcessedFeedback const& feedback = ReadFeedbackForCompareOperation(source);
  SetFeedback(source, &feedback);
  return feedback;
}

ProcessedFeedback const& JSHeapBroker::ProcessFeedbackForForIn(
    FeedbackSource const& source) {
  if (HasFeedback(source)) return GetFeedback(source);
  ProcessedFeedback const& feedback = ReadFeedbackForForIn(source);
  SetFeedback(source, &feedback);
  return feedback;
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
      zone()->New<ElementAccessFeedback>(zone(), keyed_mode, slot_kind);
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
  if (!FLAG_concurrent_recompilation) {
    // We cannot be in a background thread so it's safe to read the heap.
    AllowHandleDereference allow_handle_dereference;
    return os << ref.data() << " {" << ref.object() << "}";
  } else if (ref.data_->should_access_heap()) {
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
  if (is_concurrent_inlining_) {
    CHECK_EQ(mode(), kSerializing);
    TRACE(this, "Storing PropertyAccessInfo for "
                    << access_mode << " of property " << name << " on map "
                    << map);
    property_access_infos_.insert({target, access_info});
  }
  return access_info;
}

MinimorphicLoadPropertyAccessInfo JSHeapBroker::GetPropertyAccessInfo(
    MinimorphicLoadPropertyAccessFeedback const& feedback,
    FeedbackSource const& source, SerializationPolicy policy) {
  auto it = minimorphic_property_access_infos_.find(source);
  if (it != minimorphic_property_access_infos_.end()) return it->second;

  if (policy == SerializationPolicy::kAssumeSerialized) {
    TRACE_BROKER_MISSING(this, "MinimorphicLoadPropertyAccessInfo for slot "
                                   << source.index() << "  "
                                   << ObjectRef(this, source.vector));
    return MinimorphicLoadPropertyAccessInfo::Invalid();
  }

  AccessInfoFactory factory(this, nullptr, zone());
  MinimorphicLoadPropertyAccessInfo access_info =
      factory.ComputePropertyAccessInfo(feedback);
  if (is_concurrent_inlining_) {
    TRACE(this, "Storing MinimorphicLoadPropertyAccessInfo for "
                    << source.index() << "  "
                    << ObjectRef(this, source.vector));
    minimorphic_property_access_infos_.insert({source, access_info});
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

MinimorphicLoadPropertyAccessFeedback const&
ProcessedFeedback::AsMinimorphicPropertyAccess() const {
  CHECK_EQ(kMinimorphicPropertyAccess, kind());
  return *static_cast<MinimorphicLoadPropertyAccessFeedback const*>(this);
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

unsigned CodeRef::GetInlinedBytecodeSize() const {
  if (data_->should_access_heap()) {
    unsigned value = object()->inlined_bytecode_size();
    if (value > 0) {
      // Don't report inlined bytecode size if the code object was already
      // deoptimized.
      value = object()->marked_for_deoptimization() ? 0 : value;
    }
    return value;
  }

  return ObjectRef::data()->AsCode()->inlined_bytecode_size();
}

#undef BIMODAL_ACCESSOR
#undef BIMODAL_ACCESSOR_B
#undef BIMODAL_ACCESSOR_C
#undef IF_ACCESS_FROM_HEAP
#undef IF_ACCESS_FROM_HEAP_C
#undef TRACE
#undef TRACE_MISSING

}  // namespace compiler
}  // namespace internal
}  // namespace v8
