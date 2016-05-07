// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api-natives.h"

#include "src/api.h"
#include "src/isolate-inl.h"
#include "src/lookup.h"
#include "src/messages.h"

namespace v8 {
namespace internal {


namespace {

MaybeHandle<JSObject> InstantiateObject(Isolate* isolate,
                                        Handle<ObjectTemplateInfo> data,
                                        bool is_hidden_prototype);

MaybeHandle<JSFunction> InstantiateFunction(Isolate* isolate,
                                            Handle<FunctionTemplateInfo> data,
                                            Handle<Name> name = Handle<Name>());


MaybeHandle<Object> Instantiate(Isolate* isolate, Handle<Object> data,
                                Handle<Name> name = Handle<Name>()) {
  if (data->IsFunctionTemplateInfo()) {
    return InstantiateFunction(isolate,
                               Handle<FunctionTemplateInfo>::cast(data), name);
  } else if (data->IsObjectTemplateInfo()) {
    return InstantiateObject(isolate, Handle<ObjectTemplateInfo>::cast(data),
                             false);
  } else {
    return data;
  }
}

MaybeHandle<Object> DefineAccessorProperty(
    Isolate* isolate, Handle<JSObject> object, Handle<Name> name,
    Handle<Object> getter, Handle<Object> setter, PropertyAttributes attributes,
    bool force_instantiate) {
  DCHECK(!getter->IsFunctionTemplateInfo() ||
         !FunctionTemplateInfo::cast(*getter)->do_not_cache());
  DCHECK(!setter->IsFunctionTemplateInfo() ||
         !FunctionTemplateInfo::cast(*setter)->do_not_cache());
  if (force_instantiate) {
    if (getter->IsFunctionTemplateInfo()) {
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, getter,
          InstantiateFunction(isolate,
                              Handle<FunctionTemplateInfo>::cast(getter)),
          Object);
    }
    if (setter->IsFunctionTemplateInfo()) {
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, setter,
          InstantiateFunction(isolate,
                              Handle<FunctionTemplateInfo>::cast(setter)),
          Object);
    }
  }
  RETURN_ON_EXCEPTION(isolate, JSObject::DefineAccessor(object, name, getter,
                                                        setter, attributes),
                      Object);
  return object;
}


MaybeHandle<Object> DefineDataProperty(Isolate* isolate,
                                       Handle<JSObject> object,
                                       Handle<Name> name,
                                       Handle<Object> prop_data,
                                       PropertyAttributes attributes) {
  Handle<Object> value;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, value,
                             Instantiate(isolate, prop_data, name), Object);

  LookupIterator it = LookupIterator::PropertyOrElement(
      isolate, object, name, LookupIterator::OWN_SKIP_INTERCEPTOR);

#ifdef DEBUG
  Maybe<PropertyAttributes> maybe = JSReceiver::GetPropertyAttributes(&it);
  DCHECK(maybe.IsJust());
  if (it.IsFound()) {
    THROW_NEW_ERROR(
        isolate,
        NewTypeError(MessageTemplate::kDuplicateTemplateProperty, name),
        Object);
  }
#endif

  MAYBE_RETURN_NULL(
      Object::AddDataProperty(&it, value, attributes, Object::THROW_ON_ERROR,
                              Object::CERTAINLY_NOT_STORE_FROM_KEYED));
  return value;
}


void DisableAccessChecks(Isolate* isolate, Handle<JSObject> object) {
  Handle<Map> old_map(object->map());
  // Copy map so it won't interfere constructor's initial map.
  Handle<Map> new_map = Map::Copy(old_map, "DisableAccessChecks");
  new_map->set_is_access_check_needed(false);
  JSObject::MigrateToMap(Handle<JSObject>::cast(object), new_map);
}


void EnableAccessChecks(Isolate* isolate, Handle<JSObject> object) {
  Handle<Map> old_map(object->map());
  // Copy map so it won't interfere constructor's initial map.
  Handle<Map> new_map = Map::Copy(old_map, "EnableAccessChecks");
  new_map->set_is_access_check_needed(true);
  JSObject::MigrateToMap(object, new_map);
}


class AccessCheckDisableScope {
 public:
  AccessCheckDisableScope(Isolate* isolate, Handle<JSObject> obj)
      : isolate_(isolate),
        disabled_(obj->map()->is_access_check_needed()),
        obj_(obj) {
    if (disabled_) {
      DisableAccessChecks(isolate_, obj_);
    }
  }
  ~AccessCheckDisableScope() {
    if (disabled_) {
      EnableAccessChecks(isolate_, obj_);
    }
  }

 private:
  Isolate* isolate_;
  const bool disabled_;
  Handle<JSObject> obj_;
};


Object* GetIntrinsic(Isolate* isolate, v8::Intrinsic intrinsic) {
  Handle<Context> native_context = isolate->native_context();
  DCHECK(!native_context.is_null());
  switch (intrinsic) {
#define GET_INTRINSIC_VALUE(name, iname) \
  case v8::k##name:                      \
    return native_context->iname();
    V8_INTRINSICS_LIST(GET_INTRINSIC_VALUE)
#undef GET_INTRINSIC_VALUE
  }
  return nullptr;
}

// Returns parent function template or null.
FunctionTemplateInfo* GetParent(FunctionTemplateInfo* data) {
  Object* parent = data->parent_template();
  return parent->IsUndefined() ? nullptr : FunctionTemplateInfo::cast(parent);
}

// Starting from given object template's constructor walk up the inheritance
// chain till a function template that has an instance template is found.
ObjectTemplateInfo* GetParent(ObjectTemplateInfo* data) {
  Object* maybe_ctor = data->constructor();
  if (maybe_ctor->IsUndefined()) return nullptr;
  FunctionTemplateInfo* ctor = FunctionTemplateInfo::cast(maybe_ctor);
  while (true) {
    ctor = GetParent(ctor);
    if (ctor == nullptr) return nullptr;
    Object* maybe_obj = ctor->instance_template();
    if (!maybe_obj->IsUndefined()) return ObjectTemplateInfo::cast(maybe_obj);
  }
}

template <typename TemplateInfoT>
MaybeHandle<JSObject> ConfigureInstance(Isolate* isolate, Handle<JSObject> obj,
                                        Handle<TemplateInfoT> data,
                                        bool is_hidden_prototype) {
  HandleScope scope(isolate);
  // Disable access checks while instantiating the object.
  AccessCheckDisableScope access_check_scope(isolate, obj);

  // Walk the inheritance chain and copy all accessors to current object.
  int max_number_of_properties = 0;
  TemplateInfoT* info = *data;
  while (info != nullptr) {
    if (!info->property_accessors()->IsUndefined()) {
      Object* props = info->property_accessors();
      if (!props->IsUndefined()) {
        Handle<Object> props_handle(props, isolate);
        NeanderArray props_array(props_handle);
        max_number_of_properties += props_array.length();
      }
    }
    info = GetParent(info);
  }

  if (max_number_of_properties > 0) {
    int valid_descriptors = 0;
    // Use a temporary FixedArray to accumulate unique accessors.
    Handle<FixedArray> array =
        isolate->factory()->NewFixedArray(max_number_of_properties);

    info = *data;
    while (info != nullptr) {
      // Accumulate accessors.
      if (!info->property_accessors()->IsUndefined()) {
        Handle<Object> props(info->property_accessors(), isolate);
        valid_descriptors =
            AccessorInfo::AppendUnique(props, array, valid_descriptors);
      }
      info = GetParent(info);
    }

    // Install accumulated accessors.
    for (int i = 0; i < valid_descriptors; i++) {
      Handle<AccessorInfo> accessor(AccessorInfo::cast(array->get(i)));
      JSObject::SetAccessor(obj, accessor).Assert();
    }
  }

  auto property_list = handle(data->property_list(), isolate);
  if (property_list->IsUndefined()) return obj;
  // TODO(dcarney): just use a FixedArray here.
  NeanderArray properties(property_list);
  if (properties.length() == 0) return obj;

  int i = 0;
  for (int c = 0; c < data->number_of_properties(); c++) {
    auto name = handle(Name::cast(properties.get(i++)), isolate);
    auto bit = handle(properties.get(i++), isolate);
    if (bit->IsSmi()) {
      PropertyDetails details(Smi::cast(*bit));
      PropertyAttributes attributes = details.attributes();
      PropertyKind kind = details.kind();

      if (kind == kData) {
        auto prop_data = handle(properties.get(i++), isolate);
        RETURN_ON_EXCEPTION(isolate, DefineDataProperty(isolate, obj, name,
                                                        prop_data, attributes),
                            JSObject);
      } else {
        auto getter = handle(properties.get(i++), isolate);
        auto setter = handle(properties.get(i++), isolate);
        RETURN_ON_EXCEPTION(
            isolate, DefineAccessorProperty(isolate, obj, name, getter, setter,
                                            attributes, is_hidden_prototype),
            JSObject);
      }
    } else {
      // Intrinsic data property --- Get appropriate value from the current
      // context.
      PropertyDetails details(Smi::cast(properties.get(i++)));
      PropertyAttributes attributes = details.attributes();
      DCHECK_EQ(kData, details.kind());

      v8::Intrinsic intrinsic =
          static_cast<v8::Intrinsic>(Smi::cast(properties.get(i++))->value());
      auto prop_data = handle(GetIntrinsic(isolate, intrinsic), isolate);

      RETURN_ON_EXCEPTION(isolate, DefineDataProperty(isolate, obj, name,
                                                      prop_data, attributes),
                          JSObject);
    }
  }
  return obj;
}

void CacheTemplateInstantiation(Isolate* isolate, Handle<Smi> serial_number,
                                Handle<JSObject> object) {
  auto cache = isolate->template_instantiations_cache();
  auto new_cache = ObjectHashTable::Put(cache, serial_number, object);
  isolate->native_context()->set_template_instantiations_cache(*new_cache);
}

void UncacheTemplateInstantiation(Isolate* isolate, Handle<Smi> serial_number) {
  auto cache = isolate->template_instantiations_cache();
  bool was_present = false;
  auto new_cache = ObjectHashTable::Remove(cache, serial_number, &was_present);
  DCHECK(was_present);
  isolate->native_context()->set_template_instantiations_cache(*new_cache);
}

MaybeHandle<JSObject> InstantiateObject(Isolate* isolate,
                                        Handle<ObjectTemplateInfo> info,
                                        bool is_hidden_prototype) {
  // Enter a new scope.  Recursion could otherwise create a lot of handles.
  HandleScope scope(isolate);
  // Fast path.
  Handle<JSObject> result;
  auto constructor = handle(info->constructor(), isolate);
  Handle<JSFunction> cons;
  if (constructor->IsUndefined()) {
    cons = isolate->object_function();
  } else {
    auto cons_templ = Handle<FunctionTemplateInfo>::cast(constructor);
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, cons, InstantiateFunction(isolate, cons_templ), JSFunction);
  }
  auto serial_number = handle(Smi::cast(info->serial_number()), isolate);
  if (serial_number->value()) {
    // Probe cache.
    auto cache = isolate->template_instantiations_cache();
    Object* boilerplate = cache->Lookup(serial_number);
    if (boilerplate->IsJSObject()) {
      result = handle(JSObject::cast(boilerplate), isolate);
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, result, JSObject::DeepCopyApiBoilerplate(result), JSObject);
      return scope.CloseAndEscape(result);
    }
  }
  auto object = isolate->factory()->NewJSObject(cons);
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result,
      ConfigureInstance(isolate, object, info, is_hidden_prototype),
      JSFunction);
  // TODO(dcarney): is this necessary?
  JSObject::MigrateSlowToFast(result, 0, "ApiNatives::InstantiateObject");

  if (serial_number->value()) {
    CacheTemplateInstantiation(isolate, serial_number, result);
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, result, JSObject::DeepCopyApiBoilerplate(result), JSObject);
  }
  return scope.CloseAndEscape(result);
}


MaybeHandle<JSFunction> InstantiateFunction(Isolate* isolate,
                                            Handle<FunctionTemplateInfo> data,
                                            Handle<Name> name) {
  auto serial_number = handle(Smi::cast(data->serial_number()), isolate);
  if (serial_number->value()) {
    // Probe cache.
    auto cache = isolate->template_instantiations_cache();
    Object* element = cache->Lookup(serial_number);
    if (element->IsJSFunction()) {
      return handle(JSFunction::cast(element), isolate);
    }
  }
  // Enter a new scope.  Recursion could otherwise create a lot of handles.
  HandleScope scope(isolate);
  Handle<JSObject> prototype;
  if (!data->remove_prototype()) {
    auto prototype_templ = handle(data->prototype_template(), isolate);
    if (prototype_templ->IsUndefined()) {
      prototype = isolate->factory()->NewJSObject(isolate->object_function());
    } else {
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, prototype,
          InstantiateObject(isolate,
                            Handle<ObjectTemplateInfo>::cast(prototype_templ),
                            data->hidden_prototype()),
          JSFunction);
    }
    auto parent = handle(data->parent_template(), isolate);
    if (!parent->IsUndefined()) {
      Handle<JSFunction> parent_instance;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, parent_instance,
          InstantiateFunction(isolate,
                              Handle<FunctionTemplateInfo>::cast(parent)),
          JSFunction);
      // TODO(dcarney): decide what to do here.
      Handle<Object> parent_prototype;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, parent_prototype,
          JSObject::GetProperty(parent_instance,
                                isolate->factory()->prototype_string()),
          JSFunction);
      MAYBE_RETURN(JSObject::SetPrototype(prototype, parent_prototype, false,
                                          Object::THROW_ON_ERROR),
                   MaybeHandle<JSFunction>());
    }
  }
  auto function = ApiNatives::CreateApiFunction(
      isolate, data, prototype, ApiNatives::JavaScriptObjectType);
  if (!name.is_null() && name->IsString()) {
    function->shared()->set_name(*name);
  }
  if (serial_number->value()) {
    // Cache the function.
    CacheTemplateInstantiation(isolate, serial_number, function);
  }
  auto result =
      ConfigureInstance(isolate, function, data, data->hidden_prototype());
  if (result.is_null()) {
    // Uncache on error.
    if (serial_number->value()) {
      UncacheTemplateInstantiation(isolate, serial_number);
    }
    return MaybeHandle<JSFunction>();
  }
  return scope.CloseAndEscape(function);
}


class InvokeScope {
 public:
  explicit InvokeScope(Isolate* isolate)
      : isolate_(isolate), save_context_(isolate) {}
  ~InvokeScope() {
    bool has_exception = isolate_->has_pending_exception();
    if (has_exception) {
      isolate_->ReportPendingMessages();
    } else {
      isolate_->clear_pending_message();
    }
  }

 private:
  Isolate* isolate_;
  SaveContext save_context_;
};


void AddPropertyToPropertyList(Isolate* isolate, Handle<TemplateInfo> templ,
                               int length, Handle<Object>* data) {
  auto list = handle(templ->property_list(), isolate);
  if (list->IsUndefined()) {
    list = NeanderArray(isolate).value();
    templ->set_property_list(*list);
  }
  templ->set_number_of_properties(templ->number_of_properties() + 1);
  NeanderArray array(list);
  for (int i = 0; i < length; i++) {
    Handle<Object> value =
        data[i].is_null()
            ? Handle<Object>::cast(isolate->factory()->undefined_value())
            : data[i];
    array.add(isolate, value);
  }
}

}  // namespace


MaybeHandle<JSFunction> ApiNatives::InstantiateFunction(
    Handle<FunctionTemplateInfo> data) {
  Isolate* isolate = data->GetIsolate();
  InvokeScope invoke_scope(isolate);
  return ::v8::internal::InstantiateFunction(isolate, data);
}


MaybeHandle<JSObject> ApiNatives::InstantiateObject(
    Handle<ObjectTemplateInfo> data) {
  Isolate* isolate = data->GetIsolate();
  InvokeScope invoke_scope(isolate);
  return ::v8::internal::InstantiateObject(isolate, data, false);
}


void ApiNatives::AddDataProperty(Isolate* isolate, Handle<TemplateInfo> info,
                                 Handle<Name> name, Handle<Object> value,
                                 PropertyAttributes attributes) {
  const int kSize = 3;
  PropertyDetails details(attributes, DATA, 0, PropertyCellType::kNoCell);
  auto details_handle = handle(details.AsSmi(), isolate);
  Handle<Object> data[kSize] = {name, details_handle, value};
  AddPropertyToPropertyList(isolate, info, kSize, data);
}


void ApiNatives::AddDataProperty(Isolate* isolate, Handle<TemplateInfo> info,
                                 Handle<Name> name, v8::Intrinsic intrinsic,
                                 PropertyAttributes attributes) {
  const int kSize = 4;
  auto value = handle(Smi::FromInt(intrinsic), isolate);
  auto intrinsic_marker = isolate->factory()->true_value();
  PropertyDetails details(attributes, DATA, 0, PropertyCellType::kNoCell);
  auto details_handle = handle(details.AsSmi(), isolate);
  Handle<Object> data[kSize] = {name, intrinsic_marker, details_handle, value};
  AddPropertyToPropertyList(isolate, info, kSize, data);
}


void ApiNatives::AddAccessorProperty(Isolate* isolate,
                                     Handle<TemplateInfo> info,
                                     Handle<Name> name,
                                     Handle<FunctionTemplateInfo> getter,
                                     Handle<FunctionTemplateInfo> setter,
                                     PropertyAttributes attributes) {
  const int kSize = 4;
  PropertyDetails details(attributes, ACCESSOR, 0, PropertyCellType::kNoCell);
  auto details_handle = handle(details.AsSmi(), isolate);
  Handle<Object> data[kSize] = {name, details_handle, getter, setter};
  AddPropertyToPropertyList(isolate, info, kSize, data);
}


void ApiNatives::AddNativeDataProperty(Isolate* isolate,
                                       Handle<TemplateInfo> info,
                                       Handle<AccessorInfo> property) {
  auto list = handle(info->property_accessors(), isolate);
  if (list->IsUndefined()) {
    list = NeanderArray(isolate).value();
    info->set_property_accessors(*list);
  }
  NeanderArray array(list);
  array.add(isolate, property);
}


Handle<JSFunction> ApiNatives::CreateApiFunction(
    Isolate* isolate, Handle<FunctionTemplateInfo> obj,
    Handle<Object> prototype, ApiInstanceType instance_type) {
  Handle<Code> code;
  if (obj->call_code()->IsCallHandlerInfo() &&
      CallHandlerInfo::cast(obj->call_code())->fast_handler()->IsCode()) {
    code = isolate->builtins()->HandleFastApiCall();
  } else {
    code = isolate->builtins()->HandleApiCall();
  }
  Handle<Code> construct_stub =
      prototype.is_null() ? isolate->builtins()->ConstructedNonConstructable()
                          : isolate->builtins()->JSConstructStubApi();

  obj->set_instantiated(true);
  Handle<JSFunction> result;
  if (obj->remove_prototype()) {
    result = isolate->factory()->NewFunctionWithoutPrototype(
        isolate->factory()->empty_string(), code);
  } else {
    int internal_field_count = 0;
    if (!obj->instance_template()->IsUndefined()) {
      Handle<ObjectTemplateInfo> instance_template = Handle<ObjectTemplateInfo>(
          ObjectTemplateInfo::cast(obj->instance_template()));
      internal_field_count =
          Smi::cast(instance_template->internal_field_count())->value();
    }

    // TODO(svenpanne) Kill ApiInstanceType and refactor things by generalizing
    // JSObject::GetHeaderSize.
    int instance_size = kPointerSize * internal_field_count;
    InstanceType type;
    switch (instance_type) {
      case JavaScriptObjectType:
        type = JS_OBJECT_TYPE;
        instance_size += JSObject::kHeaderSize;
        break;
      case GlobalObjectType:
        type = JS_GLOBAL_OBJECT_TYPE;
        instance_size += JSGlobalObject::kSize;
        break;
      case GlobalProxyType:
        type = JS_GLOBAL_PROXY_TYPE;
        instance_size += JSGlobalProxy::kSize;
        break;
      default:
        UNREACHABLE();
        type = JS_OBJECT_TYPE;  // Keep the compiler happy.
        break;
    }

    result = isolate->factory()->NewFunction(
        isolate->factory()->empty_string(), code, prototype, type,
        instance_size, obj->read_only_prototype(), true);
  }

  result->shared()->set_length(obj->length());
  Handle<Object> class_name(obj->class_name(), isolate);
  if (class_name->IsString()) {
    result->shared()->set_instance_class_name(*class_name);
    result->shared()->set_name(*class_name);
  }
  result->shared()->set_function_data(*obj);
  result->shared()->set_construct_stub(*construct_stub);
  result->shared()->DontAdaptArguments();

  if (obj->remove_prototype()) {
    DCHECK(result->shared()->IsApiFunction());
    DCHECK(!result->has_initial_map());
    DCHECK(!result->has_prototype());
    return result;
  }

#ifdef DEBUG
  LookupIterator it(handle(JSObject::cast(result->prototype())),
                    isolate->factory()->constructor_string(),
                    LookupIterator::OWN_SKIP_INTERCEPTOR);
  MaybeHandle<Object> maybe_prop = Object::GetProperty(&it);
  DCHECK(it.IsFound());
  DCHECK(maybe_prop.ToHandleChecked().is_identical_to(result));
#endif

  // Down from here is only valid for API functions that can be used as a
  // constructor (don't set the "remove prototype" flag).

  Handle<Map> map(result->initial_map());

  // Mark as undetectable if needed.
  if (obj->undetectable()) {
    map->set_is_undetectable();
  }

  // Mark as needs_access_check if needed.
  if (obj->needs_access_check()) {
    map->set_is_access_check_needed(true);
  }

  // Set interceptor information in the map.
  if (!obj->named_property_handler()->IsUndefined()) {
    map->set_has_named_interceptor();
  }
  if (!obj->indexed_property_handler()->IsUndefined()) {
    map->set_has_indexed_interceptor();
  }

  // Mark instance as callable in the map.
  if (!obj->instance_call_handler()->IsUndefined()) {
    map->set_is_callable();
    map->set_is_constructor(true);
  }

  DCHECK(result->shared()->IsApiFunction());
  return result;
}

}  // namespace internal
}  // namespace v8
