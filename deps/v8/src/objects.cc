// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/accessors.h"
#include "src/allocation-site-scopes.h"
#include "src/api.h"
#include "src/arguments.h"
#include "src/bootstrapper.h"
#include "src/code-stubs.h"
#include "src/codegen.h"
#include "src/cpu-profiler.h"
#include "src/date.h"
#include "src/debug.h"
#include "src/deoptimizer.h"
#include "src/elements.h"
#include "src/execution.h"
#include "src/field-index-inl.h"
#include "src/field-index.h"
#include "src/full-codegen.h"
#include "src/heap/mark-compact.h"
#include "src/heap/objects-visiting-inl.h"
#include "src/hydrogen.h"
#include "src/isolate-inl.h"
#include "src/log.h"
#include "src/lookup.h"
#include "src/macro-assembler.h"
#include "src/objects-inl.h"
#include "src/prototype.h"
#include "src/safepoint-table.h"
#include "src/string-search.h"
#include "src/string-stream.h"
#include "src/utils.h"

#ifdef ENABLE_DISASSEMBLER
#include "src/disasm.h"
#include "src/disassembler.h"
#endif

namespace v8 {
namespace internal {

Handle<HeapType> Object::OptimalType(Isolate* isolate,
                                     Representation representation) {
  if (representation.IsNone()) return HeapType::None(isolate);
  if (FLAG_track_field_types) {
    if (representation.IsHeapObject() && IsHeapObject()) {
      // We can track only JavaScript objects with stable maps.
      Handle<Map> map(HeapObject::cast(this)->map(), isolate);
      if (map->is_stable() &&
          map->instance_type() >= FIRST_NONCALLABLE_SPEC_OBJECT_TYPE &&
          map->instance_type() <= LAST_NONCALLABLE_SPEC_OBJECT_TYPE) {
        return HeapType::Class(map, isolate);
      }
    }
  }
  return HeapType::Any(isolate);
}


MaybeHandle<JSReceiver> Object::ToObject(Isolate* isolate,
                                         Handle<Object> object,
                                         Handle<Context> native_context) {
  if (object->IsJSReceiver()) return Handle<JSReceiver>::cast(object);
  Handle<JSFunction> constructor;
  if (object->IsNumber()) {
    constructor = handle(native_context->number_function(), isolate);
  } else if (object->IsBoolean()) {
    constructor = handle(native_context->boolean_function(), isolate);
  } else if (object->IsString()) {
    constructor = handle(native_context->string_function(), isolate);
  } else if (object->IsSymbol()) {
    constructor = handle(native_context->symbol_function(), isolate);
  } else {
    return MaybeHandle<JSReceiver>();
  }
  Handle<JSObject> result = isolate->factory()->NewJSObject(constructor);
  Handle<JSValue>::cast(result)->set_value(*object);
  return result;
}


bool Object::BooleanValue() {
  if (IsBoolean()) return IsTrue();
  if (IsSmi()) return Smi::cast(this)->value() != 0;
  if (IsUndefined() || IsNull()) return false;
  if (IsUndetectableObject()) return false;   // Undetectable object is false.
  if (IsString()) return String::cast(this)->length() != 0;
  if (IsHeapNumber()) return HeapNumber::cast(this)->HeapNumberBooleanValue();
  return true;
}


bool Object::IsCallable() const {
  const Object* fun = this;
  while (fun->IsJSFunctionProxy()) {
    fun = JSFunctionProxy::cast(fun)->call_trap();
  }
  return fun->IsJSFunction() ||
         (fun->IsHeapObject() &&
          HeapObject::cast(fun)->map()->has_instance_call_handler());
}


void Object::Lookup(Handle<Name> name, LookupResult* result) {
  DisallowHeapAllocation no_gc;
  Object* holder = NULL;
  if (IsJSReceiver()) {
    holder = this;
  } else {
    Context* native_context = result->isolate()->context()->native_context();
    if (IsNumber()) {
      holder = native_context->number_function()->instance_prototype();
    } else if (IsString()) {
      holder = native_context->string_function()->instance_prototype();
    } else if (IsSymbol()) {
      holder = native_context->symbol_function()->instance_prototype();
    } else if (IsBoolean()) {
      holder = native_context->boolean_function()->instance_prototype();
    } else {
      result->isolate()->PushStackTraceAndDie(
          0xDEAD0000, this, JSReceiver::cast(this)->map(), 0xDEAD0001);
    }
  }
  DCHECK(holder != NULL);  // Cannot handle null or undefined.
  JSReceiver::cast(holder)->Lookup(name, result);
}


MaybeHandle<Object> Object::GetProperty(LookupIterator* it) {
  for (; it->IsFound(); it->Next()) {
    switch (it->state()) {
      case LookupIterator::NOT_FOUND:
        UNREACHABLE();
      case LookupIterator::JSPROXY:
        return JSProxy::GetPropertyWithHandler(it->GetHolder<JSProxy>(),
                                               it->GetReceiver(), it->name());
      case LookupIterator::INTERCEPTOR: {
        MaybeHandle<Object> maybe_result = JSObject::GetPropertyWithInterceptor(
            it->GetHolder<JSObject>(), it->GetReceiver(), it->name());
        if (!maybe_result.is_null()) return maybe_result;
        if (it->isolate()->has_pending_exception()) return maybe_result;
        break;
      }
      case LookupIterator::ACCESS_CHECK:
        if (it->HasAccess(v8::ACCESS_GET)) break;
        return JSObject::GetPropertyWithFailedAccessCheck(it);
      case LookupIterator::PROPERTY:
        if (it->HasProperty()) {
          switch (it->property_kind()) {
            case LookupIterator::ACCESSOR:
              return GetPropertyWithAccessor(it->GetReceiver(), it->name(),
                                             it->GetHolder<JSObject>(),
                                             it->GetAccessors());
            case LookupIterator::DATA:
              return it->GetDataValue();
          }
        }
        break;
    }
  }
  return it->factory()->undefined_value();
}


bool Object::ToInt32(int32_t* value) {
  if (IsSmi()) {
    *value = Smi::cast(this)->value();
    return true;
  }
  if (IsHeapNumber()) {
    double num = HeapNumber::cast(this)->value();
    if (FastI2D(FastD2I(num)) == num) {
      *value = FastD2I(num);
      return true;
    }
  }
  return false;
}


bool Object::ToUint32(uint32_t* value) {
  if (IsSmi()) {
    int num = Smi::cast(this)->value();
    if (num >= 0) {
      *value = static_cast<uint32_t>(num);
      return true;
    }
  }
  if (IsHeapNumber()) {
    double num = HeapNumber::cast(this)->value();
    if (num >= 0 && FastUI2D(FastD2UI(num)) == num) {
      *value = FastD2UI(num);
      return true;
    }
  }
  return false;
}


bool FunctionTemplateInfo::IsTemplateFor(Object* object) {
  if (!object->IsHeapObject()) return false;
  return IsTemplateFor(HeapObject::cast(object)->map());
}


bool FunctionTemplateInfo::IsTemplateFor(Map* map) {
  // There is a constraint on the object; check.
  if (!map->IsJSObjectMap()) return false;
  // Fetch the constructor function of the object.
  Object* cons_obj = map->constructor();
  if (!cons_obj->IsJSFunction()) return false;
  JSFunction* fun = JSFunction::cast(cons_obj);
  // Iterate through the chain of inheriting function templates to
  // see if the required one occurs.
  for (Object* type = fun->shared()->function_data();
       type->IsFunctionTemplateInfo();
       type = FunctionTemplateInfo::cast(type)->parent_template()) {
    if (type == this) return true;
  }
  // Didn't find the required type in the inheritance chain.
  return false;
}


template<typename To>
static inline To* CheckedCast(void *from) {
  uintptr_t temp = reinterpret_cast<uintptr_t>(from);
  DCHECK(temp % sizeof(To) == 0);
  return reinterpret_cast<To*>(temp);
}


static Handle<Object> PerformCompare(const BitmaskCompareDescriptor& descriptor,
                                     char* ptr,
                                     Isolate* isolate) {
  uint32_t bitmask = descriptor.bitmask;
  uint32_t compare_value = descriptor.compare_value;
  uint32_t value;
  switch (descriptor.size) {
    case 1:
      value = static_cast<uint32_t>(*CheckedCast<uint8_t>(ptr));
      compare_value &= 0xff;
      bitmask &= 0xff;
      break;
    case 2:
      value = static_cast<uint32_t>(*CheckedCast<uint16_t>(ptr));
      compare_value &= 0xffff;
      bitmask &= 0xffff;
      break;
    case 4:
      value = *CheckedCast<uint32_t>(ptr);
      break;
    default:
      UNREACHABLE();
      return isolate->factory()->undefined_value();
  }
  return isolate->factory()->ToBoolean(
      (bitmask & value) == (bitmask & compare_value));
}


static Handle<Object> PerformCompare(const PointerCompareDescriptor& descriptor,
                                     char* ptr,
                                     Isolate* isolate) {
  uintptr_t compare_value =
      reinterpret_cast<uintptr_t>(descriptor.compare_value);
  uintptr_t value = *CheckedCast<uintptr_t>(ptr);
  return isolate->factory()->ToBoolean(compare_value == value);
}


static Handle<Object> GetPrimitiveValue(
    const PrimitiveValueDescriptor& descriptor,
    char* ptr,
    Isolate* isolate) {
  int32_t int32_value = 0;
  switch (descriptor.data_type) {
    case kDescriptorInt8Type:
      int32_value = *CheckedCast<int8_t>(ptr);
      break;
    case kDescriptorUint8Type:
      int32_value = *CheckedCast<uint8_t>(ptr);
      break;
    case kDescriptorInt16Type:
      int32_value = *CheckedCast<int16_t>(ptr);
      break;
    case kDescriptorUint16Type:
      int32_value = *CheckedCast<uint16_t>(ptr);
      break;
    case kDescriptorInt32Type:
      int32_value = *CheckedCast<int32_t>(ptr);
      break;
    case kDescriptorUint32Type: {
      uint32_t value = *CheckedCast<uint32_t>(ptr);
      AllowHeapAllocation allow_gc;
      return isolate->factory()->NewNumberFromUint(value);
    }
    case kDescriptorBoolType: {
      uint8_t byte = *CheckedCast<uint8_t>(ptr);
      return isolate->factory()->ToBoolean(
          byte & (0x1 << descriptor.bool_offset));
    }
    case kDescriptorFloatType: {
      float value = *CheckedCast<float>(ptr);
      AllowHeapAllocation allow_gc;
      return isolate->factory()->NewNumber(value);
    }
    case kDescriptorDoubleType: {
      double value = *CheckedCast<double>(ptr);
      AllowHeapAllocation allow_gc;
      return isolate->factory()->NewNumber(value);
    }
  }
  AllowHeapAllocation allow_gc;
  return isolate->factory()->NewNumberFromInt(int32_value);
}


static Handle<Object> GetDeclaredAccessorProperty(
    Handle<Object> receiver,
    Handle<DeclaredAccessorInfo> info,
    Isolate* isolate) {
  DisallowHeapAllocation no_gc;
  char* current = reinterpret_cast<char*>(*receiver);
  DeclaredAccessorDescriptorIterator iterator(info->descriptor());
  while (true) {
    const DeclaredAccessorDescriptorData* data = iterator.Next();
    switch (data->type) {
      case kDescriptorReturnObject: {
        DCHECK(iterator.Complete());
        current = *CheckedCast<char*>(current);
        return handle(*CheckedCast<Object*>(current), isolate);
      }
      case kDescriptorPointerDereference:
        DCHECK(!iterator.Complete());
        current = *reinterpret_cast<char**>(current);
        break;
      case kDescriptorPointerShift:
        DCHECK(!iterator.Complete());
        current += data->pointer_shift_descriptor.byte_offset;
        break;
      case kDescriptorObjectDereference: {
        DCHECK(!iterator.Complete());
        Object* object = CheckedCast<Object>(current);
        int field = data->object_dereference_descriptor.internal_field;
        Object* smi = JSObject::cast(object)->GetInternalField(field);
        DCHECK(smi->IsSmi());
        current = reinterpret_cast<char*>(smi);
        break;
      }
      case kDescriptorBitmaskCompare:
        DCHECK(iterator.Complete());
        return PerformCompare(data->bitmask_compare_descriptor,
                              current,
                              isolate);
      case kDescriptorPointerCompare:
        DCHECK(iterator.Complete());
        return PerformCompare(data->pointer_compare_descriptor,
                              current,
                              isolate);
      case kDescriptorPrimitiveValue:
        DCHECK(iterator.Complete());
        return GetPrimitiveValue(data->primitive_value_descriptor,
                                 current,
                                 isolate);
    }
  }
  UNREACHABLE();
  return isolate->factory()->undefined_value();
}


Handle<FixedArray> JSObject::EnsureWritableFastElements(
    Handle<JSObject> object) {
  DCHECK(object->HasFastSmiOrObjectElements());
  Isolate* isolate = object->GetIsolate();
  Handle<FixedArray> elems(FixedArray::cast(object->elements()), isolate);
  if (elems->map() != isolate->heap()->fixed_cow_array_map()) return elems;
  Handle<FixedArray> writable_elems = isolate->factory()->CopyFixedArrayWithMap(
      elems, isolate->factory()->fixed_array_map());
  object->set_elements(*writable_elems);
  isolate->counters()->cow_arrays_converted()->Increment();
  return writable_elems;
}


MaybeHandle<Object> JSProxy::GetPropertyWithHandler(Handle<JSProxy> proxy,
                                                    Handle<Object> receiver,
                                                    Handle<Name> name) {
  Isolate* isolate = proxy->GetIsolate();

  // TODO(rossberg): adjust once there is a story for symbols vs proxies.
  if (name->IsSymbol()) return isolate->factory()->undefined_value();

  Handle<Object> args[] = { receiver, name };
  return CallTrap(
      proxy, "get",  isolate->derived_get_trap(), ARRAY_SIZE(args), args);
}


MaybeHandle<Object> Object::GetPropertyWithAccessor(Handle<Object> receiver,
                                                    Handle<Name> name,
                                                    Handle<JSObject> holder,
                                                    Handle<Object> structure) {
  Isolate* isolate = name->GetIsolate();
  DCHECK(!structure->IsForeign());
  // api style callbacks.
  if (structure->IsAccessorInfo()) {
    Handle<AccessorInfo> info = Handle<AccessorInfo>::cast(structure);
    if (!info->IsCompatibleReceiver(*receiver)) {
      Handle<Object> args[2] = { name, receiver };
      Handle<Object> error =
          isolate->factory()->NewTypeError("incompatible_method_receiver",
                                           HandleVector(args,
                                                        ARRAY_SIZE(args)));
      return isolate->Throw<Object>(error);
    }
    // TODO(rossberg): Handling symbols in the API requires changing the API,
    // so we do not support it for now.
    if (name->IsSymbol()) return isolate->factory()->undefined_value();
    if (structure->IsDeclaredAccessorInfo()) {
      return GetDeclaredAccessorProperty(
          receiver,
          Handle<DeclaredAccessorInfo>::cast(structure),
          isolate);
    }

    Handle<ExecutableAccessorInfo> data =
        Handle<ExecutableAccessorInfo>::cast(structure);
    v8::AccessorGetterCallback call_fun =
        v8::ToCData<v8::AccessorGetterCallback>(data->getter());
    if (call_fun == NULL) return isolate->factory()->undefined_value();

    Handle<String> key = Handle<String>::cast(name);
    LOG(isolate, ApiNamedPropertyAccess("load", *holder, *name));
    PropertyCallbackArguments args(isolate, data->data(), *receiver, *holder);
    v8::Handle<v8::Value> result =
        args.Call(call_fun, v8::Utils::ToLocal(key));
    RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
    if (result.IsEmpty()) {
      return isolate->factory()->undefined_value();
    }
    Handle<Object> return_value = v8::Utils::OpenHandle(*result);
    return_value->VerifyApiCallResultType();
    // Rebox handle before return.
    return handle(*return_value, isolate);
  }

  // __defineGetter__ callback
  Handle<Object> getter(Handle<AccessorPair>::cast(structure)->getter(),
                        isolate);
  if (getter->IsSpecFunction()) {
    // TODO(rossberg): nicer would be to cast to some JSCallable here...
    return Object::GetPropertyWithDefinedGetter(
        receiver, Handle<JSReceiver>::cast(getter));
  }
  // Getter is not a function.
  return isolate->factory()->undefined_value();
}


bool AccessorInfo::IsCompatibleReceiverType(Isolate* isolate,
                                            Handle<AccessorInfo> info,
                                            Handle<HeapType> type) {
  if (!info->HasExpectedReceiverType()) return true;
  Handle<Map> map = IC::TypeToMap(*type, isolate);
  if (!map->IsJSObjectMap()) return false;
  return FunctionTemplateInfo::cast(info->expected_receiver_type())
      ->IsTemplateFor(*map);
}


MaybeHandle<Object> Object::SetPropertyWithAccessor(
    Handle<Object> receiver, Handle<Name> name, Handle<Object> value,
    Handle<JSObject> holder, Handle<Object> structure, StrictMode strict_mode) {
  Isolate* isolate = name->GetIsolate();

  // We should never get here to initialize a const with the hole
  // value since a const declaration would conflict with the setter.
  DCHECK(!structure->IsForeign());
  if (structure->IsExecutableAccessorInfo()) {
    // Don't call executable accessor setters with non-JSObject receivers.
    if (!receiver->IsJSObject()) return value;
    // api style callbacks
    ExecutableAccessorInfo* info = ExecutableAccessorInfo::cast(*structure);
    if (!info->IsCompatibleReceiver(*receiver)) {
      Handle<Object> args[2] = { name, receiver };
      Handle<Object> error =
          isolate->factory()->NewTypeError("incompatible_method_receiver",
                                           HandleVector(args,
                                                        ARRAY_SIZE(args)));
      return isolate->Throw<Object>(error);
    }
    // TODO(rossberg): Support symbols in the API.
    if (name->IsSymbol()) return value;
    Object* call_obj = info->setter();
    v8::AccessorSetterCallback call_fun =
        v8::ToCData<v8::AccessorSetterCallback>(call_obj);
    if (call_fun == NULL) return value;
    Handle<String> key = Handle<String>::cast(name);
    LOG(isolate, ApiNamedPropertyAccess("store", *holder, *name));
    PropertyCallbackArguments args(isolate, info->data(), *receiver, *holder);
    args.Call(call_fun,
              v8::Utils::ToLocal(key),
              v8::Utils::ToLocal(value));
    RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
    return value;
  }

  if (structure->IsAccessorPair()) {
    Handle<Object> setter(AccessorPair::cast(*structure)->setter(), isolate);
    if (setter->IsSpecFunction()) {
      // TODO(rossberg): nicer would be to cast to some JSCallable here...
      return SetPropertyWithDefinedSetter(
          receiver, Handle<JSReceiver>::cast(setter), value);
    } else {
      if (strict_mode == SLOPPY) return value;
      Handle<Object> args[2] = { name, holder };
      Handle<Object> error =
          isolate->factory()->NewTypeError("no_setter_in_callback",
                                           HandleVector(args, 2));
      return isolate->Throw<Object>(error);
    }
  }

  // TODO(dcarney): Handle correctly.
  if (structure->IsDeclaredAccessorInfo()) {
    return value;
  }

  UNREACHABLE();
  return MaybeHandle<Object>();
}


MaybeHandle<Object> Object::GetPropertyWithDefinedGetter(
    Handle<Object> receiver,
    Handle<JSReceiver> getter) {
  Isolate* isolate = getter->GetIsolate();
  Debug* debug = isolate->debug();
  // Handle stepping into a getter if step into is active.
  // TODO(rossberg): should this apply to getters that are function proxies?
  if (debug->StepInActive() && getter->IsJSFunction()) {
    debug->HandleStepIn(
        Handle<JSFunction>::cast(getter), Handle<Object>::null(), 0, false);
  }

  return Execution::Call(isolate, getter, receiver, 0, NULL, true);
}


MaybeHandle<Object> Object::SetPropertyWithDefinedSetter(
    Handle<Object> receiver,
    Handle<JSReceiver> setter,
    Handle<Object> value) {
  Isolate* isolate = setter->GetIsolate();

  Debug* debug = isolate->debug();
  // Handle stepping into a setter if step into is active.
  // TODO(rossberg): should this apply to getters that are function proxies?
  if (debug->StepInActive() && setter->IsJSFunction()) {
    debug->HandleStepIn(
        Handle<JSFunction>::cast(setter), Handle<Object>::null(), 0, false);
  }

  Handle<Object> argv[] = { value };
  RETURN_ON_EXCEPTION(isolate, Execution::Call(isolate, setter, receiver,
                                               ARRAY_SIZE(argv), argv, true),
                      Object);
  return value;
}


static bool FindAllCanReadHolder(LookupIterator* it) {
  it->skip_interceptor();
  it->skip_access_check();
  for (; it->IsFound(); it->Next()) {
    if (it->state() == LookupIterator::PROPERTY &&
        it->HasProperty() &&
        it->property_kind() == LookupIterator::ACCESSOR) {
      Handle<Object> accessors = it->GetAccessors();
      if (accessors->IsAccessorInfo()) {
        if (AccessorInfo::cast(*accessors)->all_can_read()) return true;
      }
    }
  }
  return false;
}


MaybeHandle<Object> JSObject::GetPropertyWithFailedAccessCheck(
    LookupIterator* it) {
  Handle<JSObject> checked = it->GetHolder<JSObject>();
  if (FindAllCanReadHolder(it)) {
    return GetPropertyWithAccessor(it->GetReceiver(), it->name(),
                                   it->GetHolder<JSObject>(),
                                   it->GetAccessors());
  }
  it->isolate()->ReportFailedAccessCheck(checked, v8::ACCESS_GET);
  RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(it->isolate(), Object);
  return it->factory()->undefined_value();
}


Maybe<PropertyAttributes> JSObject::GetPropertyAttributesWithFailedAccessCheck(
    LookupIterator* it) {
  Handle<JSObject> checked = it->GetHolder<JSObject>();
  if (FindAllCanReadHolder(it))
    return maybe(it->property_details().attributes());
  it->isolate()->ReportFailedAccessCheck(checked, v8::ACCESS_HAS);
  RETURN_VALUE_IF_SCHEDULED_EXCEPTION(it->isolate(),
                                      Maybe<PropertyAttributes>());
  return maybe(ABSENT);
}


static bool FindAllCanWriteHolder(LookupIterator* it) {
  it->skip_interceptor();
  it->skip_access_check();
  for (; it->IsFound(); it->Next()) {
    if (it->state() == LookupIterator::PROPERTY && it->HasProperty() &&
        it->property_kind() == LookupIterator::ACCESSOR) {
      Handle<Object> accessors = it->GetAccessors();
      if (accessors->IsAccessorInfo()) {
        if (AccessorInfo::cast(*accessors)->all_can_write()) return true;
      }
    }
  }
  return false;
}


MaybeHandle<Object> JSObject::SetPropertyWithFailedAccessCheck(
    LookupIterator* it, Handle<Object> value, StrictMode strict_mode) {
  Handle<JSObject> checked = it->GetHolder<JSObject>();
  if (FindAllCanWriteHolder(it)) {
    return SetPropertyWithAccessor(it->GetReceiver(), it->name(), value,
                                   it->GetHolder<JSObject>(),
                                   it->GetAccessors(), strict_mode);
  }

  it->isolate()->ReportFailedAccessCheck(checked, v8::ACCESS_SET);
  RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(it->isolate(), Object);
  return value;
}


Object* JSObject::GetNormalizedProperty(const LookupResult* result) {
  DCHECK(!HasFastProperties());
  Object* value = property_dictionary()->ValueAt(result->GetDictionaryEntry());
  if (IsGlobalObject()) {
    value = PropertyCell::cast(value)->value();
  }
  DCHECK(!value->IsPropertyCell() && !value->IsCell());
  return value;
}


Handle<Object> JSObject::GetNormalizedProperty(Handle<JSObject> object,
                                               const LookupResult* result) {
  DCHECK(!object->HasFastProperties());
  Isolate* isolate = object->GetIsolate();
  Handle<Object> value(object->property_dictionary()->ValueAt(
      result->GetDictionaryEntry()), isolate);
  if (object->IsGlobalObject()) {
    value = handle(Handle<PropertyCell>::cast(value)->value(), isolate);
    DCHECK(!value->IsTheHole());
  }
  DCHECK(!value->IsPropertyCell() && !value->IsCell());
  return value;
}


void JSObject::SetNormalizedProperty(Handle<JSObject> object,
                                     const LookupResult* result,
                                     Handle<Object> value) {
  DCHECK(!object->HasFastProperties());
  NameDictionary* property_dictionary = object->property_dictionary();
  if (object->IsGlobalObject()) {
    Handle<PropertyCell> cell(PropertyCell::cast(
        property_dictionary->ValueAt(result->GetDictionaryEntry())));
    PropertyCell::SetValueInferType(cell, value);
  } else {
    property_dictionary->ValueAtPut(result->GetDictionaryEntry(), *value);
  }
}


void JSObject::SetNormalizedProperty(Handle<JSObject> object,
                                     Handle<Name> name,
                                     Handle<Object> value,
                                     PropertyDetails details) {
  DCHECK(!object->HasFastProperties());
  Handle<NameDictionary> property_dictionary(object->property_dictionary());

  if (!name->IsUniqueName()) {
    name = object->GetIsolate()->factory()->InternalizeString(
        Handle<String>::cast(name));
  }

  int entry = property_dictionary->FindEntry(name);
  if (entry == NameDictionary::kNotFound) {
    Handle<Object> store_value = value;
    if (object->IsGlobalObject()) {
      store_value = object->GetIsolate()->factory()->NewPropertyCell(value);
    }

    property_dictionary = NameDictionary::Add(
        property_dictionary, name, store_value, details);
    object->set_properties(*property_dictionary);
    return;
  }

  PropertyDetails original_details = property_dictionary->DetailsAt(entry);
  int enumeration_index;
  // Preserve the enumeration index unless the property was deleted.
  if (original_details.IsDeleted()) {
    enumeration_index = property_dictionary->NextEnumerationIndex();
    property_dictionary->SetNextEnumerationIndex(enumeration_index + 1);
  } else {
    enumeration_index = original_details.dictionary_index();
    DCHECK(enumeration_index > 0);
  }

  details = PropertyDetails(
      details.attributes(), details.type(), enumeration_index);

  if (object->IsGlobalObject()) {
    Handle<PropertyCell> cell(
        PropertyCell::cast(property_dictionary->ValueAt(entry)));
    PropertyCell::SetValueInferType(cell, value);
    // Please note we have to update the property details.
    property_dictionary->DetailsAtPut(entry, details);
  } else {
    property_dictionary->SetEntry(entry, name, value, details);
  }
}


Handle<Object> JSObject::DeleteNormalizedProperty(Handle<JSObject> object,
                                                  Handle<Name> name,
                                                  DeleteMode mode) {
  DCHECK(!object->HasFastProperties());
  Isolate* isolate = object->GetIsolate();
  Handle<NameDictionary> dictionary(object->property_dictionary());
  int entry = dictionary->FindEntry(name);
  if (entry != NameDictionary::kNotFound) {
    // If we have a global object set the cell to the hole.
    if (object->IsGlobalObject()) {
      PropertyDetails details = dictionary->DetailsAt(entry);
      if (details.IsDontDelete()) {
        if (mode != FORCE_DELETION) return isolate->factory()->false_value();
        // When forced to delete global properties, we have to make a
        // map change to invalidate any ICs that think they can load
        // from the DontDelete cell without checking if it contains
        // the hole value.
        Handle<Map> new_map = Map::CopyDropDescriptors(handle(object->map()));
        DCHECK(new_map->is_dictionary_map());
        JSObject::MigrateToMap(object, new_map);
      }
      Handle<PropertyCell> cell(PropertyCell::cast(dictionary->ValueAt(entry)));
      Handle<Object> value = isolate->factory()->the_hole_value();
      PropertyCell::SetValueInferType(cell, value);
      dictionary->DetailsAtPut(entry, details.AsDeleted());
    } else {
      Handle<Object> deleted(
          NameDictionary::DeleteProperty(dictionary, entry, mode));
      if (*deleted == isolate->heap()->true_value()) {
        Handle<NameDictionary> new_properties =
            NameDictionary::Shrink(dictionary, name);
        object->set_properties(*new_properties);
      }
      return deleted;
    }
  }
  return isolate->factory()->true_value();
}


bool JSObject::IsDirty() {
  Object* cons_obj = map()->constructor();
  if (!cons_obj->IsJSFunction())
    return true;
  JSFunction* fun = JSFunction::cast(cons_obj);
  if (!fun->shared()->IsApiFunction())
    return true;
  // If the object is fully fast case and has the same map it was
  // created with then no changes can have been made to it.
  return map() != fun->initial_map()
      || !HasFastObjectElements()
      || !HasFastProperties();
}


MaybeHandle<Object> Object::GetElementWithReceiver(Isolate* isolate,
                                                   Handle<Object> object,
                                                   Handle<Object> receiver,
                                                   uint32_t index) {
  if (object->IsUndefined()) {
    // TODO(verwaest): Why is this check here?
    UNREACHABLE();
    return isolate->factory()->undefined_value();
  }

  // Iterate up the prototype chain until an element is found or the null
  // prototype is encountered.
  for (PrototypeIterator iter(isolate, object,
                              object->IsJSProxy() || object->IsJSObject()
                                  ? PrototypeIterator::START_AT_RECEIVER
                                  : PrototypeIterator::START_AT_PROTOTYPE);
       !iter.IsAtEnd(); iter.Advance()) {
    if (PrototypeIterator::GetCurrent(iter)->IsJSProxy()) {
      return JSProxy::GetElementWithHandler(
          Handle<JSProxy>::cast(PrototypeIterator::GetCurrent(iter)), receiver,
          index);
    }

    // Inline the case for JSObjects. Doing so significantly improves the
    // performance of fetching elements where checking the prototype chain is
    // necessary.
    Handle<JSObject> js_object =
        Handle<JSObject>::cast(PrototypeIterator::GetCurrent(iter));

    // Check access rights if needed.
    if (js_object->IsAccessCheckNeeded()) {
      if (!isolate->MayIndexedAccess(js_object, index, v8::ACCESS_GET)) {
        isolate->ReportFailedAccessCheck(js_object, v8::ACCESS_GET);
        RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
        return isolate->factory()->undefined_value();
      }
    }

    if (js_object->HasIndexedInterceptor()) {
      return JSObject::GetElementWithInterceptor(js_object, receiver, index);
    }

    if (js_object->elements() != isolate->heap()->empty_fixed_array()) {
      Handle<Object> result;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, result,
          js_object->GetElementsAccessor()->Get(receiver, js_object, index),
          Object);
      if (!result->IsTheHole()) return result;
    }
  }

  return isolate->factory()->undefined_value();
}


Map* Object::GetRootMap(Isolate* isolate) {
  DisallowHeapAllocation no_alloc;
  if (IsSmi()) {
    Context* context = isolate->context()->native_context();
    return context->number_function()->initial_map();
  }

  HeapObject* heap_object = HeapObject::cast(this);

  // The object is either a number, a string, a boolean,
  // a real JS object, or a Harmony proxy.
  if (heap_object->IsJSReceiver()) {
    return heap_object->map();
  }
  Context* context = isolate->context()->native_context();

  if (heap_object->IsHeapNumber()) {
    return context->number_function()->initial_map();
  }
  if (heap_object->IsString()) {
    return context->string_function()->initial_map();
  }
  if (heap_object->IsSymbol()) {
    return context->symbol_function()->initial_map();
  }
  if (heap_object->IsBoolean()) {
    return context->boolean_function()->initial_map();
  }
  return isolate->heap()->null_value()->map();
}


Object* Object::GetHash() {
  // The object is either a number, a name, an odd-ball,
  // a real JS object, or a Harmony proxy.
  if (IsNumber()) {
    uint32_t hash = ComputeLongHash(double_to_uint64(Number()));
    return Smi::FromInt(hash & Smi::kMaxValue);
  }
  if (IsName()) {
    uint32_t hash = Name::cast(this)->Hash();
    return Smi::FromInt(hash);
  }
  if (IsOddball()) {
    uint32_t hash = Oddball::cast(this)->to_string()->Hash();
    return Smi::FromInt(hash);
  }

  DCHECK(IsJSReceiver());
  return JSReceiver::cast(this)->GetIdentityHash();
}


Handle<Smi> Object::GetOrCreateHash(Isolate* isolate, Handle<Object> object) {
  Handle<Object> hash(object->GetHash(), isolate);
  if (hash->IsSmi()) return Handle<Smi>::cast(hash);

  DCHECK(object->IsJSReceiver());
  return JSReceiver::GetOrCreateIdentityHash(Handle<JSReceiver>::cast(object));
}


bool Object::SameValue(Object* other) {
  if (other == this) return true;

  // The object is either a number, a name, an odd-ball,
  // a real JS object, or a Harmony proxy.
  if (IsNumber() && other->IsNumber()) {
    double this_value = Number();
    double other_value = other->Number();
    bool equal = this_value == other_value;
    // SameValue(NaN, NaN) is true.
    if (!equal) return std::isnan(this_value) && std::isnan(other_value);
    // SameValue(0.0, -0.0) is false.
    return (this_value != 0) || ((1 / this_value) == (1 / other_value));
  }
  if (IsString() && other->IsString()) {
    return String::cast(this)->Equals(String::cast(other));
  }
  return false;
}


bool Object::SameValueZero(Object* other) {
  if (other == this) return true;

  // The object is either a number, a name, an odd-ball,
  // a real JS object, or a Harmony proxy.
  if (IsNumber() && other->IsNumber()) {
    double this_value = Number();
    double other_value = other->Number();
    // +0 == -0 is true
    return this_value == other_value
        || (std::isnan(this_value) && std::isnan(other_value));
  }
  if (IsString() && other->IsString()) {
    return String::cast(this)->Equals(String::cast(other));
  }
  return false;
}


void Object::ShortPrint(FILE* out) {
  OFStream os(out);
  os << Brief(this);
}


void Object::ShortPrint(StringStream* accumulator) {
  OStringStream os;
  os << Brief(this);
  accumulator->Add(os.c_str());
}


OStream& operator<<(OStream& os, const Brief& v) {
  if (v.value->IsSmi()) {
    Smi::cast(v.value)->SmiPrint(os);
  } else {
    // TODO(svenpanne) Const-correct HeapObjectShortPrint!
    HeapObject* obj = const_cast<HeapObject*>(HeapObject::cast(v.value));
    obj->HeapObjectShortPrint(os);
  }
  return os;
}


void Smi::SmiPrint(OStream& os) const {  // NOLINT
  os << value();
}


// Should a word be prefixed by 'a' or 'an' in order to read naturally in
// English?  Returns false for non-ASCII or words that don't start with
// a capital letter.  The a/an rule follows pronunciation in English.
// We don't use the BBC's overcorrect "an historic occasion" though if
// you speak a dialect you may well say "an 'istoric occasion".
static bool AnWord(String* str) {
  if (str->length() == 0) return false;  // A nothing.
  int c0 = str->Get(0);
  int c1 = str->length() > 1 ? str->Get(1) : 0;
  if (c0 == 'U') {
    if (c1 > 'Z') {
      return true;  // An Umpire, but a UTF8String, a U.
    }
  } else if (c0 == 'A' || c0 == 'E' || c0 == 'I' || c0 == 'O') {
    return true;    // An Ape, an ABCBook.
  } else if ((c1 == 0 || (c1 >= 'A' && c1 <= 'Z')) &&
           (c0 == 'F' || c0 == 'H' || c0 == 'M' || c0 == 'N' || c0 == 'R' ||
            c0 == 'S' || c0 == 'X')) {
    return true;    // An MP3File, an M.
  }
  return false;
}


Handle<String> String::SlowFlatten(Handle<ConsString> cons,
                                   PretenureFlag pretenure) {
  DCHECK(AllowHeapAllocation::IsAllowed());
  DCHECK(cons->second()->length() != 0);
  Isolate* isolate = cons->GetIsolate();
  int length = cons->length();
  PretenureFlag tenure = isolate->heap()->InNewSpace(*cons) ? pretenure
                                                            : TENURED;
  Handle<SeqString> result;
  if (cons->IsOneByteRepresentation()) {
    Handle<SeqOneByteString> flat = isolate->factory()->NewRawOneByteString(
        length, tenure).ToHandleChecked();
    DisallowHeapAllocation no_gc;
    WriteToFlat(*cons, flat->GetChars(), 0, length);
    result = flat;
  } else {
    Handle<SeqTwoByteString> flat = isolate->factory()->NewRawTwoByteString(
        length, tenure).ToHandleChecked();
    DisallowHeapAllocation no_gc;
    WriteToFlat(*cons, flat->GetChars(), 0, length);
    result = flat;
  }
  cons->set_first(*result);
  cons->set_second(isolate->heap()->empty_string());
  DCHECK(result->IsFlat());
  return result;
}



bool String::MakeExternal(v8::String::ExternalStringResource* resource) {
  // Externalizing twice leaks the external resource, so it's
  // prohibited by the API.
  DCHECK(!this->IsExternalString());
#ifdef ENABLE_SLOW_DCHECKS
  if (FLAG_enable_slow_asserts) {
    // Assert that the resource and the string are equivalent.
    DCHECK(static_cast<size_t>(this->length()) == resource->length());
    ScopedVector<uc16> smart_chars(this->length());
    String::WriteToFlat(this, smart_chars.start(), 0, this->length());
    DCHECK(memcmp(smart_chars.start(),
                  resource->data(),
                  resource->length() * sizeof(smart_chars[0])) == 0);
  }
#endif  // DEBUG
  int size = this->Size();  // Byte size of the original string.
  // Abort if size does not allow in-place conversion.
  if (size < ExternalString::kShortSize) return false;
  Heap* heap = GetHeap();
  bool is_ascii = this->IsOneByteRepresentation();
  bool is_internalized = this->IsInternalizedString();

  // Morph the string to an external string by replacing the map and
  // reinitializing the fields.  This won't work if
  // - the space the existing string occupies is too small for a regular
  //   external string.
  // - the existing string is in old pointer space and the backing store of
  //   the external string is not aligned.  The GC cannot deal with a field
  //   containing a possibly unaligned address to outside of V8's heap.
  // In either case we resort to a short external string instead, omitting
  // the field caching the address of the backing store.  When we encounter
  // short external strings in generated code, we need to bailout to runtime.
  Map* new_map;
  if (size < ExternalString::kSize ||
      heap->old_pointer_space()->Contains(this)) {
    new_map = is_internalized
        ? (is_ascii
            ? heap->
                short_external_internalized_string_with_one_byte_data_map()
            : heap->short_external_internalized_string_map())
        : (is_ascii
            ? heap->short_external_string_with_one_byte_data_map()
            : heap->short_external_string_map());
  } else {
    new_map = is_internalized
        ? (is_ascii
            ? heap->external_internalized_string_with_one_byte_data_map()
            : heap->external_internalized_string_map())
        : (is_ascii
            ? heap->external_string_with_one_byte_data_map()
            : heap->external_string_map());
  }

  // Byte size of the external String object.
  int new_size = this->SizeFromMap(new_map);
  heap->CreateFillerObjectAt(this->address() + new_size, size - new_size);

  // We are storing the new map using release store after creating a filler for
  // the left-over space to avoid races with the sweeper thread.
  this->synchronized_set_map(new_map);

  ExternalTwoByteString* self = ExternalTwoByteString::cast(this);
  self->set_resource(resource);
  if (is_internalized) self->Hash();  // Force regeneration of the hash value.

  heap->AdjustLiveBytes(this->address(), new_size - size, Heap::FROM_MUTATOR);
  return true;
}


bool String::MakeExternal(v8::String::ExternalAsciiStringResource* resource) {
  // Externalizing twice leaks the external resource, so it's
  // prohibited by the API.
  DCHECK(!this->IsExternalString());
#ifdef ENABLE_SLOW_DCHECKS
  if (FLAG_enable_slow_asserts) {
    // Assert that the resource and the string are equivalent.
    DCHECK(static_cast<size_t>(this->length()) == resource->length());
    if (this->IsTwoByteRepresentation()) {
      ScopedVector<uint16_t> smart_chars(this->length());
      String::WriteToFlat(this, smart_chars.start(), 0, this->length());
      DCHECK(String::IsOneByte(smart_chars.start(), this->length()));
    }
    ScopedVector<char> smart_chars(this->length());
    String::WriteToFlat(this, smart_chars.start(), 0, this->length());
    DCHECK(memcmp(smart_chars.start(),
                  resource->data(),
                  resource->length() * sizeof(smart_chars[0])) == 0);
  }
#endif  // DEBUG
  int size = this->Size();  // Byte size of the original string.
  // Abort if size does not allow in-place conversion.
  if (size < ExternalString::kShortSize) return false;
  Heap* heap = GetHeap();
  bool is_internalized = this->IsInternalizedString();

  // Morph the string to an external string by replacing the map and
  // reinitializing the fields.  This won't work if
  // - the space the existing string occupies is too small for a regular
  //   external string.
  // - the existing string is in old pointer space and the backing store of
  //   the external string is not aligned.  The GC cannot deal with a field
  //   containing a possibly unaligned address to outside of V8's heap.
  // In either case we resort to a short external string instead, omitting
  // the field caching the address of the backing store.  When we encounter
  // short external strings in generated code, we need to bailout to runtime.
  Map* new_map;
  if (size < ExternalString::kSize ||
      heap->old_pointer_space()->Contains(this)) {
    new_map = is_internalized
        ? heap->short_external_ascii_internalized_string_map()
        : heap->short_external_ascii_string_map();
  } else {
    new_map = is_internalized
        ? heap->external_ascii_internalized_string_map()
        : heap->external_ascii_string_map();
  }

  // Byte size of the external String object.
  int new_size = this->SizeFromMap(new_map);
  heap->CreateFillerObjectAt(this->address() + new_size, size - new_size);

  // We are storing the new map using release store after creating a filler for
  // the left-over space to avoid races with the sweeper thread.
  this->synchronized_set_map(new_map);

  ExternalAsciiString* self = ExternalAsciiString::cast(this);
  self->set_resource(resource);
  if (is_internalized) self->Hash();  // Force regeneration of the hash value.

  heap->AdjustLiveBytes(this->address(), new_size - size, Heap::FROM_MUTATOR);
  return true;
}


void String::StringShortPrint(StringStream* accumulator) {
  int len = length();
  if (len > kMaxShortPrintLength) {
    accumulator->Add("<Very long string[%u]>", len);
    return;
  }

  if (!LooksValid()) {
    accumulator->Add("<Invalid String>");
    return;
  }

  ConsStringIteratorOp op;
  StringCharacterStream stream(this, &op);

  bool truncated = false;
  if (len > kMaxShortPrintLength) {
    len = kMaxShortPrintLength;
    truncated = true;
  }
  bool ascii = true;
  for (int i = 0; i < len; i++) {
    uint16_t c = stream.GetNext();

    if (c < 32 || c >= 127) {
      ascii = false;
    }
  }
  stream.Reset(this);
  if (ascii) {
    accumulator->Add("<String[%u]: ", length());
    for (int i = 0; i < len; i++) {
      accumulator->Put(static_cast<char>(stream.GetNext()));
    }
    accumulator->Put('>');
  } else {
    // Backslash indicates that the string contains control
    // characters and that backslashes are therefore escaped.
    accumulator->Add("<String[%u]\\: ", length());
    for (int i = 0; i < len; i++) {
      uint16_t c = stream.GetNext();
      if (c == '\n') {
        accumulator->Add("\\n");
      } else if (c == '\r') {
        accumulator->Add("\\r");
      } else if (c == '\\') {
        accumulator->Add("\\\\");
      } else if (c < 32 || c > 126) {
        accumulator->Add("\\x%02x", c);
      } else {
        accumulator->Put(static_cast<char>(c));
      }
    }
    if (truncated) {
      accumulator->Put('.');
      accumulator->Put('.');
      accumulator->Put('.');
    }
    accumulator->Put('>');
  }
  return;
}


void String::PrintUC16(OStream& os, int start, int end) {  // NOLINT
  if (end < 0) end = length();
  ConsStringIteratorOp op;
  StringCharacterStream stream(this, &op, start);
  for (int i = start; i < end && stream.HasMore(); i++) {
    os << AsUC16(stream.GetNext());
  }
}


void JSObject::JSObjectShortPrint(StringStream* accumulator) {
  switch (map()->instance_type()) {
    case JS_ARRAY_TYPE: {
      double length = JSArray::cast(this)->length()->IsUndefined()
          ? 0
          : JSArray::cast(this)->length()->Number();
      accumulator->Add("<JS Array[%u]>", static_cast<uint32_t>(length));
      break;
    }
    case JS_WEAK_MAP_TYPE: {
      accumulator->Add("<JS WeakMap>");
      break;
    }
    case JS_WEAK_SET_TYPE: {
      accumulator->Add("<JS WeakSet>");
      break;
    }
    case JS_REGEXP_TYPE: {
      accumulator->Add("<JS RegExp>");
      break;
    }
    case JS_FUNCTION_TYPE: {
      JSFunction* function = JSFunction::cast(this);
      Object* fun_name = function->shared()->DebugName();
      bool printed = false;
      if (fun_name->IsString()) {
        String* str = String::cast(fun_name);
        if (str->length() > 0) {
          accumulator->Add("<JS Function ");
          accumulator->Put(str);
          printed = true;
        }
      }
      if (!printed) {
        accumulator->Add("<JS Function");
      }
      accumulator->Add(" (SharedFunctionInfo %p)",
                       reinterpret_cast<void*>(function->shared()));
      accumulator->Put('>');
      break;
    }
    case JS_GENERATOR_OBJECT_TYPE: {
      accumulator->Add("<JS Generator>");
      break;
    }
    case JS_MODULE_TYPE: {
      accumulator->Add("<JS Module>");
      break;
    }
    // All other JSObjects are rather similar to each other (JSObject,
    // JSGlobalProxy, JSGlobalObject, JSUndetectableObject, JSValue).
    default: {
      Map* map_of_this = map();
      Heap* heap = GetHeap();
      Object* constructor = map_of_this->constructor();
      bool printed = false;
      if (constructor->IsHeapObject() &&
          !heap->Contains(HeapObject::cast(constructor))) {
        accumulator->Add("!!!INVALID CONSTRUCTOR!!!");
      } else {
        bool global_object = IsJSGlobalProxy();
        if (constructor->IsJSFunction()) {
          if (!heap->Contains(JSFunction::cast(constructor)->shared())) {
            accumulator->Add("!!!INVALID SHARED ON CONSTRUCTOR!!!");
          } else {
            Object* constructor_name =
                JSFunction::cast(constructor)->shared()->name();
            if (constructor_name->IsString()) {
              String* str = String::cast(constructor_name);
              if (str->length() > 0) {
                bool vowel = AnWord(str);
                accumulator->Add("<%sa%s ",
                       global_object ? "Global Object: " : "",
                       vowel ? "n" : "");
                accumulator->Put(str);
                accumulator->Add(" with %smap %p",
                    map_of_this->is_deprecated() ? "deprecated " : "",
                    map_of_this);
                printed = true;
              }
            }
          }
        }
        if (!printed) {
          accumulator->Add("<JS %sObject", global_object ? "Global " : "");
        }
      }
      if (IsJSValue()) {
        accumulator->Add(" value = ");
        JSValue::cast(this)->value()->ShortPrint(accumulator);
      }
      accumulator->Put('>');
      break;
    }
  }
}


void JSObject::PrintElementsTransition(
    FILE* file, Handle<JSObject> object,
    ElementsKind from_kind, Handle<FixedArrayBase> from_elements,
    ElementsKind to_kind, Handle<FixedArrayBase> to_elements) {
  if (from_kind != to_kind) {
    OFStream os(file);
    os << "elements transition [" << ElementsKindToString(from_kind) << " -> "
       << ElementsKindToString(to_kind) << "] in ";
    JavaScriptFrame::PrintTop(object->GetIsolate(), file, false, true);
    PrintF(file, " for ");
    object->ShortPrint(file);
    PrintF(file, " from ");
    from_elements->ShortPrint(file);
    PrintF(file, " to ");
    to_elements->ShortPrint(file);
    PrintF(file, "\n");
  }
}


void Map::PrintGeneralization(FILE* file,
                              const char* reason,
                              int modify_index,
                              int split,
                              int descriptors,
                              bool constant_to_field,
                              Representation old_representation,
                              Representation new_representation,
                              HeapType* old_field_type,
                              HeapType* new_field_type) {
  OFStream os(file);
  os << "[generalizing ";
  constructor_name()->PrintOn(file);
  os << "] ";
  Name* name = instance_descriptors()->GetKey(modify_index);
  if (name->IsString()) {
    String::cast(name)->PrintOn(file);
  } else {
    os << "{symbol " << static_cast<void*>(name) << "}";
  }
  os << ":";
  if (constant_to_field) {
    os << "c";
  } else {
    os << old_representation.Mnemonic() << "{";
    old_field_type->PrintTo(os, HeapType::SEMANTIC_DIM);
    os << "}";
  }
  os << "->" << new_representation.Mnemonic() << "{";
  new_field_type->PrintTo(os, HeapType::SEMANTIC_DIM);
  os << "} (";
  if (strlen(reason) > 0) {
    os << reason;
  } else {
    os << "+" << (descriptors - split) << " maps";
  }
  os << ") [";
  JavaScriptFrame::PrintTop(GetIsolate(), file, false, true);
  os << "]\n";
}


void JSObject::PrintInstanceMigration(FILE* file,
                                      Map* original_map,
                                      Map* new_map) {
  PrintF(file, "[migrating ");
  map()->constructor_name()->PrintOn(file);
  PrintF(file, "] ");
  DescriptorArray* o = original_map->instance_descriptors();
  DescriptorArray* n = new_map->instance_descriptors();
  for (int i = 0; i < original_map->NumberOfOwnDescriptors(); i++) {
    Representation o_r = o->GetDetails(i).representation();
    Representation n_r = n->GetDetails(i).representation();
    if (!o_r.Equals(n_r)) {
      String::cast(o->GetKey(i))->PrintOn(file);
      PrintF(file, ":%s->%s ", o_r.Mnemonic(), n_r.Mnemonic());
    } else if (o->GetDetails(i).type() == CONSTANT &&
               n->GetDetails(i).type() == FIELD) {
      Name* name = o->GetKey(i);
      if (name->IsString()) {
        String::cast(name)->PrintOn(file);
      } else {
        PrintF(file, "{symbol %p}", static_cast<void*>(name));
      }
      PrintF(file, " ");
    }
  }
  PrintF(file, "\n");
}


void HeapObject::HeapObjectShortPrint(OStream& os) {  // NOLINT
  Heap* heap = GetHeap();
  if (!heap->Contains(this)) {
    os << "!!!INVALID POINTER!!!";
    return;
  }
  if (!heap->Contains(map())) {
    os << "!!!INVALID MAP!!!";
    return;
  }

  os << this << " ";

  if (IsString()) {
    HeapStringAllocator allocator;
    StringStream accumulator(&allocator);
    String::cast(this)->StringShortPrint(&accumulator);
    os << accumulator.ToCString().get();
    return;
  }
  if (IsJSObject()) {
    HeapStringAllocator allocator;
    StringStream accumulator(&allocator);
    JSObject::cast(this)->JSObjectShortPrint(&accumulator);
    os << accumulator.ToCString().get();
    return;
  }
  switch (map()->instance_type()) {
    case MAP_TYPE:
      os << "<Map(elements=" << Map::cast(this)->elements_kind() << ")>";
      break;
    case FIXED_ARRAY_TYPE:
      os << "<FixedArray[" << FixedArray::cast(this)->length() << "]>";
      break;
    case FIXED_DOUBLE_ARRAY_TYPE:
      os << "<FixedDoubleArray[" << FixedDoubleArray::cast(this)->length()
         << "]>";
      break;
    case BYTE_ARRAY_TYPE:
      os << "<ByteArray[" << ByteArray::cast(this)->length() << "]>";
      break;
    case FREE_SPACE_TYPE:
      os << "<FreeSpace[" << FreeSpace::cast(this)->Size() << "]>";
      break;
#define TYPED_ARRAY_SHORT_PRINT(Type, type, TYPE, ctype, size)                \
  case EXTERNAL_##TYPE##_ARRAY_TYPE:                                          \
    os << "<External" #Type "Array["                                          \
       << External##Type##Array::cast(this)->length() << "]>";                \
    break;                                                                    \
  case FIXED_##TYPE##_ARRAY_TYPE:                                             \
    os << "<Fixed" #Type "Array[" << Fixed##Type##Array::cast(this)->length() \
       << "]>";                                                               \
    break;

    TYPED_ARRAYS(TYPED_ARRAY_SHORT_PRINT)
#undef TYPED_ARRAY_SHORT_PRINT

    case SHARED_FUNCTION_INFO_TYPE: {
      SharedFunctionInfo* shared = SharedFunctionInfo::cast(this);
      SmartArrayPointer<char> debug_name =
          shared->DebugName()->ToCString();
      if (debug_name[0] != 0) {
        os << "<SharedFunctionInfo " << debug_name.get() << ">";
      } else {
        os << "<SharedFunctionInfo>";
      }
      break;
    }
    case JS_MESSAGE_OBJECT_TYPE:
      os << "<JSMessageObject>";
      break;
#define MAKE_STRUCT_CASE(NAME, Name, name) \
  case NAME##_TYPE:                        \
    os << "<" #Name ">";                   \
    break;
  STRUCT_LIST(MAKE_STRUCT_CASE)
#undef MAKE_STRUCT_CASE
    case CODE_TYPE: {
      Code* code = Code::cast(this);
      os << "<Code: " << Code::Kind2String(code->kind()) << ">";
      break;
    }
    case ODDBALL_TYPE: {
      if (IsUndefined()) {
        os << "<undefined>";
      } else if (IsTheHole()) {
        os << "<the hole>";
      } else if (IsNull()) {
        os << "<null>";
      } else if (IsTrue()) {
        os << "<true>";
      } else if (IsFalse()) {
        os << "<false>";
      } else {
        os << "<Odd Oddball>";
      }
      break;
    }
    case SYMBOL_TYPE: {
      Symbol* symbol = Symbol::cast(this);
      os << "<Symbol: " << symbol->Hash();
      if (!symbol->name()->IsUndefined()) {
        os << " ";
        HeapStringAllocator allocator;
        StringStream accumulator(&allocator);
        String::cast(symbol->name())->StringShortPrint(&accumulator);
        os << accumulator.ToCString().get();
      }
      os << ">";
      break;
    }
    case HEAP_NUMBER_TYPE: {
      os << "<Number: ";
      HeapNumber::cast(this)->HeapNumberPrint(os);
      os << ">";
      break;
    }
    case MUTABLE_HEAP_NUMBER_TYPE: {
      os << "<MutableNumber: ";
      HeapNumber::cast(this)->HeapNumberPrint(os);
      os << '>';
      break;
    }
    case JS_PROXY_TYPE:
      os << "<JSProxy>";
      break;
    case JS_FUNCTION_PROXY_TYPE:
      os << "<JSFunctionProxy>";
      break;
    case FOREIGN_TYPE:
      os << "<Foreign>";
      break;
    case CELL_TYPE: {
      os << "Cell for ";
      HeapStringAllocator allocator;
      StringStream accumulator(&allocator);
      Cell::cast(this)->value()->ShortPrint(&accumulator);
      os << accumulator.ToCString().get();
      break;
    }
    case PROPERTY_CELL_TYPE: {
      os << "PropertyCell for ";
      HeapStringAllocator allocator;
      StringStream accumulator(&allocator);
      PropertyCell::cast(this)->value()->ShortPrint(&accumulator);
      os << accumulator.ToCString().get();
      break;
    }
    default:
      os << "<Other heap object (" << map()->instance_type() << ")>";
      break;
  }
}


void HeapObject::Iterate(ObjectVisitor* v) {
  // Handle header
  IteratePointer(v, kMapOffset);
  // Handle object body
  Map* m = map();
  IterateBody(m->instance_type(), SizeFromMap(m), v);
}


void HeapObject::IterateBody(InstanceType type, int object_size,
                             ObjectVisitor* v) {
  // Avoiding <Type>::cast(this) because it accesses the map pointer field.
  // During GC, the map pointer field is encoded.
  if (type < FIRST_NONSTRING_TYPE) {
    switch (type & kStringRepresentationMask) {
      case kSeqStringTag:
        break;
      case kConsStringTag:
        ConsString::BodyDescriptor::IterateBody(this, v);
        break;
      case kSlicedStringTag:
        SlicedString::BodyDescriptor::IterateBody(this, v);
        break;
      case kExternalStringTag:
        if ((type & kStringEncodingMask) == kOneByteStringTag) {
          reinterpret_cast<ExternalAsciiString*>(this)->
              ExternalAsciiStringIterateBody(v);
        } else {
          reinterpret_cast<ExternalTwoByteString*>(this)->
              ExternalTwoByteStringIterateBody(v);
        }
        break;
    }
    return;
  }

  switch (type) {
    case FIXED_ARRAY_TYPE:
      FixedArray::BodyDescriptor::IterateBody(this, object_size, v);
      break;
    case CONSTANT_POOL_ARRAY_TYPE:
      reinterpret_cast<ConstantPoolArray*>(this)->ConstantPoolIterateBody(v);
      break;
    case FIXED_DOUBLE_ARRAY_TYPE:
      break;
    case JS_OBJECT_TYPE:
    case JS_CONTEXT_EXTENSION_OBJECT_TYPE:
    case JS_GENERATOR_OBJECT_TYPE:
    case JS_MODULE_TYPE:
    case JS_VALUE_TYPE:
    case JS_DATE_TYPE:
    case JS_ARRAY_TYPE:
    case JS_ARRAY_BUFFER_TYPE:
    case JS_TYPED_ARRAY_TYPE:
    case JS_DATA_VIEW_TYPE:
    case JS_SET_TYPE:
    case JS_MAP_TYPE:
    case JS_SET_ITERATOR_TYPE:
    case JS_MAP_ITERATOR_TYPE:
    case JS_WEAK_MAP_TYPE:
    case JS_WEAK_SET_TYPE:
    case JS_REGEXP_TYPE:
    case JS_GLOBAL_PROXY_TYPE:
    case JS_GLOBAL_OBJECT_TYPE:
    case JS_BUILTINS_OBJECT_TYPE:
    case JS_MESSAGE_OBJECT_TYPE:
      JSObject::BodyDescriptor::IterateBody(this, object_size, v);
      break;
    case JS_FUNCTION_TYPE:
      reinterpret_cast<JSFunction*>(this)
          ->JSFunctionIterateBody(object_size, v);
      break;
    case ODDBALL_TYPE:
      Oddball::BodyDescriptor::IterateBody(this, v);
      break;
    case JS_PROXY_TYPE:
      JSProxy::BodyDescriptor::IterateBody(this, v);
      break;
    case JS_FUNCTION_PROXY_TYPE:
      JSFunctionProxy::BodyDescriptor::IterateBody(this, v);
      break;
    case FOREIGN_TYPE:
      reinterpret_cast<Foreign*>(this)->ForeignIterateBody(v);
      break;
    case MAP_TYPE:
      Map::BodyDescriptor::IterateBody(this, v);
      break;
    case CODE_TYPE:
      reinterpret_cast<Code*>(this)->CodeIterateBody(v);
      break;
    case CELL_TYPE:
      Cell::BodyDescriptor::IterateBody(this, v);
      break;
    case PROPERTY_CELL_TYPE:
      PropertyCell::BodyDescriptor::IterateBody(this, v);
      break;
    case SYMBOL_TYPE:
      Symbol::BodyDescriptor::IterateBody(this, v);
      break;

    case HEAP_NUMBER_TYPE:
    case MUTABLE_HEAP_NUMBER_TYPE:
    case FILLER_TYPE:
    case BYTE_ARRAY_TYPE:
    case FREE_SPACE_TYPE:
      break;

#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size)                        \
    case EXTERNAL_##TYPE##_ARRAY_TYPE:                                         \
    case FIXED_##TYPE##_ARRAY_TYPE:                                            \
      break;

    TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE

    case SHARED_FUNCTION_INFO_TYPE: {
      SharedFunctionInfo::BodyDescriptor::IterateBody(this, v);
      break;
    }

#define MAKE_STRUCT_CASE(NAME, Name, name) \
        case NAME##_TYPE:
      STRUCT_LIST(MAKE_STRUCT_CASE)
#undef MAKE_STRUCT_CASE
      if (type == ALLOCATION_SITE_TYPE) {
        AllocationSite::BodyDescriptor::IterateBody(this, v);
      } else {
        StructBodyDescriptor::IterateBody(this, object_size, v);
      }
      break;
    default:
      PrintF("Unknown type: %d\n", type);
      UNREACHABLE();
  }
}


bool HeapNumber::HeapNumberBooleanValue() {
  return DoubleToBoolean(value());
}


void HeapNumber::HeapNumberPrint(OStream& os) {  // NOLINT
  os << value();
}


String* JSReceiver::class_name() {
  if (IsJSFunction() || IsJSFunctionProxy()) {
    return GetHeap()->function_class_string();
  }
  if (map()->constructor()->IsJSFunction()) {
    JSFunction* constructor = JSFunction::cast(map()->constructor());
    return String::cast(constructor->shared()->instance_class_name());
  }
  // If the constructor is not present, return "Object".
  return GetHeap()->Object_string();
}


String* Map::constructor_name() {
  if (constructor()->IsJSFunction()) {
    JSFunction* constructor = JSFunction::cast(this->constructor());
    String* name = String::cast(constructor->shared()->name());
    if (name->length() > 0) return name;
    String* inferred_name = constructor->shared()->inferred_name();
    if (inferred_name->length() > 0) return inferred_name;
    Object* proto = prototype();
    if (proto->IsJSObject()) return JSObject::cast(proto)->constructor_name();
  }
  // TODO(rossberg): what about proxies?
  // If the constructor is not present, return "Object".
  return GetHeap()->Object_string();
}


String* JSReceiver::constructor_name() {
  return map()->constructor_name();
}


MaybeHandle<Map> Map::CopyWithField(Handle<Map> map,
                                    Handle<Name> name,
                                    Handle<HeapType> type,
                                    PropertyAttributes attributes,
                                    Representation representation,
                                    TransitionFlag flag) {
  DCHECK(DescriptorArray::kNotFound ==
         map->instance_descriptors()->Search(
             *name, map->NumberOfOwnDescriptors()));

  // Ensure the descriptor array does not get too big.
  if (map->NumberOfOwnDescriptors() >= kMaxNumberOfDescriptors) {
    return MaybeHandle<Map>();
  }

  Isolate* isolate = map->GetIsolate();

  // Compute the new index for new field.
  int index = map->NextFreePropertyIndex();

  if (map->instance_type() == JS_CONTEXT_EXTENSION_OBJECT_TYPE) {
    representation = Representation::Tagged();
    type = HeapType::Any(isolate);
  }

  FieldDescriptor new_field_desc(name, index, type, attributes, representation);
  Handle<Map> new_map = Map::CopyAddDescriptor(map, &new_field_desc, flag);
  int unused_property_fields = new_map->unused_property_fields() - 1;
  if (unused_property_fields < 0) {
    unused_property_fields += JSObject::kFieldsAdded;
  }
  new_map->set_unused_property_fields(unused_property_fields);
  return new_map;
}


MaybeHandle<Map> Map::CopyWithConstant(Handle<Map> map,
                                       Handle<Name> name,
                                       Handle<Object> constant,
                                       PropertyAttributes attributes,
                                       TransitionFlag flag) {
  // Ensure the descriptor array does not get too big.
  if (map->NumberOfOwnDescriptors() >= kMaxNumberOfDescriptors) {
    return MaybeHandle<Map>();
  }

  // Allocate new instance descriptors with (name, constant) added.
  ConstantDescriptor new_constant_desc(name, constant, attributes);
  return Map::CopyAddDescriptor(map, &new_constant_desc, flag);
}


void JSObject::AddFastProperty(Handle<JSObject> object,
                               Handle<Name> name,
                               Handle<Object> value,
                               PropertyAttributes attributes,
                               StoreFromKeyed store_mode,
                               TransitionFlag flag) {
  DCHECK(!object->IsJSGlobalProxy());

  MaybeHandle<Map> maybe_map;
  if (value->IsJSFunction()) {
    maybe_map = Map::CopyWithConstant(
        handle(object->map()), name, value, attributes, flag);
  } else if (!object->map()->TooManyFastProperties(store_mode)) {
    Isolate* isolate = object->GetIsolate();
    Representation representation = value->OptimalRepresentation();
    maybe_map = Map::CopyWithField(
        handle(object->map(), isolate), name,
        value->OptimalType(isolate, representation),
        attributes, representation, flag);
  }

  Handle<Map> new_map;
  if (!maybe_map.ToHandle(&new_map)) {
    NormalizeProperties(object, CLEAR_INOBJECT_PROPERTIES, 0);
    return;
  }

  JSObject::MigrateToNewProperty(object, new_map, value);
}


void JSObject::AddSlowProperty(Handle<JSObject> object,
                               Handle<Name> name,
                               Handle<Object> value,
                               PropertyAttributes attributes) {
  DCHECK(!object->HasFastProperties());
  Isolate* isolate = object->GetIsolate();
  Handle<NameDictionary> dict(object->property_dictionary());
  if (object->IsGlobalObject()) {
    // In case name is an orphaned property reuse the cell.
    int entry = dict->FindEntry(name);
    if (entry != NameDictionary::kNotFound) {
      Handle<PropertyCell> cell(PropertyCell::cast(dict->ValueAt(entry)));
      PropertyCell::SetValueInferType(cell, value);
      // Assign an enumeration index to the property and update
      // SetNextEnumerationIndex.
      int index = dict->NextEnumerationIndex();
      PropertyDetails details = PropertyDetails(attributes, NORMAL, index);
      dict->SetNextEnumerationIndex(index + 1);
      dict->SetEntry(entry, name, cell, details);
      return;
    }
    Handle<PropertyCell> cell = isolate->factory()->NewPropertyCell(value);
    PropertyCell::SetValueInferType(cell, value);
    value = cell;
  }
  PropertyDetails details = PropertyDetails(attributes, NORMAL, 0);
  Handle<NameDictionary> result =
      NameDictionary::Add(dict, name, value, details);
  if (*dict != *result) object->set_properties(*result);
}


MaybeHandle<Object> JSObject::AddPropertyInternal(
    Handle<JSObject> object, Handle<Name> name, Handle<Object> value,
    PropertyAttributes attributes, JSReceiver::StoreFromKeyed store_mode,
    ExtensibilityCheck extensibility_check, TransitionFlag transition_flag) {
  DCHECK(!object->IsJSGlobalProxy());
  Isolate* isolate = object->GetIsolate();

  if (!name->IsUniqueName()) {
    name = isolate->factory()->InternalizeString(
        Handle<String>::cast(name));
  }

  if (extensibility_check == PERFORM_EXTENSIBILITY_CHECK &&
      !object->map()->is_extensible()) {
    Handle<Object> args[1] = {name};
    Handle<Object> error = isolate->factory()->NewTypeError(
        "object_not_extensible", HandleVector(args, ARRAY_SIZE(args)));
    return isolate->Throw<Object>(error);
  }

  if (object->HasFastProperties()) {
    AddFastProperty(object, name, value, attributes, store_mode,
                    transition_flag);
  }

  if (!object->HasFastProperties()) {
    AddSlowProperty(object, name, value, attributes);
  }

  if (object->map()->is_observed() &&
      *name != isolate->heap()->hidden_string()) {
    Handle<Object> old_value = isolate->factory()->the_hole_value();
    EnqueueChangeRecord(object, "add", name, old_value);
  }

  return value;
}


Context* JSObject::GetCreationContext() {
  Object* constructor = this->map()->constructor();
  JSFunction* function;
  if (!constructor->IsJSFunction()) {
    // Functions have null as a constructor,
    // but any JSFunction knows its context immediately.
    function = JSFunction::cast(this);
  } else {
    function = JSFunction::cast(constructor);
  }

  return function->context()->native_context();
}


void JSObject::EnqueueChangeRecord(Handle<JSObject> object,
                                   const char* type_str,
                                   Handle<Name> name,
                                   Handle<Object> old_value) {
  DCHECK(!object->IsJSGlobalProxy());
  DCHECK(!object->IsJSGlobalObject());
  Isolate* isolate = object->GetIsolate();
  HandleScope scope(isolate);
  Handle<String> type = isolate->factory()->InternalizeUtf8String(type_str);
  Handle<Object> args[] = { type, object, name, old_value };
  int argc = name.is_null() ? 2 : old_value->IsTheHole() ? 3 : 4;

  Execution::Call(isolate,
                  Handle<JSFunction>(isolate->observers_notify_change()),
                  isolate->factory()->undefined_value(),
                  argc, args).Assert();
}


static void ReplaceSlowProperty(Handle<JSObject> object,
                                Handle<Name> name,
                                Handle<Object> value,
                                PropertyAttributes attributes) {
  NameDictionary* dictionary = object->property_dictionary();
  int old_index = dictionary->FindEntry(name);
  int new_enumeration_index = 0;  // 0 means "Use the next available index."
  if (old_index != -1) {
    // All calls to ReplaceSlowProperty have had all transitions removed.
    new_enumeration_index = dictionary->DetailsAt(old_index).dictionary_index();
  }

  PropertyDetails new_details(attributes, NORMAL, new_enumeration_index);
  JSObject::SetNormalizedProperty(object, name, value, new_details);
}


const char* Representation::Mnemonic() const {
  switch (kind_) {
    case kNone: return "v";
    case kTagged: return "t";
    case kSmi: return "s";
    case kDouble: return "d";
    case kInteger32: return "i";
    case kHeapObject: return "h";
    case kExternal: return "x";
    default:
      UNREACHABLE();
      return NULL;
  }
}


bool Map::InstancesNeedRewriting(Map* target, int target_number_of_fields,
                                 int target_inobject, int target_unused,
                                 int* old_number_of_fields) {
  // If fields were added (or removed), rewrite the instance.
  *old_number_of_fields = NumberOfFields();
  DCHECK(target_number_of_fields >= *old_number_of_fields);
  if (target_number_of_fields != *old_number_of_fields) return true;

  // If smi descriptors were replaced by double descriptors, rewrite.
  DescriptorArray* old_desc = instance_descriptors();
  DescriptorArray* new_desc = target->instance_descriptors();
  int limit = NumberOfOwnDescriptors();
  for (int i = 0; i < limit; i++) {
    if (new_desc->GetDetails(i).representation().IsDouble() !=
        old_desc->GetDetails(i).representation().IsDouble()) {
      return true;
    }
  }

  // If no fields were added, and no inobject properties were removed, setting
  // the map is sufficient.
  if (target_inobject == inobject_properties()) return false;
  // In-object slack tracking may have reduced the object size of the new map.
  // In that case, succeed if all existing fields were inobject, and they still
  // fit within the new inobject size.
  DCHECK(target_inobject < inobject_properties());
  if (target_number_of_fields <= target_inobject) {
    DCHECK(target_number_of_fields + target_unused == target_inobject);
    return false;
  }
  // Otherwise, properties will need to be moved to the backing store.
  return true;
}


void Map::ConnectElementsTransition(Handle<Map> parent, Handle<Map> child) {
  Isolate* isolate = parent->GetIsolate();
  Handle<Name> name = isolate->factory()->elements_transition_symbol();
  ConnectTransition(parent, child, name, FULL_TRANSITION);
}


void JSObject::MigrateToMap(Handle<JSObject> object, Handle<Map> new_map) {
  if (object->map() == *new_map) return;
  if (object->HasFastProperties()) {
    if (!new_map->is_dictionary_map()) {
      Handle<Map> old_map(object->map());
      MigrateFastToFast(object, new_map);
      if (old_map->is_prototype_map()) {
        // Clear out the old descriptor array to avoid problems to sharing
        // the descriptor array without using an explicit.
        old_map->InitializeDescriptors(
            old_map->GetHeap()->empty_descriptor_array());
        // Ensure that no transition was inserted for prototype migrations.
        DCHECK(!old_map->HasTransitionArray());
        DCHECK(new_map->GetBackPointer()->IsUndefined());
      }
    } else {
      MigrateFastToSlow(object, new_map, 0);
    }
  } else {
    // For slow-to-fast migrations JSObject::TransformToFastProperties()
    // must be used instead.
    CHECK(new_map->is_dictionary_map());

    // Slow-to-slow migration is trivial.
    object->set_map(*new_map);
  }
}


// To migrate a fast instance to a fast map:
// - First check whether the instance needs to be rewritten. If not, simply
//   change the map.
// - Otherwise, allocate a fixed array large enough to hold all fields, in
//   addition to unused space.
// - Copy all existing properties in, in the following order: backing store
//   properties, unused fields, inobject properties.
// - If all allocation succeeded, commit the state atomically:
//   * Copy inobject properties from the backing store back into the object.
//   * Trim the difference in instance size of the object. This also cleanly
//     frees inobject properties that moved to the backing store.
//   * If there are properties left in the backing store, trim of the space used
//     to temporarily store the inobject properties.
//   * If there are properties left in the backing store, install the backing
//     store.
void JSObject::MigrateFastToFast(Handle<JSObject> object, Handle<Map> new_map) {
  Isolate* isolate = object->GetIsolate();
  Handle<Map> old_map(object->map());
  int old_number_of_fields;
  int number_of_fields = new_map->NumberOfFields();
  int inobject = new_map->inobject_properties();
  int unused = new_map->unused_property_fields();

  // Nothing to do if no functions were converted to fields and no smis were
  // converted to doubles.
  if (!old_map->InstancesNeedRewriting(*new_map, number_of_fields, inobject,
                                       unused, &old_number_of_fields)) {
    object->synchronized_set_map(*new_map);
    return;
  }

  int total_size = number_of_fields + unused;
  int external = total_size - inobject;

  if ((old_map->unused_property_fields() == 0) &&
      (number_of_fields != old_number_of_fields) &&
      (new_map->GetBackPointer() == *old_map)) {
    DCHECK(number_of_fields == old_number_of_fields + 1);
    // This migration is a transition from a map that has run out out property
    // space. Therefore it could be done by extending the backing store.
    Handle<FixedArray> old_storage = handle(object->properties(), isolate);
    Handle<FixedArray> new_storage =
        FixedArray::CopySize(old_storage, external);

    // Properly initialize newly added property.
    PropertyDetails details = new_map->GetLastDescriptorDetails();
    Handle<Object> value;
    if (details.representation().IsDouble()) {
      value = isolate->factory()->NewHeapNumber(0, MUTABLE);
    } else {
      value = isolate->factory()->uninitialized_value();
    }
    DCHECK(details.type() == FIELD);
    int target_index = details.field_index() - inobject;
    DCHECK(target_index >= 0);  // Must be a backing store index.
    new_storage->set(target_index, *value);

    // From here on we cannot fail and we shouldn't GC anymore.
    DisallowHeapAllocation no_allocation;

    // Set the new property value and do the map transition.
    object->set_properties(*new_storage);
    object->synchronized_set_map(*new_map);
    return;
  }
  Handle<FixedArray> array = isolate->factory()->NewFixedArray(total_size);

  Handle<DescriptorArray> old_descriptors(old_map->instance_descriptors());
  Handle<DescriptorArray> new_descriptors(new_map->instance_descriptors());
  int old_nof = old_map->NumberOfOwnDescriptors();
  int new_nof = new_map->NumberOfOwnDescriptors();

  // This method only supports generalizing instances to at least the same
  // number of properties.
  DCHECK(old_nof <= new_nof);

  for (int i = 0; i < old_nof; i++) {
    PropertyDetails details = new_descriptors->GetDetails(i);
    if (details.type() != FIELD) continue;
    PropertyDetails old_details = old_descriptors->GetDetails(i);
    if (old_details.type() == CALLBACKS) {
      DCHECK(details.representation().IsTagged());
      continue;
    }
    DCHECK(old_details.type() == CONSTANT ||
           old_details.type() == FIELD);
    Object* raw_value = old_details.type() == CONSTANT
        ? old_descriptors->GetValue(i)
        : object->RawFastPropertyAt(FieldIndex::ForDescriptor(*old_map, i));
    Handle<Object> value(raw_value, isolate);
    if (!old_details.representation().IsDouble() &&
        details.representation().IsDouble()) {
      if (old_details.representation().IsNone()) {
        value = handle(Smi::FromInt(0), isolate);
      }
      value = Object::NewStorageFor(isolate, value, details.representation());
    } else if (old_details.representation().IsDouble() &&
               !details.representation().IsDouble()) {
      value = Object::WrapForRead(isolate, value, old_details.representation());
    }
    DCHECK(!(details.representation().IsDouble() && value->IsSmi()));
    int target_index = new_descriptors->GetFieldIndex(i) - inobject;
    if (target_index < 0) target_index += total_size;
    array->set(target_index, *value);
  }

  for (int i = old_nof; i < new_nof; i++) {
    PropertyDetails details = new_descriptors->GetDetails(i);
    if (details.type() != FIELD) continue;
    Handle<Object> value;
    if (details.representation().IsDouble()) {
      value = isolate->factory()->NewHeapNumber(0, MUTABLE);
    } else {
      value = isolate->factory()->uninitialized_value();
    }
    int target_index = new_descriptors->GetFieldIndex(i) - inobject;
    if (target_index < 0) target_index += total_size;
    array->set(target_index, *value);
  }

  // From here on we cannot fail and we shouldn't GC anymore.
  DisallowHeapAllocation no_allocation;

  // Copy (real) inobject properties. If necessary, stop at number_of_fields to
  // avoid overwriting |one_pointer_filler_map|.
  int limit = Min(inobject, number_of_fields);
  for (int i = 0; i < limit; i++) {
    FieldIndex index = FieldIndex::ForPropertyIndex(*new_map, i);
    object->FastPropertyAtPut(index, array->get(external + i));
  }

  Heap* heap = isolate->heap();

  // If there are properties in the new backing store, trim it to the correct
  // size and install the backing store into the object.
  if (external > 0) {
    heap->RightTrimFixedArray<Heap::FROM_MUTATOR>(*array, inobject);
    object->set_properties(*array);
  }

  // Create filler object past the new instance size.
  int new_instance_size = new_map->instance_size();
  int instance_size_delta = old_map->instance_size() - new_instance_size;
  DCHECK(instance_size_delta >= 0);

  if (instance_size_delta > 0) {
    Address address = object->address();
    heap->CreateFillerObjectAt(
        address + new_instance_size, instance_size_delta);
    heap->AdjustLiveBytes(address, -instance_size_delta, Heap::FROM_MUTATOR);
  }

  // We are storing the new map using release store after creating a filler for
  // the left-over space to avoid races with the sweeper thread.
  object->synchronized_set_map(*new_map);
}


void JSObject::GeneralizeFieldRepresentation(Handle<JSObject> object,
                                             int modify_index,
                                             Representation new_representation,
                                             Handle<HeapType> new_field_type) {
  Handle<Map> new_map = Map::GeneralizeRepresentation(
      handle(object->map()), modify_index, new_representation, new_field_type,
      FORCE_FIELD);
  MigrateToMap(object, new_map);
}


int Map::NumberOfFields() {
  DescriptorArray* descriptors = instance_descriptors();
  int result = 0;
  for (int i = 0; i < NumberOfOwnDescriptors(); i++) {
    if (descriptors->GetDetails(i).type() == FIELD) result++;
  }
  return result;
}


Handle<Map> Map::CopyGeneralizeAllRepresentations(Handle<Map> map,
                                                  int modify_index,
                                                  StoreMode store_mode,
                                                  PropertyAttributes attributes,
                                                  const char* reason) {
  Isolate* isolate = map->GetIsolate();
  Handle<Map> new_map = Copy(map);

  DescriptorArray* descriptors = new_map->instance_descriptors();
  int length = descriptors->number_of_descriptors();
  for (int i = 0; i < length; i++) {
    descriptors->SetRepresentation(i, Representation::Tagged());
    if (descriptors->GetDetails(i).type() == FIELD) {
      descriptors->SetValue(i, HeapType::Any());
    }
  }

  // Unless the instance is being migrated, ensure that modify_index is a field.
  PropertyDetails details = descriptors->GetDetails(modify_index);
  if (store_mode == FORCE_FIELD &&
      (details.type() != FIELD || details.attributes() != attributes)) {
    int field_index = details.type() == FIELD ? details.field_index()
                                              : new_map->NumberOfFields();
    FieldDescriptor d(handle(descriptors->GetKey(modify_index), isolate),
                      field_index, attributes, Representation::Tagged());
    descriptors->Replace(modify_index, &d);
    if (details.type() != FIELD) {
      int unused_property_fields = new_map->unused_property_fields() - 1;
      if (unused_property_fields < 0) {
        unused_property_fields += JSObject::kFieldsAdded;
      }
      new_map->set_unused_property_fields(unused_property_fields);
    }
  } else {
    DCHECK(details.attributes() == attributes);
  }

  if (FLAG_trace_generalization) {
    HeapType* field_type = (details.type() == FIELD)
        ? map->instance_descriptors()->GetFieldType(modify_index)
        : NULL;
    map->PrintGeneralization(stdout, reason, modify_index,
                        new_map->NumberOfOwnDescriptors(),
                        new_map->NumberOfOwnDescriptors(),
                        details.type() == CONSTANT && store_mode == FORCE_FIELD,
                        details.representation(), Representation::Tagged(),
                        field_type, HeapType::Any());
  }
  return new_map;
}


// static
Handle<Map> Map::CopyGeneralizeAllRepresentations(Handle<Map> map,
                                                  int modify_index,
                                                  StoreMode store_mode,
                                                  const char* reason) {
  PropertyDetails details =
      map->instance_descriptors()->GetDetails(modify_index);
  return CopyGeneralizeAllRepresentations(map, modify_index, store_mode,
                                          details.attributes(), reason);
}


void Map::DeprecateTransitionTree() {
  if (is_deprecated()) return;
  if (HasTransitionArray()) {
    TransitionArray* transitions = this->transitions();
    for (int i = 0; i < transitions->number_of_transitions(); i++) {
      transitions->GetTarget(i)->DeprecateTransitionTree();
    }
  }
  deprecate();
  dependent_code()->DeoptimizeDependentCodeGroup(
      GetIsolate(), DependentCode::kTransitionGroup);
  NotifyLeafMapLayoutChange();
}


// Invalidates a transition target at |key|, and installs |new_descriptors| over
// the current instance_descriptors to ensure proper sharing of descriptor
// arrays.
void Map::DeprecateTarget(Name* key, DescriptorArray* new_descriptors) {
  if (HasTransitionArray()) {
    TransitionArray* transitions = this->transitions();
    int transition = transitions->Search(key);
    if (transition != TransitionArray::kNotFound) {
      transitions->GetTarget(transition)->DeprecateTransitionTree();
    }
  }

  // Don't overwrite the empty descriptor array.
  if (NumberOfOwnDescriptors() == 0) return;

  DescriptorArray* to_replace = instance_descriptors();
  Map* current = this;
  GetHeap()->incremental_marking()->RecordWrites(to_replace);
  while (current->instance_descriptors() == to_replace) {
    current->SetEnumLength(kInvalidEnumCacheSentinel);
    current->set_instance_descriptors(new_descriptors);
    Object* next = current->GetBackPointer();
    if (next->IsUndefined()) break;
    current = Map::cast(next);
  }

  set_owns_descriptors(false);
}


Map* Map::FindRootMap() {
  Map* result = this;
  while (true) {
    Object* back = result->GetBackPointer();
    if (back->IsUndefined()) return result;
    result = Map::cast(back);
  }
}


Map* Map::FindLastMatchMap(int verbatim,
                           int length,
                           DescriptorArray* descriptors) {
  DisallowHeapAllocation no_allocation;

  // This can only be called on roots of transition trees.
  DCHECK(GetBackPointer()->IsUndefined());

  Map* current = this;

  for (int i = verbatim; i < length; i++) {
    if (!current->HasTransitionArray()) break;
    Name* name = descriptors->GetKey(i);
    TransitionArray* transitions = current->transitions();
    int transition = transitions->Search(name);
    if (transition == TransitionArray::kNotFound) break;

    Map* next = transitions->GetTarget(transition);
    DescriptorArray* next_descriptors = next->instance_descriptors();

    PropertyDetails details = descriptors->GetDetails(i);
    PropertyDetails next_details = next_descriptors->GetDetails(i);
    if (details.type() != next_details.type()) break;
    if (details.attributes() != next_details.attributes()) break;
    if (!details.representation().Equals(next_details.representation())) break;
    if (next_details.type() == FIELD) {
      if (!descriptors->GetFieldType(i)->NowIs(
              next_descriptors->GetFieldType(i))) break;
    } else {
      if (descriptors->GetValue(i) != next_descriptors->GetValue(i)) break;
    }

    current = next;
  }
  return current;
}


Map* Map::FindFieldOwner(int descriptor) {
  DisallowHeapAllocation no_allocation;
  DCHECK_EQ(FIELD, instance_descriptors()->GetDetails(descriptor).type());
  Map* result = this;
  while (true) {
    Object* back = result->GetBackPointer();
    if (back->IsUndefined()) break;
    Map* parent = Map::cast(back);
    if (parent->NumberOfOwnDescriptors() <= descriptor) break;
    result = parent;
  }
  return result;
}


void Map::UpdateFieldType(int descriptor, Handle<Name> name,
                          Handle<HeapType> new_type) {
  DisallowHeapAllocation no_allocation;
  PropertyDetails details = instance_descriptors()->GetDetails(descriptor);
  if (details.type() != FIELD) return;
  if (HasTransitionArray()) {
    TransitionArray* transitions = this->transitions();
    for (int i = 0; i < transitions->number_of_transitions(); ++i) {
      transitions->GetTarget(i)->UpdateFieldType(descriptor, name, new_type);
    }
  }
  // Skip if already updated the shared descriptor.
  if (instance_descriptors()->GetFieldType(descriptor) == *new_type) return;
  FieldDescriptor d(name, instance_descriptors()->GetFieldIndex(descriptor),
                    new_type, details.attributes(), details.representation());
  instance_descriptors()->Replace(descriptor, &d);
}


// static
Handle<HeapType> Map::GeneralizeFieldType(Handle<HeapType> type1,
                                          Handle<HeapType> type2,
                                          Isolate* isolate) {
  static const int kMaxClassesPerFieldType = 5;
  if (type1->NowIs(type2)) return type2;
  if (type2->NowIs(type1)) return type1;
  if (type1->NowStable() && type2->NowStable()) {
    Handle<HeapType> type = HeapType::Union(type1, type2, isolate);
    if (type->NumClasses() <= kMaxClassesPerFieldType) {
      DCHECK(type->NowStable());
      DCHECK(type1->NowIs(type));
      DCHECK(type2->NowIs(type));
      return type;
    }
  }
  return HeapType::Any(isolate);
}


// static
void Map::GeneralizeFieldType(Handle<Map> map,
                              int modify_index,
                              Handle<HeapType> new_field_type) {
  Isolate* isolate = map->GetIsolate();

  // Check if we actually need to generalize the field type at all.
  Handle<HeapType> old_field_type(
      map->instance_descriptors()->GetFieldType(modify_index), isolate);
  if (new_field_type->NowIs(old_field_type)) {
    DCHECK(Map::GeneralizeFieldType(old_field_type,
                                    new_field_type,
                                    isolate)->NowIs(old_field_type));
    return;
  }

  // Determine the field owner.
  Handle<Map> field_owner(map->FindFieldOwner(modify_index), isolate);
  Handle<DescriptorArray> descriptors(
      field_owner->instance_descriptors(), isolate);
  DCHECK_EQ(*old_field_type, descriptors->GetFieldType(modify_index));

  // Determine the generalized new field type.
  new_field_type = Map::GeneralizeFieldType(
      old_field_type, new_field_type, isolate);

  PropertyDetails details = descriptors->GetDetails(modify_index);
  Handle<Name> name(descriptors->GetKey(modify_index));
  field_owner->UpdateFieldType(modify_index, name, new_field_type);
  field_owner->dependent_code()->DeoptimizeDependentCodeGroup(
      isolate, DependentCode::kFieldTypeGroup);

  if (FLAG_trace_generalization) {
    map->PrintGeneralization(
        stdout, "field type generalization",
        modify_index, map->NumberOfOwnDescriptors(),
        map->NumberOfOwnDescriptors(), false,
        details.representation(), details.representation(),
        *old_field_type, *new_field_type);
  }
}


// Generalize the representation of the descriptor at |modify_index|.
// This method rewrites the transition tree to reflect the new change. To avoid
// high degrees over polymorphism, and to stabilize quickly, on every rewrite
// the new type is deduced by merging the current type with any potential new
// (partial) version of the type in the transition tree.
// To do this, on each rewrite:
// - Search the root of the transition tree using FindRootMap.
// - Find |target_map|, the newest matching version of this map using the keys
//   in the |old_map|'s descriptor array to walk the transition tree.
// - Merge/generalize the descriptor array of the |old_map| and |target_map|.
// - Generalize the |modify_index| descriptor using |new_representation| and
//   |new_field_type|.
// - Walk the tree again starting from the root towards |target_map|. Stop at
//   |split_map|, the first map who's descriptor array does not match the merged
//   descriptor array.
// - If |target_map| == |split_map|, |target_map| is in the expected state.
//   Return it.
// - Otherwise, invalidate the outdated transition target from |target_map|, and
//   replace its transition tree with a new branch for the updated descriptors.
Handle<Map> Map::GeneralizeRepresentation(Handle<Map> old_map,
                                          int modify_index,
                                          Representation new_representation,
                                          Handle<HeapType> new_field_type,
                                          StoreMode store_mode) {
  Isolate* isolate = old_map->GetIsolate();

  Handle<DescriptorArray> old_descriptors(
      old_map->instance_descriptors(), isolate);
  int old_nof = old_map->NumberOfOwnDescriptors();
  PropertyDetails old_details = old_descriptors->GetDetails(modify_index);
  Representation old_representation = old_details.representation();

  // It's fine to transition from None to anything but double without any
  // modification to the object, because the default uninitialized value for
  // representation None can be overwritten by both smi and tagged values.
  // Doubles, however, would require a box allocation.
  if (old_representation.IsNone() &&
      !new_representation.IsNone() &&
      !new_representation.IsDouble()) {
    DCHECK(old_details.type() == FIELD);
    DCHECK(old_descriptors->GetFieldType(modify_index)->NowIs(
            HeapType::None()));
    if (FLAG_trace_generalization) {
      old_map->PrintGeneralization(
          stdout, "uninitialized field",
          modify_index, old_map->NumberOfOwnDescriptors(),
          old_map->NumberOfOwnDescriptors(), false,
          old_representation, new_representation,
          old_descriptors->GetFieldType(modify_index), *new_field_type);
    }
    old_descriptors->SetRepresentation(modify_index, new_representation);
    old_descriptors->SetValue(modify_index, *new_field_type);
    return old_map;
  }

  // Check the state of the root map.
  Handle<Map> root_map(old_map->FindRootMap(), isolate);
  if (!old_map->EquivalentToForTransition(*root_map)) {
    return CopyGeneralizeAllRepresentations(
        old_map, modify_index, store_mode, "not equivalent");
  }
  int root_nof = root_map->NumberOfOwnDescriptors();
  if (modify_index < root_nof) {
    PropertyDetails old_details = old_descriptors->GetDetails(modify_index);
    if ((old_details.type() != FIELD && store_mode == FORCE_FIELD) ||
        (old_details.type() == FIELD &&
         (!new_field_type->NowIs(old_descriptors->GetFieldType(modify_index)) ||
          !new_representation.fits_into(old_details.representation())))) {
      return CopyGeneralizeAllRepresentations(
          old_map, modify_index, store_mode, "root modification");
    }
  }

  Handle<Map> target_map = root_map;
  for (int i = root_nof; i < old_nof; ++i) {
    int j = target_map->SearchTransition(old_descriptors->GetKey(i));
    if (j == TransitionArray::kNotFound) break;
    Handle<Map> tmp_map(target_map->GetTransition(j), isolate);
    Handle<DescriptorArray> tmp_descriptors = handle(
        tmp_map->instance_descriptors(), isolate);

    // Check if target map is incompatible.
    PropertyDetails old_details = old_descriptors->GetDetails(i);
    PropertyDetails tmp_details = tmp_descriptors->GetDetails(i);
    PropertyType old_type = old_details.type();
    PropertyType tmp_type = tmp_details.type();
    if (tmp_details.attributes() != old_details.attributes() ||
        ((tmp_type == CALLBACKS || old_type == CALLBACKS) &&
         (tmp_type != old_type ||
          tmp_descriptors->GetValue(i) != old_descriptors->GetValue(i)))) {
      return CopyGeneralizeAllRepresentations(
          old_map, modify_index, store_mode, "incompatible");
    }
    Representation old_representation = old_details.representation();
    Representation tmp_representation = tmp_details.representation();
    if (!old_representation.fits_into(tmp_representation) ||
        (!new_representation.fits_into(tmp_representation) &&
         modify_index == i)) {
      break;
    }
    if (tmp_type == FIELD) {
      // Generalize the field type as necessary.
      Handle<HeapType> old_field_type = (old_type == FIELD)
          ? handle(old_descriptors->GetFieldType(i), isolate)
          : old_descriptors->GetValue(i)->OptimalType(
              isolate, tmp_representation);
      if (modify_index == i) {
        old_field_type = GeneralizeFieldType(
            new_field_type, old_field_type, isolate);
      }
      GeneralizeFieldType(tmp_map, i, old_field_type);
    } else if (tmp_type == CONSTANT) {
      if (old_type != CONSTANT ||
          old_descriptors->GetConstant(i) != tmp_descriptors->GetConstant(i)) {
        break;
      }
    } else {
      DCHECK_EQ(tmp_type, old_type);
      DCHECK_EQ(tmp_descriptors->GetValue(i), old_descriptors->GetValue(i));
    }
    target_map = tmp_map;
  }

  // Directly change the map if the target map is more general.
  Handle<DescriptorArray> target_descriptors(
      target_map->instance_descriptors(), isolate);
  int target_nof = target_map->NumberOfOwnDescriptors();
  if (target_nof == old_nof &&
      (store_mode != FORCE_FIELD ||
       target_descriptors->GetDetails(modify_index).type() == FIELD)) {
    DCHECK(modify_index < target_nof);
    DCHECK(new_representation.fits_into(
            target_descriptors->GetDetails(modify_index).representation()));
    DCHECK(target_descriptors->GetDetails(modify_index).type() != FIELD ||
           new_field_type->NowIs(
               target_descriptors->GetFieldType(modify_index)));
    return target_map;
  }

  // Find the last compatible target map in the transition tree.
  for (int i = target_nof; i < old_nof; ++i) {
    int j = target_map->SearchTransition(old_descriptors->GetKey(i));
    if (j == TransitionArray::kNotFound) break;
    Handle<Map> tmp_map(target_map->GetTransition(j), isolate);
    Handle<DescriptorArray> tmp_descriptors(
        tmp_map->instance_descriptors(), isolate);

    // Check if target map is compatible.
    PropertyDetails old_details = old_descriptors->GetDetails(i);
    PropertyDetails tmp_details = tmp_descriptors->GetDetails(i);
    if (tmp_details.attributes() != old_details.attributes() ||
        ((tmp_details.type() == CALLBACKS || old_details.type() == CALLBACKS) &&
         (tmp_details.type() != old_details.type() ||
          tmp_descriptors->GetValue(i) != old_descriptors->GetValue(i)))) {
      return CopyGeneralizeAllRepresentations(
          old_map, modify_index, store_mode, "incompatible");
    }
    target_map = tmp_map;
  }
  target_nof = target_map->NumberOfOwnDescriptors();
  target_descriptors = handle(target_map->instance_descriptors(), isolate);

  // Allocate a new descriptor array large enough to hold the required
  // descriptors, with minimally the exact same size as the old descriptor
  // array.
  int new_slack = Max(
      old_nof, old_descriptors->number_of_descriptors()) - old_nof;
  Handle<DescriptorArray> new_descriptors = DescriptorArray::Allocate(
      isolate, old_nof, new_slack);
  DCHECK(new_descriptors->length() > target_descriptors->length() ||
         new_descriptors->NumberOfSlackDescriptors() > 0 ||
         new_descriptors->number_of_descriptors() ==
         old_descriptors->number_of_descriptors());
  DCHECK(new_descriptors->number_of_descriptors() == old_nof);

  // 0 -> |root_nof|
  int current_offset = 0;
  for (int i = 0; i < root_nof; ++i) {
    PropertyDetails old_details = old_descriptors->GetDetails(i);
    if (old_details.type() == FIELD) current_offset++;
    Descriptor d(handle(old_descriptors->GetKey(i), isolate),
                 handle(old_descriptors->GetValue(i), isolate),
                 old_details);
    new_descriptors->Set(i, &d);
  }

  // |root_nof| -> |target_nof|
  for (int i = root_nof; i < target_nof; ++i) {
    Handle<Name> target_key(target_descriptors->GetKey(i), isolate);
    PropertyDetails old_details = old_descriptors->GetDetails(i);
    PropertyDetails target_details = target_descriptors->GetDetails(i);
    target_details = target_details.CopyWithRepresentation(
        old_details.representation().generalize(
            target_details.representation()));
    if (modify_index == i) {
      target_details = target_details.CopyWithRepresentation(
          new_representation.generalize(target_details.representation()));
    }
    DCHECK_EQ(old_details.attributes(), target_details.attributes());
    if (old_details.type() == FIELD ||
        target_details.type() == FIELD ||
        (modify_index == i && store_mode == FORCE_FIELD) ||
        (target_descriptors->GetValue(i) != old_descriptors->GetValue(i))) {
      Handle<HeapType> old_field_type = (old_details.type() == FIELD)
          ? handle(old_descriptors->GetFieldType(i), isolate)
          : old_descriptors->GetValue(i)->OptimalType(
              isolate, target_details.representation());
      Handle<HeapType> target_field_type = (target_details.type() == FIELD)
          ? handle(target_descriptors->GetFieldType(i), isolate)
          : target_descriptors->GetValue(i)->OptimalType(
              isolate, target_details.representation());
      target_field_type = GeneralizeFieldType(
          target_field_type, old_field_type, isolate);
      if (modify_index == i) {
        target_field_type = GeneralizeFieldType(
            target_field_type, new_field_type, isolate);
      }
      FieldDescriptor d(target_key,
                        current_offset++,
                        target_field_type,
                        target_details.attributes(),
                        target_details.representation());
      new_descriptors->Set(i, &d);
    } else {
      DCHECK_NE(FIELD, target_details.type());
      Descriptor d(target_key,
                   handle(target_descriptors->GetValue(i), isolate),
                   target_details);
      new_descriptors->Set(i, &d);
    }
  }

  // |target_nof| -> |old_nof|
  for (int i = target_nof; i < old_nof; ++i) {
    PropertyDetails old_details = old_descriptors->GetDetails(i);
    Handle<Name> old_key(old_descriptors->GetKey(i), isolate);
    if (modify_index == i) {
      old_details = old_details.CopyWithRepresentation(
          new_representation.generalize(old_details.representation()));
    }
    if (old_details.type() == FIELD) {
      Handle<HeapType> old_field_type(
          old_descriptors->GetFieldType(i), isolate);
      if (modify_index == i) {
        old_field_type = GeneralizeFieldType(
            old_field_type, new_field_type, isolate);
      }
      FieldDescriptor d(old_key,
                        current_offset++,
                        old_field_type,
                        old_details.attributes(),
                        old_details.representation());
      new_descriptors->Set(i, &d);
    } else {
      DCHECK(old_details.type() == CONSTANT || old_details.type() == CALLBACKS);
      if (modify_index == i && store_mode == FORCE_FIELD) {
        FieldDescriptor d(old_key,
                          current_offset++,
                          GeneralizeFieldType(
                              old_descriptors->GetValue(i)->OptimalType(
                                  isolate, old_details.representation()),
                              new_field_type, isolate),
                          old_details.attributes(),
                          old_details.representation());
        new_descriptors->Set(i, &d);
      } else {
        DCHECK_NE(FIELD, old_details.type());
        Descriptor d(old_key,
                     handle(old_descriptors->GetValue(i), isolate),
                     old_details);
        new_descriptors->Set(i, &d);
      }
    }
  }

  new_descriptors->Sort();

  DCHECK(store_mode != FORCE_FIELD ||
         new_descriptors->GetDetails(modify_index).type() == FIELD);

  Handle<Map> split_map(root_map->FindLastMatchMap(
          root_nof, old_nof, *new_descriptors), isolate);
  int split_nof = split_map->NumberOfOwnDescriptors();
  DCHECK_NE(old_nof, split_nof);

  split_map->DeprecateTarget(
      old_descriptors->GetKey(split_nof), *new_descriptors);

  if (FLAG_trace_generalization) {
    PropertyDetails old_details = old_descriptors->GetDetails(modify_index);
    PropertyDetails new_details = new_descriptors->GetDetails(modify_index);
    Handle<HeapType> old_field_type = (old_details.type() == FIELD)
        ? handle(old_descriptors->GetFieldType(modify_index), isolate)
        : HeapType::Constant(handle(old_descriptors->GetValue(modify_index),
                                    isolate), isolate);
    Handle<HeapType> new_field_type = (new_details.type() == FIELD)
        ? handle(new_descriptors->GetFieldType(modify_index), isolate)
        : HeapType::Constant(handle(new_descriptors->GetValue(modify_index),
                                    isolate), isolate);
    old_map->PrintGeneralization(
        stdout, "", modify_index, split_nof, old_nof,
        old_details.type() == CONSTANT && store_mode == FORCE_FIELD,
        old_details.representation(), new_details.representation(),
        *old_field_type, *new_field_type);
  }

  // Add missing transitions.
  Handle<Map> new_map = split_map;
  for (int i = split_nof; i < old_nof; ++i) {
    new_map = CopyInstallDescriptors(new_map, i, new_descriptors);
  }
  new_map->set_owns_descriptors(true);
  return new_map;
}


// Generalize the representation of all FIELD descriptors.
Handle<Map> Map::GeneralizeAllFieldRepresentations(
    Handle<Map> map) {
  Handle<DescriptorArray> descriptors(map->instance_descriptors());
  for (int i = 0; i < map->NumberOfOwnDescriptors(); ++i) {
    if (descriptors->GetDetails(i).type() == FIELD) {
      map = GeneralizeRepresentation(map, i, Representation::Tagged(),
                                     HeapType::Any(map->GetIsolate()),
                                     FORCE_FIELD);
    }
  }
  return map;
}


// static
MaybeHandle<Map> Map::TryUpdate(Handle<Map> map) {
  Handle<Map> proto_map(map);
  while (proto_map->prototype()->IsJSObject()) {
    Handle<JSObject> holder(JSObject::cast(proto_map->prototype()));
    proto_map = Handle<Map>(holder->map());
    if (proto_map->is_deprecated() && JSObject::TryMigrateInstance(holder)) {
      proto_map = Handle<Map>(holder->map());
    }
  }
  return TryUpdateInternal(map);
}


// static
Handle<Map> Map::Update(Handle<Map> map) {
  return GeneralizeRepresentation(map, 0, Representation::None(),
                                  HeapType::None(map->GetIsolate()),
                                  ALLOW_AS_CONSTANT);
}


// static
MaybeHandle<Map> Map::TryUpdateInternal(Handle<Map> old_map) {
  DisallowHeapAllocation no_allocation;
  DisallowDeoptimization no_deoptimization(old_map->GetIsolate());

  if (!old_map->is_deprecated()) return old_map;

  // Check the state of the root map.
  Map* root_map = old_map->FindRootMap();
  if (!old_map->EquivalentToForTransition(root_map)) return MaybeHandle<Map>();
  int root_nof = root_map->NumberOfOwnDescriptors();

  int old_nof = old_map->NumberOfOwnDescriptors();
  DescriptorArray* old_descriptors = old_map->instance_descriptors();

  Map* new_map = root_map;
  for (int i = root_nof; i < old_nof; ++i) {
    int j = new_map->SearchTransition(old_descriptors->GetKey(i));
    if (j == TransitionArray::kNotFound) return MaybeHandle<Map>();
    new_map = new_map->GetTransition(j);
    DescriptorArray* new_descriptors = new_map->instance_descriptors();

    PropertyDetails new_details = new_descriptors->GetDetails(i);
    PropertyDetails old_details = old_descriptors->GetDetails(i);
    if (old_details.attributes() != new_details.attributes() ||
        !old_details.representation().fits_into(new_details.representation())) {
      return MaybeHandle<Map>();
    }
    PropertyType new_type = new_details.type();
    PropertyType old_type = old_details.type();
    Object* new_value = new_descriptors->GetValue(i);
    Object* old_value = old_descriptors->GetValue(i);
    switch (new_type) {
      case FIELD:
        if ((old_type == FIELD &&
             !HeapType::cast(old_value)->NowIs(HeapType::cast(new_value))) ||
            (old_type == CONSTANT &&
             !HeapType::cast(new_value)->NowContains(old_value)) ||
            (old_type == CALLBACKS &&
             !HeapType::Any()->Is(HeapType::cast(new_value)))) {
          return MaybeHandle<Map>();
        }
        break;

      case CONSTANT:
      case CALLBACKS:
        if (old_type != new_type || old_value != new_value) {
          return MaybeHandle<Map>();
        }
        break;

      case NORMAL:
      case HANDLER:
      case INTERCEPTOR:
      case NONEXISTENT:
        UNREACHABLE();
    }
  }
  if (new_map->NumberOfOwnDescriptors() != old_nof) return MaybeHandle<Map>();
  return handle(new_map);
}


MaybeHandle<Object> JSObject::SetPropertyWithInterceptor(LookupIterator* it,
                                                         Handle<Object> value) {
  // TODO(rossberg): Support symbols in the API.
  if (it->name()->IsSymbol()) return value;

  Handle<String> name_string = Handle<String>::cast(it->name());
  Handle<JSObject> holder = it->GetHolder<JSObject>();
  Handle<InterceptorInfo> interceptor(holder->GetNamedInterceptor());
  if (interceptor->setter()->IsUndefined()) return MaybeHandle<Object>();

  LOG(it->isolate(),
      ApiNamedPropertyAccess("interceptor-named-set", *holder, *name_string));
  PropertyCallbackArguments args(it->isolate(), interceptor->data(), *holder,
                                 *holder);
  v8::NamedPropertySetterCallback setter =
      v8::ToCData<v8::NamedPropertySetterCallback>(interceptor->setter());
  v8::Handle<v8::Value> result = args.Call(
      setter, v8::Utils::ToLocal(name_string), v8::Utils::ToLocal(value));
  RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(it->isolate(), Object);
  if (!result.IsEmpty()) return value;

  return MaybeHandle<Object>();
}


MaybeHandle<Object> Object::SetProperty(Handle<Object> object,
                                        Handle<Name> name, Handle<Object> value,
                                        StrictMode strict_mode,
                                        StoreFromKeyed store_mode) {
  LookupIterator it(object, name);
  return SetProperty(&it, value, strict_mode, store_mode);
}


MaybeHandle<Object> Object::SetProperty(LookupIterator* it,
                                        Handle<Object> value,
                                        StrictMode strict_mode,
                                        StoreFromKeyed store_mode) {
  // Make sure that the top context does not change when doing callbacks or
  // interceptor calls.
  AssertNoContextChange ncc(it->isolate());

  bool done = false;
  for (; it->IsFound(); it->Next()) {
    switch (it->state()) {
      case LookupIterator::NOT_FOUND:
        UNREACHABLE();

      case LookupIterator::ACCESS_CHECK:
        // TODO(verwaest): Remove the distinction. This is mostly bogus since we
        // don't know whether we'll want to fetch attributes or call a setter
        // until we find the property.
        if (it->HasAccess(v8::ACCESS_SET)) break;
        return JSObject::SetPropertyWithFailedAccessCheck(it, value,
                                                          strict_mode);

      case LookupIterator::JSPROXY:
        if (it->HolderIsReceiverOrHiddenPrototype()) {
          return JSProxy::SetPropertyWithHandler(it->GetHolder<JSProxy>(),
                                                 it->GetReceiver(), it->name(),
                                                 value, strict_mode);
        } else {
          // TODO(verwaest): Use the MaybeHandle to indicate result.
          bool has_result = false;
          MaybeHandle<Object> maybe_result =
              JSProxy::SetPropertyViaPrototypesWithHandler(
                  it->GetHolder<JSProxy>(), it->GetReceiver(), it->name(),
                  value, strict_mode, &has_result);
          if (has_result) return maybe_result;
          done = true;
        }
        break;

      case LookupIterator::INTERCEPTOR:
        if (it->HolderIsReceiverOrHiddenPrototype()) {
          MaybeHandle<Object> maybe_result =
              JSObject::SetPropertyWithInterceptor(it, value);
          if (!maybe_result.is_null()) return maybe_result;
          if (it->isolate()->has_pending_exception()) return maybe_result;
        } else {
          Maybe<PropertyAttributes> maybe_attributes =
              JSObject::GetPropertyAttributesWithInterceptor(
                  it->GetHolder<JSObject>(), it->GetReceiver(), it->name());
          if (!maybe_attributes.has_value) return MaybeHandle<Object>();
          done = maybe_attributes.value != ABSENT;
          if (done && (maybe_attributes.value & READ_ONLY) != 0) {
            return WriteToReadOnlyProperty(it, value, strict_mode);
          }
        }
        break;

      case LookupIterator::PROPERTY:
        if (!it->HasProperty()) break;
        if (it->property_details().IsReadOnly()) {
          return WriteToReadOnlyProperty(it, value, strict_mode);
        }
        switch (it->property_kind()) {
          case LookupIterator::ACCESSOR:
            if (it->HolderIsReceiverOrHiddenPrototype() ||
                !it->GetAccessors()->IsDeclaredAccessorInfo()) {
              return SetPropertyWithAccessor(it->GetReceiver(), it->name(),
                                             value, it->GetHolder<JSObject>(),
                                             it->GetAccessors(), strict_mode);
            }
            break;
          case LookupIterator::DATA:
            if (it->HolderIsReceiverOrHiddenPrototype()) {
              return SetDataProperty(it, value);
            }
        }
        done = true;
        break;
    }

    if (done) break;
  }

  return AddDataProperty(it, value, NONE, strict_mode, store_mode);
}


MaybeHandle<Object> Object::WriteToReadOnlyProperty(LookupIterator* it,
                                                    Handle<Object> value,
                                                    StrictMode strict_mode) {
  if (strict_mode != STRICT) return value;

  Handle<Object> args[] = {it->name(), it->GetReceiver()};
  Handle<Object> error = it->factory()->NewTypeError(
      "strict_read_only_property", HandleVector(args, ARRAY_SIZE(args)));
  return it->isolate()->Throw<Object>(error);
}


MaybeHandle<Object> Object::SetDataProperty(LookupIterator* it,
                                            Handle<Object> value) {
  // Proxies are handled on the WithHandler path. Other non-JSObjects cannot
  // have own properties.
  Handle<JSObject> receiver = Handle<JSObject>::cast(it->GetReceiver());

  // Store on the holder which may be hidden behind the receiver.
  DCHECK(it->HolderIsReceiverOrHiddenPrototype());

  // Old value for the observation change record.
  // Fetch before transforming the object since the encoding may become
  // incompatible with what's cached in |it|.
  bool is_observed =
      receiver->map()->is_observed() &&
      !it->name().is_identical_to(it->factory()->hidden_string());
  MaybeHandle<Object> maybe_old;
  if (is_observed) maybe_old = it->GetDataValue();

  // Possibly migrate to the most up-to-date map that will be able to store
  // |value| under it->name().
  it->PrepareForDataProperty(value);

  // Write the property value.
  it->WriteDataValue(value);

  // Send the change record if there are observers.
  if (is_observed && !value->SameValue(*maybe_old.ToHandleChecked())) {
    JSObject::EnqueueChangeRecord(receiver, "update", it->name(),
                                  maybe_old.ToHandleChecked());
  }

  return value;
}


MaybeHandle<Object> Object::AddDataProperty(LookupIterator* it,
                                            Handle<Object> value,
                                            PropertyAttributes attributes,
                                            StrictMode strict_mode,
                                            StoreFromKeyed store_mode) {
  DCHECK(!it->GetReceiver()->IsJSProxy());
  if (!it->GetReceiver()->IsJSObject()) {
    // TODO(verwaest): Throw a TypeError with a more specific message.
    return WriteToReadOnlyProperty(it, value, strict_mode);
  }
  Handle<JSObject> receiver = Handle<JSObject>::cast(it->GetReceiver());

  // If the receiver is a JSGlobalProxy, store on the prototype (JSGlobalObject)
  // instead. If the prototype is Null, the proxy is detached.
  if (receiver->IsJSGlobalProxy()) {
    // Trying to assign to a detached proxy.
    PrototypeIterator iter(it->isolate(), receiver);
    if (iter.IsAtEnd()) return value;
    receiver =
        Handle<JSGlobalObject>::cast(PrototypeIterator::GetCurrent(iter));
  }

  if (!receiver->map()->is_extensible()) {
    if (strict_mode == SLOPPY) return value;

    Handle<Object> args[1] = {it->name()};
    Handle<Object> error = it->factory()->NewTypeError(
        "object_not_extensible", HandleVector(args, ARRAY_SIZE(args)));
    return it->isolate()->Throw<Object>(error);
  }

  // Possibly migrate to the most up-to-date map that will be able to store
  // |value| under it->name() with |attributes|.
  it->TransitionToDataProperty(value, attributes, store_mode);

  // TODO(verwaest): Encapsulate dictionary handling better.
  if (receiver->map()->is_dictionary_map()) {
    // TODO(verwaest): Probably should ensure this is done beforehand.
    it->InternalizeName();
    JSObject::AddSlowProperty(receiver, it->name(), value, attributes);
  } else {
    // Write the property value.
    it->WriteDataValue(value);
  }

  // Send the change record if there are observers.
  if (receiver->map()->is_observed() &&
      !it->name().is_identical_to(it->factory()->hidden_string())) {
    JSObject::EnqueueChangeRecord(receiver, "add", it->name(),
                                  it->factory()->the_hole_value());
  }

  return value;
}


MaybeHandle<Object> JSObject::SetElementWithCallbackSetterInPrototypes(
    Handle<JSObject> object,
    uint32_t index,
    Handle<Object> value,
    bool* found,
    StrictMode strict_mode) {
  Isolate *isolate = object->GetIsolate();
  for (PrototypeIterator iter(isolate, object); !iter.IsAtEnd();
       iter.Advance()) {
    if (PrototypeIterator::GetCurrent(iter)->IsJSProxy()) {
      return JSProxy::SetPropertyViaPrototypesWithHandler(
          Handle<JSProxy>::cast(PrototypeIterator::GetCurrent(iter)), object,
          isolate->factory()->Uint32ToString(index),  // name
          value, strict_mode, found);
    }
    Handle<JSObject> js_proto =
        Handle<JSObject>::cast(PrototypeIterator::GetCurrent(iter));
    if (!js_proto->HasDictionaryElements()) {
      continue;
    }
    Handle<SeededNumberDictionary> dictionary(js_proto->element_dictionary());
    int entry = dictionary->FindEntry(index);
    if (entry != SeededNumberDictionary::kNotFound) {
      PropertyDetails details = dictionary->DetailsAt(entry);
      if (details.type() == CALLBACKS) {
        *found = true;
        Handle<Object> structure(dictionary->ValueAt(entry), isolate);
        return SetElementWithCallback(object, structure, index, value, js_proto,
                                      strict_mode);
      }
    }
  }
  *found = false;
  return isolate->factory()->the_hole_value();
}


void Map::EnsureDescriptorSlack(Handle<Map> map, int slack) {
  // Only supports adding slack to owned descriptors.
  DCHECK(map->owns_descriptors());

  Handle<DescriptorArray> descriptors(map->instance_descriptors());
  int old_size = map->NumberOfOwnDescriptors();
  if (slack <= descriptors->NumberOfSlackDescriptors()) return;

  Handle<DescriptorArray> new_descriptors = DescriptorArray::CopyUpTo(
      descriptors, old_size, slack);

  if (old_size == 0) {
    map->set_instance_descriptors(*new_descriptors);
    return;
  }

  // If the source descriptors had an enum cache we copy it. This ensures
  // that the maps to which we push the new descriptor array back can rely
  // on a cache always being available once it is set. If the map has more
  // enumerated descriptors than available in the original cache, the cache
  // will be lazily replaced by the extended cache when needed.
  if (descriptors->HasEnumCache()) {
    new_descriptors->CopyEnumCacheFrom(*descriptors);
  }

  // Replace descriptors by new_descriptors in all maps that share it.
  map->GetHeap()->incremental_marking()->RecordWrites(*descriptors);

  Map* walk_map;
  for (Object* current = map->GetBackPointer();
       !current->IsUndefined();
       current = walk_map->GetBackPointer()) {
    walk_map = Map::cast(current);
    if (walk_map->instance_descriptors() != *descriptors) break;
    walk_map->set_instance_descriptors(*new_descriptors);
  }

  map->set_instance_descriptors(*new_descriptors);
}


template<class T>
static int AppendUniqueCallbacks(NeanderArray* callbacks,
                                 Handle<typename T::Array> array,
                                 int valid_descriptors) {
  int nof_callbacks = callbacks->length();

  Isolate* isolate = array->GetIsolate();
  // Ensure the keys are unique names before writing them into the
  // instance descriptor. Since it may cause a GC, it has to be done before we
  // temporarily put the heap in an invalid state while appending descriptors.
  for (int i = 0; i < nof_callbacks; ++i) {
    Handle<AccessorInfo> entry(AccessorInfo::cast(callbacks->get(i)));
    if (entry->name()->IsUniqueName()) continue;
    Handle<String> key =
        isolate->factory()->InternalizeString(
            Handle<String>(String::cast(entry->name())));
    entry->set_name(*key);
  }

  // Fill in new callback descriptors.  Process the callbacks from
  // back to front so that the last callback with a given name takes
  // precedence over previously added callbacks with that name.
  for (int i = nof_callbacks - 1; i >= 0; i--) {
    Handle<AccessorInfo> entry(AccessorInfo::cast(callbacks->get(i)));
    Handle<Name> key(Name::cast(entry->name()));
    // Check if a descriptor with this name already exists before writing.
    if (!T::Contains(key, entry, valid_descriptors, array)) {
      T::Insert(key, entry, valid_descriptors, array);
      valid_descriptors++;
    }
  }

  return valid_descriptors;
}

struct DescriptorArrayAppender {
  typedef DescriptorArray Array;
  static bool Contains(Handle<Name> key,
                       Handle<AccessorInfo> entry,
                       int valid_descriptors,
                       Handle<DescriptorArray> array) {
    DisallowHeapAllocation no_gc;
    return array->Search(*key, valid_descriptors) != DescriptorArray::kNotFound;
  }
  static void Insert(Handle<Name> key,
                     Handle<AccessorInfo> entry,
                     int valid_descriptors,
                     Handle<DescriptorArray> array) {
    DisallowHeapAllocation no_gc;
    CallbacksDescriptor desc(key, entry, entry->property_attributes());
    array->Append(&desc);
  }
};


struct FixedArrayAppender {
  typedef FixedArray Array;
  static bool Contains(Handle<Name> key,
                       Handle<AccessorInfo> entry,
                       int valid_descriptors,
                       Handle<FixedArray> array) {
    for (int i = 0; i < valid_descriptors; i++) {
      if (*key == AccessorInfo::cast(array->get(i))->name()) return true;
    }
    return false;
  }
  static void Insert(Handle<Name> key,
                     Handle<AccessorInfo> entry,
                     int valid_descriptors,
                     Handle<FixedArray> array) {
    DisallowHeapAllocation no_gc;
    array->set(valid_descriptors, *entry);
  }
};


void Map::AppendCallbackDescriptors(Handle<Map> map,
                                    Handle<Object> descriptors) {
  int nof = map->NumberOfOwnDescriptors();
  Handle<DescriptorArray> array(map->instance_descriptors());
  NeanderArray callbacks(descriptors);
  DCHECK(array->NumberOfSlackDescriptors() >= callbacks.length());
  nof = AppendUniqueCallbacks<DescriptorArrayAppender>(&callbacks, array, nof);
  map->SetNumberOfOwnDescriptors(nof);
}


int AccessorInfo::AppendUnique(Handle<Object> descriptors,
                               Handle<FixedArray> array,
                               int valid_descriptors) {
  NeanderArray callbacks(descriptors);
  DCHECK(array->length() >= callbacks.length() + valid_descriptors);
  return AppendUniqueCallbacks<FixedArrayAppender>(&callbacks,
                                                   array,
                                                   valid_descriptors);
}


static bool ContainsMap(MapHandleList* maps, Handle<Map> map) {
  DCHECK(!map.is_null());
  for (int i = 0; i < maps->length(); ++i) {
    if (!maps->at(i).is_null() && maps->at(i).is_identical_to(map)) return true;
  }
  return false;
}


template <class T>
static Handle<T> MaybeNull(T* p) {
  if (p == NULL) return Handle<T>::null();
  return Handle<T>(p);
}


Handle<Map> Map::FindTransitionedMap(MapHandleList* candidates) {
  ElementsKind kind = elements_kind();
  Handle<Map> transitioned_map = Handle<Map>::null();
  Handle<Map> current_map(this);
  bool packed = IsFastPackedElementsKind(kind);
  if (IsTransitionableFastElementsKind(kind)) {
    while (CanTransitionToMoreGeneralFastElementsKind(kind, false)) {
      kind = GetNextMoreGeneralFastElementsKind(kind, false);
      Handle<Map> maybe_transitioned_map =
          MaybeNull(current_map->LookupElementsTransitionMap(kind));
      if (maybe_transitioned_map.is_null()) break;
      if (ContainsMap(candidates, maybe_transitioned_map) &&
          (packed || !IsFastPackedElementsKind(kind))) {
        transitioned_map = maybe_transitioned_map;
        if (!IsFastPackedElementsKind(kind)) packed = false;
      }
      current_map = maybe_transitioned_map;
    }
  }
  return transitioned_map;
}


static Map* FindClosestElementsTransition(Map* map, ElementsKind to_kind) {
  Map* current_map = map;
  int target_kind =
      IsFastElementsKind(to_kind) || IsExternalArrayElementsKind(to_kind)
      ? to_kind
      : TERMINAL_FAST_ELEMENTS_KIND;

  // Support for legacy API: SetIndexedPropertiesTo{External,Pixel}Data
  // allows to change elements from arbitrary kind to any ExternalArray
  // elements kind. Satisfy its requirements, checking whether we already
  // have the cached transition.
  if (IsExternalArrayElementsKind(to_kind) &&
      !IsFixedTypedArrayElementsKind(map->elements_kind())) {
    if (map->HasElementsTransition()) {
        Map* next_map = map->elements_transition_map();
        if (next_map->elements_kind() == to_kind) return next_map;
    }
    return map;
  }

  ElementsKind kind = map->elements_kind();
  while (kind != target_kind) {
    kind = GetNextTransitionElementsKind(kind);
    if (!current_map->HasElementsTransition()) return current_map;
    current_map = current_map->elements_transition_map();
  }

  if (to_kind != kind && current_map->HasElementsTransition()) {
    DCHECK(to_kind == DICTIONARY_ELEMENTS);
    Map* next_map = current_map->elements_transition_map();
    if (next_map->elements_kind() == to_kind) return next_map;
  }

  DCHECK(current_map->elements_kind() == target_kind);
  return current_map;
}


Map* Map::LookupElementsTransitionMap(ElementsKind to_kind) {
  Map* to_map = FindClosestElementsTransition(this, to_kind);
  if (to_map->elements_kind() == to_kind) return to_map;
  return NULL;
}


bool Map::IsMapInArrayPrototypeChain() {
  Isolate* isolate = GetIsolate();
  if (isolate->initial_array_prototype()->map() == this) {
    return true;
  }

  if (isolate->initial_object_prototype()->map() == this) {
    return true;
  }

  return false;
}


static Handle<Map> AddMissingElementsTransitions(Handle<Map> map,
                                                 ElementsKind to_kind) {
  DCHECK(IsTransitionElementsKind(map->elements_kind()));

  Handle<Map> current_map = map;

  ElementsKind kind = map->elements_kind();
  if (!map->is_prototype_map()) {
    while (kind != to_kind && !IsTerminalElementsKind(kind)) {
      kind = GetNextTransitionElementsKind(kind);
      current_map =
          Map::CopyAsElementsKind(current_map, kind, INSERT_TRANSITION);
    }
  }

  // In case we are exiting the fast elements kind system, just add the map in
  // the end.
  if (kind != to_kind) {
    current_map = Map::CopyAsElementsKind(
        current_map, to_kind, INSERT_TRANSITION);
  }

  DCHECK(current_map->elements_kind() == to_kind);
  return current_map;
}


Handle<Map> Map::TransitionElementsTo(Handle<Map> map,
                                      ElementsKind to_kind) {
  ElementsKind from_kind = map->elements_kind();
  if (from_kind == to_kind) return map;

  Isolate* isolate = map->GetIsolate();
  Context* native_context = isolate->context()->native_context();
  Object* maybe_array_maps = native_context->js_array_maps();
  if (maybe_array_maps->IsFixedArray()) {
    DisallowHeapAllocation no_gc;
    FixedArray* array_maps = FixedArray::cast(maybe_array_maps);
    if (array_maps->get(from_kind) == *map) {
      Object* maybe_transitioned_map = array_maps->get(to_kind);
      if (maybe_transitioned_map->IsMap()) {
        return handle(Map::cast(maybe_transitioned_map));
      }
    }
  }

  return TransitionElementsToSlow(map, to_kind);
}


Handle<Map> Map::TransitionElementsToSlow(Handle<Map> map,
                                          ElementsKind to_kind) {
  ElementsKind from_kind = map->elements_kind();

  if (from_kind == to_kind) {
    return map;
  }

  bool allow_store_transition =
      // Only remember the map transition if there is not an already existing
      // non-matching element transition.
      !map->IsUndefined() && !map->is_dictionary_map() &&
      IsTransitionElementsKind(from_kind);

  // Only store fast element maps in ascending generality.
  if (IsFastElementsKind(to_kind)) {
    allow_store_transition &=
        IsTransitionableFastElementsKind(from_kind) &&
        IsMoreGeneralElementsKindTransition(from_kind, to_kind);
  }

  if (!allow_store_transition) {
    return Map::CopyAsElementsKind(map, to_kind, OMIT_TRANSITION);
  }

  return Map::AsElementsKind(map, to_kind);
}


// static
Handle<Map> Map::AsElementsKind(Handle<Map> map, ElementsKind kind) {
  Handle<Map> closest_map(FindClosestElementsTransition(*map, kind));

  if (closest_map->elements_kind() == kind) {
    return closest_map;
  }

  return AddMissingElementsTransitions(closest_map, kind);
}


Handle<Map> JSObject::GetElementsTransitionMap(Handle<JSObject> object,
                                               ElementsKind to_kind) {
  Handle<Map> map(object->map());
  return Map::TransitionElementsTo(map, to_kind);
}


void JSObject::LookupOwnRealNamedProperty(Handle<Name> name,
                                          LookupResult* result) {
  DisallowHeapAllocation no_gc;
  if (IsJSGlobalProxy()) {
    PrototypeIterator iter(GetIsolate(), this);
    if (iter.IsAtEnd()) return result->NotFound();
    DCHECK(iter.GetCurrent()->IsJSGlobalObject());
    return JSObject::cast(iter.GetCurrent())
        ->LookupOwnRealNamedProperty(name, result);
  }

  if (HasFastProperties()) {
    map()->LookupDescriptor(this, *name, result);
    // A property or a map transition was found. We return all of these result
    // types because LookupOwnRealNamedProperty is used when setting
    // properties where map transitions are handled.
    DCHECK(!result->IsFound() ||
           (result->holder() == this && result->IsFastPropertyType()));
    return;
  }

  int entry = property_dictionary()->FindEntry(name);
  if (entry != NameDictionary::kNotFound) {
    Object* value = property_dictionary()->ValueAt(entry);
    if (IsGlobalObject()) {
      PropertyDetails d = property_dictionary()->DetailsAt(entry);
      if (d.IsDeleted() || PropertyCell::cast(value)->value()->IsTheHole()) {
        result->NotFound();
        return;
      }
      value = PropertyCell::cast(value)->value();
    }
    result->DictionaryResult(this, entry);
    return;
  }

  result->NotFound();
}


void JSObject::LookupRealNamedProperty(Handle<Name> name,
                                       LookupResult* result) {
  DisallowHeapAllocation no_gc;
  LookupOwnRealNamedProperty(name, result);
  if (result->IsFound()) return;

  LookupRealNamedPropertyInPrototypes(name, result);
}


void JSObject::LookupRealNamedPropertyInPrototypes(Handle<Name> name,
                                                   LookupResult* result) {
  if (name->IsOwn()) {
    result->NotFound();
    return;
  }

  DisallowHeapAllocation no_gc;
  Isolate* isolate = GetIsolate();
  for (PrototypeIterator iter(isolate, this); !iter.IsAtEnd(); iter.Advance()) {
    if (iter.GetCurrent()->IsJSProxy()) {
      return result->HandlerResult(JSProxy::cast(iter.GetCurrent()));
    }
    JSObject::cast(iter.GetCurrent())->LookupOwnRealNamedProperty(name, result);
    DCHECK(!(result->IsFound() && result->type() == INTERCEPTOR));
    if (result->IsFound()) return;
  }
  result->NotFound();
}


Maybe<bool> JSProxy::HasPropertyWithHandler(Handle<JSProxy> proxy,
                                            Handle<Name> name) {
  Isolate* isolate = proxy->GetIsolate();

  // TODO(rossberg): adjust once there is a story for symbols vs proxies.
  if (name->IsSymbol()) return maybe(false);

  Handle<Object> args[] = { name };
  Handle<Object> result;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, result, CallTrap(proxy, "has", isolate->derived_has_trap(),
                                ARRAY_SIZE(args), args),
      Maybe<bool>());

  return maybe(result->BooleanValue());
}


MaybeHandle<Object> JSProxy::SetPropertyWithHandler(Handle<JSProxy> proxy,
                                                    Handle<Object> receiver,
                                                    Handle<Name> name,
                                                    Handle<Object> value,
                                                    StrictMode strict_mode) {
  Isolate* isolate = proxy->GetIsolate();

  // TODO(rossberg): adjust once there is a story for symbols vs proxies.
  if (name->IsSymbol()) return value;

  Handle<Object> args[] = { receiver, name, value };
  RETURN_ON_EXCEPTION(
      isolate,
      CallTrap(proxy,
               "set",
               isolate->derived_set_trap(),
               ARRAY_SIZE(args),
               args),
      Object);

  return value;
}


MaybeHandle<Object> JSProxy::SetPropertyViaPrototypesWithHandler(
    Handle<JSProxy> proxy, Handle<Object> receiver, Handle<Name> name,
    Handle<Object> value, StrictMode strict_mode, bool* done) {
  Isolate* isolate = proxy->GetIsolate();
  Handle<Object> handler(proxy->handler(), isolate);  // Trap might morph proxy.

  // TODO(rossberg): adjust once there is a story for symbols vs proxies.
  if (name->IsSymbol()) {
    *done = false;
    return isolate->factory()->the_hole_value();
  }

  *done = true;  // except where redefined...
  Handle<Object> args[] = { name };
  Handle<Object> result;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result,
      CallTrap(proxy,
               "getPropertyDescriptor",
               Handle<Object>(),
               ARRAY_SIZE(args),
               args),
      Object);

  if (result->IsUndefined()) {
    *done = false;
    return isolate->factory()->the_hole_value();
  }

  // Emulate [[GetProperty]] semantics for proxies.
  Handle<Object> argv[] = { result };
  Handle<Object> desc;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, desc,
      Execution::Call(isolate,
                      isolate->to_complete_property_descriptor(),
                      result,
                      ARRAY_SIZE(argv),
                      argv),
      Object);

  // [[GetProperty]] requires to check that all properties are configurable.
  Handle<String> configurable_name =
      isolate->factory()->InternalizeOneByteString(
          STATIC_ASCII_VECTOR("configurable_"));
  Handle<Object> configurable =
      Object::GetProperty(desc, configurable_name).ToHandleChecked();
  DCHECK(configurable->IsBoolean());
  if (configurable->IsFalse()) {
    Handle<String> trap =
        isolate->factory()->InternalizeOneByteString(
            STATIC_ASCII_VECTOR("getPropertyDescriptor"));
    Handle<Object> args[] = { handler, trap, name };
    Handle<Object> error = isolate->factory()->NewTypeError(
        "proxy_prop_not_configurable", HandleVector(args, ARRAY_SIZE(args)));
    return isolate->Throw<Object>(error);
  }
  DCHECK(configurable->IsTrue());

  // Check for DataDescriptor.
  Handle<String> hasWritable_name =
      isolate->factory()->InternalizeOneByteString(
          STATIC_ASCII_VECTOR("hasWritable_"));
  Handle<Object> hasWritable =
      Object::GetProperty(desc, hasWritable_name).ToHandleChecked();
  DCHECK(hasWritable->IsBoolean());
  if (hasWritable->IsTrue()) {
    Handle<String> writable_name =
        isolate->factory()->InternalizeOneByteString(
            STATIC_ASCII_VECTOR("writable_"));
    Handle<Object> writable =
        Object::GetProperty(desc, writable_name).ToHandleChecked();
    DCHECK(writable->IsBoolean());
    *done = writable->IsFalse();
    if (!*done) return isolate->factory()->the_hole_value();
    if (strict_mode == SLOPPY) return value;
    Handle<Object> args[] = { name, receiver };
    Handle<Object> error = isolate->factory()->NewTypeError(
        "strict_read_only_property", HandleVector(args, ARRAY_SIZE(args)));
    return isolate->Throw<Object>(error);
  }

  // We have an AccessorDescriptor.
  Handle<String> set_name = isolate->factory()->InternalizeOneByteString(
      STATIC_ASCII_VECTOR("set_"));
  Handle<Object> setter = Object::GetProperty(desc, set_name).ToHandleChecked();
  if (!setter->IsUndefined()) {
    // TODO(rossberg): nicer would be to cast to some JSCallable here...
    return SetPropertyWithDefinedSetter(
        receiver, Handle<JSReceiver>::cast(setter), value);
  }

  if (strict_mode == SLOPPY) return value;
  Handle<Object> args2[] = { name, proxy };
  Handle<Object> error = isolate->factory()->NewTypeError(
      "no_setter_in_callback", HandleVector(args2, ARRAY_SIZE(args2)));
  return isolate->Throw<Object>(error);
}


MaybeHandle<Object> JSProxy::DeletePropertyWithHandler(
    Handle<JSProxy> proxy, Handle<Name> name, DeleteMode mode) {
  Isolate* isolate = proxy->GetIsolate();

  // TODO(rossberg): adjust once there is a story for symbols vs proxies.
  if (name->IsSymbol()) return isolate->factory()->false_value();

  Handle<Object> args[] = { name };
  Handle<Object> result;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result,
      CallTrap(proxy,
               "delete",
               Handle<Object>(),
               ARRAY_SIZE(args),
               args),
      Object);

  bool result_bool = result->BooleanValue();
  if (mode == STRICT_DELETION && !result_bool) {
    Handle<Object> handler(proxy->handler(), isolate);
    Handle<String> trap_name = isolate->factory()->InternalizeOneByteString(
        STATIC_ASCII_VECTOR("delete"));
    Handle<Object> args[] = { handler, trap_name };
    Handle<Object> error = isolate->factory()->NewTypeError(
        "handler_failed", HandleVector(args, ARRAY_SIZE(args)));
    return isolate->Throw<Object>(error);
  }
  return isolate->factory()->ToBoolean(result_bool);
}


MaybeHandle<Object> JSProxy::DeleteElementWithHandler(
    Handle<JSProxy> proxy, uint32_t index, DeleteMode mode) {
  Isolate* isolate = proxy->GetIsolate();
  Handle<String> name = isolate->factory()->Uint32ToString(index);
  return JSProxy::DeletePropertyWithHandler(proxy, name, mode);
}


Maybe<PropertyAttributes> JSProxy::GetPropertyAttributesWithHandler(
    Handle<JSProxy> proxy, Handle<Object> receiver, Handle<Name> name) {
  Isolate* isolate = proxy->GetIsolate();
  HandleScope scope(isolate);

  // TODO(rossberg): adjust once there is a story for symbols vs proxies.
  if (name->IsSymbol()) return maybe(ABSENT);

  Handle<Object> args[] = { name };
  Handle<Object> result;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, result,
      proxy->CallTrap(proxy, "getPropertyDescriptor", Handle<Object>(),
                      ARRAY_SIZE(args), args),
      Maybe<PropertyAttributes>());

  if (result->IsUndefined()) return maybe(ABSENT);

  Handle<Object> argv[] = { result };
  Handle<Object> desc;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, desc,
      Execution::Call(isolate, isolate->to_complete_property_descriptor(),
                      result, ARRAY_SIZE(argv), argv),
      Maybe<PropertyAttributes>());

  // Convert result to PropertyAttributes.
  Handle<String> enum_n = isolate->factory()->InternalizeOneByteString(
      STATIC_ASCII_VECTOR("enumerable_"));
  Handle<Object> enumerable;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, enumerable,
                                   Object::GetProperty(desc, enum_n),
                                   Maybe<PropertyAttributes>());
  Handle<String> conf_n = isolate->factory()->InternalizeOneByteString(
      STATIC_ASCII_VECTOR("configurable_"));
  Handle<Object> configurable;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, configurable,
                                   Object::GetProperty(desc, conf_n),
                                   Maybe<PropertyAttributes>());
  Handle<String> writ_n = isolate->factory()->InternalizeOneByteString(
      STATIC_ASCII_VECTOR("writable_"));
  Handle<Object> writable;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, writable,
                                   Object::GetProperty(desc, writ_n),
                                   Maybe<PropertyAttributes>());
  if (!writable->BooleanValue()) {
    Handle<String> set_n = isolate->factory()->InternalizeOneByteString(
        STATIC_ASCII_VECTOR("set_"));
    Handle<Object> setter;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, setter,
                                     Object::GetProperty(desc, set_n),
                                     Maybe<PropertyAttributes>());
    writable = isolate->factory()->ToBoolean(!setter->IsUndefined());
  }

  if (configurable->IsFalse()) {
    Handle<Object> handler(proxy->handler(), isolate);
    Handle<String> trap = isolate->factory()->InternalizeOneByteString(
        STATIC_ASCII_VECTOR("getPropertyDescriptor"));
    Handle<Object> args[] = { handler, trap, name };
    Handle<Object> error = isolate->factory()->NewTypeError(
        "proxy_prop_not_configurable", HandleVector(args, ARRAY_SIZE(args)));
    isolate->Throw(*error);
    return maybe(NONE);
  }

  int attributes = NONE;
  if (!enumerable->BooleanValue()) attributes |= DONT_ENUM;
  if (!configurable->BooleanValue()) attributes |= DONT_DELETE;
  if (!writable->BooleanValue()) attributes |= READ_ONLY;
  return maybe(static_cast<PropertyAttributes>(attributes));
}


Maybe<PropertyAttributes> JSProxy::GetElementAttributeWithHandler(
    Handle<JSProxy> proxy, Handle<JSReceiver> receiver, uint32_t index) {
  Isolate* isolate = proxy->GetIsolate();
  Handle<String> name = isolate->factory()->Uint32ToString(index);
  return GetPropertyAttributesWithHandler(proxy, receiver, name);
}


void JSProxy::Fix(Handle<JSProxy> proxy) {
  Isolate* isolate = proxy->GetIsolate();

  // Save identity hash.
  Handle<Object> hash(proxy->GetIdentityHash(), isolate);

  if (proxy->IsJSFunctionProxy()) {
    isolate->factory()->BecomeJSFunction(proxy);
    // Code will be set on the JavaScript side.
  } else {
    isolate->factory()->BecomeJSObject(proxy);
  }
  DCHECK(proxy->IsJSObject());

  // Inherit identity, if it was present.
  if (hash->IsSmi()) {
    JSObject::SetIdentityHash(Handle<JSObject>::cast(proxy),
                              Handle<Smi>::cast(hash));
  }
}


MaybeHandle<Object> JSProxy::CallTrap(Handle<JSProxy> proxy,
                                      const char* name,
                                      Handle<Object> derived,
                                      int argc,
                                      Handle<Object> argv[]) {
  Isolate* isolate = proxy->GetIsolate();
  Handle<Object> handler(proxy->handler(), isolate);

  Handle<String> trap_name = isolate->factory()->InternalizeUtf8String(name);
  Handle<Object> trap;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, trap,
      Object::GetPropertyOrElement(handler, trap_name),
      Object);

  if (trap->IsUndefined()) {
    if (derived.is_null()) {
      Handle<Object> args[] = { handler, trap_name };
      Handle<Object> error = isolate->factory()->NewTypeError(
        "handler_trap_missing", HandleVector(args, ARRAY_SIZE(args)));
      return isolate->Throw<Object>(error);
    }
    trap = Handle<Object>(derived);
  }

  return Execution::Call(isolate, trap, handler, argc, argv);
}


void JSObject::AllocateStorageForMap(Handle<JSObject> object, Handle<Map> map) {
  DCHECK(object->map()->inobject_properties() == map->inobject_properties());
  ElementsKind obj_kind = object->map()->elements_kind();
  ElementsKind map_kind = map->elements_kind();
  if (map_kind != obj_kind) {
    ElementsKind to_kind = map_kind;
    if (IsMoreGeneralElementsKindTransition(map_kind, obj_kind) ||
        IsDictionaryElementsKind(obj_kind)) {
      to_kind = obj_kind;
    }
    if (IsDictionaryElementsKind(to_kind)) {
      NormalizeElements(object);
    } else {
      TransitionElementsKind(object, to_kind);
    }
    map = Map::AsElementsKind(map, to_kind);
  }
  JSObject::MigrateToMap(object, map);
}


void JSObject::MigrateInstance(Handle<JSObject> object) {
  Handle<Map> original_map(object->map());
  Handle<Map> map = Map::Update(original_map);
  map->set_migration_target(true);
  MigrateToMap(object, map);
  if (FLAG_trace_migration) {
    object->PrintInstanceMigration(stdout, *original_map, *map);
  }
}


// static
bool JSObject::TryMigrateInstance(Handle<JSObject> object) {
  Isolate* isolate = object->GetIsolate();
  DisallowDeoptimization no_deoptimization(isolate);
  Handle<Map> original_map(object->map(), isolate);
  Handle<Map> new_map;
  if (!Map::TryUpdate(original_map).ToHandle(&new_map)) {
    return false;
  }
  JSObject::MigrateToMap(object, new_map);
  if (FLAG_trace_migration) {
    object->PrintInstanceMigration(stdout, *original_map, object->map());
  }
  return true;
}


MaybeHandle<Object> JSObject::SetPropertyUsingTransition(
    Handle<JSObject> object,
    LookupResult* lookup,
    Handle<Name> name,
    Handle<Object> value,
    PropertyAttributes attributes) {
  Handle<Map> transition_map(lookup->GetTransitionTarget());
  int descriptor = transition_map->LastAdded();

  Handle<DescriptorArray> descriptors(transition_map->instance_descriptors());
  PropertyDetails details = descriptors->GetDetails(descriptor);

  if (details.type() == CALLBACKS || attributes != details.attributes()) {
    // AddPropertyInternal will either normalize the object, or create a new
    // fast copy of the map. If we get a fast copy of the map, all field
    // representations will be tagged since the transition is omitted.
    return JSObject::AddPropertyInternal(
        object, name, value, attributes,
        JSReceiver::CERTAINLY_NOT_STORE_FROM_KEYED,
        JSReceiver::OMIT_EXTENSIBILITY_CHECK, OMIT_TRANSITION);
  }

  // Keep the target CONSTANT if the same value is stored.
  // TODO(verwaest): Also support keeping the placeholder
  // (value->IsUninitialized) as constant.
  if (!lookup->CanHoldValue(value)) {
    Representation field_representation = value->OptimalRepresentation();
    Handle<HeapType> field_type = value->OptimalType(
        lookup->isolate(), field_representation);
    transition_map = Map::GeneralizeRepresentation(
        transition_map, descriptor,
        field_representation, field_type, FORCE_FIELD);
  }

  JSObject::MigrateToNewProperty(object, transition_map, value);
  return value;
}


void JSObject::MigrateToNewProperty(Handle<JSObject> object,
                                    Handle<Map> map,
                                    Handle<Object> value) {
  JSObject::MigrateToMap(object, map);
  if (map->GetLastDescriptorDetails().type() != FIELD) return;
  object->WriteToField(map->LastAdded(), *value);
}


void JSObject::WriteToField(int descriptor, Object* value) {
  DisallowHeapAllocation no_gc;

  DescriptorArray* desc = map()->instance_descriptors();
  PropertyDetails details = desc->GetDetails(descriptor);

  DCHECK(details.type() == FIELD);

  FieldIndex index = FieldIndex::ForDescriptor(map(), descriptor);
  if (details.representation().IsDouble()) {
    // Nothing more to be done.
    if (value->IsUninitialized()) return;
    HeapNumber* box = HeapNumber::cast(RawFastPropertyAt(index));
    DCHECK(box->IsMutableHeapNumber());
    box->set_value(value->Number());
  } else {
    FastPropertyAtPut(index, value);
  }
}


void JSObject::SetPropertyToField(LookupResult* lookup, Handle<Object> value) {
  if (lookup->type() == CONSTANT || !lookup->CanHoldValue(value)) {
    Representation field_representation = value->OptimalRepresentation();
    Handle<HeapType> field_type = value->OptimalType(
        lookup->isolate(), field_representation);
    JSObject::GeneralizeFieldRepresentation(handle(lookup->holder()),
                                            lookup->GetDescriptorIndex(),
                                            field_representation, field_type);
  }
  lookup->holder()->WriteToField(lookup->GetDescriptorIndex(), *value);
}


void JSObject::ConvertAndSetOwnProperty(LookupResult* lookup,
                                        Handle<Name> name,
                                        Handle<Object> value,
                                        PropertyAttributes attributes) {
  Handle<JSObject> object(lookup->holder());
  if (object->map()->TooManyFastProperties(Object::MAY_BE_STORE_FROM_KEYED)) {
    JSObject::NormalizeProperties(object, CLEAR_INOBJECT_PROPERTIES, 0);
  } else if (object->map()->is_prototype_map()) {
    JSObject::NormalizeProperties(object, KEEP_INOBJECT_PROPERTIES, 0);
  }

  if (!object->HasFastProperties()) {
    ReplaceSlowProperty(object, name, value, attributes);
    ReoptimizeIfPrototype(object);
    return;
  }

  int descriptor_index = lookup->GetDescriptorIndex();
  if (lookup->GetAttributes() == attributes) {
    JSObject::GeneralizeFieldRepresentation(object, descriptor_index,
                                            Representation::Tagged(),
                                            HeapType::Any(lookup->isolate()));
  } else {
    Handle<Map> old_map(object->map());
    Handle<Map> new_map = Map::CopyGeneralizeAllRepresentations(old_map,
        descriptor_index, FORCE_FIELD, attributes, "attributes mismatch");
    JSObject::MigrateToMap(object, new_map);
  }

  object->WriteToField(descriptor_index, *value);
}


void JSObject::SetPropertyToFieldWithAttributes(LookupResult* lookup,
                                                Handle<Name> name,
                                                Handle<Object> value,
                                                PropertyAttributes attributes) {
  if (lookup->GetAttributes() == attributes) {
    if (value->IsUninitialized()) return;
    SetPropertyToField(lookup, value);
  } else {
    ConvertAndSetOwnProperty(lookup, name, value, attributes);
  }
}


void JSObject::AddProperty(Handle<JSObject> object, Handle<Name> name,
                           Handle<Object> value,
                           PropertyAttributes attributes) {
#ifdef DEBUG
  uint32_t index;
  DCHECK(!object->IsJSProxy());
  DCHECK(!name->AsArrayIndex(&index));
  LookupIterator it(object, name, LookupIterator::CHECK_OWN_REAL);
  Maybe<PropertyAttributes> maybe = GetPropertyAttributes(&it);
  DCHECK(maybe.has_value);
  DCHECK(!it.IsFound());
  DCHECK(object->map()->is_extensible());
#endif
  SetOwnPropertyIgnoreAttributes(object, name, value, attributes,
                                 OMIT_EXTENSIBILITY_CHECK).Check();
}


// Reconfigures a property to a data property with attributes, even if it is not
// reconfigurable.
MaybeHandle<Object> JSObject::SetOwnPropertyIgnoreAttributes(
    Handle<JSObject> object,
    Handle<Name> name,
    Handle<Object> value,
    PropertyAttributes attributes,
    ExtensibilityCheck extensibility_check,
    StoreFromKeyed store_from_keyed,
    ExecutableAccessorInfoHandling handling) {
  DCHECK(!value->IsTheHole());
  Isolate* isolate = object->GetIsolate();

  // Make sure that the top context does not change when doing callbacks or
  // interceptor calls.
  AssertNoContextChange ncc(isolate);

  LookupResult lookup(isolate);
  object->LookupOwn(name, &lookup, true);
  if (!lookup.IsFound()) {
    object->map()->LookupTransition(*object, *name, &lookup);
  }

  // Check access rights if needed.
  if (object->IsAccessCheckNeeded()) {
    if (!isolate->MayNamedAccess(object, name, v8::ACCESS_SET)) {
      LookupIterator it(object, name, LookupIterator::CHECK_OWN);
      return SetPropertyWithFailedAccessCheck(&it, value, SLOPPY);
    }
  }

  if (object->IsJSGlobalProxy()) {
    PrototypeIterator iter(isolate, object);
    if (iter.IsAtEnd()) return value;
    DCHECK(PrototypeIterator::GetCurrent(iter)->IsJSGlobalObject());
    return SetOwnPropertyIgnoreAttributes(
        Handle<JSObject>::cast(PrototypeIterator::GetCurrent(iter)), name,
        value, attributes, extensibility_check);
  }

  if (lookup.IsInterceptor() ||
      (lookup.IsDescriptorOrDictionary() && lookup.type() == CALLBACKS)) {
    object->LookupOwnRealNamedProperty(name, &lookup);
  }

  // Check for accessor in prototype chain removed here in clone.
  if (!lookup.IsFound()) {
    object->map()->LookupTransition(*object, *name, &lookup);
    TransitionFlag flag = lookup.IsFound()
        ? OMIT_TRANSITION : INSERT_TRANSITION;
    // Neither properties nor transitions found.
    return AddPropertyInternal(object, name, value, attributes,
                               store_from_keyed, extensibility_check, flag);
  }

  Handle<Object> old_value = isolate->factory()->the_hole_value();
  PropertyAttributes old_attributes = ABSENT;
  bool is_observed = object->map()->is_observed() &&
                     *name != isolate->heap()->hidden_string();
  if (is_observed && lookup.IsProperty()) {
    if (lookup.IsDataProperty()) {
      old_value = Object::GetPropertyOrElement(object, name).ToHandleChecked();
    }
    old_attributes = lookup.GetAttributes();
  }

  bool executed_set_prototype = false;

  // Check of IsReadOnly removed from here in clone.
  if (lookup.IsTransition()) {
    Handle<Object> result;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, result,
        SetPropertyUsingTransition(
            handle(lookup.holder()), &lookup, name, value, attributes),
        Object);
  } else {
    switch (lookup.type()) {
      case NORMAL:
        ReplaceSlowProperty(object, name, value, attributes);
        break;
      case FIELD:
        SetPropertyToFieldWithAttributes(&lookup, name, value, attributes);
        break;
      case CONSTANT:
        // Only replace the constant if necessary.
        if (lookup.GetAttributes() != attributes ||
            *value != lookup.GetConstant()) {
          SetPropertyToFieldWithAttributes(&lookup, name, value, attributes);
        }
        break;
      case CALLBACKS:
      {
        Handle<Object> callback(lookup.GetCallbackObject(), isolate);
        if (callback->IsExecutableAccessorInfo() &&
            handling == DONT_FORCE_FIELD) {
          Handle<Object> result;
          ASSIGN_RETURN_ON_EXCEPTION(
              isolate, result, JSObject::SetPropertyWithAccessor(
                                   object, name, value, handle(lookup.holder()),
                                   callback, STRICT),
              Object);

          if (attributes != lookup.GetAttributes()) {
            Handle<ExecutableAccessorInfo> new_data =
                Accessors::CloneAccessor(
                    isolate, Handle<ExecutableAccessorInfo>::cast(callback));
            new_data->set_property_attributes(attributes);
            if (attributes & READ_ONLY) {
              // This way we don't have to introduce a lookup to the setter,
              // simply make it unavailable to reflect the attributes.
              new_data->clear_setter();
            }

            SetPropertyCallback(object, name, new_data, attributes);
          }
          if (is_observed) {
            // If we are setting the prototype of a function and are observed,
            // don't send change records because the prototype handles that
            // itself.
            executed_set_prototype = object->IsJSFunction() &&
                String::Equals(isolate->factory()->prototype_string(),
                               Handle<String>::cast(name)) &&
                Handle<JSFunction>::cast(object)->should_have_prototype();
          }
        } else {
          ConvertAndSetOwnProperty(&lookup, name, value, attributes);
        }
        break;
      }
      case NONEXISTENT:
      case HANDLER:
      case INTERCEPTOR:
        UNREACHABLE();
    }
  }

  if (is_observed && !executed_set_prototype) {
    if (lookup.IsTransition()) {
      EnqueueChangeRecord(object, "add", name, old_value);
    } else if (old_value->IsTheHole()) {
      EnqueueChangeRecord(object, "reconfigure", name, old_value);
    } else {
      LookupResult new_lookup(isolate);
      object->LookupOwn(name, &new_lookup, true);
      bool value_changed = false;
      if (new_lookup.IsDataProperty()) {
        Handle<Object> new_value =
            Object::GetPropertyOrElement(object, name).ToHandleChecked();
        value_changed = !old_value->SameValue(*new_value);
      }
      if (new_lookup.GetAttributes() != old_attributes) {
        if (!value_changed) old_value = isolate->factory()->the_hole_value();
        EnqueueChangeRecord(object, "reconfigure", name, old_value);
      } else if (value_changed) {
        EnqueueChangeRecord(object, "update", name, old_value);
      }
    }
  }

  return value;
}


Maybe<PropertyAttributes> JSObject::GetPropertyAttributesWithInterceptor(
    Handle<JSObject> holder,
    Handle<Object> receiver,
    Handle<Name> name) {
  // TODO(rossberg): Support symbols in the API.
  if (name->IsSymbol()) return maybe(ABSENT);

  Isolate* isolate = holder->GetIsolate();
  HandleScope scope(isolate);

  // Make sure that the top context does not change when doing
  // callbacks or interceptor calls.
  AssertNoContextChange ncc(isolate);

  Handle<InterceptorInfo> interceptor(holder->GetNamedInterceptor());
  PropertyCallbackArguments args(
      isolate, interceptor->data(), *receiver, *holder);
  if (!interceptor->query()->IsUndefined()) {
    v8::NamedPropertyQueryCallback query =
        v8::ToCData<v8::NamedPropertyQueryCallback>(interceptor->query());
    LOG(isolate,
        ApiNamedPropertyAccess("interceptor-named-has", *holder, *name));
    v8::Handle<v8::Integer> result =
        args.Call(query, v8::Utils::ToLocal(Handle<String>::cast(name)));
    if (!result.IsEmpty()) {
      DCHECK(result->IsInt32());
      return maybe(static_cast<PropertyAttributes>(result->Int32Value()));
    }
  } else if (!interceptor->getter()->IsUndefined()) {
    v8::NamedPropertyGetterCallback getter =
        v8::ToCData<v8::NamedPropertyGetterCallback>(interceptor->getter());
    LOG(isolate,
        ApiNamedPropertyAccess("interceptor-named-get-has", *holder, *name));
    v8::Handle<v8::Value> result =
        args.Call(getter, v8::Utils::ToLocal(Handle<String>::cast(name)));
    if (!result.IsEmpty()) return maybe(DONT_ENUM);
  }

  RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Maybe<PropertyAttributes>());
  return maybe(ABSENT);
}


Maybe<PropertyAttributes> JSReceiver::GetOwnPropertyAttributes(
    Handle<JSReceiver> object, Handle<Name> name) {
  // Check whether the name is an array index.
  uint32_t index = 0;
  if (object->IsJSObject() && name->AsArrayIndex(&index)) {
    return GetOwnElementAttribute(object, index);
  }
  LookupIterator it(object, name, LookupIterator::CHECK_OWN);
  return GetPropertyAttributes(&it);
}


Maybe<PropertyAttributes> JSReceiver::GetPropertyAttributes(
    LookupIterator* it) {
  for (; it->IsFound(); it->Next()) {
    switch (it->state()) {
      case LookupIterator::NOT_FOUND:
        UNREACHABLE();
      case LookupIterator::JSPROXY:
        return JSProxy::GetPropertyAttributesWithHandler(
            it->GetHolder<JSProxy>(), it->GetReceiver(), it->name());
      case LookupIterator::INTERCEPTOR: {
        Maybe<PropertyAttributes> result =
            JSObject::GetPropertyAttributesWithInterceptor(
                it->GetHolder<JSObject>(), it->GetReceiver(), it->name());
        if (!result.has_value) return result;
        if (result.value != ABSENT) return result;
        break;
      }
      case LookupIterator::ACCESS_CHECK:
        if (it->HasAccess(v8::ACCESS_HAS)) break;
        return JSObject::GetPropertyAttributesWithFailedAccessCheck(it);
      case LookupIterator::PROPERTY:
        if (it->HasProperty()) {
          return maybe(it->property_details().attributes());
        }
        break;
    }
  }
  return maybe(ABSENT);
}


Maybe<PropertyAttributes> JSObject::GetElementAttributeWithReceiver(
    Handle<JSObject> object, Handle<JSReceiver> receiver, uint32_t index,
    bool check_prototype) {
  Isolate* isolate = object->GetIsolate();

  // Check access rights if needed.
  if (object->IsAccessCheckNeeded()) {
    if (!isolate->MayIndexedAccess(object, index, v8::ACCESS_HAS)) {
      isolate->ReportFailedAccessCheck(object, v8::ACCESS_HAS);
      RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Maybe<PropertyAttributes>());
      return maybe(ABSENT);
    }
  }

  if (object->IsJSGlobalProxy()) {
    PrototypeIterator iter(isolate, object);
    if (iter.IsAtEnd()) return maybe(ABSENT);
    DCHECK(PrototypeIterator::GetCurrent(iter)->IsJSGlobalObject());
    return JSObject::GetElementAttributeWithReceiver(
        Handle<JSObject>::cast(PrototypeIterator::GetCurrent(iter)), receiver,
        index, check_prototype);
  }

  // Check for lookup interceptor except when bootstrapping.
  if (object->HasIndexedInterceptor() && !isolate->bootstrapper()->IsActive()) {
    return JSObject::GetElementAttributeWithInterceptor(
        object, receiver, index, check_prototype);
  }

  return GetElementAttributeWithoutInterceptor(
      object, receiver, index, check_prototype);
}


Maybe<PropertyAttributes> JSObject::GetElementAttributeWithInterceptor(
    Handle<JSObject> object, Handle<JSReceiver> receiver, uint32_t index,
    bool check_prototype) {
  Isolate* isolate = object->GetIsolate();
  HandleScope scope(isolate);

  // Make sure that the top context does not change when doing
  // callbacks or interceptor calls.
  AssertNoContextChange ncc(isolate);

  Handle<InterceptorInfo> interceptor(object->GetIndexedInterceptor());
  PropertyCallbackArguments args(
      isolate, interceptor->data(), *receiver, *object);
  if (!interceptor->query()->IsUndefined()) {
    v8::IndexedPropertyQueryCallback query =
        v8::ToCData<v8::IndexedPropertyQueryCallback>(interceptor->query());
    LOG(isolate,
        ApiIndexedPropertyAccess("interceptor-indexed-has", *object, index));
    v8::Handle<v8::Integer> result = args.Call(query, index);
    if (!result.IsEmpty())
      return maybe(static_cast<PropertyAttributes>(result->Int32Value()));
  } else if (!interceptor->getter()->IsUndefined()) {
    v8::IndexedPropertyGetterCallback getter =
        v8::ToCData<v8::IndexedPropertyGetterCallback>(interceptor->getter());
    LOG(isolate,
        ApiIndexedPropertyAccess(
            "interceptor-indexed-get-has", *object, index));
    v8::Handle<v8::Value> result = args.Call(getter, index);
    if (!result.IsEmpty()) return maybe(NONE);
  }

  return GetElementAttributeWithoutInterceptor(
       object, receiver, index, check_prototype);
}


Maybe<PropertyAttributes> JSObject::GetElementAttributeWithoutInterceptor(
    Handle<JSObject> object, Handle<JSReceiver> receiver, uint32_t index,
    bool check_prototype) {
  PropertyAttributes attr = object->GetElementsAccessor()->GetAttributes(
      receiver, object, index);
  if (attr != ABSENT) return maybe(attr);

  // Handle [] on String objects.
  if (object->IsStringObjectWithCharacterAt(index)) {
    return maybe(static_cast<PropertyAttributes>(READ_ONLY | DONT_DELETE));
  }

  if (!check_prototype) return maybe(ABSENT);

  PrototypeIterator iter(object->GetIsolate(), object);
  if (PrototypeIterator::GetCurrent(iter)->IsJSProxy()) {
    // We need to follow the spec and simulate a call to [[GetOwnProperty]].
    return JSProxy::GetElementAttributeWithHandler(
        Handle<JSProxy>::cast(PrototypeIterator::GetCurrent(iter)), receiver,
        index);
  }
  if (iter.IsAtEnd()) return maybe(ABSENT);
  return GetElementAttributeWithReceiver(
      Handle<JSObject>::cast(PrototypeIterator::GetCurrent(iter)), receiver,
      index, true);
}


Handle<NormalizedMapCache> NormalizedMapCache::New(Isolate* isolate) {
  Handle<FixedArray> array(
      isolate->factory()->NewFixedArray(kEntries, TENURED));
  return Handle<NormalizedMapCache>::cast(array);
}


MaybeHandle<Map> NormalizedMapCache::Get(Handle<Map> fast_map,
                                         PropertyNormalizationMode mode) {
  DisallowHeapAllocation no_gc;
  Object* value = FixedArray::get(GetIndex(fast_map));
  if (!value->IsMap() ||
      !Map::cast(value)->EquivalentToForNormalization(*fast_map, mode)) {
    return MaybeHandle<Map>();
  }
  return handle(Map::cast(value));
}


void NormalizedMapCache::Set(Handle<Map> fast_map,
                             Handle<Map> normalized_map) {
  DisallowHeapAllocation no_gc;
  DCHECK(normalized_map->is_dictionary_map());
  FixedArray::set(GetIndex(fast_map), *normalized_map);
}


void NormalizedMapCache::Clear() {
  int entries = length();
  for (int i = 0; i != entries; i++) {
    set_undefined(i);
  }
}


void HeapObject::UpdateMapCodeCache(Handle<HeapObject> object,
                                    Handle<Name> name,
                                    Handle<Code> code) {
  Handle<Map> map(object->map());
  Map::UpdateCodeCache(map, name, code);
}


void JSObject::NormalizeProperties(Handle<JSObject> object,
                                   PropertyNormalizationMode mode,
                                   int expected_additional_properties) {
  if (!object->HasFastProperties()) return;

  Handle<Map> map(object->map());
  Handle<Map> new_map = Map::Normalize(map, mode);

  MigrateFastToSlow(object, new_map, expected_additional_properties);
}


void JSObject::MigrateFastToSlow(Handle<JSObject> object,
                                 Handle<Map> new_map,
                                 int expected_additional_properties) {
  // The global object is always normalized.
  DCHECK(!object->IsGlobalObject());
  // JSGlobalProxy must never be normalized
  DCHECK(!object->IsJSGlobalProxy());

  Isolate* isolate = object->GetIsolate();
  HandleScope scope(isolate);
  Handle<Map> map(object->map());

  // Allocate new content.
  int real_size = map->NumberOfOwnDescriptors();
  int property_count = real_size;
  if (expected_additional_properties > 0) {
    property_count += expected_additional_properties;
  } else {
    property_count += 2;  // Make space for two more properties.
  }
  Handle<NameDictionary> dictionary =
      NameDictionary::New(isolate, property_count);

  Handle<DescriptorArray> descs(map->instance_descriptors());
  for (int i = 0; i < real_size; i++) {
    PropertyDetails details = descs->GetDetails(i);
    switch (details.type()) {
      case CONSTANT: {
        Handle<Name> key(descs->GetKey(i));
        Handle<Object> value(descs->GetConstant(i), isolate);
        PropertyDetails d = PropertyDetails(
            details.attributes(), NORMAL, i + 1);
        dictionary = NameDictionary::Add(dictionary, key, value, d);
        break;
      }
      case FIELD: {
        Handle<Name> key(descs->GetKey(i));
        FieldIndex index = FieldIndex::ForDescriptor(*map, i);
        Handle<Object> value(
            object->RawFastPropertyAt(index), isolate);
        if (details.representation().IsDouble()) {
          DCHECK(value->IsMutableHeapNumber());
          Handle<HeapNumber> old = Handle<HeapNumber>::cast(value);
          value = isolate->factory()->NewHeapNumber(old->value());
        }
        PropertyDetails d =
            PropertyDetails(details.attributes(), NORMAL, i + 1);
        dictionary = NameDictionary::Add(dictionary, key, value, d);
        break;
      }
      case CALLBACKS: {
        Handle<Name> key(descs->GetKey(i));
        Handle<Object> value(descs->GetCallbacksObject(i), isolate);
        PropertyDetails d = PropertyDetails(
            details.attributes(), CALLBACKS, i + 1);
        dictionary = NameDictionary::Add(dictionary, key, value, d);
        break;
      }
      case INTERCEPTOR:
        break;
      case HANDLER:
      case NORMAL:
      case NONEXISTENT:
        UNREACHABLE();
        break;
    }
  }

  // Copy the next enumeration index from instance descriptor.
  dictionary->SetNextEnumerationIndex(real_size + 1);

  // From here on we cannot fail and we shouldn't GC anymore.
  DisallowHeapAllocation no_allocation;

  // Resize the object in the heap if necessary.
  int new_instance_size = new_map->instance_size();
  int instance_size_delta = map->instance_size() - new_instance_size;
  DCHECK(instance_size_delta >= 0);

  if (instance_size_delta > 0) {
    Heap* heap = isolate->heap();
    heap->CreateFillerObjectAt(object->address() + new_instance_size,
                               instance_size_delta);
    heap->AdjustLiveBytes(object->address(), -instance_size_delta,
                          Heap::FROM_MUTATOR);
  }

  // We are storing the new map using release store after creating a filler for
  // the left-over space to avoid races with the sweeper thread.
  object->synchronized_set_map(*new_map);

  object->set_properties(*dictionary);

  isolate->counters()->props_to_dictionary()->Increment();

#ifdef DEBUG
  if (FLAG_trace_normalization) {
    OFStream os(stdout);
    os << "Object properties have been normalized:\n";
    object->Print(os);
  }
#endif
}


void JSObject::MigrateSlowToFast(Handle<JSObject> object,
                                 int unused_property_fields) {
  if (object->HasFastProperties()) return;
  DCHECK(!object->IsGlobalObject());
  Isolate* isolate = object->GetIsolate();
  Factory* factory = isolate->factory();
  Handle<NameDictionary> dictionary(object->property_dictionary());

  // Make sure we preserve dictionary representation if there are too many
  // descriptors.
  int number_of_elements = dictionary->NumberOfElements();
  if (number_of_elements > kMaxNumberOfDescriptors) return;

  if (number_of_elements != dictionary->NextEnumerationIndex()) {
    NameDictionary::DoGenerateNewEnumerationIndices(dictionary);
  }

  int instance_descriptor_length = 0;
  int number_of_fields = 0;

  // Compute the length of the instance descriptor.
  int capacity = dictionary->Capacity();
  for (int i = 0; i < capacity; i++) {
    Object* k = dictionary->KeyAt(i);
    if (dictionary->IsKey(k)) {
      Object* value = dictionary->ValueAt(i);
      PropertyType type = dictionary->DetailsAt(i).type();
      DCHECK(type != FIELD);
      instance_descriptor_length++;
      if (type == NORMAL && !value->IsJSFunction()) {
        number_of_fields += 1;
      }
    }
  }

  int inobject_props = object->map()->inobject_properties();

  // Allocate new map.
  Handle<Map> new_map = Map::CopyDropDescriptors(handle(object->map()));
  new_map->set_dictionary_map(false);

  if (instance_descriptor_length == 0) {
    DisallowHeapAllocation no_gc;
    DCHECK_LE(unused_property_fields, inobject_props);
    // Transform the object.
    new_map->set_unused_property_fields(inobject_props);
    object->synchronized_set_map(*new_map);
    object->set_properties(isolate->heap()->empty_fixed_array());
    // Check that it really works.
    DCHECK(object->HasFastProperties());
    return;
  }

  // Allocate the instance descriptor.
  Handle<DescriptorArray> descriptors = DescriptorArray::Allocate(
      isolate, instance_descriptor_length);

  int number_of_allocated_fields =
      number_of_fields + unused_property_fields - inobject_props;
  if (number_of_allocated_fields < 0) {
    // There is enough inobject space for all fields (including unused).
    number_of_allocated_fields = 0;
    unused_property_fields = inobject_props - number_of_fields;
  }

  // Allocate the fixed array for the fields.
  Handle<FixedArray> fields = factory->NewFixedArray(
      number_of_allocated_fields);

  // Fill in the instance descriptor and the fields.
  int current_offset = 0;
  for (int i = 0; i < capacity; i++) {
    Object* k = dictionary->KeyAt(i);
    if (dictionary->IsKey(k)) {
      Object* value = dictionary->ValueAt(i);
      Handle<Name> key;
      if (k->IsSymbol()) {
        key = handle(Symbol::cast(k));
      } else {
        // Ensure the key is a unique name before writing into the
        // instance descriptor.
        key = factory->InternalizeString(handle(String::cast(k)));
      }

      PropertyDetails details = dictionary->DetailsAt(i);
      int enumeration_index = details.dictionary_index();
      PropertyType type = details.type();

      if (value->IsJSFunction()) {
        ConstantDescriptor d(key,
                             handle(value, isolate),
                             details.attributes());
        descriptors->Set(enumeration_index - 1, &d);
      } else if (type == NORMAL) {
        if (current_offset < inobject_props) {
          object->InObjectPropertyAtPut(current_offset,
                                        value,
                                        UPDATE_WRITE_BARRIER);
        } else {
          int offset = current_offset - inobject_props;
          fields->set(offset, value);
        }
        FieldDescriptor d(key,
                          current_offset++,
                          details.attributes(),
                          // TODO(verwaest): value->OptimalRepresentation();
                          Representation::Tagged());
        descriptors->Set(enumeration_index - 1, &d);
      } else if (type == CALLBACKS) {
        CallbacksDescriptor d(key,
                              handle(value, isolate),
                              details.attributes());
        descriptors->Set(enumeration_index - 1, &d);
      } else {
        UNREACHABLE();
      }
    }
  }
  DCHECK(current_offset == number_of_fields);

  descriptors->Sort();

  DisallowHeapAllocation no_gc;
  new_map->InitializeDescriptors(*descriptors);
  new_map->set_unused_property_fields(unused_property_fields);

  // Transform the object.
  object->synchronized_set_map(*new_map);

  object->set_properties(*fields);
  DCHECK(object->IsJSObject());

  // Check that it really works.
  DCHECK(object->HasFastProperties());
}


void JSObject::ResetElements(Handle<JSObject> object) {
  Heap* heap = object->GetIsolate()->heap();
  CHECK(object->map() != heap->sloppy_arguments_elements_map());
  object->set_elements(object->map()->GetInitialElements());
}


static Handle<SeededNumberDictionary> CopyFastElementsToDictionary(
    Handle<FixedArrayBase> array,
    int length,
    Handle<SeededNumberDictionary> dictionary) {
  Isolate* isolate = array->GetIsolate();
  Factory* factory = isolate->factory();
  bool has_double_elements = array->IsFixedDoubleArray();
  for (int i = 0; i < length; i++) {
    Handle<Object> value;
    if (has_double_elements) {
      Handle<FixedDoubleArray> double_array =
          Handle<FixedDoubleArray>::cast(array);
      if (double_array->is_the_hole(i)) {
        value = factory->the_hole_value();
      } else {
        value = factory->NewHeapNumber(double_array->get_scalar(i));
      }
    } else {
      value = handle(Handle<FixedArray>::cast(array)->get(i), isolate);
    }
    if (!value->IsTheHole()) {
      PropertyDetails details = PropertyDetails(NONE, NORMAL, 0);
      dictionary =
          SeededNumberDictionary::AddNumberEntry(dictionary, i, value, details);
    }
  }
  return dictionary;
}


Handle<SeededNumberDictionary> JSObject::NormalizeElements(
    Handle<JSObject> object) {
  DCHECK(!object->HasExternalArrayElements() &&
         !object->HasFixedTypedArrayElements());
  Isolate* isolate = object->GetIsolate();

  // Find the backing store.
  Handle<FixedArrayBase> array(FixedArrayBase::cast(object->elements()));
  bool is_arguments =
      (array->map() == isolate->heap()->sloppy_arguments_elements_map());
  if (is_arguments) {
    array = handle(FixedArrayBase::cast(
        Handle<FixedArray>::cast(array)->get(1)));
  }
  if (array->IsDictionary()) return Handle<SeededNumberDictionary>::cast(array);

  DCHECK(object->HasFastSmiOrObjectElements() ||
         object->HasFastDoubleElements() ||
         object->HasFastArgumentsElements());
  // Compute the effective length and allocate a new backing store.
  int length = object->IsJSArray()
      ? Smi::cast(Handle<JSArray>::cast(object)->length())->value()
      : array->length();
  int old_capacity = 0;
  int used_elements = 0;
  object->GetElementsCapacityAndUsage(&old_capacity, &used_elements);
  Handle<SeededNumberDictionary> dictionary =
      SeededNumberDictionary::New(isolate, used_elements);

  dictionary = CopyFastElementsToDictionary(array, length, dictionary);

  // Switch to using the dictionary as the backing storage for elements.
  if (is_arguments) {
    FixedArray::cast(object->elements())->set(1, *dictionary);
  } else {
    // Set the new map first to satify the elements type assert in
    // set_elements().
    Handle<Map> new_map =
        JSObject::GetElementsTransitionMap(object, DICTIONARY_ELEMENTS);

    JSObject::MigrateToMap(object, new_map);
    object->set_elements(*dictionary);
  }

  isolate->counters()->elements_to_dictionary()->Increment();

#ifdef DEBUG
  if (FLAG_trace_normalization) {
    OFStream os(stdout);
    os << "Object elements have been normalized:\n";
    object->Print(os);
  }
#endif

  DCHECK(object->HasDictionaryElements() ||
         object->HasDictionaryArgumentsElements());
  return dictionary;
}


static Smi* GenerateIdentityHash(Isolate* isolate) {
  int hash_value;
  int attempts = 0;
  do {
    // Generate a random 32-bit hash value but limit range to fit
    // within a smi.
    hash_value = isolate->random_number_generator()->NextInt() & Smi::kMaxValue;
    attempts++;
  } while (hash_value == 0 && attempts < 30);
  hash_value = hash_value != 0 ? hash_value : 1;  // never return 0

  return Smi::FromInt(hash_value);
}


void JSObject::SetIdentityHash(Handle<JSObject> object, Handle<Smi> hash) {
  DCHECK(!object->IsJSGlobalProxy());
  Isolate* isolate = object->GetIsolate();
  SetHiddenProperty(object, isolate->factory()->identity_hash_string(), hash);
}


template<typename ProxyType>
static Handle<Smi> GetOrCreateIdentityHashHelper(Handle<ProxyType> proxy) {
  Isolate* isolate = proxy->GetIsolate();

  Handle<Object> maybe_hash(proxy->hash(), isolate);
  if (maybe_hash->IsSmi()) return Handle<Smi>::cast(maybe_hash);

  Handle<Smi> hash(GenerateIdentityHash(isolate), isolate);
  proxy->set_hash(*hash);
  return hash;
}


Object* JSObject::GetIdentityHash() {
  DisallowHeapAllocation no_gc;
  Isolate* isolate = GetIsolate();
  if (IsJSGlobalProxy()) {
    return JSGlobalProxy::cast(this)->hash();
  }
  Object* stored_value =
      GetHiddenProperty(isolate->factory()->identity_hash_string());
  return stored_value->IsSmi()
      ? stored_value
      : isolate->heap()->undefined_value();
}


Handle<Smi> JSObject::GetOrCreateIdentityHash(Handle<JSObject> object) {
  if (object->IsJSGlobalProxy()) {
    return GetOrCreateIdentityHashHelper(Handle<JSGlobalProxy>::cast(object));
  }

  Isolate* isolate = object->GetIsolate();

  Handle<Object> maybe_hash(object->GetIdentityHash(), isolate);
  if (maybe_hash->IsSmi()) return Handle<Smi>::cast(maybe_hash);

  Handle<Smi> hash(GenerateIdentityHash(isolate), isolate);
  SetHiddenProperty(object, isolate->factory()->identity_hash_string(), hash);
  return hash;
}


Object* JSProxy::GetIdentityHash() {
  return this->hash();
}


Handle<Smi> JSProxy::GetOrCreateIdentityHash(Handle<JSProxy> proxy) {
  return GetOrCreateIdentityHashHelper(proxy);
}


Object* JSObject::GetHiddenProperty(Handle<Name> key) {
  DisallowHeapAllocation no_gc;
  DCHECK(key->IsUniqueName());
  if (IsJSGlobalProxy()) {
    // JSGlobalProxies store their hash internally.
    DCHECK(*key != GetHeap()->identity_hash_string());
    // For a proxy, use the prototype as target object.
    PrototypeIterator iter(GetIsolate(), this);
    // If the proxy is detached, return undefined.
    if (iter.IsAtEnd()) return GetHeap()->the_hole_value();
    DCHECK(iter.GetCurrent()->IsJSGlobalObject());
    return JSObject::cast(iter.GetCurrent())->GetHiddenProperty(key);
  }
  DCHECK(!IsJSGlobalProxy());
  Object* inline_value = GetHiddenPropertiesHashTable();

  if (inline_value->IsSmi()) {
    // Handle inline-stored identity hash.
    if (*key == GetHeap()->identity_hash_string()) {
      return inline_value;
    } else {
      return GetHeap()->the_hole_value();
    }
  }

  if (inline_value->IsUndefined()) return GetHeap()->the_hole_value();

  ObjectHashTable* hashtable = ObjectHashTable::cast(inline_value);
  Object* entry = hashtable->Lookup(key);
  return entry;
}


Handle<Object> JSObject::SetHiddenProperty(Handle<JSObject> object,
                                           Handle<Name> key,
                                           Handle<Object> value) {
  Isolate* isolate = object->GetIsolate();

  DCHECK(key->IsUniqueName());
  if (object->IsJSGlobalProxy()) {
    // JSGlobalProxies store their hash internally.
    DCHECK(*key != *isolate->factory()->identity_hash_string());
    // For a proxy, use the prototype as target object.
    PrototypeIterator iter(isolate, object);
    // If the proxy is detached, return undefined.
    if (iter.IsAtEnd()) return isolate->factory()->undefined_value();
    DCHECK(PrototypeIterator::GetCurrent(iter)->IsJSGlobalObject());
    return SetHiddenProperty(
        Handle<JSObject>::cast(PrototypeIterator::GetCurrent(iter)), key,
        value);
  }
  DCHECK(!object->IsJSGlobalProxy());

  Handle<Object> inline_value(object->GetHiddenPropertiesHashTable(), isolate);

  // If there is no backing store yet, store the identity hash inline.
  if (value->IsSmi() &&
      *key == *isolate->factory()->identity_hash_string() &&
      (inline_value->IsUndefined() || inline_value->IsSmi())) {
    return JSObject::SetHiddenPropertiesHashTable(object, value);
  }

  Handle<ObjectHashTable> hashtable =
      GetOrCreateHiddenPropertiesHashtable(object);

  // If it was found, check if the key is already in the dictionary.
  Handle<ObjectHashTable> new_table = ObjectHashTable::Put(hashtable, key,
                                                           value);
  if (*new_table != *hashtable) {
    // If adding the key expanded the dictionary (i.e., Add returned a new
    // dictionary), store it back to the object.
    SetHiddenPropertiesHashTable(object, new_table);
  }

  // Return this to mark success.
  return object;
}


void JSObject::DeleteHiddenProperty(Handle<JSObject> object, Handle<Name> key) {
  Isolate* isolate = object->GetIsolate();
  DCHECK(key->IsUniqueName());

  if (object->IsJSGlobalProxy()) {
    PrototypeIterator iter(isolate, object);
    if (iter.IsAtEnd()) return;
    DCHECK(PrototypeIterator::GetCurrent(iter)->IsJSGlobalObject());
    return DeleteHiddenProperty(
        Handle<JSObject>::cast(PrototypeIterator::GetCurrent(iter)), key);
  }

  Object* inline_value = object->GetHiddenPropertiesHashTable();

  // We never delete (inline-stored) identity hashes.
  DCHECK(*key != *isolate->factory()->identity_hash_string());
  if (inline_value->IsUndefined() || inline_value->IsSmi()) return;

  Handle<ObjectHashTable> hashtable(ObjectHashTable::cast(inline_value));
  bool was_present = false;
  ObjectHashTable::Remove(hashtable, key, &was_present);
}


bool JSObject::HasHiddenProperties(Handle<JSObject> object) {
  Handle<Name> hidden = object->GetIsolate()->factory()->hidden_string();
  LookupIterator it(object, hidden, LookupIterator::CHECK_OWN_REAL);
  Maybe<PropertyAttributes> maybe = GetPropertyAttributes(&it);
  // Cannot get an exception since the hidden_string isn't accessible to JS.
  DCHECK(maybe.has_value);
  return maybe.value != ABSENT;
}


Object* JSObject::GetHiddenPropertiesHashTable() {
  DCHECK(!IsJSGlobalProxy());
  if (HasFastProperties()) {
    // If the object has fast properties, check whether the first slot
    // in the descriptor array matches the hidden string. Since the
    // hidden strings hash code is zero (and no other name has hash
    // code zero) it will always occupy the first entry if present.
    DescriptorArray* descriptors = this->map()->instance_descriptors();
    if (descriptors->number_of_descriptors() > 0) {
      int sorted_index = descriptors->GetSortedKeyIndex(0);
      if (descriptors->GetKey(sorted_index) == GetHeap()->hidden_string() &&
          sorted_index < map()->NumberOfOwnDescriptors()) {
        DCHECK(descriptors->GetType(sorted_index) == FIELD);
        DCHECK(descriptors->GetDetails(sorted_index).representation().
               IsCompatibleForLoad(Representation::Tagged()));
        FieldIndex index = FieldIndex::ForDescriptor(this->map(),
                                                     sorted_index);
        return this->RawFastPropertyAt(index);
      } else {
        return GetHeap()->undefined_value();
      }
    } else {
      return GetHeap()->undefined_value();
    }
  } else {
    Isolate* isolate = GetIsolate();
    LookupResult result(isolate);
    LookupOwnRealNamedProperty(isolate->factory()->hidden_string(), &result);
    if (result.IsFound()) {
      DCHECK(result.IsNormal());
      DCHECK(result.holder() == this);
      return GetNormalizedProperty(&result);
    }
    return GetHeap()->undefined_value();
  }
}

Handle<ObjectHashTable> JSObject::GetOrCreateHiddenPropertiesHashtable(
    Handle<JSObject> object) {
  Isolate* isolate = object->GetIsolate();

  static const int kInitialCapacity = 4;
  Handle<Object> inline_value(object->GetHiddenPropertiesHashTable(), isolate);
  if (inline_value->IsHashTable()) {
    return Handle<ObjectHashTable>::cast(inline_value);
  }

  Handle<ObjectHashTable> hashtable = ObjectHashTable::New(
      isolate, kInitialCapacity, USE_CUSTOM_MINIMUM_CAPACITY);

  if (inline_value->IsSmi()) {
    // We were storing the identity hash inline and now allocated an actual
    // dictionary.  Put the identity hash into the new dictionary.
    hashtable = ObjectHashTable::Put(hashtable,
                                     isolate->factory()->identity_hash_string(),
                                     inline_value);
  }

  JSObject::SetOwnPropertyIgnoreAttributes(
      object, isolate->factory()->hidden_string(),
      hashtable, DONT_ENUM).Assert();

  return hashtable;
}


Handle<Object> JSObject::SetHiddenPropertiesHashTable(Handle<JSObject> object,
                                                      Handle<Object> value) {
  DCHECK(!object->IsJSGlobalProxy());

  Isolate* isolate = object->GetIsolate();

  // We can store the identity hash inline iff there is no backing store
  // for hidden properties yet.
  DCHECK(JSObject::HasHiddenProperties(object) != value->IsSmi());
  if (object->HasFastProperties()) {
    // If the object has fast properties, check whether the first slot
    // in the descriptor array matches the hidden string. Since the
    // hidden strings hash code is zero (and no other name has hash
    // code zero) it will always occupy the first entry if present.
    DescriptorArray* descriptors = object->map()->instance_descriptors();
    if (descriptors->number_of_descriptors() > 0) {
      int sorted_index = descriptors->GetSortedKeyIndex(0);
      if (descriptors->GetKey(sorted_index) == isolate->heap()->hidden_string()
          && sorted_index < object->map()->NumberOfOwnDescriptors()) {
        object->WriteToField(sorted_index, *value);
        return object;
      }
    }
  }

  SetOwnPropertyIgnoreAttributes(object, isolate->factory()->hidden_string(),
                                 value, DONT_ENUM,
                                 OMIT_EXTENSIBILITY_CHECK).Assert();
  return object;
}


Handle<Object> JSObject::DeletePropertyPostInterceptor(Handle<JSObject> object,
                                                       Handle<Name> name,
                                                       DeleteMode delete_mode) {
  // Check own property, ignore interceptor.
  Isolate* isolate = object->GetIsolate();
  LookupResult lookup(isolate);
  object->LookupOwnRealNamedProperty(name, &lookup);
  if (!lookup.IsFound()) return isolate->factory()->true_value();

  PropertyNormalizationMode mode = object->map()->is_prototype_map()
                                       ? KEEP_INOBJECT_PROPERTIES
                                       : CLEAR_INOBJECT_PROPERTIES;
  // Normalize object if needed.
  NormalizeProperties(object, mode, 0);

  Handle<Object> result = DeleteNormalizedProperty(object, name, delete_mode);
  ReoptimizeIfPrototype(object);
  return result;
}


MaybeHandle<Object> JSObject::DeletePropertyWithInterceptor(
    Handle<JSObject> object, Handle<Name> name) {
  Isolate* isolate = object->GetIsolate();

  // TODO(rossberg): Support symbols in the API.
  if (name->IsSymbol()) return isolate->factory()->false_value();

  Handle<InterceptorInfo> interceptor(object->GetNamedInterceptor());
  if (!interceptor->deleter()->IsUndefined()) {
    v8::NamedPropertyDeleterCallback deleter =
        v8::ToCData<v8::NamedPropertyDeleterCallback>(interceptor->deleter());
    LOG(isolate,
        ApiNamedPropertyAccess("interceptor-named-delete", *object, *name));
    PropertyCallbackArguments args(
        isolate, interceptor->data(), *object, *object);
    v8::Handle<v8::Boolean> result =
        args.Call(deleter, v8::Utils::ToLocal(Handle<String>::cast(name)));
    RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
    if (!result.IsEmpty()) {
      DCHECK(result->IsBoolean());
      Handle<Object> result_internal = v8::Utils::OpenHandle(*result);
      result_internal->VerifyApiCallResultType();
      // Rebox CustomArguments::kReturnValueOffset before returning.
      return handle(*result_internal, isolate);
    }
  }
  Handle<Object> result =
      DeletePropertyPostInterceptor(object, name, NORMAL_DELETION);
  return result;
}


MaybeHandle<Object> JSObject::DeleteElementWithInterceptor(
    Handle<JSObject> object,
    uint32_t index) {
  Isolate* isolate = object->GetIsolate();
  Factory* factory = isolate->factory();

  // Make sure that the top context does not change when doing
  // callbacks or interceptor calls.
  AssertNoContextChange ncc(isolate);

  Handle<InterceptorInfo> interceptor(object->GetIndexedInterceptor());
  if (interceptor->deleter()->IsUndefined()) return factory->false_value();
  v8::IndexedPropertyDeleterCallback deleter =
      v8::ToCData<v8::IndexedPropertyDeleterCallback>(interceptor->deleter());
  LOG(isolate,
      ApiIndexedPropertyAccess("interceptor-indexed-delete", *object, index));
  PropertyCallbackArguments args(
      isolate, interceptor->data(), *object, *object);
  v8::Handle<v8::Boolean> result = args.Call(deleter, index);
  RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
  if (!result.IsEmpty()) {
    DCHECK(result->IsBoolean());
    Handle<Object> result_internal = v8::Utils::OpenHandle(*result);
    result_internal->VerifyApiCallResultType();
    // Rebox CustomArguments::kReturnValueOffset before returning.
    return handle(*result_internal, isolate);
  }
  MaybeHandle<Object> delete_result = object->GetElementsAccessor()->Delete(
      object, index, NORMAL_DELETION);
  return delete_result;
}


MaybeHandle<Object> JSObject::DeleteElement(Handle<JSObject> object,
                                            uint32_t index,
                                            DeleteMode mode) {
  Isolate* isolate = object->GetIsolate();
  Factory* factory = isolate->factory();

  // Check access rights if needed.
  if (object->IsAccessCheckNeeded() &&
      !isolate->MayIndexedAccess(object, index, v8::ACCESS_DELETE)) {
    isolate->ReportFailedAccessCheck(object, v8::ACCESS_DELETE);
    RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
    return factory->false_value();
  }

  if (object->IsStringObjectWithCharacterAt(index)) {
    if (mode == STRICT_DELETION) {
      // Deleting a non-configurable property in strict mode.
      Handle<Object> name = factory->NewNumberFromUint(index);
      Handle<Object> args[2] = { name, object };
      Handle<Object> error =
          factory->NewTypeError("strict_delete_property",
                                HandleVector(args, 2));
      isolate->Throw(*error);
      return Handle<Object>();
    }
    return factory->false_value();
  }

  if (object->IsJSGlobalProxy()) {
    PrototypeIterator iter(isolate, object);
    if (iter.IsAtEnd()) return factory->false_value();
    DCHECK(PrototypeIterator::GetCurrent(iter)->IsJSGlobalObject());
    return DeleteElement(
        Handle<JSObject>::cast(PrototypeIterator::GetCurrent(iter)), index,
        mode);
  }

  Handle<Object> old_value;
  bool should_enqueue_change_record = false;
  if (object->map()->is_observed()) {
    Maybe<bool> maybe = HasOwnElement(object, index);
    if (!maybe.has_value) return MaybeHandle<Object>();
    should_enqueue_change_record = maybe.value;
    if (should_enqueue_change_record) {
      if (!GetOwnElementAccessorPair(object, index).is_null()) {
        old_value = Handle<Object>::cast(factory->the_hole_value());
      } else {
        old_value = Object::GetElement(
            isolate, object, index).ToHandleChecked();
      }
    }
  }

  // Skip interceptor if forcing deletion.
  MaybeHandle<Object> maybe_result;
  if (object->HasIndexedInterceptor() && mode != FORCE_DELETION) {
    maybe_result = DeleteElementWithInterceptor(object, index);
  } else {
    maybe_result = object->GetElementsAccessor()->Delete(object, index, mode);
  }
  Handle<Object> result;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, result, maybe_result, Object);

  if (should_enqueue_change_record) {
    Maybe<bool> maybe = HasOwnElement(object, index);
    if (!maybe.has_value) return MaybeHandle<Object>();
    if (!maybe.value) {
      Handle<String> name = factory->Uint32ToString(index);
      EnqueueChangeRecord(object, "delete", name, old_value);
    }
  }

  return result;
}


MaybeHandle<Object> JSObject::DeleteProperty(Handle<JSObject> object,
                                             Handle<Name> name,
                                             DeleteMode delete_mode) {
  Isolate* isolate = object->GetIsolate();
  // ECMA-262, 3rd, 8.6.2.5
  DCHECK(name->IsName());

  // Check access rights if needed.
  if (object->IsAccessCheckNeeded() &&
      !isolate->MayNamedAccess(object, name, v8::ACCESS_DELETE)) {
    isolate->ReportFailedAccessCheck(object, v8::ACCESS_DELETE);
    RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
    return isolate->factory()->false_value();
  }

  if (object->IsJSGlobalProxy()) {
    PrototypeIterator iter(isolate, object);
    if (iter.IsAtEnd()) return isolate->factory()->false_value();
    DCHECK(PrototypeIterator::GetCurrent(iter)->IsJSGlobalObject());
    return JSGlobalObject::DeleteProperty(
        Handle<JSGlobalObject>::cast(PrototypeIterator::GetCurrent(iter)), name,
        delete_mode);
  }

  uint32_t index = 0;
  if (name->AsArrayIndex(&index)) {
    return DeleteElement(object, index, delete_mode);
  }

  LookupResult lookup(isolate);
  object->LookupOwn(name, &lookup, true);
  if (!lookup.IsFound()) return isolate->factory()->true_value();
  // Ignore attributes if forcing a deletion.
  if (lookup.IsDontDelete() && delete_mode != FORCE_DELETION) {
    if (delete_mode == STRICT_DELETION) {
      // Deleting a non-configurable property in strict mode.
      Handle<Object> args[2] = { name, object };
      Handle<Object> error = isolate->factory()->NewTypeError(
          "strict_delete_property", HandleVector(args, ARRAY_SIZE(args)));
      isolate->Throw(*error);
      return Handle<Object>();
    }
    return isolate->factory()->false_value();
  }

  Handle<Object> old_value = isolate->factory()->the_hole_value();
  bool is_observed = object->map()->is_observed() &&
                     *name != isolate->heap()->hidden_string();
  if (is_observed && lookup.IsDataProperty()) {
    old_value = Object::GetPropertyOrElement(object, name).ToHandleChecked();
  }
  Handle<Object> result;

  // Check for interceptor.
  if (lookup.IsInterceptor()) {
    // Skip interceptor if forcing a deletion.
    if (delete_mode == FORCE_DELETION) {
      result = DeletePropertyPostInterceptor(object, name, delete_mode);
    } else {
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, result,
          DeletePropertyWithInterceptor(object, name),
          Object);
    }
  } else {
    PropertyNormalizationMode mode = object->map()->is_prototype_map()
                                         ? KEEP_INOBJECT_PROPERTIES
                                         : CLEAR_INOBJECT_PROPERTIES;
    // Normalize object if needed.
    NormalizeProperties(object, mode, 0);
    // Make sure the properties are normalized before removing the entry.
    result = DeleteNormalizedProperty(object, name, delete_mode);
    ReoptimizeIfPrototype(object);
  }

  if (is_observed) {
    Maybe<bool> maybe = HasOwnProperty(object, name);
    if (!maybe.has_value) return MaybeHandle<Object>();
    if (!maybe.value) {
      EnqueueChangeRecord(object, "delete", name, old_value);
    }
  }

  return result;
}


MaybeHandle<Object> JSReceiver::DeleteElement(Handle<JSReceiver> object,
                                              uint32_t index,
                                              DeleteMode mode) {
  if (object->IsJSProxy()) {
    return JSProxy::DeleteElementWithHandler(
        Handle<JSProxy>::cast(object), index, mode);
  }
  return JSObject::DeleteElement(Handle<JSObject>::cast(object), index, mode);
}


MaybeHandle<Object> JSReceiver::DeleteProperty(Handle<JSReceiver> object,
                                               Handle<Name> name,
                                               DeleteMode mode) {
  if (object->IsJSProxy()) {
    return JSProxy::DeletePropertyWithHandler(
        Handle<JSProxy>::cast(object), name, mode);
  }
  return JSObject::DeleteProperty(Handle<JSObject>::cast(object), name, mode);
}


bool JSObject::ReferencesObjectFromElements(FixedArray* elements,
                                            ElementsKind kind,
                                            Object* object) {
  DCHECK(IsFastObjectElementsKind(kind) ||
         kind == DICTIONARY_ELEMENTS);
  if (IsFastObjectElementsKind(kind)) {
    int length = IsJSArray()
        ? Smi::cast(JSArray::cast(this)->length())->value()
        : elements->length();
    for (int i = 0; i < length; ++i) {
      Object* element = elements->get(i);
      if (!element->IsTheHole() && element == object) return true;
    }
  } else {
    Object* key =
        SeededNumberDictionary::cast(elements)->SlowReverseLookup(object);
    if (!key->IsUndefined()) return true;
  }
  return false;
}


// Check whether this object references another object.
bool JSObject::ReferencesObject(Object* obj) {
  Map* map_of_this = map();
  Heap* heap = GetHeap();
  DisallowHeapAllocation no_allocation;

  // Is the object the constructor for this object?
  if (map_of_this->constructor() == obj) {
    return true;
  }

  // Is the object the prototype for this object?
  if (map_of_this->prototype() == obj) {
    return true;
  }

  // Check if the object is among the named properties.
  Object* key = SlowReverseLookup(obj);
  if (!key->IsUndefined()) {
    return true;
  }

  // Check if the object is among the indexed properties.
  ElementsKind kind = GetElementsKind();
  switch (kind) {
    // Raw pixels and external arrays do not reference other
    // objects.
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size)                        \
    case EXTERNAL_##TYPE##_ELEMENTS:                                           \
    case TYPE##_ELEMENTS:                                                      \
      break;

    TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE

    case FAST_DOUBLE_ELEMENTS:
    case FAST_HOLEY_DOUBLE_ELEMENTS:
      break;
    case FAST_SMI_ELEMENTS:
    case FAST_HOLEY_SMI_ELEMENTS:
      break;
    case FAST_ELEMENTS:
    case FAST_HOLEY_ELEMENTS:
    case DICTIONARY_ELEMENTS: {
      FixedArray* elements = FixedArray::cast(this->elements());
      if (ReferencesObjectFromElements(elements, kind, obj)) return true;
      break;
    }
    case SLOPPY_ARGUMENTS_ELEMENTS: {
      FixedArray* parameter_map = FixedArray::cast(elements());
      // Check the mapped parameters.
      int length = parameter_map->length();
      for (int i = 2; i < length; ++i) {
        Object* value = parameter_map->get(i);
        if (!value->IsTheHole() && value == obj) return true;
      }
      // Check the arguments.
      FixedArray* arguments = FixedArray::cast(parameter_map->get(1));
      kind = arguments->IsDictionary() ? DICTIONARY_ELEMENTS :
          FAST_HOLEY_ELEMENTS;
      if (ReferencesObjectFromElements(arguments, kind, obj)) return true;
      break;
    }
  }

  // For functions check the context.
  if (IsJSFunction()) {
    // Get the constructor function for arguments array.
    Map* arguments_map =
        heap->isolate()->context()->native_context()->sloppy_arguments_map();
    JSFunction* arguments_function =
        JSFunction::cast(arguments_map->constructor());

    // Get the context and don't check if it is the native context.
    JSFunction* f = JSFunction::cast(this);
    Context* context = f->context();
    if (context->IsNativeContext()) {
      return false;
    }

    // Check the non-special context slots.
    for (int i = Context::MIN_CONTEXT_SLOTS; i < context->length(); i++) {
      // Only check JS objects.
      if (context->get(i)->IsJSObject()) {
        JSObject* ctxobj = JSObject::cast(context->get(i));
        // If it is an arguments array check the content.
        if (ctxobj->map()->constructor() == arguments_function) {
          if (ctxobj->ReferencesObject(obj)) {
            return true;
          }
        } else if (ctxobj == obj) {
          return true;
        }
      }
    }

    // Check the context extension (if any) if it can have references.
    if (context->has_extension() && !context->IsCatchContext()) {
      // With harmony scoping, a JSFunction may have a global context.
      // TODO(mvstanton): walk into the ScopeInfo.
      if (FLAG_harmony_scoping && context->IsGlobalContext()) {
        return false;
      }

      return JSObject::cast(context->extension())->ReferencesObject(obj);
    }
  }

  // No references to object.
  return false;
}


MaybeHandle<Object> JSObject::PreventExtensions(Handle<JSObject> object) {
  Isolate* isolate = object->GetIsolate();

  if (!object->map()->is_extensible()) return object;

  if (object->IsAccessCheckNeeded() &&
      !isolate->MayNamedAccess(
          object, isolate->factory()->undefined_value(), v8::ACCESS_KEYS)) {
    isolate->ReportFailedAccessCheck(object, v8::ACCESS_KEYS);
    RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
    return isolate->factory()->false_value();
  }

  if (object->IsJSGlobalProxy()) {
    PrototypeIterator iter(isolate, object);
    if (iter.IsAtEnd()) return object;
    DCHECK(PrototypeIterator::GetCurrent(iter)->IsJSGlobalObject());
    return PreventExtensions(
        Handle<JSObject>::cast(PrototypeIterator::GetCurrent(iter)));
  }

  // It's not possible to seal objects with external array elements
  if (object->HasExternalArrayElements() ||
      object->HasFixedTypedArrayElements()) {
    Handle<Object> error  =
        isolate->factory()->NewTypeError(
            "cant_prevent_ext_external_array_elements",
            HandleVector(&object, 1));
    return isolate->Throw<Object>(error);
  }

  // If there are fast elements we normalize.
  Handle<SeededNumberDictionary> dictionary = NormalizeElements(object);
  DCHECK(object->HasDictionaryElements() ||
         object->HasDictionaryArgumentsElements());

  // Make sure that we never go back to fast case.
  dictionary->set_requires_slow_elements();

  // Do a map transition, other objects with this map may still
  // be extensible.
  // TODO(adamk): Extend the NormalizedMapCache to handle non-extensible maps.
  Handle<Map> new_map = Map::Copy(handle(object->map()));

  new_map->set_is_extensible(false);
  JSObject::MigrateToMap(object, new_map);
  DCHECK(!object->map()->is_extensible());

  if (object->map()->is_observed()) {
    EnqueueChangeRecord(object, "preventExtensions", Handle<Name>(),
                        isolate->factory()->the_hole_value());
  }
  return object;
}


template<typename Dictionary>
static void FreezeDictionary(Dictionary* dictionary) {
  int capacity = dictionary->Capacity();
  for (int i = 0; i < capacity; i++) {
    Object* k = dictionary->KeyAt(i);
    if (dictionary->IsKey(k) &&
        !(k->IsSymbol() && Symbol::cast(k)->is_private())) {
      PropertyDetails details = dictionary->DetailsAt(i);
      int attrs = DONT_DELETE;
      // READ_ONLY is an invalid attribute for JS setters/getters.
      if (details.type() == CALLBACKS) {
        Object* v = dictionary->ValueAt(i);
        if (v->IsPropertyCell()) v = PropertyCell::cast(v)->value();
        if (!v->IsAccessorPair()) attrs |= READ_ONLY;
      } else {
        attrs |= READ_ONLY;
      }
      details = details.CopyAddAttributes(
          static_cast<PropertyAttributes>(attrs));
      dictionary->DetailsAtPut(i, details);
    }
  }
}


MaybeHandle<Object> JSObject::Freeze(Handle<JSObject> object) {
  // Freezing sloppy arguments should be handled elsewhere.
  DCHECK(!object->HasSloppyArgumentsElements());
  DCHECK(!object->map()->is_observed());

  if (object->map()->is_frozen()) return object;

  Isolate* isolate = object->GetIsolate();
  if (object->IsAccessCheckNeeded() &&
      !isolate->MayNamedAccess(
          object, isolate->factory()->undefined_value(), v8::ACCESS_KEYS)) {
    isolate->ReportFailedAccessCheck(object, v8::ACCESS_KEYS);
    RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
    return isolate->factory()->false_value();
  }

  if (object->IsJSGlobalProxy()) {
    PrototypeIterator iter(isolate, object);
    if (iter.IsAtEnd()) return object;
    DCHECK(PrototypeIterator::GetCurrent(iter)->IsJSGlobalObject());
    return Freeze(Handle<JSObject>::cast(PrototypeIterator::GetCurrent(iter)));
  }

  // It's not possible to freeze objects with external array elements
  if (object->HasExternalArrayElements() ||
      object->HasFixedTypedArrayElements()) {
    Handle<Object> error  =
        isolate->factory()->NewTypeError(
            "cant_prevent_ext_external_array_elements",
            HandleVector(&object, 1));
    return isolate->Throw<Object>(error);
  }

  Handle<SeededNumberDictionary> new_element_dictionary;
  if (!object->elements()->IsDictionary()) {
    int length = object->IsJSArray()
        ? Smi::cast(Handle<JSArray>::cast(object)->length())->value()
        : object->elements()->length();
    if (length > 0) {
      int capacity = 0;
      int used = 0;
      object->GetElementsCapacityAndUsage(&capacity, &used);
      new_element_dictionary = SeededNumberDictionary::New(isolate, used);

      // Move elements to a dictionary; avoid calling NormalizeElements to avoid
      // unnecessary transitions.
      new_element_dictionary = CopyFastElementsToDictionary(
          handle(object->elements()), length, new_element_dictionary);
    } else {
      // No existing elements, use a pre-allocated empty backing store
      new_element_dictionary =
          isolate->factory()->empty_slow_element_dictionary();
    }
  }

  Handle<Map> old_map(object->map(), isolate);
  int transition_index = old_map->SearchTransition(
      isolate->heap()->frozen_symbol());
  if (transition_index != TransitionArray::kNotFound) {
    Handle<Map> transition_map(old_map->GetTransition(transition_index));
    DCHECK(transition_map->has_dictionary_elements());
    DCHECK(transition_map->is_frozen());
    DCHECK(!transition_map->is_extensible());
    JSObject::MigrateToMap(object, transition_map);
  } else if (object->HasFastProperties() && old_map->CanHaveMoreTransitions()) {
    // Create a new descriptor array with fully-frozen properties
    Handle<Map> new_map = Map::CopyForFreeze(old_map);
    JSObject::MigrateToMap(object, new_map);
  } else {
    DCHECK(old_map->is_dictionary_map() || !old_map->is_prototype_map());
    // Slow path: need to normalize properties for safety
    NormalizeProperties(object, CLEAR_INOBJECT_PROPERTIES, 0);

    // Create a new map, since other objects with this map may be extensible.
    // TODO(adamk): Extend the NormalizedMapCache to handle non-extensible maps.
    Handle<Map> new_map = Map::Copy(handle(object->map()));
    new_map->freeze();
    new_map->set_is_extensible(false);
    new_map->set_elements_kind(DICTIONARY_ELEMENTS);
    JSObject::MigrateToMap(object, new_map);

    // Freeze dictionary-mode properties
    FreezeDictionary(object->property_dictionary());
  }

  DCHECK(object->map()->has_dictionary_elements());
  if (!new_element_dictionary.is_null()) {
    object->set_elements(*new_element_dictionary);
  }

  if (object->elements() != isolate->heap()->empty_slow_element_dictionary()) {
    SeededNumberDictionary* dictionary = object->element_dictionary();
    // Make sure we never go back to the fast case
    dictionary->set_requires_slow_elements();
    // Freeze all elements in the dictionary
    FreezeDictionary(dictionary);
  }

  return object;
}


void JSObject::SetObserved(Handle<JSObject> object) {
  DCHECK(!object->IsJSGlobalProxy());
  DCHECK(!object->IsJSGlobalObject());
  Isolate* isolate = object->GetIsolate();
  Handle<Map> new_map;
  Handle<Map> old_map(object->map(), isolate);
  DCHECK(!old_map->is_observed());
  int transition_index = old_map->SearchTransition(
      isolate->heap()->observed_symbol());
  if (transition_index != TransitionArray::kNotFound) {
    new_map = handle(old_map->GetTransition(transition_index), isolate);
    DCHECK(new_map->is_observed());
  } else if (object->HasFastProperties() && old_map->CanHaveMoreTransitions()) {
    new_map = Map::CopyForObserved(old_map);
  } else {
    new_map = Map::Copy(old_map);
    new_map->set_is_observed();
  }
  JSObject::MigrateToMap(object, new_map);
}


Handle<Object> JSObject::FastPropertyAt(Handle<JSObject> object,
                                        Representation representation,
                                        FieldIndex index) {
  Isolate* isolate = object->GetIsolate();
  Handle<Object> raw_value(object->RawFastPropertyAt(index), isolate);
  return Object::WrapForRead(isolate, raw_value, representation);
}


template<class ContextObject>
class JSObjectWalkVisitor {
 public:
  JSObjectWalkVisitor(ContextObject* site_context, bool copying,
                      JSObject::DeepCopyHints hints)
    : site_context_(site_context),
      copying_(copying),
      hints_(hints) {}

  MUST_USE_RESULT MaybeHandle<JSObject> StructureWalk(Handle<JSObject> object);

 protected:
  MUST_USE_RESULT inline MaybeHandle<JSObject> VisitElementOrProperty(
      Handle<JSObject> object,
      Handle<JSObject> value) {
    Handle<AllocationSite> current_site = site_context()->EnterNewScope();
    MaybeHandle<JSObject> copy_of_value = StructureWalk(value);
    site_context()->ExitScope(current_site, value);
    return copy_of_value;
  }

  inline ContextObject* site_context() { return site_context_; }
  inline Isolate* isolate() { return site_context()->isolate(); }

  inline bool copying() const { return copying_; }

 private:
  ContextObject* site_context_;
  const bool copying_;
  const JSObject::DeepCopyHints hints_;
};


template <class ContextObject>
MaybeHandle<JSObject> JSObjectWalkVisitor<ContextObject>::StructureWalk(
    Handle<JSObject> object) {
  Isolate* isolate = this->isolate();
  bool copying = this->copying();
  bool shallow = hints_ == JSObject::kObjectIsShallow;

  if (!shallow) {
    StackLimitCheck check(isolate);

    if (check.HasOverflowed()) {
      isolate->StackOverflow();
      return MaybeHandle<JSObject>();
    }
  }

  if (object->map()->is_deprecated()) {
    JSObject::MigrateInstance(object);
  }

  Handle<JSObject> copy;
  if (copying) {
    Handle<AllocationSite> site_to_pass;
    if (site_context()->ShouldCreateMemento(object)) {
      site_to_pass = site_context()->current();
    }
    copy = isolate->factory()->CopyJSObjectWithAllocationSite(
        object, site_to_pass);
  } else {
    copy = object;
  }

  DCHECK(copying || copy.is_identical_to(object));

  ElementsKind kind = copy->GetElementsKind();
  if (copying && IsFastSmiOrObjectElementsKind(kind) &&
      FixedArray::cast(copy->elements())->map() ==
        isolate->heap()->fixed_cow_array_map()) {
    isolate->counters()->cow_arrays_created_runtime()->Increment();
  }

  if (!shallow) {
    HandleScope scope(isolate);

    // Deep copy own properties.
    if (copy->HasFastProperties()) {
      Handle<DescriptorArray> descriptors(copy->map()->instance_descriptors());
      int limit = copy->map()->NumberOfOwnDescriptors();
      for (int i = 0; i < limit; i++) {
        PropertyDetails details = descriptors->GetDetails(i);
        if (details.type() != FIELD) continue;
        FieldIndex index = FieldIndex::ForDescriptor(copy->map(), i);
        Handle<Object> value(object->RawFastPropertyAt(index), isolate);
        if (value->IsJSObject()) {
          ASSIGN_RETURN_ON_EXCEPTION(
              isolate, value,
              VisitElementOrProperty(copy, Handle<JSObject>::cast(value)),
              JSObject);
        } else {
          Representation representation = details.representation();
          value = Object::NewStorageFor(isolate, value, representation);
        }
        if (copying) {
          copy->FastPropertyAtPut(index, *value);
        }
      }
    } else {
      Handle<FixedArray> names =
          isolate->factory()->NewFixedArray(copy->NumberOfOwnProperties());
      copy->GetOwnPropertyNames(*names, 0);
      for (int i = 0; i < names->length(); i++) {
        DCHECK(names->get(i)->IsString());
        Handle<String> key_string(String::cast(names->get(i)));
        Maybe<PropertyAttributes> maybe =
            JSReceiver::GetOwnPropertyAttributes(copy, key_string);
        DCHECK(maybe.has_value);
        PropertyAttributes attributes = maybe.value;
        // Only deep copy fields from the object literal expression.
        // In particular, don't try to copy the length attribute of
        // an array.
        if (attributes != NONE) continue;
        Handle<Object> value =
            Object::GetProperty(copy, key_string).ToHandleChecked();
        if (value->IsJSObject()) {
          Handle<JSObject> result;
          ASSIGN_RETURN_ON_EXCEPTION(
              isolate, result,
              VisitElementOrProperty(copy, Handle<JSObject>::cast(value)),
              JSObject);
          if (copying) {
            // Creating object copy for literals. No strict mode needed.
            JSObject::SetProperty(copy, key_string, result, SLOPPY).Assert();
          }
        }
      }
    }

    // Deep copy own elements.
    // Pixel elements cannot be created using an object literal.
    DCHECK(!copy->HasExternalArrayElements());
    switch (kind) {
      case FAST_SMI_ELEMENTS:
      case FAST_ELEMENTS:
      case FAST_HOLEY_SMI_ELEMENTS:
      case FAST_HOLEY_ELEMENTS: {
        Handle<FixedArray> elements(FixedArray::cast(copy->elements()));
        if (elements->map() == isolate->heap()->fixed_cow_array_map()) {
#ifdef DEBUG
          for (int i = 0; i < elements->length(); i++) {
            DCHECK(!elements->get(i)->IsJSObject());
          }
#endif
        } else {
          for (int i = 0; i < elements->length(); i++) {
            Handle<Object> value(elements->get(i), isolate);
            DCHECK(value->IsSmi() ||
                   value->IsTheHole() ||
                   (IsFastObjectElementsKind(copy->GetElementsKind())));
            if (value->IsJSObject()) {
              Handle<JSObject> result;
              ASSIGN_RETURN_ON_EXCEPTION(
                  isolate, result,
                  VisitElementOrProperty(copy, Handle<JSObject>::cast(value)),
                  JSObject);
              if (copying) {
                elements->set(i, *result);
              }
            }
          }
        }
        break;
      }
      case DICTIONARY_ELEMENTS: {
        Handle<SeededNumberDictionary> element_dictionary(
            copy->element_dictionary());
        int capacity = element_dictionary->Capacity();
        for (int i = 0; i < capacity; i++) {
          Object* k = element_dictionary->KeyAt(i);
          if (element_dictionary->IsKey(k)) {
            Handle<Object> value(element_dictionary->ValueAt(i), isolate);
            if (value->IsJSObject()) {
              Handle<JSObject> result;
              ASSIGN_RETURN_ON_EXCEPTION(
                  isolate, result,
                  VisitElementOrProperty(copy, Handle<JSObject>::cast(value)),
                  JSObject);
              if (copying) {
                element_dictionary->ValueAtPut(i, *result);
              }
            }
          }
        }
        break;
      }
      case SLOPPY_ARGUMENTS_ELEMENTS:
        UNIMPLEMENTED();
        break;


#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size)                        \
      case EXTERNAL_##TYPE##_ELEMENTS:                                         \
      case TYPE##_ELEMENTS:                                                    \

      TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE

      case FAST_DOUBLE_ELEMENTS:
      case FAST_HOLEY_DOUBLE_ELEMENTS:
        // No contained objects, nothing to do.
        break;
    }
  }

  return copy;
}


MaybeHandle<JSObject> JSObject::DeepWalk(
    Handle<JSObject> object,
    AllocationSiteCreationContext* site_context) {
  JSObjectWalkVisitor<AllocationSiteCreationContext> v(site_context, false,
                                                       kNoHints);
  MaybeHandle<JSObject> result = v.StructureWalk(object);
  Handle<JSObject> for_assert;
  DCHECK(!result.ToHandle(&for_assert) || for_assert.is_identical_to(object));
  return result;
}


MaybeHandle<JSObject> JSObject::DeepCopy(
    Handle<JSObject> object,
    AllocationSiteUsageContext* site_context,
    DeepCopyHints hints) {
  JSObjectWalkVisitor<AllocationSiteUsageContext> v(site_context, true, hints);
  MaybeHandle<JSObject> copy = v.StructureWalk(object);
  Handle<JSObject> for_assert;
  DCHECK(!copy.ToHandle(&for_assert) || !for_assert.is_identical_to(object));
  return copy;
}


Handle<Object> JSObject::GetDataProperty(Handle<JSObject> object,
                                         Handle<Name> key) {
  Isolate* isolate = object->GetIsolate();
  LookupResult lookup(isolate);
  {
    DisallowHeapAllocation no_allocation;
    object->LookupRealNamedProperty(key, &lookup);
  }
  Handle<Object> result = isolate->factory()->undefined_value();
  if (lookup.IsFound() && !lookup.IsTransition()) {
    switch (lookup.type()) {
      case NORMAL:
        result = GetNormalizedProperty(
            Handle<JSObject>(lookup.holder(), isolate), &lookup);
        break;
      case FIELD:
        result = FastPropertyAt(Handle<JSObject>(lookup.holder(), isolate),
                                lookup.representation(),
                                lookup.GetFieldIndex());
        break;
      case CONSTANT:
        result = Handle<Object>(lookup.GetConstant(), isolate);
        break;
      case CALLBACKS:
      case HANDLER:
      case INTERCEPTOR:
        break;
      case NONEXISTENT:
        UNREACHABLE();
    }
  }
  return result;
}


// Tests for the fast common case for property enumeration:
// - This object and all prototypes has an enum cache (which means that
//   it is no proxy, has no interceptors and needs no access checks).
// - This object has no elements.
// - No prototype has enumerable properties/elements.
bool JSReceiver::IsSimpleEnum() {
  for (PrototypeIterator iter(GetIsolate(), this,
                              PrototypeIterator::START_AT_RECEIVER);
       !iter.IsAtEnd(); iter.Advance()) {
    if (!iter.GetCurrent()->IsJSObject()) return false;
    JSObject* curr = JSObject::cast(iter.GetCurrent());
    int enum_length = curr->map()->EnumLength();
    if (enum_length == kInvalidEnumCacheSentinel) return false;
    if (curr->IsAccessCheckNeeded()) return false;
    DCHECK(!curr->HasNamedInterceptor());
    DCHECK(!curr->HasIndexedInterceptor());
    if (curr->NumberOfEnumElements() > 0) return false;
    if (curr != this && enum_length != 0) return false;
  }
  return true;
}


static bool FilterKey(Object* key, PropertyAttributes filter) {
  if ((filter & SYMBOLIC) && key->IsSymbol()) {
    return true;
  }

  if ((filter & PRIVATE_SYMBOL) &&
      key->IsSymbol() && Symbol::cast(key)->is_private()) {
    return true;
  }

  if ((filter & STRING) && !key->IsSymbol()) {
    return true;
  }

  return false;
}


int Map::NumberOfDescribedProperties(DescriptorFlag which,
                                     PropertyAttributes filter) {
  int result = 0;
  DescriptorArray* descs = instance_descriptors();
  int limit = which == ALL_DESCRIPTORS
      ? descs->number_of_descriptors()
      : NumberOfOwnDescriptors();
  for (int i = 0; i < limit; i++) {
    if ((descs->GetDetails(i).attributes() & filter) == 0 &&
        !FilterKey(descs->GetKey(i), filter)) {
      result++;
    }
  }
  return result;
}


int Map::NextFreePropertyIndex() {
  int max_index = -1;
  int number_of_own_descriptors = NumberOfOwnDescriptors();
  DescriptorArray* descs = instance_descriptors();
  for (int i = 0; i < number_of_own_descriptors; i++) {
    if (descs->GetType(i) == FIELD) {
      int current_index = descs->GetFieldIndex(i);
      if (current_index > max_index) max_index = current_index;
    }
  }
  return max_index + 1;
}


void JSReceiver::LookupOwn(
    Handle<Name> name, LookupResult* result, bool search_hidden_prototypes) {
  DisallowHeapAllocation no_gc;
  DCHECK(name->IsName());

  if (IsJSGlobalProxy()) {
    PrototypeIterator iter(GetIsolate(), this);
    if (iter.IsAtEnd()) return result->NotFound();
    DCHECK(iter.GetCurrent()->IsJSGlobalObject());
    return JSReceiver::cast(iter.GetCurrent())
        ->LookupOwn(name, result, search_hidden_prototypes);
  }

  if (IsJSProxy()) {
    result->HandlerResult(JSProxy::cast(this));
    return;
  }

  // Do not use inline caching if the object is a non-global object
  // that requires access checks.
  if (IsAccessCheckNeeded()) {
    result->DisallowCaching();
  }

  JSObject* js_object = JSObject::cast(this);

  // Check for lookup interceptor except when bootstrapping.
  if (js_object->HasNamedInterceptor() &&
      !GetIsolate()->bootstrapper()->IsActive()) {
    result->InterceptorResult(js_object);
    return;
  }

  js_object->LookupOwnRealNamedProperty(name, result);
  if (result->IsFound() || name->IsOwn() || !search_hidden_prototypes) return;

  PrototypeIterator iter(GetIsolate(), js_object);
  if (!iter.GetCurrent()->IsJSReceiver()) return;
  JSReceiver* receiver = JSReceiver::cast(iter.GetCurrent());
  if (receiver->map()->is_hidden_prototype()) {
    receiver->LookupOwn(name, result, search_hidden_prototypes);
  }
}


void JSReceiver::Lookup(Handle<Name> name, LookupResult* result) {
  DisallowHeapAllocation no_gc;
  // Ecma-262 3rd 8.6.2.4
  for (PrototypeIterator iter(GetIsolate(), this,
                              PrototypeIterator::START_AT_RECEIVER);
       !iter.IsAtEnd(); iter.Advance()) {
    JSReceiver::cast(iter.GetCurrent())->LookupOwn(name, result, false);
    if (result->IsFound()) return;
    if (name->IsOwn()) {
      result->NotFound();
      return;
    }
  }
  result->NotFound();
}


static bool ContainsOnlyValidKeys(Handle<FixedArray> array) {
  int len = array->length();
  for (int i = 0; i < len; i++) {
    Object* e = array->get(i);
    if (!(e->IsString() || e->IsNumber())) return false;
  }
  return true;
}


static Handle<FixedArray> ReduceFixedArrayTo(
    Handle<FixedArray> array, int length) {
  DCHECK(array->length() >= length);
  if (array->length() == length) return array;

  Handle<FixedArray> new_array =
      array->GetIsolate()->factory()->NewFixedArray(length);
  for (int i = 0; i < length; ++i) new_array->set(i, array->get(i));
  return new_array;
}


static Handle<FixedArray> GetEnumPropertyKeys(Handle<JSObject> object,
                                              bool cache_result) {
  Isolate* isolate = object->GetIsolate();
  if (object->HasFastProperties()) {
    int own_property_count = object->map()->EnumLength();
    // If the enum length of the given map is set to kInvalidEnumCache, this
    // means that the map itself has never used the present enum cache. The
    // first step to using the cache is to set the enum length of the map by
    // counting the number of own descriptors that are not DONT_ENUM or
    // SYMBOLIC.
    if (own_property_count == kInvalidEnumCacheSentinel) {
      own_property_count = object->map()->NumberOfDescribedProperties(
          OWN_DESCRIPTORS, DONT_SHOW);
    } else {
      DCHECK(own_property_count == object->map()->NumberOfDescribedProperties(
          OWN_DESCRIPTORS, DONT_SHOW));
    }

    if (object->map()->instance_descriptors()->HasEnumCache()) {
      DescriptorArray* desc = object->map()->instance_descriptors();
      Handle<FixedArray> keys(desc->GetEnumCache(), isolate);

      // In case the number of properties required in the enum are actually
      // present, we can reuse the enum cache. Otherwise, this means that the
      // enum cache was generated for a previous (smaller) version of the
      // Descriptor Array. In that case we regenerate the enum cache.
      if (own_property_count <= keys->length()) {
        if (cache_result) object->map()->SetEnumLength(own_property_count);
        isolate->counters()->enum_cache_hits()->Increment();
        return ReduceFixedArrayTo(keys, own_property_count);
      }
    }

    Handle<Map> map(object->map());

    if (map->instance_descriptors()->IsEmpty()) {
      isolate->counters()->enum_cache_hits()->Increment();
      if (cache_result) map->SetEnumLength(0);
      return isolate->factory()->empty_fixed_array();
    }

    isolate->counters()->enum_cache_misses()->Increment();

    Handle<FixedArray> storage = isolate->factory()->NewFixedArray(
        own_property_count);
    Handle<FixedArray> indices = isolate->factory()->NewFixedArray(
        own_property_count);

    Handle<DescriptorArray> descs =
        Handle<DescriptorArray>(object->map()->instance_descriptors(), isolate);

    int size = map->NumberOfOwnDescriptors();
    int index = 0;

    for (int i = 0; i < size; i++) {
      PropertyDetails details = descs->GetDetails(i);
      Object* key = descs->GetKey(i);
      if (!(details.IsDontEnum() || key->IsSymbol())) {
        storage->set(index, key);
        if (!indices.is_null()) {
          if (details.type() != FIELD) {
            indices = Handle<FixedArray>();
          } else {
            FieldIndex field_index = FieldIndex::ForDescriptor(*map, i);
            int load_by_field_index = field_index.GetLoadByFieldIndex();
            indices->set(index, Smi::FromInt(load_by_field_index));
          }
        }
        index++;
      }
    }
    DCHECK(index == storage->length());

    Handle<FixedArray> bridge_storage =
        isolate->factory()->NewFixedArray(
            DescriptorArray::kEnumCacheBridgeLength);
    DescriptorArray* desc = object->map()->instance_descriptors();
    desc->SetEnumCache(*bridge_storage,
                       *storage,
                       indices.is_null() ? Object::cast(Smi::FromInt(0))
                                         : Object::cast(*indices));
    if (cache_result) {
      object->map()->SetEnumLength(own_property_count);
    }
    return storage;
  } else {
    Handle<NameDictionary> dictionary(object->property_dictionary());
    int length = dictionary->NumberOfEnumElements();
    if (length == 0) {
      return Handle<FixedArray>(isolate->heap()->empty_fixed_array());
    }
    Handle<FixedArray> storage = isolate->factory()->NewFixedArray(length);
    dictionary->CopyEnumKeysTo(*storage);
    return storage;
  }
}


MaybeHandle<FixedArray> JSReceiver::GetKeys(Handle<JSReceiver> object,
                                            KeyCollectionType type) {
  USE(ContainsOnlyValidKeys);
  Isolate* isolate = object->GetIsolate();
  Handle<FixedArray> content = isolate->factory()->empty_fixed_array();
  Handle<JSFunction> arguments_function(
      JSFunction::cast(isolate->sloppy_arguments_map()->constructor()));

  // Only collect keys if access is permitted.
  for (PrototypeIterator iter(isolate, object,
                              PrototypeIterator::START_AT_RECEIVER);
       !iter.IsAtEnd(); iter.Advance()) {
    if (PrototypeIterator::GetCurrent(iter)->IsJSProxy()) {
      Handle<JSProxy> proxy(JSProxy::cast(*PrototypeIterator::GetCurrent(iter)),
                            isolate);
      Handle<Object> args[] = { proxy };
      Handle<Object> names;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, names,
          Execution::Call(isolate,
                          isolate->proxy_enumerate(),
                          object,
                          ARRAY_SIZE(args),
                          args),
          FixedArray);
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, content,
          FixedArray::AddKeysFromArrayLike(
              content, Handle<JSObject>::cast(names)),
          FixedArray);
      break;
    }

    Handle<JSObject> current =
        Handle<JSObject>::cast(PrototypeIterator::GetCurrent(iter));

    // Check access rights if required.
    if (current->IsAccessCheckNeeded() &&
        !isolate->MayNamedAccess(
            current, isolate->factory()->undefined_value(), v8::ACCESS_KEYS)) {
      isolate->ReportFailedAccessCheck(current, v8::ACCESS_KEYS);
      RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, FixedArray);
      break;
    }

    // Compute the element keys.
    Handle<FixedArray> element_keys =
        isolate->factory()->NewFixedArray(current->NumberOfEnumElements());
    current->GetEnumElementKeys(*element_keys);
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, content,
        FixedArray::UnionOfKeys(content, element_keys),
        FixedArray);
    DCHECK(ContainsOnlyValidKeys(content));

    // Add the element keys from the interceptor.
    if (current->HasIndexedInterceptor()) {
      Handle<JSObject> result;
      if (JSObject::GetKeysForIndexedInterceptor(
              current, object).ToHandle(&result)) {
        ASSIGN_RETURN_ON_EXCEPTION(
            isolate, content,
            FixedArray::AddKeysFromArrayLike(content, result),
            FixedArray);
      }
      DCHECK(ContainsOnlyValidKeys(content));
    }

    // We can cache the computed property keys if access checks are
    // not needed and no interceptors are involved.
    //
    // We do not use the cache if the object has elements and
    // therefore it does not make sense to cache the property names
    // for arguments objects.  Arguments objects will always have
    // elements.
    // Wrapped strings have elements, but don't have an elements
    // array or dictionary.  So the fast inline test for whether to
    // use the cache says yes, so we should not create a cache.
    bool cache_enum_keys =
        ((current->map()->constructor() != *arguments_function) &&
         !current->IsJSValue() &&
         !current->IsAccessCheckNeeded() &&
         !current->HasNamedInterceptor() &&
         !current->HasIndexedInterceptor());
    // Compute the property keys and cache them if possible.
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, content,
        FixedArray::UnionOfKeys(
            content, GetEnumPropertyKeys(current, cache_enum_keys)),
        FixedArray);
    DCHECK(ContainsOnlyValidKeys(content));

    // Add the property keys from the interceptor.
    if (current->HasNamedInterceptor()) {
      Handle<JSObject> result;
      if (JSObject::GetKeysForNamedInterceptor(
              current, object).ToHandle(&result)) {
        ASSIGN_RETURN_ON_EXCEPTION(
            isolate, content,
            FixedArray::AddKeysFromArrayLike(content, result),
            FixedArray);
      }
      DCHECK(ContainsOnlyValidKeys(content));
    }

    // If we only want own properties we bail out after the first
    // iteration.
    if (type == OWN_ONLY) break;
  }
  return content;
}


// Try to update an accessor in an elements dictionary. Return true if the
// update succeeded, and false otherwise.
static bool UpdateGetterSetterInDictionary(
    SeededNumberDictionary* dictionary,
    uint32_t index,
    Object* getter,
    Object* setter,
    PropertyAttributes attributes) {
  int entry = dictionary->FindEntry(index);
  if (entry != SeededNumberDictionary::kNotFound) {
    Object* result = dictionary->ValueAt(entry);
    PropertyDetails details = dictionary->DetailsAt(entry);
    if (details.type() == CALLBACKS && result->IsAccessorPair()) {
      DCHECK(!details.IsDontDelete());
      if (details.attributes() != attributes) {
        dictionary->DetailsAtPut(
            entry,
            PropertyDetails(attributes, CALLBACKS, index));
      }
      AccessorPair::cast(result)->SetComponents(getter, setter);
      return true;
    }
  }
  return false;
}


void JSObject::DefineElementAccessor(Handle<JSObject> object,
                                     uint32_t index,
                                     Handle<Object> getter,
                                     Handle<Object> setter,
                                     PropertyAttributes attributes) {
  switch (object->GetElementsKind()) {
    case FAST_SMI_ELEMENTS:
    case FAST_ELEMENTS:
    case FAST_DOUBLE_ELEMENTS:
    case FAST_HOLEY_SMI_ELEMENTS:
    case FAST_HOLEY_ELEMENTS:
    case FAST_HOLEY_DOUBLE_ELEMENTS:
      break;

#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size)                        \
    case EXTERNAL_##TYPE##_ELEMENTS:                                           \
    case TYPE##_ELEMENTS:                                                      \

    TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
      // Ignore getters and setters on pixel and external array elements.
      return;

    case DICTIONARY_ELEMENTS:
      if (UpdateGetterSetterInDictionary(object->element_dictionary(),
                                         index,
                                         *getter,
                                         *setter,
                                         attributes)) {
        return;
      }
      break;
    case SLOPPY_ARGUMENTS_ELEMENTS: {
      // Ascertain whether we have read-only properties or an existing
      // getter/setter pair in an arguments elements dictionary backing
      // store.
      FixedArray* parameter_map = FixedArray::cast(object->elements());
      uint32_t length = parameter_map->length();
      Object* probe =
          index < (length - 2) ? parameter_map->get(index + 2) : NULL;
      if (probe == NULL || probe->IsTheHole()) {
        FixedArray* arguments = FixedArray::cast(parameter_map->get(1));
        if (arguments->IsDictionary()) {
          SeededNumberDictionary* dictionary =
              SeededNumberDictionary::cast(arguments);
          if (UpdateGetterSetterInDictionary(dictionary,
                                             index,
                                             *getter,
                                             *setter,
                                             attributes)) {
            return;
          }
        }
      }
      break;
    }
  }

  Isolate* isolate = object->GetIsolate();
  Handle<AccessorPair> accessors = isolate->factory()->NewAccessorPair();
  accessors->SetComponents(*getter, *setter);

  SetElementCallback(object, index, accessors, attributes);
}


Handle<AccessorPair> JSObject::CreateAccessorPairFor(Handle<JSObject> object,
                                                     Handle<Name> name) {
  Isolate* isolate = object->GetIsolate();
  LookupResult result(isolate);
  object->LookupOwnRealNamedProperty(name, &result);
  if (result.IsPropertyCallbacks()) {
    // Note that the result can actually have IsDontDelete() == true when we
    // e.g. have to fall back to the slow case while adding a setter after
    // successfully reusing a map transition for a getter. Nevertheless, this is
    // OK, because the assertion only holds for the whole addition of both
    // accessors, not for the addition of each part. See first comment in
    // DefinePropertyAccessor below.
    Object* obj = result.GetCallbackObject();
    if (obj->IsAccessorPair()) {
      return AccessorPair::Copy(handle(AccessorPair::cast(obj), isolate));
    }
  }
  return isolate->factory()->NewAccessorPair();
}


void JSObject::DefinePropertyAccessor(Handle<JSObject> object,
                                      Handle<Name> name,
                                      Handle<Object> getter,
                                      Handle<Object> setter,
                                      PropertyAttributes attributes) {
  // We could assert that the property is configurable here, but we would need
  // to do a lookup, which seems to be a bit of overkill.
  bool only_attribute_changes = getter->IsNull() && setter->IsNull();
  if (object->HasFastProperties() && !only_attribute_changes &&
      (object->map()->NumberOfOwnDescriptors() <= kMaxNumberOfDescriptors)) {
    bool getterOk = getter->IsNull() ||
        DefineFastAccessor(object, name, ACCESSOR_GETTER, getter, attributes);
    bool setterOk = !getterOk || setter->IsNull() ||
        DefineFastAccessor(object, name, ACCESSOR_SETTER, setter, attributes);
    if (getterOk && setterOk) return;
  }

  Handle<AccessorPair> accessors = CreateAccessorPairFor(object, name);
  accessors->SetComponents(*getter, *setter);

  SetPropertyCallback(object, name, accessors, attributes);
}


bool Map::DictionaryElementsInPrototypeChainOnly() {
  if (IsDictionaryElementsKind(elements_kind())) {
    return false;
  }

  for (PrototypeIterator iter(this); !iter.IsAtEnd(); iter.Advance()) {
    if (iter.GetCurrent()->IsJSProxy()) {
      // Be conservative, don't walk into proxies.
      return true;
    }

    if (IsDictionaryElementsKind(
            JSObject::cast(iter.GetCurrent())->map()->elements_kind())) {
      return true;
    }
  }

  return false;
}


void JSObject::SetElementCallback(Handle<JSObject> object,
                                  uint32_t index,
                                  Handle<Object> structure,
                                  PropertyAttributes attributes) {
  Heap* heap = object->GetHeap();
  PropertyDetails details = PropertyDetails(attributes, CALLBACKS, 0);

  // Normalize elements to make this operation simple.
  bool had_dictionary_elements = object->HasDictionaryElements();
  Handle<SeededNumberDictionary> dictionary = NormalizeElements(object);
  DCHECK(object->HasDictionaryElements() ||
         object->HasDictionaryArgumentsElements());
  // Update the dictionary with the new CALLBACKS property.
  dictionary = SeededNumberDictionary::Set(dictionary, index, structure,
                                           details);
  dictionary->set_requires_slow_elements();

  // Update the dictionary backing store on the object.
  if (object->elements()->map() == heap->sloppy_arguments_elements_map()) {
    // Also delete any parameter alias.
    //
    // TODO(kmillikin): when deleting the last parameter alias we could
    // switch to a direct backing store without the parameter map.  This
    // would allow GC of the context.
    FixedArray* parameter_map = FixedArray::cast(object->elements());
    if (index < static_cast<uint32_t>(parameter_map->length()) - 2) {
      parameter_map->set(index + 2, heap->the_hole_value());
    }
    parameter_map->set(1, *dictionary);
  } else {
    object->set_elements(*dictionary);

    if (!had_dictionary_elements) {
      // KeyedStoreICs (at least the non-generic ones) need a reset.
      heap->ClearAllICsByKind(Code::KEYED_STORE_IC);
    }
  }
}


void JSObject::SetPropertyCallback(Handle<JSObject> object,
                                   Handle<Name> name,
                                   Handle<Object> structure,
                                   PropertyAttributes attributes) {
  PropertyNormalizationMode mode = object->map()->is_prototype_map()
                                       ? KEEP_INOBJECT_PROPERTIES
                                       : CLEAR_INOBJECT_PROPERTIES;
  // Normalize object to make this operation simple.
  NormalizeProperties(object, mode, 0);

  // For the global object allocate a new map to invalidate the global inline
  // caches which have a global property cell reference directly in the code.
  if (object->IsGlobalObject()) {
    Handle<Map> new_map = Map::CopyDropDescriptors(handle(object->map()));
    DCHECK(new_map->is_dictionary_map());
    JSObject::MigrateToMap(object, new_map);

    // When running crankshaft, changing the map is not enough. We
    // need to deoptimize all functions that rely on this global
    // object.
    Deoptimizer::DeoptimizeGlobalObject(*object);
  }

  // Update the dictionary with the new CALLBACKS property.
  PropertyDetails details = PropertyDetails(attributes, CALLBACKS, 0);
  SetNormalizedProperty(object, name, structure, details);

  ReoptimizeIfPrototype(object);
}


MaybeHandle<Object> JSObject::DefineAccessor(Handle<JSObject> object,
                                             Handle<Name> name,
                                             Handle<Object> getter,
                                             Handle<Object> setter,
                                             PropertyAttributes attributes) {
  Isolate* isolate = object->GetIsolate();
  // Check access rights if needed.
  if (object->IsAccessCheckNeeded() &&
      !isolate->MayNamedAccess(object, name, v8::ACCESS_SET)) {
    isolate->ReportFailedAccessCheck(object, v8::ACCESS_SET);
    RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
    return isolate->factory()->undefined_value();
  }

  if (object->IsJSGlobalProxy()) {
    PrototypeIterator iter(isolate, object);
    if (iter.IsAtEnd()) return isolate->factory()->undefined_value();
    DCHECK(PrototypeIterator::GetCurrent(iter)->IsJSGlobalObject());
    DefineAccessor(Handle<JSObject>::cast(PrototypeIterator::GetCurrent(iter)),
                   name, getter, setter, attributes);
    return isolate->factory()->undefined_value();
  }

  // Make sure that the top context does not change when doing callbacks or
  // interceptor calls.
  AssertNoContextChange ncc(isolate);

  // Try to flatten before operating on the string.
  if (name->IsString()) name = String::Flatten(Handle<String>::cast(name));

  uint32_t index = 0;
  bool is_element = name->AsArrayIndex(&index);

  Handle<Object> old_value = isolate->factory()->the_hole_value();
  bool is_observed = object->map()->is_observed() &&
                     *name != isolate->heap()->hidden_string();
  bool preexists = false;
  if (is_observed) {
    if (is_element) {
      Maybe<bool> maybe = HasOwnElement(object, index);
      // Workaround for a GCC 4.4.3 bug which leads to "‘preexists’ may be used
      // uninitialized in this function".
      if (!maybe.has_value) {
        DCHECK(false);
        return isolate->factory()->undefined_value();
      }
      preexists = maybe.value;
      if (preexists && GetOwnElementAccessorPair(object, index).is_null()) {
        old_value =
            Object::GetElement(isolate, object, index).ToHandleChecked();
      }
    } else {
      LookupResult lookup(isolate);
      object->LookupOwn(name, &lookup, true);
      preexists = lookup.IsProperty();
      if (preexists && lookup.IsDataProperty()) {
        old_value =
            Object::GetPropertyOrElement(object, name).ToHandleChecked();
      }
    }
  }

  if (is_element) {
    DefineElementAccessor(object, index, getter, setter, attributes);
  } else {
    DefinePropertyAccessor(object, name, getter, setter, attributes);
  }

  if (is_observed) {
    const char* type = preexists ? "reconfigure" : "add";
    EnqueueChangeRecord(object, type, name, old_value);
  }

  return isolate->factory()->undefined_value();
}


static bool TryAccessorTransition(Handle<JSObject> self,
                                  Handle<Map> transitioned_map,
                                  int target_descriptor,
                                  AccessorComponent component,
                                  Handle<Object> accessor,
                                  PropertyAttributes attributes) {
  DescriptorArray* descs = transitioned_map->instance_descriptors();
  PropertyDetails details = descs->GetDetails(target_descriptor);

  // If the transition target was not callbacks, fall back to the slow case.
  if (details.type() != CALLBACKS) return false;
  Object* descriptor = descs->GetCallbacksObject(target_descriptor);
  if (!descriptor->IsAccessorPair()) return false;

  Object* target_accessor = AccessorPair::cast(descriptor)->get(component);
  PropertyAttributes target_attributes = details.attributes();

  // Reuse transition if adding same accessor with same attributes.
  if (target_accessor == *accessor && target_attributes == attributes) {
    JSObject::MigrateToMap(self, transitioned_map);
    return true;
  }

  // If either not the same accessor, or not the same attributes, fall back to
  // the slow case.
  return false;
}


bool JSObject::DefineFastAccessor(Handle<JSObject> object,
                                  Handle<Name> name,
                                  AccessorComponent component,
                                  Handle<Object> accessor,
                                  PropertyAttributes attributes) {
  DCHECK(accessor->IsSpecFunction() || accessor->IsUndefined());
  Isolate* isolate = object->GetIsolate();
  LookupResult result(isolate);
  object->LookupOwn(name, &result);

  if (result.IsFound() && !result.IsPropertyCallbacks()) {
    return false;
  }

  // Return success if the same accessor with the same attributes already exist.
  AccessorPair* source_accessors = NULL;
  if (result.IsPropertyCallbacks()) {
    Object* callback_value = result.GetCallbackObject();
    if (callback_value->IsAccessorPair()) {
      source_accessors = AccessorPair::cast(callback_value);
      Object* entry = source_accessors->get(component);
      if (entry == *accessor && result.GetAttributes() == attributes) {
        return true;
      }
    } else {
      return false;
    }

    int descriptor_number = result.GetDescriptorIndex();

    object->map()->LookupTransition(*object, *name, &result);

    if (result.IsFound()) {
      Handle<Map> target(result.GetTransitionTarget());
      DCHECK(target->NumberOfOwnDescriptors() ==
             object->map()->NumberOfOwnDescriptors());
      // This works since descriptors are sorted in order of addition.
      DCHECK(Name::Equals(
          handle(object->map()->instance_descriptors()->GetKey(
              descriptor_number)),
          name));
      return TryAccessorTransition(object, target, descriptor_number,
                                   component, accessor, attributes);
    }
  } else {
    // If not, lookup a transition.
    object->map()->LookupTransition(*object, *name, &result);

    // If there is a transition, try to follow it.
    if (result.IsFound()) {
      Handle<Map> target(result.GetTransitionTarget());
      int descriptor_number = target->LastAdded();
      DCHECK(Name::Equals(name,
          handle(target->instance_descriptors()->GetKey(descriptor_number))));
      return TryAccessorTransition(object, target, descriptor_number,
                                   component, accessor, attributes);
    }
  }

  // If there is no transition yet, add a transition to the a new accessor pair
  // containing the accessor.  Allocate a new pair if there were no source
  // accessors.  Otherwise, copy the pair and modify the accessor.
  Handle<AccessorPair> accessors = source_accessors != NULL
      ? AccessorPair::Copy(Handle<AccessorPair>(source_accessors))
      : isolate->factory()->NewAccessorPair();
  accessors->set(component, *accessor);

  CallbacksDescriptor new_accessors_desc(name, accessors, attributes);
  Handle<Map> new_map = Map::CopyInsertDescriptor(
      handle(object->map()), &new_accessors_desc, INSERT_TRANSITION);

  JSObject::MigrateToMap(object, new_map);
  return true;
}


MaybeHandle<Object> JSObject::SetAccessor(Handle<JSObject> object,
                                          Handle<AccessorInfo> info) {
  Isolate* isolate = object->GetIsolate();
  Factory* factory = isolate->factory();
  Handle<Name> name(Name::cast(info->name()));

  // Check access rights if needed.
  if (object->IsAccessCheckNeeded() &&
      !isolate->MayNamedAccess(object, name, v8::ACCESS_SET)) {
    isolate->ReportFailedAccessCheck(object, v8::ACCESS_SET);
    RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
    return factory->undefined_value();
  }

  if (object->IsJSGlobalProxy()) {
    PrototypeIterator iter(isolate, object);
    if (iter.IsAtEnd()) return object;
    DCHECK(PrototypeIterator::GetCurrent(iter)->IsJSGlobalObject());
    return SetAccessor(
        Handle<JSObject>::cast(PrototypeIterator::GetCurrent(iter)), info);
  }

  // Make sure that the top context does not change when doing callbacks or
  // interceptor calls.
  AssertNoContextChange ncc(isolate);

  // Try to flatten before operating on the string.
  if (name->IsString()) name = String::Flatten(Handle<String>::cast(name));

  uint32_t index = 0;
  bool is_element = name->AsArrayIndex(&index);

  if (is_element) {
    if (object->IsJSArray()) return factory->undefined_value();

    // Accessors overwrite previous callbacks (cf. with getters/setters).
    switch (object->GetElementsKind()) {
      case FAST_SMI_ELEMENTS:
      case FAST_ELEMENTS:
      case FAST_DOUBLE_ELEMENTS:
      case FAST_HOLEY_SMI_ELEMENTS:
      case FAST_HOLEY_ELEMENTS:
      case FAST_HOLEY_DOUBLE_ELEMENTS:
        break;

#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size)                        \
      case EXTERNAL_##TYPE##_ELEMENTS:                                         \
      case TYPE##_ELEMENTS:                                                    \

      TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
        // Ignore getters and setters on pixel and external array
        // elements.
        return factory->undefined_value();

      case DICTIONARY_ELEMENTS:
        break;
      case SLOPPY_ARGUMENTS_ELEMENTS:
        UNIMPLEMENTED();
        break;
    }

    SetElementCallback(object, index, info, info->property_attributes());
  } else {
    // Lookup the name.
    LookupResult result(isolate);
    object->LookupOwn(name, &result, true);
    // ES5 forbids turning a property into an accessor if it's not
    // configurable (that is IsDontDelete in ES3 and v8), see 8.6.1 (Table 5).
    if (result.IsFound() && (result.IsReadOnly() || result.IsDontDelete())) {
      return factory->undefined_value();
    }

    SetPropertyCallback(object, name, info, info->property_attributes());
  }

  return object;
}


MaybeHandle<Object> JSObject::GetAccessor(Handle<JSObject> object,
                                          Handle<Name> name,
                                          AccessorComponent component) {
  Isolate* isolate = object->GetIsolate();

  // Make sure that the top context does not change when doing callbacks or
  // interceptor calls.
  AssertNoContextChange ncc(isolate);

  // Check access rights if needed.
  if (object->IsAccessCheckNeeded() &&
      !isolate->MayNamedAccess(object, name, v8::ACCESS_HAS)) {
    isolate->ReportFailedAccessCheck(object, v8::ACCESS_HAS);
    RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
    return isolate->factory()->undefined_value();
  }

  // Make the lookup and include prototypes.
  uint32_t index = 0;
  if (name->AsArrayIndex(&index)) {
    for (PrototypeIterator iter(isolate, object,
                                PrototypeIterator::START_AT_RECEIVER);
         !iter.IsAtEnd(); iter.Advance()) {
      if (PrototypeIterator::GetCurrent(iter)->IsJSObject() &&
          JSObject::cast(*PrototypeIterator::GetCurrent(iter))
              ->HasDictionaryElements()) {
        JSObject* js_object =
            JSObject::cast(*PrototypeIterator::GetCurrent(iter));
        SeededNumberDictionary* dictionary = js_object->element_dictionary();
        int entry = dictionary->FindEntry(index);
        if (entry != SeededNumberDictionary::kNotFound) {
          Object* element = dictionary->ValueAt(entry);
          if (dictionary->DetailsAt(entry).type() == CALLBACKS &&
              element->IsAccessorPair()) {
            return handle(AccessorPair::cast(element)->GetComponent(component),
                          isolate);
          }
        }
      }
    }
  } else {
    for (PrototypeIterator iter(isolate, object,
                                PrototypeIterator::START_AT_RECEIVER);
         !iter.IsAtEnd(); iter.Advance()) {
      LookupResult result(isolate);
      JSReceiver::cast(*PrototypeIterator::GetCurrent(iter))
          ->LookupOwn(name, &result);
      if (result.IsFound()) {
        if (result.IsReadOnly()) return isolate->factory()->undefined_value();
        if (result.IsPropertyCallbacks()) {
          Object* obj = result.GetCallbackObject();
          if (obj->IsAccessorPair()) {
            return handle(AccessorPair::cast(obj)->GetComponent(component),
                          isolate);
          }
        }
      }
    }
  }
  return isolate->factory()->undefined_value();
}


Object* JSObject::SlowReverseLookup(Object* value) {
  if (HasFastProperties()) {
    int number_of_own_descriptors = map()->NumberOfOwnDescriptors();
    DescriptorArray* descs = map()->instance_descriptors();
    for (int i = 0; i < number_of_own_descriptors; i++) {
      if (descs->GetType(i) == FIELD) {
        Object* property =
            RawFastPropertyAt(FieldIndex::ForDescriptor(map(), i));
        if (descs->GetDetails(i).representation().IsDouble()) {
          DCHECK(property->IsMutableHeapNumber());
          if (value->IsNumber() && property->Number() == value->Number()) {
            return descs->GetKey(i);
          }
        } else if (property == value) {
          return descs->GetKey(i);
        }
      } else if (descs->GetType(i) == CONSTANT) {
        if (descs->GetConstant(i) == value) {
          return descs->GetKey(i);
        }
      }
    }
    return GetHeap()->undefined_value();
  } else {
    return property_dictionary()->SlowReverseLookup(value);
  }
}


Handle<Map> Map::RawCopy(Handle<Map> map, int instance_size) {
  Handle<Map> result = map->GetIsolate()->factory()->NewMap(
      map->instance_type(), instance_size);
  result->set_prototype(map->prototype());
  result->set_constructor(map->constructor());
  result->set_bit_field(map->bit_field());
  result->set_bit_field2(map->bit_field2());
  int new_bit_field3 = map->bit_field3();
  new_bit_field3 = OwnsDescriptors::update(new_bit_field3, true);
  new_bit_field3 = NumberOfOwnDescriptorsBits::update(new_bit_field3, 0);
  new_bit_field3 = EnumLengthBits::update(new_bit_field3,
                                          kInvalidEnumCacheSentinel);
  new_bit_field3 = Deprecated::update(new_bit_field3, false);
  if (!map->is_dictionary_map()) {
    new_bit_field3 = IsUnstable::update(new_bit_field3, false);
  }
  new_bit_field3 = ConstructionCount::update(new_bit_field3,
                                             JSFunction::kNoSlackTracking);
  result->set_bit_field3(new_bit_field3);
  return result;
}


Handle<Map> Map::Normalize(Handle<Map> fast_map,
                           PropertyNormalizationMode mode) {
  DCHECK(!fast_map->is_dictionary_map());

  Isolate* isolate = fast_map->GetIsolate();
  Handle<Object> maybe_cache(isolate->native_context()->normalized_map_cache(),
                             isolate);
  bool use_cache = !maybe_cache->IsUndefined();
  Handle<NormalizedMapCache> cache;
  if (use_cache) cache = Handle<NormalizedMapCache>::cast(maybe_cache);

  Handle<Map> new_map;
  if (use_cache && cache->Get(fast_map, mode).ToHandle(&new_map)) {
#ifdef VERIFY_HEAP
    if (FLAG_verify_heap) new_map->DictionaryMapVerify();
#endif
#ifdef ENABLE_SLOW_DCHECKS
    if (FLAG_enable_slow_asserts) {
      // The cached map should match newly created normalized map bit-by-bit,
      // except for the code cache, which can contain some ics which can be
      // applied to the shared map.
      Handle<Map> fresh = Map::CopyNormalized(fast_map, mode);

      DCHECK(memcmp(fresh->address(),
                    new_map->address(),
                    Map::kCodeCacheOffset) == 0);
      STATIC_ASSERT(Map::kDependentCodeOffset ==
                    Map::kCodeCacheOffset + kPointerSize);
      int offset = Map::kDependentCodeOffset + kPointerSize;
      DCHECK(memcmp(fresh->address() + offset,
                    new_map->address() + offset,
                    Map::kSize - offset) == 0);
    }
#endif
  } else {
    new_map = Map::CopyNormalized(fast_map, mode);
    if (use_cache) {
      cache->Set(fast_map, new_map);
      isolate->counters()->normalized_maps()->Increment();
    }
  }
  fast_map->NotifyLeafMapLayoutChange();
  return new_map;
}


Handle<Map> Map::CopyNormalized(Handle<Map> map,
                                PropertyNormalizationMode mode) {
  int new_instance_size = map->instance_size();
  if (mode == CLEAR_INOBJECT_PROPERTIES) {
    new_instance_size -= map->inobject_properties() * kPointerSize;
  }

  Handle<Map> result = RawCopy(map, new_instance_size);

  if (mode != CLEAR_INOBJECT_PROPERTIES) {
    result->set_inobject_properties(map->inobject_properties());
  }

  result->set_dictionary_map(true);
  result->set_migration_target(false);

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) result->DictionaryMapVerify();
#endif

  return result;
}


Handle<Map> Map::CopyDropDescriptors(Handle<Map> map) {
  Handle<Map> result = RawCopy(map, map->instance_size());

  // Please note instance_type and instance_size are set when allocated.
  result->set_inobject_properties(map->inobject_properties());
  result->set_unused_property_fields(map->unused_property_fields());

  result->set_pre_allocated_property_fields(
      map->pre_allocated_property_fields());
  result->ClearCodeCache(map->GetHeap());
  map->NotifyLeafMapLayoutChange();
  return result;
}


Handle<Map> Map::ShareDescriptor(Handle<Map> map,
                                 Handle<DescriptorArray> descriptors,
                                 Descriptor* descriptor) {
  // Sanity check. This path is only to be taken if the map owns its descriptor
  // array, implying that its NumberOfOwnDescriptors equals the number of
  // descriptors in the descriptor array.
  DCHECK(map->NumberOfOwnDescriptors() ==
         map->instance_descriptors()->number_of_descriptors());

  Handle<Map> result = CopyDropDescriptors(map);
  Handle<Name> name = descriptor->GetKey();

  // Ensure there's space for the new descriptor in the shared descriptor array.
  if (descriptors->NumberOfSlackDescriptors() == 0) {
    int old_size = descriptors->number_of_descriptors();
    if (old_size == 0) {
      descriptors = DescriptorArray::Allocate(map->GetIsolate(), 0, 1);
    } else {
      EnsureDescriptorSlack(map, old_size < 4 ? 1 : old_size / 2);
      descriptors = handle(map->instance_descriptors());
    }
  }

  {
    DisallowHeapAllocation no_gc;
    descriptors->Append(descriptor);
    result->InitializeDescriptors(*descriptors);
  }

  DCHECK(result->NumberOfOwnDescriptors() == map->NumberOfOwnDescriptors() + 1);
  ConnectTransition(map, result, name, SIMPLE_TRANSITION);

  return result;
}


void Map::ConnectTransition(Handle<Map> parent, Handle<Map> child,
                            Handle<Name> name, SimpleTransitionFlag flag) {
  parent->set_owns_descriptors(false);
  if (parent->is_prototype_map()) {
    DCHECK(child->is_prototype_map());
  } else {
    Handle<TransitionArray> transitions =
        TransitionArray::CopyInsert(parent, name, child, flag);
    parent->set_transitions(*transitions);
    child->SetBackPointer(*parent);
  }
}


Handle<Map> Map::CopyReplaceDescriptors(Handle<Map> map,
                                        Handle<DescriptorArray> descriptors,
                                        TransitionFlag flag,
                                        MaybeHandle<Name> maybe_name,
                                        SimpleTransitionFlag simple_flag) {
  DCHECK(descriptors->IsSortedNoDuplicates());

  Handle<Map> result = CopyDropDescriptors(map);
  result->InitializeDescriptors(*descriptors);

  if (!map->is_prototype_map()) {
    if (flag == INSERT_TRANSITION && map->CanHaveMoreTransitions()) {
      Handle<Name> name;
      CHECK(maybe_name.ToHandle(&name));
      ConnectTransition(map, result, name, simple_flag);
    } else {
      int length = descriptors->number_of_descriptors();
      for (int i = 0; i < length; i++) {
        descriptors->SetRepresentation(i, Representation::Tagged());
        if (descriptors->GetDetails(i).type() == FIELD) {
          descriptors->SetValue(i, HeapType::Any());
        }
      }
    }
  }

  return result;
}


// Since this method is used to rewrite an existing transition tree, it can
// always insert transitions without checking.
Handle<Map> Map::CopyInstallDescriptors(Handle<Map> map,
                                        int new_descriptor,
                                        Handle<DescriptorArray> descriptors) {
  DCHECK(descriptors->IsSortedNoDuplicates());

  Handle<Map> result = CopyDropDescriptors(map);

  result->InitializeDescriptors(*descriptors);
  result->SetNumberOfOwnDescriptors(new_descriptor + 1);

  int unused_property_fields = map->unused_property_fields();
  if (descriptors->GetDetails(new_descriptor).type() == FIELD) {
    unused_property_fields = map->unused_property_fields() - 1;
    if (unused_property_fields < 0) {
      unused_property_fields += JSObject::kFieldsAdded;
    }
  }

  result->set_unused_property_fields(unused_property_fields);

  Handle<Name> name = handle(descriptors->GetKey(new_descriptor));
  ConnectTransition(map, result, name, SIMPLE_TRANSITION);

  return result;
}


Handle<Map> Map::CopyAsElementsKind(Handle<Map> map, ElementsKind kind,
                                    TransitionFlag flag) {
  if (flag == INSERT_TRANSITION) {
    DCHECK(!map->HasElementsTransition() ||
        ((map->elements_transition_map()->elements_kind() ==
          DICTIONARY_ELEMENTS ||
          IsExternalArrayElementsKind(
              map->elements_transition_map()->elements_kind())) &&
         (kind == DICTIONARY_ELEMENTS ||
          IsExternalArrayElementsKind(kind))));
    DCHECK(!IsFastElementsKind(kind) ||
           IsMoreGeneralElementsKindTransition(map->elements_kind(), kind));
    DCHECK(kind != map->elements_kind());
  }

  bool insert_transition =
      flag == INSERT_TRANSITION && !map->HasElementsTransition();

  if (insert_transition && map->owns_descriptors()) {
    // In case the map owned its own descriptors, share the descriptors and
    // transfer ownership to the new map.
    Handle<Map> new_map = CopyDropDescriptors(map);

    ConnectElementsTransition(map, new_map);

    new_map->set_elements_kind(kind);
    new_map->InitializeDescriptors(map->instance_descriptors());
    return new_map;
  }

  // In case the map did not own its own descriptors, a split is forced by
  // copying the map; creating a new descriptor array cell.
  // Create a new free-floating map only if we are not allowed to store it.
  Handle<Map> new_map = Copy(map);

  new_map->set_elements_kind(kind);

  if (insert_transition) {
    ConnectElementsTransition(map, new_map);
  }

  return new_map;
}


Handle<Map> Map::CopyForObserved(Handle<Map> map) {
  DCHECK(!map->is_observed());

  Isolate* isolate = map->GetIsolate();

  // In case the map owned its own descriptors, share the descriptors and
  // transfer ownership to the new map.
  Handle<Map> new_map;
  if (map->owns_descriptors()) {
    new_map = CopyDropDescriptors(map);
  } else {
    DCHECK(!map->is_prototype_map());
    new_map = Copy(map);
  }

  new_map->set_is_observed();
  if (map->owns_descriptors()) {
    new_map->InitializeDescriptors(map->instance_descriptors());
  }

  Handle<Name> name = isolate->factory()->observed_symbol();
  ConnectTransition(map, new_map, name, FULL_TRANSITION);

  return new_map;
}


Handle<Map> Map::Copy(Handle<Map> map) {
  Handle<DescriptorArray> descriptors(map->instance_descriptors());
  int number_of_own_descriptors = map->NumberOfOwnDescriptors();
  Handle<DescriptorArray> new_descriptors =
      DescriptorArray::CopyUpTo(descriptors, number_of_own_descriptors);
  return CopyReplaceDescriptors(
      map, new_descriptors, OMIT_TRANSITION, MaybeHandle<Name>());
}


Handle<Map> Map::Create(Handle<JSFunction> constructor,
                        int extra_inobject_properties) {
  Handle<Map> copy = Copy(handle(constructor->initial_map()));

  // Check that we do not overflow the instance size when adding the
  // extra inobject properties.
  int instance_size_delta = extra_inobject_properties * kPointerSize;
  int max_instance_size_delta =
      JSObject::kMaxInstanceSize - copy->instance_size();
  int max_extra_properties = max_instance_size_delta >> kPointerSizeLog2;

  // If the instance size overflows, we allocate as many properties as we can as
  // inobject properties.
  if (extra_inobject_properties > max_extra_properties) {
    instance_size_delta = max_instance_size_delta;
    extra_inobject_properties = max_extra_properties;
  }

  // Adjust the map with the extra inobject properties.
  int inobject_properties =
      copy->inobject_properties() + extra_inobject_properties;
  copy->set_inobject_properties(inobject_properties);
  copy->set_unused_property_fields(inobject_properties);
  copy->set_instance_size(copy->instance_size() + instance_size_delta);
  copy->set_visitor_id(StaticVisitorBase::GetVisitorId(*copy));
  return copy;
}


Handle<Map> Map::CopyForFreeze(Handle<Map> map) {
  int num_descriptors = map->NumberOfOwnDescriptors();
  Isolate* isolate = map->GetIsolate();
  Handle<DescriptorArray> new_desc = DescriptorArray::CopyUpToAddAttributes(
      handle(map->instance_descriptors(), isolate), num_descriptors, FROZEN);
  Handle<Map> new_map = CopyReplaceDescriptors(
      map, new_desc, INSERT_TRANSITION, isolate->factory()->frozen_symbol());
  new_map->freeze();
  new_map->set_is_extensible(false);
  new_map->set_elements_kind(DICTIONARY_ELEMENTS);
  return new_map;
}


bool DescriptorArray::CanHoldValue(int descriptor, Object* value) {
  PropertyDetails details = GetDetails(descriptor);
  switch (details.type()) {
    case FIELD:
      return value->FitsRepresentation(details.representation()) &&
             GetFieldType(descriptor)->NowContains(value);

    case CONSTANT:
      DCHECK(GetConstant(descriptor) != value ||
             value->FitsRepresentation(details.representation()));
      return GetConstant(descriptor) == value;

    case CALLBACKS:
      return false;

    case NORMAL:
    case INTERCEPTOR:
    case HANDLER:
    case NONEXISTENT:
      break;
  }

  UNREACHABLE();
  return false;
}


Handle<Map> Map::PrepareForDataProperty(Handle<Map> map, int descriptor,
                                        Handle<Object> value) {
  // Dictionaries can store any property value.
  if (map->is_dictionary_map()) return map;

  // Migrate to the newest map before storing the property.
  if (map->is_deprecated()) map = Update(map);

  Handle<DescriptorArray> descriptors(map->instance_descriptors());

  if (descriptors->CanHoldValue(descriptor, *value)) return map;

  Isolate* isolate = map->GetIsolate();
  Representation representation = value->OptimalRepresentation();
  Handle<HeapType> type = value->OptimalType(isolate, representation);

  return GeneralizeRepresentation(map, descriptor, representation, type,
                                  FORCE_FIELD);
}


Handle<Map> Map::TransitionToDataProperty(Handle<Map> map, Handle<Name> name,
                                          Handle<Object> value,
                                          PropertyAttributes attributes,
                                          StoreFromKeyed store_mode) {
  // Dictionary maps can always have additional data properties.
  if (map->is_dictionary_map()) return map;

  // Migrate to the newest map before transitioning to the new property.
  if (map->is_deprecated()) map = Update(map);

  int index = map->SearchTransition(*name);
  if (index != TransitionArray::kNotFound) {
    Handle<Map> transition(map->GetTransition(index));
    int descriptor = transition->LastAdded();

    // TODO(verwaest): Handle attributes better.
    DescriptorArray* descriptors = transition->instance_descriptors();
    if (descriptors->GetDetails(descriptor).attributes() != attributes) {
      return CopyGeneralizeAllRepresentations(transition, descriptor,
                                              FORCE_FIELD, attributes,
                                              "attributes mismatch");
    }

    return Map::PrepareForDataProperty(transition, descriptor, value);
  }

  TransitionFlag flag = INSERT_TRANSITION;
  MaybeHandle<Map> maybe_map;
  if (value->IsJSFunction()) {
    maybe_map = Map::CopyWithConstant(map, name, value, attributes, flag);
  } else if (!map->TooManyFastProperties(store_mode)) {
    Isolate* isolate = name->GetIsolate();
    Representation representation = value->OptimalRepresentation();
    Handle<HeapType> type = value->OptimalType(isolate, representation);
    maybe_map =
        Map::CopyWithField(map, name, type, attributes, representation, flag);
  }

  Handle<Map> result;
  if (!maybe_map.ToHandle(&result)) {
    return Map::Normalize(map, CLEAR_INOBJECT_PROPERTIES);
  }

  return result;
}


Handle<Map> Map::CopyAddDescriptor(Handle<Map> map,
                                   Descriptor* descriptor,
                                   TransitionFlag flag) {
  Handle<DescriptorArray> descriptors(map->instance_descriptors());

  // Ensure the key is unique.
  descriptor->KeyToUniqueName();

  if (flag == INSERT_TRANSITION &&
      map->owns_descriptors() &&
      map->CanHaveMoreTransitions()) {
    return ShareDescriptor(map, descriptors, descriptor);
  }

  Handle<DescriptorArray> new_descriptors = DescriptorArray::CopyUpTo(
      descriptors, map->NumberOfOwnDescriptors(), 1);
  new_descriptors->Append(descriptor);

  return CopyReplaceDescriptors(
      map, new_descriptors, flag, descriptor->GetKey(), SIMPLE_TRANSITION);
}


Handle<Map> Map::CopyInsertDescriptor(Handle<Map> map,
                                      Descriptor* descriptor,
                                      TransitionFlag flag) {
  Handle<DescriptorArray> old_descriptors(map->instance_descriptors());

  // Ensure the key is unique.
  descriptor->KeyToUniqueName();

  // We replace the key if it is already present.
  int index = old_descriptors->SearchWithCache(*descriptor->GetKey(), *map);
  if (index != DescriptorArray::kNotFound) {
    return CopyReplaceDescriptor(map, old_descriptors, descriptor, index, flag);
  }
  return CopyAddDescriptor(map, descriptor, flag);
}


Handle<DescriptorArray> DescriptorArray::CopyUpTo(
    Handle<DescriptorArray> desc,
    int enumeration_index,
    int slack) {
  return DescriptorArray::CopyUpToAddAttributes(
      desc, enumeration_index, NONE, slack);
}


Handle<DescriptorArray> DescriptorArray::CopyUpToAddAttributes(
    Handle<DescriptorArray> desc,
    int enumeration_index,
    PropertyAttributes attributes,
    int slack) {
  if (enumeration_index + slack == 0) {
    return desc->GetIsolate()->factory()->empty_descriptor_array();
  }

  int size = enumeration_index;

  Handle<DescriptorArray> descriptors =
      DescriptorArray::Allocate(desc->GetIsolate(), size, slack);
  DescriptorArray::WhitenessWitness witness(*descriptors);

  if (attributes != NONE) {
    for (int i = 0; i < size; ++i) {
      Object* value = desc->GetValue(i);
      Name* key = desc->GetKey(i);
      PropertyDetails details = desc->GetDetails(i);
      // Bulk attribute changes never affect private properties.
      if (!key->IsSymbol() || !Symbol::cast(key)->is_private()) {
        int mask = DONT_DELETE | DONT_ENUM;
        // READ_ONLY is an invalid attribute for JS setters/getters.
        if (details.type() != CALLBACKS || !value->IsAccessorPair()) {
          mask |= READ_ONLY;
        }
        details = details.CopyAddAttributes(
            static_cast<PropertyAttributes>(attributes & mask));
      }
      Descriptor inner_desc(
          handle(key), handle(value, desc->GetIsolate()), details);
      descriptors->Set(i, &inner_desc, witness);
    }
  } else {
    for (int i = 0; i < size; ++i) {
      descriptors->CopyFrom(i, *desc, witness);
    }
  }

  if (desc->number_of_descriptors() != enumeration_index) descriptors->Sort();

  return descriptors;
}


Handle<Map> Map::CopyReplaceDescriptor(Handle<Map> map,
                                       Handle<DescriptorArray> descriptors,
                                       Descriptor* descriptor,
                                       int insertion_index,
                                       TransitionFlag flag) {
  // Ensure the key is unique.
  descriptor->KeyToUniqueName();

  Handle<Name> key = descriptor->GetKey();
  DCHECK(*key == descriptors->GetKey(insertion_index));

  Handle<DescriptorArray> new_descriptors = DescriptorArray::CopyUpTo(
      descriptors, map->NumberOfOwnDescriptors());

  new_descriptors->Replace(insertion_index, descriptor);

  SimpleTransitionFlag simple_flag =
      (insertion_index == descriptors->number_of_descriptors() - 1)
      ? SIMPLE_TRANSITION
      : FULL_TRANSITION;
  return CopyReplaceDescriptors(map, new_descriptors, flag, key, simple_flag);
}


void Map::UpdateCodeCache(Handle<Map> map,
                          Handle<Name> name,
                          Handle<Code> code) {
  Isolate* isolate = map->GetIsolate();
  HandleScope scope(isolate);
  // Allocate the code cache if not present.
  if (map->code_cache()->IsFixedArray()) {
    Handle<Object> result = isolate->factory()->NewCodeCache();
    map->set_code_cache(*result);
  }

  // Update the code cache.
  Handle<CodeCache> code_cache(CodeCache::cast(map->code_cache()), isolate);
  CodeCache::Update(code_cache, name, code);
}


Object* Map::FindInCodeCache(Name* name, Code::Flags flags) {
  // Do a lookup if a code cache exists.
  if (!code_cache()->IsFixedArray()) {
    return CodeCache::cast(code_cache())->Lookup(name, flags);
  } else {
    return GetHeap()->undefined_value();
  }
}


int Map::IndexInCodeCache(Object* name, Code* code) {
  // Get the internal index if a code cache exists.
  if (!code_cache()->IsFixedArray()) {
    return CodeCache::cast(code_cache())->GetIndex(name, code);
  }
  return -1;
}


void Map::RemoveFromCodeCache(Name* name, Code* code, int index) {
  // No GC is supposed to happen between a call to IndexInCodeCache and
  // RemoveFromCodeCache so the code cache must be there.
  DCHECK(!code_cache()->IsFixedArray());
  CodeCache::cast(code_cache())->RemoveByIndex(name, code, index);
}


// An iterator over all map transitions in an descriptor array, reusing the
// constructor field of the map while it is running. Negative values in
// the constructor field indicate an active map transition iteration. The
// original constructor is restored after iterating over all entries.
class IntrusiveMapTransitionIterator {
 public:
  IntrusiveMapTransitionIterator(
      Map* map, TransitionArray* transition_array, Object* constructor)
      : map_(map),
        transition_array_(transition_array),
        constructor_(constructor) { }

  void StartIfNotStarted() {
    DCHECK(!(*IteratorField())->IsSmi() || IsIterating());
    if (!(*IteratorField())->IsSmi()) {
      DCHECK(*IteratorField() == constructor_);
      *IteratorField() = Smi::FromInt(-1);
    }
  }

  bool IsIterating() {
    return (*IteratorField())->IsSmi() &&
           Smi::cast(*IteratorField())->value() < 0;
  }

  Map* Next() {
    DCHECK(IsIterating());
    int value = Smi::cast(*IteratorField())->value();
    int index = -value - 1;
    int number_of_transitions = transition_array_->number_of_transitions();
    while (index < number_of_transitions) {
      *IteratorField() = Smi::FromInt(value - 1);
      return transition_array_->GetTarget(index);
    }

    *IteratorField() = constructor_;
    return NULL;
  }

 private:
  Object** IteratorField() {
    return HeapObject::RawField(map_, Map::kConstructorOffset);
  }

  Map* map_;
  TransitionArray* transition_array_;
  Object* constructor_;
};


// An iterator over all prototype transitions, reusing the constructor field
// of the map while it is running.  Positive values in the constructor field
// indicate an active prototype transition iteration. The original constructor
// is restored after iterating over all entries.
class IntrusivePrototypeTransitionIterator {
 public:
  IntrusivePrototypeTransitionIterator(
      Map* map, HeapObject* proto_trans, Object* constructor)
      : map_(map), proto_trans_(proto_trans), constructor_(constructor) { }

  void StartIfNotStarted() {
    if (!(*IteratorField())->IsSmi()) {
      DCHECK(*IteratorField() == constructor_);
      *IteratorField() = Smi::FromInt(0);
    }
  }

  bool IsIterating() {
    return (*IteratorField())->IsSmi() &&
           Smi::cast(*IteratorField())->value() >= 0;
  }

  Map* Next() {
    DCHECK(IsIterating());
    int transitionNumber = Smi::cast(*IteratorField())->value();
    if (transitionNumber < NumberOfTransitions()) {
      *IteratorField() = Smi::FromInt(transitionNumber + 1);
      return GetTransition(transitionNumber);
    }
    *IteratorField() = constructor_;
    return NULL;
  }

 private:
  Object** IteratorField() {
    return HeapObject::RawField(map_, Map::kConstructorOffset);
  }

  int NumberOfTransitions() {
    FixedArray* proto_trans = reinterpret_cast<FixedArray*>(proto_trans_);
    Object* num = proto_trans->get(Map::kProtoTransitionNumberOfEntriesOffset);
    return Smi::cast(num)->value();
  }

  Map* GetTransition(int transitionNumber) {
    FixedArray* proto_trans = reinterpret_cast<FixedArray*>(proto_trans_);
    return Map::cast(proto_trans->get(IndexFor(transitionNumber)));
  }

  int IndexFor(int transitionNumber) {
    return Map::kProtoTransitionHeaderSize +
        Map::kProtoTransitionMapOffset +
        transitionNumber * Map::kProtoTransitionElementsPerEntry;
  }

  Map* map_;
  HeapObject* proto_trans_;
  Object* constructor_;
};


// To traverse the transition tree iteratively, we have to store two kinds of
// information in a map: The parent map in the traversal and which children of a
// node have already been visited. To do this without additional memory, we
// temporarily reuse two fields with known values:
//
//  (1) The map of the map temporarily holds the parent, and is restored to the
//      meta map afterwards.
//
//  (2) The info which children have already been visited depends on which part
//      of the map we currently iterate. We use the constructor field of the
//      map to store the current index. We can do that because the constructor
//      is the same for all involved maps.
//
//    (a) If we currently follow normal map transitions, we temporarily store
//        the current index in the constructor field, and restore it to the
//        original constructor afterwards. Note that a single descriptor can
//        have 0, 1, or 2 transitions.
//
//    (b) If we currently follow prototype transitions, we temporarily store
//        the current index in the constructor field, and restore it to the
//        original constructor afterwards.
//
// Note that the child iterator is just a concatenation of two iterators: One
// iterating over map transitions and one iterating over prototype transisitons.
class TraversableMap : public Map {
 public:
  // Record the parent in the traversal within this map. Note that this destroys
  // this map's map!
  void SetParent(TraversableMap* parent) { set_map_no_write_barrier(parent); }

  // Reset the current map's map, returning the parent previously stored in it.
  TraversableMap* GetAndResetParent() {
    TraversableMap* old_parent = static_cast<TraversableMap*>(map());
    set_map_no_write_barrier(GetHeap()->meta_map());
    return old_parent;
  }

  // If we have an unvisited child map, return that one and advance. If we have
  // none, return NULL and restore the overwritten constructor field.
  TraversableMap* ChildIteratorNext(Object* constructor) {
    if (!HasTransitionArray()) return NULL;

    TransitionArray* transition_array = transitions();
    if (transition_array->HasPrototypeTransitions()) {
      HeapObject* proto_transitions =
          transition_array->GetPrototypeTransitions();
      IntrusivePrototypeTransitionIterator proto_iterator(this,
                                                          proto_transitions,
                                                          constructor);
      proto_iterator.StartIfNotStarted();
      if (proto_iterator.IsIterating()) {
        Map* next = proto_iterator.Next();
        if (next != NULL) return static_cast<TraversableMap*>(next);
      }
    }

    IntrusiveMapTransitionIterator transition_iterator(this,
                                                       transition_array,
                                                       constructor);
    transition_iterator.StartIfNotStarted();
    if (transition_iterator.IsIterating()) {
      Map* next = transition_iterator.Next();
      if (next != NULL) return static_cast<TraversableMap*>(next);
    }

    return NULL;
  }
};


// Traverse the transition tree in postorder without using the C++ stack by
// doing pointer reversal.
void Map::TraverseTransitionTree(TraverseCallback callback, void* data) {
  // Make sure that we do not allocate in the callback.
  DisallowHeapAllocation no_allocation;

  TraversableMap* current = static_cast<TraversableMap*>(this);
  // Get the root constructor here to restore it later when finished iterating
  // over maps.
  Object* root_constructor = constructor();
  while (true) {
    TraversableMap* child = current->ChildIteratorNext(root_constructor);
    if (child != NULL) {
      child->SetParent(current);
      current = child;
    } else {
      TraversableMap* parent = current->GetAndResetParent();
      callback(current, data);
      if (current == this) break;
      current = parent;
    }
  }
}


void CodeCache::Update(
    Handle<CodeCache> code_cache, Handle<Name> name, Handle<Code> code) {
  // The number of monomorphic stubs for normal load/store/call IC's can grow to
  // a large number and therefore they need to go into a hash table. They are
  // used to load global properties from cells.
  if (code->type() == Code::NORMAL) {
    // Make sure that a hash table is allocated for the normal load code cache.
    if (code_cache->normal_type_cache()->IsUndefined()) {
      Handle<Object> result =
          CodeCacheHashTable::New(code_cache->GetIsolate(),
                                  CodeCacheHashTable::kInitialSize);
      code_cache->set_normal_type_cache(*result);
    }
    UpdateNormalTypeCache(code_cache, name, code);
  } else {
    DCHECK(code_cache->default_cache()->IsFixedArray());
    UpdateDefaultCache(code_cache, name, code);
  }
}


void CodeCache::UpdateDefaultCache(
    Handle<CodeCache> code_cache, Handle<Name> name, Handle<Code> code) {
  // When updating the default code cache we disregard the type encoded in the
  // flags. This allows call constant stubs to overwrite call field
  // stubs, etc.
  Code::Flags flags = Code::RemoveTypeFromFlags(code->flags());

  // First check whether we can update existing code cache without
  // extending it.
  Handle<FixedArray> cache = handle(code_cache->default_cache());
  int length = cache->length();
  {
    DisallowHeapAllocation no_alloc;
    int deleted_index = -1;
    for (int i = 0; i < length; i += kCodeCacheEntrySize) {
      Object* key = cache->get(i);
      if (key->IsNull()) {
        if (deleted_index < 0) deleted_index = i;
        continue;
      }
      if (key->IsUndefined()) {
        if (deleted_index >= 0) i = deleted_index;
        cache->set(i + kCodeCacheEntryNameOffset, *name);
        cache->set(i + kCodeCacheEntryCodeOffset, *code);
        return;
      }
      if (name->Equals(Name::cast(key))) {
        Code::Flags found =
            Code::cast(cache->get(i + kCodeCacheEntryCodeOffset))->flags();
        if (Code::RemoveTypeFromFlags(found) == flags) {
          cache->set(i + kCodeCacheEntryCodeOffset, *code);
          return;
        }
      }
    }

    // Reached the end of the code cache.  If there were deleted
    // elements, reuse the space for the first of them.
    if (deleted_index >= 0) {
      cache->set(deleted_index + kCodeCacheEntryNameOffset, *name);
      cache->set(deleted_index + kCodeCacheEntryCodeOffset, *code);
      return;
    }
  }

  // Extend the code cache with some new entries (at least one). Must be a
  // multiple of the entry size.
  int new_length = length + ((length >> 1)) + kCodeCacheEntrySize;
  new_length = new_length - new_length % kCodeCacheEntrySize;
  DCHECK((new_length % kCodeCacheEntrySize) == 0);
  cache = FixedArray::CopySize(cache, new_length);

  // Add the (name, code) pair to the new cache.
  cache->set(length + kCodeCacheEntryNameOffset, *name);
  cache->set(length + kCodeCacheEntryCodeOffset, *code);
  code_cache->set_default_cache(*cache);
}


void CodeCache::UpdateNormalTypeCache(
    Handle<CodeCache> code_cache, Handle<Name> name, Handle<Code> code) {
  // Adding a new entry can cause a new cache to be allocated.
  Handle<CodeCacheHashTable> cache(
      CodeCacheHashTable::cast(code_cache->normal_type_cache()));
  Handle<Object> new_cache = CodeCacheHashTable::Put(cache, name, code);
  code_cache->set_normal_type_cache(*new_cache);
}


Object* CodeCache::Lookup(Name* name, Code::Flags flags) {
  Object* result = LookupDefaultCache(name, Code::RemoveTypeFromFlags(flags));
  if (result->IsCode()) {
    if (Code::cast(result)->flags() == flags) return result;
    return GetHeap()->undefined_value();
  }
  return LookupNormalTypeCache(name, flags);
}


Object* CodeCache::LookupDefaultCache(Name* name, Code::Flags flags) {
  FixedArray* cache = default_cache();
  int length = cache->length();
  for (int i = 0; i < length; i += kCodeCacheEntrySize) {
    Object* key = cache->get(i + kCodeCacheEntryNameOffset);
    // Skip deleted elements.
    if (key->IsNull()) continue;
    if (key->IsUndefined()) return key;
    if (name->Equals(Name::cast(key))) {
      Code* code = Code::cast(cache->get(i + kCodeCacheEntryCodeOffset));
      if (Code::RemoveTypeFromFlags(code->flags()) == flags) {
        return code;
      }
    }
  }
  return GetHeap()->undefined_value();
}


Object* CodeCache::LookupNormalTypeCache(Name* name, Code::Flags flags) {
  if (!normal_type_cache()->IsUndefined()) {
    CodeCacheHashTable* cache = CodeCacheHashTable::cast(normal_type_cache());
    return cache->Lookup(name, flags);
  } else {
    return GetHeap()->undefined_value();
  }
}


int CodeCache::GetIndex(Object* name, Code* code) {
  if (code->type() == Code::NORMAL) {
    if (normal_type_cache()->IsUndefined()) return -1;
    CodeCacheHashTable* cache = CodeCacheHashTable::cast(normal_type_cache());
    return cache->GetIndex(Name::cast(name), code->flags());
  }

  FixedArray* array = default_cache();
  int len = array->length();
  for (int i = 0; i < len; i += kCodeCacheEntrySize) {
    if (array->get(i + kCodeCacheEntryCodeOffset) == code) return i + 1;
  }
  return -1;
}


void CodeCache::RemoveByIndex(Object* name, Code* code, int index) {
  if (code->type() == Code::NORMAL) {
    DCHECK(!normal_type_cache()->IsUndefined());
    CodeCacheHashTable* cache = CodeCacheHashTable::cast(normal_type_cache());
    DCHECK(cache->GetIndex(Name::cast(name), code->flags()) == index);
    cache->RemoveByIndex(index);
  } else {
    FixedArray* array = default_cache();
    DCHECK(array->length() >= index && array->get(index)->IsCode());
    // Use null instead of undefined for deleted elements to distinguish
    // deleted elements from unused elements.  This distinction is used
    // when looking up in the cache and when updating the cache.
    DCHECK_EQ(1, kCodeCacheEntryCodeOffset - kCodeCacheEntryNameOffset);
    array->set_null(index - 1);  // Name.
    array->set_null(index);  // Code.
  }
}


// The key in the code cache hash table consists of the property name and the
// code object. The actual match is on the name and the code flags. If a key
// is created using the flags and not a code object it can only be used for
// lookup not to create a new entry.
class CodeCacheHashTableKey : public HashTableKey {
 public:
  CodeCacheHashTableKey(Handle<Name> name, Code::Flags flags)
      : name_(name), flags_(flags), code_() { }

  CodeCacheHashTableKey(Handle<Name> name, Handle<Code> code)
      : name_(name), flags_(code->flags()), code_(code) { }

  bool IsMatch(Object* other) V8_OVERRIDE {
    if (!other->IsFixedArray()) return false;
    FixedArray* pair = FixedArray::cast(other);
    Name* name = Name::cast(pair->get(0));
    Code::Flags flags = Code::cast(pair->get(1))->flags();
    if (flags != flags_) {
      return false;
    }
    return name_->Equals(name);
  }

  static uint32_t NameFlagsHashHelper(Name* name, Code::Flags flags) {
    return name->Hash() ^ flags;
  }

  uint32_t Hash() V8_OVERRIDE { return NameFlagsHashHelper(*name_, flags_); }

  uint32_t HashForObject(Object* obj) V8_OVERRIDE {
    FixedArray* pair = FixedArray::cast(obj);
    Name* name = Name::cast(pair->get(0));
    Code* code = Code::cast(pair->get(1));
    return NameFlagsHashHelper(name, code->flags());
  }

  MUST_USE_RESULT Handle<Object> AsHandle(Isolate* isolate) V8_OVERRIDE {
    Handle<Code> code = code_.ToHandleChecked();
    Handle<FixedArray> pair = isolate->factory()->NewFixedArray(2);
    pair->set(0, *name_);
    pair->set(1, *code);
    return pair;
  }

 private:
  Handle<Name> name_;
  Code::Flags flags_;
  // TODO(jkummerow): We should be able to get by without this.
  MaybeHandle<Code> code_;
};


Object* CodeCacheHashTable::Lookup(Name* name, Code::Flags flags) {
  DisallowHeapAllocation no_alloc;
  CodeCacheHashTableKey key(handle(name), flags);
  int entry = FindEntry(&key);
  if (entry == kNotFound) return GetHeap()->undefined_value();
  return get(EntryToIndex(entry) + 1);
}


Handle<CodeCacheHashTable> CodeCacheHashTable::Put(
    Handle<CodeCacheHashTable> cache, Handle<Name> name, Handle<Code> code) {
  CodeCacheHashTableKey key(name, code);

  Handle<CodeCacheHashTable> new_cache = EnsureCapacity(cache, 1, &key);

  int entry = new_cache->FindInsertionEntry(key.Hash());
  Handle<Object> k = key.AsHandle(cache->GetIsolate());

  new_cache->set(EntryToIndex(entry), *k);
  new_cache->set(EntryToIndex(entry) + 1, *code);
  new_cache->ElementAdded();
  return new_cache;
}


int CodeCacheHashTable::GetIndex(Name* name, Code::Flags flags) {
  DisallowHeapAllocation no_alloc;
  CodeCacheHashTableKey key(handle(name), flags);
  int entry = FindEntry(&key);
  return (entry == kNotFound) ? -1 : entry;
}


void CodeCacheHashTable::RemoveByIndex(int index) {
  DCHECK(index >= 0);
  Heap* heap = GetHeap();
  set(EntryToIndex(index), heap->the_hole_value());
  set(EntryToIndex(index) + 1, heap->the_hole_value());
  ElementRemoved();
}


void PolymorphicCodeCache::Update(Handle<PolymorphicCodeCache> code_cache,
                                  MapHandleList* maps,
                                  Code::Flags flags,
                                  Handle<Code> code) {
  Isolate* isolate = code_cache->GetIsolate();
  if (code_cache->cache()->IsUndefined()) {
    Handle<PolymorphicCodeCacheHashTable> result =
        PolymorphicCodeCacheHashTable::New(
            isolate,
            PolymorphicCodeCacheHashTable::kInitialSize);
    code_cache->set_cache(*result);
  } else {
    // This entry shouldn't be contained in the cache yet.
    DCHECK(PolymorphicCodeCacheHashTable::cast(code_cache->cache())
               ->Lookup(maps, flags)->IsUndefined());
  }
  Handle<PolymorphicCodeCacheHashTable> hash_table =
      handle(PolymorphicCodeCacheHashTable::cast(code_cache->cache()));
  Handle<PolymorphicCodeCacheHashTable> new_cache =
      PolymorphicCodeCacheHashTable::Put(hash_table, maps, flags, code);
  code_cache->set_cache(*new_cache);
}


Handle<Object> PolymorphicCodeCache::Lookup(MapHandleList* maps,
                                            Code::Flags flags) {
  if (!cache()->IsUndefined()) {
    PolymorphicCodeCacheHashTable* hash_table =
        PolymorphicCodeCacheHashTable::cast(cache());
    return Handle<Object>(hash_table->Lookup(maps, flags), GetIsolate());
  } else {
    return GetIsolate()->factory()->undefined_value();
  }
}


// Despite their name, object of this class are not stored in the actual
// hash table; instead they're temporarily used for lookups. It is therefore
// safe to have a weak (non-owning) pointer to a MapList as a member field.
class PolymorphicCodeCacheHashTableKey : public HashTableKey {
 public:
  // Callers must ensure that |maps| outlives the newly constructed object.
  PolymorphicCodeCacheHashTableKey(MapHandleList* maps, int code_flags)
      : maps_(maps),
        code_flags_(code_flags) {}

  bool IsMatch(Object* other) V8_OVERRIDE {
    MapHandleList other_maps(kDefaultListAllocationSize);
    int other_flags;
    FromObject(other, &other_flags, &other_maps);
    if (code_flags_ != other_flags) return false;
    if (maps_->length() != other_maps.length()) return false;
    // Compare just the hashes first because it's faster.
    int this_hash = MapsHashHelper(maps_, code_flags_);
    int other_hash = MapsHashHelper(&other_maps, other_flags);
    if (this_hash != other_hash) return false;

    // Full comparison: for each map in maps_, look for an equivalent map in
    // other_maps. This implementation is slow, but probably good enough for
    // now because the lists are short (<= 4 elements currently).
    for (int i = 0; i < maps_->length(); ++i) {
      bool match_found = false;
      for (int j = 0; j < other_maps.length(); ++j) {
        if (*(maps_->at(i)) == *(other_maps.at(j))) {
          match_found = true;
          break;
        }
      }
      if (!match_found) return false;
    }
    return true;
  }

  static uint32_t MapsHashHelper(MapHandleList* maps, int code_flags) {
    uint32_t hash = code_flags;
    for (int i = 0; i < maps->length(); ++i) {
      hash ^= maps->at(i)->Hash();
    }
    return hash;
  }

  uint32_t Hash() V8_OVERRIDE {
    return MapsHashHelper(maps_, code_flags_);
  }

  uint32_t HashForObject(Object* obj) V8_OVERRIDE {
    MapHandleList other_maps(kDefaultListAllocationSize);
    int other_flags;
    FromObject(obj, &other_flags, &other_maps);
    return MapsHashHelper(&other_maps, other_flags);
  }

  MUST_USE_RESULT Handle<Object> AsHandle(Isolate* isolate) V8_OVERRIDE {
    // The maps in |maps_| must be copied to a newly allocated FixedArray,
    // both because the referenced MapList is short-lived, and because C++
    // objects can't be stored in the heap anyway.
    Handle<FixedArray> list =
        isolate->factory()->NewUninitializedFixedArray(maps_->length() + 1);
    list->set(0, Smi::FromInt(code_flags_));
    for (int i = 0; i < maps_->length(); ++i) {
      list->set(i + 1, *maps_->at(i));
    }
    return list;
  }

 private:
  static MapHandleList* FromObject(Object* obj,
                                   int* code_flags,
                                   MapHandleList* maps) {
    FixedArray* list = FixedArray::cast(obj);
    maps->Rewind(0);
    *code_flags = Smi::cast(list->get(0))->value();
    for (int i = 1; i < list->length(); ++i) {
      maps->Add(Handle<Map>(Map::cast(list->get(i))));
    }
    return maps;
  }

  MapHandleList* maps_;  // weak.
  int code_flags_;
  static const int kDefaultListAllocationSize = kMaxKeyedPolymorphism + 1;
};


Object* PolymorphicCodeCacheHashTable::Lookup(MapHandleList* maps,
                                              int code_kind) {
  DisallowHeapAllocation no_alloc;
  PolymorphicCodeCacheHashTableKey key(maps, code_kind);
  int entry = FindEntry(&key);
  if (entry == kNotFound) return GetHeap()->undefined_value();
  return get(EntryToIndex(entry) + 1);
}


Handle<PolymorphicCodeCacheHashTable> PolymorphicCodeCacheHashTable::Put(
      Handle<PolymorphicCodeCacheHashTable> hash_table,
      MapHandleList* maps,
      int code_kind,
      Handle<Code> code) {
  PolymorphicCodeCacheHashTableKey key(maps, code_kind);
  Handle<PolymorphicCodeCacheHashTable> cache =
      EnsureCapacity(hash_table, 1, &key);
  int entry = cache->FindInsertionEntry(key.Hash());

  Handle<Object> obj = key.AsHandle(hash_table->GetIsolate());
  cache->set(EntryToIndex(entry), *obj);
  cache->set(EntryToIndex(entry) + 1, *code);
  cache->ElementAdded();
  return cache;
}


void FixedArray::Shrink(int new_length) {
  DCHECK(0 <= new_length && new_length <= length());
  if (new_length < length()) {
    GetHeap()->RightTrimFixedArray<Heap::FROM_MUTATOR>(
        this, length() - new_length);
  }
}


MaybeHandle<FixedArray> FixedArray::AddKeysFromArrayLike(
    Handle<FixedArray> content,
    Handle<JSObject> array) {
  DCHECK(array->IsJSArray() || array->HasSloppyArgumentsElements());
  ElementsAccessor* accessor = array->GetElementsAccessor();
  Handle<FixedArray> result;
  ASSIGN_RETURN_ON_EXCEPTION(
      array->GetIsolate(), result,
      accessor->AddElementsToFixedArray(array, array, content),
      FixedArray);

#ifdef ENABLE_SLOW_DCHECKS
  if (FLAG_enable_slow_asserts) {
    DisallowHeapAllocation no_allocation;
    for (int i = 0; i < result->length(); i++) {
      Object* current = result->get(i);
      DCHECK(current->IsNumber() || current->IsName());
    }
  }
#endif
  return result;
}


MaybeHandle<FixedArray> FixedArray::UnionOfKeys(Handle<FixedArray> first,
                                                Handle<FixedArray> second) {
  ElementsAccessor* accessor = ElementsAccessor::ForArray(second);
  Handle<FixedArray> result;
  ASSIGN_RETURN_ON_EXCEPTION(
      first->GetIsolate(), result,
      accessor->AddElementsToFixedArray(
          Handle<Object>::null(),     // receiver
          Handle<JSObject>::null(),   // holder
          first,
          Handle<FixedArrayBase>::cast(second)),
      FixedArray);

#ifdef ENABLE_SLOW_DCHECKS
  if (FLAG_enable_slow_asserts) {
    DisallowHeapAllocation no_allocation;
    for (int i = 0; i < result->length(); i++) {
      Object* current = result->get(i);
      DCHECK(current->IsNumber() || current->IsName());
    }
  }
#endif
  return result;
}


Handle<FixedArray> FixedArray::CopySize(
    Handle<FixedArray> array, int new_length, PretenureFlag pretenure) {
  Isolate* isolate = array->GetIsolate();
  if (new_length == 0) return isolate->factory()->empty_fixed_array();
  Handle<FixedArray> result =
      isolate->factory()->NewFixedArray(new_length, pretenure);
  // Copy the content
  DisallowHeapAllocation no_gc;
  int len = array->length();
  if (new_length < len) len = new_length;
  // We are taking the map from the old fixed array so the map is sure to
  // be an immortal immutable object.
  result->set_map_no_write_barrier(array->map());
  WriteBarrierMode mode = result->GetWriteBarrierMode(no_gc);
  for (int i = 0; i < len; i++) {
    result->set(i, array->get(i), mode);
  }
  return result;
}


void FixedArray::CopyTo(int pos, FixedArray* dest, int dest_pos, int len) {
  DisallowHeapAllocation no_gc;
  WriteBarrierMode mode = dest->GetWriteBarrierMode(no_gc);
  for (int index = 0; index < len; index++) {
    dest->set(dest_pos+index, get(pos+index), mode);
  }
}


#ifdef DEBUG
bool FixedArray::IsEqualTo(FixedArray* other) {
  if (length() != other->length()) return false;
  for (int i = 0 ; i < length(); ++i) {
    if (get(i) != other->get(i)) return false;
  }
  return true;
}
#endif


Handle<DescriptorArray> DescriptorArray::Allocate(Isolate* isolate,
                                                  int number_of_descriptors,
                                                  int slack) {
  DCHECK(0 <= number_of_descriptors);
  Factory* factory = isolate->factory();
  // Do not use DescriptorArray::cast on incomplete object.
  int size = number_of_descriptors + slack;
  if (size == 0) return factory->empty_descriptor_array();
  // Allocate the array of keys.
  Handle<FixedArray> result = factory->NewFixedArray(LengthFor(size));

  result->set(kDescriptorLengthIndex, Smi::FromInt(number_of_descriptors));
  result->set(kEnumCacheIndex, Smi::FromInt(0));
  return Handle<DescriptorArray>::cast(result);
}


void DescriptorArray::ClearEnumCache() {
  set(kEnumCacheIndex, Smi::FromInt(0));
}


void DescriptorArray::Replace(int index, Descriptor* descriptor) {
  descriptor->SetSortedKeyIndex(GetSortedKeyIndex(index));
  Set(index, descriptor);
}


void DescriptorArray::SetEnumCache(FixedArray* bridge_storage,
                                   FixedArray* new_cache,
                                   Object* new_index_cache) {
  DCHECK(bridge_storage->length() >= kEnumCacheBridgeLength);
  DCHECK(new_index_cache->IsSmi() || new_index_cache->IsFixedArray());
  DCHECK(!IsEmpty());
  DCHECK(!HasEnumCache() || new_cache->length() > GetEnumCache()->length());
  FixedArray::cast(bridge_storage)->
    set(kEnumCacheBridgeCacheIndex, new_cache);
  FixedArray::cast(bridge_storage)->
    set(kEnumCacheBridgeIndicesCacheIndex, new_index_cache);
  set(kEnumCacheIndex, bridge_storage);
}


void DescriptorArray::CopyFrom(int index,
                               DescriptorArray* src,
                               const WhitenessWitness& witness) {
  Object* value = src->GetValue(index);
  PropertyDetails details = src->GetDetails(index);
  Descriptor desc(handle(src->GetKey(index)),
                  handle(value, src->GetIsolate()),
                  details);
  Set(index, &desc, witness);
}


// We need the whiteness witness since sort will reshuffle the entries in the
// descriptor array. If the descriptor array were to be black, the shuffling
// would move a slot that was already recorded as pointing into an evacuation
// candidate. This would result in missing updates upon evacuation.
void DescriptorArray::Sort() {
  // In-place heap sort.
  int len = number_of_descriptors();
  // Reset sorting since the descriptor array might contain invalid pointers.
  for (int i = 0; i < len; ++i) SetSortedKey(i, i);
  // Bottom-up max-heap construction.
  // Index of the last node with children
  const int max_parent_index = (len / 2) - 1;
  for (int i = max_parent_index; i >= 0; --i) {
    int parent_index = i;
    const uint32_t parent_hash = GetSortedKey(i)->Hash();
    while (parent_index <= max_parent_index) {
      int child_index = 2 * parent_index + 1;
      uint32_t child_hash = GetSortedKey(child_index)->Hash();
      if (child_index + 1 < len) {
        uint32_t right_child_hash = GetSortedKey(child_index + 1)->Hash();
        if (right_child_hash > child_hash) {
          child_index++;
          child_hash = right_child_hash;
        }
      }
      if (child_hash <= parent_hash) break;
      SwapSortedKeys(parent_index, child_index);
      // Now element at child_index could be < its children.
      parent_index = child_index;  // parent_hash remains correct.
    }
  }

  // Extract elements and create sorted array.
  for (int i = len - 1; i > 0; --i) {
    // Put max element at the back of the array.
    SwapSortedKeys(0, i);
    // Shift down the new top element.
    int parent_index = 0;
    const uint32_t parent_hash = GetSortedKey(parent_index)->Hash();
    const int max_parent_index = (i / 2) - 1;
    while (parent_index <= max_parent_index) {
      int child_index = parent_index * 2 + 1;
      uint32_t child_hash = GetSortedKey(child_index)->Hash();
      if (child_index + 1 < i) {
        uint32_t right_child_hash = GetSortedKey(child_index + 1)->Hash();
        if (right_child_hash > child_hash) {
          child_index++;
          child_hash = right_child_hash;
        }
      }
      if (child_hash <= parent_hash) break;
      SwapSortedKeys(parent_index, child_index);
      parent_index = child_index;
    }
  }
  DCHECK(IsSortedNoDuplicates());
}


Handle<AccessorPair> AccessorPair::Copy(Handle<AccessorPair> pair) {
  Handle<AccessorPair> copy = pair->GetIsolate()->factory()->NewAccessorPair();
  copy->set_getter(pair->getter());
  copy->set_setter(pair->setter());
  return copy;
}


Object* AccessorPair::GetComponent(AccessorComponent component) {
  Object* accessor = get(component);
  return accessor->IsTheHole() ? GetHeap()->undefined_value() : accessor;
}


Handle<DeoptimizationInputData> DeoptimizationInputData::New(
    Isolate* isolate, int deopt_entry_count, int return_patch_address_count,
    PretenureFlag pretenure) {
  DCHECK(deopt_entry_count + return_patch_address_count > 0);
  Handle<FixedArray> deoptimization_data =
      Handle<FixedArray>::cast(isolate->factory()->NewFixedArray(
          LengthFor(deopt_entry_count, return_patch_address_count), pretenure));
  deoptimization_data->set(kDeoptEntryCountIndex,
                           Smi::FromInt(deopt_entry_count));
  deoptimization_data->set(kReturnAddressPatchEntryCountIndex,
                           Smi::FromInt(return_patch_address_count));
  return Handle<DeoptimizationInputData>::cast(deoptimization_data);
}


Handle<DeoptimizationOutputData> DeoptimizationOutputData::New(
    Isolate* isolate,
    int number_of_deopt_points,
    PretenureFlag pretenure) {
  Handle<FixedArray> result;
  if (number_of_deopt_points == 0) {
    result = isolate->factory()->empty_fixed_array();
  } else {
    result = isolate->factory()->NewFixedArray(
        LengthOfFixedArray(number_of_deopt_points), pretenure);
  }
  return Handle<DeoptimizationOutputData>::cast(result);
}


#ifdef DEBUG
bool DescriptorArray::IsEqualTo(DescriptorArray* other) {
  if (IsEmpty()) return other->IsEmpty();
  if (other->IsEmpty()) return false;
  if (length() != other->length()) return false;
  for (int i = 0; i < length(); ++i) {
    if (get(i) != other->get(i)) return false;
  }
  return true;
}
#endif


bool String::LooksValid() {
  if (!GetIsolate()->heap()->Contains(this)) return false;
  return true;
}


String::FlatContent String::GetFlatContent() {
  DCHECK(!AllowHeapAllocation::IsAllowed());
  int length = this->length();
  StringShape shape(this);
  String* string = this;
  int offset = 0;
  if (shape.representation_tag() == kConsStringTag) {
    ConsString* cons = ConsString::cast(string);
    if (cons->second()->length() != 0) {
      return FlatContent();
    }
    string = cons->first();
    shape = StringShape(string);
  }
  if (shape.representation_tag() == kSlicedStringTag) {
    SlicedString* slice = SlicedString::cast(string);
    offset = slice->offset();
    string = slice->parent();
    shape = StringShape(string);
    DCHECK(shape.representation_tag() != kConsStringTag &&
           shape.representation_tag() != kSlicedStringTag);
  }
  if (shape.encoding_tag() == kOneByteStringTag) {
    const uint8_t* start;
    if (shape.representation_tag() == kSeqStringTag) {
      start = SeqOneByteString::cast(string)->GetChars();
    } else {
      start = ExternalAsciiString::cast(string)->GetChars();
    }
    return FlatContent(start + offset, length);
  } else {
    DCHECK(shape.encoding_tag() == kTwoByteStringTag);
    const uc16* start;
    if (shape.representation_tag() == kSeqStringTag) {
      start = SeqTwoByteString::cast(string)->GetChars();
    } else {
      start = ExternalTwoByteString::cast(string)->GetChars();
    }
    return FlatContent(start + offset, length);
  }
}


SmartArrayPointer<char> String::ToCString(AllowNullsFlag allow_nulls,
                                          RobustnessFlag robust_flag,
                                          int offset,
                                          int length,
                                          int* length_return) {
  if (robust_flag == ROBUST_STRING_TRAVERSAL && !LooksValid()) {
    return SmartArrayPointer<char>(NULL);
  }
  Heap* heap = GetHeap();

  // Negative length means the to the end of the string.
  if (length < 0) length = kMaxInt - offset;

  // Compute the size of the UTF-8 string. Start at the specified offset.
  Access<ConsStringIteratorOp> op(
      heap->isolate()->objects_string_iterator());
  StringCharacterStream stream(this, op.value(), offset);
  int character_position = offset;
  int utf8_bytes = 0;
  int last = unibrow::Utf16::kNoPreviousCharacter;
  while (stream.HasMore() && character_position++ < offset + length) {
    uint16_t character = stream.GetNext();
    utf8_bytes += unibrow::Utf8::Length(character, last);
    last = character;
  }

  if (length_return) {
    *length_return = utf8_bytes;
  }

  char* result = NewArray<char>(utf8_bytes + 1);

  // Convert the UTF-16 string to a UTF-8 buffer. Start at the specified offset.
  stream.Reset(this, offset);
  character_position = offset;
  int utf8_byte_position = 0;
  last = unibrow::Utf16::kNoPreviousCharacter;
  while (stream.HasMore() && character_position++ < offset + length) {
    uint16_t character = stream.GetNext();
    if (allow_nulls == DISALLOW_NULLS && character == 0) {
      character = ' ';
    }
    utf8_byte_position +=
        unibrow::Utf8::Encode(result + utf8_byte_position, character, last);
    last = character;
  }
  result[utf8_byte_position] = 0;
  return SmartArrayPointer<char>(result);
}


SmartArrayPointer<char> String::ToCString(AllowNullsFlag allow_nulls,
                                          RobustnessFlag robust_flag,
                                          int* length_return) {
  return ToCString(allow_nulls, robust_flag, 0, -1, length_return);
}


const uc16* String::GetTwoByteData(unsigned start) {
  DCHECK(!IsOneByteRepresentationUnderneath());
  switch (StringShape(this).representation_tag()) {
    case kSeqStringTag:
      return SeqTwoByteString::cast(this)->SeqTwoByteStringGetData(start);
    case kExternalStringTag:
      return ExternalTwoByteString::cast(this)->
        ExternalTwoByteStringGetData(start);
    case kSlicedStringTag: {
      SlicedString* slice = SlicedString::cast(this);
      return slice->parent()->GetTwoByteData(start + slice->offset());
    }
    case kConsStringTag:
      UNREACHABLE();
      return NULL;
  }
  UNREACHABLE();
  return NULL;
}


SmartArrayPointer<uc16> String::ToWideCString(RobustnessFlag robust_flag) {
  if (robust_flag == ROBUST_STRING_TRAVERSAL && !LooksValid()) {
    return SmartArrayPointer<uc16>();
  }
  Heap* heap = GetHeap();

  Access<ConsStringIteratorOp> op(
      heap->isolate()->objects_string_iterator());
  StringCharacterStream stream(this, op.value());

  uc16* result = NewArray<uc16>(length() + 1);

  int i = 0;
  while (stream.HasMore()) {
    uint16_t character = stream.GetNext();
    result[i++] = character;
  }
  result[i] = 0;
  return SmartArrayPointer<uc16>(result);
}


const uc16* SeqTwoByteString::SeqTwoByteStringGetData(unsigned start) {
  return reinterpret_cast<uc16*>(
      reinterpret_cast<char*>(this) - kHeapObjectTag + kHeaderSize) + start;
}


void Relocatable::PostGarbageCollectionProcessing(Isolate* isolate) {
  Relocatable* current = isolate->relocatable_top();
  while (current != NULL) {
    current->PostGarbageCollection();
    current = current->prev_;
  }
}


// Reserve space for statics needing saving and restoring.
int Relocatable::ArchiveSpacePerThread() {
  return sizeof(Relocatable*);  // NOLINT
}


// Archive statics that are thread-local.
char* Relocatable::ArchiveState(Isolate* isolate, char* to) {
  *reinterpret_cast<Relocatable**>(to) = isolate->relocatable_top();
  isolate->set_relocatable_top(NULL);
  return to + ArchiveSpacePerThread();
}


// Restore statics that are thread-local.
char* Relocatable::RestoreState(Isolate* isolate, char* from) {
  isolate->set_relocatable_top(*reinterpret_cast<Relocatable**>(from));
  return from + ArchiveSpacePerThread();
}


char* Relocatable::Iterate(ObjectVisitor* v, char* thread_storage) {
  Relocatable* top = *reinterpret_cast<Relocatable**>(thread_storage);
  Iterate(v, top);
  return thread_storage + ArchiveSpacePerThread();
}


void Relocatable::Iterate(Isolate* isolate, ObjectVisitor* v) {
  Iterate(v, isolate->relocatable_top());
}


void Relocatable::Iterate(ObjectVisitor* v, Relocatable* top) {
  Relocatable* current = top;
  while (current != NULL) {
    current->IterateInstance(v);
    current = current->prev_;
  }
}


FlatStringReader::FlatStringReader(Isolate* isolate, Handle<String> str)
    : Relocatable(isolate),
      str_(str.location()),
      length_(str->length()) {
  PostGarbageCollection();
}


FlatStringReader::FlatStringReader(Isolate* isolate, Vector<const char> input)
    : Relocatable(isolate),
      str_(0),
      is_ascii_(true),
      length_(input.length()),
      start_(input.start()) { }


void FlatStringReader::PostGarbageCollection() {
  if (str_ == NULL) return;
  Handle<String> str(str_);
  DCHECK(str->IsFlat());
  DisallowHeapAllocation no_gc;
  // This does not actually prevent the vector from being relocated later.
  String::FlatContent content = str->GetFlatContent();
  DCHECK(content.IsFlat());
  is_ascii_ = content.IsAscii();
  if (is_ascii_) {
    start_ = content.ToOneByteVector().start();
  } else {
    start_ = content.ToUC16Vector().start();
  }
}


void ConsStringIteratorOp::Initialize(ConsString* cons_string, int offset) {
  DCHECK(cons_string != NULL);
  root_ = cons_string;
  consumed_ = offset;
  // Force stack blown condition to trigger restart.
  depth_ = 1;
  maximum_depth_ = kStackSize + depth_;
  DCHECK(StackBlown());
}


String* ConsStringIteratorOp::Continue(int* offset_out) {
  DCHECK(depth_ != 0);
  DCHECK_EQ(0, *offset_out);
  bool blew_stack = StackBlown();
  String* string = NULL;
  // Get the next leaf if there is one.
  if (!blew_stack) string = NextLeaf(&blew_stack);
  // Restart search from root.
  if (blew_stack) {
    DCHECK(string == NULL);
    string = Search(offset_out);
  }
  // Ensure future calls return null immediately.
  if (string == NULL) Reset(NULL);
  return string;
}


String* ConsStringIteratorOp::Search(int* offset_out) {
  ConsString* cons_string = root_;
  // Reset the stack, pushing the root string.
  depth_ = 1;
  maximum_depth_ = 1;
  frames_[0] = cons_string;
  const int consumed = consumed_;
  int offset = 0;
  while (true) {
    // Loop until the string is found which contains the target offset.
    String* string = cons_string->first();
    int length = string->length();
    int32_t type;
    if (consumed < offset + length) {
      // Target offset is in the left branch.
      // Keep going if we're still in a ConString.
      type = string->map()->instance_type();
      if ((type & kStringRepresentationMask) == kConsStringTag) {
        cons_string = ConsString::cast(string);
        PushLeft(cons_string);
        continue;
      }
      // Tell the stack we're done descending.
      AdjustMaximumDepth();
    } else {
      // Descend right.
      // Update progress through the string.
      offset += length;
      // Keep going if we're still in a ConString.
      string = cons_string->second();
      type = string->map()->instance_type();
      if ((type & kStringRepresentationMask) == kConsStringTag) {
        cons_string = ConsString::cast(string);
        PushRight(cons_string);
        continue;
      }
      // Need this to be updated for the current string.
      length = string->length();
      // Account for the possibility of an empty right leaf.
      // This happens only if we have asked for an offset outside the string.
      if (length == 0) {
        // Reset so future operations will return null immediately.
        Reset(NULL);
        return NULL;
      }
      // Tell the stack we're done descending.
      AdjustMaximumDepth();
      // Pop stack so next iteration is in correct place.
      Pop();
    }
    DCHECK(length != 0);
    // Adjust return values and exit.
    consumed_ = offset + length;
    *offset_out = consumed - offset;
    return string;
  }
  UNREACHABLE();
  return NULL;
}


String* ConsStringIteratorOp::NextLeaf(bool* blew_stack) {
  while (true) {
    // Tree traversal complete.
    if (depth_ == 0) {
      *blew_stack = false;
      return NULL;
    }
    // We've lost track of higher nodes.
    if (StackBlown()) {
      *blew_stack = true;
      return NULL;
    }
    // Go right.
    ConsString* cons_string = frames_[OffsetForDepth(depth_ - 1)];
    String* string = cons_string->second();
    int32_t type = string->map()->instance_type();
    if ((type & kStringRepresentationMask) != kConsStringTag) {
      // Pop stack so next iteration is in correct place.
      Pop();
      int length = string->length();
      // Could be a flattened ConsString.
      if (length == 0) continue;
      consumed_ += length;
      return string;
    }
    cons_string = ConsString::cast(string);
    PushRight(cons_string);
    // Need to traverse all the way left.
    while (true) {
      // Continue left.
      string = cons_string->first();
      type = string->map()->instance_type();
      if ((type & kStringRepresentationMask) != kConsStringTag) {
        AdjustMaximumDepth();
        int length = string->length();
        DCHECK(length != 0);
        consumed_ += length;
        return string;
      }
      cons_string = ConsString::cast(string);
      PushLeft(cons_string);
    }
  }
  UNREACHABLE();
  return NULL;
}


uint16_t ConsString::ConsStringGet(int index) {
  DCHECK(index >= 0 && index < this->length());

  // Check for a flattened cons string
  if (second()->length() == 0) {
    String* left = first();
    return left->Get(index);
  }

  String* string = String::cast(this);

  while (true) {
    if (StringShape(string).IsCons()) {
      ConsString* cons_string = ConsString::cast(string);
      String* left = cons_string->first();
      if (left->length() > index) {
        string = left;
      } else {
        index -= left->length();
        string = cons_string->second();
      }
    } else {
      return string->Get(index);
    }
  }

  UNREACHABLE();
  return 0;
}


uint16_t SlicedString::SlicedStringGet(int index) {
  return parent()->Get(offset() + index);
}


template <typename sinkchar>
void String::WriteToFlat(String* src,
                         sinkchar* sink,
                         int f,
                         int t) {
  String* source = src;
  int from = f;
  int to = t;
  while (true) {
    DCHECK(0 <= from && from <= to && to <= source->length());
    switch (StringShape(source).full_representation_tag()) {
      case kOneByteStringTag | kExternalStringTag: {
        CopyChars(sink,
                  ExternalAsciiString::cast(source)->GetChars() + from,
                  to - from);
        return;
      }
      case kTwoByteStringTag | kExternalStringTag: {
        const uc16* data =
            ExternalTwoByteString::cast(source)->GetChars();
        CopyChars(sink,
                  data + from,
                  to - from);
        return;
      }
      case kOneByteStringTag | kSeqStringTag: {
        CopyChars(sink,
                  SeqOneByteString::cast(source)->GetChars() + from,
                  to - from);
        return;
      }
      case kTwoByteStringTag | kSeqStringTag: {
        CopyChars(sink,
                  SeqTwoByteString::cast(source)->GetChars() + from,
                  to - from);
        return;
      }
      case kOneByteStringTag | kConsStringTag:
      case kTwoByteStringTag | kConsStringTag: {
        ConsString* cons_string = ConsString::cast(source);
        String* first = cons_string->first();
        int boundary = first->length();
        if (to - boundary >= boundary - from) {
          // Right hand side is longer.  Recurse over left.
          if (from < boundary) {
            WriteToFlat(first, sink, from, boundary);
            sink += boundary - from;
            from = 0;
          } else {
            from -= boundary;
          }
          to -= boundary;
          source = cons_string->second();
        } else {
          // Left hand side is longer.  Recurse over right.
          if (to > boundary) {
            String* second = cons_string->second();
            // When repeatedly appending to a string, we get a cons string that
            // is unbalanced to the left, a list, essentially.  We inline the
            // common case of sequential ascii right child.
            if (to - boundary == 1) {
              sink[boundary - from] = static_cast<sinkchar>(second->Get(0));
            } else if (second->IsSeqOneByteString()) {
              CopyChars(sink + boundary - from,
                        SeqOneByteString::cast(second)->GetChars(),
                        to - boundary);
            } else {
              WriteToFlat(second,
                          sink + boundary - from,
                          0,
                          to - boundary);
            }
            to = boundary;
          }
          source = first;
        }
        break;
      }
      case kOneByteStringTag | kSlicedStringTag:
      case kTwoByteStringTag | kSlicedStringTag: {
        SlicedString* slice = SlicedString::cast(source);
        unsigned offset = slice->offset();
        WriteToFlat(slice->parent(), sink, from + offset, to + offset);
        return;
      }
    }
  }
}



template <typename SourceChar>
static void CalculateLineEndsImpl(Isolate* isolate,
                                  List<int>* line_ends,
                                  Vector<const SourceChar> src,
                                  bool include_ending_line) {
  const int src_len = src.length();
  StringSearch<uint8_t, SourceChar> search(isolate, STATIC_ASCII_VECTOR("\n"));

  // Find and record line ends.
  int position = 0;
  while (position != -1 && position < src_len) {
    position = search.Search(src, position);
    if (position != -1) {
      line_ends->Add(position);
      position++;
    } else if (include_ending_line) {
      // Even if the last line misses a line end, it is counted.
      line_ends->Add(src_len);
      return;
    }
  }
}


Handle<FixedArray> String::CalculateLineEnds(Handle<String> src,
                                             bool include_ending_line) {
  src = Flatten(src);
  // Rough estimate of line count based on a roughly estimated average
  // length of (unpacked) code.
  int line_count_estimate = src->length() >> 4;
  List<int> line_ends(line_count_estimate);
  Isolate* isolate = src->GetIsolate();
  { DisallowHeapAllocation no_allocation;  // ensure vectors stay valid.
    // Dispatch on type of strings.
    String::FlatContent content = src->GetFlatContent();
    DCHECK(content.IsFlat());
    if (content.IsAscii()) {
      CalculateLineEndsImpl(isolate,
                            &line_ends,
                            content.ToOneByteVector(),
                            include_ending_line);
    } else {
      CalculateLineEndsImpl(isolate,
                            &line_ends,
                            content.ToUC16Vector(),
                            include_ending_line);
    }
  }
  int line_count = line_ends.length();
  Handle<FixedArray> array = isolate->factory()->NewFixedArray(line_count);
  for (int i = 0; i < line_count; i++) {
    array->set(i, Smi::FromInt(line_ends[i]));
  }
  return array;
}


// Compares the contents of two strings by reading and comparing
// int-sized blocks of characters.
template <typename Char>
static inline bool CompareRawStringContents(const Char* const a,
                                            const Char* const b,
                                            int length) {
  int i = 0;
#ifndef V8_HOST_CAN_READ_UNALIGNED
  // If this architecture isn't comfortable reading unaligned ints
  // then we have to check that the strings are aligned before
  // comparing them blockwise.
  const int kAlignmentMask = sizeof(uint32_t) - 1;  // NOLINT
  uintptr_t pa_addr = reinterpret_cast<uintptr_t>(a);
  uintptr_t pb_addr = reinterpret_cast<uintptr_t>(b);
  if (((pa_addr & kAlignmentMask) | (pb_addr & kAlignmentMask)) == 0) {
#endif
    const int kStepSize = sizeof(int) / sizeof(Char);  // NOLINT
    int endpoint = length - kStepSize;
    // Compare blocks until we reach near the end of the string.
    for (; i <= endpoint; i += kStepSize) {
      uint32_t wa = *reinterpret_cast<const uint32_t*>(a + i);
      uint32_t wb = *reinterpret_cast<const uint32_t*>(b + i);
      if (wa != wb) {
        return false;
      }
    }
#ifndef V8_HOST_CAN_READ_UNALIGNED
  }
#endif
  // Compare the remaining characters that didn't fit into a block.
  for (; i < length; i++) {
    if (a[i] != b[i]) {
      return false;
    }
  }
  return true;
}


template<typename Chars1, typename Chars2>
class RawStringComparator : public AllStatic {
 public:
  static inline bool compare(const Chars1* a, const Chars2* b, int len) {
    DCHECK(sizeof(Chars1) != sizeof(Chars2));
    for (int i = 0; i < len; i++) {
      if (a[i] != b[i]) {
        return false;
      }
    }
    return true;
  }
};


template<>
class RawStringComparator<uint16_t, uint16_t> {
 public:
  static inline bool compare(const uint16_t* a, const uint16_t* b, int len) {
    return CompareRawStringContents(a, b, len);
  }
};


template<>
class RawStringComparator<uint8_t, uint8_t> {
 public:
  static inline bool compare(const uint8_t* a, const uint8_t* b, int len) {
    return CompareRawStringContents(a, b, len);
  }
};


class StringComparator {
  class State {
   public:
    explicit inline State(ConsStringIteratorOp* op)
      : op_(op), is_one_byte_(true), length_(0), buffer8_(NULL) {}

    inline void Init(String* string) {
      ConsString* cons_string = String::VisitFlat(this, string);
      op_->Reset(cons_string);
      if (cons_string != NULL) {
        int offset;
        string = op_->Next(&offset);
        String::VisitFlat(this, string, offset);
      }
    }

    inline void VisitOneByteString(const uint8_t* chars, int length) {
      is_one_byte_ = true;
      buffer8_ = chars;
      length_ = length;
    }

    inline void VisitTwoByteString(const uint16_t* chars, int length) {
      is_one_byte_ = false;
      buffer16_ = chars;
      length_ = length;
    }

    void Advance(int consumed) {
      DCHECK(consumed <= length_);
      // Still in buffer.
      if (length_ != consumed) {
        if (is_one_byte_) {
          buffer8_ += consumed;
        } else {
          buffer16_ += consumed;
        }
        length_ -= consumed;
        return;
      }
      // Advance state.
      int offset;
      String* next = op_->Next(&offset);
      DCHECK_EQ(0, offset);
      DCHECK(next != NULL);
      String::VisitFlat(this, next);
    }

    ConsStringIteratorOp* const op_;
    bool is_one_byte_;
    int length_;
    union {
      const uint8_t* buffer8_;
      const uint16_t* buffer16_;
    };

   private:
    DISALLOW_IMPLICIT_CONSTRUCTORS(State);
  };

 public:
  inline StringComparator(ConsStringIteratorOp* op_1,
                          ConsStringIteratorOp* op_2)
    : state_1_(op_1),
      state_2_(op_2) {
  }

  template<typename Chars1, typename Chars2>
  static inline bool Equals(State* state_1, State* state_2, int to_check) {
    const Chars1* a = reinterpret_cast<const Chars1*>(state_1->buffer8_);
    const Chars2* b = reinterpret_cast<const Chars2*>(state_2->buffer8_);
    return RawStringComparator<Chars1, Chars2>::compare(a, b, to_check);
  }

  bool Equals(String* string_1, String* string_2) {
    int length = string_1->length();
    state_1_.Init(string_1);
    state_2_.Init(string_2);
    while (true) {
      int to_check = Min(state_1_.length_, state_2_.length_);
      DCHECK(to_check > 0 && to_check <= length);
      bool is_equal;
      if (state_1_.is_one_byte_) {
        if (state_2_.is_one_byte_) {
          is_equal = Equals<uint8_t, uint8_t>(&state_1_, &state_2_, to_check);
        } else {
          is_equal = Equals<uint8_t, uint16_t>(&state_1_, &state_2_, to_check);
        }
      } else {
        if (state_2_.is_one_byte_) {
          is_equal = Equals<uint16_t, uint8_t>(&state_1_, &state_2_, to_check);
        } else {
          is_equal = Equals<uint16_t, uint16_t>(&state_1_, &state_2_, to_check);
        }
      }
      // Looping done.
      if (!is_equal) return false;
      length -= to_check;
      // Exit condition. Strings are equal.
      if (length == 0) return true;
      state_1_.Advance(to_check);
      state_2_.Advance(to_check);
    }
  }

 private:
  State state_1_;
  State state_2_;
  DISALLOW_IMPLICIT_CONSTRUCTORS(StringComparator);
};


bool String::SlowEquals(String* other) {
  DisallowHeapAllocation no_gc;
  // Fast check: negative check with lengths.
  int len = length();
  if (len != other->length()) return false;
  if (len == 0) return true;

  // Fast check: if hash code is computed for both strings
  // a fast negative check can be performed.
  if (HasHashCode() && other->HasHashCode()) {
#ifdef ENABLE_SLOW_DCHECKS
    if (FLAG_enable_slow_asserts) {
      if (Hash() != other->Hash()) {
        bool found_difference = false;
        for (int i = 0; i < len; i++) {
          if (Get(i) != other->Get(i)) {
            found_difference = true;
            break;
          }
        }
        DCHECK(found_difference);
      }
    }
#endif
    if (Hash() != other->Hash()) return false;
  }

  // We know the strings are both non-empty. Compare the first chars
  // before we try to flatten the strings.
  if (this->Get(0) != other->Get(0)) return false;

  if (IsSeqOneByteString() && other->IsSeqOneByteString()) {
    const uint8_t* str1 = SeqOneByteString::cast(this)->GetChars();
    const uint8_t* str2 = SeqOneByteString::cast(other)->GetChars();
    return CompareRawStringContents(str1, str2, len);
  }

  Isolate* isolate = GetIsolate();
  StringComparator comparator(isolate->objects_string_compare_iterator_a(),
                              isolate->objects_string_compare_iterator_b());

  return comparator.Equals(this, other);
}


bool String::SlowEquals(Handle<String> one, Handle<String> two) {
  // Fast check: negative check with lengths.
  int one_length = one->length();
  if (one_length != two->length()) return false;
  if (one_length == 0) return true;

  // Fast check: if hash code is computed for both strings
  // a fast negative check can be performed.
  if (one->HasHashCode() && two->HasHashCode()) {
#ifdef ENABLE_SLOW_DCHECKS
    if (FLAG_enable_slow_asserts) {
      if (one->Hash() != two->Hash()) {
        bool found_difference = false;
        for (int i = 0; i < one_length; i++) {
          if (one->Get(i) != two->Get(i)) {
            found_difference = true;
            break;
          }
        }
        DCHECK(found_difference);
      }
    }
#endif
    if (one->Hash() != two->Hash()) return false;
  }

  // We know the strings are both non-empty. Compare the first chars
  // before we try to flatten the strings.
  if (one->Get(0) != two->Get(0)) return false;

  one = String::Flatten(one);
  two = String::Flatten(two);

  DisallowHeapAllocation no_gc;
  String::FlatContent flat1 = one->GetFlatContent();
  String::FlatContent flat2 = two->GetFlatContent();

  if (flat1.IsAscii() && flat2.IsAscii()) {
      return CompareRawStringContents(flat1.ToOneByteVector().start(),
                                      flat2.ToOneByteVector().start(),
                                      one_length);
  } else {
    for (int i = 0; i < one_length; i++) {
      if (flat1.Get(i) != flat2.Get(i)) return false;
    }
    return true;
  }
}


bool String::MarkAsUndetectable() {
  if (StringShape(this).IsInternalized()) return false;

  Map* map = this->map();
  Heap* heap = GetHeap();
  if (map == heap->string_map()) {
    this->set_map(heap->undetectable_string_map());
    return true;
  } else if (map == heap->ascii_string_map()) {
    this->set_map(heap->undetectable_ascii_string_map());
    return true;
  }
  // Rest cannot be marked as undetectable
  return false;
}


bool String::IsUtf8EqualTo(Vector<const char> str, bool allow_prefix_match) {
  int slen = length();
  // Can't check exact length equality, but we can check bounds.
  int str_len = str.length();
  if (!allow_prefix_match &&
      (str_len < slen ||
          str_len > slen*static_cast<int>(unibrow::Utf8::kMaxEncodedSize))) {
    return false;
  }
  int i;
  unsigned remaining_in_str = static_cast<unsigned>(str_len);
  const uint8_t* utf8_data = reinterpret_cast<const uint8_t*>(str.start());
  for (i = 0; i < slen && remaining_in_str > 0; i++) {
    unsigned cursor = 0;
    uint32_t r = unibrow::Utf8::ValueOf(utf8_data, remaining_in_str, &cursor);
    DCHECK(cursor > 0 && cursor <= remaining_in_str);
    if (r > unibrow::Utf16::kMaxNonSurrogateCharCode) {
      if (i > slen - 1) return false;
      if (Get(i++) != unibrow::Utf16::LeadSurrogate(r)) return false;
      if (Get(i) != unibrow::Utf16::TrailSurrogate(r)) return false;
    } else {
      if (Get(i) != r) return false;
    }
    utf8_data += cursor;
    remaining_in_str -= cursor;
  }
  return (allow_prefix_match || i == slen) && remaining_in_str == 0;
}


bool String::IsOneByteEqualTo(Vector<const uint8_t> str) {
  int slen = length();
  if (str.length() != slen) return false;
  DisallowHeapAllocation no_gc;
  FlatContent content = GetFlatContent();
  if (content.IsAscii()) {
    return CompareChars(content.ToOneByteVector().start(),
                        str.start(), slen) == 0;
  }
  for (int i = 0; i < slen; i++) {
    if (Get(i) != static_cast<uint16_t>(str[i])) return false;
  }
  return true;
}


bool String::IsTwoByteEqualTo(Vector<const uc16> str) {
  int slen = length();
  if (str.length() != slen) return false;
  DisallowHeapAllocation no_gc;
  FlatContent content = GetFlatContent();
  if (content.IsTwoByte()) {
    return CompareChars(content.ToUC16Vector().start(), str.start(), slen) == 0;
  }
  for (int i = 0; i < slen; i++) {
    if (Get(i) != str[i]) return false;
  }
  return true;
}


uint32_t String::ComputeAndSetHash() {
  // Should only be called if hash code has not yet been computed.
  DCHECK(!HasHashCode());

  // Store the hash code in the object.
  uint32_t field = IteratingStringHasher::Hash(this, GetHeap()->HashSeed());
  set_hash_field(field);

  // Check the hash code is there.
  DCHECK(HasHashCode());
  uint32_t result = field >> kHashShift;
  DCHECK(result != 0);  // Ensure that the hash value of 0 is never computed.
  return result;
}


bool String::ComputeArrayIndex(uint32_t* index) {
  int length = this->length();
  if (length == 0 || length > kMaxArrayIndexSize) return false;
  ConsStringIteratorOp op;
  StringCharacterStream stream(this, &op);
  return StringToArrayIndex(&stream, index);
}


bool String::SlowAsArrayIndex(uint32_t* index) {
  if (length() <= kMaxCachedArrayIndexLength) {
    Hash();  // force computation of hash code
    uint32_t field = hash_field();
    if ((field & kIsNotArrayIndexMask) != 0) return false;
    // Isolate the array index form the full hash field.
    *index = ArrayIndexValueBits::decode(field);
    return true;
  } else {
    return ComputeArrayIndex(index);
  }
}


Handle<String> SeqString::Truncate(Handle<SeqString> string, int new_length) {
  int new_size, old_size;
  int old_length = string->length();
  if (old_length <= new_length) return string;

  if (string->IsSeqOneByteString()) {
    old_size = SeqOneByteString::SizeFor(old_length);
    new_size = SeqOneByteString::SizeFor(new_length);
  } else {
    DCHECK(string->IsSeqTwoByteString());
    old_size = SeqTwoByteString::SizeFor(old_length);
    new_size = SeqTwoByteString::SizeFor(new_length);
  }

  int delta = old_size - new_size;

  Address start_of_string = string->address();
  DCHECK_OBJECT_ALIGNED(start_of_string);
  DCHECK_OBJECT_ALIGNED(start_of_string + new_size);

  Heap* heap = string->GetHeap();
  NewSpace* newspace = heap->new_space();
  if (newspace->Contains(start_of_string) &&
      newspace->top() == start_of_string + old_size) {
    // Last allocated object in new space.  Simply lower allocation top.
    newspace->set_top(start_of_string + new_size);
  } else {
    // Sizes are pointer size aligned, so that we can use filler objects
    // that are a multiple of pointer size.
    heap->CreateFillerObjectAt(start_of_string + new_size, delta);
  }
  heap->AdjustLiveBytes(start_of_string, -delta, Heap::FROM_MUTATOR);

  // We are storing the new length using release store after creating a filler
  // for the left-over space to avoid races with the sweeper thread.
  string->synchronized_set_length(new_length);

  if (new_length == 0) return heap->isolate()->factory()->empty_string();
  return string;
}


uint32_t StringHasher::MakeArrayIndexHash(uint32_t value, int length) {
  // For array indexes mix the length into the hash as an array index could
  // be zero.
  DCHECK(length > 0);
  DCHECK(length <= String::kMaxArrayIndexSize);
  DCHECK(TenToThe(String::kMaxCachedArrayIndexLength) <
         (1 << String::kArrayIndexValueBits));

  value <<= String::ArrayIndexValueBits::kShift;
  value |= length << String::ArrayIndexLengthBits::kShift;

  DCHECK((value & String::kIsNotArrayIndexMask) == 0);
  DCHECK((length > String::kMaxCachedArrayIndexLength) ||
         (value & String::kContainsCachedArrayIndexMask) == 0);
  return value;
}


uint32_t StringHasher::GetHashField() {
  if (length_ <= String::kMaxHashCalcLength) {
    if (is_array_index_) {
      return MakeArrayIndexHash(array_index_, length_);
    }
    return (GetHashCore(raw_running_hash_) << String::kHashShift) |
           String::kIsNotArrayIndexMask;
  } else {
    return (length_ << String::kHashShift) | String::kIsNotArrayIndexMask;
  }
}


uint32_t StringHasher::ComputeUtf8Hash(Vector<const char> chars,
                                       uint32_t seed,
                                       int* utf16_length_out) {
  int vector_length = chars.length();
  // Handle some edge cases
  if (vector_length <= 1) {
    DCHECK(vector_length == 0 ||
           static_cast<uint8_t>(chars.start()[0]) <=
               unibrow::Utf8::kMaxOneByteChar);
    *utf16_length_out = vector_length;
    return HashSequentialString(chars.start(), vector_length, seed);
  }
  // Start with a fake length which won't affect computation.
  // It will be updated later.
  StringHasher hasher(String::kMaxArrayIndexSize, seed);
  unsigned remaining = static_cast<unsigned>(vector_length);
  const uint8_t* stream = reinterpret_cast<const uint8_t*>(chars.start());
  int utf16_length = 0;
  bool is_index = true;
  DCHECK(hasher.is_array_index_);
  while (remaining > 0) {
    unsigned consumed = 0;
    uint32_t c = unibrow::Utf8::ValueOf(stream, remaining, &consumed);
    DCHECK(consumed > 0 && consumed <= remaining);
    stream += consumed;
    remaining -= consumed;
    bool is_two_characters = c > unibrow::Utf16::kMaxNonSurrogateCharCode;
    utf16_length += is_two_characters ? 2 : 1;
    // No need to keep hashing. But we do need to calculate utf16_length.
    if (utf16_length > String::kMaxHashCalcLength) continue;
    if (is_two_characters) {
      uint16_t c1 = unibrow::Utf16::LeadSurrogate(c);
      uint16_t c2 = unibrow::Utf16::TrailSurrogate(c);
      hasher.AddCharacter(c1);
      hasher.AddCharacter(c2);
      if (is_index) is_index = hasher.UpdateIndex(c1);
      if (is_index) is_index = hasher.UpdateIndex(c2);
    } else {
      hasher.AddCharacter(c);
      if (is_index) is_index = hasher.UpdateIndex(c);
    }
  }
  *utf16_length_out = static_cast<int>(utf16_length);
  // Must set length here so that hash computation is correct.
  hasher.length_ = utf16_length;
  return hasher.GetHashField();
}


void String::PrintOn(FILE* file) {
  int length = this->length();
  for (int i = 0; i < length; i++) {
    PrintF(file, "%c", Get(i));
  }
}


int Map::Hash() {
  // For performance reasons we only hash the 3 most variable fields of a map:
  // constructor, prototype and bit_field2.

  // Shift away the tag.
  int hash = (static_cast<uint32_t>(
        reinterpret_cast<uintptr_t>(constructor())) >> 2);

  // XOR-ing the prototype and constructor directly yields too many zero bits
  // when the two pointers are close (which is fairly common).
  // To avoid this we shift the prototype 4 bits relatively to the constructor.
  hash ^= (static_cast<uint32_t>(
        reinterpret_cast<uintptr_t>(prototype())) << 2);

  return hash ^ (hash >> 16) ^ bit_field2();
}


static bool CheckEquivalent(Map* first, Map* second) {
  return
    first->constructor() == second->constructor() &&
    first->prototype() == second->prototype() &&
    first->instance_type() == second->instance_type() &&
    first->bit_field() == second->bit_field() &&
    first->bit_field2() == second->bit_field2() &&
    first->is_frozen() == second->is_frozen() &&
    first->has_instance_call_handler() == second->has_instance_call_handler();
}


bool Map::EquivalentToForTransition(Map* other) {
  return CheckEquivalent(this, other);
}


bool Map::EquivalentToForNormalization(Map* other,
                                       PropertyNormalizationMode mode) {
  int properties = mode == CLEAR_INOBJECT_PROPERTIES
      ? 0 : other->inobject_properties();
  return CheckEquivalent(this, other) && inobject_properties() == properties;
}


void ConstantPoolArray::ConstantPoolIterateBody(ObjectVisitor* v) {
  // Unfortunately the serializer relies on pointers within an object being
  // visited in-order, so we have to iterate both the code and heap pointers in
  // the small section before doing so in the extended section.
  for (int s = 0; s <= final_section(); ++s) {
    LayoutSection section = static_cast<LayoutSection>(s);
    ConstantPoolArray::Iterator code_iter(this, ConstantPoolArray::CODE_PTR,
                                          section);
    while (!code_iter.is_finished()) {
      v->VisitCodeEntry(reinterpret_cast<Address>(
          RawFieldOfElementAt(code_iter.next_index())));
    }

    ConstantPoolArray::Iterator heap_iter(this, ConstantPoolArray::HEAP_PTR,
                                          section);
    while (!heap_iter.is_finished()) {
      v->VisitPointer(RawFieldOfElementAt(heap_iter.next_index()));
    }
  }
}


void ConstantPoolArray::ClearPtrEntries(Isolate* isolate) {
  Type type[] = { CODE_PTR, HEAP_PTR };
  Address default_value[] = {
        isolate->builtins()->builtin(Builtins::kIllegal)->entry(),
        reinterpret_cast<Address>(isolate->heap()->undefined_value()) };

  for (int i = 0; i < 2; ++i) {
    for (int s = 0; s <= final_section(); ++s) {
      LayoutSection section = static_cast<LayoutSection>(s);
      if (number_of_entries(type[i], section) > 0) {
        int offset = OffsetOfElementAt(first_index(type[i], section));
        MemsetPointer(
          reinterpret_cast<Address*>(HeapObject::RawField(this, offset)),
          default_value[i],
          number_of_entries(type[i], section));
      }
    }
  }
}


void JSFunction::JSFunctionIterateBody(int object_size, ObjectVisitor* v) {
  // Iterate over all fields in the body but take care in dealing with
  // the code entry.
  IteratePointers(v, kPropertiesOffset, kCodeEntryOffset);
  v->VisitCodeEntry(this->address() + kCodeEntryOffset);
  IteratePointers(v, kCodeEntryOffset + kPointerSize, object_size);
}


void JSFunction::MarkForOptimization() {
  DCHECK(is_compiled() || GetIsolate()->DebuggerHasBreakPoints());
  DCHECK(!IsOptimized());
  DCHECK(shared()->allows_lazy_compilation() ||
         code()->optimizable());
  DCHECK(!shared()->is_generator());
  set_code_no_write_barrier(
      GetIsolate()->builtins()->builtin(Builtins::kCompileOptimized));
  // No write barrier required, since the builtin is part of the root set.
}


void JSFunction::MarkForConcurrentOptimization() {
  DCHECK(is_compiled() || GetIsolate()->DebuggerHasBreakPoints());
  DCHECK(!IsOptimized());
  DCHECK(shared()->allows_lazy_compilation() || code()->optimizable());
  DCHECK(!shared()->is_generator());
  DCHECK(GetIsolate()->concurrent_recompilation_enabled());
  if (FLAG_trace_concurrent_recompilation) {
    PrintF("  ** Marking ");
    PrintName();
    PrintF(" for concurrent recompilation.\n");
  }
  set_code_no_write_barrier(
      GetIsolate()->builtins()->builtin(Builtins::kCompileOptimizedConcurrent));
  // No write barrier required, since the builtin is part of the root set.
}


void JSFunction::MarkInOptimizationQueue() {
  // We can only arrive here via the concurrent-recompilation builtin.  If
  // break points were set, the code would point to the lazy-compile builtin.
  DCHECK(!GetIsolate()->DebuggerHasBreakPoints());
  DCHECK(IsMarkedForConcurrentOptimization() && !IsOptimized());
  DCHECK(shared()->allows_lazy_compilation() || code()->optimizable());
  DCHECK(GetIsolate()->concurrent_recompilation_enabled());
  if (FLAG_trace_concurrent_recompilation) {
    PrintF("  ** Queueing ");
    PrintName();
    PrintF(" for concurrent recompilation.\n");
  }
  set_code_no_write_barrier(
      GetIsolate()->builtins()->builtin(Builtins::kInOptimizationQueue));
  // No write barrier required, since the builtin is part of the root set.
}


void SharedFunctionInfo::AddToOptimizedCodeMap(
    Handle<SharedFunctionInfo> shared,
    Handle<Context> native_context,
    Handle<Code> code,
    Handle<FixedArray> literals,
    BailoutId osr_ast_id) {
  Isolate* isolate = shared->GetIsolate();
  DCHECK(code->kind() == Code::OPTIMIZED_FUNCTION);
  DCHECK(native_context->IsNativeContext());
  STATIC_ASSERT(kEntryLength == 4);
  Handle<FixedArray> new_code_map;
  Handle<Object> value(shared->optimized_code_map(), isolate);
  int old_length;
  if (value->IsSmi()) {
    // No optimized code map.
    DCHECK_EQ(0, Smi::cast(*value)->value());
    // Create 3 entries per context {context, code, literals}.
    new_code_map = isolate->factory()->NewFixedArray(kInitialLength);
    old_length = kEntriesStart;
  } else {
    // Copy old map and append one new entry.
    Handle<FixedArray> old_code_map = Handle<FixedArray>::cast(value);
    DCHECK_EQ(-1, shared->SearchOptimizedCodeMap(*native_context, osr_ast_id));
    old_length = old_code_map->length();
    new_code_map = FixedArray::CopySize(
        old_code_map, old_length + kEntryLength);
    // Zap the old map for the sake of the heap verifier.
    if (Heap::ShouldZapGarbage()) {
      Object** data = old_code_map->data_start();
      MemsetPointer(data, isolate->heap()->the_hole_value(), old_length);
    }
  }
  new_code_map->set(old_length + kContextOffset, *native_context);
  new_code_map->set(old_length + kCachedCodeOffset, *code);
  new_code_map->set(old_length + kLiteralsOffset, *literals);
  new_code_map->set(old_length + kOsrAstIdOffset,
                    Smi::FromInt(osr_ast_id.ToInt()));

#ifdef DEBUG
  for (int i = kEntriesStart; i < new_code_map->length(); i += kEntryLength) {
    DCHECK(new_code_map->get(i + kContextOffset)->IsNativeContext());
    DCHECK(new_code_map->get(i + kCachedCodeOffset)->IsCode());
    DCHECK(Code::cast(new_code_map->get(i + kCachedCodeOffset))->kind() ==
           Code::OPTIMIZED_FUNCTION);
    DCHECK(new_code_map->get(i + kLiteralsOffset)->IsFixedArray());
    DCHECK(new_code_map->get(i + kOsrAstIdOffset)->IsSmi());
  }
#endif
  shared->set_optimized_code_map(*new_code_map);
}


FixedArray* SharedFunctionInfo::GetLiteralsFromOptimizedCodeMap(int index) {
  DCHECK(index > kEntriesStart);
  FixedArray* code_map = FixedArray::cast(optimized_code_map());
  if (!bound()) {
    FixedArray* cached_literals = FixedArray::cast(code_map->get(index + 1));
    DCHECK_NE(NULL, cached_literals);
    return cached_literals;
  }
  return NULL;
}


Code* SharedFunctionInfo::GetCodeFromOptimizedCodeMap(int index) {
  DCHECK(index > kEntriesStart);
  FixedArray* code_map = FixedArray::cast(optimized_code_map());
  Code* code = Code::cast(code_map->get(index));
  DCHECK_NE(NULL, code);
  return code;
}


void SharedFunctionInfo::ClearOptimizedCodeMap() {
  FixedArray* code_map = FixedArray::cast(optimized_code_map());

  // If the next map link slot is already used then the function was
  // enqueued with code flushing and we remove it now.
  if (!code_map->get(kNextMapIndex)->IsUndefined()) {
    CodeFlusher* flusher = GetHeap()->mark_compact_collector()->code_flusher();
    flusher->EvictOptimizedCodeMap(this);
  }

  DCHECK(code_map->get(kNextMapIndex)->IsUndefined());
  set_optimized_code_map(Smi::FromInt(0));
}


void SharedFunctionInfo::EvictFromOptimizedCodeMap(Code* optimized_code,
                                                   const char* reason) {
  DisallowHeapAllocation no_gc;
  if (optimized_code_map()->IsSmi()) return;

  FixedArray* code_map = FixedArray::cast(optimized_code_map());
  int dst = kEntriesStart;
  int length = code_map->length();
  for (int src = kEntriesStart; src < length; src += kEntryLength) {
    DCHECK(code_map->get(src)->IsNativeContext());
    if (Code::cast(code_map->get(src + kCachedCodeOffset)) == optimized_code) {
      // Evict the src entry by not copying it to the dst entry.
      if (FLAG_trace_opt) {
        PrintF("[evicting entry from optimizing code map (%s) for ", reason);
        ShortPrint();
        BailoutId osr(Smi::cast(code_map->get(src + kOsrAstIdOffset))->value());
        if (osr.IsNone()) {
          PrintF("]\n");
        } else {
          PrintF(" (osr ast id %d)]\n", osr.ToInt());
        }
      }
    } else {
      // Keep the src entry by copying it to the dst entry.
      if (dst != src) {
        code_map->set(dst + kContextOffset,
                      code_map->get(src + kContextOffset));
        code_map->set(dst + kCachedCodeOffset,
                      code_map->get(src + kCachedCodeOffset));
        code_map->set(dst + kLiteralsOffset,
                      code_map->get(src + kLiteralsOffset));
        code_map->set(dst + kOsrAstIdOffset,
                      code_map->get(src + kOsrAstIdOffset));
      }
      dst += kEntryLength;
    }
  }
  if (dst != length) {
    // Always trim even when array is cleared because of heap verifier.
    GetHeap()->RightTrimFixedArray<Heap::FROM_MUTATOR>(code_map, length - dst);
    if (code_map->length() == kEntriesStart) ClearOptimizedCodeMap();
  }
}


void SharedFunctionInfo::TrimOptimizedCodeMap(int shrink_by) {
  FixedArray* code_map = FixedArray::cast(optimized_code_map());
  DCHECK(shrink_by % kEntryLength == 0);
  DCHECK(shrink_by <= code_map->length() - kEntriesStart);
  // Always trim even when array is cleared because of heap verifier.
  GetHeap()->RightTrimFixedArray<Heap::FROM_GC>(code_map, shrink_by);
  if (code_map->length() == kEntriesStart) {
    ClearOptimizedCodeMap();
  }
}


void JSObject::OptimizeAsPrototype(Handle<JSObject> object,
                                   PrototypeOptimizationMode mode) {
  if (object->IsGlobalObject()) return;
  if (object->IsJSGlobalProxy()) return;
  if (mode == FAST_PROTOTYPE && !object->map()->is_prototype_map()) {
    // First normalize to ensure all JSFunctions are CONSTANT.
    JSObject::NormalizeProperties(object, KEEP_INOBJECT_PROPERTIES, 0);
  }
  if (!object->HasFastProperties()) {
    JSObject::MigrateSlowToFast(object, 0);
  }
  if (mode == FAST_PROTOTYPE && object->HasFastProperties() &&
      !object->map()->is_prototype_map()) {
    Handle<Map> new_map = Map::Copy(handle(object->map()));
    JSObject::MigrateToMap(object, new_map);
    object->map()->set_is_prototype_map(true);
  }
}


void JSObject::ReoptimizeIfPrototype(Handle<JSObject> object) {
  if (!object->map()->is_prototype_map()) return;
  OptimizeAsPrototype(object, FAST_PROTOTYPE);
}


Handle<Object> CacheInitialJSArrayMaps(
    Handle<Context> native_context, Handle<Map> initial_map) {
  // Replace all of the cached initial array maps in the native context with
  // the appropriate transitioned elements kind maps.
  Factory* factory = native_context->GetIsolate()->factory();
  Handle<FixedArray> maps = factory->NewFixedArrayWithHoles(
      kElementsKindCount, TENURED);

  Handle<Map> current_map = initial_map;
  ElementsKind kind = current_map->elements_kind();
  DCHECK(kind == GetInitialFastElementsKind());
  maps->set(kind, *current_map);
  for (int i = GetSequenceIndexFromFastElementsKind(kind) + 1;
       i < kFastElementsKindCount; ++i) {
    Handle<Map> new_map;
    ElementsKind next_kind = GetFastElementsKindFromSequenceIndex(i);
    if (current_map->HasElementsTransition()) {
      new_map = handle(current_map->elements_transition_map());
      DCHECK(new_map->elements_kind() == next_kind);
    } else {
      new_map = Map::CopyAsElementsKind(
          current_map, next_kind, INSERT_TRANSITION);
    }
    maps->set(next_kind, *new_map);
    current_map = new_map;
  }
  native_context->set_js_array_maps(*maps);
  return initial_map;
}


void JSFunction::SetInstancePrototype(Handle<JSFunction> function,
                                      Handle<Object> value) {
  Isolate* isolate = function->GetIsolate();

  DCHECK(value->IsJSReceiver());

  // Now some logic for the maps of the objects that are created by using this
  // function as a constructor.
  if (function->has_initial_map()) {
    // If the function has allocated the initial map replace it with a
    // copy containing the new prototype.  Also complete any in-object
    // slack tracking that is in progress at this point because it is
    // still tracking the old copy.
    if (function->IsInobjectSlackTrackingInProgress()) {
      function->CompleteInobjectSlackTracking();
    }

    Handle<Map> initial_map(function->initial_map(), isolate);

    if (!initial_map->GetIsolate()->bootstrapper()->IsActive() &&
        initial_map->instance_type() == JS_OBJECT_TYPE) {
      // Put the value in the initial map field until an initial map is needed.
      // At that point, a new initial map is created and the prototype is put
      // into the initial map where it belongs.
      function->set_prototype_or_initial_map(*value);
    } else {
      Handle<Map> new_map = Map::Copy(initial_map);
      JSFunction::SetInitialMap(function, new_map, value);

      // If the function is used as the global Array function, cache the
      // initial map (and transitioned versions) in the native context.
      Context* native_context = function->context()->native_context();
      Object* array_function =
          native_context->get(Context::ARRAY_FUNCTION_INDEX);
      if (array_function->IsJSFunction() &&
          *function == JSFunction::cast(array_function)) {
        CacheInitialJSArrayMaps(handle(native_context, isolate), new_map);
      }
    }

    // Deoptimize all code that embeds the previous initial map.
    initial_map->dependent_code()->DeoptimizeDependentCodeGroup(
        isolate, DependentCode::kInitialMapChangedGroup);
  } else {
    // Put the value in the initial map field until an initial map is
    // needed.  At that point, a new initial map is created and the
    // prototype is put into the initial map where it belongs.
    function->set_prototype_or_initial_map(*value);
  }
  isolate->heap()->ClearInstanceofCache();
}


void JSFunction::SetPrototype(Handle<JSFunction> function,
                              Handle<Object> value) {
  DCHECK(function->should_have_prototype());
  Handle<Object> construct_prototype = value;

  // If the value is not a JSReceiver, store the value in the map's
  // constructor field so it can be accessed.  Also, set the prototype
  // used for constructing objects to the original object prototype.
  // See ECMA-262 13.2.2.
  if (!value->IsJSReceiver()) {
    // Copy the map so this does not affect unrelated functions.
    // Remove map transitions because they point to maps with a
    // different prototype.
    Handle<Map> new_map = Map::Copy(handle(function->map()));

    JSObject::MigrateToMap(function, new_map);
    new_map->set_constructor(*value);
    new_map->set_non_instance_prototype(true);
    Isolate* isolate = new_map->GetIsolate();
    construct_prototype = handle(
        isolate->context()->native_context()->initial_object_prototype(),
        isolate);
  } else {
    function->map()->set_non_instance_prototype(false);
  }

  return SetInstancePrototype(function, construct_prototype);
}


bool JSFunction::RemovePrototype() {
  Context* native_context = context()->native_context();
  Map* no_prototype_map = shared()->strict_mode() == SLOPPY
      ? native_context->sloppy_function_without_prototype_map()
      : native_context->strict_function_without_prototype_map();

  if (map() == no_prototype_map) return true;

#ifdef DEBUG
  if (map() != (shared()->strict_mode() == SLOPPY
                   ? native_context->sloppy_function_map()
                   : native_context->strict_function_map())) {
    return false;
  }
#endif

  set_map(no_prototype_map);
  set_prototype_or_initial_map(no_prototype_map->GetHeap()->the_hole_value());
  return true;
}


void JSFunction::SetInitialMap(Handle<JSFunction> function, Handle<Map> map,
                               Handle<Object> prototype) {
  if (prototype->IsJSObject()) {
    Handle<JSObject> js_proto = Handle<JSObject>::cast(prototype);
    JSObject::OptimizeAsPrototype(js_proto, FAST_PROTOTYPE);
  }
  map->set_prototype(*prototype);
  function->set_prototype_or_initial_map(*map);
  map->set_constructor(*function);
}


void JSFunction::EnsureHasInitialMap(Handle<JSFunction> function) {
  if (function->has_initial_map()) return;
  Isolate* isolate = function->GetIsolate();

  // First create a new map with the size and number of in-object properties
  // suggested by the function.
  InstanceType instance_type;
  int instance_size;
  int in_object_properties;
  if (function->shared()->is_generator()) {
    instance_type = JS_GENERATOR_OBJECT_TYPE;
    instance_size = JSGeneratorObject::kSize;
    in_object_properties = 0;
  } else {
    instance_type = JS_OBJECT_TYPE;
    instance_size = function->shared()->CalculateInstanceSize();
    in_object_properties = function->shared()->CalculateInObjectProperties();
  }
  Handle<Map> map = isolate->factory()->NewMap(instance_type, instance_size);

  // Fetch or allocate prototype.
  Handle<Object> prototype;
  if (function->has_instance_prototype()) {
    prototype = handle(function->instance_prototype(), isolate);
  } else {
    prototype = isolate->factory()->NewFunctionPrototype(function);
  }
  map->set_inobject_properties(in_object_properties);
  map->set_unused_property_fields(in_object_properties);
  DCHECK(map->has_fast_object_elements());

  // Finally link initial map and constructor function.
  JSFunction::SetInitialMap(function, map, Handle<JSReceiver>::cast(prototype));

  if (!function->shared()->is_generator()) {
    function->StartInobjectSlackTracking();
  }
}


void JSFunction::SetInstanceClassName(String* name) {
  shared()->set_instance_class_name(name);
}


void JSFunction::PrintName(FILE* out) {
  SmartArrayPointer<char> name = shared()->DebugName()->ToCString();
  PrintF(out, "%s", name.get());
}


Context* JSFunction::NativeContextFromLiterals(FixedArray* literals) {
  return Context::cast(literals->get(JSFunction::kLiteralNativeContextIndex));
}


// The filter is a pattern that matches function names in this way:
//   "*"      all; the default
//   "-"      all but the top-level function
//   "-name"  all but the function "name"
//   ""       only the top-level function
//   "name"   only the function "name"
//   "name*"  only functions starting with "name"
//   "~"      none; the tilde is not an identifier
bool JSFunction::PassesFilter(const char* raw_filter) {
  if (*raw_filter == '*') return true;
  String* name = shared()->DebugName();
  Vector<const char> filter = CStrVector(raw_filter);
  if (filter.length() == 0) return name->length() == 0;
  if (filter[0] == '-') {
    // Negative filter.
    if (filter.length() == 1) {
      return (name->length() != 0);
    } else if (name->IsUtf8EqualTo(filter.SubVector(1, filter.length()))) {
      return false;
    }
    if (filter[filter.length() - 1] == '*' &&
        name->IsUtf8EqualTo(filter.SubVector(1, filter.length() - 1), true)) {
      return false;
    }
    return true;

  } else if (name->IsUtf8EqualTo(filter)) {
    return true;
  }
  if (filter[filter.length() - 1] == '*' &&
      name->IsUtf8EqualTo(filter.SubVector(0, filter.length() - 1), true)) {
    return true;
  }
  return false;
}


void Oddball::Initialize(Isolate* isolate,
                         Handle<Oddball> oddball,
                         const char* to_string,
                         Handle<Object> to_number,
                         byte kind) {
  Handle<String> internalized_to_string =
      isolate->factory()->InternalizeUtf8String(to_string);
  oddball->set_to_string(*internalized_to_string);
  oddball->set_to_number(*to_number);
  oddball->set_kind(kind);
}


void Script::InitLineEnds(Handle<Script> script) {
  if (!script->line_ends()->IsUndefined()) return;

  Isolate* isolate = script->GetIsolate();

  if (!script->source()->IsString()) {
    DCHECK(script->source()->IsUndefined());
    Handle<FixedArray> empty = isolate->factory()->NewFixedArray(0);
    script->set_line_ends(*empty);
    DCHECK(script->line_ends()->IsFixedArray());
    return;
  }

  Handle<String> src(String::cast(script->source()), isolate);

  Handle<FixedArray> array = String::CalculateLineEnds(src, true);

  if (*array != isolate->heap()->empty_fixed_array()) {
    array->set_map(isolate->heap()->fixed_cow_array_map());
  }

  script->set_line_ends(*array);
  DCHECK(script->line_ends()->IsFixedArray());
}


int Script::GetColumnNumber(Handle<Script> script, int code_pos) {
  int line_number = GetLineNumber(script, code_pos);
  if (line_number == -1) return -1;

  DisallowHeapAllocation no_allocation;
  FixedArray* line_ends_array = FixedArray::cast(script->line_ends());
  line_number = line_number - script->line_offset()->value();
  if (line_number == 0) return code_pos + script->column_offset()->value();
  int prev_line_end_pos =
      Smi::cast(line_ends_array->get(line_number - 1))->value();
  return code_pos - (prev_line_end_pos + 1);
}


int Script::GetLineNumberWithArray(int code_pos) {
  DisallowHeapAllocation no_allocation;
  DCHECK(line_ends()->IsFixedArray());
  FixedArray* line_ends_array = FixedArray::cast(line_ends());
  int line_ends_len = line_ends_array->length();
  if (line_ends_len == 0) return -1;

  if ((Smi::cast(line_ends_array->get(0)))->value() >= code_pos) {
    return line_offset()->value();
  }

  int left = 0;
  int right = line_ends_len;
  while (int half = (right - left) / 2) {
    if ((Smi::cast(line_ends_array->get(left + half)))->value() > code_pos) {
      right -= half;
    } else {
      left += half;
    }
  }
  return right + line_offset()->value();
}


int Script::GetLineNumber(Handle<Script> script, int code_pos) {
  InitLineEnds(script);
  return script->GetLineNumberWithArray(code_pos);
}


int Script::GetLineNumber(int code_pos) {
  DisallowHeapAllocation no_allocation;
  if (!line_ends()->IsUndefined()) return GetLineNumberWithArray(code_pos);

  // Slow mode: we do not have line_ends. We have to iterate through source.
  if (!source()->IsString()) return -1;

  String* source_string = String::cast(source());
  int line = 0;
  int len = source_string->length();
  for (int pos = 0; pos < len; pos++) {
    if (pos == code_pos) break;
    if (source_string->Get(pos) == '\n') line++;
  }
  return line;
}


Handle<Object> Script::GetNameOrSourceURL(Handle<Script> script) {
  Isolate* isolate = script->GetIsolate();
  Handle<String> name_or_source_url_key =
      isolate->factory()->InternalizeOneByteString(
          STATIC_ASCII_VECTOR("nameOrSourceURL"));
  Handle<JSObject> script_wrapper = Script::GetWrapper(script);
  Handle<Object> property = Object::GetProperty(
      script_wrapper, name_or_source_url_key).ToHandleChecked();
  DCHECK(property->IsJSFunction());
  Handle<JSFunction> method = Handle<JSFunction>::cast(property);
  Handle<Object> result;
  // Do not check against pending exception, since this function may be called
  // when an exception has already been pending.
  if (!Execution::TryCall(method, script_wrapper, 0, NULL).ToHandle(&result)) {
    return isolate->factory()->undefined_value();
  }
  return result;
}


// Wrappers for scripts are kept alive and cached in weak global
// handles referred from foreign objects held by the scripts as long as
// they are used. When they are not used anymore, the garbage
// collector will call the weak callback on the global handle
// associated with the wrapper and get rid of both the wrapper and the
// handle.
static void ClearWrapperCacheWeakCallback(
    const v8::WeakCallbackData<v8::Value, void>& data) {
  Object** location = reinterpret_cast<Object**>(data.GetParameter());
  JSValue* wrapper = JSValue::cast(*location);
  Script::cast(wrapper->value())->ClearWrapperCache();
}


void Script::ClearWrapperCache() {
  Foreign* foreign = wrapper();
  Object** location = reinterpret_cast<Object**>(foreign->foreign_address());
  DCHECK_EQ(foreign->foreign_address(), reinterpret_cast<Address>(location));
  foreign->set_foreign_address(0);
  GlobalHandles::Destroy(location);
  GetIsolate()->counters()->script_wrappers()->Decrement();
}


Handle<JSObject> Script::GetWrapper(Handle<Script> script) {
  if (script->wrapper()->foreign_address() != NULL) {
    // Return a handle for the existing script wrapper from the cache.
    return Handle<JSValue>(
        *reinterpret_cast<JSValue**>(script->wrapper()->foreign_address()));
  }
  Isolate* isolate = script->GetIsolate();
  // Construct a new script wrapper.
  isolate->counters()->script_wrappers()->Increment();
  Handle<JSFunction> constructor = isolate->script_function();
  Handle<JSValue> result =
      Handle<JSValue>::cast(isolate->factory()->NewJSObject(constructor));

  result->set_value(*script);

  // Create a new weak global handle and use it to cache the wrapper
  // for future use. The cache will automatically be cleared by the
  // garbage collector when it is not used anymore.
  Handle<Object> handle = isolate->global_handles()->Create(*result);
  GlobalHandles::MakeWeak(handle.location(),
                          reinterpret_cast<void*>(handle.location()),
                          &ClearWrapperCacheWeakCallback);
  script->wrapper()->set_foreign_address(
      reinterpret_cast<Address>(handle.location()));
  return result;
}


String* SharedFunctionInfo::DebugName() {
  Object* n = name();
  if (!n->IsString() || String::cast(n)->length() == 0) return inferred_name();
  return String::cast(n);
}


bool SharedFunctionInfo::HasSourceCode() const {
  return !script()->IsUndefined() &&
         !reinterpret_cast<Script*>(script())->source()->IsUndefined();
}


Handle<Object> SharedFunctionInfo::GetSourceCode() {
  if (!HasSourceCode()) return GetIsolate()->factory()->undefined_value();
  Handle<String> source(String::cast(Script::cast(script())->source()));
  return GetIsolate()->factory()->NewSubString(
      source, start_position(), end_position());
}


bool SharedFunctionInfo::IsInlineable() {
  // Check that the function has a script associated with it.
  if (!script()->IsScript()) return false;
  if (optimization_disabled()) return false;
  // If we never ran this (unlikely) then lets try to optimize it.
  if (code()->kind() != Code::FUNCTION) return true;
  return code()->optimizable();
}


int SharedFunctionInfo::SourceSize() {
  return end_position() - start_position();
}


int SharedFunctionInfo::CalculateInstanceSize() {
  int instance_size =
      JSObject::kHeaderSize +
      expected_nof_properties() * kPointerSize;
  if (instance_size > JSObject::kMaxInstanceSize) {
    instance_size = JSObject::kMaxInstanceSize;
  }
  return instance_size;
}


int SharedFunctionInfo::CalculateInObjectProperties() {
  return (CalculateInstanceSize() - JSObject::kHeaderSize) / kPointerSize;
}


// Output the source code without any allocation in the heap.
OStream& operator<<(OStream& os, const SourceCodeOf& v) {
  const SharedFunctionInfo* s = v.value;
  // For some native functions there is no source.
  if (!s->HasSourceCode()) return os << "<No Source>";

  // Get the source for the script which this function came from.
  // Don't use String::cast because we don't want more assertion errors while
  // we are already creating a stack dump.
  String* script_source =
      reinterpret_cast<String*>(Script::cast(s->script())->source());

  if (!script_source->LooksValid()) return os << "<Invalid Source>";

  if (!s->is_toplevel()) {
    os << "function ";
    Object* name = s->name();
    if (name->IsString() && String::cast(name)->length() > 0) {
      String::cast(name)->PrintUC16(os);
    }
  }

  int len = s->end_position() - s->start_position();
  if (len <= v.max_length || v.max_length < 0) {
    script_source->PrintUC16(os, s->start_position(), s->end_position());
    return os;
  } else {
    script_source->PrintUC16(os, s->start_position(),
                             s->start_position() + v.max_length);
    return os << "...\n";
  }
}


static bool IsCodeEquivalent(Code* code, Code* recompiled) {
  if (code->instruction_size() != recompiled->instruction_size()) return false;
  ByteArray* code_relocation = code->relocation_info();
  ByteArray* recompiled_relocation = recompiled->relocation_info();
  int length = code_relocation->length();
  if (length != recompiled_relocation->length()) return false;
  int compare = memcmp(code_relocation->GetDataStartAddress(),
                       recompiled_relocation->GetDataStartAddress(),
                       length);
  return compare == 0;
}


void SharedFunctionInfo::EnableDeoptimizationSupport(Code* recompiled) {
  DCHECK(!has_deoptimization_support());
  DisallowHeapAllocation no_allocation;
  Code* code = this->code();
  if (IsCodeEquivalent(code, recompiled)) {
    // Copy the deoptimization data from the recompiled code.
    code->set_deoptimization_data(recompiled->deoptimization_data());
    code->set_has_deoptimization_support(true);
  } else {
    // TODO(3025757): In case the recompiled isn't equivalent to the
    // old code, we have to replace it. We should try to avoid this
    // altogether because it flushes valuable type feedback by
    // effectively resetting all IC state.
    ReplaceCode(recompiled);
  }
  DCHECK(has_deoptimization_support());
}


void SharedFunctionInfo::DisableOptimization(BailoutReason reason) {
  // Disable optimization for the shared function info and mark the
  // code as non-optimizable. The marker on the shared function info
  // is there because we flush non-optimized code thereby loosing the
  // non-optimizable information for the code. When the code is
  // regenerated and set on the shared function info it is marked as
  // non-optimizable if optimization is disabled for the shared
  // function info.
  set_optimization_disabled(true);
  set_bailout_reason(reason);
  // Code should be the lazy compilation stub or else unoptimized.  If the
  // latter, disable optimization for the code too.
  DCHECK(code()->kind() == Code::FUNCTION || code()->kind() == Code::BUILTIN);
  if (code()->kind() == Code::FUNCTION) {
    code()->set_optimizable(false);
  }
  PROFILE(GetIsolate(), CodeDisableOptEvent(code(), this));
  if (FLAG_trace_opt) {
    PrintF("[disabled optimization for ");
    ShortPrint();
    PrintF(", reason: %s]\n", GetBailoutReason(reason));
  }
}


bool SharedFunctionInfo::VerifyBailoutId(BailoutId id) {
  DCHECK(!id.IsNone());
  Code* unoptimized = code();
  DeoptimizationOutputData* data =
      DeoptimizationOutputData::cast(unoptimized->deoptimization_data());
  unsigned ignore = Deoptimizer::GetOutputInfo(data, id, this);
  USE(ignore);
  return true;  // Return true if there was no DCHECK.
}


void JSFunction::StartInobjectSlackTracking() {
  DCHECK(has_initial_map() && !IsInobjectSlackTrackingInProgress());

  if (!FLAG_clever_optimizations) return;
  Map* map = initial_map();

  // Only initiate the tracking the first time.
  if (map->done_inobject_slack_tracking()) return;
  map->set_done_inobject_slack_tracking(true);

  // No tracking during the snapshot construction phase.
  Isolate* isolate = GetIsolate();
  if (isolate->serializer_enabled()) return;

  if (map->unused_property_fields() == 0) return;

  map->set_construction_count(kGenerousAllocationCount);
}


void SharedFunctionInfo::ResetForNewContext(int new_ic_age) {
  code()->ClearInlineCaches();
  // If we clear ICs, we need to clear the type feedback vector too, since
  // CallICs are synced with a feedback vector slot.
  ClearTypeFeedbackInfo();
  set_ic_age(new_ic_age);
  if (code()->kind() == Code::FUNCTION) {
    code()->set_profiler_ticks(0);
    if (optimization_disabled() &&
        opt_count() >= FLAG_max_opt_count) {
      // Re-enable optimizations if they were disabled due to opt_count limit.
      set_optimization_disabled(false);
      code()->set_optimizable(true);
    }
    set_opt_count(0);
    set_deopt_count(0);
  }
}


static void GetMinInobjectSlack(Map* map, void* data) {
  int slack = map->unused_property_fields();
  if (*reinterpret_cast<int*>(data) > slack) {
    *reinterpret_cast<int*>(data) = slack;
  }
}


static void ShrinkInstanceSize(Map* map, void* data) {
  int slack = *reinterpret_cast<int*>(data);
  map->set_inobject_properties(map->inobject_properties() - slack);
  map->set_unused_property_fields(map->unused_property_fields() - slack);
  map->set_instance_size(map->instance_size() - slack * kPointerSize);

  // Visitor id might depend on the instance size, recalculate it.
  map->set_visitor_id(StaticVisitorBase::GetVisitorId(map));
}


void JSFunction::CompleteInobjectSlackTracking() {
  DCHECK(has_initial_map());
  Map* map = initial_map();

  DCHECK(map->done_inobject_slack_tracking());
  map->set_construction_count(kNoSlackTracking);

  int slack = map->unused_property_fields();
  map->TraverseTransitionTree(&GetMinInobjectSlack, &slack);
  if (slack != 0) {
    // Resize the initial map and all maps in its transition tree.
    map->TraverseTransitionTree(&ShrinkInstanceSize, &slack);
  }
}


int SharedFunctionInfo::SearchOptimizedCodeMap(Context* native_context,
                                               BailoutId osr_ast_id) {
  DisallowHeapAllocation no_gc;
  DCHECK(native_context->IsNativeContext());
  if (!FLAG_cache_optimized_code) return -1;
  Object* value = optimized_code_map();
  if (!value->IsSmi()) {
    FixedArray* optimized_code_map = FixedArray::cast(value);
    int length = optimized_code_map->length();
    Smi* osr_ast_id_smi = Smi::FromInt(osr_ast_id.ToInt());
    for (int i = kEntriesStart; i < length; i += kEntryLength) {
      if (optimized_code_map->get(i + kContextOffset) == native_context &&
          optimized_code_map->get(i + kOsrAstIdOffset) == osr_ast_id_smi) {
        return i + kCachedCodeOffset;
      }
    }
    if (FLAG_trace_opt) {
      PrintF("[didn't find optimized code in optimized code map for ");
      ShortPrint();
      PrintF("]\n");
    }
  }
  return -1;
}


#define DECLARE_TAG(ignore1, name, ignore2) name,
const char* const VisitorSynchronization::kTags[
    VisitorSynchronization::kNumberOfSyncTags] = {
  VISITOR_SYNCHRONIZATION_TAGS_LIST(DECLARE_TAG)
};
#undef DECLARE_TAG


#define DECLARE_TAG(ignore1, ignore2, name) name,
const char* const VisitorSynchronization::kTagNames[
    VisitorSynchronization::kNumberOfSyncTags] = {
  VISITOR_SYNCHRONIZATION_TAGS_LIST(DECLARE_TAG)
};
#undef DECLARE_TAG


void ObjectVisitor::VisitCodeTarget(RelocInfo* rinfo) {
  DCHECK(RelocInfo::IsCodeTarget(rinfo->rmode()));
  Object* target = Code::GetCodeFromTargetAddress(rinfo->target_address());
  Object* old_target = target;
  VisitPointer(&target);
  CHECK_EQ(target, old_target);  // VisitPointer doesn't change Code* *target.
}


void ObjectVisitor::VisitCodeAgeSequence(RelocInfo* rinfo) {
  DCHECK(RelocInfo::IsCodeAgeSequence(rinfo->rmode()));
  Object* stub = rinfo->code_age_stub();
  if (stub) {
    VisitPointer(&stub);
  }
}


void ObjectVisitor::VisitCodeEntry(Address entry_address) {
  Object* code = Code::GetObjectFromEntryAddress(entry_address);
  Object* old_code = code;
  VisitPointer(&code);
  if (code != old_code) {
    Memory::Address_at(entry_address) = reinterpret_cast<Code*>(code)->entry();
  }
}


void ObjectVisitor::VisitCell(RelocInfo* rinfo) {
  DCHECK(rinfo->rmode() == RelocInfo::CELL);
  Object* cell = rinfo->target_cell();
  Object* old_cell = cell;
  VisitPointer(&cell);
  if (cell != old_cell) {
    rinfo->set_target_cell(reinterpret_cast<Cell*>(cell));
  }
}


void ObjectVisitor::VisitDebugTarget(RelocInfo* rinfo) {
  DCHECK((RelocInfo::IsJSReturn(rinfo->rmode()) &&
          rinfo->IsPatchedReturnSequence()) ||
         (RelocInfo::IsDebugBreakSlot(rinfo->rmode()) &&
          rinfo->IsPatchedDebugBreakSlotSequence()));
  Object* target = Code::GetCodeFromTargetAddress(rinfo->call_address());
  Object* old_target = target;
  VisitPointer(&target);
  CHECK_EQ(target, old_target);  // VisitPointer doesn't change Code* *target.
}


void ObjectVisitor::VisitEmbeddedPointer(RelocInfo* rinfo) {
  DCHECK(rinfo->rmode() == RelocInfo::EMBEDDED_OBJECT);
  Object* p = rinfo->target_object();
  VisitPointer(&p);
}


void ObjectVisitor::VisitExternalReference(RelocInfo* rinfo) {
  Address p = rinfo->target_reference();
  VisitExternalReference(&p);
}


void Code::InvalidateRelocation() {
  InvalidateEmbeddedObjects();
  set_relocation_info(GetHeap()->empty_byte_array());
}


void Code::InvalidateEmbeddedObjects() {
  Object* undefined = GetHeap()->undefined_value();
  Cell* undefined_cell = GetHeap()->undefined_cell();
  int mode_mask = RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT) |
                  RelocInfo::ModeMask(RelocInfo::CELL);
  for (RelocIterator it(this, mode_mask); !it.done(); it.next()) {
    RelocInfo::Mode mode = it.rinfo()->rmode();
    if (mode == RelocInfo::EMBEDDED_OBJECT) {
      it.rinfo()->set_target_object(undefined, SKIP_WRITE_BARRIER);
    } else if (mode == RelocInfo::CELL) {
      it.rinfo()->set_target_cell(undefined_cell, SKIP_WRITE_BARRIER);
    }
  }
}


void Code::Relocate(intptr_t delta) {
  for (RelocIterator it(this, RelocInfo::kApplyMask); !it.done(); it.next()) {
    it.rinfo()->apply(delta, SKIP_ICACHE_FLUSH);
  }
  CpuFeatures::FlushICache(instruction_start(), instruction_size());
}


void Code::CopyFrom(const CodeDesc& desc) {
  DCHECK(Marking::Color(this) == Marking::WHITE_OBJECT);

  // copy code
  CopyBytes(instruction_start(), desc.buffer,
            static_cast<size_t>(desc.instr_size));

  // copy reloc info
  CopyBytes(relocation_start(),
            desc.buffer + desc.buffer_size - desc.reloc_size,
            static_cast<size_t>(desc.reloc_size));

  // unbox handles and relocate
  intptr_t delta = instruction_start() - desc.buffer;
  int mode_mask = RelocInfo::kCodeTargetMask |
                  RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT) |
                  RelocInfo::ModeMask(RelocInfo::CELL) |
                  RelocInfo::ModeMask(RelocInfo::RUNTIME_ENTRY) |
                  RelocInfo::kApplyMask;
  // Needed to find target_object and runtime_entry on X64
  Assembler* origin = desc.origin;
  AllowDeferredHandleDereference embedding_raw_address;
  for (RelocIterator it(this, mode_mask); !it.done(); it.next()) {
    RelocInfo::Mode mode = it.rinfo()->rmode();
    if (mode == RelocInfo::EMBEDDED_OBJECT) {
      Handle<Object> p = it.rinfo()->target_object_handle(origin);
      it.rinfo()->set_target_object(*p, SKIP_WRITE_BARRIER, SKIP_ICACHE_FLUSH);
    } else if (mode == RelocInfo::CELL) {
      Handle<Cell> cell  = it.rinfo()->target_cell_handle();
      it.rinfo()->set_target_cell(*cell, SKIP_WRITE_BARRIER, SKIP_ICACHE_FLUSH);
    } else if (RelocInfo::IsCodeTarget(mode)) {
      // rewrite code handles in inline cache targets to direct
      // pointers to the first instruction in the code object
      Handle<Object> p = it.rinfo()->target_object_handle(origin);
      Code* code = Code::cast(*p);
      it.rinfo()->set_target_address(code->instruction_start(),
                                     SKIP_WRITE_BARRIER,
                                     SKIP_ICACHE_FLUSH);
    } else if (RelocInfo::IsRuntimeEntry(mode)) {
      Address p = it.rinfo()->target_runtime_entry(origin);
      it.rinfo()->set_target_runtime_entry(p, SKIP_WRITE_BARRIER,
                                           SKIP_ICACHE_FLUSH);
    } else if (mode == RelocInfo::CODE_AGE_SEQUENCE) {
      Handle<Object> p = it.rinfo()->code_age_stub_handle(origin);
      Code* code = Code::cast(*p);
      it.rinfo()->set_code_age_stub(code, SKIP_ICACHE_FLUSH);
    } else {
      it.rinfo()->apply(delta, SKIP_ICACHE_FLUSH);
    }
  }
  CpuFeatures::FlushICache(instruction_start(), instruction_size());
}


// Locate the source position which is closest to the address in the code. This
// is using the source position information embedded in the relocation info.
// The position returned is relative to the beginning of the script where the
// source for this function is found.
int Code::SourcePosition(Address pc) {
  int distance = kMaxInt;
  int position = RelocInfo::kNoPosition;  // Initially no position found.
  // Run through all the relocation info to find the best matching source
  // position. All the code needs to be considered as the sequence of the
  // instructions in the code does not necessarily follow the same order as the
  // source.
  RelocIterator it(this, RelocInfo::kPositionMask);
  while (!it.done()) {
    // Only look at positions after the current pc.
    if (it.rinfo()->pc() < pc) {
      // Get position and distance.

      int dist = static_cast<int>(pc - it.rinfo()->pc());
      int pos = static_cast<int>(it.rinfo()->data());
      // If this position is closer than the current candidate or if it has the
      // same distance as the current candidate and the position is higher then
      // this position is the new candidate.
      if ((dist < distance) ||
          (dist == distance && pos > position)) {
        position = pos;
        distance = dist;
      }
    }
    it.next();
  }
  return position;
}


// Same as Code::SourcePosition above except it only looks for statement
// positions.
int Code::SourceStatementPosition(Address pc) {
  // First find the position as close as possible using all position
  // information.
  int position = SourcePosition(pc);
  // Now find the closest statement position before the position.
  int statement_position = 0;
  RelocIterator it(this, RelocInfo::kPositionMask);
  while (!it.done()) {
    if (RelocInfo::IsStatementPosition(it.rinfo()->rmode())) {
      int p = static_cast<int>(it.rinfo()->data());
      if (statement_position < p && p <= position) {
        statement_position = p;
      }
    }
    it.next();
  }
  return statement_position;
}


SafepointEntry Code::GetSafepointEntry(Address pc) {
  SafepointTable table(this);
  SafepointEntry entry = table.FindEntry(pc);
  if (entry.is_valid() || !is_turbofanned()) {
    return entry;
  }

  // If the code is turbofanned, we might be looking for
  // an address that was patched by lazy deoptimization.
  // In that case look through the patch table, try to
  // lookup the original address there, and then use this
  // to find the safepoint entry.
  DeoptimizationInputData* deopt_data =
      DeoptimizationInputData::cast(deoptimization_data());
  intptr_t offset = pc - instruction_start();
  for (int i = 0; i < deopt_data->ReturnAddressPatchCount(); i++) {
    if (deopt_data->PatchedAddressPc(i)->value() == offset) {
      int original_offset = deopt_data->ReturnAddressPc(i)->value();
      return table.FindEntry(instruction_start() + original_offset);
    }
  }
  return SafepointEntry();
}


Object* Code::FindNthObject(int n, Map* match_map) {
  DCHECK(is_inline_cache_stub());
  DisallowHeapAllocation no_allocation;
  int mask = RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT);
  for (RelocIterator it(this, mask); !it.done(); it.next()) {
    RelocInfo* info = it.rinfo();
    Object* object = info->target_object();
    if (object->IsHeapObject()) {
      if (HeapObject::cast(object)->map() == match_map) {
        if (--n == 0) return object;
      }
    }
  }
  return NULL;
}


AllocationSite* Code::FindFirstAllocationSite() {
  Object* result = FindNthObject(1, GetHeap()->allocation_site_map());
  return (result != NULL) ? AllocationSite::cast(result) : NULL;
}


Map* Code::FindFirstMap() {
  Object* result = FindNthObject(1, GetHeap()->meta_map());
  return (result != NULL) ? Map::cast(result) : NULL;
}


void Code::FindAndReplace(const FindAndReplacePattern& pattern) {
  DCHECK(is_inline_cache_stub() || is_handler());
  DisallowHeapAllocation no_allocation;
  int mask = RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT);
  STATIC_ASSERT(FindAndReplacePattern::kMaxCount < 32);
  int current_pattern = 0;
  for (RelocIterator it(this, mask); !it.done(); it.next()) {
    RelocInfo* info = it.rinfo();
    Object* object = info->target_object();
    if (object->IsHeapObject()) {
      Map* map = HeapObject::cast(object)->map();
      if (map == *pattern.find_[current_pattern]) {
        info->set_target_object(*pattern.replace_[current_pattern]);
        if (++current_pattern == pattern.count_) return;
      }
    }
  }
  UNREACHABLE();
}


void Code::FindAllMaps(MapHandleList* maps) {
  DCHECK(is_inline_cache_stub());
  DisallowHeapAllocation no_allocation;
  int mask = RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT);
  for (RelocIterator it(this, mask); !it.done(); it.next()) {
    RelocInfo* info = it.rinfo();
    Object* object = info->target_object();
    if (object->IsMap()) maps->Add(handle(Map::cast(object)));
  }
}


Code* Code::FindFirstHandler() {
  DCHECK(is_inline_cache_stub());
  DisallowHeapAllocation no_allocation;
  int mask = RelocInfo::ModeMask(RelocInfo::CODE_TARGET);
  for (RelocIterator it(this, mask); !it.done(); it.next()) {
    RelocInfo* info = it.rinfo();
    Code* code = Code::GetCodeFromTargetAddress(info->target_address());
    if (code->kind() == Code::HANDLER) return code;
  }
  return NULL;
}


bool Code::FindHandlers(CodeHandleList* code_list, int length) {
  DCHECK(is_inline_cache_stub());
  DisallowHeapAllocation no_allocation;
  int mask = RelocInfo::ModeMask(RelocInfo::CODE_TARGET);
  int i = 0;
  for (RelocIterator it(this, mask); !it.done(); it.next()) {
    if (i == length) return true;
    RelocInfo* info = it.rinfo();
    Code* code = Code::GetCodeFromTargetAddress(info->target_address());
    // IC stubs with handlers never contain non-handler code objects before
    // handler targets.
    if (code->kind() != Code::HANDLER) break;
    code_list->Add(Handle<Code>(code));
    i++;
  }
  return i == length;
}


MaybeHandle<Code> Code::FindHandlerForMap(Map* map) {
  DCHECK(is_inline_cache_stub());
  int mask = RelocInfo::ModeMask(RelocInfo::CODE_TARGET) |
             RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT);
  bool return_next = false;
  for (RelocIterator it(this, mask); !it.done(); it.next()) {
    RelocInfo* info = it.rinfo();
    if (info->rmode() == RelocInfo::EMBEDDED_OBJECT) {
      Object* object = info->target_object();
      if (object == map) return_next = true;
    } else if (return_next) {
      Code* code = Code::GetCodeFromTargetAddress(info->target_address());
      DCHECK(code->kind() == Code::HANDLER);
      return handle(code);
    }
  }
  return MaybeHandle<Code>();
}


Name* Code::FindFirstName() {
  DCHECK(is_inline_cache_stub());
  DisallowHeapAllocation no_allocation;
  int mask = RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT);
  for (RelocIterator it(this, mask); !it.done(); it.next()) {
    RelocInfo* info = it.rinfo();
    Object* object = info->target_object();
    if (object->IsName()) return Name::cast(object);
  }
  return NULL;
}


void Code::ClearInlineCaches() {
  ClearInlineCaches(NULL);
}


void Code::ClearInlineCaches(Code::Kind kind) {
  ClearInlineCaches(&kind);
}


void Code::ClearInlineCaches(Code::Kind* kind) {
  int mask = RelocInfo::ModeMask(RelocInfo::CODE_TARGET) |
             RelocInfo::ModeMask(RelocInfo::CONSTRUCT_CALL) |
             RelocInfo::ModeMask(RelocInfo::CODE_TARGET_WITH_ID);
  for (RelocIterator it(this, mask); !it.done(); it.next()) {
    RelocInfo* info = it.rinfo();
    Code* target(Code::GetCodeFromTargetAddress(info->target_address()));
    if (target->is_inline_cache_stub()) {
      if (kind == NULL || *kind == target->kind()) {
        IC::Clear(this->GetIsolate(), info->pc(),
                  info->host()->constant_pool());
      }
    }
  }
}


void SharedFunctionInfo::ClearTypeFeedbackInfo() {
  FixedArray* vector = feedback_vector();
  Heap* heap = GetHeap();
  int length = vector->length();

  for (int i = 0; i < length; i++) {
    Object* obj = vector->get(i);
    if (obj->IsHeapObject()) {
      InstanceType instance_type =
          HeapObject::cast(obj)->map()->instance_type();
      switch (instance_type) {
        case ALLOCATION_SITE_TYPE:
          // AllocationSites are not cleared because they do not store
          // information that leaks.
          break;
          // Fall through...
        default:
          vector->set(i, TypeFeedbackInfo::RawUninitializedSentinel(heap),
                      SKIP_WRITE_BARRIER);
      }
    }
  }
}


BailoutId Code::TranslatePcOffsetToAstId(uint32_t pc_offset) {
  DisallowHeapAllocation no_gc;
  DCHECK(kind() == FUNCTION);
  BackEdgeTable back_edges(this, &no_gc);
  for (uint32_t i = 0; i < back_edges.length(); i++) {
    if (back_edges.pc_offset(i) == pc_offset) return back_edges.ast_id(i);
  }
  return BailoutId::None();
}


uint32_t Code::TranslateAstIdToPcOffset(BailoutId ast_id) {
  DisallowHeapAllocation no_gc;
  DCHECK(kind() == FUNCTION);
  BackEdgeTable back_edges(this, &no_gc);
  for (uint32_t i = 0; i < back_edges.length(); i++) {
    if (back_edges.ast_id(i) == ast_id) return back_edges.pc_offset(i);
  }
  UNREACHABLE();  // We expect to find the back edge.
  return 0;
}


void Code::MakeCodeAgeSequenceYoung(byte* sequence, Isolate* isolate) {
  PatchPlatformCodeAge(isolate, sequence, kNoAgeCodeAge, NO_MARKING_PARITY);
}


void Code::MarkCodeAsExecuted(byte* sequence, Isolate* isolate) {
  PatchPlatformCodeAge(isolate, sequence, kExecutedOnceCodeAge,
      NO_MARKING_PARITY);
}


static Code::Age EffectiveAge(Code::Age age) {
  if (age == Code::kNotExecutedCodeAge) {
    // Treat that's never been executed as old immediately.
    age = Code::kIsOldCodeAge;
  } else if (age == Code::kExecutedOnceCodeAge) {
    // Pre-age code that has only been executed once.
    age = Code::kPreAgedCodeAge;
  }
  return age;
}


void Code::MakeOlder(MarkingParity current_parity) {
  byte* sequence = FindCodeAgeSequence();
  if (sequence != NULL) {
    Age age;
    MarkingParity code_parity;
    Isolate* isolate = GetIsolate();
    GetCodeAgeAndParity(isolate, sequence, &age, &code_parity);
    age = EffectiveAge(age);
    if (age != kLastCodeAge && code_parity != current_parity) {
      PatchPlatformCodeAge(isolate,
                           sequence,
                           static_cast<Age>(age + 1),
                           current_parity);
    }
  }
}


bool Code::IsOld() {
  return GetAge() >= kIsOldCodeAge;
}


byte* Code::FindCodeAgeSequence() {
  return FLAG_age_code &&
      prologue_offset() != Code::kPrologueOffsetNotSet &&
      (kind() == OPTIMIZED_FUNCTION ||
       (kind() == FUNCTION && !has_debug_break_slots()))
      ? instruction_start() + prologue_offset()
      : NULL;
}


Code::Age Code::GetAge() {
  return EffectiveAge(GetRawAge());
}


Code::Age Code::GetRawAge() {
  byte* sequence = FindCodeAgeSequence();
  if (sequence == NULL) {
    return kNoAgeCodeAge;
  }
  Age age;
  MarkingParity parity;
  GetCodeAgeAndParity(GetIsolate(), sequence, &age, &parity);
  return age;
}


void Code::GetCodeAgeAndParity(Code* code, Age* age,
                               MarkingParity* parity) {
  Isolate* isolate = code->GetIsolate();
  Builtins* builtins = isolate->builtins();
  Code* stub = NULL;
#define HANDLE_CODE_AGE(AGE)                                            \
  stub = *builtins->Make##AGE##CodeYoungAgainEvenMarking();             \
  if (code == stub) {                                                   \
    *age = k##AGE##CodeAge;                                             \
    *parity = EVEN_MARKING_PARITY;                                      \
    return;                                                             \
  }                                                                     \
  stub = *builtins->Make##AGE##CodeYoungAgainOddMarking();              \
  if (code == stub) {                                                   \
    *age = k##AGE##CodeAge;                                             \
    *parity = ODD_MARKING_PARITY;                                       \
    return;                                                             \
  }
  CODE_AGE_LIST(HANDLE_CODE_AGE)
#undef HANDLE_CODE_AGE
  stub = *builtins->MarkCodeAsExecutedOnce();
  if (code == stub) {
    *age = kNotExecutedCodeAge;
    *parity = NO_MARKING_PARITY;
    return;
  }
  stub = *builtins->MarkCodeAsExecutedTwice();
  if (code == stub) {
    *age = kExecutedOnceCodeAge;
    *parity = NO_MARKING_PARITY;
    return;
  }
  UNREACHABLE();
}


Code* Code::GetCodeAgeStub(Isolate* isolate, Age age, MarkingParity parity) {
  Builtins* builtins = isolate->builtins();
  switch (age) {
#define HANDLE_CODE_AGE(AGE)                                            \
    case k##AGE##CodeAge: {                                             \
      Code* stub = parity == EVEN_MARKING_PARITY                        \
          ? *builtins->Make##AGE##CodeYoungAgainEvenMarking()           \
          : *builtins->Make##AGE##CodeYoungAgainOddMarking();           \
      return stub;                                                      \
    }
    CODE_AGE_LIST(HANDLE_CODE_AGE)
#undef HANDLE_CODE_AGE
    case kNotExecutedCodeAge: {
      DCHECK(parity == NO_MARKING_PARITY);
      return *builtins->MarkCodeAsExecutedOnce();
    }
    case kExecutedOnceCodeAge: {
      DCHECK(parity == NO_MARKING_PARITY);
      return *builtins->MarkCodeAsExecutedTwice();
    }
    default:
      UNREACHABLE();
      break;
  }
  return NULL;
}


void Code::PrintDeoptLocation(FILE* out, int bailout_id) {
  const char* last_comment = NULL;
  int mask = RelocInfo::ModeMask(RelocInfo::COMMENT)
      | RelocInfo::ModeMask(RelocInfo::RUNTIME_ENTRY);
  for (RelocIterator it(this, mask); !it.done(); it.next()) {
    RelocInfo* info = it.rinfo();
    if (info->rmode() == RelocInfo::COMMENT) {
      last_comment = reinterpret_cast<const char*>(info->data());
    } else if (last_comment != NULL) {
      if ((bailout_id == Deoptimizer::GetDeoptimizationId(
              GetIsolate(), info->target_address(), Deoptimizer::EAGER)) ||
          (bailout_id == Deoptimizer::GetDeoptimizationId(
              GetIsolate(), info->target_address(), Deoptimizer::SOFT)) ||
          (bailout_id == Deoptimizer::GetDeoptimizationId(
              GetIsolate(), info->target_address(), Deoptimizer::LAZY))) {
        CHECK(RelocInfo::IsRuntimeEntry(info->rmode()));
        PrintF(out, "            %s\n", last_comment);
        return;
      }
    }
  }
}


bool Code::CanDeoptAt(Address pc) {
  DeoptimizationInputData* deopt_data =
      DeoptimizationInputData::cast(deoptimization_data());
  Address code_start_address = instruction_start();
  for (int i = 0; i < deopt_data->DeoptCount(); i++) {
    if (deopt_data->Pc(i)->value() == -1) continue;
    Address address = code_start_address + deopt_data->Pc(i)->value();
    if (address == pc) return true;
  }
  return false;
}


// Identify kind of code.
const char* Code::Kind2String(Kind kind) {
  switch (kind) {
#define CASE(name) case name: return #name;
    CODE_KIND_LIST(CASE)
#undef CASE
    case NUMBER_OF_KINDS: break;
  }
  UNREACHABLE();
  return NULL;
}


#ifdef ENABLE_DISASSEMBLER

void DeoptimizationInputData::DeoptimizationInputDataPrint(
    OStream& os) {  // NOLINT
  disasm::NameConverter converter;
  int deopt_count = DeoptCount();
  os << "Deoptimization Input Data (deopt points = " << deopt_count << ")\n";
  if (0 != deopt_count) {
    os << " index  ast id    argc     pc";
    if (FLAG_print_code_verbose) os << "  commands";
    os << "\n";
  }
  for (int i = 0; i < deopt_count; i++) {
    // TODO(svenpanne) Add some basic formatting to our streams.
    Vector<char> buf1 = Vector<char>::New(128);
    SNPrintF(buf1, "%6d  %6d  %6d %6d", i, AstId(i).ToInt(),
             ArgumentsStackHeight(i)->value(), Pc(i)->value());
    os << buf1.start();

    if (!FLAG_print_code_verbose) {
      os << "\n";
      continue;
    }
    // Print details of the frame translation.
    int translation_index = TranslationIndex(i)->value();
    TranslationIterator iterator(TranslationByteArray(), translation_index);
    Translation::Opcode opcode =
        static_cast<Translation::Opcode>(iterator.Next());
    DCHECK(Translation::BEGIN == opcode);
    int frame_count = iterator.Next();
    int jsframe_count = iterator.Next();
    os << "  " << Translation::StringFor(opcode)
       << " {frame count=" << frame_count
       << ", js frame count=" << jsframe_count << "}\n";

    while (iterator.HasNext() &&
           Translation::BEGIN !=
           (opcode = static_cast<Translation::Opcode>(iterator.Next()))) {
      Vector<char> buf2 = Vector<char>::New(128);
      SNPrintF(buf2, "%27s    %s ", "", Translation::StringFor(opcode));
      os << buf2.start();

      switch (opcode) {
        case Translation::BEGIN:
          UNREACHABLE();
          break;

        case Translation::JS_FRAME: {
          int ast_id = iterator.Next();
          int function_id = iterator.Next();
          unsigned height = iterator.Next();
          os << "{ast_id=" << ast_id << ", function=";
          if (function_id != Translation::kSelfLiteralId) {
            Object* function = LiteralArray()->get(function_id);
            os << Brief(JSFunction::cast(function)->shared()->DebugName());
          } else {
            os << "<self>";
          }
          os << ", height=" << height << "}";
          break;
        }

        case Translation::COMPILED_STUB_FRAME: {
          Code::Kind stub_kind = static_cast<Code::Kind>(iterator.Next());
          os << "{kind=" << stub_kind << "}";
          break;
        }

        case Translation::ARGUMENTS_ADAPTOR_FRAME:
        case Translation::CONSTRUCT_STUB_FRAME: {
          int function_id = iterator.Next();
          JSFunction* function =
              JSFunction::cast(LiteralArray()->get(function_id));
          unsigned height = iterator.Next();
          os << "{function=" << Brief(function->shared()->DebugName())
             << ", height=" << height << "}";
          break;
        }

        case Translation::GETTER_STUB_FRAME:
        case Translation::SETTER_STUB_FRAME: {
          int function_id = iterator.Next();
          JSFunction* function =
              JSFunction::cast(LiteralArray()->get(function_id));
          os << "{function=" << Brief(function->shared()->DebugName()) << "}";
          break;
        }

        case Translation::REGISTER: {
          int reg_code = iterator.Next();
          os << "{input=" << converter.NameOfCPURegister(reg_code) << "}";
          break;
        }

        case Translation::INT32_REGISTER: {
          int reg_code = iterator.Next();
          os << "{input=" << converter.NameOfCPURegister(reg_code) << "}";
          break;
        }

        case Translation::UINT32_REGISTER: {
          int reg_code = iterator.Next();
          os << "{input=" << converter.NameOfCPURegister(reg_code)
             << " (unsigned)}";
          break;
        }

        case Translation::DOUBLE_REGISTER: {
          int reg_code = iterator.Next();
          os << "{input=" << DoubleRegister::AllocationIndexToString(reg_code)
             << "}";
          break;
        }

        case Translation::STACK_SLOT: {
          int input_slot_index = iterator.Next();
          os << "{input=" << input_slot_index << "}";
          break;
        }

        case Translation::INT32_STACK_SLOT: {
          int input_slot_index = iterator.Next();
          os << "{input=" << input_slot_index << "}";
          break;
        }

        case Translation::UINT32_STACK_SLOT: {
          int input_slot_index = iterator.Next();
          os << "{input=" << input_slot_index << " (unsigned)}";
          break;
        }

        case Translation::DOUBLE_STACK_SLOT: {
          int input_slot_index = iterator.Next();
          os << "{input=" << input_slot_index << "}";
          break;
        }

        case Translation::LITERAL: {
          unsigned literal_index = iterator.Next();
          os << "{literal_id=" << literal_index << "}";
          break;
        }

        case Translation::DUPLICATED_OBJECT: {
          int object_index = iterator.Next();
          os << "{object_index=" << object_index << "}";
          break;
        }

        case Translation::ARGUMENTS_OBJECT:
        case Translation::CAPTURED_OBJECT: {
          int args_length = iterator.Next();
          os << "{length=" << args_length << "}";
          break;
        }
      }
      os << "\n";
    }
  }

  int return_address_patch_count = ReturnAddressPatchCount();
  if (return_address_patch_count != 0) {
    os << "Return address patch data (count = " << return_address_patch_count
       << ")\n";
    os << " index    pc  patched_pc\n";
  }
  for (int i = 0; i < return_address_patch_count; i++) {
    Vector<char> buf = Vector<char>::New(128);
    SNPrintF(buf, "%6d  %6d  %12d\n", i, ReturnAddressPc(i)->value(),
             PatchedAddressPc(i)->value());
    os << buf.start();
  }
}


void DeoptimizationOutputData::DeoptimizationOutputDataPrint(
    OStream& os) {  // NOLINT
  os << "Deoptimization Output Data (deopt points = " << this->DeoptPoints()
     << ")\n";
  if (this->DeoptPoints() == 0) return;

  os << "ast id        pc  state\n";
  for (int i = 0; i < this->DeoptPoints(); i++) {
    int pc_and_state = this->PcAndState(i)->value();
    // TODO(svenpanne) Add some basic formatting to our streams.
    Vector<char> buf = Vector<char>::New(100);
    SNPrintF(buf, "%6d  %8d  %s\n", this->AstId(i).ToInt(),
             FullCodeGenerator::PcField::decode(pc_and_state),
             FullCodeGenerator::State2String(
                 FullCodeGenerator::StateField::decode(pc_and_state)));
    os << buf.start();
  }
}


const char* Code::ICState2String(InlineCacheState state) {
  switch (state) {
    case UNINITIALIZED: return "UNINITIALIZED";
    case PREMONOMORPHIC: return "PREMONOMORPHIC";
    case MONOMORPHIC: return "MONOMORPHIC";
    case PROTOTYPE_FAILURE:
      return "PROTOTYPE_FAILURE";
    case POLYMORPHIC: return "POLYMORPHIC";
    case MEGAMORPHIC: return "MEGAMORPHIC";
    case GENERIC: return "GENERIC";
    case DEBUG_STUB: return "DEBUG_STUB";
    case DEFAULT:
      return "DEFAULT";
  }
  UNREACHABLE();
  return NULL;
}


const char* Code::StubType2String(StubType type) {
  switch (type) {
    case NORMAL: return "NORMAL";
    case FAST: return "FAST";
  }
  UNREACHABLE();  // keep the compiler happy
  return NULL;
}


void Code::PrintExtraICState(OStream& os,  // NOLINT
                             Kind kind, ExtraICState extra) {
  os << "extra_ic_state = ";
  if ((kind == STORE_IC || kind == KEYED_STORE_IC) && (extra == STRICT)) {
    os << "STRICT\n";
  } else {
    os << extra << "\n";
  }
}


void Code::Disassemble(const char* name, OStream& os) {  // NOLINT
  os << "kind = " << Kind2String(kind()) << "\n";
  if (IsCodeStubOrIC()) {
    const char* n = CodeStub::MajorName(CodeStub::GetMajorKey(this), true);
    os << "major_key = " << (n == NULL ? "null" : n) << "\n";
  }
  if (is_inline_cache_stub()) {
    os << "ic_state = " << ICState2String(ic_state()) << "\n";
    PrintExtraICState(os, kind(), extra_ic_state());
    if (ic_state() == MONOMORPHIC) {
      os << "type = " << StubType2String(type()) << "\n";
    }
    if (is_compare_ic_stub()) {
      DCHECK(CodeStub::GetMajorKey(this) == CodeStub::CompareIC);
      CompareIC::State left_state, right_state, handler_state;
      Token::Value op;
      ICCompareStub::DecodeKey(stub_key(), &left_state, &right_state,
                               &handler_state, &op);
      os << "compare_state = " << CompareIC::GetStateName(left_state) << "*"
         << CompareIC::GetStateName(right_state) << " -> "
         << CompareIC::GetStateName(handler_state) << "\n";
      os << "compare_operation = " << Token::Name(op) << "\n";
    }
  }
  if ((name != NULL) && (name[0] != '\0')) {
    os << "name = " << name << "\n";
  }
  if (kind() == OPTIMIZED_FUNCTION) {
    os << "stack_slots = " << stack_slots() << "\n";
  }

  os << "Instructions (size = " << instruction_size() << ")\n";
  // TODO(svenpanne) The Disassembler should use streams, too!
  {
    CodeTracer::Scope trace_scope(GetIsolate()->GetCodeTracer());
    Disassembler::Decode(trace_scope.file(), this);
  }
  os << "\n";

  if (kind() == FUNCTION) {
    DeoptimizationOutputData* data =
        DeoptimizationOutputData::cast(this->deoptimization_data());
    data->DeoptimizationOutputDataPrint(os);
  } else if (kind() == OPTIMIZED_FUNCTION) {
    DeoptimizationInputData* data =
        DeoptimizationInputData::cast(this->deoptimization_data());
    data->DeoptimizationInputDataPrint(os);
  }
  os << "\n";

  if (is_crankshafted()) {
    SafepointTable table(this);
    os << "Safepoints (size = " << table.size() << ")\n";
    for (unsigned i = 0; i < table.length(); i++) {
      unsigned pc_offset = table.GetPcOffset(i);
      os << (instruction_start() + pc_offset) << "  ";
      // TODO(svenpanne) Add some basic formatting to our streams.
      Vector<char> buf1 = Vector<char>::New(30);
      SNPrintF(buf1, "%4d", pc_offset);
      os << buf1.start() << "  ";
      table.PrintEntry(i, os);
      os << " (sp -> fp)  ";
      SafepointEntry entry = table.GetEntry(i);
      if (entry.deoptimization_index() != Safepoint::kNoDeoptimizationIndex) {
        Vector<char> buf2 = Vector<char>::New(30);
        SNPrintF(buf2, "%6d", entry.deoptimization_index());
        os << buf2.start();
      } else {
        os << "<none>";
      }
      if (entry.argument_count() > 0) {
        os << " argc: " << entry.argument_count();
      }
      os << "\n";
    }
    os << "\n";
  } else if (kind() == FUNCTION) {
    unsigned offset = back_edge_table_offset();
    // If there is no back edge table, the "table start" will be at or after
    // (due to alignment) the end of the instruction stream.
    if (static_cast<int>(offset) < instruction_size()) {
      DisallowHeapAllocation no_gc;
      BackEdgeTable back_edges(this, &no_gc);

      os << "Back edges (size = " << back_edges.length() << ")\n";
      os << "ast_id  pc_offset  loop_depth\n";

      for (uint32_t i = 0; i < back_edges.length(); i++) {
        Vector<char> buf = Vector<char>::New(100);
        SNPrintF(buf, "%6d  %9u  %10u\n", back_edges.ast_id(i).ToInt(),
                 back_edges.pc_offset(i), back_edges.loop_depth(i));
        os << buf.start();
      }

      os << "\n";
    }
#ifdef OBJECT_PRINT
    if (!type_feedback_info()->IsUndefined()) {
      OFStream os(stdout);
      TypeFeedbackInfo::cast(type_feedback_info())->TypeFeedbackInfoPrint(os);
      os << "\n";
    }
#endif
  }

  os << "RelocInfo (size = " << relocation_size() << ")\n";
  for (RelocIterator it(this); !it.done(); it.next()) {
    it.rinfo()->Print(GetIsolate(), os);
  }
  os << "\n";
}
#endif  // ENABLE_DISASSEMBLER


Handle<FixedArray> JSObject::SetFastElementsCapacityAndLength(
    Handle<JSObject> object,
    int capacity,
    int length,
    SetFastElementsCapacitySmiMode smi_mode) {
  // We should never end in here with a pixel or external array.
  DCHECK(!object->HasExternalArrayElements());

  // Allocate a new fast elements backing store.
  Handle<FixedArray> new_elements =
      object->GetIsolate()->factory()->NewUninitializedFixedArray(capacity);

  ElementsKind elements_kind = object->GetElementsKind();
  ElementsKind new_elements_kind;
  // The resized array has FAST_*_SMI_ELEMENTS if the capacity mode forces it,
  // or if it's allowed and the old elements array contained only SMIs.
  bool has_fast_smi_elements =
      (smi_mode == kForceSmiElements) ||
      ((smi_mode == kAllowSmiElements) && object->HasFastSmiElements());
  if (has_fast_smi_elements) {
    if (IsHoleyElementsKind(elements_kind)) {
      new_elements_kind = FAST_HOLEY_SMI_ELEMENTS;
    } else {
      new_elements_kind = FAST_SMI_ELEMENTS;
    }
  } else {
    if (IsHoleyElementsKind(elements_kind)) {
      new_elements_kind = FAST_HOLEY_ELEMENTS;
    } else {
      new_elements_kind = FAST_ELEMENTS;
    }
  }
  Handle<FixedArrayBase> old_elements(object->elements());
  ElementsAccessor* accessor = ElementsAccessor::ForKind(new_elements_kind);
  accessor->CopyElements(object, new_elements, elements_kind);

  if (elements_kind != SLOPPY_ARGUMENTS_ELEMENTS) {
    Handle<Map> new_map = (new_elements_kind != elements_kind)
        ? GetElementsTransitionMap(object, new_elements_kind)
        : handle(object->map());
    JSObject::ValidateElements(object);
    JSObject::SetMapAndElements(object, new_map, new_elements);

    // Transition through the allocation site as well if present.
    JSObject::UpdateAllocationSite(object, new_elements_kind);
  } else {
    Handle<FixedArray> parameter_map = Handle<FixedArray>::cast(old_elements);
    parameter_map->set(1, *new_elements);
  }

  if (FLAG_trace_elements_transitions) {
    PrintElementsTransition(stdout, object, elements_kind, old_elements,
                            object->GetElementsKind(), new_elements);
  }

  if (object->IsJSArray()) {
    Handle<JSArray>::cast(object)->set_length(Smi::FromInt(length));
  }
  return new_elements;
}


void JSObject::SetFastDoubleElementsCapacityAndLength(Handle<JSObject> object,
                                                      int capacity,
                                                      int length) {
  // We should never end in here with a pixel or external array.
  DCHECK(!object->HasExternalArrayElements());

  Handle<FixedArrayBase> elems =
      object->GetIsolate()->factory()->NewFixedDoubleArray(capacity);

  ElementsKind elements_kind = object->GetElementsKind();
  CHECK(elements_kind != SLOPPY_ARGUMENTS_ELEMENTS);
  ElementsKind new_elements_kind = elements_kind;
  if (IsHoleyElementsKind(elements_kind)) {
    new_elements_kind = FAST_HOLEY_DOUBLE_ELEMENTS;
  } else {
    new_elements_kind = FAST_DOUBLE_ELEMENTS;
  }

  Handle<Map> new_map = GetElementsTransitionMap(object, new_elements_kind);

  Handle<FixedArrayBase> old_elements(object->elements());
  ElementsAccessor* accessor = ElementsAccessor::ForKind(FAST_DOUBLE_ELEMENTS);
  accessor->CopyElements(object, elems, elements_kind);

  JSObject::ValidateElements(object);
  JSObject::SetMapAndElements(object, new_map, elems);

  if (FLAG_trace_elements_transitions) {
    PrintElementsTransition(stdout, object, elements_kind, old_elements,
                            object->GetElementsKind(), elems);
  }

  if (object->IsJSArray()) {
    Handle<JSArray>::cast(object)->set_length(Smi::FromInt(length));
  }
}


// static
void JSArray::Initialize(Handle<JSArray> array, int capacity, int length) {
  DCHECK(capacity >= 0);
  array->GetIsolate()->factory()->NewJSArrayStorage(
      array, length, capacity, INITIALIZE_ARRAY_ELEMENTS_WITH_HOLE);
}


void JSArray::Expand(Handle<JSArray> array, int required_size) {
  ElementsAccessor* accessor = array->GetElementsAccessor();
  accessor->SetCapacityAndLength(array, required_size, required_size);
}


// Returns false if the passed-in index is marked non-configurable,
// which will cause the ES5 truncation operation to halt, and thus
// no further old values need be collected.
static bool GetOldValue(Isolate* isolate,
                        Handle<JSObject> object,
                        uint32_t index,
                        List<Handle<Object> >* old_values,
                        List<uint32_t>* indices) {
  Maybe<PropertyAttributes> maybe =
      JSReceiver::GetOwnElementAttribute(object, index);
  DCHECK(maybe.has_value);
  DCHECK(maybe.value != ABSENT);
  if (maybe.value == DONT_DELETE) return false;
  Handle<Object> value;
  if (!JSObject::GetOwnElementAccessorPair(object, index).is_null()) {
    value = Handle<Object>::cast(isolate->factory()->the_hole_value());
  } else {
    value = Object::GetElement(isolate, object, index).ToHandleChecked();
  }
  old_values->Add(value);
  indices->Add(index);
  return true;
}

static void EnqueueSpliceRecord(Handle<JSArray> object,
                                uint32_t index,
                                Handle<JSArray> deleted,
                                uint32_t add_count) {
  Isolate* isolate = object->GetIsolate();
  HandleScope scope(isolate);
  Handle<Object> index_object = isolate->factory()->NewNumberFromUint(index);
  Handle<Object> add_count_object =
      isolate->factory()->NewNumberFromUint(add_count);

  Handle<Object> args[] =
      { object, index_object, deleted, add_count_object };

  Execution::Call(isolate,
                  Handle<JSFunction>(isolate->observers_enqueue_splice()),
                  isolate->factory()->undefined_value(),
                  ARRAY_SIZE(args),
                  args).Assert();
}


static void BeginPerformSplice(Handle<JSArray> object) {
  Isolate* isolate = object->GetIsolate();
  HandleScope scope(isolate);
  Handle<Object> args[] = { object };

  Execution::Call(isolate,
                  Handle<JSFunction>(isolate->observers_begin_perform_splice()),
                  isolate->factory()->undefined_value(),
                  ARRAY_SIZE(args),
                  args).Assert();
}


static void EndPerformSplice(Handle<JSArray> object) {
  Isolate* isolate = object->GetIsolate();
  HandleScope scope(isolate);
  Handle<Object> args[] = { object };

  Execution::Call(isolate,
                  Handle<JSFunction>(isolate->observers_end_perform_splice()),
                  isolate->factory()->undefined_value(),
                  ARRAY_SIZE(args),
                  args).Assert();
}


MaybeHandle<Object> JSArray::SetElementsLength(
    Handle<JSArray> array,
    Handle<Object> new_length_handle) {
  if (array->HasFastElements()) {
    // If the new array won't fit in a some non-trivial fraction of the max old
    // space size, then force it to go dictionary mode.
    int max_fast_array_size = static_cast<int>(
        (array->GetHeap()->MaxOldGenerationSize() / kDoubleSize) / 4);
    if (new_length_handle->IsNumber() &&
        NumberToInt32(*new_length_handle) >= max_fast_array_size) {
      NormalizeElements(array);
    }
  }

  // We should never end in here with a pixel or external array.
  DCHECK(array->AllowsSetElementsLength());
  if (!array->map()->is_observed()) {
    return array->GetElementsAccessor()->SetLength(array, new_length_handle);
  }

  Isolate* isolate = array->GetIsolate();
  List<uint32_t> indices;
  List<Handle<Object> > old_values;
  Handle<Object> old_length_handle(array->length(), isolate);
  uint32_t old_length = 0;
  CHECK(old_length_handle->ToArrayIndex(&old_length));
  uint32_t new_length = 0;
  CHECK(new_length_handle->ToArrayIndex(&new_length));

  static const PropertyAttributes kNoAttrFilter = NONE;
  int num_elements = array->NumberOfOwnElements(kNoAttrFilter);
  if (num_elements > 0) {
    if (old_length == static_cast<uint32_t>(num_elements)) {
      // Simple case for arrays without holes.
      for (uint32_t i = old_length - 1; i + 1 > new_length; --i) {
        if (!GetOldValue(isolate, array, i, &old_values, &indices)) break;
      }
    } else {
      // For sparse arrays, only iterate over existing elements.
      // TODO(rafaelw): For fast, sparse arrays, we can avoid iterating over
      // the to-be-removed indices twice.
      Handle<FixedArray> keys = isolate->factory()->NewFixedArray(num_elements);
      array->GetOwnElementKeys(*keys, kNoAttrFilter);
      while (num_elements-- > 0) {
        uint32_t index = NumberToUint32(keys->get(num_elements));
        if (index < new_length) break;
        if (!GetOldValue(isolate, array, index, &old_values, &indices)) break;
      }
    }
  }

  Handle<Object> hresult;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, hresult,
      array->GetElementsAccessor()->SetLength(array, new_length_handle),
      Object);

  CHECK(array->length()->ToArrayIndex(&new_length));
  if (old_length == new_length) return hresult;

  BeginPerformSplice(array);

  for (int i = 0; i < indices.length(); ++i) {
    // For deletions where the property was an accessor, old_values[i]
    // will be the hole, which instructs EnqueueChangeRecord to elide
    // the "oldValue" property.
    JSObject::EnqueueChangeRecord(
        array, "delete", isolate->factory()->Uint32ToString(indices[i]),
        old_values[i]);
  }
  JSObject::EnqueueChangeRecord(
      array, "update", isolate->factory()->length_string(),
      old_length_handle);

  EndPerformSplice(array);

  uint32_t index = Min(old_length, new_length);
  uint32_t add_count = new_length > old_length ? new_length - old_length : 0;
  uint32_t delete_count = new_length < old_length ? old_length - new_length : 0;
  Handle<JSArray> deleted = isolate->factory()->NewJSArray(0);
  if (delete_count > 0) {
    for (int i = indices.length() - 1; i >= 0; i--) {
      // Skip deletions where the property was an accessor, leaving holes
      // in the array of old values.
      if (old_values[i]->IsTheHole()) continue;
      JSObject::SetElement(
          deleted, indices[i] - index, old_values[i], NONE, SLOPPY).Assert();
    }

    SetProperty(deleted, isolate->factory()->length_string(),
                isolate->factory()->NewNumberFromUint(delete_count),
                STRICT).Assert();
  }

  EnqueueSpliceRecord(array, index, deleted, add_count);

  return hresult;
}


Handle<Map> Map::GetPrototypeTransition(Handle<Map> map,
                                        Handle<Object> prototype) {
  FixedArray* cache = map->GetPrototypeTransitions();
  int number_of_transitions = map->NumberOfProtoTransitions();
  const int proto_offset =
      kProtoTransitionHeaderSize + kProtoTransitionPrototypeOffset;
  const int map_offset = kProtoTransitionHeaderSize + kProtoTransitionMapOffset;
  const int step = kProtoTransitionElementsPerEntry;
  for (int i = 0; i < number_of_transitions; i++) {
    if (cache->get(proto_offset + i * step) == *prototype) {
      Object* result = cache->get(map_offset + i * step);
      return Handle<Map>(Map::cast(result));
    }
  }
  return Handle<Map>();
}


Handle<Map> Map::PutPrototypeTransition(Handle<Map> map,
                                        Handle<Object> prototype,
                                        Handle<Map> target_map) {
  DCHECK(target_map->IsMap());
  DCHECK(HeapObject::cast(*prototype)->map()->IsMap());
  // Don't cache prototype transition if this map is either shared, or a map of
  // a prototype.
  if (map->is_prototype_map()) return map;
  if (map->is_dictionary_map() || !FLAG_cache_prototype_transitions) return map;

  const int step = kProtoTransitionElementsPerEntry;
  const int header = kProtoTransitionHeaderSize;

  Handle<FixedArray> cache(map->GetPrototypeTransitions());
  int capacity = (cache->length() - header) / step;
  int transitions = map->NumberOfProtoTransitions() + 1;

  if (transitions > capacity) {
    if (capacity > kMaxCachedPrototypeTransitions) return map;

    // Grow array by factor 2 over and above what we need.
    cache = FixedArray::CopySize(cache, transitions * 2 * step + header);

    SetPrototypeTransitions(map, cache);
  }

  // Reload number of transitions as GC might shrink them.
  int last = map->NumberOfProtoTransitions();
  int entry = header + last * step;

  cache->set(entry + kProtoTransitionPrototypeOffset, *prototype);
  cache->set(entry + kProtoTransitionMapOffset, *target_map);
  map->SetNumberOfProtoTransitions(last + 1);

  return map;
}


void Map::ZapTransitions() {
  TransitionArray* transition_array = transitions();
  // TODO(mstarzinger): Temporarily use a slower version instead of the faster
  // MemsetPointer to investigate a crasher. Switch back to MemsetPointer.
  Object** data = transition_array->data_start();
  Object* the_hole = GetHeap()->the_hole_value();
  int length = transition_array->length();
  for (int i = 0; i < length; i++) {
    data[i] = the_hole;
  }
}


void Map::ZapPrototypeTransitions() {
  FixedArray* proto_transitions = GetPrototypeTransitions();
  MemsetPointer(proto_transitions->data_start(),
                GetHeap()->the_hole_value(),
                proto_transitions->length());
}


// static
void Map::AddDependentCompilationInfo(Handle<Map> map,
                                      DependentCode::DependencyGroup group,
                                      CompilationInfo* info) {
  Handle<DependentCode> codes =
      DependentCode::Insert(handle(map->dependent_code(), info->isolate()),
                            group, info->object_wrapper());
  if (*codes != map->dependent_code()) map->set_dependent_code(*codes);
  info->dependencies(group)->Add(map, info->zone());
}


// static
void Map::AddDependentCode(Handle<Map> map,
                           DependentCode::DependencyGroup group,
                           Handle<Code> code) {
  Handle<DependentCode> codes = DependentCode::Insert(
      Handle<DependentCode>(map->dependent_code()), group, code);
  if (*codes != map->dependent_code()) map->set_dependent_code(*codes);
}


// static
void Map::AddDependentIC(Handle<Map> map,
                         Handle<Code> stub) {
  DCHECK(stub->next_code_link()->IsUndefined());
  int n = map->dependent_code()->number_of_entries(DependentCode::kWeakICGroup);
  if (n == 0) {
    // Slow path: insert the head of the list with possible heap allocation.
    Map::AddDependentCode(map, DependentCode::kWeakICGroup, stub);
  } else {
    // Fast path: link the stub to the existing head of the list without any
    // heap allocation.
    DCHECK(n == 1);
    map->dependent_code()->AddToDependentICList(stub);
  }
}


DependentCode::GroupStartIndexes::GroupStartIndexes(DependentCode* entries) {
  Recompute(entries);
}


void DependentCode::GroupStartIndexes::Recompute(DependentCode* entries) {
  start_indexes_[0] = 0;
  for (int g = 1; g <= kGroupCount; g++) {
    int count = entries->number_of_entries(static_cast<DependencyGroup>(g - 1));
    start_indexes_[g] = start_indexes_[g - 1] + count;
  }
}


DependentCode* DependentCode::ForObject(Handle<HeapObject> object,
                                        DependencyGroup group) {
  AllowDeferredHandleDereference dependencies_are_safe;
  if (group == DependentCode::kPropertyCellChangedGroup) {
    return Handle<PropertyCell>::cast(object)->dependent_code();
  } else if (group == DependentCode::kAllocationSiteTenuringChangedGroup ||
      group == DependentCode::kAllocationSiteTransitionChangedGroup) {
    return Handle<AllocationSite>::cast(object)->dependent_code();
  }
  return Handle<Map>::cast(object)->dependent_code();
}


Handle<DependentCode> DependentCode::Insert(Handle<DependentCode> entries,
                                            DependencyGroup group,
                                            Handle<Object> object) {
  GroupStartIndexes starts(*entries);
  int start = starts.at(group);
  int end = starts.at(group + 1);
  int number_of_entries = starts.number_of_entries();
  // Check for existing entry to avoid duplicates.
  for (int i = start; i < end; i++) {
    if (entries->object_at(i) == *object) return entries;
  }
  if (entries->length() < kCodesStartIndex + number_of_entries + 1) {
    int capacity = kCodesStartIndex + number_of_entries + 1;
    if (capacity > 5) capacity = capacity * 5 / 4;
    Handle<DependentCode> new_entries = Handle<DependentCode>::cast(
        FixedArray::CopySize(entries, capacity, TENURED));
    // The number of codes can change after GC.
    starts.Recompute(*entries);
    start = starts.at(group);
    end = starts.at(group + 1);
    number_of_entries = starts.number_of_entries();
    for (int i = 0; i < number_of_entries; i++) {
      entries->clear_at(i);
    }
    // If the old fixed array was empty, we need to reset counters of the
    // new array.
    if (number_of_entries == 0) {
      for (int g = 0; g < kGroupCount; g++) {
        new_entries->set_number_of_entries(static_cast<DependencyGroup>(g), 0);
      }
    }
    entries = new_entries;
  }
  entries->ExtendGroup(group);
  entries->set_object_at(end, *object);
  entries->set_number_of_entries(group, end + 1 - start);
  return entries;
}


void DependentCode::UpdateToFinishedCode(DependencyGroup group,
                                         CompilationInfo* info,
                                         Code* code) {
  DisallowHeapAllocation no_gc;
  AllowDeferredHandleDereference get_object_wrapper;
  Foreign* info_wrapper = *info->object_wrapper();
  GroupStartIndexes starts(this);
  int start = starts.at(group);
  int end = starts.at(group + 1);
  for (int i = start; i < end; i++) {
    if (object_at(i) == info_wrapper) {
      set_object_at(i, code);
      break;
    }
  }

#ifdef DEBUG
  for (int i = start; i < end; i++) {
    DCHECK(is_code_at(i) || compilation_info_at(i) != info);
  }
#endif
}


void DependentCode::RemoveCompilationInfo(DependentCode::DependencyGroup group,
                                          CompilationInfo* info) {
  DisallowHeapAllocation no_allocation;
  AllowDeferredHandleDereference get_object_wrapper;
  Foreign* info_wrapper = *info->object_wrapper();
  GroupStartIndexes starts(this);
  int start = starts.at(group);
  int end = starts.at(group + 1);
  // Find compilation info wrapper.
  int info_pos = -1;
  for (int i = start; i < end; i++) {
    if (object_at(i) == info_wrapper) {
      info_pos = i;
      break;
    }
  }
  if (info_pos == -1) return;  // Not found.
  int gap = info_pos;
  // Use the last of each group to fill the gap in the previous group.
  for (int i = group; i < kGroupCount; i++) {
    int last_of_group = starts.at(i + 1) - 1;
    DCHECK(last_of_group >= gap);
    if (last_of_group == gap) continue;
    copy(last_of_group, gap);
    gap = last_of_group;
  }
  DCHECK(gap == starts.number_of_entries() - 1);
  clear_at(gap);  // Clear last gap.
  set_number_of_entries(group, end - start - 1);

#ifdef DEBUG
  for (int i = start; i < end - 1; i++) {
    DCHECK(is_code_at(i) || compilation_info_at(i) != info);
  }
#endif
}


static bool CodeListContains(Object* head, Code* code) {
  while (!head->IsUndefined()) {
    if (head == code) return true;
    head = Code::cast(head)->next_code_link();
  }
  return false;
}


bool DependentCode::Contains(DependencyGroup group, Code* code) {
  GroupStartIndexes starts(this);
  int start = starts.at(group);
  int end = starts.at(group + 1);
  if (group == kWeakICGroup) {
    return CodeListContains(object_at(start), code);
  }
  for (int i = start; i < end; i++) {
    if (object_at(i) == code) return true;
  }
  return false;
}


bool DependentCode::MarkCodeForDeoptimization(
    Isolate* isolate,
    DependentCode::DependencyGroup group) {
  DisallowHeapAllocation no_allocation_scope;
  DependentCode::GroupStartIndexes starts(this);
  int start = starts.at(group);
  int end = starts.at(group + 1);
  int code_entries = starts.number_of_entries();
  if (start == end) return false;

  // Mark all the code that needs to be deoptimized.
  bool marked = false;
  for (int i = start; i < end; i++) {
    if (is_code_at(i)) {
      Code* code = code_at(i);
      if (!code->marked_for_deoptimization()) {
        code->set_marked_for_deoptimization(true);
        marked = true;
      }
    } else {
      CompilationInfo* info = compilation_info_at(i);
      info->AbortDueToDependencyChange();
    }
  }
  // Compact the array by moving all subsequent groups to fill in the new holes.
  for (int src = end, dst = start; src < code_entries; src++, dst++) {
    copy(src, dst);
  }
  // Now the holes are at the end of the array, zap them for heap-verifier.
  int removed = end - start;
  for (int i = code_entries - removed; i < code_entries; i++) {
    clear_at(i);
  }
  set_number_of_entries(group, 0);
  return marked;
}


void DependentCode::DeoptimizeDependentCodeGroup(
    Isolate* isolate,
    DependentCode::DependencyGroup group) {
  DCHECK(AllowCodeDependencyChange::IsAllowed());
  DisallowHeapAllocation no_allocation_scope;
  bool marked = MarkCodeForDeoptimization(isolate, group);

  if (marked) Deoptimizer::DeoptimizeMarkedCode(isolate);
}


void DependentCode::AddToDependentICList(Handle<Code> stub) {
  DisallowHeapAllocation no_heap_allocation;
  GroupStartIndexes starts(this);
  int i = starts.at(kWeakICGroup);
  Object* head = object_at(i);
  // Try to insert the stub after the head of the list to minimize number of
  // writes to the DependentCode array, since a write to the array can make it
  // strong if it was alread marked by incremental marker.
  if (head->IsCode()) {
    stub->set_next_code_link(Code::cast(head)->next_code_link());
    Code::cast(head)->set_next_code_link(*stub);
  } else {
    stub->set_next_code_link(head);
    set_object_at(i, *stub);
  }
}


Handle<Map> Map::TransitionToPrototype(Handle<Map> map,
                                       Handle<Object> prototype) {
  Handle<Map> new_map = GetPrototypeTransition(map, prototype);
  if (new_map.is_null()) {
    new_map = Copy(map);
    PutPrototypeTransition(map, prototype, new_map);
    new_map->set_prototype(*prototype);
  }
  return new_map;
}


MaybeHandle<Object> JSObject::SetPrototype(Handle<JSObject> object,
                                           Handle<Object> value,
                                           bool from_javascript) {
#ifdef DEBUG
  int size = object->Size();
#endif

  Isolate* isolate = object->GetIsolate();
  Heap* heap = isolate->heap();
  // Silently ignore the change if value is not a JSObject or null.
  // SpiderMonkey behaves this way.
  if (!value->IsJSReceiver() && !value->IsNull()) return value;

  // From 8.6.2 Object Internal Methods
  // ...
  // In addition, if [[Extensible]] is false the value of the [[Class]] and
  // [[Prototype]] internal properties of the object may not be modified.
  // ...
  // Implementation specific extensions that modify [[Class]], [[Prototype]]
  // or [[Extensible]] must not violate the invariants defined in the preceding
  // paragraph.
  if (!object->map()->is_extensible()) {
    Handle<Object> args[] = { object };
    Handle<Object> error = isolate->factory()->NewTypeError(
        "non_extensible_proto", HandleVector(args, ARRAY_SIZE(args)));
    return isolate->Throw<Object>(error);
  }

  // Before we can set the prototype we need to be sure
  // prototype cycles are prevented.
  // It is sufficient to validate that the receiver is not in the new prototype
  // chain.
  for (PrototypeIterator iter(isolate, *value,
                              PrototypeIterator::START_AT_RECEIVER);
       !iter.IsAtEnd(); iter.Advance()) {
    if (JSReceiver::cast(iter.GetCurrent()) == *object) {
      // Cycle detected.
      Handle<Object> error = isolate->factory()->NewError(
          "cyclic_proto", HandleVector<Object>(NULL, 0));
      return isolate->Throw<Object>(error);
    }
  }

  bool dictionary_elements_in_chain =
      object->map()->DictionaryElementsInPrototypeChainOnly();
  Handle<JSObject> real_receiver = object;

  if (from_javascript) {
    // Find the first object in the chain whose prototype object is not
    // hidden and set the new prototype on that object.
    PrototypeIterator iter(isolate, real_receiver);
    while (!iter.IsAtEnd(PrototypeIterator::END_AT_NON_HIDDEN)) {
      real_receiver =
          Handle<JSObject>::cast(PrototypeIterator::GetCurrent(iter));
      iter.Advance();
    }
  }

  // Set the new prototype of the object.
  Handle<Map> map(real_receiver->map());

  // Nothing to do if prototype is already set.
  if (map->prototype() == *value) return value;

  if (value->IsJSObject()) {
    PrototypeOptimizationMode mode =
        from_javascript ? REGULAR_PROTOTYPE : FAST_PROTOTYPE;
    JSObject::OptimizeAsPrototype(Handle<JSObject>::cast(value), mode);
  }

  Handle<Map> new_map = Map::TransitionToPrototype(map, value);
  DCHECK(new_map->prototype() == *value);
  JSObject::MigrateToMap(real_receiver, new_map);

  if (!dictionary_elements_in_chain &&
      new_map->DictionaryElementsInPrototypeChainOnly()) {
    // If the prototype chain didn't previously have element callbacks, then
    // KeyedStoreICs need to be cleared to ensure any that involve this
    // map go generic.
    object->GetHeap()->ClearAllICsByKind(Code::KEYED_STORE_IC);
  }

  heap->ClearInstanceofCache();
  DCHECK(size == object->Size());
  return value;
}


void JSObject::EnsureCanContainElements(Handle<JSObject> object,
                                        Arguments* args,
                                        uint32_t first_arg,
                                        uint32_t arg_count,
                                        EnsureElementsMode mode) {
  // Elements in |Arguments| are ordered backwards (because they're on the
  // stack), but the method that's called here iterates over them in forward
  // direction.
  return EnsureCanContainElements(
      object, args->arguments() - first_arg - (arg_count - 1), arg_count, mode);
}


MaybeHandle<AccessorPair> JSObject::GetOwnElementAccessorPair(
    Handle<JSObject> object,
    uint32_t index) {
  if (object->IsJSGlobalProxy()) {
    PrototypeIterator iter(object->GetIsolate(), object);
    if (iter.IsAtEnd()) return MaybeHandle<AccessorPair>();
    DCHECK(PrototypeIterator::GetCurrent(iter)->IsJSGlobalObject());
    return GetOwnElementAccessorPair(
        Handle<JSObject>::cast(PrototypeIterator::GetCurrent(iter)), index);
  }

  // Check for lookup interceptor.
  if (object->HasIndexedInterceptor()) return MaybeHandle<AccessorPair>();

  return object->GetElementsAccessor()->GetAccessorPair(object, object, index);
}


MaybeHandle<Object> JSObject::SetElementWithInterceptor(
    Handle<JSObject> object,
    uint32_t index,
    Handle<Object> value,
    PropertyAttributes attributes,
    StrictMode strict_mode,
    bool check_prototype,
    SetPropertyMode set_mode) {
  Isolate* isolate = object->GetIsolate();

  // Make sure that the top context does not change when doing
  // callbacks or interceptor calls.
  AssertNoContextChange ncc(isolate);

  Handle<InterceptorInfo> interceptor(object->GetIndexedInterceptor());
  if (!interceptor->setter()->IsUndefined()) {
    v8::IndexedPropertySetterCallback setter =
        v8::ToCData<v8::IndexedPropertySetterCallback>(interceptor->setter());
    LOG(isolate,
        ApiIndexedPropertyAccess("interceptor-indexed-set", *object, index));
    PropertyCallbackArguments args(isolate, interceptor->data(), *object,
                                   *object);
    v8::Handle<v8::Value> result =
        args.Call(setter, index, v8::Utils::ToLocal(value));
    RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
    if (!result.IsEmpty()) return value;
  }

  return SetElementWithoutInterceptor(object, index, value, attributes,
                                      strict_mode,
                                      check_prototype,
                                      set_mode);
}


MaybeHandle<Object> JSObject::GetElementWithCallback(
    Handle<JSObject> object,
    Handle<Object> receiver,
    Handle<Object> structure,
    uint32_t index,
    Handle<Object> holder) {
  Isolate* isolate = object->GetIsolate();
  DCHECK(!structure->IsForeign());
  // api style callbacks.
  if (structure->IsExecutableAccessorInfo()) {
    Handle<ExecutableAccessorInfo> data =
        Handle<ExecutableAccessorInfo>::cast(structure);
    Object* fun_obj = data->getter();
    v8::AccessorGetterCallback call_fun =
        v8::ToCData<v8::AccessorGetterCallback>(fun_obj);
    if (call_fun == NULL) return isolate->factory()->undefined_value();
    Handle<JSObject> holder_handle = Handle<JSObject>::cast(holder);
    Handle<Object> number = isolate->factory()->NewNumberFromUint(index);
    Handle<String> key = isolate->factory()->NumberToString(number);
    LOG(isolate, ApiNamedPropertyAccess("load", *holder_handle, *key));
    PropertyCallbackArguments
        args(isolate, data->data(), *receiver, *holder_handle);
    v8::Handle<v8::Value> result = args.Call(call_fun, v8::Utils::ToLocal(key));
    RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
    if (result.IsEmpty()) return isolate->factory()->undefined_value();
    Handle<Object> result_internal = v8::Utils::OpenHandle(*result);
    result_internal->VerifyApiCallResultType();
    // Rebox handle before return.
    return handle(*result_internal, isolate);
  }

  // __defineGetter__ callback
  if (structure->IsAccessorPair()) {
    Handle<Object> getter(Handle<AccessorPair>::cast(structure)->getter(),
                          isolate);
    if (getter->IsSpecFunction()) {
      // TODO(rossberg): nicer would be to cast to some JSCallable here...
      return GetPropertyWithDefinedGetter(
          receiver, Handle<JSReceiver>::cast(getter));
    }
    // Getter is not a function.
    return isolate->factory()->undefined_value();
  }

  if (structure->IsDeclaredAccessorInfo()) {
    return GetDeclaredAccessorProperty(
        receiver, Handle<DeclaredAccessorInfo>::cast(structure), isolate);
  }

  UNREACHABLE();
  return MaybeHandle<Object>();
}


MaybeHandle<Object> JSObject::SetElementWithCallback(Handle<JSObject> object,
                                                     Handle<Object> structure,
                                                     uint32_t index,
                                                     Handle<Object> value,
                                                     Handle<JSObject> holder,
                                                     StrictMode strict_mode) {
  Isolate* isolate = object->GetIsolate();

  // We should never get here to initialize a const with the hole
  // value since a const declaration would conflict with the setter.
  DCHECK(!value->IsTheHole());
  DCHECK(!structure->IsForeign());
  if (structure->IsExecutableAccessorInfo()) {
    // api style callbacks
    Handle<ExecutableAccessorInfo> data =
        Handle<ExecutableAccessorInfo>::cast(structure);
    Object* call_obj = data->setter();
    v8::AccessorSetterCallback call_fun =
        v8::ToCData<v8::AccessorSetterCallback>(call_obj);
    if (call_fun == NULL) return value;
    Handle<Object> number = isolate->factory()->NewNumberFromUint(index);
    Handle<String> key(isolate->factory()->NumberToString(number));
    LOG(isolate, ApiNamedPropertyAccess("store", *object, *key));
    PropertyCallbackArguments
        args(isolate, data->data(), *object, *holder);
    args.Call(call_fun,
              v8::Utils::ToLocal(key),
              v8::Utils::ToLocal(value));
    RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
    return value;
  }

  if (structure->IsAccessorPair()) {
    Handle<Object> setter(AccessorPair::cast(*structure)->setter(), isolate);
    if (setter->IsSpecFunction()) {
      // TODO(rossberg): nicer would be to cast to some JSCallable here...
      return SetPropertyWithDefinedSetter(
          object, Handle<JSReceiver>::cast(setter), value);
    } else {
      if (strict_mode == SLOPPY) return value;
      Handle<Object> key(isolate->factory()->NewNumberFromUint(index));
      Handle<Object> args[2] = { key, holder };
      Handle<Object> error = isolate->factory()->NewTypeError(
          "no_setter_in_callback", HandleVector(args, 2));
      return isolate->Throw<Object>(error);
    }
  }

  // TODO(dcarney): Handle correctly.
  if (structure->IsDeclaredAccessorInfo()) return value;

  UNREACHABLE();
  return MaybeHandle<Object>();
}


bool JSObject::HasFastArgumentsElements() {
  Heap* heap = GetHeap();
  if (!elements()->IsFixedArray()) return false;
  FixedArray* elements = FixedArray::cast(this->elements());
  if (elements->map() != heap->sloppy_arguments_elements_map()) {
    return false;
  }
  FixedArray* arguments = FixedArray::cast(elements->get(1));
  return !arguments->IsDictionary();
}


bool JSObject::HasDictionaryArgumentsElements() {
  Heap* heap = GetHeap();
  if (!elements()->IsFixedArray()) return false;
  FixedArray* elements = FixedArray::cast(this->elements());
  if (elements->map() != heap->sloppy_arguments_elements_map()) {
    return false;
  }
  FixedArray* arguments = FixedArray::cast(elements->get(1));
  return arguments->IsDictionary();
}


// Adding n elements in fast case is O(n*n).
// Note: revisit design to have dual undefined values to capture absent
// elements.
MaybeHandle<Object> JSObject::SetFastElement(Handle<JSObject> object,
                                             uint32_t index,
                                             Handle<Object> value,
                                             StrictMode strict_mode,
                                             bool check_prototype) {
  DCHECK(object->HasFastSmiOrObjectElements() ||
         object->HasFastArgumentsElements());

  Isolate* isolate = object->GetIsolate();

  // Array optimizations rely on the prototype lookups of Array objects always
  // returning undefined. If there is a store to the initial prototype object,
  // make sure all of these optimizations are invalidated.
  if (isolate->is_initial_object_prototype(*object) ||
      isolate->is_initial_array_prototype(*object)) {
    object->map()->dependent_code()->DeoptimizeDependentCodeGroup(isolate,
        DependentCode::kElementsCantBeAddedGroup);
  }

  Handle<FixedArray> backing_store(FixedArray::cast(object->elements()));
  if (backing_store->map() ==
      isolate->heap()->sloppy_arguments_elements_map()) {
    backing_store = handle(FixedArray::cast(backing_store->get(1)));
  } else {
    backing_store = EnsureWritableFastElements(object);
  }
  uint32_t capacity = static_cast<uint32_t>(backing_store->length());

  if (check_prototype &&
      (index >= capacity || backing_store->get(index)->IsTheHole())) {
    bool found;
    MaybeHandle<Object> result = SetElementWithCallbackSetterInPrototypes(
        object, index, value, &found, strict_mode);
    if (found) return result;
  }

  uint32_t new_capacity = capacity;
  // Check if the length property of this object needs to be updated.
  uint32_t array_length = 0;
  bool must_update_array_length = false;
  bool introduces_holes = true;
  if (object->IsJSArray()) {
    CHECK(Handle<JSArray>::cast(object)->length()->ToArrayIndex(&array_length));
    introduces_holes = index > array_length;
    if (index >= array_length) {
      must_update_array_length = true;
      array_length = index + 1;
    }
  } else {
    introduces_holes = index >= capacity;
  }

  // If the array is growing, and it's not growth by a single element at the
  // end, make sure that the ElementsKind is HOLEY.
  ElementsKind elements_kind = object->GetElementsKind();
  if (introduces_holes &&
      IsFastElementsKind(elements_kind) &&
      !IsFastHoleyElementsKind(elements_kind)) {
    ElementsKind transitioned_kind = GetHoleyElementsKind(elements_kind);
    TransitionElementsKind(object, transitioned_kind);
  }

  // Check if the capacity of the backing store needs to be increased, or if
  // a transition to slow elements is necessary.
  if (index >= capacity) {
    bool convert_to_slow = true;
    if ((index - capacity) < kMaxGap) {
      new_capacity = NewElementsCapacity(index + 1);
      DCHECK(new_capacity > index);
      if (!object->ShouldConvertToSlowElements(new_capacity)) {
        convert_to_slow = false;
      }
    }
    if (convert_to_slow) {
      NormalizeElements(object);
      return SetDictionaryElement(object, index, value, NONE, strict_mode,
                                  check_prototype);
    }
  }
  // Convert to fast double elements if appropriate.
  if (object->HasFastSmiElements() && !value->IsSmi() && value->IsNumber()) {
    // Consider fixing the boilerplate as well if we have one.
    ElementsKind to_kind = IsHoleyElementsKind(elements_kind)
        ? FAST_HOLEY_DOUBLE_ELEMENTS
        : FAST_DOUBLE_ELEMENTS;

    UpdateAllocationSite(object, to_kind);

    SetFastDoubleElementsCapacityAndLength(object, new_capacity, array_length);
    FixedDoubleArray::cast(object->elements())->set(index, value->Number());
    JSObject::ValidateElements(object);
    return value;
  }
  // Change elements kind from Smi-only to generic FAST if necessary.
  if (object->HasFastSmiElements() && !value->IsSmi()) {
    ElementsKind kind = object->HasFastHoleyElements()
        ? FAST_HOLEY_ELEMENTS
        : FAST_ELEMENTS;

    UpdateAllocationSite(object, kind);
    Handle<Map> new_map = GetElementsTransitionMap(object, kind);
    JSObject::MigrateToMap(object, new_map);
    DCHECK(IsFastObjectElementsKind(object->GetElementsKind()));
  }
  // Increase backing store capacity if that's been decided previously.
  if (new_capacity != capacity) {
    SetFastElementsCapacitySmiMode smi_mode =
        value->IsSmi() && object->HasFastSmiElements()
            ? kAllowSmiElements
            : kDontAllowSmiElements;
    Handle<FixedArray> new_elements =
        SetFastElementsCapacityAndLength(object, new_capacity, array_length,
                                         smi_mode);
    new_elements->set(index, *value);
    JSObject::ValidateElements(object);
    return value;
  }

  // Finally, set the new element and length.
  DCHECK(object->elements()->IsFixedArray());
  backing_store->set(index, *value);
  if (must_update_array_length) {
    Handle<JSArray>::cast(object)->set_length(Smi::FromInt(array_length));
  }
  return value;
}


MaybeHandle<Object> JSObject::SetDictionaryElement(
    Handle<JSObject> object,
    uint32_t index,
    Handle<Object> value,
    PropertyAttributes attributes,
    StrictMode strict_mode,
    bool check_prototype,
    SetPropertyMode set_mode) {
  DCHECK(object->HasDictionaryElements() ||
         object->HasDictionaryArgumentsElements());
  Isolate* isolate = object->GetIsolate();

  // Insert element in the dictionary.
  Handle<FixedArray> elements(FixedArray::cast(object->elements()));
  bool is_arguments =
      (elements->map() == isolate->heap()->sloppy_arguments_elements_map());
  Handle<SeededNumberDictionary> dictionary(is_arguments
    ? SeededNumberDictionary::cast(elements->get(1))
    : SeededNumberDictionary::cast(*elements));

  int entry = dictionary->FindEntry(index);
  if (entry != SeededNumberDictionary::kNotFound) {
    Handle<Object> element(dictionary->ValueAt(entry), isolate);
    PropertyDetails details = dictionary->DetailsAt(entry);
    if (details.type() == CALLBACKS && set_mode == SET_PROPERTY) {
      return SetElementWithCallback(object, element, index, value, object,
                                    strict_mode);
    } else {
      dictionary->UpdateMaxNumberKey(index);
      // If a value has not been initialized we allow writing to it even if it
      // is read-only (a declared const that has not been initialized).  If a
      // value is being defined we skip attribute checks completely.
      if (set_mode == DEFINE_PROPERTY) {
        details = PropertyDetails(
            attributes, NORMAL, details.dictionary_index());
        dictionary->DetailsAtPut(entry, details);
      } else if (details.IsReadOnly() && !element->IsTheHole()) {
        if (strict_mode == SLOPPY) {
          return isolate->factory()->undefined_value();
        } else {
          Handle<Object> number = isolate->factory()->NewNumberFromUint(index);
          Handle<Object> args[2] = { number, object };
          Handle<Object> error =
              isolate->factory()->NewTypeError("strict_read_only_property",
                                               HandleVector(args, 2));
          return isolate->Throw<Object>(error);
        }
      }
      // Elements of the arguments object in slow mode might be slow aliases.
      if (is_arguments && element->IsAliasedArgumentsEntry()) {
        Handle<AliasedArgumentsEntry> entry =
            Handle<AliasedArgumentsEntry>::cast(element);
        Handle<Context> context(Context::cast(elements->get(0)));
        int context_index = entry->aliased_context_slot();
        DCHECK(!context->get(context_index)->IsTheHole());
        context->set(context_index, *value);
        // For elements that are still writable we keep slow aliasing.
        if (!details.IsReadOnly()) value = element;
      }
      dictionary->ValueAtPut(entry, *value);
    }
  } else {
    // Index not already used. Look for an accessor in the prototype chain.
    // Can cause GC!
    if (check_prototype) {
      bool found;
      MaybeHandle<Object> result = SetElementWithCallbackSetterInPrototypes(
          object, index, value, &found, strict_mode);
      if (found) return result;
    }

    // When we set the is_extensible flag to false we always force the
    // element into dictionary mode (and force them to stay there).
    if (!object->map()->is_extensible()) {
      if (strict_mode == SLOPPY) {
        return isolate->factory()->undefined_value();
      } else {
        Handle<Object> number = isolate->factory()->NewNumberFromUint(index);
        Handle<String> name = isolate->factory()->NumberToString(number);
        Handle<Object> args[1] = { name };
        Handle<Object> error =
            isolate->factory()->NewTypeError("object_not_extensible",
                                             HandleVector(args, 1));
        return isolate->Throw<Object>(error);
      }
    }

    PropertyDetails details = PropertyDetails(attributes, NORMAL, 0);
    Handle<SeededNumberDictionary> new_dictionary =
        SeededNumberDictionary::AddNumberEntry(dictionary, index, value,
                                               details);
    if (*dictionary != *new_dictionary) {
      if (is_arguments) {
        elements->set(1, *new_dictionary);
      } else {
        object->set_elements(*new_dictionary);
      }
      dictionary = new_dictionary;
    }
  }

  // Update the array length if this JSObject is an array.
  if (object->IsJSArray()) {
    JSArray::JSArrayUpdateLengthFromIndex(Handle<JSArray>::cast(object), index,
                                          value);
  }

  // Attempt to put this object back in fast case.
  if (object->ShouldConvertToFastElements()) {
    uint32_t new_length = 0;
    if (object->IsJSArray()) {
      CHECK(Handle<JSArray>::cast(object)->length()->ToArrayIndex(&new_length));
    } else {
      new_length = dictionary->max_number_key() + 1;
    }
    bool has_smi_only_elements = false;
    bool should_convert_to_fast_double_elements =
        object->ShouldConvertToFastDoubleElements(&has_smi_only_elements);
    SetFastElementsCapacitySmiMode smi_mode =
        has_smi_only_elements ? kForceSmiElements : kAllowSmiElements;

    if (should_convert_to_fast_double_elements) {
      SetFastDoubleElementsCapacityAndLength(object, new_length, new_length);
    } else {
      SetFastElementsCapacityAndLength(object, new_length, new_length,
                                       smi_mode);
    }
    JSObject::ValidateElements(object);
#ifdef DEBUG
    if (FLAG_trace_normalization) {
      OFStream os(stdout);
      os << "Object elements are fast case again:\n";
      object->Print(os);
    }
#endif
  }
  return value;
}

MaybeHandle<Object> JSObject::SetFastDoubleElement(
    Handle<JSObject> object,
    uint32_t index,
    Handle<Object> value,
    StrictMode strict_mode,
    bool check_prototype) {
  DCHECK(object->HasFastDoubleElements());

  Handle<FixedArrayBase> base_elms(FixedArrayBase::cast(object->elements()));
  uint32_t elms_length = static_cast<uint32_t>(base_elms->length());

  // If storing to an element that isn't in the array, pass the store request
  // up the prototype chain before storing in the receiver's elements.
  if (check_prototype &&
      (index >= elms_length ||
       Handle<FixedDoubleArray>::cast(base_elms)->is_the_hole(index))) {
    bool found;
    MaybeHandle<Object> result = SetElementWithCallbackSetterInPrototypes(
        object, index, value, &found, strict_mode);
    if (found) return result;
  }

  // If the value object is not a heap number, switch to fast elements and try
  // again.
  bool value_is_smi = value->IsSmi();
  bool introduces_holes = true;
  uint32_t length = elms_length;
  if (object->IsJSArray()) {
    CHECK(Handle<JSArray>::cast(object)->length()->ToArrayIndex(&length));
    introduces_holes = index > length;
  } else {
    introduces_holes = index >= elms_length;
  }

  if (!value->IsNumber()) {
    SetFastElementsCapacityAndLength(object, elms_length, length,
                                     kDontAllowSmiElements);
    Handle<Object> result;
    ASSIGN_RETURN_ON_EXCEPTION(
        object->GetIsolate(), result,
        SetFastElement(object, index, value, strict_mode, check_prototype),
        Object);
    JSObject::ValidateElements(object);
    return result;
  }

  double double_value = value_is_smi
      ? static_cast<double>(Handle<Smi>::cast(value)->value())
      : Handle<HeapNumber>::cast(value)->value();

  // If the array is growing, and it's not growth by a single element at the
  // end, make sure that the ElementsKind is HOLEY.
  ElementsKind elements_kind = object->GetElementsKind();
  if (introduces_holes && !IsFastHoleyElementsKind(elements_kind)) {
    ElementsKind transitioned_kind = GetHoleyElementsKind(elements_kind);
    TransitionElementsKind(object, transitioned_kind);
  }

  // Check whether there is extra space in the fixed array.
  if (index < elms_length) {
    Handle<FixedDoubleArray> elms(FixedDoubleArray::cast(object->elements()));
    elms->set(index, double_value);
    if (object->IsJSArray()) {
      // Update the length of the array if needed.
      uint32_t array_length = 0;
      CHECK(
          Handle<JSArray>::cast(object)->length()->ToArrayIndex(&array_length));
      if (index >= array_length) {
        Handle<JSArray>::cast(object)->set_length(Smi::FromInt(index + 1));
      }
    }
    return value;
  }

  // Allow gap in fast case.
  if ((index - elms_length) < kMaxGap) {
    // Try allocating extra space.
    int new_capacity = NewElementsCapacity(index+1);
    if (!object->ShouldConvertToSlowElements(new_capacity)) {
      DCHECK(static_cast<uint32_t>(new_capacity) > index);
      SetFastDoubleElementsCapacityAndLength(object, new_capacity, index + 1);
      FixedDoubleArray::cast(object->elements())->set(index, double_value);
      JSObject::ValidateElements(object);
      return value;
    }
  }

  // Otherwise default to slow case.
  DCHECK(object->HasFastDoubleElements());
  DCHECK(object->map()->has_fast_double_elements());
  DCHECK(object->elements()->IsFixedDoubleArray() ||
         object->elements()->length() == 0);

  NormalizeElements(object);
  DCHECK(object->HasDictionaryElements());
  return SetElement(object, index, value, NONE, strict_mode, check_prototype);
}


MaybeHandle<Object> JSReceiver::SetElement(Handle<JSReceiver> object,
                                           uint32_t index,
                                           Handle<Object> value,
                                           PropertyAttributes attributes,
                                           StrictMode strict_mode) {
  if (object->IsJSProxy()) {
    return JSProxy::SetElementWithHandler(
        Handle<JSProxy>::cast(object), object, index, value, strict_mode);
  }
  return JSObject::SetElement(
      Handle<JSObject>::cast(object), index, value, attributes, strict_mode);
}


MaybeHandle<Object> JSObject::SetOwnElement(Handle<JSObject> object,
                                            uint32_t index,
                                            Handle<Object> value,
                                            StrictMode strict_mode) {
  DCHECK(!object->HasExternalArrayElements());
  return JSObject::SetElement(object, index, value, NONE, strict_mode, false);
}


MaybeHandle<Object> JSObject::SetElement(Handle<JSObject> object,
                                         uint32_t index,
                                         Handle<Object> value,
                                         PropertyAttributes attributes,
                                         StrictMode strict_mode,
                                         bool check_prototype,
                                         SetPropertyMode set_mode) {
  Isolate* isolate = object->GetIsolate();

  if (object->HasExternalArrayElements() ||
      object->HasFixedTypedArrayElements()) {
    if (!value->IsNumber() && !value->IsUndefined()) {
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, value,
          Execution::ToNumber(isolate, value), Object);
    }
  }

  // Check access rights if needed.
  if (object->IsAccessCheckNeeded()) {
    if (!isolate->MayIndexedAccess(object, index, v8::ACCESS_SET)) {
      isolate->ReportFailedAccessCheck(object, v8::ACCESS_SET);
      RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
      return value;
    }
  }

  if (object->IsJSGlobalProxy()) {
    PrototypeIterator iter(isolate, object);
    if (iter.IsAtEnd()) return value;
    DCHECK(PrototypeIterator::GetCurrent(iter)->IsJSGlobalObject());
    return SetElement(
        Handle<JSObject>::cast(PrototypeIterator::GetCurrent(iter)), index,
        value, attributes, strict_mode, check_prototype, set_mode);
  }

  // Don't allow element properties to be redefined for external arrays.
  if ((object->HasExternalArrayElements() ||
          object->HasFixedTypedArrayElements()) &&
      set_mode == DEFINE_PROPERTY) {
    Handle<Object> number = isolate->factory()->NewNumberFromUint(index);
    Handle<Object> args[] = { object, number };
    Handle<Object> error = isolate->factory()->NewTypeError(
        "redef_external_array_element", HandleVector(args, ARRAY_SIZE(args)));
    return isolate->Throw<Object>(error);
  }

  // Normalize the elements to enable attributes on the property.
  if ((attributes & (DONT_DELETE | DONT_ENUM | READ_ONLY)) != 0) {
    Handle<SeededNumberDictionary> dictionary = NormalizeElements(object);
    // Make sure that we never go back to fast case.
    dictionary->set_requires_slow_elements();
  }

  if (!object->map()->is_observed()) {
    return object->HasIndexedInterceptor()
      ? SetElementWithInterceptor(object, index, value, attributes,
                                  strict_mode, check_prototype, set_mode)
      : SetElementWithoutInterceptor(object, index, value, attributes,
                                     strict_mode, check_prototype, set_mode);
  }

  Maybe<PropertyAttributes> maybe =
      JSReceiver::GetOwnElementAttribute(object, index);
  if (!maybe.has_value) return MaybeHandle<Object>();
  PropertyAttributes old_attributes = maybe.value;

  Handle<Object> old_value = isolate->factory()->the_hole_value();
  Handle<Object> old_length_handle;
  Handle<Object> new_length_handle;

  if (old_attributes != ABSENT) {
    if (GetOwnElementAccessorPair(object, index).is_null()) {
      old_value = Object::GetElement(isolate, object, index).ToHandleChecked();
    }
  } else if (object->IsJSArray()) {
    // Store old array length in case adding an element grows the array.
    old_length_handle = handle(Handle<JSArray>::cast(object)->length(),
                               isolate);
  }

  // Check for lookup interceptor
  Handle<Object> result;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result,
      object->HasIndexedInterceptor()
          ? SetElementWithInterceptor(
              object, index, value, attributes,
              strict_mode, check_prototype, set_mode)
          : SetElementWithoutInterceptor(
              object, index, value, attributes,
              strict_mode, check_prototype, set_mode),
      Object);

  Handle<String> name = isolate->factory()->Uint32ToString(index);
  maybe = GetOwnElementAttribute(object, index);
  if (!maybe.has_value) return MaybeHandle<Object>();
  PropertyAttributes new_attributes = maybe.value;

  if (old_attributes == ABSENT) {
    if (object->IsJSArray() &&
        !old_length_handle->SameValue(
            Handle<JSArray>::cast(object)->length())) {
      new_length_handle = handle(Handle<JSArray>::cast(object)->length(),
                                 isolate);
      uint32_t old_length = 0;
      uint32_t new_length = 0;
      CHECK(old_length_handle->ToArrayIndex(&old_length));
      CHECK(new_length_handle->ToArrayIndex(&new_length));

      BeginPerformSplice(Handle<JSArray>::cast(object));
      EnqueueChangeRecord(object, "add", name, old_value);
      EnqueueChangeRecord(object, "update", isolate->factory()->length_string(),
                          old_length_handle);
      EndPerformSplice(Handle<JSArray>::cast(object));
      Handle<JSArray> deleted = isolate->factory()->NewJSArray(0);
      EnqueueSpliceRecord(Handle<JSArray>::cast(object), old_length, deleted,
                          new_length - old_length);
    } else {
      EnqueueChangeRecord(object, "add", name, old_value);
    }
  } else if (old_value->IsTheHole()) {
    EnqueueChangeRecord(object, "reconfigure", name, old_value);
  } else {
    Handle<Object> new_value =
        Object::GetElement(isolate, object, index).ToHandleChecked();
    bool value_changed = !old_value->SameValue(*new_value);
    if (old_attributes != new_attributes) {
      if (!value_changed) old_value = isolate->factory()->the_hole_value();
      EnqueueChangeRecord(object, "reconfigure", name, old_value);
    } else if (value_changed) {
      EnqueueChangeRecord(object, "update", name, old_value);
    }
  }

  return result;
}


MaybeHandle<Object> JSObject::SetElementWithoutInterceptor(
    Handle<JSObject> object,
    uint32_t index,
    Handle<Object> value,
    PropertyAttributes attributes,
    StrictMode strict_mode,
    bool check_prototype,
    SetPropertyMode set_mode) {
  DCHECK(object->HasDictionaryElements() ||
         object->HasDictionaryArgumentsElements() ||
         (attributes & (DONT_DELETE | DONT_ENUM | READ_ONLY)) == 0);
  Isolate* isolate = object->GetIsolate();
  if (FLAG_trace_external_array_abuse &&
      IsExternalArrayElementsKind(object->GetElementsKind())) {
    CheckArrayAbuse(object, "external elements write", index);
  }
  if (FLAG_trace_js_array_abuse &&
      !IsExternalArrayElementsKind(object->GetElementsKind())) {
    if (object->IsJSArray()) {
      CheckArrayAbuse(object, "elements write", index, true);
    }
  }
  if (object->IsJSArray() && JSArray::WouldChangeReadOnlyLength(
      Handle<JSArray>::cast(object), index)) {
    if (strict_mode == SLOPPY) {
      return value;
    } else {
      return JSArray::ReadOnlyLengthError(Handle<JSArray>::cast(object));
    }
  }
  switch (object->GetElementsKind()) {
    case FAST_SMI_ELEMENTS:
    case FAST_ELEMENTS:
    case FAST_HOLEY_SMI_ELEMENTS:
    case FAST_HOLEY_ELEMENTS:
      return SetFastElement(object, index, value, strict_mode, check_prototype);
    case FAST_DOUBLE_ELEMENTS:
    case FAST_HOLEY_DOUBLE_ELEMENTS:
      return SetFastDoubleElement(object, index, value, strict_mode,
                                  check_prototype);

#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size)                       \
    case EXTERNAL_##TYPE##_ELEMENTS: {                                        \
      Handle<External##Type##Array> array(                                    \
          External##Type##Array::cast(object->elements()));                   \
      return External##Type##Array::SetValue(array, index, value);            \
    }                                                                         \
    case TYPE##_ELEMENTS: {                                                   \
      Handle<Fixed##Type##Array> array(                                       \
          Fixed##Type##Array::cast(object->elements()));                      \
      return Fixed##Type##Array::SetValue(array, index, value);               \
    }

    TYPED_ARRAYS(TYPED_ARRAY_CASE)

#undef TYPED_ARRAY_CASE

    case DICTIONARY_ELEMENTS:
      return SetDictionaryElement(object, index, value, attributes, strict_mode,
                                  check_prototype,
                                  set_mode);
    case SLOPPY_ARGUMENTS_ELEMENTS: {
      Handle<FixedArray> parameter_map(FixedArray::cast(object->elements()));
      uint32_t length = parameter_map->length();
      Handle<Object> probe = index < length - 2 ?
          Handle<Object>(parameter_map->get(index + 2), isolate) :
          Handle<Object>();
      if (!probe.is_null() && !probe->IsTheHole()) {
        Handle<Context> context(Context::cast(parameter_map->get(0)));
        int context_index = Handle<Smi>::cast(probe)->value();
        DCHECK(!context->get(context_index)->IsTheHole());
        context->set(context_index, *value);
        // Redefining attributes of an aliased element destroys fast aliasing.
        if (set_mode == SET_PROPERTY || attributes == NONE) return value;
        parameter_map->set_the_hole(index + 2);
        // For elements that are still writable we re-establish slow aliasing.
        if ((attributes & READ_ONLY) == 0) {
          value = Handle<Object>::cast(
              isolate->factory()->NewAliasedArgumentsEntry(context_index));
        }
      }
      Handle<FixedArray> arguments(FixedArray::cast(parameter_map->get(1)));
      if (arguments->IsDictionary()) {
        return SetDictionaryElement(object, index, value, attributes,
                                    strict_mode,
                                    check_prototype,
                                    set_mode);
      } else {
        return SetFastElement(object, index, value, strict_mode,
                              check_prototype);
      }
    }
  }
  // All possible cases have been handled above. Add a return to avoid the
  // complaints from the compiler.
  UNREACHABLE();
  return isolate->factory()->null_value();
}


const double AllocationSite::kPretenureRatio = 0.85;


void AllocationSite::ResetPretenureDecision() {
  set_pretenure_decision(kUndecided);
  set_memento_found_count(0);
  set_memento_create_count(0);
}


PretenureFlag AllocationSite::GetPretenureMode() {
  PretenureDecision mode = pretenure_decision();
  // Zombie objects "decide" to be untenured.
  return mode == kTenure ? TENURED : NOT_TENURED;
}


bool AllocationSite::IsNestedSite() {
  DCHECK(FLAG_trace_track_allocation_sites);
  Object* current = GetHeap()->allocation_sites_list();
  while (current->IsAllocationSite()) {
    AllocationSite* current_site = AllocationSite::cast(current);
    if (current_site->nested_site() == this) {
      return true;
    }
    current = current_site->weak_next();
  }
  return false;
}


void AllocationSite::DigestTransitionFeedback(Handle<AllocationSite> site,
                                              ElementsKind to_kind) {
  Isolate* isolate = site->GetIsolate();

  if (site->SitePointsToLiteral() && site->transition_info()->IsJSArray()) {
    Handle<JSArray> transition_info =
        handle(JSArray::cast(site->transition_info()));
    ElementsKind kind = transition_info->GetElementsKind();
    // if kind is holey ensure that to_kind is as well.
    if (IsHoleyElementsKind(kind)) {
      to_kind = GetHoleyElementsKind(to_kind);
    }
    if (IsMoreGeneralElementsKindTransition(kind, to_kind)) {
      // If the array is huge, it's not likely to be defined in a local
      // function, so we shouldn't make new instances of it very often.
      uint32_t length = 0;
      CHECK(transition_info->length()->ToArrayIndex(&length));
      if (length <= kMaximumArrayBytesToPretransition) {
        if (FLAG_trace_track_allocation_sites) {
          bool is_nested = site->IsNestedSite();
          PrintF(
              "AllocationSite: JSArray %p boilerplate %s updated %s->%s\n",
              reinterpret_cast<void*>(*site),
              is_nested ? "(nested)" : "",
              ElementsKindToString(kind),
              ElementsKindToString(to_kind));
        }
        JSObject::TransitionElementsKind(transition_info, to_kind);
        site->dependent_code()->DeoptimizeDependentCodeGroup(
            isolate, DependentCode::kAllocationSiteTransitionChangedGroup);
      }
    }
  } else {
    ElementsKind kind = site->GetElementsKind();
    // if kind is holey ensure that to_kind is as well.
    if (IsHoleyElementsKind(kind)) {
      to_kind = GetHoleyElementsKind(to_kind);
    }
    if (IsMoreGeneralElementsKindTransition(kind, to_kind)) {
      if (FLAG_trace_track_allocation_sites) {
        PrintF("AllocationSite: JSArray %p site updated %s->%s\n",
               reinterpret_cast<void*>(*site),
               ElementsKindToString(kind),
               ElementsKindToString(to_kind));
      }
      site->SetElementsKind(to_kind);
      site->dependent_code()->DeoptimizeDependentCodeGroup(
          isolate, DependentCode::kAllocationSiteTransitionChangedGroup);
    }
  }
}


// static
void AllocationSite::AddDependentCompilationInfo(Handle<AllocationSite> site,
                                                 Reason reason,
                                                 CompilationInfo* info) {
  DependentCode::DependencyGroup group = site->ToDependencyGroup(reason);
  Handle<DependentCode> dep(site->dependent_code());
  Handle<DependentCode> codes =
      DependentCode::Insert(dep, group, info->object_wrapper());
  if (*codes != site->dependent_code()) site->set_dependent_code(*codes);
  info->dependencies(group)->Add(Handle<HeapObject>(*site), info->zone());
}


const char* AllocationSite::PretenureDecisionName(PretenureDecision decision) {
  switch (decision) {
    case kUndecided: return "undecided";
    case kDontTenure: return "don't tenure";
    case kMaybeTenure: return "maybe tenure";
    case kTenure: return "tenure";
    case kZombie: return "zombie";
    default: UNREACHABLE();
  }
  return NULL;
}


void JSObject::UpdateAllocationSite(Handle<JSObject> object,
                                    ElementsKind to_kind) {
  if (!object->IsJSArray()) return;

  Heap* heap = object->GetHeap();
  if (!heap->InNewSpace(*object)) return;

  Handle<AllocationSite> site;
  {
    DisallowHeapAllocation no_allocation;

    AllocationMemento* memento = heap->FindAllocationMemento(*object);
    if (memento == NULL) return;

    // Walk through to the Allocation Site
    site = handle(memento->GetAllocationSite());
  }
  AllocationSite::DigestTransitionFeedback(site, to_kind);
}


void JSObject::TransitionElementsKind(Handle<JSObject> object,
                                      ElementsKind to_kind) {
  ElementsKind from_kind = object->map()->elements_kind();

  if (IsFastHoleyElementsKind(from_kind)) {
    to_kind = GetHoleyElementsKind(to_kind);
  }

  if (from_kind == to_kind) return;
  // Don't update the site if to_kind isn't fast
  if (IsFastElementsKind(to_kind)) {
    UpdateAllocationSite(object, to_kind);
  }

  Isolate* isolate = object->GetIsolate();
  if (object->elements() == isolate->heap()->empty_fixed_array() ||
      (IsFastSmiOrObjectElementsKind(from_kind) &&
       IsFastSmiOrObjectElementsKind(to_kind)) ||
      (from_kind == FAST_DOUBLE_ELEMENTS &&
       to_kind == FAST_HOLEY_DOUBLE_ELEMENTS)) {
    DCHECK(from_kind != TERMINAL_FAST_ELEMENTS_KIND);
    // No change is needed to the elements() buffer, the transition
    // only requires a map change.
    Handle<Map> new_map = GetElementsTransitionMap(object, to_kind);
    MigrateToMap(object, new_map);
    if (FLAG_trace_elements_transitions) {
      Handle<FixedArrayBase> elms(object->elements());
      PrintElementsTransition(stdout, object, from_kind, elms, to_kind, elms);
    }
    return;
  }

  Handle<FixedArrayBase> elms(object->elements());
  uint32_t capacity = static_cast<uint32_t>(elms->length());
  uint32_t length = capacity;

  if (object->IsJSArray()) {
    Object* raw_length = Handle<JSArray>::cast(object)->length();
    if (raw_length->IsUndefined()) {
      // If length is undefined, then JSArray is being initialized and has no
      // elements, assume a length of zero.
      length = 0;
    } else {
      CHECK(raw_length->ToArrayIndex(&length));
    }
  }

  if (IsFastSmiElementsKind(from_kind) &&
      IsFastDoubleElementsKind(to_kind)) {
    SetFastDoubleElementsCapacityAndLength(object, capacity, length);
    JSObject::ValidateElements(object);
    return;
  }

  if (IsFastDoubleElementsKind(from_kind) &&
      IsFastObjectElementsKind(to_kind)) {
    SetFastElementsCapacityAndLength(object, capacity, length,
                                     kDontAllowSmiElements);
    JSObject::ValidateElements(object);
    return;
  }

  // This method should never be called for any other case than the ones
  // handled above.
  UNREACHABLE();
}


// static
bool Map::IsValidElementsTransition(ElementsKind from_kind,
                                    ElementsKind to_kind) {
  // Transitions can't go backwards.
  if (!IsMoreGeneralElementsKindTransition(from_kind, to_kind)) {
    return false;
  }

  // Transitions from HOLEY -> PACKED are not allowed.
  return !IsFastHoleyElementsKind(from_kind) ||
      IsFastHoleyElementsKind(to_kind);
}


void JSArray::JSArrayUpdateLengthFromIndex(Handle<JSArray> array,
                                           uint32_t index,
                                           Handle<Object> value) {
  uint32_t old_len = 0;
  CHECK(array->length()->ToArrayIndex(&old_len));
  // Check to see if we need to update the length. For now, we make
  // sure that the length stays within 32-bits (unsigned).
  if (index >= old_len && index != 0xffffffff) {
    Handle<Object> len = array->GetIsolate()->factory()->NewNumber(
        static_cast<double>(index) + 1);
    array->set_length(*len);
  }
}


bool JSArray::IsReadOnlyLengthDescriptor(Handle<Map> jsarray_map) {
    Isolate* isolate = jsarray_map->GetIsolate();
    DCHECK(!jsarray_map->is_dictionary_map());
    LookupResult lookup(isolate);
    Handle<Name> length_string = isolate->factory()->length_string();
    jsarray_map->LookupDescriptor(NULL, *length_string, &lookup);
    return lookup.IsReadOnly();
}


bool JSArray::WouldChangeReadOnlyLength(Handle<JSArray> array,
                                        uint32_t index) {
  uint32_t length = 0;
  CHECK(array->length()->ToArrayIndex(&length));
  if (length <= index) {
    Isolate* isolate = array->GetIsolate();
    LookupResult lookup(isolate);
    Handle<Name> length_string = isolate->factory()->length_string();
    array->LookupOwnRealNamedProperty(length_string, &lookup);
    return lookup.IsReadOnly();
  }
  return false;
}


MaybeHandle<Object> JSArray::ReadOnlyLengthError(Handle<JSArray> array) {
  Isolate* isolate = array->GetIsolate();
  Handle<Name> length = isolate->factory()->length_string();
  Handle<Object> args[2] = { length, array };
  Handle<Object> error = isolate->factory()->NewTypeError(
      "strict_read_only_property", HandleVector(args, ARRAY_SIZE(args)));
  return isolate->Throw<Object>(error);
}


MaybeHandle<Object> JSObject::GetElementWithInterceptor(
    Handle<JSObject> object,
    Handle<Object> receiver,
    uint32_t index) {
  Isolate* isolate = object->GetIsolate();

  // Make sure that the top context does not change when doing
  // callbacks or interceptor calls.
  AssertNoContextChange ncc(isolate);

  Handle<InterceptorInfo> interceptor(object->GetIndexedInterceptor(), isolate);
  if (!interceptor->getter()->IsUndefined()) {
    v8::IndexedPropertyGetterCallback getter =
        v8::ToCData<v8::IndexedPropertyGetterCallback>(interceptor->getter());
    LOG(isolate,
        ApiIndexedPropertyAccess("interceptor-indexed-get", *object, index));
    PropertyCallbackArguments
        args(isolate, interceptor->data(), *receiver, *object);
    v8::Handle<v8::Value> result = args.Call(getter, index);
    RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
    if (!result.IsEmpty()) {
      Handle<Object> result_internal = v8::Utils::OpenHandle(*result);
      result_internal->VerifyApiCallResultType();
      // Rebox handle before return.
      return handle(*result_internal, isolate);
    }
  }

  ElementsAccessor* handler = object->GetElementsAccessor();
  Handle<Object> result;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result, handler->Get(receiver,  object, index),
      Object);
  if (!result->IsTheHole()) return result;

  PrototypeIterator iter(isolate, object);
  if (iter.IsAtEnd()) return isolate->factory()->undefined_value();
  return Object::GetElementWithReceiver(
      isolate, PrototypeIterator::GetCurrent(iter), receiver, index);
}


bool JSObject::HasDenseElements() {
  int capacity = 0;
  int used = 0;
  GetElementsCapacityAndUsage(&capacity, &used);
  return (capacity == 0) || (used > (capacity / 2));
}


void JSObject::GetElementsCapacityAndUsage(int* capacity, int* used) {
  *capacity = 0;
  *used = 0;

  FixedArrayBase* backing_store_base = FixedArrayBase::cast(elements());
  FixedArray* backing_store = NULL;
  switch (GetElementsKind()) {
    case SLOPPY_ARGUMENTS_ELEMENTS:
      backing_store_base =
          FixedArray::cast(FixedArray::cast(backing_store_base)->get(1));
      backing_store = FixedArray::cast(backing_store_base);
      if (backing_store->IsDictionary()) {
        SeededNumberDictionary* dictionary =
            SeededNumberDictionary::cast(backing_store);
        *capacity = dictionary->Capacity();
        *used = dictionary->NumberOfElements();
        break;
      }
      // Fall through.
    case FAST_SMI_ELEMENTS:
    case FAST_ELEMENTS:
      if (IsJSArray()) {
        *capacity = backing_store_base->length();
        *used = Smi::cast(JSArray::cast(this)->length())->value();
        break;
      }
      // Fall through if packing is not guaranteed.
    case FAST_HOLEY_SMI_ELEMENTS:
    case FAST_HOLEY_ELEMENTS:
      backing_store = FixedArray::cast(backing_store_base);
      *capacity = backing_store->length();
      for (int i = 0; i < *capacity; ++i) {
        if (!backing_store->get(i)->IsTheHole()) ++(*used);
      }
      break;
    case DICTIONARY_ELEMENTS: {
      SeededNumberDictionary* dictionary = element_dictionary();
      *capacity = dictionary->Capacity();
      *used = dictionary->NumberOfElements();
      break;
    }
    case FAST_DOUBLE_ELEMENTS:
      if (IsJSArray()) {
        *capacity = backing_store_base->length();
        *used = Smi::cast(JSArray::cast(this)->length())->value();
        break;
      }
      // Fall through if packing is not guaranteed.
    case FAST_HOLEY_DOUBLE_ELEMENTS: {
      *capacity = elements()->length();
      if (*capacity == 0) break;
      FixedDoubleArray * elms = FixedDoubleArray::cast(elements());
      for (int i = 0; i < *capacity; i++) {
        if (!elms->is_the_hole(i)) ++(*used);
      }
      break;
    }

#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size)                      \
    case EXTERNAL_##TYPE##_ELEMENTS:                                         \
    case TYPE##_ELEMENTS:                                                    \

    TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
    {
      // External arrays are considered 100% used.
      FixedArrayBase* external_array = FixedArrayBase::cast(elements());
      *capacity = external_array->length();
      *used = external_array->length();
      break;
    }
  }
}


bool JSObject::WouldConvertToSlowElements(Handle<Object> key) {
  uint32_t index;
  if (HasFastElements() && key->ToArrayIndex(&index)) {
    Handle<FixedArrayBase> backing_store(FixedArrayBase::cast(elements()));
    uint32_t capacity = static_cast<uint32_t>(backing_store->length());
    if (index >= capacity) {
      if ((index - capacity) >= kMaxGap) return true;
      uint32_t new_capacity = NewElementsCapacity(index + 1);
      return ShouldConvertToSlowElements(new_capacity);
    }
  }
  return false;
}


bool JSObject::ShouldConvertToSlowElements(int new_capacity) {
  STATIC_ASSERT(kMaxUncheckedOldFastElementsLength <=
                kMaxUncheckedFastElementsLength);
  if (new_capacity <= kMaxUncheckedOldFastElementsLength ||
      (new_capacity <= kMaxUncheckedFastElementsLength &&
       GetHeap()->InNewSpace(this))) {
    return false;
  }
  // If the fast-case backing storage takes up roughly three times as
  // much space (in machine words) as a dictionary backing storage
  // would, the object should have slow elements.
  int old_capacity = 0;
  int used_elements = 0;
  GetElementsCapacityAndUsage(&old_capacity, &used_elements);
  int dictionary_size = SeededNumberDictionary::ComputeCapacity(used_elements) *
      SeededNumberDictionary::kEntrySize;
  return 3 * dictionary_size <= new_capacity;
}


bool JSObject::ShouldConvertToFastElements() {
  DCHECK(HasDictionaryElements() || HasDictionaryArgumentsElements());
  // If the elements are sparse, we should not go back to fast case.
  if (!HasDenseElements()) return false;
  // An object requiring access checks is never allowed to have fast
  // elements.  If it had fast elements we would skip security checks.
  if (IsAccessCheckNeeded()) return false;
  // Observed objects may not go to fast mode because they rely on map checks,
  // and for fast element accesses we sometimes check element kinds only.
  if (map()->is_observed()) return false;

  FixedArray* elements = FixedArray::cast(this->elements());
  SeededNumberDictionary* dictionary = NULL;
  if (elements->map() == GetHeap()->sloppy_arguments_elements_map()) {
    dictionary = SeededNumberDictionary::cast(elements->get(1));
  } else {
    dictionary = SeededNumberDictionary::cast(elements);
  }
  // If an element has been added at a very high index in the elements
  // dictionary, we cannot go back to fast case.
  if (dictionary->requires_slow_elements()) return false;
  // If the dictionary backing storage takes up roughly half as much
  // space (in machine words) as a fast-case backing storage would,
  // the object should have fast elements.
  uint32_t array_size = 0;
  if (IsJSArray()) {
    CHECK(JSArray::cast(this)->length()->ToArrayIndex(&array_size));
  } else {
    array_size = dictionary->max_number_key();
  }
  uint32_t dictionary_size = static_cast<uint32_t>(dictionary->Capacity()) *
      SeededNumberDictionary::kEntrySize;
  return 2 * dictionary_size >= array_size;
}


bool JSObject::ShouldConvertToFastDoubleElements(
    bool* has_smi_only_elements) {
  *has_smi_only_elements = false;
  if (HasSloppyArgumentsElements()) return false;
  if (FLAG_unbox_double_arrays) {
    DCHECK(HasDictionaryElements());
    SeededNumberDictionary* dictionary = element_dictionary();
    bool found_double = false;
    for (int i = 0; i < dictionary->Capacity(); i++) {
      Object* key = dictionary->KeyAt(i);
      if (key->IsNumber()) {
        Object* value = dictionary->ValueAt(i);
        if (!value->IsNumber()) return false;
        if (!value->IsSmi()) {
          found_double = true;
        }
      }
    }
    *has_smi_only_elements = !found_double;
    return found_double;
  } else {
    return false;
  }
}


// Certain compilers request function template instantiation when they
// see the definition of the other template functions in the
// class. This requires us to have the template functions put
// together, so even though this function belongs in objects-debug.cc,
// we keep it here instead to satisfy certain compilers.
#ifdef OBJECT_PRINT
template <typename Derived, typename Shape, typename Key>
void Dictionary<Derived, Shape, Key>::Print(OStream& os) {  // NOLINT
  int capacity = DerivedHashTable::Capacity();
  for (int i = 0; i < capacity; i++) {
    Object* k = DerivedHashTable::KeyAt(i);
    if (DerivedHashTable::IsKey(k)) {
      os << " ";
      if (k->IsString()) {
        String::cast(k)->StringPrint(os);
      } else {
        os << Brief(k);
      }
      os << ": " << Brief(ValueAt(i)) << "\n";
    }
  }
}
#endif


template<typename Derived, typename Shape, typename Key>
void Dictionary<Derived, Shape, Key>::CopyValuesTo(FixedArray* elements) {
  int pos = 0;
  int capacity = DerivedHashTable::Capacity();
  DisallowHeapAllocation no_gc;
  WriteBarrierMode mode = elements->GetWriteBarrierMode(no_gc);
  for (int i = 0; i < capacity; i++) {
    Object* k =  Dictionary::KeyAt(i);
    if (Dictionary::IsKey(k)) {
      elements->set(pos++, ValueAt(i), mode);
    }
  }
  DCHECK(pos == elements->length());
}


InterceptorInfo* JSObject::GetNamedInterceptor() {
  DCHECK(map()->has_named_interceptor());
  JSFunction* constructor = JSFunction::cast(map()->constructor());
  DCHECK(constructor->shared()->IsApiFunction());
  Object* result =
      constructor->shared()->get_api_func_data()->named_property_handler();
  return InterceptorInfo::cast(result);
}


InterceptorInfo* JSObject::GetIndexedInterceptor() {
  DCHECK(map()->has_indexed_interceptor());
  JSFunction* constructor = JSFunction::cast(map()->constructor());
  DCHECK(constructor->shared()->IsApiFunction());
  Object* result =
      constructor->shared()->get_api_func_data()->indexed_property_handler();
  return InterceptorInfo::cast(result);
}


MaybeHandle<Object> JSObject::GetPropertyWithInterceptor(
    Handle<JSObject> holder,
    Handle<Object> receiver,
    Handle<Name> name) {
  Isolate* isolate = holder->GetIsolate();

  // TODO(rossberg): Support symbols in the API.
  if (name->IsSymbol()) return isolate->factory()->undefined_value();

  Handle<InterceptorInfo> interceptor(holder->GetNamedInterceptor(), isolate);
  Handle<String> name_string = Handle<String>::cast(name);

  if (interceptor->getter()->IsUndefined()) return MaybeHandle<Object>();

  v8::NamedPropertyGetterCallback getter =
      v8::ToCData<v8::NamedPropertyGetterCallback>(interceptor->getter());
  LOG(isolate,
      ApiNamedPropertyAccess("interceptor-named-get", *holder, *name));
  PropertyCallbackArguments
      args(isolate, interceptor->data(), *receiver, *holder);
  v8::Handle<v8::Value> result =
      args.Call(getter, v8::Utils::ToLocal(name_string));
  RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
  if (result.IsEmpty()) return MaybeHandle<Object>();

  Handle<Object> result_internal = v8::Utils::OpenHandle(*result);
  result_internal->VerifyApiCallResultType();
  // Rebox handle before return
  return handle(*result_internal, isolate);
}


// Compute the property keys from the interceptor.
// TODO(rossberg): support symbols in API, and filter here if needed.
MaybeHandle<JSObject> JSObject::GetKeysForNamedInterceptor(
    Handle<JSObject> object, Handle<JSReceiver> receiver) {
  Isolate* isolate = receiver->GetIsolate();
  Handle<InterceptorInfo> interceptor(object->GetNamedInterceptor());
  PropertyCallbackArguments
      args(isolate, interceptor->data(), *receiver, *object);
  v8::Handle<v8::Object> result;
  if (!interceptor->enumerator()->IsUndefined()) {
    v8::NamedPropertyEnumeratorCallback enum_fun =
        v8::ToCData<v8::NamedPropertyEnumeratorCallback>(
            interceptor->enumerator());
    LOG(isolate, ApiObjectAccess("interceptor-named-enum", *object));
    result = args.Call(enum_fun);
  }
  if (result.IsEmpty()) return MaybeHandle<JSObject>();
#if ENABLE_EXTRA_CHECKS
  CHECK(v8::Utils::OpenHandle(*result)->IsJSArray() ||
        v8::Utils::OpenHandle(*result)->HasSloppyArgumentsElements());
#endif
  // Rebox before returning.
  return handle(*v8::Utils::OpenHandle(*result), isolate);
}


// Compute the element keys from the interceptor.
MaybeHandle<JSObject> JSObject::GetKeysForIndexedInterceptor(
    Handle<JSObject> object, Handle<JSReceiver> receiver) {
  Isolate* isolate = receiver->GetIsolate();
  Handle<InterceptorInfo> interceptor(object->GetIndexedInterceptor());
  PropertyCallbackArguments
      args(isolate, interceptor->data(), *receiver, *object);
  v8::Handle<v8::Object> result;
  if (!interceptor->enumerator()->IsUndefined()) {
    v8::IndexedPropertyEnumeratorCallback enum_fun =
        v8::ToCData<v8::IndexedPropertyEnumeratorCallback>(
            interceptor->enumerator());
    LOG(isolate, ApiObjectAccess("interceptor-indexed-enum", *object));
    result = args.Call(enum_fun);
  }
  if (result.IsEmpty()) return MaybeHandle<JSObject>();
#if ENABLE_EXTRA_CHECKS
  CHECK(v8::Utils::OpenHandle(*result)->IsJSArray() ||
        v8::Utils::OpenHandle(*result)->HasSloppyArgumentsElements());
#endif
  // Rebox before returning.
  return handle(*v8::Utils::OpenHandle(*result), isolate);
}


Maybe<bool> JSObject::HasRealNamedProperty(Handle<JSObject> object,
                                           Handle<Name> key) {
  Isolate* isolate = object->GetIsolate();
  SealHandleScope shs(isolate);
  // Check access rights if needed.
  if (object->IsAccessCheckNeeded()) {
    if (!isolate->MayNamedAccess(object, key, v8::ACCESS_HAS)) {
      isolate->ReportFailedAccessCheck(object, v8::ACCESS_HAS);
      RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Maybe<bool>());
      return maybe(false);
    }
  }

  LookupResult result(isolate);
  object->LookupOwnRealNamedProperty(key, &result);
  return maybe(result.IsFound() && !result.IsInterceptor());
}


Maybe<bool> JSObject::HasRealElementProperty(Handle<JSObject> object,
                                             uint32_t index) {
  Isolate* isolate = object->GetIsolate();
  HandleScope scope(isolate);
  // Check access rights if needed.
  if (object->IsAccessCheckNeeded()) {
    if (!isolate->MayIndexedAccess(object, index, v8::ACCESS_HAS)) {
      isolate->ReportFailedAccessCheck(object, v8::ACCESS_HAS);
      RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Maybe<bool>());
      return maybe(false);
    }
  }

  if (object->IsJSGlobalProxy()) {
    HandleScope scope(isolate);
    PrototypeIterator iter(isolate, object);
    if (iter.IsAtEnd()) return maybe(false);
    DCHECK(PrototypeIterator::GetCurrent(iter)->IsJSGlobalObject());
    return HasRealElementProperty(
        Handle<JSObject>::cast(PrototypeIterator::GetCurrent(iter)), index);
  }

  Maybe<PropertyAttributes> result =
      GetElementAttributeWithoutInterceptor(object, object, index, false);
  if (!result.has_value) return Maybe<bool>();
  return maybe(result.value != ABSENT);
}


Maybe<bool> JSObject::HasRealNamedCallbackProperty(Handle<JSObject> object,
                                                   Handle<Name> key) {
  Isolate* isolate = object->GetIsolate();
  SealHandleScope shs(isolate);
  // Check access rights if needed.
  if (object->IsAccessCheckNeeded()) {
    if (!isolate->MayNamedAccess(object, key, v8::ACCESS_HAS)) {
      isolate->ReportFailedAccessCheck(object, v8::ACCESS_HAS);
      RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Maybe<bool>());
      return maybe(false);
    }
  }

  LookupResult result(isolate);
  object->LookupOwnRealNamedProperty(key, &result);
  return maybe(result.IsPropertyCallbacks());
}


int JSObject::NumberOfOwnProperties(PropertyAttributes filter) {
  if (HasFastProperties()) {
    Map* map = this->map();
    if (filter == NONE) return map->NumberOfOwnDescriptors();
    if (filter & DONT_ENUM) {
      int result = map->EnumLength();
      if (result != kInvalidEnumCacheSentinel) return result;
    }
    return map->NumberOfDescribedProperties(OWN_DESCRIPTORS, filter);
  }
  return property_dictionary()->NumberOfElementsFilterAttributes(filter);
}


void FixedArray::SwapPairs(FixedArray* numbers, int i, int j) {
  Object* temp = get(i);
  set(i, get(j));
  set(j, temp);
  if (this != numbers) {
    temp = numbers->get(i);
    numbers->set(i, Smi::cast(numbers->get(j)));
    numbers->set(j, Smi::cast(temp));
  }
}


static void InsertionSortPairs(FixedArray* content,
                               FixedArray* numbers,
                               int len) {
  for (int i = 1; i < len; i++) {
    int j = i;
    while (j > 0 &&
           (NumberToUint32(numbers->get(j - 1)) >
            NumberToUint32(numbers->get(j)))) {
      content->SwapPairs(numbers, j - 1, j);
      j--;
    }
  }
}


void HeapSortPairs(FixedArray* content, FixedArray* numbers, int len) {
  // In-place heap sort.
  DCHECK(content->length() == numbers->length());

  // Bottom-up max-heap construction.
  for (int i = 1; i < len; ++i) {
    int child_index = i;
    while (child_index > 0) {
      int parent_index = ((child_index + 1) >> 1) - 1;
      uint32_t parent_value = NumberToUint32(numbers->get(parent_index));
      uint32_t child_value = NumberToUint32(numbers->get(child_index));
      if (parent_value < child_value) {
        content->SwapPairs(numbers, parent_index, child_index);
      } else {
        break;
      }
      child_index = parent_index;
    }
  }

  // Extract elements and create sorted array.
  for (int i = len - 1; i > 0; --i) {
    // Put max element at the back of the array.
    content->SwapPairs(numbers, 0, i);
    // Sift down the new top element.
    int parent_index = 0;
    while (true) {
      int child_index = ((parent_index + 1) << 1) - 1;
      if (child_index >= i) break;
      uint32_t child1_value = NumberToUint32(numbers->get(child_index));
      uint32_t child2_value = NumberToUint32(numbers->get(child_index + 1));
      uint32_t parent_value = NumberToUint32(numbers->get(parent_index));
      if (child_index + 1 >= i || child1_value > child2_value) {
        if (parent_value > child1_value) break;
        content->SwapPairs(numbers, parent_index, child_index);
        parent_index = child_index;
      } else {
        if (parent_value > child2_value) break;
        content->SwapPairs(numbers, parent_index, child_index + 1);
        parent_index = child_index + 1;
      }
    }
  }
}


// Sort this array and the numbers as pairs wrt. the (distinct) numbers.
void FixedArray::SortPairs(FixedArray* numbers, uint32_t len) {
  DCHECK(this->length() == numbers->length());
  // For small arrays, simply use insertion sort.
  if (len <= 10) {
    InsertionSortPairs(this, numbers, len);
    return;
  }
  // Check the range of indices.
  uint32_t min_index = NumberToUint32(numbers->get(0));
  uint32_t max_index = min_index;
  uint32_t i;
  for (i = 1; i < len; i++) {
    if (NumberToUint32(numbers->get(i)) < min_index) {
      min_index = NumberToUint32(numbers->get(i));
    } else if (NumberToUint32(numbers->get(i)) > max_index) {
      max_index = NumberToUint32(numbers->get(i));
    }
  }
  if (max_index - min_index + 1 == len) {
    // Indices form a contiguous range, unless there are duplicates.
    // Do an in-place linear time sort assuming distinct numbers, but
    // avoid hanging in case they are not.
    for (i = 0; i < len; i++) {
      uint32_t p;
      uint32_t j = 0;
      // While the current element at i is not at its correct position p,
      // swap the elements at these two positions.
      while ((p = NumberToUint32(numbers->get(i)) - min_index) != i &&
             j++ < len) {
        SwapPairs(numbers, i, p);
      }
    }
  } else {
    HeapSortPairs(this, numbers, len);
    return;
  }
}


// Fill in the names of own properties into the supplied storage. The main
// purpose of this function is to provide reflection information for the object
// mirrors.
void JSObject::GetOwnPropertyNames(
    FixedArray* storage, int index, PropertyAttributes filter) {
  DCHECK(storage->length() >= (NumberOfOwnProperties(filter) - index));
  if (HasFastProperties()) {
    int real_size = map()->NumberOfOwnDescriptors();
    DescriptorArray* descs = map()->instance_descriptors();
    for (int i = 0; i < real_size; i++) {
      if ((descs->GetDetails(i).attributes() & filter) == 0 &&
          !FilterKey(descs->GetKey(i), filter)) {
        storage->set(index++, descs->GetKey(i));
      }
    }
  } else {
    property_dictionary()->CopyKeysTo(storage,
                                      index,
                                      filter,
                                      NameDictionary::UNSORTED);
  }
}


int JSObject::NumberOfOwnElements(PropertyAttributes filter) {
  return GetOwnElementKeys(NULL, filter);
}


int JSObject::NumberOfEnumElements() {
  // Fast case for objects with no elements.
  if (!IsJSValue() && HasFastObjectElements()) {
    uint32_t length = IsJSArray() ?
        static_cast<uint32_t>(
            Smi::cast(JSArray::cast(this)->length())->value()) :
        static_cast<uint32_t>(FixedArray::cast(elements())->length());
    if (length == 0) return 0;
  }
  // Compute the number of enumerable elements.
  return NumberOfOwnElements(static_cast<PropertyAttributes>(DONT_ENUM));
}


int JSObject::GetOwnElementKeys(FixedArray* storage,
                                PropertyAttributes filter) {
  int counter = 0;
  switch (GetElementsKind()) {
    case FAST_SMI_ELEMENTS:
    case FAST_ELEMENTS:
    case FAST_HOLEY_SMI_ELEMENTS:
    case FAST_HOLEY_ELEMENTS: {
      int length = IsJSArray() ?
          Smi::cast(JSArray::cast(this)->length())->value() :
          FixedArray::cast(elements())->length();
      for (int i = 0; i < length; i++) {
        if (!FixedArray::cast(elements())->get(i)->IsTheHole()) {
          if (storage != NULL) {
            storage->set(counter, Smi::FromInt(i));
          }
          counter++;
        }
      }
      DCHECK(!storage || storage->length() >= counter);
      break;
    }
    case FAST_DOUBLE_ELEMENTS:
    case FAST_HOLEY_DOUBLE_ELEMENTS: {
      int length = IsJSArray() ?
          Smi::cast(JSArray::cast(this)->length())->value() :
          FixedArrayBase::cast(elements())->length();
      for (int i = 0; i < length; i++) {
        if (!FixedDoubleArray::cast(elements())->is_the_hole(i)) {
          if (storage != NULL) {
            storage->set(counter, Smi::FromInt(i));
          }
          counter++;
        }
      }
      DCHECK(!storage || storage->length() >= counter);
      break;
    }

#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size)                      \
    case EXTERNAL_##TYPE##_ELEMENTS:                                         \
    case TYPE##_ELEMENTS:                                                    \

    TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
    {
      int length = FixedArrayBase::cast(elements())->length();
      while (counter < length) {
        if (storage != NULL) {
          storage->set(counter, Smi::FromInt(counter));
        }
        counter++;
      }
      DCHECK(!storage || storage->length() >= counter);
      break;
    }

    case DICTIONARY_ELEMENTS: {
      if (storage != NULL) {
        element_dictionary()->CopyKeysTo(storage,
                                         filter,
                                         SeededNumberDictionary::SORTED);
      }
      counter += element_dictionary()->NumberOfElementsFilterAttributes(filter);
      break;
    }
    case SLOPPY_ARGUMENTS_ELEMENTS: {
      FixedArray* parameter_map = FixedArray::cast(elements());
      int mapped_length = parameter_map->length() - 2;
      FixedArray* arguments = FixedArray::cast(parameter_map->get(1));
      if (arguments->IsDictionary()) {
        // Copy the keys from arguments first, because Dictionary::CopyKeysTo
        // will insert in storage starting at index 0.
        SeededNumberDictionary* dictionary =
            SeededNumberDictionary::cast(arguments);
        if (storage != NULL) {
          dictionary->CopyKeysTo(
              storage, filter, SeededNumberDictionary::UNSORTED);
        }
        counter += dictionary->NumberOfElementsFilterAttributes(filter);
        for (int i = 0; i < mapped_length; ++i) {
          if (!parameter_map->get(i + 2)->IsTheHole()) {
            if (storage != NULL) storage->set(counter, Smi::FromInt(i));
            ++counter;
          }
        }
        if (storage != NULL) storage->SortPairs(storage, counter);

      } else {
        int backing_length = arguments->length();
        int i = 0;
        for (; i < mapped_length; ++i) {
          if (!parameter_map->get(i + 2)->IsTheHole()) {
            if (storage != NULL) storage->set(counter, Smi::FromInt(i));
            ++counter;
          } else if (i < backing_length && !arguments->get(i)->IsTheHole()) {
            if (storage != NULL) storage->set(counter, Smi::FromInt(i));
            ++counter;
          }
        }
        for (; i < backing_length; ++i) {
          if (storage != NULL) storage->set(counter, Smi::FromInt(i));
          ++counter;
        }
      }
      break;
    }
  }

  if (this->IsJSValue()) {
    Object* val = JSValue::cast(this)->value();
    if (val->IsString()) {
      String* str = String::cast(val);
      if (storage) {
        for (int i = 0; i < str->length(); i++) {
          storage->set(counter + i, Smi::FromInt(i));
        }
      }
      counter += str->length();
    }
  }
  DCHECK(!storage || storage->length() == counter);
  return counter;
}


int JSObject::GetEnumElementKeys(FixedArray* storage) {
  return GetOwnElementKeys(storage, static_cast<PropertyAttributes>(DONT_ENUM));
}


// StringSharedKeys are used as keys in the eval cache.
class StringSharedKey : public HashTableKey {
 public:
  StringSharedKey(Handle<String> source,
                  Handle<SharedFunctionInfo> shared,
                  StrictMode strict_mode,
                  int scope_position)
      : source_(source),
        shared_(shared),
        strict_mode_(strict_mode),
        scope_position_(scope_position) { }

  bool IsMatch(Object* other) V8_OVERRIDE {
    DisallowHeapAllocation no_allocation;
    if (!other->IsFixedArray()) return false;
    FixedArray* other_array = FixedArray::cast(other);
    SharedFunctionInfo* shared = SharedFunctionInfo::cast(other_array->get(0));
    if (shared != *shared_) return false;
    int strict_unchecked = Smi::cast(other_array->get(2))->value();
    DCHECK(strict_unchecked == SLOPPY || strict_unchecked == STRICT);
    StrictMode strict_mode = static_cast<StrictMode>(strict_unchecked);
    if (strict_mode != strict_mode_) return false;
    int scope_position = Smi::cast(other_array->get(3))->value();
    if (scope_position != scope_position_) return false;
    String* source = String::cast(other_array->get(1));
    return source->Equals(*source_);
  }

  static uint32_t StringSharedHashHelper(String* source,
                                         SharedFunctionInfo* shared,
                                         StrictMode strict_mode,
                                         int scope_position) {
    uint32_t hash = source->Hash();
    if (shared->HasSourceCode()) {
      // Instead of using the SharedFunctionInfo pointer in the hash
      // code computation, we use a combination of the hash of the
      // script source code and the start position of the calling scope.
      // We do this to ensure that the cache entries can survive garbage
      // collection.
      Script* script(Script::cast(shared->script()));
      hash ^= String::cast(script->source())->Hash();
      if (strict_mode == STRICT) hash ^= 0x8000;
      hash += scope_position;
    }
    return hash;
  }

  uint32_t Hash() V8_OVERRIDE {
    return StringSharedHashHelper(*source_, *shared_, strict_mode_,
                                  scope_position_);
  }

  uint32_t HashForObject(Object* obj) V8_OVERRIDE {
    DisallowHeapAllocation no_allocation;
    FixedArray* other_array = FixedArray::cast(obj);
    SharedFunctionInfo* shared = SharedFunctionInfo::cast(other_array->get(0));
    String* source = String::cast(other_array->get(1));
    int strict_unchecked = Smi::cast(other_array->get(2))->value();
    DCHECK(strict_unchecked == SLOPPY || strict_unchecked == STRICT);
    StrictMode strict_mode = static_cast<StrictMode>(strict_unchecked);
    int scope_position = Smi::cast(other_array->get(3))->value();
    return StringSharedHashHelper(
        source, shared, strict_mode, scope_position);
  }


  Handle<Object> AsHandle(Isolate* isolate) V8_OVERRIDE {
    Handle<FixedArray> array = isolate->factory()->NewFixedArray(4);
    array->set(0, *shared_);
    array->set(1, *source_);
    array->set(2, Smi::FromInt(strict_mode_));
    array->set(3, Smi::FromInt(scope_position_));
    return array;
  }

 private:
  Handle<String> source_;
  Handle<SharedFunctionInfo> shared_;
  StrictMode strict_mode_;
  int scope_position_;
};


// RegExpKey carries the source and flags of a regular expression as key.
class RegExpKey : public HashTableKey {
 public:
  RegExpKey(Handle<String> string, JSRegExp::Flags flags)
      : string_(string),
        flags_(Smi::FromInt(flags.value())) { }

  // Rather than storing the key in the hash table, a pointer to the
  // stored value is stored where the key should be.  IsMatch then
  // compares the search key to the found object, rather than comparing
  // a key to a key.
  bool IsMatch(Object* obj) V8_OVERRIDE {
    FixedArray* val = FixedArray::cast(obj);
    return string_->Equals(String::cast(val->get(JSRegExp::kSourceIndex)))
        && (flags_ == val->get(JSRegExp::kFlagsIndex));
  }

  uint32_t Hash() V8_OVERRIDE { return RegExpHash(*string_, flags_); }

  Handle<Object> AsHandle(Isolate* isolate) V8_OVERRIDE {
    // Plain hash maps, which is where regexp keys are used, don't
    // use this function.
    UNREACHABLE();
    return MaybeHandle<Object>().ToHandleChecked();
  }

  uint32_t HashForObject(Object* obj) V8_OVERRIDE {
    FixedArray* val = FixedArray::cast(obj);
    return RegExpHash(String::cast(val->get(JSRegExp::kSourceIndex)),
                      Smi::cast(val->get(JSRegExp::kFlagsIndex)));
  }

  static uint32_t RegExpHash(String* string, Smi* flags) {
    return string->Hash() + flags->value();
  }

  Handle<String> string_;
  Smi* flags_;
};


Handle<Object> OneByteStringKey::AsHandle(Isolate* isolate) {
  if (hash_field_ == 0) Hash();
  return isolate->factory()->NewOneByteInternalizedString(string_, hash_field_);
}


Handle<Object> TwoByteStringKey::AsHandle(Isolate* isolate) {
  if (hash_field_ == 0) Hash();
  return isolate->factory()->NewTwoByteInternalizedString(string_, hash_field_);
}


template<>
const uint8_t* SubStringKey<uint8_t>::GetChars() {
  return string_->IsSeqOneByteString()
      ? SeqOneByteString::cast(*string_)->GetChars()
      : ExternalAsciiString::cast(*string_)->GetChars();
}


template<>
const uint16_t* SubStringKey<uint16_t>::GetChars() {
  return string_->IsSeqTwoByteString()
      ? SeqTwoByteString::cast(*string_)->GetChars()
      : ExternalTwoByteString::cast(*string_)->GetChars();
}


template<>
Handle<Object> SubStringKey<uint8_t>::AsHandle(Isolate* isolate) {
  if (hash_field_ == 0) Hash();
  Vector<const uint8_t> chars(GetChars() + from_, length_);
  return isolate->factory()->NewOneByteInternalizedString(chars, hash_field_);
}


template<>
Handle<Object> SubStringKey<uint16_t>::AsHandle(Isolate* isolate) {
  if (hash_field_ == 0) Hash();
  Vector<const uint16_t> chars(GetChars() + from_, length_);
  return isolate->factory()->NewTwoByteInternalizedString(chars, hash_field_);
}


template<>
bool SubStringKey<uint8_t>::IsMatch(Object* string) {
  Vector<const uint8_t> chars(GetChars() + from_, length_);
  return String::cast(string)->IsOneByteEqualTo(chars);
}


template<>
bool SubStringKey<uint16_t>::IsMatch(Object* string) {
  Vector<const uint16_t> chars(GetChars() + from_, length_);
  return String::cast(string)->IsTwoByteEqualTo(chars);
}


template class SubStringKey<uint8_t>;
template class SubStringKey<uint16_t>;


// InternalizedStringKey carries a string/internalized-string object as key.
class InternalizedStringKey : public HashTableKey {
 public:
  explicit InternalizedStringKey(Handle<String> string)
      : string_(string) { }

  virtual bool IsMatch(Object* string) V8_OVERRIDE {
    return String::cast(string)->Equals(*string_);
  }

  virtual uint32_t Hash() V8_OVERRIDE { return string_->Hash(); }

  virtual uint32_t HashForObject(Object* other) V8_OVERRIDE {
    return String::cast(other)->Hash();
  }

  virtual Handle<Object> AsHandle(Isolate* isolate) V8_OVERRIDE {
    // Internalize the string if possible.
    MaybeHandle<Map> maybe_map =
        isolate->factory()->InternalizedStringMapForString(string_);
    Handle<Map> map;
    if (maybe_map.ToHandle(&map)) {
      string_->set_map_no_write_barrier(*map);
      DCHECK(string_->IsInternalizedString());
      return string_;
    }
    // Otherwise allocate a new internalized string.
    return isolate->factory()->NewInternalizedStringImpl(
        string_, string_->length(), string_->hash_field());
  }

  static uint32_t StringHash(Object* obj) {
    return String::cast(obj)->Hash();
  }

  Handle<String> string_;
};


template<typename Derived, typename Shape, typename Key>
void HashTable<Derived, Shape, Key>::IteratePrefix(ObjectVisitor* v) {
  IteratePointers(v, 0, kElementsStartOffset);
}


template<typename Derived, typename Shape, typename Key>
void HashTable<Derived, Shape, Key>::IterateElements(ObjectVisitor* v) {
  IteratePointers(v,
                  kElementsStartOffset,
                  kHeaderSize + length() * kPointerSize);
}


template<typename Derived, typename Shape, typename Key>
Handle<Derived> HashTable<Derived, Shape, Key>::New(
    Isolate* isolate,
    int at_least_space_for,
    MinimumCapacity capacity_option,
    PretenureFlag pretenure) {
  DCHECK(0 <= at_least_space_for);
  DCHECK(!capacity_option || IsPowerOf2(at_least_space_for));
  int capacity = (capacity_option == USE_CUSTOM_MINIMUM_CAPACITY)
                     ? at_least_space_for
                     : ComputeCapacity(at_least_space_for);
  if (capacity > HashTable::kMaxCapacity) {
    v8::internal::Heap::FatalProcessOutOfMemory("invalid table size", true);
  }

  Factory* factory = isolate->factory();
  int length = EntryToIndex(capacity);
  Handle<FixedArray> array = factory->NewFixedArray(length, pretenure);
  array->set_map_no_write_barrier(*factory->hash_table_map());
  Handle<Derived> table = Handle<Derived>::cast(array);

  table->SetNumberOfElements(0);
  table->SetNumberOfDeletedElements(0);
  table->SetCapacity(capacity);
  return table;
}


// Find entry for key otherwise return kNotFound.
int NameDictionary::FindEntry(Handle<Name> key) {
  if (!key->IsUniqueName()) {
    return DerivedHashTable::FindEntry(key);
  }

  // Optimized for unique names. Knowledge of the key type allows:
  // 1. Move the check if the key is unique out of the loop.
  // 2. Avoid comparing hash codes in unique-to-unique comparison.
  // 3. Detect a case when a dictionary key is not unique but the key is.
  //    In case of positive result the dictionary key may be replaced by the
  //    internalized string with minimal performance penalty. It gives a chance
  //    to perform further lookups in code stubs (and significant performance
  //    boost a certain style of code).

  // EnsureCapacity will guarantee the hash table is never full.
  uint32_t capacity = Capacity();
  uint32_t entry = FirstProbe(key->Hash(), capacity);
  uint32_t count = 1;

  while (true) {
    int index = EntryToIndex(entry);
    Object* element = get(index);
    if (element->IsUndefined()) break;  // Empty entry.
    if (*key == element) return entry;
    if (!element->IsUniqueName() &&
        !element->IsTheHole() &&
        Name::cast(element)->Equals(*key)) {
      // Replace a key that is a non-internalized string by the equivalent
      // internalized string for faster further lookups.
      set(index, *key);
      return entry;
    }
    DCHECK(element->IsTheHole() || !Name::cast(element)->Equals(*key));
    entry = NextProbe(entry, count++, capacity);
  }
  return kNotFound;
}


template<typename Derived, typename Shape, typename Key>
void HashTable<Derived, Shape, Key>::Rehash(
    Handle<Derived> new_table,
    Key key) {
  DCHECK(NumberOfElements() < new_table->Capacity());

  DisallowHeapAllocation no_gc;
  WriteBarrierMode mode = new_table->GetWriteBarrierMode(no_gc);

  // Copy prefix to new array.
  for (int i = kPrefixStartIndex;
       i < kPrefixStartIndex + Shape::kPrefixSize;
       i++) {
    new_table->set(i, get(i), mode);
  }

  // Rehash the elements.
  int capacity = Capacity();
  for (int i = 0; i < capacity; i++) {
    uint32_t from_index = EntryToIndex(i);
    Object* k = get(from_index);
    if (IsKey(k)) {
      uint32_t hash = HashTable::HashForObject(key, k);
      uint32_t insertion_index =
          EntryToIndex(new_table->FindInsertionEntry(hash));
      for (int j = 0; j < Shape::kEntrySize; j++) {
        new_table->set(insertion_index + j, get(from_index + j), mode);
      }
    }
  }
  new_table->SetNumberOfElements(NumberOfElements());
  new_table->SetNumberOfDeletedElements(0);
}


template<typename Derived, typename Shape, typename Key>
uint32_t HashTable<Derived, Shape, Key>::EntryForProbe(
    Key key,
    Object* k,
    int probe,
    uint32_t expected) {
  uint32_t hash = HashTable::HashForObject(key, k);
  uint32_t capacity = Capacity();
  uint32_t entry = FirstProbe(hash, capacity);
  for (int i = 1; i < probe; i++) {
    if (entry == expected) return expected;
    entry = NextProbe(entry, i, capacity);
  }
  return entry;
}


template<typename Derived, typename Shape, typename Key>
void HashTable<Derived, Shape, Key>::Swap(uint32_t entry1,
                                          uint32_t entry2,
                                          WriteBarrierMode mode) {
  int index1 = EntryToIndex(entry1);
  int index2 = EntryToIndex(entry2);
  Object* temp[Shape::kEntrySize];
  for (int j = 0; j < Shape::kEntrySize; j++) {
    temp[j] = get(index1 + j);
  }
  for (int j = 0; j < Shape::kEntrySize; j++) {
    set(index1 + j, get(index2 + j), mode);
  }
  for (int j = 0; j < Shape::kEntrySize; j++) {
    set(index2 + j, temp[j], mode);
  }
}


template<typename Derived, typename Shape, typename Key>
void HashTable<Derived, Shape, Key>::Rehash(Key key) {
  DisallowHeapAllocation no_gc;
  WriteBarrierMode mode = GetWriteBarrierMode(no_gc);
  uint32_t capacity = Capacity();
  bool done = false;
  for (int probe = 1; !done; probe++) {
    // All elements at entries given by one of the first _probe_ probes
    // are placed correctly. Other elements might need to be moved.
    done = true;
    for (uint32_t current = 0; current < capacity; current++) {
      Object* current_key = get(EntryToIndex(current));
      if (IsKey(current_key)) {
        uint32_t target = EntryForProbe(key, current_key, probe, current);
        if (current == target) continue;
        Object* target_key = get(EntryToIndex(target));
        if (!IsKey(target_key) ||
            EntryForProbe(key, target_key, probe, target) != target) {
          // Put the current element into the correct position.
          Swap(current, target, mode);
          // The other element will be processed on the next iteration.
          current--;
        } else {
          // The place for the current element is occupied. Leave the element
          // for the next probe.
          done = false;
        }
      }
    }
  }
}


template<typename Derived, typename Shape, typename Key>
Handle<Derived> HashTable<Derived, Shape, Key>::EnsureCapacity(
    Handle<Derived> table,
    int n,
    Key key,
    PretenureFlag pretenure) {
  Isolate* isolate = table->GetIsolate();
  int capacity = table->Capacity();
  int nof = table->NumberOfElements() + n;
  int nod = table->NumberOfDeletedElements();
  // Return if:
  //   50% is still free after adding n elements and
  //   at most 50% of the free elements are deleted elements.
  if (nod <= (capacity - nof) >> 1) {
    int needed_free = nof >> 1;
    if (nof + needed_free <= capacity) return table;
  }

  const int kMinCapacityForPretenure = 256;
  bool should_pretenure = pretenure == TENURED ||
      ((capacity > kMinCapacityForPretenure) &&
          !isolate->heap()->InNewSpace(*table));
  Handle<Derived> new_table = HashTable::New(
      isolate,
      nof * 2,
      USE_DEFAULT_MINIMUM_CAPACITY,
      should_pretenure ? TENURED : NOT_TENURED);

  table->Rehash(new_table, key);
  return new_table;
}


template<typename Derived, typename Shape, typename Key>
Handle<Derived> HashTable<Derived, Shape, Key>::Shrink(Handle<Derived> table,
                                                       Key key) {
  int capacity = table->Capacity();
  int nof = table->NumberOfElements();

  // Shrink to fit the number of elements if only a quarter of the
  // capacity is filled with elements.
  if (nof > (capacity >> 2)) return table;
  // Allocate a new dictionary with room for at least the current
  // number of elements. The allocation method will make sure that
  // there is extra room in the dictionary for additions. Don't go
  // lower than room for 16 elements.
  int at_least_room_for = nof;
  if (at_least_room_for < 16) return table;

  Isolate* isolate = table->GetIsolate();
  const int kMinCapacityForPretenure = 256;
  bool pretenure =
      (at_least_room_for > kMinCapacityForPretenure) &&
      !isolate->heap()->InNewSpace(*table);
  Handle<Derived> new_table = HashTable::New(
      isolate,
      at_least_room_for,
      USE_DEFAULT_MINIMUM_CAPACITY,
      pretenure ? TENURED : NOT_TENURED);

  table->Rehash(new_table, key);
  return new_table;
}


template<typename Derived, typename Shape, typename Key>
uint32_t HashTable<Derived, Shape, Key>::FindInsertionEntry(uint32_t hash) {
  uint32_t capacity = Capacity();
  uint32_t entry = FirstProbe(hash, capacity);
  uint32_t count = 1;
  // EnsureCapacity will guarantee the hash table is never full.
  while (true) {
    Object* element = KeyAt(entry);
    if (element->IsUndefined() || element->IsTheHole()) break;
    entry = NextProbe(entry, count++, capacity);
  }
  return entry;
}


// Force instantiation of template instances class.
// Please note this list is compiler dependent.

template class HashTable<StringTable, StringTableShape, HashTableKey*>;

template class HashTable<CompilationCacheTable,
                         CompilationCacheShape,
                         HashTableKey*>;

template class HashTable<MapCache, MapCacheShape, HashTableKey*>;

template class HashTable<ObjectHashTable,
                         ObjectHashTableShape,
                         Handle<Object> >;

template class HashTable<WeakHashTable, WeakHashTableShape<2>, Handle<Object> >;

template class Dictionary<NameDictionary, NameDictionaryShape, Handle<Name> >;

template class Dictionary<SeededNumberDictionary,
                          SeededNumberDictionaryShape,
                          uint32_t>;

template class Dictionary<UnseededNumberDictionary,
                          UnseededNumberDictionaryShape,
                          uint32_t>;

template Handle<SeededNumberDictionary>
Dictionary<SeededNumberDictionary, SeededNumberDictionaryShape, uint32_t>::
    New(Isolate*, int at_least_space_for, PretenureFlag pretenure);

template Handle<UnseededNumberDictionary>
Dictionary<UnseededNumberDictionary, UnseededNumberDictionaryShape, uint32_t>::
    New(Isolate*, int at_least_space_for, PretenureFlag pretenure);

template Handle<NameDictionary>
Dictionary<NameDictionary, NameDictionaryShape, Handle<Name> >::
    New(Isolate*, int n, PretenureFlag pretenure);

template Handle<SeededNumberDictionary>
Dictionary<SeededNumberDictionary, SeededNumberDictionaryShape, uint32_t>::
    AtPut(Handle<SeededNumberDictionary>, uint32_t, Handle<Object>);

template Handle<UnseededNumberDictionary>
Dictionary<UnseededNumberDictionary, UnseededNumberDictionaryShape, uint32_t>::
    AtPut(Handle<UnseededNumberDictionary>, uint32_t, Handle<Object>);

template Object*
Dictionary<SeededNumberDictionary, SeededNumberDictionaryShape, uint32_t>::
    SlowReverseLookup(Object* value);

template Object*
Dictionary<NameDictionary, NameDictionaryShape, Handle<Name> >::
    SlowReverseLookup(Object* value);

template void
Dictionary<SeededNumberDictionary, SeededNumberDictionaryShape, uint32_t>::
    CopyKeysTo(
        FixedArray*,
        PropertyAttributes,
        Dictionary<SeededNumberDictionary,
                   SeededNumberDictionaryShape,
                   uint32_t>::SortMode);

template Handle<Object>
Dictionary<NameDictionary, NameDictionaryShape, Handle<Name> >::DeleteProperty(
    Handle<NameDictionary>, int, JSObject::DeleteMode);

template Handle<Object>
Dictionary<SeededNumberDictionary, SeededNumberDictionaryShape, uint32_t>::
    DeleteProperty(Handle<SeededNumberDictionary>, int, JSObject::DeleteMode);

template Handle<NameDictionary>
HashTable<NameDictionary, NameDictionaryShape, Handle<Name> >::
    New(Isolate*, int, MinimumCapacity, PretenureFlag);

template Handle<NameDictionary>
HashTable<NameDictionary, NameDictionaryShape, Handle<Name> >::
    Shrink(Handle<NameDictionary>, Handle<Name>);

template Handle<SeededNumberDictionary>
HashTable<SeededNumberDictionary, SeededNumberDictionaryShape, uint32_t>::
    Shrink(Handle<SeededNumberDictionary>, uint32_t);

template void Dictionary<NameDictionary, NameDictionaryShape, Handle<Name> >::
    CopyKeysTo(
        FixedArray*,
        int,
        PropertyAttributes,
        Dictionary<
            NameDictionary, NameDictionaryShape, Handle<Name> >::SortMode);

template int
Dictionary<NameDictionary, NameDictionaryShape, Handle<Name> >::
    NumberOfElementsFilterAttributes(PropertyAttributes);

template Handle<NameDictionary>
Dictionary<NameDictionary, NameDictionaryShape, Handle<Name> >::Add(
    Handle<NameDictionary>, Handle<Name>, Handle<Object>, PropertyDetails);

template void
Dictionary<NameDictionary, NameDictionaryShape, Handle<Name> >::
    GenerateNewEnumerationIndices(Handle<NameDictionary>);

template int
Dictionary<SeededNumberDictionary, SeededNumberDictionaryShape, uint32_t>::
    NumberOfElementsFilterAttributes(PropertyAttributes);

template Handle<SeededNumberDictionary>
Dictionary<SeededNumberDictionary, SeededNumberDictionaryShape, uint32_t>::
    Add(Handle<SeededNumberDictionary>,
        uint32_t,
        Handle<Object>,
        PropertyDetails);

template Handle<UnseededNumberDictionary>
Dictionary<UnseededNumberDictionary, UnseededNumberDictionaryShape, uint32_t>::
    Add(Handle<UnseededNumberDictionary>,
        uint32_t,
        Handle<Object>,
        PropertyDetails);

template Handle<SeededNumberDictionary>
Dictionary<SeededNumberDictionary, SeededNumberDictionaryShape, uint32_t>::
    EnsureCapacity(Handle<SeededNumberDictionary>, int, uint32_t);

template Handle<UnseededNumberDictionary>
Dictionary<UnseededNumberDictionary, UnseededNumberDictionaryShape, uint32_t>::
    EnsureCapacity(Handle<UnseededNumberDictionary>, int, uint32_t);

template Handle<NameDictionary>
Dictionary<NameDictionary, NameDictionaryShape, Handle<Name> >::
    EnsureCapacity(Handle<NameDictionary>, int, Handle<Name>);

template
int Dictionary<SeededNumberDictionary, SeededNumberDictionaryShape, uint32_t>::
    NumberOfEnumElements();

template
int Dictionary<NameDictionary, NameDictionaryShape, Handle<Name> >::
    NumberOfEnumElements();

template
int HashTable<SeededNumberDictionary, SeededNumberDictionaryShape, uint32_t>::
    FindEntry(uint32_t);


Handle<Object> JSObject::PrepareSlowElementsForSort(
    Handle<JSObject> object, uint32_t limit) {
  DCHECK(object->HasDictionaryElements());
  Isolate* isolate = object->GetIsolate();
  // Must stay in dictionary mode, either because of requires_slow_elements,
  // or because we are not going to sort (and therefore compact) all of the
  // elements.
  Handle<SeededNumberDictionary> dict(object->element_dictionary(), isolate);
  Handle<SeededNumberDictionary> new_dict =
      SeededNumberDictionary::New(isolate, dict->NumberOfElements());

  uint32_t pos = 0;
  uint32_t undefs = 0;
  int capacity = dict->Capacity();
  Handle<Smi> bailout(Smi::FromInt(-1), isolate);
  // Entry to the new dictionary does not cause it to grow, as we have
  // allocated one that is large enough for all entries.
  DisallowHeapAllocation no_gc;
  for (int i = 0; i < capacity; i++) {
    Object* k = dict->KeyAt(i);
    if (!dict->IsKey(k)) continue;

    DCHECK(k->IsNumber());
    DCHECK(!k->IsSmi() || Smi::cast(k)->value() >= 0);
    DCHECK(!k->IsHeapNumber() || HeapNumber::cast(k)->value() >= 0);
    DCHECK(!k->IsHeapNumber() || HeapNumber::cast(k)->value() <= kMaxUInt32);

    HandleScope scope(isolate);
    Handle<Object> value(dict->ValueAt(i), isolate);
    PropertyDetails details = dict->DetailsAt(i);
    if (details.type() == CALLBACKS || details.IsReadOnly()) {
      // Bail out and do the sorting of undefineds and array holes in JS.
      // Also bail out if the element is not supposed to be moved.
      return bailout;
    }

    uint32_t key = NumberToUint32(k);
    if (key < limit) {
      if (value->IsUndefined()) {
        undefs++;
      } else if (pos > static_cast<uint32_t>(Smi::kMaxValue)) {
        // Adding an entry with the key beyond smi-range requires
        // allocation. Bailout.
        return bailout;
      } else {
        Handle<Object> result = SeededNumberDictionary::AddNumberEntry(
            new_dict, pos, value, details);
        DCHECK(result.is_identical_to(new_dict));
        USE(result);
        pos++;
      }
    } else if (key > static_cast<uint32_t>(Smi::kMaxValue)) {
      // Adding an entry with the key beyond smi-range requires
      // allocation. Bailout.
      return bailout;
    } else {
      Handle<Object> result = SeededNumberDictionary::AddNumberEntry(
          new_dict, key, value, details);
      DCHECK(result.is_identical_to(new_dict));
      USE(result);
    }
  }

  uint32_t result = pos;
  PropertyDetails no_details = PropertyDetails(NONE, NORMAL, 0);
  while (undefs > 0) {
    if (pos > static_cast<uint32_t>(Smi::kMaxValue)) {
      // Adding an entry with the key beyond smi-range requires
      // allocation. Bailout.
      return bailout;
    }
    HandleScope scope(isolate);
    Handle<Object> result = SeededNumberDictionary::AddNumberEntry(
        new_dict, pos, isolate->factory()->undefined_value(), no_details);
    DCHECK(result.is_identical_to(new_dict));
    USE(result);
    pos++;
    undefs--;
  }

  object->set_elements(*new_dict);

  AllowHeapAllocation allocate_return_value;
  return isolate->factory()->NewNumberFromUint(result);
}


// Collects all defined (non-hole) and non-undefined (array) elements at
// the start of the elements array.
// If the object is in dictionary mode, it is converted to fast elements
// mode.
Handle<Object> JSObject::PrepareElementsForSort(Handle<JSObject> object,
                                                uint32_t limit) {
  Isolate* isolate = object->GetIsolate();
  if (object->HasSloppyArgumentsElements() ||
      object->map()->is_observed()) {
    return handle(Smi::FromInt(-1), isolate);
  }

  if (object->HasDictionaryElements()) {
    // Convert to fast elements containing only the existing properties.
    // Ordering is irrelevant, since we are going to sort anyway.
    Handle<SeededNumberDictionary> dict(object->element_dictionary());
    if (object->IsJSArray() || dict->requires_slow_elements() ||
        dict->max_number_key() >= limit) {
      return JSObject::PrepareSlowElementsForSort(object, limit);
    }
    // Convert to fast elements.

    Handle<Map> new_map =
        JSObject::GetElementsTransitionMap(object, FAST_HOLEY_ELEMENTS);

    PretenureFlag tenure = isolate->heap()->InNewSpace(*object) ?
        NOT_TENURED: TENURED;
    Handle<FixedArray> fast_elements =
        isolate->factory()->NewFixedArray(dict->NumberOfElements(), tenure);
    dict->CopyValuesTo(*fast_elements);
    JSObject::ValidateElements(object);

    JSObject::SetMapAndElements(object, new_map, fast_elements);
  } else if (object->HasExternalArrayElements() ||
             object->HasFixedTypedArrayElements()) {
    // Typed arrays cannot have holes or undefined elements.
    return handle(Smi::FromInt(
        FixedArrayBase::cast(object->elements())->length()), isolate);
  } else if (!object->HasFastDoubleElements()) {
    EnsureWritableFastElements(object);
  }
  DCHECK(object->HasFastSmiOrObjectElements() ||
         object->HasFastDoubleElements());

  // Collect holes at the end, undefined before that and the rest at the
  // start, and return the number of non-hole, non-undefined values.

  Handle<FixedArrayBase> elements_base(object->elements());
  uint32_t elements_length = static_cast<uint32_t>(elements_base->length());
  if (limit > elements_length) {
    limit = elements_length ;
  }
  if (limit == 0) {
    return handle(Smi::FromInt(0), isolate);
  }

  uint32_t result = 0;
  if (elements_base->map() == isolate->heap()->fixed_double_array_map()) {
    FixedDoubleArray* elements = FixedDoubleArray::cast(*elements_base);
    // Split elements into defined and the_hole, in that order.
    unsigned int holes = limit;
    // Assume most arrays contain no holes and undefined values, so minimize the
    // number of stores of non-undefined, non-the-hole values.
    for (unsigned int i = 0; i < holes; i++) {
      if (elements->is_the_hole(i)) {
        holes--;
      } else {
        continue;
      }
      // Position i needs to be filled.
      while (holes > i) {
        if (elements->is_the_hole(holes)) {
          holes--;
        } else {
          elements->set(i, elements->get_scalar(holes));
          break;
        }
      }
    }
    result = holes;
    while (holes < limit) {
      elements->set_the_hole(holes);
      holes++;
    }
  } else {
    FixedArray* elements = FixedArray::cast(*elements_base);
    DisallowHeapAllocation no_gc;

    // Split elements into defined, undefined and the_hole, in that order.  Only
    // count locations for undefined and the hole, and fill them afterwards.
    WriteBarrierMode write_barrier = elements->GetWriteBarrierMode(no_gc);
    unsigned int undefs = limit;
    unsigned int holes = limit;
    // Assume most arrays contain no holes and undefined values, so minimize the
    // number of stores of non-undefined, non-the-hole values.
    for (unsigned int i = 0; i < undefs; i++) {
      Object* current = elements->get(i);
      if (current->IsTheHole()) {
        holes--;
        undefs--;
      } else if (current->IsUndefined()) {
        undefs--;
      } else {
        continue;
      }
      // Position i needs to be filled.
      while (undefs > i) {
        current = elements->get(undefs);
        if (current->IsTheHole()) {
          holes--;
          undefs--;
        } else if (current->IsUndefined()) {
          undefs--;
        } else {
          elements->set(i, current, write_barrier);
          break;
        }
      }
    }
    result = undefs;
    while (undefs < holes) {
      elements->set_undefined(undefs);
      undefs++;
    }
    while (holes < limit) {
      elements->set_the_hole(holes);
      holes++;
    }
  }

  return isolate->factory()->NewNumberFromUint(result);
}


ExternalArrayType JSTypedArray::type() {
  switch (elements()->map()->instance_type()) {
#define INSTANCE_TYPE_TO_ARRAY_TYPE(Type, type, TYPE, ctype, size)            \
    case EXTERNAL_##TYPE##_ARRAY_TYPE:                                        \
    case FIXED_##TYPE##_ARRAY_TYPE:                                           \
      return kExternal##Type##Array;

    TYPED_ARRAYS(INSTANCE_TYPE_TO_ARRAY_TYPE)
#undef INSTANCE_TYPE_TO_ARRAY_TYPE

    default:
      UNREACHABLE();
      return static_cast<ExternalArrayType>(-1);
  }
}


size_t JSTypedArray::element_size() {
  switch (elements()->map()->instance_type()) {
#define INSTANCE_TYPE_TO_ELEMENT_SIZE(Type, type, TYPE, ctype, size)          \
    case EXTERNAL_##TYPE##_ARRAY_TYPE:                                        \
      return size;

    TYPED_ARRAYS(INSTANCE_TYPE_TO_ELEMENT_SIZE)
#undef INSTANCE_TYPE_TO_ELEMENT_SIZE

    default:
      UNREACHABLE();
      return 0;
  }
}


Handle<Object> ExternalUint8ClampedArray::SetValue(
    Handle<ExternalUint8ClampedArray> array,
    uint32_t index,
    Handle<Object> value) {
  uint8_t clamped_value = 0;
  if (index < static_cast<uint32_t>(array->length())) {
    if (value->IsSmi()) {
      int int_value = Handle<Smi>::cast(value)->value();
      if (int_value < 0) {
        clamped_value = 0;
      } else if (int_value > 255) {
        clamped_value = 255;
      } else {
        clamped_value = static_cast<uint8_t>(int_value);
      }
    } else if (value->IsHeapNumber()) {
      double double_value = Handle<HeapNumber>::cast(value)->value();
      if (!(double_value > 0)) {
        // NaN and less than zero clamp to zero.
        clamped_value = 0;
      } else if (double_value > 255) {
        // Greater than 255 clamp to 255.
        clamped_value = 255;
      } else {
        // Other doubles are rounded to the nearest integer.
        clamped_value = static_cast<uint8_t>(lrint(double_value));
      }
    } else {
      // Clamp undefined to zero (default). All other types have been
      // converted to a number type further up in the call chain.
      DCHECK(value->IsUndefined());
    }
    array->set(index, clamped_value);
  }
  return handle(Smi::FromInt(clamped_value), array->GetIsolate());
}


template<typename ExternalArrayClass, typename ValueType>
static Handle<Object> ExternalArrayIntSetter(
    Isolate* isolate,
    Handle<ExternalArrayClass> receiver,
    uint32_t index,
    Handle<Object> value) {
  ValueType cast_value = 0;
  if (index < static_cast<uint32_t>(receiver->length())) {
    if (value->IsSmi()) {
      int int_value = Handle<Smi>::cast(value)->value();
      cast_value = static_cast<ValueType>(int_value);
    } else if (value->IsHeapNumber()) {
      double double_value = Handle<HeapNumber>::cast(value)->value();
      cast_value = static_cast<ValueType>(DoubleToInt32(double_value));
    } else {
      // Clamp undefined to zero (default). All other types have been
      // converted to a number type further up in the call chain.
      DCHECK(value->IsUndefined());
    }
    receiver->set(index, cast_value);
  }
  return isolate->factory()->NewNumberFromInt(cast_value);
}


Handle<Object> ExternalInt8Array::SetValue(Handle<ExternalInt8Array> array,
                                           uint32_t index,
                                           Handle<Object> value) {
  return ExternalArrayIntSetter<ExternalInt8Array, int8_t>(
      array->GetIsolate(), array, index, value);
}


Handle<Object> ExternalUint8Array::SetValue(Handle<ExternalUint8Array> array,
                                            uint32_t index,
                                            Handle<Object> value) {
  return ExternalArrayIntSetter<ExternalUint8Array, uint8_t>(
      array->GetIsolate(), array, index, value);
}


Handle<Object> ExternalInt16Array::SetValue(Handle<ExternalInt16Array> array,
                                            uint32_t index,
                                            Handle<Object> value) {
  return ExternalArrayIntSetter<ExternalInt16Array, int16_t>(
      array->GetIsolate(), array, index, value);
}


Handle<Object> ExternalUint16Array::SetValue(Handle<ExternalUint16Array> array,
                                             uint32_t index,
                                             Handle<Object> value) {
  return ExternalArrayIntSetter<ExternalUint16Array, uint16_t>(
      array->GetIsolate(), array, index, value);
}


Handle<Object> ExternalInt32Array::SetValue(Handle<ExternalInt32Array> array,
                                            uint32_t index,
                                            Handle<Object> value) {
  return ExternalArrayIntSetter<ExternalInt32Array, int32_t>(
      array->GetIsolate(), array, index, value);
}


Handle<Object> ExternalUint32Array::SetValue(
    Handle<ExternalUint32Array> array,
    uint32_t index,
    Handle<Object> value) {
  uint32_t cast_value = 0;
  if (index < static_cast<uint32_t>(array->length())) {
    if (value->IsSmi()) {
      int int_value = Handle<Smi>::cast(value)->value();
      cast_value = static_cast<uint32_t>(int_value);
    } else if (value->IsHeapNumber()) {
      double double_value = Handle<HeapNumber>::cast(value)->value();
      cast_value = static_cast<uint32_t>(DoubleToUint32(double_value));
    } else {
      // Clamp undefined to zero (default). All other types have been
      // converted to a number type further up in the call chain.
      DCHECK(value->IsUndefined());
    }
    array->set(index, cast_value);
  }
  return array->GetIsolate()->factory()->NewNumberFromUint(cast_value);
}


Handle<Object> ExternalFloat32Array::SetValue(
    Handle<ExternalFloat32Array> array,
    uint32_t index,
    Handle<Object> value) {
  float cast_value = static_cast<float>(base::OS::nan_value());
  if (index < static_cast<uint32_t>(array->length())) {
    if (value->IsSmi()) {
      int int_value = Handle<Smi>::cast(value)->value();
      cast_value = static_cast<float>(int_value);
    } else if (value->IsHeapNumber()) {
      double double_value = Handle<HeapNumber>::cast(value)->value();
      cast_value = static_cast<float>(double_value);
    } else {
      // Clamp undefined to NaN (default). All other types have been
      // converted to a number type further up in the call chain.
      DCHECK(value->IsUndefined());
    }
    array->set(index, cast_value);
  }
  return array->GetIsolate()->factory()->NewNumber(cast_value);
}


Handle<Object> ExternalFloat64Array::SetValue(
    Handle<ExternalFloat64Array> array,
    uint32_t index,
    Handle<Object> value) {
  double double_value = base::OS::nan_value();
  if (index < static_cast<uint32_t>(array->length())) {
    if (value->IsNumber()) {
      double_value = value->Number();
    } else {
      // Clamp undefined to NaN (default). All other types have been
      // converted to a number type further up in the call chain.
      DCHECK(value->IsUndefined());
    }
    array->set(index, double_value);
  }
  return array->GetIsolate()->factory()->NewNumber(double_value);
}


PropertyCell* GlobalObject::GetPropertyCell(LookupResult* result) {
  DCHECK(!HasFastProperties());
  Object* value = property_dictionary()->ValueAt(result->GetDictionaryEntry());
  return PropertyCell::cast(value);
}


Handle<PropertyCell> JSGlobalObject::EnsurePropertyCell(
    Handle<JSGlobalObject> global,
    Handle<Name> name) {
  DCHECK(!global->HasFastProperties());
  int entry = global->property_dictionary()->FindEntry(name);
  if (entry == NameDictionary::kNotFound) {
    Isolate* isolate = global->GetIsolate();
    Handle<PropertyCell> cell = isolate->factory()->NewPropertyCell(
        isolate->factory()->the_hole_value());
    PropertyDetails details(NONE, NORMAL, 0);
    details = details.AsDeleted();
    Handle<NameDictionary> dictionary = NameDictionary::Add(
        handle(global->property_dictionary()), name, cell, details);
    global->set_properties(*dictionary);
    return cell;
  } else {
    Object* value = global->property_dictionary()->ValueAt(entry);
    DCHECK(value->IsPropertyCell());
    return handle(PropertyCell::cast(value));
  }
}


// This class is used for looking up two character strings in the string table.
// If we don't have a hit we don't want to waste much time so we unroll the
// string hash calculation loop here for speed.  Doesn't work if the two
// characters form a decimal integer, since such strings have a different hash
// algorithm.
class TwoCharHashTableKey : public HashTableKey {
 public:
  TwoCharHashTableKey(uint16_t c1, uint16_t c2, uint32_t seed)
    : c1_(c1), c2_(c2) {
    // Char 1.
    uint32_t hash = seed;
    hash += c1;
    hash += hash << 10;
    hash ^= hash >> 6;
    // Char 2.
    hash += c2;
    hash += hash << 10;
    hash ^= hash >> 6;
    // GetHash.
    hash += hash << 3;
    hash ^= hash >> 11;
    hash += hash << 15;
    if ((hash & String::kHashBitMask) == 0) hash = StringHasher::kZeroHash;
    hash_ = hash;
#ifdef DEBUG
    // If this assert fails then we failed to reproduce the two-character
    // version of the string hashing algorithm above.  One reason could be
    // that we were passed two digits as characters, since the hash
    // algorithm is different in that case.
    uint16_t chars[2] = {c1, c2};
    uint32_t check_hash = StringHasher::HashSequentialString(chars, 2, seed);
    hash = (hash << String::kHashShift) | String::kIsNotArrayIndexMask;
    DCHECK_EQ(static_cast<int32_t>(hash), static_cast<int32_t>(check_hash));
#endif
  }

  bool IsMatch(Object* o) V8_OVERRIDE {
    if (!o->IsString()) return false;
    String* other = String::cast(o);
    if (other->length() != 2) return false;
    if (other->Get(0) != c1_) return false;
    return other->Get(1) == c2_;
  }

  uint32_t Hash() V8_OVERRIDE { return hash_; }
  uint32_t HashForObject(Object* key) V8_OVERRIDE {
    if (!key->IsString()) return 0;
    return String::cast(key)->Hash();
  }

  Handle<Object> AsHandle(Isolate* isolate) V8_OVERRIDE {
    // The TwoCharHashTableKey is only used for looking in the string
    // table, not for adding to it.
    UNREACHABLE();
    return MaybeHandle<Object>().ToHandleChecked();
  }

 private:
  uint16_t c1_;
  uint16_t c2_;
  uint32_t hash_;
};


MaybeHandle<String> StringTable::InternalizeStringIfExists(
    Isolate* isolate,
    Handle<String> string) {
  if (string->IsInternalizedString()) {
    return string;
  }
  return LookupStringIfExists(isolate, string);
}


MaybeHandle<String> StringTable::LookupStringIfExists(
    Isolate* isolate,
    Handle<String> string) {
  Handle<StringTable> string_table = isolate->factory()->string_table();
  InternalizedStringKey key(string);
  int entry = string_table->FindEntry(&key);
  if (entry == kNotFound) {
    return MaybeHandle<String>();
  } else {
    Handle<String> result(String::cast(string_table->KeyAt(entry)), isolate);
    DCHECK(StringShape(*result).IsInternalized());
    return result;
  }
}


MaybeHandle<String> StringTable::LookupTwoCharsStringIfExists(
    Isolate* isolate,
    uint16_t c1,
    uint16_t c2) {
  Handle<StringTable> string_table = isolate->factory()->string_table();
  TwoCharHashTableKey key(c1, c2, isolate->heap()->HashSeed());
  int entry = string_table->FindEntry(&key);
  if (entry == kNotFound) {
    return MaybeHandle<String>();
  } else {
    Handle<String> result(String::cast(string_table->KeyAt(entry)), isolate);
    DCHECK(StringShape(*result).IsInternalized());
    return result;
  }
}


Handle<String> StringTable::LookupString(Isolate* isolate,
                                         Handle<String> string) {
  InternalizedStringKey key(string);
  return LookupKey(isolate, &key);
}


Handle<String> StringTable::LookupKey(Isolate* isolate, HashTableKey* key) {
  Handle<StringTable> table = isolate->factory()->string_table();
  int entry = table->FindEntry(key);

  // String already in table.
  if (entry != kNotFound) {
    return handle(String::cast(table->KeyAt(entry)), isolate);
  }

  // Adding new string. Grow table if needed.
  table = StringTable::EnsureCapacity(table, 1, key);

  // Create string object.
  Handle<Object> string = key->AsHandle(isolate);
  // There must be no attempts to internalize strings that could throw
  // InvalidStringLength error.
  CHECK(!string.is_null());

  // Add the new string and return it along with the string table.
  entry = table->FindInsertionEntry(key->Hash());
  table->set(EntryToIndex(entry), *string);
  table->ElementAdded();

  isolate->factory()->set_string_table(table);
  return Handle<String>::cast(string);
}


Handle<Object> CompilationCacheTable::Lookup(Handle<String> src,
                                             Handle<Context> context) {
  Isolate* isolate = GetIsolate();
  Handle<SharedFunctionInfo> shared(context->closure()->shared());
  StringSharedKey key(src, shared, FLAG_use_strict ? STRICT : SLOPPY,
                      RelocInfo::kNoPosition);
  int entry = FindEntry(&key);
  if (entry == kNotFound) return isolate->factory()->undefined_value();
  return Handle<Object>(get(EntryToIndex(entry) + 1), isolate);
}


Handle<Object> CompilationCacheTable::LookupEval(Handle<String> src,
                                                 Handle<Context> context,
                                                 StrictMode strict_mode,
                                                 int scope_position) {
  Isolate* isolate = GetIsolate();
  Handle<SharedFunctionInfo> shared(context->closure()->shared());
  StringSharedKey key(src, shared, strict_mode, scope_position);
  int entry = FindEntry(&key);
  if (entry == kNotFound) return isolate->factory()->undefined_value();
  return Handle<Object>(get(EntryToIndex(entry) + 1), isolate);
}


Handle<Object> CompilationCacheTable::LookupRegExp(Handle<String> src,
                                                   JSRegExp::Flags flags) {
  Isolate* isolate = GetIsolate();
  DisallowHeapAllocation no_allocation;
  RegExpKey key(src, flags);
  int entry = FindEntry(&key);
  if (entry == kNotFound) return isolate->factory()->undefined_value();
  return Handle<Object>(get(EntryToIndex(entry) + 1), isolate);
}


Handle<CompilationCacheTable> CompilationCacheTable::Put(
    Handle<CompilationCacheTable> cache, Handle<String> src,
    Handle<Context> context, Handle<Object> value) {
  Isolate* isolate = cache->GetIsolate();
  Handle<SharedFunctionInfo> shared(context->closure()->shared());
  StringSharedKey key(src, shared, FLAG_use_strict ? STRICT : SLOPPY,
                      RelocInfo::kNoPosition);
  cache = EnsureCapacity(cache, 1, &key);
  Handle<Object> k = key.AsHandle(isolate);
  int entry = cache->FindInsertionEntry(key.Hash());
  cache->set(EntryToIndex(entry), *k);
  cache->set(EntryToIndex(entry) + 1, *value);
  cache->ElementAdded();
  return cache;
}


Handle<CompilationCacheTable> CompilationCacheTable::PutEval(
    Handle<CompilationCacheTable> cache, Handle<String> src,
    Handle<Context> context, Handle<SharedFunctionInfo> value,
    int scope_position) {
  Isolate* isolate = cache->GetIsolate();
  Handle<SharedFunctionInfo> shared(context->closure()->shared());
  StringSharedKey key(src, shared, value->strict_mode(), scope_position);
  cache = EnsureCapacity(cache, 1, &key);
  Handle<Object> k = key.AsHandle(isolate);
  int entry = cache->FindInsertionEntry(key.Hash());
  cache->set(EntryToIndex(entry), *k);
  cache->set(EntryToIndex(entry) + 1, *value);
  cache->ElementAdded();
  return cache;
}


Handle<CompilationCacheTable> CompilationCacheTable::PutRegExp(
      Handle<CompilationCacheTable> cache, Handle<String> src,
      JSRegExp::Flags flags, Handle<FixedArray> value) {
  RegExpKey key(src, flags);
  cache = EnsureCapacity(cache, 1, &key);
  int entry = cache->FindInsertionEntry(key.Hash());
  // We store the value in the key slot, and compare the search key
  // to the stored value with a custon IsMatch function during lookups.
  cache->set(EntryToIndex(entry), *value);
  cache->set(EntryToIndex(entry) + 1, *value);
  cache->ElementAdded();
  return cache;
}


void CompilationCacheTable::Remove(Object* value) {
  DisallowHeapAllocation no_allocation;
  Object* the_hole_value = GetHeap()->the_hole_value();
  for (int entry = 0, size = Capacity(); entry < size; entry++) {
    int entry_index = EntryToIndex(entry);
    int value_index = entry_index + 1;
    if (get(value_index) == value) {
      NoWriteBarrierSet(this, entry_index, the_hole_value);
      NoWriteBarrierSet(this, value_index, the_hole_value);
      ElementRemoved();
    }
  }
  return;
}


// StringsKey used for HashTable where key is array of internalized strings.
class StringsKey : public HashTableKey {
 public:
  explicit StringsKey(Handle<FixedArray> strings) : strings_(strings) { }

  bool IsMatch(Object* strings) V8_OVERRIDE {
    FixedArray* o = FixedArray::cast(strings);
    int len = strings_->length();
    if (o->length() != len) return false;
    for (int i = 0; i < len; i++) {
      if (o->get(i) != strings_->get(i)) return false;
    }
    return true;
  }

  uint32_t Hash() V8_OVERRIDE { return HashForObject(*strings_); }

  uint32_t HashForObject(Object* obj) V8_OVERRIDE {
    FixedArray* strings = FixedArray::cast(obj);
    int len = strings->length();
    uint32_t hash = 0;
    for (int i = 0; i < len; i++) {
      hash ^= String::cast(strings->get(i))->Hash();
    }
    return hash;
  }

  Handle<Object> AsHandle(Isolate* isolate) V8_OVERRIDE { return strings_; }

 private:
  Handle<FixedArray> strings_;
};


Object* MapCache::Lookup(FixedArray* array) {
  DisallowHeapAllocation no_alloc;
  StringsKey key(handle(array));
  int entry = FindEntry(&key);
  if (entry == kNotFound) return GetHeap()->undefined_value();
  return get(EntryToIndex(entry) + 1);
}


Handle<MapCache> MapCache::Put(
    Handle<MapCache> map_cache, Handle<FixedArray> array, Handle<Map> value) {
  StringsKey key(array);

  Handle<MapCache> new_cache = EnsureCapacity(map_cache, 1, &key);
  int entry = new_cache->FindInsertionEntry(key.Hash());
  new_cache->set(EntryToIndex(entry), *array);
  new_cache->set(EntryToIndex(entry) + 1, *value);
  new_cache->ElementAdded();
  return new_cache;
}


template<typename Derived, typename Shape, typename Key>
Handle<Derived> Dictionary<Derived, Shape, Key>::New(
    Isolate* isolate,
    int at_least_space_for,
    PretenureFlag pretenure) {
  DCHECK(0 <= at_least_space_for);
  Handle<Derived> dict = DerivedHashTable::New(isolate,
                                               at_least_space_for,
                                               USE_DEFAULT_MINIMUM_CAPACITY,
                                               pretenure);

  // Initialize the next enumeration index.
  dict->SetNextEnumerationIndex(PropertyDetails::kInitialIndex);
  return dict;
}


template<typename Derived, typename Shape, typename Key>
void Dictionary<Derived, Shape, Key>::GenerateNewEnumerationIndices(
    Handle<Derived> dictionary) {
  Factory* factory = dictionary->GetIsolate()->factory();
  int length = dictionary->NumberOfElements();

  // Allocate and initialize iteration order array.
  Handle<FixedArray> iteration_order = factory->NewFixedArray(length);
  for (int i = 0; i < length; i++) {
    iteration_order->set(i, Smi::FromInt(i));
  }

  // Allocate array with enumeration order.
  Handle<FixedArray> enumeration_order = factory->NewFixedArray(length);

  // Fill the enumeration order array with property details.
  int capacity = dictionary->Capacity();
  int pos = 0;
  for (int i = 0; i < capacity; i++) {
    if (dictionary->IsKey(dictionary->KeyAt(i))) {
      int index = dictionary->DetailsAt(i).dictionary_index();
      enumeration_order->set(pos++, Smi::FromInt(index));
    }
  }

  // Sort the arrays wrt. enumeration order.
  iteration_order->SortPairs(*enumeration_order, enumeration_order->length());

  // Overwrite the enumeration_order with the enumeration indices.
  for (int i = 0; i < length; i++) {
    int index = Smi::cast(iteration_order->get(i))->value();
    int enum_index = PropertyDetails::kInitialIndex + i;
    enumeration_order->set(index, Smi::FromInt(enum_index));
  }

  // Update the dictionary with new indices.
  capacity = dictionary->Capacity();
  pos = 0;
  for (int i = 0; i < capacity; i++) {
    if (dictionary->IsKey(dictionary->KeyAt(i))) {
      int enum_index = Smi::cast(enumeration_order->get(pos++))->value();
      PropertyDetails details = dictionary->DetailsAt(i);
      PropertyDetails new_details = PropertyDetails(
          details.attributes(), details.type(), enum_index);
      dictionary->DetailsAtPut(i, new_details);
    }
  }

  // Set the next enumeration index.
  dictionary->SetNextEnumerationIndex(PropertyDetails::kInitialIndex+length);
}


template<typename Derived, typename Shape, typename Key>
Handle<Derived> Dictionary<Derived, Shape, Key>::EnsureCapacity(
    Handle<Derived> dictionary, int n, Key key) {
  // Check whether there are enough enumeration indices to add n elements.
  if (Shape::kIsEnumerable &&
      !PropertyDetails::IsValidIndex(dictionary->NextEnumerationIndex() + n)) {
    // If not, we generate new indices for the properties.
    GenerateNewEnumerationIndices(dictionary);
  }
  return DerivedHashTable::EnsureCapacity(dictionary, n, key);
}


template<typename Derived, typename Shape, typename Key>
Handle<Object> Dictionary<Derived, Shape, Key>::DeleteProperty(
    Handle<Derived> dictionary,
    int entry,
    JSObject::DeleteMode mode) {
  Factory* factory = dictionary->GetIsolate()->factory();
  PropertyDetails details = dictionary->DetailsAt(entry);
  // Ignore attributes if forcing a deletion.
  if (details.IsDontDelete() && mode != JSReceiver::FORCE_DELETION) {
    return factory->false_value();
  }

  dictionary->SetEntry(
      entry, factory->the_hole_value(), factory->the_hole_value());
  dictionary->ElementRemoved();
  return factory->true_value();
}


template<typename Derived, typename Shape, typename Key>
Handle<Derived> Dictionary<Derived, Shape, Key>::AtPut(
    Handle<Derived> dictionary, Key key, Handle<Object> value) {
  int entry = dictionary->FindEntry(key);

  // If the entry is present set the value;
  if (entry != Dictionary::kNotFound) {
    dictionary->ValueAtPut(entry, *value);
    return dictionary;
  }

  // Check whether the dictionary should be extended.
  dictionary = EnsureCapacity(dictionary, 1, key);
#ifdef DEBUG
  USE(Shape::AsHandle(dictionary->GetIsolate(), key));
#endif
  PropertyDetails details = PropertyDetails(NONE, NORMAL, 0);

  AddEntry(dictionary, key, value, details, dictionary->Hash(key));
  return dictionary;
}


template<typename Derived, typename Shape, typename Key>
Handle<Derived> Dictionary<Derived, Shape, Key>::Add(
    Handle<Derived> dictionary,
    Key key,
    Handle<Object> value,
    PropertyDetails details) {
  // Valdate key is absent.
  SLOW_DCHECK((dictionary->FindEntry(key) == Dictionary::kNotFound));
  // Check whether the dictionary should be extended.
  dictionary = EnsureCapacity(dictionary, 1, key);

  AddEntry(dictionary, key, value, details, dictionary->Hash(key));
  return dictionary;
}


// Add a key, value pair to the dictionary.
template<typename Derived, typename Shape, typename Key>
void Dictionary<Derived, Shape, Key>::AddEntry(
    Handle<Derived> dictionary,
    Key key,
    Handle<Object> value,
    PropertyDetails details,
    uint32_t hash) {
  // Compute the key object.
  Handle<Object> k = Shape::AsHandle(dictionary->GetIsolate(), key);

  uint32_t entry = dictionary->FindInsertionEntry(hash);
  // Insert element at empty or deleted entry
  if (!details.IsDeleted() &&
      details.dictionary_index() == 0 &&
      Shape::kIsEnumerable) {
    // Assign an enumeration index to the property and update
    // SetNextEnumerationIndex.
    int index = dictionary->NextEnumerationIndex();
    details = PropertyDetails(details.attributes(), details.type(), index);
    dictionary->SetNextEnumerationIndex(index + 1);
  }
  dictionary->SetEntry(entry, k, value, details);
  DCHECK((dictionary->KeyAt(entry)->IsNumber() ||
          dictionary->KeyAt(entry)->IsName()));
  dictionary->ElementAdded();
}


void SeededNumberDictionary::UpdateMaxNumberKey(uint32_t key) {
  DisallowHeapAllocation no_allocation;
  // If the dictionary requires slow elements an element has already
  // been added at a high index.
  if (requires_slow_elements()) return;
  // Check if this index is high enough that we should require slow
  // elements.
  if (key > kRequiresSlowElementsLimit) {
    set_requires_slow_elements();
    return;
  }
  // Update max key value.
  Object* max_index_object = get(kMaxNumberKeyIndex);
  if (!max_index_object->IsSmi() || max_number_key() < key) {
    FixedArray::set(kMaxNumberKeyIndex,
                    Smi::FromInt(key << kRequiresSlowElementsTagSize));
  }
}


Handle<SeededNumberDictionary> SeededNumberDictionary::AddNumberEntry(
    Handle<SeededNumberDictionary> dictionary,
    uint32_t key,
    Handle<Object> value,
    PropertyDetails details) {
  dictionary->UpdateMaxNumberKey(key);
  SLOW_DCHECK(dictionary->FindEntry(key) == kNotFound);
  return Add(dictionary, key, value, details);
}


Handle<UnseededNumberDictionary> UnseededNumberDictionary::AddNumberEntry(
    Handle<UnseededNumberDictionary> dictionary,
    uint32_t key,
    Handle<Object> value) {
  SLOW_DCHECK(dictionary->FindEntry(key) == kNotFound);
  return Add(dictionary, key, value, PropertyDetails(NONE, NORMAL, 0));
}


Handle<SeededNumberDictionary> SeededNumberDictionary::AtNumberPut(
    Handle<SeededNumberDictionary> dictionary,
    uint32_t key,
    Handle<Object> value) {
  dictionary->UpdateMaxNumberKey(key);
  return AtPut(dictionary, key, value);
}


Handle<UnseededNumberDictionary> UnseededNumberDictionary::AtNumberPut(
    Handle<UnseededNumberDictionary> dictionary,
    uint32_t key,
    Handle<Object> value) {
  return AtPut(dictionary, key, value);
}


Handle<SeededNumberDictionary> SeededNumberDictionary::Set(
    Handle<SeededNumberDictionary> dictionary,
    uint32_t key,
    Handle<Object> value,
    PropertyDetails details) {
  int entry = dictionary->FindEntry(key);
  if (entry == kNotFound) {
    return AddNumberEntry(dictionary, key, value, details);
  }
  // Preserve enumeration index.
  details = PropertyDetails(details.attributes(),
                            details.type(),
                            dictionary->DetailsAt(entry).dictionary_index());
  Handle<Object> object_key =
      SeededNumberDictionaryShape::AsHandle(dictionary->GetIsolate(), key);
  dictionary->SetEntry(entry, object_key, value, details);
  return dictionary;
}


Handle<UnseededNumberDictionary> UnseededNumberDictionary::Set(
    Handle<UnseededNumberDictionary> dictionary,
    uint32_t key,
    Handle<Object> value) {
  int entry = dictionary->FindEntry(key);
  if (entry == kNotFound) return AddNumberEntry(dictionary, key, value);
  Handle<Object> object_key =
      UnseededNumberDictionaryShape::AsHandle(dictionary->GetIsolate(), key);
  dictionary->SetEntry(entry, object_key, value);
  return dictionary;
}



template<typename Derived, typename Shape, typename Key>
int Dictionary<Derived, Shape, Key>::NumberOfElementsFilterAttributes(
    PropertyAttributes filter) {
  int capacity = DerivedHashTable::Capacity();
  int result = 0;
  for (int i = 0; i < capacity; i++) {
    Object* k = DerivedHashTable::KeyAt(i);
    if (DerivedHashTable::IsKey(k) && !FilterKey(k, filter)) {
      PropertyDetails details = DetailsAt(i);
      if (details.IsDeleted()) continue;
      PropertyAttributes attr = details.attributes();
      if ((attr & filter) == 0) result++;
    }
  }
  return result;
}


template<typename Derived, typename Shape, typename Key>
int Dictionary<Derived, Shape, Key>::NumberOfEnumElements() {
  return NumberOfElementsFilterAttributes(
      static_cast<PropertyAttributes>(DONT_ENUM | SYMBOLIC));
}


template<typename Derived, typename Shape, typename Key>
void Dictionary<Derived, Shape, Key>::CopyKeysTo(
    FixedArray* storage,
    PropertyAttributes filter,
    typename Dictionary<Derived, Shape, Key>::SortMode sort_mode) {
  DCHECK(storage->length() >= NumberOfElementsFilterAttributes(filter));
  int capacity = DerivedHashTable::Capacity();
  int index = 0;
  for (int i = 0; i < capacity; i++) {
     Object* k = DerivedHashTable::KeyAt(i);
     if (DerivedHashTable::IsKey(k) && !FilterKey(k, filter)) {
       PropertyDetails details = DetailsAt(i);
       if (details.IsDeleted()) continue;
       PropertyAttributes attr = details.attributes();
       if ((attr & filter) == 0) storage->set(index++, k);
     }
  }
  if (sort_mode == Dictionary::SORTED) {
    storage->SortPairs(storage, index);
  }
  DCHECK(storage->length() >= index);
}


struct EnumIndexComparator {
  explicit EnumIndexComparator(NameDictionary* dict) : dict(dict) { }
  bool operator() (Smi* a, Smi* b) {
    PropertyDetails da(dict->DetailsAt(a->value()));
    PropertyDetails db(dict->DetailsAt(b->value()));
    return da.dictionary_index() < db.dictionary_index();
  }
  NameDictionary* dict;
};


void NameDictionary::CopyEnumKeysTo(FixedArray* storage) {
  int length = storage->length();
  int capacity = Capacity();
  int properties = 0;
  for (int i = 0; i < capacity; i++) {
     Object* k = KeyAt(i);
     if (IsKey(k) && !k->IsSymbol()) {
       PropertyDetails details = DetailsAt(i);
       if (details.IsDeleted() || details.IsDontEnum()) continue;
       storage->set(properties, Smi::FromInt(i));
       properties++;
       if (properties == length) break;
     }
  }
  CHECK_EQ(length, properties);
  EnumIndexComparator cmp(this);
  Smi** start = reinterpret_cast<Smi**>(storage->GetFirstElementAddress());
  std::sort(start, start + length, cmp);
  for (int i = 0; i < length; i++) {
    int index = Smi::cast(storage->get(i))->value();
    storage->set(i, KeyAt(index));
  }
}


template<typename Derived, typename Shape, typename Key>
void Dictionary<Derived, Shape, Key>::CopyKeysTo(
    FixedArray* storage,
    int index,
    PropertyAttributes filter,
    typename Dictionary<Derived, Shape, Key>::SortMode sort_mode) {
  DCHECK(storage->length() >= NumberOfElementsFilterAttributes(filter));
  int capacity = DerivedHashTable::Capacity();
  for (int i = 0; i < capacity; i++) {
    Object* k = DerivedHashTable::KeyAt(i);
    if (DerivedHashTable::IsKey(k) && !FilterKey(k, filter)) {
      PropertyDetails details = DetailsAt(i);
      if (details.IsDeleted()) continue;
      PropertyAttributes attr = details.attributes();
      if ((attr & filter) == 0) storage->set(index++, k);
    }
  }
  if (sort_mode == Dictionary::SORTED) {
    storage->SortPairs(storage, index);
  }
  DCHECK(storage->length() >= index);
}


// Backwards lookup (slow).
template<typename Derived, typename Shape, typename Key>
Object* Dictionary<Derived, Shape, Key>::SlowReverseLookup(Object* value) {
  int capacity = DerivedHashTable::Capacity();
  for (int i = 0; i < capacity; i++) {
    Object* k =  DerivedHashTable::KeyAt(i);
    if (Dictionary::IsKey(k)) {
      Object* e = ValueAt(i);
      if (e->IsPropertyCell()) {
        e = PropertyCell::cast(e)->value();
      }
      if (e == value) return k;
    }
  }
  Heap* heap = Dictionary::GetHeap();
  return heap->undefined_value();
}


Object* ObjectHashTable::Lookup(Handle<Object> key) {
  DisallowHeapAllocation no_gc;
  DCHECK(IsKey(*key));

  // If the object does not have an identity hash, it was never used as a key.
  Object* hash = key->GetHash();
  if (hash->IsUndefined()) {
    return GetHeap()->the_hole_value();
  }
  int entry = FindEntry(key);
  if (entry == kNotFound) return GetHeap()->the_hole_value();
  return get(EntryToIndex(entry) + 1);
}


Handle<ObjectHashTable> ObjectHashTable::Put(Handle<ObjectHashTable> table,
                                             Handle<Object> key,
                                             Handle<Object> value) {
  DCHECK(table->IsKey(*key));
  DCHECK(!value->IsTheHole());

  Isolate* isolate = table->GetIsolate();

  // Make sure the key object has an identity hash code.
  Handle<Smi> hash = Object::GetOrCreateHash(isolate, key);

  int entry = table->FindEntry(key);

  // Key is already in table, just overwrite value.
  if (entry != kNotFound) {
    table->set(EntryToIndex(entry) + 1, *value);
    return table;
  }

  // Check whether the hash table should be extended.
  table = EnsureCapacity(table, 1, key);
  table->AddEntry(table->FindInsertionEntry(hash->value()),
                  *key,
                  *value);
  return table;
}


Handle<ObjectHashTable> ObjectHashTable::Remove(Handle<ObjectHashTable> table,
                                                Handle<Object> key,
                                                bool* was_present) {
  DCHECK(table->IsKey(*key));

  Object* hash = key->GetHash();
  if (hash->IsUndefined()) {
    *was_present = false;
    return table;
  }

  int entry = table->FindEntry(key);
  if (entry == kNotFound) {
    *was_present = false;
    return table;
  }

  *was_present = true;
  table->RemoveEntry(entry);
  return Shrink(table, key);
}


void ObjectHashTable::AddEntry(int entry, Object* key, Object* value) {
  set(EntryToIndex(entry), key);
  set(EntryToIndex(entry) + 1, value);
  ElementAdded();
}


void ObjectHashTable::RemoveEntry(int entry) {
  set_the_hole(EntryToIndex(entry));
  set_the_hole(EntryToIndex(entry) + 1);
  ElementRemoved();
}


Object* WeakHashTable::Lookup(Handle<Object> key) {
  DisallowHeapAllocation no_gc;
  DCHECK(IsKey(*key));
  int entry = FindEntry(key);
  if (entry == kNotFound) return GetHeap()->the_hole_value();
  return get(EntryToValueIndex(entry));
}


Handle<WeakHashTable> WeakHashTable::Put(Handle<WeakHashTable> table,
                                         Handle<Object> key,
                                         Handle<Object> value) {
  DCHECK(table->IsKey(*key));
  int entry = table->FindEntry(key);
  // Key is already in table, just overwrite value.
  if (entry != kNotFound) {
    // TODO(ulan): Skipping write barrier is a temporary solution to avoid
    // memory leaks. Remove this once we have special visitor for weak fixed
    // arrays.
    table->set(EntryToValueIndex(entry), *value, SKIP_WRITE_BARRIER);
    return table;
  }

  // Check whether the hash table should be extended.
  table = EnsureCapacity(table, 1, key, TENURED);

  table->AddEntry(table->FindInsertionEntry(table->Hash(key)), key, value);
  return table;
}


void WeakHashTable::AddEntry(int entry,
                             Handle<Object> key,
                             Handle<Object> value) {
  DisallowHeapAllocation no_allocation;
  // TODO(ulan): Skipping write barrier is a temporary solution to avoid
  // memory leaks. Remove this once we have special visitor for weak fixed
  // arrays.
  set(EntryToIndex(entry), *key, SKIP_WRITE_BARRIER);
  set(EntryToValueIndex(entry), *value, SKIP_WRITE_BARRIER);
  ElementAdded();
}


template<class Derived, class Iterator, int entrysize>
Handle<Derived> OrderedHashTable<Derived, Iterator, entrysize>::Allocate(
    Isolate* isolate, int capacity, PretenureFlag pretenure) {
  // Capacity must be a power of two, since we depend on being able
  // to divide and multiple by 2 (kLoadFactor) to derive capacity
  // from number of buckets. If we decide to change kLoadFactor
  // to something other than 2, capacity should be stored as another
  // field of this object.
  capacity = RoundUpToPowerOf2(Max(kMinCapacity, capacity));
  if (capacity > kMaxCapacity) {
    v8::internal::Heap::FatalProcessOutOfMemory("invalid table size", true);
  }
  int num_buckets = capacity / kLoadFactor;
  Handle<FixedArray> backing_store = isolate->factory()->NewFixedArray(
      kHashTableStartIndex + num_buckets + (capacity * kEntrySize), pretenure);
  backing_store->set_map_no_write_barrier(
      isolate->heap()->ordered_hash_table_map());
  Handle<Derived> table = Handle<Derived>::cast(backing_store);
  for (int i = 0; i < num_buckets; ++i) {
    table->set(kHashTableStartIndex + i, Smi::FromInt(kNotFound));
  }
  table->SetNumberOfBuckets(num_buckets);
  table->SetNumberOfElements(0);
  table->SetNumberOfDeletedElements(0);
  return table;
}


template<class Derived, class Iterator, int entrysize>
Handle<Derived> OrderedHashTable<Derived, Iterator, entrysize>::EnsureGrowable(
    Handle<Derived> table) {
  DCHECK(!table->IsObsolete());

  int nof = table->NumberOfElements();
  int nod = table->NumberOfDeletedElements();
  int capacity = table->Capacity();
  if ((nof + nod) < capacity) return table;
  // Don't need to grow if we can simply clear out deleted entries instead.
  // Note that we can't compact in place, though, so we always allocate
  // a new table.
  return Rehash(table, (nod < (capacity >> 1)) ? capacity << 1 : capacity);
}


template<class Derived, class Iterator, int entrysize>
Handle<Derived> OrderedHashTable<Derived, Iterator, entrysize>::Shrink(
    Handle<Derived> table) {
  DCHECK(!table->IsObsolete());

  int nof = table->NumberOfElements();
  int capacity = table->Capacity();
  if (nof >= (capacity >> 2)) return table;
  return Rehash(table, capacity / 2);
}


template<class Derived, class Iterator, int entrysize>
Handle<Derived> OrderedHashTable<Derived, Iterator, entrysize>::Clear(
    Handle<Derived> table) {
  DCHECK(!table->IsObsolete());

  Handle<Derived> new_table =
      Allocate(table->GetIsolate(),
               kMinCapacity,
               table->GetHeap()->InNewSpace(*table) ? NOT_TENURED : TENURED);

  table->SetNextTable(*new_table);
  table->SetNumberOfDeletedElements(-1);

  return new_table;
}


template<class Derived, class Iterator, int entrysize>
Handle<Derived> OrderedHashTable<Derived, Iterator, entrysize>::Remove(
    Handle<Derived> table, Handle<Object> key, bool* was_present) {
  int entry = table->FindEntry(key);
  if (entry == kNotFound) {
    *was_present = false;
    return table;
  }
  *was_present = true;
  table->RemoveEntry(entry);
  return Shrink(table);
}


template<class Derived, class Iterator, int entrysize>
Handle<Derived> OrderedHashTable<Derived, Iterator, entrysize>::Rehash(
    Handle<Derived> table, int new_capacity) {
  DCHECK(!table->IsObsolete());

  Handle<Derived> new_table =
      Allocate(table->GetIsolate(),
               new_capacity,
               table->GetHeap()->InNewSpace(*table) ? NOT_TENURED : TENURED);
  int nof = table->NumberOfElements();
  int nod = table->NumberOfDeletedElements();
  int new_buckets = new_table->NumberOfBuckets();
  int new_entry = 0;
  int removed_holes_index = 0;

  for (int old_entry = 0; old_entry < (nof + nod); ++old_entry) {
    Object* key = table->KeyAt(old_entry);
    if (key->IsTheHole()) {
      table->SetRemovedIndexAt(removed_holes_index++, old_entry);
      continue;
    }

    Object* hash = key->GetHash();
    int bucket = Smi::cast(hash)->value() & (new_buckets - 1);
    Object* chain_entry = new_table->get(kHashTableStartIndex + bucket);
    new_table->set(kHashTableStartIndex + bucket, Smi::FromInt(new_entry));
    int new_index = new_table->EntryToIndex(new_entry);
    int old_index = table->EntryToIndex(old_entry);
    for (int i = 0; i < entrysize; ++i) {
      Object* value = table->get(old_index + i);
      new_table->set(new_index + i, value);
    }
    new_table->set(new_index + kChainOffset, chain_entry);
    ++new_entry;
  }

  DCHECK_EQ(nod, removed_holes_index);

  new_table->SetNumberOfElements(nof);
  table->SetNextTable(*new_table);

  return new_table;
}


template <class Derived, class Iterator, int entrysize>
int OrderedHashTable<Derived, Iterator, entrysize>::FindEntry(
    Handle<Object> key, int hash) {
  DCHECK(!IsObsolete());

  DisallowHeapAllocation no_gc;
  DCHECK(!key->IsTheHole());
  for (int entry = HashToEntry(hash); entry != kNotFound;
       entry = ChainAt(entry)) {
    Object* candidate = KeyAt(entry);
    if (candidate->SameValueZero(*key))
      return entry;
  }
  return kNotFound;
}


template <class Derived, class Iterator, int entrysize>
int OrderedHashTable<Derived, Iterator, entrysize>::FindEntry(
    Handle<Object> key) {
  DisallowHeapAllocation no_gc;
  Object* hash = key->GetHash();
  if (!hash->IsSmi()) return kNotFound;
  return FindEntry(key, Smi::cast(hash)->value());
}


template <class Derived, class Iterator, int entrysize>
int OrderedHashTable<Derived, Iterator, entrysize>::AddEntry(int hash) {
  DCHECK(!IsObsolete());

  int entry = UsedCapacity();
  int bucket = HashToBucket(hash);
  int index = EntryToIndex(entry);
  Object* chain_entry = get(kHashTableStartIndex + bucket);
  set(kHashTableStartIndex + bucket, Smi::FromInt(entry));
  set(index + kChainOffset, chain_entry);
  SetNumberOfElements(NumberOfElements() + 1);
  return index;
}


template<class Derived, class Iterator, int entrysize>
void OrderedHashTable<Derived, Iterator, entrysize>::RemoveEntry(int entry) {
  DCHECK(!IsObsolete());

  int index = EntryToIndex(entry);
  for (int i = 0; i < entrysize; ++i) {
    set_the_hole(index + i);
  }
  SetNumberOfElements(NumberOfElements() - 1);
  SetNumberOfDeletedElements(NumberOfDeletedElements() + 1);
}


template Handle<OrderedHashSet>
OrderedHashTable<OrderedHashSet, JSSetIterator, 1>::Allocate(
    Isolate* isolate, int capacity, PretenureFlag pretenure);

template Handle<OrderedHashSet>
OrderedHashTable<OrderedHashSet, JSSetIterator, 1>::EnsureGrowable(
    Handle<OrderedHashSet> table);

template Handle<OrderedHashSet>
OrderedHashTable<OrderedHashSet, JSSetIterator, 1>::Shrink(
    Handle<OrderedHashSet> table);

template Handle<OrderedHashSet>
OrderedHashTable<OrderedHashSet, JSSetIterator, 1>::Clear(
    Handle<OrderedHashSet> table);

template Handle<OrderedHashSet>
OrderedHashTable<OrderedHashSet, JSSetIterator, 1>::Remove(
    Handle<OrderedHashSet> table, Handle<Object> key, bool* was_present);

template int OrderedHashTable<OrderedHashSet, JSSetIterator, 1>::FindEntry(
    Handle<Object> key, int hash);
template int OrderedHashTable<OrderedHashSet, JSSetIterator, 1>::FindEntry(
    Handle<Object> key);

template int
OrderedHashTable<OrderedHashSet, JSSetIterator, 1>::AddEntry(int hash);

template void
OrderedHashTable<OrderedHashSet, JSSetIterator, 1>::RemoveEntry(int entry);


template Handle<OrderedHashMap>
OrderedHashTable<OrderedHashMap, JSMapIterator, 2>::Allocate(
    Isolate* isolate, int capacity, PretenureFlag pretenure);

template Handle<OrderedHashMap>
OrderedHashTable<OrderedHashMap, JSMapIterator, 2>::EnsureGrowable(
    Handle<OrderedHashMap> table);

template Handle<OrderedHashMap>
OrderedHashTable<OrderedHashMap, JSMapIterator, 2>::Shrink(
    Handle<OrderedHashMap> table);

template Handle<OrderedHashMap>
OrderedHashTable<OrderedHashMap, JSMapIterator, 2>::Clear(
    Handle<OrderedHashMap> table);

template Handle<OrderedHashMap>
OrderedHashTable<OrderedHashMap, JSMapIterator, 2>::Remove(
    Handle<OrderedHashMap> table, Handle<Object> key, bool* was_present);

template int OrderedHashTable<OrderedHashMap, JSMapIterator, 2>::FindEntry(
    Handle<Object> key, int hash);
template int OrderedHashTable<OrderedHashMap, JSMapIterator, 2>::FindEntry(
    Handle<Object> key);

template int
OrderedHashTable<OrderedHashMap, JSMapIterator, 2>::AddEntry(int hash);

template void
OrderedHashTable<OrderedHashMap, JSMapIterator, 2>::RemoveEntry(int entry);


bool OrderedHashSet::Contains(Handle<Object> key) {
  return FindEntry(key) != kNotFound;
}


Handle<OrderedHashSet> OrderedHashSet::Add(Handle<OrderedHashSet> table,
                                           Handle<Object> key) {
  int hash = GetOrCreateHash(table->GetIsolate(), key)->value();
  if (table->FindEntry(key, hash) != kNotFound) return table;

  table = EnsureGrowable(table);

  int index = table->AddEntry(hash);
  table->set(index, *key);
  return table;
}


Object* OrderedHashMap::Lookup(Handle<Object> key) {
  DisallowHeapAllocation no_gc;
  int entry = FindEntry(key);
  if (entry == kNotFound) return GetHeap()->the_hole_value();
  return ValueAt(entry);
}


Handle<OrderedHashMap> OrderedHashMap::Put(Handle<OrderedHashMap> table,
                                           Handle<Object> key,
                                           Handle<Object> value) {
  DCHECK(!key->IsTheHole());

  int hash = GetOrCreateHash(table->GetIsolate(), key)->value();
  int entry = table->FindEntry(key, hash);

  if (entry != kNotFound) {
    table->set(table->EntryToIndex(entry) + kValueOffset, *value);
    return table;
  }

  table = EnsureGrowable(table);

  int index = table->AddEntry(hash);
  table->set(index, *key);
  table->set(index + kValueOffset, *value);
  return table;
}


template<class Derived, class TableType>
void OrderedHashTableIterator<Derived, TableType>::Transition() {
  DisallowHeapAllocation no_allocation;
  TableType* table = TableType::cast(this->table());
  if (!table->IsObsolete()) return;

  int index = Smi::cast(this->index())->value();
  while (table->IsObsolete()) {
    TableType* next_table = table->NextTable();

    if (index > 0) {
      int nod = table->NumberOfDeletedElements();

      // When we clear the table we set the number of deleted elements to -1.
      if (nod == -1) {
        index = 0;
      } else {
        int old_index = index;
        for (int i = 0; i < nod; ++i) {
          int removed_index = table->RemovedIndexAt(i);
          if (removed_index >= old_index) break;
          --index;
        }
      }
    }

    table = next_table;
  }

  set_table(table);
  set_index(Smi::FromInt(index));
}


template<class Derived, class TableType>
bool OrderedHashTableIterator<Derived, TableType>::HasMore() {
  DisallowHeapAllocation no_allocation;
  if (this->table()->IsUndefined()) return false;

  Transition();

  TableType* table = TableType::cast(this->table());
  int index = Smi::cast(this->index())->value();
  int used_capacity = table->UsedCapacity();

  while (index < used_capacity && table->KeyAt(index)->IsTheHole()) {
    index++;
  }

  set_index(Smi::FromInt(index));

  if (index < used_capacity) return true;

  set_table(GetHeap()->undefined_value());
  return false;
}


template<class Derived, class TableType>
Smi* OrderedHashTableIterator<Derived, TableType>::Next(JSArray* value_array) {
  DisallowHeapAllocation no_allocation;
  if (HasMore()) {
    FixedArray* array = FixedArray::cast(value_array->elements());
    static_cast<Derived*>(this)->PopulateValueArray(array);
    MoveNext();
    return kind();
  }
  return Smi::FromInt(0);
}


template Smi*
OrderedHashTableIterator<JSSetIterator, OrderedHashSet>::Next(
    JSArray* value_array);

template bool
OrderedHashTableIterator<JSSetIterator, OrderedHashSet>::HasMore();

template void
OrderedHashTableIterator<JSSetIterator, OrderedHashSet>::MoveNext();

template Object*
OrderedHashTableIterator<JSSetIterator, OrderedHashSet>::CurrentKey();

template void
OrderedHashTableIterator<JSSetIterator, OrderedHashSet>::Transition();


template Smi*
OrderedHashTableIterator<JSMapIterator, OrderedHashMap>::Next(
    JSArray* value_array);

template bool
OrderedHashTableIterator<JSMapIterator, OrderedHashMap>::HasMore();

template void
OrderedHashTableIterator<JSMapIterator, OrderedHashMap>::MoveNext();

template Object*
OrderedHashTableIterator<JSMapIterator, OrderedHashMap>::CurrentKey();

template void
OrderedHashTableIterator<JSMapIterator, OrderedHashMap>::Transition();


DeclaredAccessorDescriptorIterator::DeclaredAccessorDescriptorIterator(
    DeclaredAccessorDescriptor* descriptor)
    : array_(descriptor->serialized_data()->GetDataStartAddress()),
      length_(descriptor->serialized_data()->length()),
      offset_(0) {
}


const DeclaredAccessorDescriptorData*
  DeclaredAccessorDescriptorIterator::Next() {
  DCHECK(offset_ < length_);
  uint8_t* ptr = &array_[offset_];
  DCHECK(reinterpret_cast<uintptr_t>(ptr) % sizeof(uintptr_t) == 0);
  const DeclaredAccessorDescriptorData* data =
      reinterpret_cast<const DeclaredAccessorDescriptorData*>(ptr);
  offset_ += sizeof(*data);
  DCHECK(offset_ <= length_);
  return data;
}


Handle<DeclaredAccessorDescriptor> DeclaredAccessorDescriptor::Create(
    Isolate* isolate,
    const DeclaredAccessorDescriptorData& descriptor,
    Handle<DeclaredAccessorDescriptor> previous) {
  int previous_length =
      previous.is_null() ? 0 : previous->serialized_data()->length();
  int length = sizeof(descriptor) + previous_length;
  Handle<ByteArray> serialized_descriptor =
      isolate->factory()->NewByteArray(length);
  Handle<DeclaredAccessorDescriptor> value =
      isolate->factory()->NewDeclaredAccessorDescriptor();
  value->set_serialized_data(*serialized_descriptor);
  // Copy in the data.
  {
    DisallowHeapAllocation no_allocation;
    uint8_t* array = serialized_descriptor->GetDataStartAddress();
    if (previous_length != 0) {
      uint8_t* previous_array =
          previous->serialized_data()->GetDataStartAddress();
      MemCopy(array, previous_array, previous_length);
      array += previous_length;
    }
    DCHECK(reinterpret_cast<uintptr_t>(array) % sizeof(uintptr_t) == 0);
    DeclaredAccessorDescriptorData* data =
        reinterpret_cast<DeclaredAccessorDescriptorData*>(array);
    *data = descriptor;
  }
  return value;
}


// Check if there is a break point at this code position.
bool DebugInfo::HasBreakPoint(int code_position) {
  // Get the break point info object for this code position.
  Object* break_point_info = GetBreakPointInfo(code_position);

  // If there is no break point info object or no break points in the break
  // point info object there is no break point at this code position.
  if (break_point_info->IsUndefined()) return false;
  return BreakPointInfo::cast(break_point_info)->GetBreakPointCount() > 0;
}


// Get the break point info object for this code position.
Object* DebugInfo::GetBreakPointInfo(int code_position) {
  // Find the index of the break point info object for this code position.
  int index = GetBreakPointInfoIndex(code_position);

  // Return the break point info object if any.
  if (index == kNoBreakPointInfo) return GetHeap()->undefined_value();
  return BreakPointInfo::cast(break_points()->get(index));
}


// Clear a break point at the specified code position.
void DebugInfo::ClearBreakPoint(Handle<DebugInfo> debug_info,
                                int code_position,
                                Handle<Object> break_point_object) {
  Handle<Object> break_point_info(debug_info->GetBreakPointInfo(code_position),
                                  debug_info->GetIsolate());
  if (break_point_info->IsUndefined()) return;
  BreakPointInfo::ClearBreakPoint(
      Handle<BreakPointInfo>::cast(break_point_info),
      break_point_object);
}


void DebugInfo::SetBreakPoint(Handle<DebugInfo> debug_info,
                              int code_position,
                              int source_position,
                              int statement_position,
                              Handle<Object> break_point_object) {
  Isolate* isolate = debug_info->GetIsolate();
  Handle<Object> break_point_info(debug_info->GetBreakPointInfo(code_position),
                                  isolate);
  if (!break_point_info->IsUndefined()) {
    BreakPointInfo::SetBreakPoint(
        Handle<BreakPointInfo>::cast(break_point_info),
        break_point_object);
    return;
  }

  // Adding a new break point for a code position which did not have any
  // break points before. Try to find a free slot.
  int index = kNoBreakPointInfo;
  for (int i = 0; i < debug_info->break_points()->length(); i++) {
    if (debug_info->break_points()->get(i)->IsUndefined()) {
      index = i;
      break;
    }
  }
  if (index == kNoBreakPointInfo) {
    // No free slot - extend break point info array.
    Handle<FixedArray> old_break_points =
        Handle<FixedArray>(FixedArray::cast(debug_info->break_points()));
    Handle<FixedArray> new_break_points =
        isolate->factory()->NewFixedArray(
            old_break_points->length() +
            DebugInfo::kEstimatedNofBreakPointsInFunction);

    debug_info->set_break_points(*new_break_points);
    for (int i = 0; i < old_break_points->length(); i++) {
      new_break_points->set(i, old_break_points->get(i));
    }
    index = old_break_points->length();
  }
  DCHECK(index != kNoBreakPointInfo);

  // Allocate new BreakPointInfo object and set the break point.
  Handle<BreakPointInfo> new_break_point_info = Handle<BreakPointInfo>::cast(
      isolate->factory()->NewStruct(BREAK_POINT_INFO_TYPE));
  new_break_point_info->set_code_position(Smi::FromInt(code_position));
  new_break_point_info->set_source_position(Smi::FromInt(source_position));
  new_break_point_info->
      set_statement_position(Smi::FromInt(statement_position));
  new_break_point_info->set_break_point_objects(
      isolate->heap()->undefined_value());
  BreakPointInfo::SetBreakPoint(new_break_point_info, break_point_object);
  debug_info->break_points()->set(index, *new_break_point_info);
}


// Get the break point objects for a code position.
Object* DebugInfo::GetBreakPointObjects(int code_position) {
  Object* break_point_info = GetBreakPointInfo(code_position);
  if (break_point_info->IsUndefined()) {
    return GetHeap()->undefined_value();
  }
  return BreakPointInfo::cast(break_point_info)->break_point_objects();
}


// Get the total number of break points.
int DebugInfo::GetBreakPointCount() {
  if (break_points()->IsUndefined()) return 0;
  int count = 0;
  for (int i = 0; i < break_points()->length(); i++) {
    if (!break_points()->get(i)->IsUndefined()) {
      BreakPointInfo* break_point_info =
          BreakPointInfo::cast(break_points()->get(i));
      count += break_point_info->GetBreakPointCount();
    }
  }
  return count;
}


Object* DebugInfo::FindBreakPointInfo(Handle<DebugInfo> debug_info,
                                      Handle<Object> break_point_object) {
  Heap* heap = debug_info->GetHeap();
  if (debug_info->break_points()->IsUndefined()) return heap->undefined_value();
  for (int i = 0; i < debug_info->break_points()->length(); i++) {
    if (!debug_info->break_points()->get(i)->IsUndefined()) {
      Handle<BreakPointInfo> break_point_info =
          Handle<BreakPointInfo>(BreakPointInfo::cast(
              debug_info->break_points()->get(i)));
      if (BreakPointInfo::HasBreakPointObject(break_point_info,
                                              break_point_object)) {
        return *break_point_info;
      }
    }
  }
  return heap->undefined_value();
}


// Find the index of the break point info object for the specified code
// position.
int DebugInfo::GetBreakPointInfoIndex(int code_position) {
  if (break_points()->IsUndefined()) return kNoBreakPointInfo;
  for (int i = 0; i < break_points()->length(); i++) {
    if (!break_points()->get(i)->IsUndefined()) {
      BreakPointInfo* break_point_info =
          BreakPointInfo::cast(break_points()->get(i));
      if (break_point_info->code_position()->value() == code_position) {
        return i;
      }
    }
  }
  return kNoBreakPointInfo;
}


// Remove the specified break point object.
void BreakPointInfo::ClearBreakPoint(Handle<BreakPointInfo> break_point_info,
                                     Handle<Object> break_point_object) {
  Isolate* isolate = break_point_info->GetIsolate();
  // If there are no break points just ignore.
  if (break_point_info->break_point_objects()->IsUndefined()) return;
  // If there is a single break point clear it if it is the same.
  if (!break_point_info->break_point_objects()->IsFixedArray()) {
    if (break_point_info->break_point_objects() == *break_point_object) {
      break_point_info->set_break_point_objects(
          isolate->heap()->undefined_value());
    }
    return;
  }
  // If there are multiple break points shrink the array
  DCHECK(break_point_info->break_point_objects()->IsFixedArray());
  Handle<FixedArray> old_array =
      Handle<FixedArray>(
          FixedArray::cast(break_point_info->break_point_objects()));
  Handle<FixedArray> new_array =
      isolate->factory()->NewFixedArray(old_array->length() - 1);
  int found_count = 0;
  for (int i = 0; i < old_array->length(); i++) {
    if (old_array->get(i) == *break_point_object) {
      DCHECK(found_count == 0);
      found_count++;
    } else {
      new_array->set(i - found_count, old_array->get(i));
    }
  }
  // If the break point was found in the list change it.
  if (found_count > 0) break_point_info->set_break_point_objects(*new_array);
}


// Add the specified break point object.
void BreakPointInfo::SetBreakPoint(Handle<BreakPointInfo> break_point_info,
                                   Handle<Object> break_point_object) {
  Isolate* isolate = break_point_info->GetIsolate();

  // If there was no break point objects before just set it.
  if (break_point_info->break_point_objects()->IsUndefined()) {
    break_point_info->set_break_point_objects(*break_point_object);
    return;
  }
  // If the break point object is the same as before just ignore.
  if (break_point_info->break_point_objects() == *break_point_object) return;
  // If there was one break point object before replace with array.
  if (!break_point_info->break_point_objects()->IsFixedArray()) {
    Handle<FixedArray> array = isolate->factory()->NewFixedArray(2);
    array->set(0, break_point_info->break_point_objects());
    array->set(1, *break_point_object);
    break_point_info->set_break_point_objects(*array);
    return;
  }
  // If there was more than one break point before extend array.
  Handle<FixedArray> old_array =
      Handle<FixedArray>(
          FixedArray::cast(break_point_info->break_point_objects()));
  Handle<FixedArray> new_array =
      isolate->factory()->NewFixedArray(old_array->length() + 1);
  for (int i = 0; i < old_array->length(); i++) {
    // If the break point was there before just ignore.
    if (old_array->get(i) == *break_point_object) return;
    new_array->set(i, old_array->get(i));
  }
  // Add the new break point.
  new_array->set(old_array->length(), *break_point_object);
  break_point_info->set_break_point_objects(*new_array);
}


bool BreakPointInfo::HasBreakPointObject(
    Handle<BreakPointInfo> break_point_info,
    Handle<Object> break_point_object) {
  // No break point.
  if (break_point_info->break_point_objects()->IsUndefined()) return false;
  // Single break point.
  if (!break_point_info->break_point_objects()->IsFixedArray()) {
    return break_point_info->break_point_objects() == *break_point_object;
  }
  // Multiple break points.
  FixedArray* array = FixedArray::cast(break_point_info->break_point_objects());
  for (int i = 0; i < array->length(); i++) {
    if (array->get(i) == *break_point_object) {
      return true;
    }
  }
  return false;
}


// Get the number of break points.
int BreakPointInfo::GetBreakPointCount() {
  // No break point.
  if (break_point_objects()->IsUndefined()) return 0;
  // Single break point.
  if (!break_point_objects()->IsFixedArray()) return 1;
  // Multiple break points.
  return FixedArray::cast(break_point_objects())->length();
}


Object* JSDate::GetField(Object* object, Smi* index) {
  return JSDate::cast(object)->DoGetField(
      static_cast<FieldIndex>(index->value()));
}


Object* JSDate::DoGetField(FieldIndex index) {
  DCHECK(index != kDateValue);

  DateCache* date_cache = GetIsolate()->date_cache();

  if (index < kFirstUncachedField) {
    Object* stamp = cache_stamp();
    if (stamp != date_cache->stamp() && stamp->IsSmi()) {
      // Since the stamp is not NaN, the value is also not NaN.
      int64_t local_time_ms =
          date_cache->ToLocal(static_cast<int64_t>(value()->Number()));
      SetCachedFields(local_time_ms, date_cache);
    }
    switch (index) {
      case kYear: return year();
      case kMonth: return month();
      case kDay: return day();
      case kWeekday: return weekday();
      case kHour: return hour();
      case kMinute: return min();
      case kSecond: return sec();
      default: UNREACHABLE();
    }
  }

  if (index >= kFirstUTCField) {
    return GetUTCField(index, value()->Number(), date_cache);
  }

  double time = value()->Number();
  if (std::isnan(time)) return GetIsolate()->heap()->nan_value();

  int64_t local_time_ms = date_cache->ToLocal(static_cast<int64_t>(time));
  int days = DateCache::DaysFromTime(local_time_ms);

  if (index == kDays) return Smi::FromInt(days);

  int time_in_day_ms = DateCache::TimeInDay(local_time_ms, days);
  if (index == kMillisecond) return Smi::FromInt(time_in_day_ms % 1000);
  DCHECK(index == kTimeInDay);
  return Smi::FromInt(time_in_day_ms);
}


Object* JSDate::GetUTCField(FieldIndex index,
                            double value,
                            DateCache* date_cache) {
  DCHECK(index >= kFirstUTCField);

  if (std::isnan(value)) return GetIsolate()->heap()->nan_value();

  int64_t time_ms = static_cast<int64_t>(value);

  if (index == kTimezoneOffset) {
    return Smi::FromInt(date_cache->TimezoneOffset(time_ms));
  }

  int days = DateCache::DaysFromTime(time_ms);

  if (index == kWeekdayUTC) return Smi::FromInt(date_cache->Weekday(days));

  if (index <= kDayUTC) {
    int year, month, day;
    date_cache->YearMonthDayFromDays(days, &year, &month, &day);
    if (index == kYearUTC) return Smi::FromInt(year);
    if (index == kMonthUTC) return Smi::FromInt(month);
    DCHECK(index == kDayUTC);
    return Smi::FromInt(day);
  }

  int time_in_day_ms = DateCache::TimeInDay(time_ms, days);
  switch (index) {
    case kHourUTC: return Smi::FromInt(time_in_day_ms / (60 * 60 * 1000));
    case kMinuteUTC: return Smi::FromInt((time_in_day_ms / (60 * 1000)) % 60);
    case kSecondUTC: return Smi::FromInt((time_in_day_ms / 1000) % 60);
    case kMillisecondUTC: return Smi::FromInt(time_in_day_ms % 1000);
    case kDaysUTC: return Smi::FromInt(days);
    case kTimeInDayUTC: return Smi::FromInt(time_in_day_ms);
    default: UNREACHABLE();
  }

  UNREACHABLE();
  return NULL;
}


void JSDate::SetValue(Object* value, bool is_value_nan) {
  set_value(value);
  if (is_value_nan) {
    HeapNumber* nan = GetIsolate()->heap()->nan_value();
    set_cache_stamp(nan, SKIP_WRITE_BARRIER);
    set_year(nan, SKIP_WRITE_BARRIER);
    set_month(nan, SKIP_WRITE_BARRIER);
    set_day(nan, SKIP_WRITE_BARRIER);
    set_hour(nan, SKIP_WRITE_BARRIER);
    set_min(nan, SKIP_WRITE_BARRIER);
    set_sec(nan, SKIP_WRITE_BARRIER);
    set_weekday(nan, SKIP_WRITE_BARRIER);
  } else {
    set_cache_stamp(Smi::FromInt(DateCache::kInvalidStamp), SKIP_WRITE_BARRIER);
  }
}


void JSDate::SetCachedFields(int64_t local_time_ms, DateCache* date_cache) {
  int days = DateCache::DaysFromTime(local_time_ms);
  int time_in_day_ms = DateCache::TimeInDay(local_time_ms, days);
  int year, month, day;
  date_cache->YearMonthDayFromDays(days, &year, &month, &day);
  int weekday = date_cache->Weekday(days);
  int hour = time_in_day_ms / (60 * 60 * 1000);
  int min = (time_in_day_ms / (60 * 1000)) % 60;
  int sec = (time_in_day_ms / 1000) % 60;
  set_cache_stamp(date_cache->stamp());
  set_year(Smi::FromInt(year), SKIP_WRITE_BARRIER);
  set_month(Smi::FromInt(month), SKIP_WRITE_BARRIER);
  set_day(Smi::FromInt(day), SKIP_WRITE_BARRIER);
  set_weekday(Smi::FromInt(weekday), SKIP_WRITE_BARRIER);
  set_hour(Smi::FromInt(hour), SKIP_WRITE_BARRIER);
  set_min(Smi::FromInt(min), SKIP_WRITE_BARRIER);
  set_sec(Smi::FromInt(sec), SKIP_WRITE_BARRIER);
}


void JSArrayBuffer::Neuter() {
  DCHECK(is_external());
  set_backing_store(NULL);
  set_byte_length(Smi::FromInt(0));
}


void JSArrayBufferView::NeuterView() {
  set_byte_offset(Smi::FromInt(0));
  set_byte_length(Smi::FromInt(0));
}


void JSDataView::Neuter() {
  NeuterView();
}


void JSTypedArray::Neuter() {
  NeuterView();
  set_length(Smi::FromInt(0));
  set_elements(GetHeap()->EmptyExternalArrayForMap(map()));
}


static ElementsKind FixedToExternalElementsKind(ElementsKind elements_kind) {
  switch (elements_kind) {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size)                       \
    case TYPE##_ELEMENTS: return EXTERNAL_##TYPE##_ELEMENTS;

    TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE

    default:
      UNREACHABLE();
      return FIRST_EXTERNAL_ARRAY_ELEMENTS_KIND;
  }
}


Handle<JSArrayBuffer> JSTypedArray::MaterializeArrayBuffer(
    Handle<JSTypedArray> typed_array) {

  Handle<Map> map(typed_array->map());
  Isolate* isolate = typed_array->GetIsolate();

  DCHECK(IsFixedTypedArrayElementsKind(map->elements_kind()));

  Handle<Map> new_map = Map::TransitionElementsTo(
          map,
          FixedToExternalElementsKind(map->elements_kind()));

  Handle<JSArrayBuffer> buffer = isolate->factory()->NewJSArrayBuffer();
  Handle<FixedTypedArrayBase> fixed_typed_array(
      FixedTypedArrayBase::cast(typed_array->elements()));
  Runtime::SetupArrayBufferAllocatingData(isolate, buffer,
      fixed_typed_array->DataSize(), false);
  memcpy(buffer->backing_store(),
         fixed_typed_array->DataPtr(),
         fixed_typed_array->DataSize());
  Handle<ExternalArray> new_elements =
      isolate->factory()->NewExternalArray(
          fixed_typed_array->length(), typed_array->type(),
          static_cast<uint8_t*>(buffer->backing_store()));

  buffer->set_weak_first_view(*typed_array);
  DCHECK(typed_array->weak_next() == isolate->heap()->undefined_value());
  typed_array->set_buffer(*buffer);
  JSObject::SetMapAndElements(typed_array, new_map, new_elements);

  return buffer;
}


Handle<JSArrayBuffer> JSTypedArray::GetBuffer() {
  Handle<Object> result(buffer(), GetIsolate());
  if (*result != Smi::FromInt(0)) {
    DCHECK(IsExternalArrayElementsKind(map()->elements_kind()));
    return Handle<JSArrayBuffer>::cast(result);
  }
  Handle<JSTypedArray> self(this);
  return MaterializeArrayBuffer(self);
}


HeapType* PropertyCell::type() {
  return static_cast<HeapType*>(type_raw());
}


void PropertyCell::set_type(HeapType* type, WriteBarrierMode ignored) {
  DCHECK(IsPropertyCell());
  set_type_raw(type, ignored);
}


Handle<HeapType> PropertyCell::UpdatedType(Handle<PropertyCell> cell,
                                           Handle<Object> value) {
  Isolate* isolate = cell->GetIsolate();
  Handle<HeapType> old_type(cell->type(), isolate);
  Handle<HeapType> new_type = HeapType::Constant(value, isolate);

  if (new_type->Is(old_type)) return old_type;

  cell->dependent_code()->DeoptimizeDependentCodeGroup(
      isolate, DependentCode::kPropertyCellChangedGroup);

  if (old_type->Is(HeapType::None()) || old_type->Is(HeapType::Undefined())) {
    return new_type;
  }

  return HeapType::Any(isolate);
}


void PropertyCell::SetValueInferType(Handle<PropertyCell> cell,
                                     Handle<Object> value) {
  cell->set_value(*value);
  if (!HeapType::Any()->Is(cell->type())) {
    Handle<HeapType> new_type = UpdatedType(cell, value);
    cell->set_type(*new_type);
  }
}


// static
void PropertyCell::AddDependentCompilationInfo(Handle<PropertyCell> cell,
                                               CompilationInfo* info) {
  Handle<DependentCode> codes =
      DependentCode::Insert(handle(cell->dependent_code(), info->isolate()),
                            DependentCode::kPropertyCellChangedGroup,
                            info->object_wrapper());
  if (*codes != cell->dependent_code()) cell->set_dependent_code(*codes);
  info->dependencies(DependentCode::kPropertyCellChangedGroup)->Add(
      cell, info->zone());
}


const char* GetBailoutReason(BailoutReason reason) {
  DCHECK(reason < kLastErrorMessage);
#define ERROR_MESSAGES_TEXTS(C, T) T,
  static const char* error_messages_[] = {
      ERROR_MESSAGES_LIST(ERROR_MESSAGES_TEXTS)
  };
#undef ERROR_MESSAGES_TEXTS
  return error_messages_[reason];
}


} }  // namespace v8::internal
