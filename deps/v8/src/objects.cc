// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"

#include "accessors.h"
#include "api.h"
#include "arguments.h"
#include "bootstrapper.h"
#include "codegen.h"
#include "cpu-profiler.h"
#include "debug.h"
#include "deoptimizer.h"
#include "date.h"
#include "elements.h"
#include "execution.h"
#include "full-codegen.h"
#include "hydrogen.h"
#include "isolate-inl.h"
#include "log.h"
#include "objects-inl.h"
#include "objects-visiting.h"
#include "objects-visiting-inl.h"
#include "macro-assembler.h"
#include "mark-compact.h"
#include "safepoint-table.h"
#include "string-stream.h"
#include "utils.h"

#ifdef ENABLE_DISASSEMBLER
#include "disasm.h"
#include "disassembler.h"
#endif

namespace v8 {
namespace internal {


MUST_USE_RESULT static MaybeObject* CreateJSValue(JSFunction* constructor,
                                                  Object* value) {
  Object* result;
  { MaybeObject* maybe_result =
        constructor->GetHeap()->AllocateJSObject(constructor);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
  JSValue::cast(result)->set_value(value);
  return result;
}


MaybeObject* Object::ToObject(Context* native_context) {
  if (IsNumber()) {
    return CreateJSValue(native_context->number_function(), this);
  } else if (IsBoolean()) {
    return CreateJSValue(native_context->boolean_function(), this);
  } else if (IsString()) {
    return CreateJSValue(native_context->string_function(), this);
  }
  ASSERT(IsJSObject());
  return this;
}


MaybeObject* Object::ToObject(Isolate* isolate) {
  if (IsJSReceiver()) {
    return this;
  } else if (IsNumber()) {
    Context* native_context = isolate->context()->native_context();
    return CreateJSValue(native_context->number_function(), this);
  } else if (IsBoolean()) {
    Context* native_context = isolate->context()->native_context();
    return CreateJSValue(native_context->boolean_function(), this);
  } else if (IsString()) {
    Context* native_context = isolate->context()->native_context();
    return CreateJSValue(native_context->string_function(), this);
  } else if (IsSymbol()) {
    Context* native_context = isolate->context()->native_context();
    return CreateJSValue(native_context->symbol_function(), this);
  }

  // Throw a type error.
  return Failure::InternalError();
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


void Object::Lookup(Name* name, LookupResult* result) {
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
  ASSERT(holder != NULL);  // Cannot handle null or undefined.
  JSReceiver::cast(holder)->Lookup(name, result);
}


MaybeObject* Object::GetPropertyWithReceiver(Object* receiver,
                                             Name* name,
                                             PropertyAttributes* attributes) {
  LookupResult result(name->GetIsolate());
  Lookup(name, &result);
  MaybeObject* value = GetProperty(receiver, &result, name, attributes);
  ASSERT(*attributes <= ABSENT);
  return value;
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


template<typename To>
static inline To* CheckedCast(void *from) {
  uintptr_t temp = reinterpret_cast<uintptr_t>(from);
  ASSERT(temp % sizeof(To) == 0);
  return reinterpret_cast<To*>(temp);
}


static MaybeObject* PerformCompare(const BitmaskCompareDescriptor& descriptor,
                                   char* ptr,
                                   Heap* heap) {
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
      return NULL;
  }
  return heap->ToBoolean((bitmask & value) == (bitmask & compare_value));
}


static MaybeObject* PerformCompare(const PointerCompareDescriptor& descriptor,
                                   char* ptr,
                                   Heap* heap) {
  uintptr_t compare_value =
      reinterpret_cast<uintptr_t>(descriptor.compare_value);
  uintptr_t value = *CheckedCast<uintptr_t>(ptr);
  return heap->ToBoolean(compare_value == value);
}


static MaybeObject* GetPrimitiveValue(
    const PrimitiveValueDescriptor& descriptor,
    char* ptr,
    Heap* heap) {
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
      return heap->NumberFromUint32(value);
    }
    case kDescriptorBoolType: {
      uint8_t byte = *CheckedCast<uint8_t>(ptr);
      return heap->ToBoolean(byte & (0x1 << descriptor.bool_offset));
    }
    case kDescriptorFloatType: {
      float value = *CheckedCast<float>(ptr);
      return heap->NumberFromDouble(value);
    }
    case kDescriptorDoubleType: {
      double value = *CheckedCast<double>(ptr);
      return heap->NumberFromDouble(value);
    }
  }
  return heap->NumberFromInt32(int32_value);
}


static MaybeObject* GetDeclaredAccessorProperty(Object* receiver,
                                                DeclaredAccessorInfo* info,
                                                Isolate* isolate) {
  char* current = reinterpret_cast<char*>(receiver);
  DeclaredAccessorDescriptorIterator iterator(info->descriptor());
  while (true) {
    const DeclaredAccessorDescriptorData* data = iterator.Next();
    switch (data->type) {
      case kDescriptorReturnObject: {
        ASSERT(iterator.Complete());
        current = *CheckedCast<char*>(current);
        return *CheckedCast<Object*>(current);
      }
      case kDescriptorPointerDereference:
        ASSERT(!iterator.Complete());
        current = *reinterpret_cast<char**>(current);
        break;
      case kDescriptorPointerShift:
        ASSERT(!iterator.Complete());
        current += data->pointer_shift_descriptor.byte_offset;
        break;
      case kDescriptorObjectDereference: {
        ASSERT(!iterator.Complete());
        Object* object = CheckedCast<Object>(current);
        int field = data->object_dereference_descriptor.internal_field;
        Object* smi = JSObject::cast(object)->GetInternalField(field);
        ASSERT(smi->IsSmi());
        current = reinterpret_cast<char*>(smi);
        break;
      }
      case kDescriptorBitmaskCompare:
        ASSERT(iterator.Complete());
        return PerformCompare(data->bitmask_compare_descriptor,
                              current,
                              isolate->heap());
      case kDescriptorPointerCompare:
        ASSERT(iterator.Complete());
        return PerformCompare(data->pointer_compare_descriptor,
                              current,
                              isolate->heap());
      case kDescriptorPrimitiveValue:
        ASSERT(iterator.Complete());
        return GetPrimitiveValue(data->primitive_value_descriptor,
                                 current,
                                 isolate->heap());
    }
  }
  UNREACHABLE();
  return NULL;
}


MaybeObject* JSObject::GetPropertyWithCallback(Object* receiver,
                                               Object* structure,
                                               Name* name) {
  Isolate* isolate = name->GetIsolate();
  // To accommodate both the old and the new api we switch on the
  // data structure used to store the callbacks.  Eventually foreign
  // callbacks should be phased out.
  if (structure->IsForeign()) {
    AccessorDescriptor* callback =
        reinterpret_cast<AccessorDescriptor*>(
            Foreign::cast(structure)->foreign_address());
    MaybeObject* value = (callback->getter)(isolate, receiver, callback->data);
    RETURN_IF_SCHEDULED_EXCEPTION(isolate);
    return value;
  }

  // api style callbacks.
  if (structure->IsAccessorInfo()) {
    if (!AccessorInfo::cast(structure)->IsCompatibleReceiver(receiver)) {
      Handle<Object> name_handle(name, isolate);
      Handle<Object> receiver_handle(receiver, isolate);
      Handle<Object> args[2] = { name_handle, receiver_handle };
      Handle<Object> error =
          isolate->factory()->NewTypeError("incompatible_method_receiver",
                                           HandleVector(args,
                                                        ARRAY_SIZE(args)));
      return isolate->Throw(*error);
    }
    // TODO(rossberg): Handling symbols in the API requires changing the API,
    // so we do not support it for now.
    if (name->IsSymbol()) return isolate->heap()->undefined_value();
    if (structure->IsDeclaredAccessorInfo()) {
      return GetDeclaredAccessorProperty(receiver,
                                         DeclaredAccessorInfo::cast(structure),
                                         isolate);
    }
    ExecutableAccessorInfo* data = ExecutableAccessorInfo::cast(structure);
    Object* fun_obj = data->getter();
    v8::AccessorGetterCallback call_fun =
        v8::ToCData<v8::AccessorGetterCallback>(fun_obj);
    if (call_fun == NULL) return isolate->heap()->undefined_value();
    HandleScope scope(isolate);
    JSObject* self = JSObject::cast(receiver);
    Handle<String> key(String::cast(name));
    LOG(isolate, ApiNamedPropertyAccess("load", self, name));
    PropertyCallbackArguments args(isolate, data->data(), self, this);
    v8::Handle<v8::Value> result =
        args.Call(call_fun, v8::Utils::ToLocal(key));
    RETURN_IF_SCHEDULED_EXCEPTION(isolate);
    if (result.IsEmpty()) {
      return isolate->heap()->undefined_value();
    }
    Object* return_value = *v8::Utils::OpenHandle(*result);
    return_value->VerifyApiCallResultType();
    return return_value;
  }

  // __defineGetter__ callback
  if (structure->IsAccessorPair()) {
    Object* getter = AccessorPair::cast(structure)->getter();
    if (getter->IsSpecFunction()) {
      // TODO(rossberg): nicer would be to cast to some JSCallable here...
      return GetPropertyWithDefinedGetter(receiver, JSReceiver::cast(getter));
    }
    // Getter is not a function.
    return isolate->heap()->undefined_value();
  }

  UNREACHABLE();
  return NULL;
}


MaybeObject* JSProxy::GetPropertyWithHandler(Object* receiver_raw,
                                             Name* name_raw) {
  Isolate* isolate = GetIsolate();
  HandleScope scope(isolate);
  Handle<Object> receiver(receiver_raw, isolate);
  Handle<Object> name(name_raw, isolate);

  // TODO(rossberg): adjust once there is a story for symbols vs proxies.
  if (name->IsSymbol()) return isolate->heap()->undefined_value();

  Handle<Object> args[] = { receiver, name };
  Handle<Object> result = CallTrap(
    "get", isolate->derived_get_trap(), ARRAY_SIZE(args), args);
  if (isolate->has_pending_exception()) return Failure::Exception();

  return *result;
}


Handle<Object> Object::GetProperty(Handle<Object> object,
                                   Handle<Name> name) {
  // TODO(rossberg): The index test should not be here but in the GetProperty
  // method (or somewhere else entirely). Needs more global clean-up.
  uint32_t index;
  Isolate* isolate = name->GetIsolate();
  if (name->AsArrayIndex(&index))
    return GetElement(isolate, object, index);
  CALL_HEAP_FUNCTION(isolate, object->GetProperty(*name), Object);
}


Handle<Object> Object::GetElement(Isolate* isolate,
                                  Handle<Object> object,
                                  uint32_t index) {
  CALL_HEAP_FUNCTION(isolate, object->GetElement(isolate, index), Object);
}


MaybeObject* JSProxy::GetElementWithHandler(Object* receiver,
                                            uint32_t index) {
  String* name;
  MaybeObject* maybe = GetHeap()->Uint32ToString(index);
  if (!maybe->To<String>(&name)) return maybe;
  return GetPropertyWithHandler(receiver, name);
}


Handle<Object> JSProxy::SetElementWithHandler(Handle<JSProxy> proxy,
                                              Handle<JSReceiver> receiver,
                                              uint32_t index,
                                              Handle<Object> value,
                                              StrictModeFlag strict_mode) {
  Isolate* isolate = proxy->GetIsolate();
  Handle<String> name = isolate->factory()->Uint32ToString(index);
  CALL_HEAP_FUNCTION(isolate,
                     proxy->SetPropertyWithHandler(
                         *receiver, *name, *value, NONE, strict_mode),
                     Object);
}


bool JSProxy::HasElementWithHandler(uint32_t index) {
  String* name;
  MaybeObject* maybe = GetHeap()->Uint32ToString(index);
  if (!maybe->To<String>(&name)) return maybe;
  return HasPropertyWithHandler(name);
}


MaybeObject* Object::GetPropertyWithDefinedGetter(Object* receiver,
                                                  JSReceiver* getter) {
  Isolate* isolate = getter->GetIsolate();
  HandleScope scope(isolate);
  Handle<JSReceiver> fun(getter);
  Handle<Object> self(receiver, isolate);
#ifdef ENABLE_DEBUGGER_SUPPORT
  Debug* debug = isolate->debug();
  // Handle stepping into a getter if step into is active.
  // TODO(rossberg): should this apply to getters that are function proxies?
  if (debug->StepInActive() && fun->IsJSFunction()) {
    debug->HandleStepIn(
        Handle<JSFunction>::cast(fun), Handle<Object>::null(), 0, false);
  }
#endif

  bool has_pending_exception;
  Handle<Object> result = Execution::Call(
      isolate, fun, self, 0, NULL, &has_pending_exception, true);
  // Check for pending exception and return the result.
  if (has_pending_exception) return Failure::Exception();
  return *result;
}


// Only deal with CALLBACKS and INTERCEPTOR
MaybeObject* JSObject::GetPropertyWithFailedAccessCheck(
    Object* receiver,
    LookupResult* result,
    Name* name,
    PropertyAttributes* attributes) {
  if (result->IsProperty()) {
    switch (result->type()) {
      case CALLBACKS: {
        // Only allow API accessors.
        Object* obj = result->GetCallbackObject();
        if (obj->IsAccessorInfo()) {
          AccessorInfo* info = AccessorInfo::cast(obj);
          if (info->all_can_read()) {
            *attributes = result->GetAttributes();
            return result->holder()->GetPropertyWithCallback(
                receiver, result->GetCallbackObject(), name);
          }
        } else if (obj->IsAccessorPair()) {
          AccessorPair* pair = AccessorPair::cast(obj);
          if (pair->all_can_read()) {
            return result->holder()->GetPropertyWithCallback(
                receiver, result->GetCallbackObject(), name);
          }
        }
        break;
      }
      case NORMAL:
      case FIELD:
      case CONSTANT: {
        // Search ALL_CAN_READ accessors in prototype chain.
        LookupResult r(GetIsolate());
        result->holder()->LookupRealNamedPropertyInPrototypes(name, &r);
        if (r.IsProperty()) {
          return GetPropertyWithFailedAccessCheck(receiver,
                                                  &r,
                                                  name,
                                                  attributes);
        }
        break;
      }
      case INTERCEPTOR: {
        // If the object has an interceptor, try real named properties.
        // No access check in GetPropertyAttributeWithInterceptor.
        LookupResult r(GetIsolate());
        result->holder()->LookupRealNamedProperty(name, &r);
        if (r.IsProperty()) {
          return GetPropertyWithFailedAccessCheck(receiver,
                                                  &r,
                                                  name,
                                                  attributes);
        }
        break;
      }
      default:
        UNREACHABLE();
    }
  }

  // No accessible property found.
  *attributes = ABSENT;
  Heap* heap = name->GetHeap();
  Isolate* isolate = heap->isolate();
  isolate->ReportFailedAccessCheck(this, v8::ACCESS_GET);
  RETURN_IF_SCHEDULED_EXCEPTION(isolate);
  return heap->undefined_value();
}


PropertyAttributes JSObject::GetPropertyAttributeWithFailedAccessCheck(
    Object* receiver,
    LookupResult* result,
    Name* name,
    bool continue_search) {
  if (result->IsProperty()) {
    switch (result->type()) {
      case CALLBACKS: {
        // Only allow API accessors.
        Object* obj = result->GetCallbackObject();
        if (obj->IsAccessorInfo()) {
          AccessorInfo* info = AccessorInfo::cast(obj);
          if (info->all_can_read()) {
            return result->GetAttributes();
          }
        } else if (obj->IsAccessorPair()) {
          AccessorPair* pair = AccessorPair::cast(obj);
          if (pair->all_can_read()) {
            return result->GetAttributes();
          }
        }
        break;
      }

      case NORMAL:
      case FIELD:
      case CONSTANT: {
        if (!continue_search) break;
        // Search ALL_CAN_READ accessors in prototype chain.
        LookupResult r(GetIsolate());
        result->holder()->LookupRealNamedPropertyInPrototypes(name, &r);
        if (r.IsProperty()) {
          return GetPropertyAttributeWithFailedAccessCheck(receiver,
                                                           &r,
                                                           name,
                                                           continue_search);
        }
        break;
      }

      case INTERCEPTOR: {
        // If the object has an interceptor, try real named properties.
        // No access check in GetPropertyAttributeWithInterceptor.
        LookupResult r(GetIsolate());
        if (continue_search) {
          result->holder()->LookupRealNamedProperty(name, &r);
        } else {
          result->holder()->LocalLookupRealNamedProperty(name, &r);
        }
        if (!r.IsFound()) break;
        return GetPropertyAttributeWithFailedAccessCheck(receiver,
                                                         &r,
                                                         name,
                                                         continue_search);
      }

      case HANDLER:
      case TRANSITION:
      case NONEXISTENT:
        UNREACHABLE();
    }
  }

  GetIsolate()->ReportFailedAccessCheck(this, v8::ACCESS_HAS);
  return ABSENT;
}


Object* JSObject::GetNormalizedProperty(LookupResult* result) {
  ASSERT(!HasFastProperties());
  Object* value = property_dictionary()->ValueAt(result->GetDictionaryEntry());
  if (IsGlobalObject()) {
    value = PropertyCell::cast(value)->value();
  }
  ASSERT(!value->IsPropertyCell() && !value->IsCell());
  return value;
}


Handle<Object> JSObject::SetNormalizedProperty(Handle<JSObject> object,
                                               LookupResult* result,
                                               Handle<Object> value) {
  CALL_HEAP_FUNCTION(object->GetIsolate(),
                     object->SetNormalizedProperty(result, *value),
                     Object);
}


MaybeObject* JSObject::SetNormalizedProperty(LookupResult* result,
                                             Object* value) {
  ASSERT(!HasFastProperties());
  if (IsGlobalObject()) {
    PropertyCell* cell = PropertyCell::cast(
        property_dictionary()->ValueAt(result->GetDictionaryEntry()));
    MaybeObject* maybe_type = cell->SetValueInferType(value);
    if (maybe_type->IsFailure()) return maybe_type;
  } else {
    property_dictionary()->ValueAtPut(result->GetDictionaryEntry(), value);
  }
  return value;
}


Handle<Object> JSObject::SetNormalizedProperty(Handle<JSObject> object,
                                               Handle<Name> key,
                                               Handle<Object> value,
                                               PropertyDetails details) {
  CALL_HEAP_FUNCTION(object->GetIsolate(),
                     object->SetNormalizedProperty(*key, *value, details),
                     Object);
}


MaybeObject* JSObject::SetNormalizedProperty(Name* name,
                                             Object* value,
                                             PropertyDetails details) {
  ASSERT(!HasFastProperties());
  int entry = property_dictionary()->FindEntry(name);
  if (entry == NameDictionary::kNotFound) {
    Object* store_value = value;
    if (IsGlobalObject()) {
      Heap* heap = name->GetHeap();
      MaybeObject* maybe_store_value = heap->AllocatePropertyCell(value);
      if (!maybe_store_value->ToObject(&store_value)) return maybe_store_value;
    }
    Object* dict;
    { MaybeObject* maybe_dict =
          property_dictionary()->Add(name, store_value, details);
      if (!maybe_dict->ToObject(&dict)) return maybe_dict;
    }
    set_properties(NameDictionary::cast(dict));
    return value;
  }

  PropertyDetails original_details = property_dictionary()->DetailsAt(entry);
  int enumeration_index;
  // Preserve the enumeration index unless the property was deleted.
  if (original_details.IsDeleted()) {
    enumeration_index = property_dictionary()->NextEnumerationIndex();
    property_dictionary()->SetNextEnumerationIndex(enumeration_index + 1);
  } else {
    enumeration_index = original_details.dictionary_index();
    ASSERT(enumeration_index > 0);
  }

  details = PropertyDetails(
      details.attributes(), details.type(), enumeration_index);

  if (IsGlobalObject()) {
    PropertyCell* cell =
        PropertyCell::cast(property_dictionary()->ValueAt(entry));
    MaybeObject* maybe_type = cell->SetValueInferType(value);
    if (maybe_type->IsFailure()) return maybe_type;
    // Please note we have to update the property details.
    property_dictionary()->DetailsAtPut(entry, details);
  } else {
    property_dictionary()->SetEntry(entry, name, value, details);
  }
  return value;
}


// TODO(mstarzinger): Temporary wrapper until target is handlified.
Handle<NameDictionary> NameDictionaryShrink(Handle<NameDictionary> dict,
                                            Handle<Name> name) {
  CALL_HEAP_FUNCTION(dict->GetIsolate(), dict->Shrink(*name), NameDictionary);
}


static void CellSetValueInferType(Handle<PropertyCell> cell,
                                  Handle<Object> value) {
  CALL_HEAP_FUNCTION_VOID(cell->GetIsolate(), cell->SetValueInferType(*value));
}


Handle<Object> JSObject::DeleteNormalizedProperty(Handle<JSObject> object,
                                                  Handle<Name> name,
                                                  DeleteMode mode) {
  ASSERT(!object->HasFastProperties());
  Isolate* isolate = object->GetIsolate();
  Handle<NameDictionary> dictionary(object->property_dictionary());
  int entry = dictionary->FindEntry(*name);
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
        ASSERT(new_map->is_dictionary_map());
        object->set_map(*new_map);
      }
      Handle<PropertyCell> cell(PropertyCell::cast(dictionary->ValueAt(entry)));
      CellSetValueInferType(cell, isolate->factory()->the_hole_value());
      dictionary->DetailsAtPut(entry, details.AsDeleted());
    } else {
      Handle<Object> deleted(dictionary->DeleteProperty(entry, mode), isolate);
      if (*deleted == isolate->heap()->true_value()) {
        Handle<NameDictionary> new_properties =
            NameDictionaryShrink(dictionary, name);
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


Handle<Object> Object::GetProperty(Handle<Object> object,
                                   Handle<Object> receiver,
                                   LookupResult* result,
                                   Handle<Name> key,
                                   PropertyAttributes* attributes) {
  Isolate* isolate = result->isolate();
  CALL_HEAP_FUNCTION(
      isolate,
      object->GetProperty(*receiver, result, *key, attributes),
      Object);
}


MaybeObject* Object::GetPropertyOrFail(Handle<Object> object,
                                       Handle<Object> receiver,
                                       LookupResult* result,
                                       Handle<Name> key,
                                       PropertyAttributes* attributes) {
  Isolate* isolate = result->isolate();
  CALL_HEAP_FUNCTION_PASS_EXCEPTION(
      isolate,
      object->GetProperty(*receiver, result, *key, attributes));
}


MaybeObject* Object::GetProperty(Object* receiver,
                                 LookupResult* result,
                                 Name* name,
                                 PropertyAttributes* attributes) {
  // Make sure that the top context does not change when doing
  // callbacks or interceptor calls.
  AssertNoContextChangeWithHandleScope ncc;

  Isolate* isolate = name->GetIsolate();
  Heap* heap = isolate->heap();

  // Traverse the prototype chain from the current object (this) to
  // the holder and check for access rights. This avoids traversing the
  // objects more than once in case of interceptors, because the
  // holder will always be the interceptor holder and the search may
  // only continue with a current object just after the interceptor
  // holder in the prototype chain.
  // Proxy handlers do not use the proxy's prototype, so we can skip this.
  if (!result->IsHandler()) {
    Object* last = result->IsProperty()
        ? result->holder()
        : Object::cast(heap->null_value());
    ASSERT(this != this->GetPrototype(isolate));
    for (Object* current = this;
         true;
         current = current->GetPrototype(isolate)) {
      if (current->IsAccessCheckNeeded()) {
        // Check if we're allowed to read from the current object. Note
        // that even though we may not actually end up loading the named
        // property from the current object, we still check that we have
        // access to it.
        JSObject* checked = JSObject::cast(current);
        if (!heap->isolate()->MayNamedAccess(checked, name, v8::ACCESS_GET)) {
          return checked->GetPropertyWithFailedAccessCheck(receiver,
                                                           result,
                                                           name,
                                                           attributes);
        }
      }
      // Stop traversing the chain once we reach the last object in the
      // chain; either the holder of the result or null in case of an
      // absent property.
      if (current == last) break;
    }
  }

  if (!result->IsProperty()) {
    *attributes = ABSENT;
    return heap->undefined_value();
  }
  *attributes = result->GetAttributes();
  Object* value;
  switch (result->type()) {
    case NORMAL:
      value = result->holder()->GetNormalizedProperty(result);
      ASSERT(!value->IsTheHole() || result->IsReadOnly());
      return value->IsTheHole() ? heap->undefined_value() : value;
    case FIELD: {
      MaybeObject* maybe_result = result->holder()->FastPropertyAt(
          result->representation(),
          result->GetFieldIndex().field_index());
      if (!maybe_result->To(&value)) return maybe_result;
      ASSERT(!value->IsTheHole() || result->IsReadOnly());
      return value->IsTheHole() ? heap->undefined_value() : value;
    }
    case CONSTANT:
      return result->GetConstant();
    case CALLBACKS:
      return result->holder()->GetPropertyWithCallback(
          receiver, result->GetCallbackObject(), name);
    case HANDLER:
      return result->proxy()->GetPropertyWithHandler(receiver, name);
    case INTERCEPTOR:
      return result->holder()->GetPropertyWithInterceptor(
          receiver, name, attributes);
    case TRANSITION:
    case NONEXISTENT:
      UNREACHABLE();
      break;
  }
  UNREACHABLE();
  return NULL;
}


MaybeObject* Object::GetElementWithReceiver(Isolate* isolate,
                                            Object* receiver,
                                            uint32_t index) {
  Heap* heap = isolate->heap();
  Object* holder = this;

  // Iterate up the prototype chain until an element is found or the null
  // prototype is encountered.
  for (holder = this;
       holder != heap->null_value();
       holder = holder->GetPrototype(isolate)) {
    if (!holder->IsJSObject()) {
      Context* native_context = isolate->context()->native_context();
      if (holder->IsNumber()) {
        holder = native_context->number_function()->instance_prototype();
      } else if (holder->IsString()) {
        holder = native_context->string_function()->instance_prototype();
      } else if (holder->IsSymbol()) {
        holder = native_context->symbol_function()->instance_prototype();
      } else if (holder->IsBoolean()) {
        holder = native_context->boolean_function()->instance_prototype();
      } else if (holder->IsJSProxy()) {
        return JSProxy::cast(holder)->GetElementWithHandler(receiver, index);
      } else {
        // Undefined and null have no indexed properties.
        ASSERT(holder->IsUndefined() || holder->IsNull());
        return heap->undefined_value();
      }
    }

    // Inline the case for JSObjects. Doing so significantly improves the
    // performance of fetching elements where checking the prototype chain is
    // necessary.
    JSObject* js_object = JSObject::cast(holder);

    // Check access rights if needed.
    if (js_object->IsAccessCheckNeeded()) {
      Isolate* isolate = heap->isolate();
      if (!isolate->MayIndexedAccess(js_object, index, v8::ACCESS_GET)) {
        isolate->ReportFailedAccessCheck(js_object, v8::ACCESS_GET);
        RETURN_IF_SCHEDULED_EXCEPTION(isolate);
        return heap->undefined_value();
      }
    }

    if (js_object->HasIndexedInterceptor()) {
      return js_object->GetElementWithInterceptor(receiver, index);
    }

    if (js_object->elements() != heap->empty_fixed_array()) {
      MaybeObject* result = js_object->GetElementsAccessor()->Get(
          receiver, js_object, index);
      if (result != heap->the_hole_value()) return result;
    }
  }

  return heap->undefined_value();
}


Object* Object::GetPrototype(Isolate* isolate) {
  if (IsSmi()) {
    Context* context = isolate->context()->native_context();
    return context->number_function()->instance_prototype();
  }

  HeapObject* heap_object = HeapObject::cast(this);

  // The object is either a number, a string, a boolean,
  // a real JS object, or a Harmony proxy.
  if (heap_object->IsJSReceiver()) {
    return heap_object->map()->prototype();
  }
  Context* context = isolate->context()->native_context();

  if (heap_object->IsHeapNumber()) {
    return context->number_function()->instance_prototype();
  }
  if (heap_object->IsString()) {
    return context->string_function()->instance_prototype();
  }
  if (heap_object->IsSymbol()) {
    return context->symbol_function()->instance_prototype();
  }
  if (heap_object->IsBoolean()) {
    return context->boolean_function()->instance_prototype();
  } else {
    return isolate->heap()->null_value();
  }
}


MaybeObject* Object::GetHash(CreationFlag flag) {
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
  if (IsJSReceiver()) {
    return JSReceiver::cast(this)->GetIdentityHash(flag);
  }

  UNREACHABLE();
  return Smi::FromInt(0);
}


bool Object::SameValue(Object* other) {
  if (other == this) return true;

  // The object is either a number, a name, an odd-ball,
  // a real JS object, or a Harmony proxy.
  if (IsNumber() && other->IsNumber()) {
    double this_value = Number();
    double other_value = other->Number();
    return (this_value == other_value) ||
        (std::isnan(this_value) && std::isnan(other_value));
  }
  if (IsString() && other->IsString()) {
    return String::cast(this)->Equals(String::cast(other));
  }
  return false;
}


void Object::ShortPrint(FILE* out) {
  HeapStringAllocator allocator;
  StringStream accumulator(&allocator);
  ShortPrint(&accumulator);
  accumulator.OutputToFile(out);
}


void Object::ShortPrint(StringStream* accumulator) {
  if (IsSmi()) {
    Smi::cast(this)->SmiPrint(accumulator);
  } else if (IsFailure()) {
    Failure::cast(this)->FailurePrint(accumulator);
  } else {
    HeapObject::cast(this)->HeapObjectShortPrint(accumulator);
  }
}


void Smi::SmiPrint(FILE* out) {
  PrintF(out, "%d", value());
}


void Smi::SmiPrint(StringStream* accumulator) {
  accumulator->Add("%d", value());
}


void Failure::FailurePrint(StringStream* accumulator) {
  accumulator->Add("Failure(%p)", reinterpret_cast<void*>(value()));
}


void Failure::FailurePrint(FILE* out) {
  PrintF(out, "Failure(%p)", reinterpret_cast<void*>(value()));
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


MaybeObject* String::SlowTryFlatten(PretenureFlag pretenure) {
#ifdef DEBUG
  // Do not attempt to flatten in debug mode when allocation is not
  // allowed.  This is to avoid an assertion failure when allocating.
  // Flattening strings is the only case where we always allow
  // allocation because no GC is performed if the allocation fails.
  if (!AllowHeapAllocation::IsAllowed()) return this;
#endif

  Heap* heap = GetHeap();
  switch (StringShape(this).representation_tag()) {
    case kConsStringTag: {
      ConsString* cs = ConsString::cast(this);
      if (cs->second()->length() == 0) {
        return cs->first();
      }
      // There's little point in putting the flat string in new space if the
      // cons string is in old space.  It can never get GCed until there is
      // an old space GC.
      PretenureFlag tenure = heap->InNewSpace(this) ? pretenure : TENURED;
      int len = length();
      Object* object;
      String* result;
      if (IsOneByteRepresentation()) {
        { MaybeObject* maybe_object =
              heap->AllocateRawOneByteString(len, tenure);
          if (!maybe_object->ToObject(&object)) return maybe_object;
        }
        result = String::cast(object);
        String* first = cs->first();
        int first_length = first->length();
        uint8_t* dest = SeqOneByteString::cast(result)->GetChars();
        WriteToFlat(first, dest, 0, first_length);
        String* second = cs->second();
        WriteToFlat(second,
                    dest + first_length,
                    0,
                    len - first_length);
      } else {
        { MaybeObject* maybe_object =
              heap->AllocateRawTwoByteString(len, tenure);
          if (!maybe_object->ToObject(&object)) return maybe_object;
        }
        result = String::cast(object);
        uc16* dest = SeqTwoByteString::cast(result)->GetChars();
        String* first = cs->first();
        int first_length = first->length();
        WriteToFlat(first, dest, 0, first_length);
        String* second = cs->second();
        WriteToFlat(second,
                    dest + first_length,
                    0,
                    len - first_length);
      }
      cs->set_first(result);
      cs->set_second(heap->empty_string(), SKIP_WRITE_BARRIER);
      return result;
    }
    default:
      return this;
  }
}


bool String::MakeExternal(v8::String::ExternalStringResource* resource) {
  // Externalizing twice leaks the external resource, so it's
  // prohibited by the API.
  ASSERT(!this->IsExternalString());
#ifdef DEBUG
  if (FLAG_enable_slow_asserts) {
    // Assert that the resource and the string are equivalent.
    ASSERT(static_cast<size_t>(this->length()) == resource->length());
    ScopedVector<uc16> smart_chars(this->length());
    String::WriteToFlat(this, smart_chars.start(), 0, this->length());
    ASSERT(memcmp(smart_chars.start(),
                  resource->data(),
                  resource->length() * sizeof(smart_chars[0])) == 0);
  }
#endif  // DEBUG
  Heap* heap = GetHeap();
  int size = this->Size();  // Byte size of the original string.
  if (size < ExternalString::kShortSize) {
    return false;
  }
  bool is_ascii = this->IsOneByteRepresentation();
  bool is_internalized = this->IsInternalizedString();

  // Morph the object to an external string by adjusting the map and
  // reinitializing the fields.
  if (size >= ExternalString::kSize) {
    this->set_map_no_write_barrier(
        is_internalized
            ? (is_ascii
                   ? heap->external_internalized_string_with_one_byte_data_map()
                   : heap->external_internalized_string_map())
            : (is_ascii
                   ? heap->external_string_with_one_byte_data_map()
                   : heap->external_string_map()));
  } else {
    this->set_map_no_write_barrier(
        is_internalized
          ? (is_ascii
               ? heap->
                   short_external_internalized_string_with_one_byte_data_map()
               : heap->short_external_internalized_string_map())
          : (is_ascii
                 ? heap->short_external_string_with_one_byte_data_map()
                 : heap->short_external_string_map()));
  }
  ExternalTwoByteString* self = ExternalTwoByteString::cast(this);
  self->set_resource(resource);
  if (is_internalized) self->Hash();  // Force regeneration of the hash value.

  // Fill the remainder of the string with dead wood.
  int new_size = this->Size();  // Byte size of the external String object.
  heap->CreateFillerObjectAt(this->address() + new_size, size - new_size);
  if (Marking::IsBlack(Marking::MarkBitFrom(this))) {
    MemoryChunk::IncrementLiveBytesFromMutator(this->address(),
                                               new_size - size);
  }
  return true;
}


bool String::MakeExternal(v8::String::ExternalAsciiStringResource* resource) {
#ifdef DEBUG
  if (FLAG_enable_slow_asserts) {
    // Assert that the resource and the string are equivalent.
    ASSERT(static_cast<size_t>(this->length()) == resource->length());
    if (this->IsTwoByteRepresentation()) {
      ScopedVector<uint16_t> smart_chars(this->length());
      String::WriteToFlat(this, smart_chars.start(), 0, this->length());
      ASSERT(String::IsOneByte(smart_chars.start(), this->length()));
    }
    ScopedVector<char> smart_chars(this->length());
    String::WriteToFlat(this, smart_chars.start(), 0, this->length());
    ASSERT(memcmp(smart_chars.start(),
                  resource->data(),
                  resource->length() * sizeof(smart_chars[0])) == 0);
  }
#endif  // DEBUG
  Heap* heap = GetHeap();
  int size = this->Size();  // Byte size of the original string.
  if (size < ExternalString::kShortSize) {
    return false;
  }
  bool is_internalized = this->IsInternalizedString();

  // Morph the object to an external string by adjusting the map and
  // reinitializing the fields.  Use short version if space is limited.
  if (size >= ExternalString::kSize) {
    this->set_map_no_write_barrier(
        is_internalized ? heap->external_ascii_internalized_string_map()
                        : heap->external_ascii_string_map());
  } else {
    this->set_map_no_write_barrier(
        is_internalized ? heap->short_external_ascii_internalized_string_map()
                        : heap->short_external_ascii_string_map());
  }
  ExternalAsciiString* self = ExternalAsciiString::cast(this);
  self->set_resource(resource);
  if (is_internalized) self->Hash();  // Force regeneration of the hash value.

  // Fill the remainder of the string with dead wood.
  int new_size = this->Size();  // Byte size of the external String object.
  heap->CreateFillerObjectAt(this->address() + new_size, size - new_size);
  if (Marking::IsBlack(Marking::MarkBitFrom(this))) {
    MemoryChunk::IncrementLiveBytesFromMutator(this->address(),
                                               new_size - size);
  }
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
    FILE* file, ElementsKind from_kind, FixedArrayBase* from_elements,
    ElementsKind to_kind, FixedArrayBase* to_elements) {
  if (from_kind != to_kind) {
    PrintF(file, "elements transition [");
    PrintElementsKind(file, from_kind);
    PrintF(file, " -> ");
    PrintElementsKind(file, to_kind);
    PrintF(file, "] in ");
    JavaScriptFrame::PrintTop(GetIsolate(), file, false, true);
    PrintF(file, " for ");
    ShortPrint(file);
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
                              Representation new_representation) {
  PrintF(file, "[generalizing ");
  constructor_name()->PrintOn(file);
  PrintF(file, "] ");
  String::cast(instance_descriptors()->GetKey(modify_index))->PrintOn(file);
  if (constant_to_field) {
    PrintF(file, ":c->f");
  } else {
    PrintF(file, ":%s->%s",
           old_representation.Mnemonic(),
           new_representation.Mnemonic());
  }
  PrintF(file, " (");
  if (strlen(reason) > 0) {
    PrintF(file, "%s", reason);
  } else {
    PrintF(file, "+%i maps", descriptors - split);
  }
  PrintF(file, ") [");
  JavaScriptFrame::PrintTop(GetIsolate(), file, false, true);
  PrintF(file, "]\n");
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
        PrintF(file, "???");
      }
      PrintF(file, " ");
    }
  }
  PrintF(file, "\n");
}


void HeapObject::HeapObjectShortPrint(StringStream* accumulator) {
  Heap* heap = GetHeap();
  if (!heap->Contains(this)) {
    accumulator->Add("!!!INVALID POINTER!!!");
    return;
  }
  if (!heap->Contains(map())) {
    accumulator->Add("!!!INVALID MAP!!!");
    return;
  }

  accumulator->Add("%p ", this);

  if (IsString()) {
    String::cast(this)->StringShortPrint(accumulator);
    return;
  }
  if (IsJSObject()) {
    JSObject::cast(this)->JSObjectShortPrint(accumulator);
    return;
  }
  switch (map()->instance_type()) {
    case MAP_TYPE:
      accumulator->Add("<Map(elements=%u)>", Map::cast(this)->elements_kind());
      break;
    case FIXED_ARRAY_TYPE:
      accumulator->Add("<FixedArray[%u]>", FixedArray::cast(this)->length());
      break;
    case FIXED_DOUBLE_ARRAY_TYPE:
      accumulator->Add("<FixedDoubleArray[%u]>",
                       FixedDoubleArray::cast(this)->length());
      break;
    case BYTE_ARRAY_TYPE:
      accumulator->Add("<ByteArray[%u]>", ByteArray::cast(this)->length());
      break;
    case FREE_SPACE_TYPE:
      accumulator->Add("<FreeSpace[%u]>", FreeSpace::cast(this)->Size());
      break;
    case EXTERNAL_PIXEL_ARRAY_TYPE:
      accumulator->Add("<ExternalPixelArray[%u]>",
                       ExternalPixelArray::cast(this)->length());
      break;
    case EXTERNAL_BYTE_ARRAY_TYPE:
      accumulator->Add("<ExternalByteArray[%u]>",
                       ExternalByteArray::cast(this)->length());
      break;
    case EXTERNAL_UNSIGNED_BYTE_ARRAY_TYPE:
      accumulator->Add("<ExternalUnsignedByteArray[%u]>",
                       ExternalUnsignedByteArray::cast(this)->length());
      break;
    case EXTERNAL_SHORT_ARRAY_TYPE:
      accumulator->Add("<ExternalShortArray[%u]>",
                       ExternalShortArray::cast(this)->length());
      break;
    case EXTERNAL_UNSIGNED_SHORT_ARRAY_TYPE:
      accumulator->Add("<ExternalUnsignedShortArray[%u]>",
                       ExternalUnsignedShortArray::cast(this)->length());
      break;
    case EXTERNAL_INT_ARRAY_TYPE:
      accumulator->Add("<ExternalIntArray[%u]>",
                       ExternalIntArray::cast(this)->length());
      break;
    case EXTERNAL_UNSIGNED_INT_ARRAY_TYPE:
      accumulator->Add("<ExternalUnsignedIntArray[%u]>",
                       ExternalUnsignedIntArray::cast(this)->length());
      break;
    case EXTERNAL_FLOAT_ARRAY_TYPE:
      accumulator->Add("<ExternalFloatArray[%u]>",
                       ExternalFloatArray::cast(this)->length());
      break;
    case EXTERNAL_DOUBLE_ARRAY_TYPE:
      accumulator->Add("<ExternalDoubleArray[%u]>",
                       ExternalDoubleArray::cast(this)->length());
      break;
    case SHARED_FUNCTION_INFO_TYPE: {
      SharedFunctionInfo* shared = SharedFunctionInfo::cast(this);
      SmartArrayPointer<char> debug_name =
          shared->DebugName()->ToCString();
      if (debug_name[0] != 0) {
        accumulator->Add("<SharedFunctionInfo %s>", *debug_name);
      } else {
        accumulator->Add("<SharedFunctionInfo>");
      }
      break;
    }
    case JS_MESSAGE_OBJECT_TYPE:
      accumulator->Add("<JSMessageObject>");
      break;
#define MAKE_STRUCT_CASE(NAME, Name, name) \
  case NAME##_TYPE:                        \
    accumulator->Put('<');                 \
    accumulator->Add(#Name);               \
    accumulator->Put('>');                 \
    break;
  STRUCT_LIST(MAKE_STRUCT_CASE)
#undef MAKE_STRUCT_CASE
    case CODE_TYPE:
      accumulator->Add("<Code>");
      break;
    case ODDBALL_TYPE: {
      if (IsUndefined())
        accumulator->Add("<undefined>");
      else if (IsTheHole())
        accumulator->Add("<the hole>");
      else if (IsNull())
        accumulator->Add("<null>");
      else if (IsTrue())
        accumulator->Add("<true>");
      else if (IsFalse())
        accumulator->Add("<false>");
      else
        accumulator->Add("<Odd Oddball>");
      break;
    }
    case SYMBOL_TYPE: {
      Symbol* symbol = Symbol::cast(this);
      accumulator->Add("<Symbol: %d", symbol->Hash());
      if (!symbol->name()->IsUndefined()) {
        accumulator->Add(" ");
        String::cast(symbol->name())->StringShortPrint(accumulator);
      }
      accumulator->Add(">");
      break;
    }
    case HEAP_NUMBER_TYPE:
      accumulator->Add("<Number: ");
      HeapNumber::cast(this)->HeapNumberPrint(accumulator);
      accumulator->Put('>');
      break;
    case JS_PROXY_TYPE:
      accumulator->Add("<JSProxy>");
      break;
    case JS_FUNCTION_PROXY_TYPE:
      accumulator->Add("<JSFunctionProxy>");
      break;
    case FOREIGN_TYPE:
      accumulator->Add("<Foreign>");
      break;
    case CELL_TYPE:
      accumulator->Add("Cell for ");
      Cell::cast(this)->value()->ShortPrint(accumulator);
      break;
    case PROPERTY_CELL_TYPE:
      accumulator->Add("PropertyCell for ");
      PropertyCell::cast(this)->value()->ShortPrint(accumulator);
      break;
    default:
      accumulator->Add("<Other heap object (%d)>", map()->instance_type());
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
    case FILLER_TYPE:
    case BYTE_ARRAY_TYPE:
    case FREE_SPACE_TYPE:
    case EXTERNAL_PIXEL_ARRAY_TYPE:
    case EXTERNAL_BYTE_ARRAY_TYPE:
    case EXTERNAL_UNSIGNED_BYTE_ARRAY_TYPE:
    case EXTERNAL_SHORT_ARRAY_TYPE:
    case EXTERNAL_UNSIGNED_SHORT_ARRAY_TYPE:
    case EXTERNAL_INT_ARRAY_TYPE:
    case EXTERNAL_UNSIGNED_INT_ARRAY_TYPE:
    case EXTERNAL_FLOAT_ARRAY_TYPE:
    case EXTERNAL_DOUBLE_ARRAY_TYPE:
      break;
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
  // NaN, +0, and -0 should return the false object
#if __BYTE_ORDER == __LITTLE_ENDIAN
  union IeeeDoubleLittleEndianArchType u;
#elif __BYTE_ORDER == __BIG_ENDIAN
  union IeeeDoubleBigEndianArchType u;
#endif
  u.d = value();
  if (u.bits.exp == 2047) {
    // Detect NaN for IEEE double precision floating point.
    if ((u.bits.man_low | u.bits.man_high) != 0) return false;
  }
  if (u.bits.exp == 0) {
    // Detect +0, and -0 for IEEE double precision floating point.
    if ((u.bits.man_low | u.bits.man_high) == 0) return false;
  }
  return true;
}


void HeapNumber::HeapNumberPrint(FILE* out) {
  PrintF(out, "%.16g", Number());
}


void HeapNumber::HeapNumberPrint(StringStream* accumulator) {
  // The Windows version of vsnprintf can allocate when printing a %g string
  // into a buffer that may not be big enough.  We don't want random memory
  // allocation when producing post-crash stack traces, so we print into a
  // buffer that is plenty big enough for any floating point number, then
  // print that using vsnprintf (which may truncate but never allocate if
  // there is no more space in the buffer).
  EmbeddedVector<char, 100> buffer;
  OS::SNPrintF(buffer, "%.16g", Number());
  accumulator->Add("%s", buffer.start());
}


String* JSReceiver::class_name() {
  if (IsJSFunction() && IsJSFunctionProxy()) {
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


MaybeObject* JSObject::AddFastPropertyUsingMap(Map* new_map,
                                               Name* name,
                                               Object* value,
                                               int field_index,
                                               Representation representation) {
  // This method is used to transition to a field. If we are transitioning to a
  // double field, allocate new storage.
  Object* storage;
  MaybeObject* maybe_storage =
      value->AllocateNewStorageFor(GetHeap(), representation);
  if (!maybe_storage->To(&storage)) return maybe_storage;

  if (map()->unused_property_fields() == 0) {
    int new_unused = new_map->unused_property_fields();
    FixedArray* values;
    MaybeObject* maybe_values =
        properties()->CopySize(properties()->length() + new_unused + 1);
    if (!maybe_values->To(&values)) return maybe_values;

    set_properties(values);
  }

  set_map(new_map);

  FastPropertyAtPut(field_index, storage);
  return value;
}


MaybeObject* JSObject::AddFastProperty(Name* name,
                                       Object* value,
                                       PropertyAttributes attributes,
                                       StoreFromKeyed store_mode,
                                       ValueType value_type,
                                       TransitionFlag flag) {
  ASSERT(!IsJSGlobalProxy());
  ASSERT(DescriptorArray::kNotFound ==
         map()->instance_descriptors()->Search(
             name, map()->NumberOfOwnDescriptors()));

  // Normalize the object if the name is an actual name (not the
  // hidden strings) and is not a real identifier.
  // Normalize the object if it will have too many fast properties.
  Isolate* isolate = GetHeap()->isolate();
  if (!name->IsCacheable(isolate) || TooManyFastProperties(store_mode)) {
    MaybeObject* maybe_failure =
        NormalizeProperties(CLEAR_INOBJECT_PROPERTIES, 0);
    if (maybe_failure->IsFailure()) return maybe_failure;
    return AddSlowProperty(name, value, attributes);
  }

  // Compute the new index for new field.
  int index = map()->NextFreePropertyIndex();

  // Allocate new instance descriptors with (name, index) added
  if (IsJSContextExtensionObject()) value_type = FORCE_TAGGED;
  Representation representation = value->OptimalRepresentation(value_type);

  FieldDescriptor new_field(name, index, attributes, representation);

  Map* new_map;
  MaybeObject* maybe_new_map = map()->CopyAddDescriptor(&new_field, flag);
  if (!maybe_new_map->To(&new_map)) return maybe_new_map;

  int unused_property_fields = map()->unused_property_fields() - 1;
  if (unused_property_fields < 0) {
    unused_property_fields += kFieldsAdded;
  }
  new_map->set_unused_property_fields(unused_property_fields);

  return AddFastPropertyUsingMap(new_map, name, value, index, representation);
}


MaybeObject* JSObject::AddConstantProperty(
    Name* name,
    Object* constant,
    PropertyAttributes attributes,
    TransitionFlag initial_flag) {
  // Allocate new instance descriptors with (name, constant) added
  ConstantDescriptor d(name, constant, attributes);

  TransitionFlag flag =
      // Do not add transitions to global objects.
      (IsGlobalObject() ||
      // Don't add transitions to special properties with non-trivial
      // attributes.
       attributes != NONE)
      ? OMIT_TRANSITION
      : initial_flag;

  Map* new_map;
  MaybeObject* maybe_new_map = map()->CopyAddDescriptor(&d, flag);
  if (!maybe_new_map->To(&new_map)) return maybe_new_map;

  set_map(new_map);
  return constant;
}


// Add property in slow mode
MaybeObject* JSObject::AddSlowProperty(Name* name,
                                       Object* value,
                                       PropertyAttributes attributes) {
  ASSERT(!HasFastProperties());
  NameDictionary* dict = property_dictionary();
  Object* store_value = value;
  if (IsGlobalObject()) {
    // In case name is an orphaned property reuse the cell.
    int entry = dict->FindEntry(name);
    if (entry != NameDictionary::kNotFound) {
      store_value = dict->ValueAt(entry);
      MaybeObject* maybe_type =
          PropertyCell::cast(store_value)->SetValueInferType(value);
      if (maybe_type->IsFailure()) return maybe_type;
      // Assign an enumeration index to the property and update
      // SetNextEnumerationIndex.
      int index = dict->NextEnumerationIndex();
      PropertyDetails details = PropertyDetails(attributes, NORMAL, index);
      dict->SetNextEnumerationIndex(index + 1);
      dict->SetEntry(entry, name, store_value, details);
      return value;
    }
    Heap* heap = GetHeap();
    { MaybeObject* maybe_store_value =
          heap->AllocatePropertyCell(value);
      if (!maybe_store_value->ToObject(&store_value)) return maybe_store_value;
    }
    MaybeObject* maybe_type =
        PropertyCell::cast(store_value)->SetValueInferType(value);
    if (maybe_type->IsFailure()) return maybe_type;
  }
  PropertyDetails details = PropertyDetails(attributes, NORMAL, 0);
  Object* result;
  { MaybeObject* maybe_result = dict->Add(name, store_value, details);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
  if (dict != result) set_properties(NameDictionary::cast(result));
  return value;
}


MaybeObject* JSObject::AddProperty(Name* name,
                                   Object* value,
                                   PropertyAttributes attributes,
                                   StrictModeFlag strict_mode,
                                   JSReceiver::StoreFromKeyed store_mode,
                                   ExtensibilityCheck extensibility_check,
                                   ValueType value_type,
                                   StoreMode mode,
                                   TransitionFlag transition_flag) {
  ASSERT(!IsJSGlobalProxy());
  Map* map_of_this = map();
  Heap* heap = GetHeap();
  Isolate* isolate = heap->isolate();
  MaybeObject* result;
  if (extensibility_check == PERFORM_EXTENSIBILITY_CHECK &&
      !map_of_this->is_extensible()) {
    if (strict_mode == kNonStrictMode) {
      return value;
    } else {
      Handle<Object> args[1] = {Handle<Name>(name)};
      return isolate->Throw(
          *isolate->factory()->NewTypeError("object_not_extensible",
                                            HandleVector(args, 1)));
    }
  }

  if (HasFastProperties()) {
    // Ensure the descriptor array does not get too big.
    if (map_of_this->NumberOfOwnDescriptors() <
        DescriptorArray::kMaxNumberOfDescriptors) {
      // TODO(verwaest): Support other constants.
      // if (mode == ALLOW_AS_CONSTANT &&
      //     !value->IsTheHole() &&
      //     !value->IsConsString()) {
      if (value->IsJSFunction()) {
        result = AddConstantProperty(name, value, attributes, transition_flag);
      } else {
        result = AddFastProperty(
            name, value, attributes, store_mode, value_type, transition_flag);
      }
    } else {
      // Normalize the object to prevent very large instance descriptors.
      // This eliminates unwanted N^2 allocation and lookup behavior.
      Object* obj;
      MaybeObject* maybe = NormalizeProperties(CLEAR_INOBJECT_PROPERTIES, 0);
      if (!maybe->To(&obj)) return maybe;
      result = AddSlowProperty(name, value, attributes);
    }
  } else {
    result = AddSlowProperty(name, value, attributes);
  }

  Handle<Object> hresult;
  if (!result->ToHandle(&hresult, isolate)) return result;

  if (FLAG_harmony_observation && map()->is_observed()) {
    EnqueueChangeRecord(handle(this, isolate),
                        "new",
                        handle(name, isolate),
                        handle(heap->the_hole_value(), isolate));
  }

  return *hresult;
}


void JSObject::EnqueueChangeRecord(Handle<JSObject> object,
                                   const char* type_str,
                                   Handle<Name> name,
                                   Handle<Object> old_value) {
  Isolate* isolate = object->GetIsolate();
  HandleScope scope(isolate);
  Handle<String> type = isolate->factory()->InternalizeUtf8String(type_str);
  if (object->IsJSGlobalObject()) {
    object = handle(JSGlobalObject::cast(*object)->global_receiver(), isolate);
  }
  Handle<Object> args[] = { type, object, name, old_value };
  bool threw;
  Execution::Call(isolate,
                  Handle<JSFunction>(isolate->observers_notify_change()),
                  isolate->factory()->undefined_value(),
                  old_value->IsTheHole() ? 3 : 4, args,
                  &threw);
  ASSERT(!threw);
}


void JSObject::DeliverChangeRecords(Isolate* isolate) {
  ASSERT(isolate->observer_delivery_pending());
  bool threw = false;
  Execution::Call(
      isolate,
      isolate->observers_deliver_changes(),
      isolate->factory()->undefined_value(),
      0,
      NULL,
      &threw);
  ASSERT(!threw);
  isolate->set_observer_delivery_pending(false);
}


MaybeObject* JSObject::SetPropertyPostInterceptor(
    Name* name,
    Object* value,
    PropertyAttributes attributes,
    StrictModeFlag strict_mode,
    StoreMode mode) {
  // Check local property, ignore interceptor.
  LookupResult result(GetIsolate());
  LocalLookupRealNamedProperty(name, &result);
  if (!result.IsFound()) map()->LookupTransition(this, name, &result);
  if (result.IsFound()) {
    // An existing property or a map transition was found. Use set property to
    // handle all these cases.
    return SetProperty(&result, name, value, attributes, strict_mode);
  }
  bool done = false;
  MaybeObject* result_object =
      SetPropertyViaPrototypes(name, value, attributes, strict_mode, &done);
  if (done) return result_object;
  // Add a new real property.
  return AddProperty(name, value, attributes, strict_mode,
                     MAY_BE_STORE_FROM_KEYED, PERFORM_EXTENSIBILITY_CHECK,
                     OPTIMAL_REPRESENTATION, mode);
}


MaybeObject* JSObject::ReplaceSlowProperty(Name* name,
                                           Object* value,
                                           PropertyAttributes attributes) {
  NameDictionary* dictionary = property_dictionary();
  int old_index = dictionary->FindEntry(name);
  int new_enumeration_index = 0;  // 0 means "Use the next available index."
  if (old_index != -1) {
    // All calls to ReplaceSlowProperty have had all transitions removed.
    new_enumeration_index = dictionary->DetailsAt(old_index).dictionary_index();
  }

  PropertyDetails new_details(attributes, NORMAL, new_enumeration_index);
  return SetNormalizedProperty(name, value, new_details);
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


enum RightTrimMode { FROM_GC, FROM_MUTATOR };


static void ZapEndOfFixedArray(Address new_end, int to_trim) {
  // If we are doing a big trim in old space then we zap the space.
  Object** zap = reinterpret_cast<Object**>(new_end);
  zap++;  // Header of filler must be at least one word so skip that.
  for (int i = 1; i < to_trim; i++) {
    *zap++ = Smi::FromInt(0);
  }
}


template<RightTrimMode trim_mode>
static void RightTrimFixedArray(Heap* heap, FixedArray* elms, int to_trim) {
  ASSERT(elms->map() != heap->fixed_cow_array_map());
  // For now this trick is only applied to fixed arrays in new and paged space.
  ASSERT(!heap->lo_space()->Contains(elms));

  const int len = elms->length();

  ASSERT(to_trim < len);

  Address new_end = elms->address() + FixedArray::SizeFor(len - to_trim);

  if (trim_mode != FROM_GC || Heap::ShouldZapGarbage()) {
    ZapEndOfFixedArray(new_end, to_trim);
  }

  int size_delta = to_trim * kPointerSize;

  // Technically in new space this write might be omitted (except for
  // debug mode which iterates through the heap), but to play safer
  // we still do it.
  heap->CreateFillerObjectAt(new_end, size_delta);

  elms->set_length(len - to_trim);

  // Maintain marking consistency for IncrementalMarking.
  if (Marking::IsBlack(Marking::MarkBitFrom(elms))) {
    if (trim_mode == FROM_GC) {
      MemoryChunk::IncrementLiveBytesFromGC(elms->address(), -size_delta);
    } else {
      MemoryChunk::IncrementLiveBytesFromMutator(elms->address(), -size_delta);
    }
  }
}


bool Map::InstancesNeedRewriting(Map* target,
                                 int target_number_of_fields,
                                 int target_inobject,
                                 int target_unused) {
  // If fields were added (or removed), rewrite the instance.
  int number_of_fields = NumberOfFields();
  ASSERT(target_number_of_fields >= number_of_fields);
  if (target_number_of_fields != number_of_fields) return true;

  if (FLAG_track_double_fields) {
    // If smi descriptors were replaced by double descriptors, rewrite.
    DescriptorArray* old_desc = instance_descriptors();
    DescriptorArray* new_desc = target->instance_descriptors();
    int limit = NumberOfOwnDescriptors();
    for (int i = 0; i < limit; i++) {
      if (new_desc->GetDetails(i).representation().IsDouble() &&
          !old_desc->GetDetails(i).representation().IsDouble()) {
        return true;
      }
    }
  }

  // If no fields were added, and no inobject properties were removed, setting
  // the map is sufficient.
  if (target_inobject == inobject_properties()) return false;
  // In-object slack tracking may have reduced the object size of the new map.
  // In that case, succeed if all existing fields were inobject, and they still
  // fit within the new inobject size.
  ASSERT(target_inobject < inobject_properties());
  if (target_number_of_fields <= target_inobject) {
    ASSERT(target_number_of_fields + target_unused == target_inobject);
    return false;
  }
  // Otherwise, properties will need to be moved to the backing store.
  return true;
}


// To migrate an instance to a map:
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
MaybeObject* JSObject::MigrateToMap(Map* new_map) {
  Heap* heap = GetHeap();
  Map* old_map = map();
  int number_of_fields = new_map->NumberOfFields();
  int inobject = new_map->inobject_properties();
  int unused = new_map->unused_property_fields();

  // Nothing to do if no functions were converted to fields.
  if (!old_map->InstancesNeedRewriting(
          new_map, number_of_fields, inobject, unused)) {
    set_map(new_map);
    return this;
  }

  int total_size = number_of_fields + unused;
  int external = total_size - inobject;
  FixedArray* array;
  MaybeObject* maybe_array = heap->AllocateFixedArray(total_size);
  if (!maybe_array->To(&array)) return maybe_array;

  DescriptorArray* old_descriptors = old_map->instance_descriptors();
  DescriptorArray* new_descriptors = new_map->instance_descriptors();
  int descriptors = new_map->NumberOfOwnDescriptors();

  for (int i = 0; i < descriptors; i++) {
    PropertyDetails details = new_descriptors->GetDetails(i);
    if (details.type() != FIELD) continue;
    PropertyDetails old_details = old_descriptors->GetDetails(i);
    if (old_details.type() == CALLBACKS) {
      ASSERT(details.representation().IsTagged());
      continue;
    }
    ASSERT(old_details.type() == CONSTANT ||
           old_details.type() == FIELD);
    Object* value = old_details.type() == CONSTANT
        ? old_descriptors->GetValue(i)
        : RawFastPropertyAt(old_descriptors->GetFieldIndex(i));
    if (FLAG_track_double_fields &&
        !old_details.representation().IsDouble() &&
        details.representation().IsDouble()) {
      if (old_details.representation().IsNone()) value = Smi::FromInt(0);
      // Objects must be allocated in the old object space, since the
      // overall number of HeapNumbers needed for the conversion might
      // exceed the capacity of new space, and we would fail repeatedly
      // trying to migrate the instance.
      MaybeObject* maybe_storage =
          value->AllocateNewStorageFor(heap, details.representation(), TENURED);
      if (!maybe_storage->To(&value)) return maybe_storage;
    }
    ASSERT(!(FLAG_track_double_fields &&
             details.representation().IsDouble() &&
             value->IsSmi()));
    int target_index = new_descriptors->GetFieldIndex(i) - inobject;
    if (target_index < 0) target_index += total_size;
    array->set(target_index, value);
  }

  // From here on we cannot fail anymore.

  // Copy (real) inobject properties. If necessary, stop at number_of_fields to
  // avoid overwriting |one_pointer_filler_map|.
  int limit = Min(inobject, number_of_fields);
  for (int i = 0; i < limit; i++) {
    FastPropertyAtPut(i, array->get(external + i));
  }

  // Create filler object past the new instance size.
  int new_instance_size = new_map->instance_size();
  int instance_size_delta = old_map->instance_size() - new_instance_size;
  ASSERT(instance_size_delta >= 0);
  Address address = this->address() + new_instance_size;
  heap->CreateFillerObjectAt(address, instance_size_delta);

  // If there are properties in the new backing store, trim it to the correct
  // size and install the backing store into the object.
  if (external > 0) {
    RightTrimFixedArray<FROM_MUTATOR>(heap, array, inobject);
    set_properties(array);
  }

  set_map(new_map);

  return this;
}


MaybeObject* JSObject::GeneralizeFieldRepresentation(
    int modify_index,
    Representation new_representation,
    StoreMode store_mode) {
  Map* new_map;
  MaybeObject* maybe_new_map = map()->GeneralizeRepresentation(
      modify_index, new_representation, store_mode);
  if (!maybe_new_map->To(&new_map)) return maybe_new_map;
  if (map() == new_map) return this;

  return MigrateToMap(new_map);
}


int Map::NumberOfFields() {
  DescriptorArray* descriptors = instance_descriptors();
  int result = 0;
  for (int i = 0; i < NumberOfOwnDescriptors(); i++) {
    if (descriptors->GetDetails(i).type() == FIELD) result++;
  }
  return result;
}


MaybeObject* Map::CopyGeneralizeAllRepresentations(
    int modify_index,
    StoreMode store_mode,
    PropertyAttributes attributes,
    const char* reason) {
  Map* new_map;
  MaybeObject* maybe_map = this->Copy();
  if (!maybe_map->To(&new_map)) return maybe_map;

  DescriptorArray* descriptors = new_map->instance_descriptors();
  descriptors->InitializeRepresentations(Representation::Tagged());

  // Unless the instance is being migrated, ensure that modify_index is a field.
  PropertyDetails details = descriptors->GetDetails(modify_index);
  if (store_mode == FORCE_FIELD && details.type() != FIELD) {
    FieldDescriptor d(descriptors->GetKey(modify_index),
                      new_map->NumberOfFields(),
                      attributes,
                      Representation::Tagged());
    d.SetSortedKeyIndex(details.pointer());
    descriptors->Set(modify_index, &d);
    int unused_property_fields = new_map->unused_property_fields() - 1;
    if (unused_property_fields < 0) {
      unused_property_fields += JSObject::kFieldsAdded;
    }
    new_map->set_unused_property_fields(unused_property_fields);
  }

  if (FLAG_trace_generalization) {
    PrintGeneralization(stdout, reason, modify_index,
                        new_map->NumberOfOwnDescriptors(),
                        new_map->NumberOfOwnDescriptors(),
                        details.type() == CONSTANT && store_mode == FORCE_FIELD,
                        Representation::Tagged(), Representation::Tagged());
  }
  return new_map;
}


void Map::DeprecateTransitionTree() {
  if (!FLAG_track_fields) return;
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
  while (current->instance_descriptors() == to_replace) {
    current->SetEnumLength(Map::kInvalidEnumCache);
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


// Returns NULL if the updated map is incompatible.
Map* Map::FindUpdatedMap(int verbatim,
                         int length,
                         DescriptorArray* descriptors) {
  // This can only be called on roots of transition trees.
  ASSERT(GetBackPointer()->IsUndefined());

  Map* current = this;

  for (int i = verbatim; i < length; i++) {
    if (!current->HasTransitionArray()) break;
    Name* name = descriptors->GetKey(i);
    TransitionArray* transitions = current->transitions();
    int transition = transitions->Search(name);
    if (transition == TransitionArray::kNotFound) break;
    current = transitions->GetTarget(transition);
    PropertyDetails details = descriptors->GetDetails(i);
    PropertyDetails target_details =
        current->instance_descriptors()->GetDetails(i);
    if (details.attributes() != target_details.attributes()) return NULL;
    if (details.type() == CALLBACKS) {
      if (target_details.type() != CALLBACKS) return NULL;
      if (descriptors->GetValue(i) !=
              current->instance_descriptors()->GetValue(i)) {
        return NULL;
      }
    }
  }

  return current;
}


Map* Map::FindLastMatchMap(int verbatim,
                           int length,
                           DescriptorArray* descriptors) {
  // This can only be called on roots of transition trees.
  ASSERT(GetBackPointer()->IsUndefined());

  Map* current = this;

  for (int i = verbatim; i < length; i++) {
    if (!current->HasTransitionArray()) break;
    Name* name = descriptors->GetKey(i);
    TransitionArray* transitions = current->transitions();
    int transition = transitions->Search(name);
    if (transition == TransitionArray::kNotFound) break;

    Map* next = transitions->GetTarget(transition);
    DescriptorArray* next_descriptors = next->instance_descriptors();

    if (next_descriptors->GetValue(i) != descriptors->GetValue(i)) break;

    PropertyDetails details = descriptors->GetDetails(i);
    PropertyDetails next_details = next_descriptors->GetDetails(i);
    if (details.type() != next_details.type()) break;
    if (details.attributes() != next_details.attributes()) break;
    if (!details.representation().Equals(next_details.representation())) break;

    current = next;
  }
  return current;
}


// Generalize the representation of the descriptor at |modify_index|.
// This method rewrites the transition tree to reflect the new change. To avoid
// high degrees over polymorphism, and to stabilize quickly, on every rewrite
// the new type is deduced by merging the current type with any potential new
// (partial) version of the type in the transition tree.
// To do this, on each rewrite:
// - Search the root of the transition tree using FindRootMap.
// - Find |updated|, the newest matching version of this map using
//   FindUpdatedMap. This uses the keys in the own map's descriptor array to
//   walk the transition tree.
// - Merge/generalize the descriptor array of the current map and |updated|.
// - Generalize the |modify_index| descriptor using |new_representation|.
// - Walk the tree again starting from the root towards |updated|. Stop at
//   |split_map|, the first map who's descriptor array does not match the merged
//   descriptor array.
// - If |updated| == |split_map|, |updated| is in the expected state. Return it.
// - Otherwise, invalidate the outdated transition target from |updated|, and
//   replace its transition tree with a new branch for the updated descriptors.
MaybeObject* Map::GeneralizeRepresentation(int modify_index,
                                           Representation new_representation,
                                           StoreMode store_mode) {
  Map* old_map = this;
  DescriptorArray* old_descriptors = old_map->instance_descriptors();
  PropertyDetails old_details = old_descriptors->GetDetails(modify_index);
  Representation old_representation = old_details.representation();

  // It's fine to transition from None to anything but double without any
  // modification to the object, because the default uninitialized value for
  // representation None can be overwritten by both smi and tagged values.
  // Doubles, however, would require a box allocation.
  if (old_representation.IsNone() &&
      !new_representation.IsNone() &&
      !new_representation.IsDouble()) {
    old_descriptors->SetRepresentation(modify_index, new_representation);
    return old_map;
  }

  int descriptors = old_map->NumberOfOwnDescriptors();
  Map* root_map = old_map->FindRootMap();

  // Check the state of the root map.
  if (!old_map->EquivalentToForTransition(root_map)) {
    return CopyGeneralizeAllRepresentations(
        modify_index, store_mode, old_details.attributes(), "not equivalent");
  }

  int verbatim = root_map->NumberOfOwnDescriptors();

  if (store_mode != ALLOW_AS_CONSTANT && modify_index < verbatim) {
    return CopyGeneralizeAllRepresentations(
        modify_index, store_mode,
        old_details.attributes(), "root modification");
  }

  Map* updated = root_map->FindUpdatedMap(
      verbatim, descriptors, old_descriptors);
  if (updated == NULL) {
    return CopyGeneralizeAllRepresentations(
        modify_index, store_mode, old_details.attributes(), "incompatible");
  }

  DescriptorArray* updated_descriptors = updated->instance_descriptors();

  int valid = updated->NumberOfOwnDescriptors();

  // Directly change the map if the target map is more general. Ensure that the
  // target type of the modify_index is a FIELD, unless we are migrating.
  if (updated_descriptors->IsMoreGeneralThan(
          verbatim, valid, descriptors, old_descriptors) &&
      (store_mode == ALLOW_AS_CONSTANT ||
       updated_descriptors->GetDetails(modify_index).type() == FIELD)) {
    Representation updated_representation =
        updated_descriptors->GetDetails(modify_index).representation();
    if (new_representation.fits_into(updated_representation)) return updated;
  }

  DescriptorArray* new_descriptors;
  MaybeObject* maybe_descriptors = updated_descriptors->Merge(
      verbatim, valid, descriptors, modify_index, store_mode, old_descriptors);
  if (!maybe_descriptors->To(&new_descriptors)) return maybe_descriptors;
  ASSERT(store_mode == ALLOW_AS_CONSTANT ||
         new_descriptors->GetDetails(modify_index).type() == FIELD);

  old_representation =
      new_descriptors->GetDetails(modify_index).representation();
  Representation updated_representation =
      new_representation.generalize(old_representation);
  if (!updated_representation.Equals(old_representation)) {
    new_descriptors->SetRepresentation(modify_index, updated_representation);
  }

  Map* split_map = root_map->FindLastMatchMap(
      verbatim, descriptors, new_descriptors);

  int split_descriptors = split_map->NumberOfOwnDescriptors();
  // This is shadowed by |updated_descriptors| being more general than
  // |old_descriptors|.
  ASSERT(descriptors != split_descriptors);

  int descriptor = split_descriptors;
  split_map->DeprecateTarget(
      old_descriptors->GetKey(descriptor), new_descriptors);

  if (FLAG_trace_generalization) {
    PrintGeneralization(
        stdout, "", modify_index, descriptor, descriptors,
        old_descriptors->GetDetails(modify_index).type() == CONSTANT &&
            store_mode == FORCE_FIELD,
        old_representation, updated_representation);
  }

  Map* new_map = split_map;
  // Add missing transitions.
  for (; descriptor < descriptors; descriptor++) {
    MaybeObject* maybe_map = new_map->CopyInstallDescriptors(
        descriptor, new_descriptors);
    if (!maybe_map->To(&new_map)) {
      // Create a handle for the last created map to ensure it stays alive
      // during GC. Its descriptor array is too large, but it will be
      // overwritten during retry anyway.
      Handle<Map>(new_map);
      return maybe_map;
    }
    new_map->set_migration_target(true);
  }

  new_map->set_owns_descriptors(true);
  return new_map;
}


Map* Map::CurrentMapForDeprecated() {
  DisallowHeapAllocation no_allocation;
  if (!is_deprecated()) return this;

  DescriptorArray* old_descriptors = instance_descriptors();

  int descriptors = NumberOfOwnDescriptors();
  Map* root_map = FindRootMap();

  // Check the state of the root map.
  if (!EquivalentToForTransition(root_map)) return NULL;
  int verbatim = root_map->NumberOfOwnDescriptors();

  Map* updated = root_map->FindUpdatedMap(
      verbatim, descriptors, old_descriptors);
  if (updated == NULL) return NULL;

  DescriptorArray* updated_descriptors = updated->instance_descriptors();
  int valid = updated->NumberOfOwnDescriptors();
  if (!updated_descriptors->IsMoreGeneralThan(
          verbatim, valid, descriptors, old_descriptors)) {
    return NULL;
  }

  return updated;
}


MaybeObject* JSObject::SetPropertyWithInterceptor(
    Name* name,
    Object* value,
    PropertyAttributes attributes,
    StrictModeFlag strict_mode) {
  // TODO(rossberg): Support symbols in the API.
  if (name->IsSymbol()) return value;
  Isolate* isolate = GetIsolate();
  HandleScope scope(isolate);
  Handle<JSObject> this_handle(this);
  Handle<String> name_handle(String::cast(name));
  Handle<Object> value_handle(value, isolate);
  Handle<InterceptorInfo> interceptor(GetNamedInterceptor());
  if (!interceptor->setter()->IsUndefined()) {
    LOG(isolate, ApiNamedPropertyAccess("interceptor-named-set", this, name));
    PropertyCallbackArguments args(isolate, interceptor->data(), this, this);
    v8::NamedPropertySetterCallback setter =
        v8::ToCData<v8::NamedPropertySetterCallback>(interceptor->setter());
    Handle<Object> value_unhole(value->IsTheHole() ?
                                isolate->heap()->undefined_value() :
                                value,
                                isolate);
    v8::Handle<v8::Value> result = args.Call(setter,
                                             v8::Utils::ToLocal(name_handle),
                                             v8::Utils::ToLocal(value_unhole));
    RETURN_IF_SCHEDULED_EXCEPTION(isolate);
    if (!result.IsEmpty()) return *value_handle;
  }
  MaybeObject* raw_result =
      this_handle->SetPropertyPostInterceptor(*name_handle,
                                              *value_handle,
                                              attributes,
                                              strict_mode);
  RETURN_IF_SCHEDULED_EXCEPTION(isolate);
  return raw_result;
}


Handle<Object> JSReceiver::SetProperty(Handle<JSReceiver> object,
                                       Handle<Name> key,
                                       Handle<Object> value,
                                       PropertyAttributes attributes,
                                       StrictModeFlag strict_mode) {
  CALL_HEAP_FUNCTION(object->GetIsolate(),
                     object->SetProperty(*key, *value, attributes, strict_mode),
                     Object);
}


MaybeObject* JSReceiver::SetPropertyOrFail(
    Handle<JSReceiver> object,
    Handle<Name> key,
    Handle<Object> value,
    PropertyAttributes attributes,
    StrictModeFlag strict_mode,
    JSReceiver::StoreFromKeyed store_mode) {
  CALL_HEAP_FUNCTION_PASS_EXCEPTION(
      object->GetIsolate(),
      object->SetProperty(*key, *value, attributes, strict_mode, store_mode));
}


MaybeObject* JSReceiver::SetProperty(Name* name,
                                     Object* value,
                                     PropertyAttributes attributes,
                                     StrictModeFlag strict_mode,
                                     JSReceiver::StoreFromKeyed store_mode) {
  LookupResult result(GetIsolate());
  LocalLookup(name, &result, true);
  if (!result.IsFound()) {
    map()->LookupTransition(JSObject::cast(this), name, &result);
  }
  return SetProperty(&result, name, value, attributes, strict_mode, store_mode);
}


MaybeObject* JSObject::SetPropertyWithCallback(Object* structure,
                                               Name* name,
                                               Object* value,
                                               JSObject* holder,
                                               StrictModeFlag strict_mode) {
  Isolate* isolate = GetIsolate();
  HandleScope scope(isolate);

  // We should never get here to initialize a const with the hole
  // value since a const declaration would conflict with the setter.
  ASSERT(!value->IsTheHole());
  Handle<Object> value_handle(value, isolate);

  // To accommodate both the old and the new api we switch on the
  // data structure used to store the callbacks.  Eventually foreign
  // callbacks should be phased out.
  if (structure->IsForeign()) {
    AccessorDescriptor* callback =
        reinterpret_cast<AccessorDescriptor*>(
            Foreign::cast(structure)->foreign_address());
    MaybeObject* obj = (callback->setter)(
        isolate, this,  value, callback->data);
    RETURN_IF_SCHEDULED_EXCEPTION(isolate);
    if (obj->IsFailure()) return obj;
    return *value_handle;
  }

  if (structure->IsExecutableAccessorInfo()) {
    // api style callbacks
    ExecutableAccessorInfo* data = ExecutableAccessorInfo::cast(structure);
    if (!data->IsCompatibleReceiver(this)) {
      Handle<Object> name_handle(name, isolate);
      Handle<Object> receiver_handle(this, isolate);
      Handle<Object> args[2] = { name_handle, receiver_handle };
      Handle<Object> error =
          isolate->factory()->NewTypeError("incompatible_method_receiver",
                                           HandleVector(args,
                                                        ARRAY_SIZE(args)));
      return isolate->Throw(*error);
    }
    // TODO(rossberg): Support symbols in the API.
    if (name->IsSymbol()) return value;
    Object* call_obj = data->setter();
    v8::AccessorSetterCallback call_fun =
        v8::ToCData<v8::AccessorSetterCallback>(call_obj);
    if (call_fun == NULL) return value;
    Handle<String> key(String::cast(name));
    LOG(isolate, ApiNamedPropertyAccess("store", this, name));
    PropertyCallbackArguments args(
        isolate, data->data(), this, JSObject::cast(holder));
    args.Call(call_fun,
              v8::Utils::ToLocal(key),
              v8::Utils::ToLocal(value_handle));
    RETURN_IF_SCHEDULED_EXCEPTION(isolate);
    return *value_handle;
  }

  if (structure->IsAccessorPair()) {
    Object* setter = AccessorPair::cast(structure)->setter();
    if (setter->IsSpecFunction()) {
      // TODO(rossberg): nicer would be to cast to some JSCallable here...
     return SetPropertyWithDefinedSetter(JSReceiver::cast(setter), value);
    } else {
      if (strict_mode == kNonStrictMode) {
        return value;
      }
      Handle<Name> key(name);
      Handle<Object> holder_handle(holder, isolate);
      Handle<Object> args[2] = { key, holder_handle };
      return isolate->Throw(
          *isolate->factory()->NewTypeError("no_setter_in_callback",
                                            HandleVector(args, 2)));
    }
  }

  // TODO(dcarney): Handle correctly.
  if (structure->IsDeclaredAccessorInfo()) {
    return value;
  }

  UNREACHABLE();
  return NULL;
}


MaybeObject* JSReceiver::SetPropertyWithDefinedSetter(JSReceiver* setter,
                                                      Object* value) {
  Isolate* isolate = GetIsolate();
  Handle<Object> value_handle(value, isolate);
  Handle<JSReceiver> fun(setter, isolate);
  Handle<JSReceiver> self(this, isolate);
#ifdef ENABLE_DEBUGGER_SUPPORT
  Debug* debug = isolate->debug();
  // Handle stepping into a setter if step into is active.
  // TODO(rossberg): should this apply to getters that are function proxies?
  if (debug->StepInActive() && fun->IsJSFunction()) {
    debug->HandleStepIn(
        Handle<JSFunction>::cast(fun), Handle<Object>::null(), 0, false);
  }
#endif
  bool has_pending_exception;
  Handle<Object> argv[] = { value_handle };
  Execution::Call(
      isolate, fun, self, ARRAY_SIZE(argv), argv, &has_pending_exception);
  // Check for pending exception and return the result.
  if (has_pending_exception) return Failure::Exception();
  return *value_handle;
}


MaybeObject* JSObject::SetElementWithCallbackSetterInPrototypes(
    uint32_t index,
    Object* value,
    bool* found,
    StrictModeFlag strict_mode) {
  Heap* heap = GetHeap();
  for (Object* pt = GetPrototype();
       pt != heap->null_value();
       pt = pt->GetPrototype(GetIsolate())) {
    if (pt->IsJSProxy()) {
      String* name;
      MaybeObject* maybe = heap->Uint32ToString(index);
      if (!maybe->To<String>(&name)) {
        *found = true;  // Force abort
        return maybe;
      }
      return JSProxy::cast(pt)->SetPropertyViaPrototypesWithHandler(
          this, name, value, NONE, strict_mode, found);
    }
    if (!JSObject::cast(pt)->HasDictionaryElements()) {
      continue;
    }
    SeededNumberDictionary* dictionary =
        JSObject::cast(pt)->element_dictionary();
    int entry = dictionary->FindEntry(index);
    if (entry != SeededNumberDictionary::kNotFound) {
      PropertyDetails details = dictionary->DetailsAt(entry);
      if (details.type() == CALLBACKS) {
        *found = true;
        return SetElementWithCallback(dictionary->ValueAt(entry),
                                      index,
                                      value,
                                      JSObject::cast(pt),
                                      strict_mode);
      }
    }
  }
  *found = false;
  return heap->the_hole_value();
}

MaybeObject* JSObject::SetPropertyViaPrototypes(
    Name* name,
    Object* value,
    PropertyAttributes attributes,
    StrictModeFlag strict_mode,
    bool* done) {
  Heap* heap = GetHeap();
  Isolate* isolate = heap->isolate();

  *done = false;
  // We could not find a local property so let's check whether there is an
  // accessor that wants to handle the property, or whether the property is
  // read-only on the prototype chain.
  LookupResult result(isolate);
  LookupRealNamedPropertyInPrototypes(name, &result);
  if (result.IsFound()) {
    switch (result.type()) {
      case NORMAL:
      case FIELD:
      case CONSTANT:
        *done = result.IsReadOnly();
        break;
      case INTERCEPTOR: {
        PropertyAttributes attr =
            result.holder()->GetPropertyAttributeWithInterceptor(
                this, name, true);
        *done = !!(attr & READ_ONLY);
        break;
      }
      case CALLBACKS: {
        if (!FLAG_es5_readonly && result.IsReadOnly()) break;
        *done = true;
        return SetPropertyWithCallback(result.GetCallbackObject(),
            name, value, result.holder(), strict_mode);
      }
      case HANDLER: {
        return result.proxy()->SetPropertyViaPrototypesWithHandler(
            this, name, value, attributes, strict_mode, done);
      }
      case TRANSITION:
      case NONEXISTENT:
        UNREACHABLE();
        break;
    }
  }

  // If we get here with *done true, we have encountered a read-only property.
  if (!FLAG_es5_readonly) *done = false;
  if (*done) {
    if (strict_mode == kNonStrictMode) return value;
    Handle<Object> args[] = { Handle<Object>(name, isolate),
                              Handle<Object>(this, isolate)};
    return isolate->Throw(*isolate->factory()->NewTypeError(
      "strict_read_only_property", HandleVector(args, ARRAY_SIZE(args))));
  }
  return heap->the_hole_value();
}


void Map::EnsureDescriptorSlack(Handle<Map> map, int slack) {
  Handle<DescriptorArray> descriptors(map->instance_descriptors());
  if (slack <= descriptors->NumberOfSlackDescriptors()) return;
  int number_of_descriptors = descriptors->number_of_descriptors();
  Isolate* isolate = map->GetIsolate();
  Handle<DescriptorArray> new_descriptors =
      isolate->factory()->NewDescriptorArray(number_of_descriptors, slack);
  DescriptorArray::WhitenessWitness witness(*new_descriptors);

  for (int i = 0; i < number_of_descriptors; ++i) {
    new_descriptors->CopyFrom(i, *descriptors, i, witness);
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
        isolate->factory()->InternalizedStringFromString(
            Handle<String>(String::cast(entry->name())));
    entry->set_name(*key);
  }

  // Fill in new callback descriptors.  Process the callbacks from
  // back to front so that the last callback with a given name takes
  // precedence over previously added callbacks with that name.
  for (int i = nof_callbacks - 1; i >= 0; i--) {
    AccessorInfo* entry = AccessorInfo::cast(callbacks->get(i));
    Name* key = Name::cast(entry->name());
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
  static bool Contains(Name* key,
                       AccessorInfo* entry,
                       int valid_descriptors,
                       Handle<DescriptorArray> array) {
    return array->Search(key, valid_descriptors) != DescriptorArray::kNotFound;
  }
  static void Insert(Name* key,
                     AccessorInfo* entry,
                     int valid_descriptors,
                     Handle<DescriptorArray> array) {
    CallbacksDescriptor desc(key, entry, entry->property_attributes());
    array->Append(&desc);
  }
};


struct FixedArrayAppender {
  typedef FixedArray Array;
  static bool Contains(Name* key,
                       AccessorInfo* entry,
                       int valid_descriptors,
                       Handle<FixedArray> array) {
    for (int i = 0; i < valid_descriptors; i++) {
      if (key == AccessorInfo::cast(array->get(i))->name()) return true;
    }
    return false;
  }
  static void Insert(Name* key,
                     AccessorInfo* entry,
                     int valid_descriptors,
                     Handle<FixedArray> array) {
    array->set(valid_descriptors, entry);
  }
};


void Map::AppendCallbackDescriptors(Handle<Map> map,
                                    Handle<Object> descriptors) {
  int nof = map->NumberOfOwnDescriptors();
  Handle<DescriptorArray> array(map->instance_descriptors());
  NeanderArray callbacks(descriptors);
  ASSERT(array->NumberOfSlackDescriptors() >= callbacks.length());
  nof = AppendUniqueCallbacks<DescriptorArrayAppender>(&callbacks, array, nof);
  map->SetNumberOfOwnDescriptors(nof);
}


int AccessorInfo::AppendUnique(Handle<Object> descriptors,
                               Handle<FixedArray> array,
                               int valid_descriptors) {
  NeanderArray callbacks(descriptors);
  ASSERT(array->length() >= callbacks.length() + valid_descriptors);
  return AppendUniqueCallbacks<FixedArrayAppender>(&callbacks,
                                                   array,
                                                   valid_descriptors);
}


static bool ContainsMap(MapHandleList* maps, Handle<Map> map) {
  ASSERT(!map.is_null());
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
  int index = GetSequenceIndexFromFastElementsKind(map->elements_kind());
  int to_index = IsFastElementsKind(to_kind)
      ? GetSequenceIndexFromFastElementsKind(to_kind)
      : GetSequenceIndexFromFastElementsKind(TERMINAL_FAST_ELEMENTS_KIND);

  ASSERT(index <= to_index);

  for (; index < to_index; ++index) {
    if (!current_map->HasElementsTransition()) return current_map;
    current_map = current_map->elements_transition_map();
  }
  if (!IsFastElementsKind(to_kind) && current_map->HasElementsTransition()) {
    Map* next_map = current_map->elements_transition_map();
    if (next_map->elements_kind() == to_kind) return next_map;
  }
  ASSERT(IsFastElementsKind(to_kind)
         ? current_map->elements_kind() == to_kind
         : current_map->elements_kind() == TERMINAL_FAST_ELEMENTS_KIND);
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


static MaybeObject* AddMissingElementsTransitions(Map* map,
                                                  ElementsKind to_kind) {
  ASSERT(IsFastElementsKind(map->elements_kind()));
  int index = GetSequenceIndexFromFastElementsKind(map->elements_kind());
  int to_index = IsFastElementsKind(to_kind)
      ? GetSequenceIndexFromFastElementsKind(to_kind)
      : GetSequenceIndexFromFastElementsKind(TERMINAL_FAST_ELEMENTS_KIND);

  ASSERT(index <= to_index);

  Map* current_map = map;

  for (; index < to_index; ++index) {
    ElementsKind next_kind = GetFastElementsKindFromSequenceIndex(index + 1);
    MaybeObject* maybe_next_map =
        current_map->CopyAsElementsKind(next_kind, INSERT_TRANSITION);
    if (!maybe_next_map->To(&current_map)) return maybe_next_map;
  }

  // In case we are exiting the fast elements kind system, just add the map in
  // the end.
  if (!IsFastElementsKind(to_kind)) {
    MaybeObject* maybe_next_map =
        current_map->CopyAsElementsKind(to_kind, INSERT_TRANSITION);
    if (!maybe_next_map->To(&current_map)) return maybe_next_map;
  }

  ASSERT(current_map->elements_kind() == to_kind);
  return current_map;
}


Handle<Map> JSObject::GetElementsTransitionMap(Handle<JSObject> object,
                                               ElementsKind to_kind) {
  Isolate* isolate = object->GetIsolate();
  CALL_HEAP_FUNCTION(isolate,
                     object->GetElementsTransitionMap(isolate, to_kind),
                     Map);
}


MaybeObject* JSObject::GetElementsTransitionMapSlow(ElementsKind to_kind) {
  Map* start_map = map();
  ElementsKind from_kind = start_map->elements_kind();

  if (from_kind == to_kind) {
    return start_map;
  }

  bool allow_store_transition =
      // Only remember the map transition if there is not an already existing
      // non-matching element transition.
      !start_map->IsUndefined() && !start_map->is_shared() &&
      IsFastElementsKind(from_kind);

  // Only store fast element maps in ascending generality.
  if (IsFastElementsKind(to_kind)) {
    allow_store_transition &=
        IsTransitionableFastElementsKind(from_kind) &&
        IsMoreGeneralElementsKindTransition(from_kind, to_kind);
  }

  if (!allow_store_transition) {
    return start_map->CopyAsElementsKind(to_kind, OMIT_TRANSITION);
  }

  return start_map->AsElementsKind(to_kind);
}


MaybeObject* Map::AsElementsKind(ElementsKind kind) {
  Map* closest_map = FindClosestElementsTransition(this, kind);

  if (closest_map->elements_kind() == kind) {
    return closest_map;
  }

  return AddMissingElementsTransitions(closest_map, kind);
}


void JSObject::LocalLookupRealNamedProperty(Name* name, LookupResult* result) {
  if (IsJSGlobalProxy()) {
    Object* proto = GetPrototype();
    if (proto->IsNull()) return result->NotFound();
    ASSERT(proto->IsJSGlobalObject());
    return JSObject::cast(proto)->LocalLookupRealNamedProperty(name, result);
  }

  if (HasFastProperties()) {
    map()->LookupDescriptor(this, name, result);
    // A property or a map transition was found. We return all of these result
    // types because LocalLookupRealNamedProperty is used when setting
    // properties where map transitions are handled.
    ASSERT(!result->IsFound() ||
           (result->holder() == this && result->IsFastPropertyType()));
    // Disallow caching for uninitialized constants. These can only
    // occur as fields.
    if (result->IsField() &&
        result->IsReadOnly() &&
        RawFastPropertyAt(result->GetFieldIndex().field_index())->IsTheHole()) {
      result->DisallowCaching();
    }
    return;
  }

  int entry = property_dictionary()->FindEntry(name);
  if (entry != NameDictionary::kNotFound) {
    Object* value = property_dictionary()->ValueAt(entry);
    if (IsGlobalObject()) {
      PropertyDetails d = property_dictionary()->DetailsAt(entry);
      if (d.IsDeleted()) {
        result->NotFound();
        return;
      }
      value = PropertyCell::cast(value)->value();
    }
    // Make sure to disallow caching for uninitialized constants
    // found in the dictionary-mode objects.
    if (value->IsTheHole()) result->DisallowCaching();
    result->DictionaryResult(this, entry);
    return;
  }

  result->NotFound();
}


void JSObject::LookupRealNamedProperty(Name* name, LookupResult* result) {
  LocalLookupRealNamedProperty(name, result);
  if (result->IsFound()) return;

  LookupRealNamedPropertyInPrototypes(name, result);
}


void JSObject::LookupRealNamedPropertyInPrototypes(Name* name,
                                                   LookupResult* result) {
  Isolate* isolate = GetIsolate();
  Heap* heap = isolate->heap();
  for (Object* pt = GetPrototype();
       pt != heap->null_value();
       pt = pt->GetPrototype(isolate)) {
    if (pt->IsJSProxy()) {
      return result->HandlerResult(JSProxy::cast(pt));
    }
    JSObject::cast(pt)->LocalLookupRealNamedProperty(name, result);
    ASSERT(!(result->IsFound() && result->type() == INTERCEPTOR));
    if (result->IsFound()) return;
  }
  result->NotFound();
}


// We only need to deal with CALLBACKS and INTERCEPTORS
MaybeObject* JSObject::SetPropertyWithFailedAccessCheck(
    LookupResult* result,
    Name* name,
    Object* value,
    bool check_prototype,
    StrictModeFlag strict_mode) {
  if (check_prototype && !result->IsProperty()) {
    LookupRealNamedPropertyInPrototypes(name, result);
  }

  if (result->IsProperty()) {
    if (!result->IsReadOnly()) {
      switch (result->type()) {
        case CALLBACKS: {
          Object* obj = result->GetCallbackObject();
          if (obj->IsAccessorInfo()) {
            AccessorInfo* info = AccessorInfo::cast(obj);
            if (info->all_can_write()) {
              return SetPropertyWithCallback(result->GetCallbackObject(),
                                             name,
                                             value,
                                             result->holder(),
                                             strict_mode);
            }
          } else if (obj->IsAccessorPair()) {
            AccessorPair* pair = AccessorPair::cast(obj);
            if (pair->all_can_read()) {
              return SetPropertyWithCallback(result->GetCallbackObject(),
                                             name,
                                             value,
                                             result->holder(),
                                             strict_mode);
            }
          }
          break;
        }
        case INTERCEPTOR: {
          // Try lookup real named properties. Note that only property can be
          // set is callbacks marked as ALL_CAN_WRITE on the prototype chain.
          LookupResult r(GetIsolate());
          LookupRealNamedProperty(name, &r);
          if (r.IsProperty()) {
            return SetPropertyWithFailedAccessCheck(&r,
                                                    name,
                                                    value,
                                                    check_prototype,
                                                    strict_mode);
          }
          break;
        }
        default: {
          break;
        }
      }
    }
  }

  Isolate* isolate = GetIsolate();
  HandleScope scope(isolate);
  Handle<Object> value_handle(value, isolate);
  isolate->ReportFailedAccessCheck(this, v8::ACCESS_SET);
  RETURN_IF_SCHEDULED_EXCEPTION(isolate);
  return *value_handle;
}


MaybeObject* JSReceiver::SetProperty(LookupResult* result,
                                     Name* key,
                                     Object* value,
                                     PropertyAttributes attributes,
                                     StrictModeFlag strict_mode,
                                     JSReceiver::StoreFromKeyed store_mode) {
  if (result->IsHandler()) {
    return result->proxy()->SetPropertyWithHandler(
        this, key, value, attributes, strict_mode);
  } else {
    return JSObject::cast(this)->SetPropertyForResult(
        result, key, value, attributes, strict_mode, store_mode);
  }
}


bool JSProxy::HasPropertyWithHandler(Name* name_raw) {
  Isolate* isolate = GetIsolate();
  HandleScope scope(isolate);
  Handle<Object> receiver(this, isolate);
  Handle<Object> name(name_raw, isolate);

  // TODO(rossberg): adjust once there is a story for symbols vs proxies.
  if (name->IsSymbol()) return false;

  Handle<Object> args[] = { name };
  Handle<Object> result = CallTrap(
    "has", isolate->derived_has_trap(), ARRAY_SIZE(args), args);
  if (isolate->has_pending_exception()) return false;

  return result->BooleanValue();
}


MUST_USE_RESULT MaybeObject* JSProxy::SetPropertyWithHandler(
    JSReceiver* receiver_raw,
    Name* name_raw,
    Object* value_raw,
    PropertyAttributes attributes,
    StrictModeFlag strict_mode) {
  Isolate* isolate = GetIsolate();
  HandleScope scope(isolate);
  Handle<JSReceiver> receiver(receiver_raw);
  Handle<Object> name(name_raw, isolate);
  Handle<Object> value(value_raw, isolate);

  // TODO(rossberg): adjust once there is a story for symbols vs proxies.
  if (name->IsSymbol()) return *value;

  Handle<Object> args[] = { receiver, name, value };
  CallTrap("set", isolate->derived_set_trap(), ARRAY_SIZE(args), args);
  if (isolate->has_pending_exception()) return Failure::Exception();

  return *value;
}


MUST_USE_RESULT MaybeObject* JSProxy::SetPropertyViaPrototypesWithHandler(
    JSReceiver* receiver_raw,
    Name* name_raw,
    Object* value_raw,
    PropertyAttributes attributes,
    StrictModeFlag strict_mode,
    bool* done) {
  Isolate* isolate = GetIsolate();
  Handle<JSProxy> proxy(this);
  Handle<JSReceiver> receiver(receiver_raw);
  Handle<Name> name(name_raw);
  Handle<Object> value(value_raw, isolate);
  Handle<Object> handler(this->handler(), isolate);  // Trap might morph proxy.

  // TODO(rossberg): adjust once there is a story for symbols vs proxies.
  if (name->IsSymbol()) {
    *done = false;
    return isolate->heap()->the_hole_value();
  }

  *done = true;  // except where redefined...
  Handle<Object> args[] = { name };
  Handle<Object> result = proxy->CallTrap(
      "getPropertyDescriptor", Handle<Object>(), ARRAY_SIZE(args), args);
  if (isolate->has_pending_exception()) return Failure::Exception();

  if (result->IsUndefined()) {
    *done = false;
    return isolate->heap()->the_hole_value();
  }

  // Emulate [[GetProperty]] semantics for proxies.
  bool has_pending_exception;
  Handle<Object> argv[] = { result };
  Handle<Object> desc = Execution::Call(
      isolate, isolate->to_complete_property_descriptor(), result,
      ARRAY_SIZE(argv), argv, &has_pending_exception);
  if (has_pending_exception) return Failure::Exception();

  // [[GetProperty]] requires to check that all properties are configurable.
  Handle<String> configurable_name =
      isolate->factory()->InternalizeOneByteString(
          STATIC_ASCII_VECTOR("configurable_"));
  Handle<Object> configurable(
      v8::internal::GetProperty(isolate, desc, configurable_name));
  ASSERT(!isolate->has_pending_exception());
  ASSERT(configurable->IsTrue() || configurable->IsFalse());
  if (configurable->IsFalse()) {
    Handle<String> trap =
        isolate->factory()->InternalizeOneByteString(
            STATIC_ASCII_VECTOR("getPropertyDescriptor"));
    Handle<Object> args[] = { handler, trap, name };
    Handle<Object> error = isolate->factory()->NewTypeError(
        "proxy_prop_not_configurable", HandleVector(args, ARRAY_SIZE(args)));
    return isolate->Throw(*error);
  }
  ASSERT(configurable->IsTrue());

  // Check for DataDescriptor.
  Handle<String> hasWritable_name =
      isolate->factory()->InternalizeOneByteString(
          STATIC_ASCII_VECTOR("hasWritable_"));
  Handle<Object> hasWritable(
      v8::internal::GetProperty(isolate, desc, hasWritable_name));
  ASSERT(!isolate->has_pending_exception());
  ASSERT(hasWritable->IsTrue() || hasWritable->IsFalse());
  if (hasWritable->IsTrue()) {
    Handle<String> writable_name =
        isolate->factory()->InternalizeOneByteString(
            STATIC_ASCII_VECTOR("writable_"));
    Handle<Object> writable(
        v8::internal::GetProperty(isolate, desc, writable_name));
    ASSERT(!isolate->has_pending_exception());
    ASSERT(writable->IsTrue() || writable->IsFalse());
    *done = writable->IsFalse();
    if (!*done) return GetHeap()->the_hole_value();
    if (strict_mode == kNonStrictMode) return *value;
    Handle<Object> args[] = { name, receiver };
    Handle<Object> error = isolate->factory()->NewTypeError(
        "strict_read_only_property", HandleVector(args, ARRAY_SIZE(args)));
    return isolate->Throw(*error);
  }

  // We have an AccessorDescriptor.
  Handle<String> set_name = isolate->factory()->InternalizeOneByteString(
      STATIC_ASCII_VECTOR("set_"));
  Handle<Object> setter(v8::internal::GetProperty(isolate, desc, set_name));
  ASSERT(!isolate->has_pending_exception());
  if (!setter->IsUndefined()) {
    // TODO(rossberg): nicer would be to cast to some JSCallable here...
    return receiver->SetPropertyWithDefinedSetter(
        JSReceiver::cast(*setter), *value);
  }

  if (strict_mode == kNonStrictMode) return *value;
  Handle<Object> args2[] = { name, proxy };
  Handle<Object> error = isolate->factory()->NewTypeError(
      "no_setter_in_callback", HandleVector(args2, ARRAY_SIZE(args2)));
  return isolate->Throw(*error);
}


Handle<Object> JSProxy::DeletePropertyWithHandler(
    Handle<JSProxy> proxy, Handle<Name> name, DeleteMode mode) {
  Isolate* isolate = proxy->GetIsolate();

  // TODO(rossberg): adjust once there is a story for symbols vs proxies.
  if (name->IsSymbol()) return isolate->factory()->false_value();

  Handle<Object> args[] = { name };
  Handle<Object> result = proxy->CallTrap(
      "delete", Handle<Object>(), ARRAY_SIZE(args), args);
  if (isolate->has_pending_exception()) return Handle<Object>();

  bool result_bool = result->BooleanValue();
  if (mode == STRICT_DELETION && !result_bool) {
    Handle<Object> handler(proxy->handler(), isolate);
    Handle<String> trap_name = isolate->factory()->InternalizeOneByteString(
        STATIC_ASCII_VECTOR("delete"));
    Handle<Object> args[] = { handler, trap_name };
    Handle<Object> error = isolate->factory()->NewTypeError(
        "handler_failed", HandleVector(args, ARRAY_SIZE(args)));
    isolate->Throw(*error);
    return Handle<Object>();
  }
  return isolate->factory()->ToBoolean(result_bool);
}


Handle<Object> JSProxy::DeleteElementWithHandler(
    Handle<JSProxy> proxy, uint32_t index, DeleteMode mode) {
  Isolate* isolate = proxy->GetIsolate();
  Handle<String> name = isolate->factory()->Uint32ToString(index);
  return JSProxy::DeletePropertyWithHandler(proxy, name, mode);
}


MUST_USE_RESULT PropertyAttributes JSProxy::GetPropertyAttributeWithHandler(
    JSReceiver* receiver_raw,
    Name* name_raw) {
  Isolate* isolate = GetIsolate();
  HandleScope scope(isolate);
  Handle<JSProxy> proxy(this);
  Handle<Object> handler(this->handler(), isolate);  // Trap might morph proxy.
  Handle<JSReceiver> receiver(receiver_raw);
  Handle<Object> name(name_raw, isolate);

  // TODO(rossberg): adjust once there is a story for symbols vs proxies.
  if (name->IsSymbol()) return ABSENT;

  Handle<Object> args[] = { name };
  Handle<Object> result = CallTrap(
    "getPropertyDescriptor", Handle<Object>(), ARRAY_SIZE(args), args);
  if (isolate->has_pending_exception()) return NONE;

  if (result->IsUndefined()) return ABSENT;

  bool has_pending_exception;
  Handle<Object> argv[] = { result };
  Handle<Object> desc = Execution::Call(
      isolate, isolate->to_complete_property_descriptor(), result,
      ARRAY_SIZE(argv), argv, &has_pending_exception);
  if (has_pending_exception) return NONE;

  // Convert result to PropertyAttributes.
  Handle<String> enum_n = isolate->factory()->InternalizeOneByteString(
      STATIC_ASCII_VECTOR("enumerable_"));
  Handle<Object> enumerable(v8::internal::GetProperty(isolate, desc, enum_n));
  if (isolate->has_pending_exception()) return NONE;
  Handle<String> conf_n = isolate->factory()->InternalizeOneByteString(
      STATIC_ASCII_VECTOR("configurable_"));
  Handle<Object> configurable(v8::internal::GetProperty(isolate, desc, conf_n));
  if (isolate->has_pending_exception()) return NONE;
  Handle<String> writ_n = isolate->factory()->InternalizeOneByteString(
      STATIC_ASCII_VECTOR("writable_"));
  Handle<Object> writable(v8::internal::GetProperty(isolate, desc, writ_n));
  if (isolate->has_pending_exception()) return NONE;
  if (!writable->BooleanValue()) {
    Handle<String> set_n = isolate->factory()->InternalizeOneByteString(
        STATIC_ASCII_VECTOR("set_"));
    Handle<Object> setter(v8::internal::GetProperty(isolate, desc, set_n));
    if (isolate->has_pending_exception()) return NONE;
    writable = isolate->factory()->ToBoolean(!setter->IsUndefined());
  }

  if (configurable->IsFalse()) {
    Handle<String> trap = isolate->factory()->InternalizeOneByteString(
        STATIC_ASCII_VECTOR("getPropertyDescriptor"));
    Handle<Object> args[] = { handler, trap, name };
    Handle<Object> error = isolate->factory()->NewTypeError(
        "proxy_prop_not_configurable", HandleVector(args, ARRAY_SIZE(args)));
    isolate->Throw(*error);
    return NONE;
  }

  int attributes = NONE;
  if (!enumerable->BooleanValue()) attributes |= DONT_ENUM;
  if (!configurable->BooleanValue()) attributes |= DONT_DELETE;
  if (!writable->BooleanValue()) attributes |= READ_ONLY;
  return static_cast<PropertyAttributes>(attributes);
}


MUST_USE_RESULT PropertyAttributes JSProxy::GetElementAttributeWithHandler(
    JSReceiver* receiver_raw,
    uint32_t index) {
  Isolate* isolate = GetIsolate();
  HandleScope scope(isolate);
  Handle<JSProxy> proxy(this);
  Handle<JSReceiver> receiver(receiver_raw);
  Handle<String> name = isolate->factory()->Uint32ToString(index);
  return proxy->GetPropertyAttributeWithHandler(*receiver, *name);
}


void JSProxy::Fix(Handle<JSProxy> proxy) {
  Isolate* isolate = proxy->GetIsolate();

  // Save identity hash.
  Handle<Object> hash = JSProxy::GetIdentityHash(proxy, OMIT_CREATION);

  if (proxy->IsJSFunctionProxy()) {
    isolate->factory()->BecomeJSFunction(proxy);
    // Code will be set on the JavaScript side.
  } else {
    isolate->factory()->BecomeJSObject(proxy);
  }
  ASSERT(proxy->IsJSObject());

  // Inherit identity, if it was present.
  if (hash->IsSmi()) {
    JSObject::SetIdentityHash(Handle<JSObject>::cast(proxy), Smi::cast(*hash));
  }
}


MUST_USE_RESULT Handle<Object> JSProxy::CallTrap(const char* name,
                                                 Handle<Object> derived,
                                                 int argc,
                                                 Handle<Object> argv[]) {
  Isolate* isolate = GetIsolate();
  Handle<Object> handler(this->handler(), isolate);

  Handle<String> trap_name = isolate->factory()->InternalizeUtf8String(name);
  Handle<Object> trap(v8::internal::GetProperty(isolate, handler, trap_name));
  if (isolate->has_pending_exception()) return trap;

  if (trap->IsUndefined()) {
    if (derived.is_null()) {
      Handle<Object> args[] = { handler, trap_name };
      Handle<Object> error = isolate->factory()->NewTypeError(
        "handler_trap_missing", HandleVector(args, ARRAY_SIZE(args)));
      isolate->Throw(*error);
      return Handle<Object>();
    }
    trap = Handle<Object>(derived);
  }

  bool threw;
  return Execution::Call(isolate, trap, handler, argc, argv, &threw);
}


void JSObject::AllocateStorageForMap(Handle<JSObject> object, Handle<Map> map) {
  CALL_HEAP_FUNCTION_VOID(
      object->GetIsolate(),
      object->AllocateStorageForMap(*map));
}


void JSObject::MigrateInstance(Handle<JSObject> object) {
  CALL_HEAP_FUNCTION_VOID(
      object->GetIsolate(),
      object->MigrateInstance());
}


Handle<Object> JSObject::TryMigrateInstance(Handle<JSObject> object) {
  CALL_HEAP_FUNCTION(
      object->GetIsolate(),
      object->MigrateInstance(),
      Object);
}


Handle<Map> Map::GeneralizeRepresentation(Handle<Map> map,
                                          int modify_index,
                                          Representation representation,
                                          StoreMode store_mode) {
  CALL_HEAP_FUNCTION(
      map->GetIsolate(),
      map->GeneralizeRepresentation(modify_index, representation, store_mode),
      Map);
}


static MaybeObject* SetPropertyUsingTransition(LookupResult* lookup,
                                               Handle<Name> name,
                                               Handle<Object> value,
                                               PropertyAttributes attributes) {
  Map* transition_map = lookup->GetTransitionTarget();
  int descriptor = transition_map->LastAdded();

  DescriptorArray* descriptors = transition_map->instance_descriptors();
  PropertyDetails details = descriptors->GetDetails(descriptor);

  if (details.type() == CALLBACKS || attributes != details.attributes()) {
    // AddProperty will either normalize the object, or create a new fast copy
    // of the map. If we get a fast copy of the map, all field representations
    // will be tagged since the transition is omitted.
    return lookup->holder()->AddProperty(
        *name, *value, attributes, kNonStrictMode,
        JSReceiver::CERTAINLY_NOT_STORE_FROM_KEYED,
        JSReceiver::OMIT_EXTENSIBILITY_CHECK,
        JSObject::FORCE_TAGGED, FORCE_FIELD, OMIT_TRANSITION);
  }

  // Keep the target CONSTANT if the same value is stored.
  // TODO(verwaest): Also support keeping the placeholder
  // (value->IsUninitialized) as constant.
  if (details.type() == CONSTANT &&
      descriptors->GetValue(descriptor) == *value) {
    lookup->holder()->set_map(transition_map);
    return *value;
  }

  Representation representation = details.representation();

  if (!value->FitsRepresentation(representation) ||
      details.type() == CONSTANT) {
    MaybeObject* maybe_map = transition_map->GeneralizeRepresentation(
        descriptor, value->OptimalRepresentation(), FORCE_FIELD);
    if (!maybe_map->To(&transition_map)) return maybe_map;
    Object* back = transition_map->GetBackPointer();
    if (back->IsMap()) {
      MaybeObject* maybe_failure =
          lookup->holder()->MigrateToMap(Map::cast(back));
      if (maybe_failure->IsFailure()) return maybe_failure;
    }
    descriptors = transition_map->instance_descriptors();
    representation = descriptors->GetDetails(descriptor).representation();
  }

  int field_index = descriptors->GetFieldIndex(descriptor);
  return lookup->holder()->AddFastPropertyUsingMap(
      transition_map, *name, *value, field_index, representation);
}


static MaybeObject* SetPropertyToField(LookupResult* lookup,
                                       Handle<Name> name,
                                       Handle<Object> value) {
  Representation representation = lookup->representation();
  if (!value->FitsRepresentation(representation) ||
      lookup->type() == CONSTANT) {
    MaybeObject* maybe_failure =
        lookup->holder()->GeneralizeFieldRepresentation(
            lookup->GetDescriptorIndex(),
            value->OptimalRepresentation(),
            FORCE_FIELD);
    if (maybe_failure->IsFailure()) return maybe_failure;
    DescriptorArray* desc = lookup->holder()->map()->instance_descriptors();
    int descriptor = lookup->GetDescriptorIndex();
    representation = desc->GetDetails(descriptor).representation();
  }

  if (FLAG_track_double_fields && representation.IsDouble()) {
    HeapNumber* storage = HeapNumber::cast(lookup->holder()->RawFastPropertyAt(
        lookup->GetFieldIndex().field_index()));
    storage->set_value(value->Number());
    return *value;
  }

  lookup->holder()->FastPropertyAtPut(
      lookup->GetFieldIndex().field_index(), *value);
  return *value;
}


static MaybeObject* ConvertAndSetLocalProperty(LookupResult* lookup,
                                               Name* name,
                                               Object* value,
                                               PropertyAttributes attributes) {
  JSObject* object = lookup->holder();
  if (object->TooManyFastProperties()) {
    MaybeObject* maybe_failure = object->NormalizeProperties(
        CLEAR_INOBJECT_PROPERTIES, 0);
    if (maybe_failure->IsFailure()) return maybe_failure;
  }

  if (!object->HasFastProperties()) {
    return object->ReplaceSlowProperty(name, value, attributes);
  }

  int descriptor_index = lookup->GetDescriptorIndex();
  if (lookup->GetAttributes() == attributes) {
    MaybeObject* maybe_failure = object->GeneralizeFieldRepresentation(
        descriptor_index, Representation::Tagged(), FORCE_FIELD);
    if (maybe_failure->IsFailure()) return maybe_failure;
  } else {
    Map* map;
    MaybeObject* maybe_map = object->map()->CopyGeneralizeAllRepresentations(
        descriptor_index, FORCE_FIELD, attributes, "attributes mismatch");
    if (!maybe_map->To(&map)) return maybe_map;
    MaybeObject* maybe_failure = object->MigrateToMap(map);
    if (maybe_failure->IsFailure()) return maybe_failure;
  }

  DescriptorArray* descriptors = object->map()->instance_descriptors();
  int index = descriptors->GetDetails(descriptor_index).field_index();
  object->FastPropertyAtPut(index, value);
  return value;
}


static MaybeObject* SetPropertyToFieldWithAttributes(
    LookupResult* lookup,
    Handle<Name> name,
    Handle<Object> value,
    PropertyAttributes attributes) {
  if (lookup->GetAttributes() == attributes) {
    if (value->IsUninitialized()) return *value;
    return SetPropertyToField(lookup, name, value);
  } else {
    return ConvertAndSetLocalProperty(lookup, *name, *value, attributes);
  }
}


MaybeObject* JSObject::SetPropertyForResult(LookupResult* lookup,
                                            Name* name_raw,
                                            Object* value_raw,
                                            PropertyAttributes attributes,
                                            StrictModeFlag strict_mode,
                                            StoreFromKeyed store_mode) {
  Heap* heap = GetHeap();
  Isolate* isolate = heap->isolate();

  // Make sure that the top context does not change when doing callbacks or
  // interceptor calls.
  AssertNoContextChangeWithHandleScope ncc;

  // Optimization for 2-byte strings often used as keys in a decompression
  // dictionary.  We internalize these short keys to avoid constantly
  // reallocating them.
  if (name_raw->IsString() && !name_raw->IsInternalizedString() &&
      String::cast(name_raw)->length() <= 2) {
    Object* internalized_version;
    { MaybeObject* maybe_string_version =
        heap->InternalizeString(String::cast(name_raw));
      if (maybe_string_version->ToObject(&internalized_version)) {
        name_raw = String::cast(internalized_version);
      }
    }
  }

  // Check access rights if needed.
  if (IsAccessCheckNeeded()) {
    if (!isolate->MayNamedAccess(this, name_raw, v8::ACCESS_SET)) {
      return SetPropertyWithFailedAccessCheck(
          lookup, name_raw, value_raw, true, strict_mode);
    }
  }

  if (IsJSGlobalProxy()) {
    Object* proto = GetPrototype();
    if (proto->IsNull()) return value_raw;
    ASSERT(proto->IsJSGlobalObject());
    return JSObject::cast(proto)->SetPropertyForResult(
        lookup, name_raw, value_raw, attributes, strict_mode, store_mode);
  }

  ASSERT(!lookup->IsFound() || lookup->holder() == this ||
         lookup->holder()->map()->is_hidden_prototype());

  // From this point on everything needs to be handlified, because
  // SetPropertyViaPrototypes might call back into JavaScript.
  HandleScope scope(isolate);
  Handle<JSObject> self(this);
  Handle<Name> name(name_raw);
  Handle<Object> value(value_raw, isolate);

  if (!lookup->IsProperty() && !self->IsJSContextExtensionObject()) {
    bool done = false;
    MaybeObject* result_object = self->SetPropertyViaPrototypes(
        *name, *value, attributes, strict_mode, &done);
    if (done) return result_object;
  }

  if (!lookup->IsFound()) {
    // Neither properties nor transitions found.
    return self->AddProperty(
        *name, *value, attributes, strict_mode, store_mode);
  }

  if (lookup->IsProperty() && lookup->IsReadOnly()) {
    if (strict_mode == kStrictMode) {
      Handle<Object> args[] = { name, self };
      return isolate->Throw(*isolate->factory()->NewTypeError(
          "strict_read_only_property", HandleVector(args, ARRAY_SIZE(args))));
    } else {
      return *value;
    }
  }

  Handle<Object> old_value(heap->the_hole_value(), isolate);
  if (FLAG_harmony_observation &&
      map()->is_observed() && lookup->IsDataProperty()) {
    old_value = Object::GetProperty(self, name);
  }

  // This is a real property that is not read-only, or it is a
  // transition or null descriptor and there are no setters in the prototypes.
  MaybeObject* result = *value;
  switch (lookup->type()) {
    case NORMAL:
      result = lookup->holder()->SetNormalizedProperty(lookup, *value);
      break;
    case FIELD:
      result = SetPropertyToField(lookup, name, value);
      break;
    case CONSTANT:
      // Only replace the constant if necessary.
      if (*value == lookup->GetConstant()) return *value;
      result = SetPropertyToField(lookup, name, value);
      break;
    case CALLBACKS: {
      Object* callback_object = lookup->GetCallbackObject();
      return self->SetPropertyWithCallback(
          callback_object, *name, *value, lookup->holder(), strict_mode);
    }
    case INTERCEPTOR:
      result = lookup->holder()->SetPropertyWithInterceptor(
          *name, *value, attributes, strict_mode);
      break;
    case TRANSITION: {
      result = SetPropertyUsingTransition(lookup, name, value, attributes);
      break;
    }
    case HANDLER:
    case NONEXISTENT:
      UNREACHABLE();
  }

  Handle<Object> hresult;
  if (!result->ToHandle(&hresult, isolate)) return result;

  if (FLAG_harmony_observation && self->map()->is_observed()) {
    if (lookup->IsTransition()) {
      EnqueueChangeRecord(self, "new", name, old_value);
    } else {
      LookupResult new_lookup(isolate);
      self->LocalLookup(*name, &new_lookup, true);
      if (new_lookup.IsDataProperty()) {
        Handle<Object> new_value = Object::GetProperty(self, name);
        if (!new_value->SameValue(*old_value)) {
          EnqueueChangeRecord(self, "updated", name, old_value);
        }
      }
    }
  }

  return *hresult;
}


MaybeObject* JSObject::SetLocalPropertyIgnoreAttributesTrampoline(
    Name* key,
    Object* value,
    PropertyAttributes attributes,
    ValueType value_type,
    StoreMode mode,
    ExtensibilityCheck extensibility_check) {
  // TODO(mstarzinger): The trampoline is a giant hack, don't use it anywhere
  // else or handlification people will start hating you for all eternity.
  HandleScope scope(GetIsolate());
  IdempotentPointerToHandleCodeTrampoline trampoline(GetIsolate());
  return trampoline.CallWithReturnValue(
      &JSObject::SetLocalPropertyIgnoreAttributes,
      Handle<JSObject>(this),
      Handle<Name>(key),
      Handle<Object>(value, GetIsolate()),
      attributes,
      value_type,
      mode,
      extensibility_check);
}


// Set a real local property, even if it is READ_ONLY.  If the property is not
// present, add it with attributes NONE.  This code is an exact clone of
// SetProperty, with the check for IsReadOnly and the check for a
// callback setter removed.  The two lines looking up the LookupResult
// result are also added.  If one of the functions is changed, the other
// should be.
// Note that this method cannot be used to set the prototype of a function
// because ConvertDescriptorToField() which is called in "case CALLBACKS:"
// doesn't handle function prototypes correctly.
Handle<Object> JSObject::SetLocalPropertyIgnoreAttributes(
    Handle<JSObject> object,
    Handle<Name> key,
    Handle<Object> value,
    PropertyAttributes attributes,
    ValueType value_type,
    StoreMode mode,
    ExtensibilityCheck extensibility_check) {
  CALL_HEAP_FUNCTION(
    object->GetIsolate(),
    object->SetLocalPropertyIgnoreAttributes(
        *key, *value, attributes, value_type, mode, extensibility_check),
    Object);
}


MaybeObject* JSObject::SetLocalPropertyIgnoreAttributes(
    Name* name_raw,
    Object* value_raw,
    PropertyAttributes attributes,
    ValueType value_type,
    StoreMode mode,
    ExtensibilityCheck extensibility_check) {
  // Make sure that the top context does not change when doing callbacks or
  // interceptor calls.
  AssertNoContextChangeWithHandleScope ncc;
  Isolate* isolate = GetIsolate();
  LookupResult lookup(isolate);
  LocalLookup(name_raw, &lookup, true);
  if (!lookup.IsFound()) map()->LookupTransition(this, name_raw, &lookup);
  // Check access rights if needed.
  if (IsAccessCheckNeeded()) {
    if (!isolate->MayNamedAccess(this, name_raw, v8::ACCESS_SET)) {
      return SetPropertyWithFailedAccessCheck(&lookup,
                                              name_raw,
                                              value_raw,
                                              false,
                                              kNonStrictMode);
    }
  }

  if (IsJSGlobalProxy()) {
    Object* proto = GetPrototype();
    if (proto->IsNull()) return value_raw;
    ASSERT(proto->IsJSGlobalObject());
    return JSObject::cast(proto)->SetLocalPropertyIgnoreAttributes(
        name_raw,
        value_raw,
        attributes,
        value_type,
        mode,
        extensibility_check);
  }

  if (lookup.IsFound() &&
      (lookup.type() == INTERCEPTOR || lookup.type() == CALLBACKS)) {
    LocalLookupRealNamedProperty(name_raw, &lookup);
  }

  // Check for accessor in prototype chain removed here in clone.
  if (!lookup.IsFound()) {
    // Neither properties nor transitions found.
    return AddProperty(
        name_raw, value_raw, attributes, kNonStrictMode,
        MAY_BE_STORE_FROM_KEYED, extensibility_check, value_type, mode);
  }

  // From this point on everything needs to be handlified.
  HandleScope scope(isolate);
  Handle<JSObject> self(this);
  Handle<Name> name(name_raw);
  Handle<Object> value(value_raw, isolate);

  Handle<Object> old_value(isolate->heap()->the_hole_value(), isolate);
  PropertyAttributes old_attributes = ABSENT;
  bool is_observed = FLAG_harmony_observation && self->map()->is_observed();
  if (is_observed && lookup.IsProperty()) {
    if (lookup.IsDataProperty()) old_value =
        Object::GetProperty(self, name);
    old_attributes = lookup.GetAttributes();
  }

  // Check of IsReadOnly removed from here in clone.
  MaybeObject* result = *value;
  switch (lookup.type()) {
    case NORMAL:
      result = self->ReplaceSlowProperty(*name, *value, attributes);
      break;
    case FIELD:
      result = SetPropertyToFieldWithAttributes(
          &lookup, name, value, attributes);
      break;
    case CONSTANT:
      // Only replace the constant if necessary.
      if (lookup.GetAttributes() != attributes ||
          *value != lookup.GetConstant()) {
        result = SetPropertyToFieldWithAttributes(
            &lookup, name, value, attributes);
      }
      break;
    case CALLBACKS:
      result = ConvertAndSetLocalProperty(&lookup, *name, *value, attributes);
      break;
    case TRANSITION:
      result = SetPropertyUsingTransition(&lookup, name, value, attributes);
      break;
    case NONEXISTENT:
    case HANDLER:
    case INTERCEPTOR:
      UNREACHABLE();
  }

  Handle<Object> hresult;
  if (!result->ToHandle(&hresult, isolate)) return result;

  if (is_observed) {
    if (lookup.IsTransition()) {
      EnqueueChangeRecord(self, "new", name, old_value);
    } else if (old_value->IsTheHole()) {
      EnqueueChangeRecord(self, "reconfigured", name, old_value);
    } else {
      LookupResult new_lookup(isolate);
      self->LocalLookup(*name, &new_lookup, true);
      bool value_changed = false;
      if (new_lookup.IsDataProperty()) {
        Handle<Object> new_value = Object::GetProperty(self, name);
        value_changed = !old_value->SameValue(*new_value);
      }
      if (new_lookup.GetAttributes() != old_attributes) {
        if (!value_changed) old_value = isolate->factory()->the_hole_value();
        EnqueueChangeRecord(self, "reconfigured", name, old_value);
      } else if (value_changed) {
        EnqueueChangeRecord(self, "updated", name, old_value);
      }
    }
  }

  return *hresult;
}


PropertyAttributes JSObject::GetPropertyAttributePostInterceptor(
      JSObject* receiver,
      Name* name,
      bool continue_search) {
  // Check local property, ignore interceptor.
  LookupResult result(GetIsolate());
  LocalLookupRealNamedProperty(name, &result);
  if (result.IsFound()) return result.GetAttributes();

  if (continue_search) {
    // Continue searching via the prototype chain.
    Object* pt = GetPrototype();
    if (!pt->IsNull()) {
      return JSObject::cast(pt)->
        GetPropertyAttributeWithReceiver(receiver, name);
    }
  }
  return ABSENT;
}


PropertyAttributes JSObject::GetPropertyAttributeWithInterceptor(
      JSObject* receiver,
      Name* name,
      bool continue_search) {
  // TODO(rossberg): Support symbols in the API.
  if (name->IsSymbol()) return ABSENT;

  Isolate* isolate = GetIsolate();
  HandleScope scope(isolate);

  // Make sure that the top context does not change when doing
  // callbacks or interceptor calls.
  AssertNoContextChange ncc;

  Handle<InterceptorInfo> interceptor(GetNamedInterceptor());
  Handle<JSObject> receiver_handle(receiver);
  Handle<JSObject> holder_handle(this);
  Handle<String> name_handle(String::cast(name));
  PropertyCallbackArguments args(isolate, interceptor->data(), receiver, this);
  if (!interceptor->query()->IsUndefined()) {
    v8::NamedPropertyQueryCallback query =
        v8::ToCData<v8::NamedPropertyQueryCallback>(interceptor->query());
    LOG(isolate,
        ApiNamedPropertyAccess("interceptor-named-has", *holder_handle, name));
    v8::Handle<v8::Integer> result =
        args.Call(query, v8::Utils::ToLocal(name_handle));
    if (!result.IsEmpty()) {
      ASSERT(result->IsInt32());
      return static_cast<PropertyAttributes>(result->Int32Value());
    }
  } else if (!interceptor->getter()->IsUndefined()) {
    v8::NamedPropertyGetterCallback getter =
        v8::ToCData<v8::NamedPropertyGetterCallback>(interceptor->getter());
    LOG(isolate,
        ApiNamedPropertyAccess("interceptor-named-get-has", this, name));
    v8::Handle<v8::Value> result =
        args.Call(getter, v8::Utils::ToLocal(name_handle));
    if (!result.IsEmpty()) return DONT_ENUM;
  }
  return holder_handle->GetPropertyAttributePostInterceptor(*receiver_handle,
                                                            *name_handle,
                                                            continue_search);
}


PropertyAttributes JSReceiver::GetPropertyAttributeWithReceiver(
      JSReceiver* receiver,
      Name* key) {
  uint32_t index = 0;
  if (IsJSObject() && key->AsArrayIndex(&index)) {
    return JSObject::cast(this)->GetElementAttributeWithReceiver(
        receiver, index, true);
  }
  // Named property.
  LookupResult lookup(GetIsolate());
  Lookup(key, &lookup);
  return GetPropertyAttributeForResult(receiver, &lookup, key, true);
}


PropertyAttributes JSReceiver::GetPropertyAttributeForResult(
    JSReceiver* receiver,
    LookupResult* lookup,
    Name* name,
    bool continue_search) {
  // Check access rights if needed.
  if (IsAccessCheckNeeded()) {
    JSObject* this_obj = JSObject::cast(this);
    Heap* heap = GetHeap();
    if (!heap->isolate()->MayNamedAccess(this_obj, name, v8::ACCESS_HAS)) {
      return this_obj->GetPropertyAttributeWithFailedAccessCheck(
          receiver, lookup, name, continue_search);
    }
  }
  if (lookup->IsFound()) {
    switch (lookup->type()) {
      case NORMAL:  // fall through
      case FIELD:
      case CONSTANT:
      case CALLBACKS:
        return lookup->GetAttributes();
      case HANDLER: {
        return JSProxy::cast(lookup->proxy())->GetPropertyAttributeWithHandler(
            receiver, name);
      }
      case INTERCEPTOR:
        return lookup->holder()->GetPropertyAttributeWithInterceptor(
            JSObject::cast(receiver), name, continue_search);
      case TRANSITION:
      case NONEXISTENT:
        UNREACHABLE();
    }
  }
  return ABSENT;
}


PropertyAttributes JSReceiver::GetLocalPropertyAttribute(Name* name) {
  // Check whether the name is an array index.
  uint32_t index = 0;
  if (IsJSObject() && name->AsArrayIndex(&index)) {
    return GetLocalElementAttribute(index);
  }
  // Named property.
  LookupResult lookup(GetIsolate());
  LocalLookup(name, &lookup, true);
  return GetPropertyAttributeForResult(this, &lookup, name, false);
}


PropertyAttributes JSObject::GetElementAttributeWithReceiver(
    JSReceiver* receiver, uint32_t index, bool continue_search) {
  Isolate* isolate = GetIsolate();

  // Check access rights if needed.
  if (IsAccessCheckNeeded()) {
    if (!isolate->MayIndexedAccess(this, index, v8::ACCESS_HAS)) {
      isolate->ReportFailedAccessCheck(this, v8::ACCESS_HAS);
      return ABSENT;
    }
  }

  if (IsJSGlobalProxy()) {
    Object* proto = GetPrototype();
    if (proto->IsNull()) return ABSENT;
    ASSERT(proto->IsJSGlobalObject());
    return JSObject::cast(proto)->GetElementAttributeWithReceiver(
        receiver, index, continue_search);
  }

  // Check for lookup interceptor except when bootstrapping.
  if (HasIndexedInterceptor() && !isolate->bootstrapper()->IsActive()) {
    return GetElementAttributeWithInterceptor(receiver, index, continue_search);
  }

  return GetElementAttributeWithoutInterceptor(
      receiver, index, continue_search);
}


PropertyAttributes JSObject::GetElementAttributeWithInterceptor(
    JSReceiver* receiver, uint32_t index, bool continue_search) {
  Isolate* isolate = GetIsolate();
  HandleScope scope(isolate);

  // Make sure that the top context does not change when doing
  // callbacks or interceptor calls.
  AssertNoContextChange ncc;

  Handle<InterceptorInfo> interceptor(GetIndexedInterceptor());
  Handle<JSReceiver> hreceiver(receiver);
  Handle<JSObject> holder(this);
  PropertyCallbackArguments args(isolate, interceptor->data(), receiver, this);
  if (!interceptor->query()->IsUndefined()) {
    v8::IndexedPropertyQueryCallback query =
        v8::ToCData<v8::IndexedPropertyQueryCallback>(interceptor->query());
    LOG(isolate,
        ApiIndexedPropertyAccess("interceptor-indexed-has", this, index));
    v8::Handle<v8::Integer> result = args.Call(query, index);
    if (!result.IsEmpty())
      return static_cast<PropertyAttributes>(result->Int32Value());
  } else if (!interceptor->getter()->IsUndefined()) {
    v8::IndexedPropertyGetterCallback getter =
        v8::ToCData<v8::IndexedPropertyGetterCallback>(interceptor->getter());
    LOG(isolate,
        ApiIndexedPropertyAccess("interceptor-indexed-get-has", this, index));
    v8::Handle<v8::Value> result = args.Call(getter, index);
    if (!result.IsEmpty()) return NONE;
  }

  return holder->GetElementAttributeWithoutInterceptor(
      *hreceiver, index, continue_search);
}


PropertyAttributes JSObject::GetElementAttributeWithoutInterceptor(
      JSReceiver* receiver, uint32_t index, bool continue_search) {
  PropertyAttributes attr = GetElementsAccessor()->GetAttributes(
      receiver, this, index);
  if (attr != ABSENT) return attr;

  // Handle [] on String objects.
  if (IsStringObjectWithCharacterAt(index)) {
    return static_cast<PropertyAttributes>(READ_ONLY | DONT_DELETE);
  }

  if (!continue_search) return ABSENT;

  Object* pt = GetPrototype();
  if (pt->IsJSProxy()) {
    // We need to follow the spec and simulate a call to [[GetOwnProperty]].
    return JSProxy::cast(pt)->GetElementAttributeWithHandler(receiver, index);
  }
  if (pt->IsNull()) return ABSENT;
  return JSObject::cast(pt)->GetElementAttributeWithReceiver(
      receiver, index, true);
}


MaybeObject* NormalizedMapCache::Get(JSObject* obj,
                                     PropertyNormalizationMode mode) {
  Isolate* isolate = obj->GetIsolate();
  Map* fast = obj->map();
  int index = fast->Hash() % kEntries;
  Object* result = get(index);
  if (result->IsMap() &&
      Map::cast(result)->EquivalentToForNormalization(fast, mode)) {
#ifdef VERIFY_HEAP
    if (FLAG_verify_heap) {
      Map::cast(result)->SharedMapVerify();
    }
#endif
#ifdef DEBUG
    if (FLAG_enable_slow_asserts) {
      // The cached map should match newly created normalized map bit-by-bit,
      // except for the code cache, which can contain some ics which can be
      // applied to the shared map.
      Object* fresh;
      MaybeObject* maybe_fresh =
          fast->CopyNormalized(mode, SHARED_NORMALIZED_MAP);
      if (maybe_fresh->ToObject(&fresh)) {
        ASSERT(memcmp(Map::cast(fresh)->address(),
                      Map::cast(result)->address(),
                      Map::kCodeCacheOffset) == 0);
        STATIC_ASSERT(Map::kDependentCodeOffset ==
                      Map::kCodeCacheOffset + kPointerSize);
        int offset = Map::kDependentCodeOffset + kPointerSize;
        ASSERT(memcmp(Map::cast(fresh)->address() + offset,
                      Map::cast(result)->address() + offset,
                      Map::kSize - offset) == 0);
      }
    }
#endif
    return result;
  }

  { MaybeObject* maybe_result =
        fast->CopyNormalized(mode, SHARED_NORMALIZED_MAP);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
  ASSERT(Map::cast(result)->is_dictionary_map());
  set(index, result);
  isolate->counters()->normalized_maps()->Increment();

  return result;
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
  if (map->is_shared()) {
    Handle<JSObject> receiver = Handle<JSObject>::cast(object);
    // Fast case maps are never marked as shared.
    ASSERT(!receiver->HasFastProperties());
    // Replace the map with an identical copy that can be safely modified.
    map = Map::CopyNormalized(map, KEEP_INOBJECT_PROPERTIES,
                              UNIQUE_NORMALIZED_MAP);
    receiver->GetIsolate()->counters()->normalized_maps()->Increment();
    receiver->set_map(*map);
  }
  Map::UpdateCodeCache(map, name, code);
}


void JSObject::NormalizeProperties(Handle<JSObject> object,
                                   PropertyNormalizationMode mode,
                                   int expected_additional_properties) {
  CALL_HEAP_FUNCTION_VOID(object->GetIsolate(),
                          object->NormalizeProperties(
                              mode, expected_additional_properties));
}


MaybeObject* JSObject::NormalizeProperties(PropertyNormalizationMode mode,
                                           int expected_additional_properties) {
  if (!HasFastProperties()) return this;

  // The global object is always normalized.
  ASSERT(!IsGlobalObject());
  // JSGlobalProxy must never be normalized
  ASSERT(!IsJSGlobalProxy());

  Map* map_of_this = map();

  // Allocate new content.
  int real_size = map_of_this->NumberOfOwnDescriptors();
  int property_count = real_size;
  if (expected_additional_properties > 0) {
    property_count += expected_additional_properties;
  } else {
    property_count += 2;  // Make space for two more properties.
  }
  NameDictionary* dictionary;
  MaybeObject* maybe_dictionary =
      NameDictionary::Allocate(GetHeap(), property_count);
  if (!maybe_dictionary->To(&dictionary)) return maybe_dictionary;

  DescriptorArray* descs = map_of_this->instance_descriptors();
  for (int i = 0; i < real_size; i++) {
    PropertyDetails details = descs->GetDetails(i);
    switch (details.type()) {
      case CONSTANT: {
        PropertyDetails d = PropertyDetails(
            details.attributes(), NORMAL, i + 1);
        Object* value = descs->GetConstant(i);
        MaybeObject* maybe_dictionary =
            dictionary->Add(descs->GetKey(i), value, d);
        if (!maybe_dictionary->To(&dictionary)) return maybe_dictionary;
        break;
      }
      case FIELD: {
        PropertyDetails d =
            PropertyDetails(details.attributes(), NORMAL, i + 1);
        Object* value = RawFastPropertyAt(descs->GetFieldIndex(i));
        MaybeObject* maybe_dictionary =
            dictionary->Add(descs->GetKey(i), value, d);
        if (!maybe_dictionary->To(&dictionary)) return maybe_dictionary;
        break;
      }
      case CALLBACKS: {
        Object* value = descs->GetCallbacksObject(i);
        PropertyDetails d = PropertyDetails(
            details.attributes(), CALLBACKS, i + 1);
        MaybeObject* maybe_dictionary =
            dictionary->Add(descs->GetKey(i), value, d);
        if (!maybe_dictionary->To(&dictionary)) return maybe_dictionary;
        break;
      }
      case INTERCEPTOR:
        break;
      case HANDLER:
      case NORMAL:
      case TRANSITION:
      case NONEXISTENT:
        UNREACHABLE();
        break;
    }
  }

  Heap* current_heap = GetHeap();

  // Copy the next enumeration index from instance descriptor.
  dictionary->SetNextEnumerationIndex(real_size + 1);

  Map* new_map;
  MaybeObject* maybe_map =
      current_heap->isolate()->context()->native_context()->
      normalized_map_cache()->Get(this, mode);
  if (!maybe_map->To(&new_map)) return maybe_map;
  ASSERT(new_map->is_dictionary_map());

  // We have now successfully allocated all the necessary objects.
  // Changes can now be made with the guarantee that all of them take effect.

  // Resize the object in the heap if necessary.
  int new_instance_size = new_map->instance_size();
  int instance_size_delta = map_of_this->instance_size() - new_instance_size;
  ASSERT(instance_size_delta >= 0);
  current_heap->CreateFillerObjectAt(this->address() + new_instance_size,
                                     instance_size_delta);
  if (Marking::IsBlack(Marking::MarkBitFrom(this))) {
    MemoryChunk::IncrementLiveBytesFromMutator(this->address(),
                                               -instance_size_delta);
  }

  set_map(new_map);
  map_of_this->NotifyLeafMapLayoutChange();

  set_properties(dictionary);

  current_heap->isolate()->counters()->props_to_dictionary()->Increment();

#ifdef DEBUG
  if (FLAG_trace_normalization) {
    PrintF("Object properties have been normalized:\n");
    Print();
  }
#endif
  return this;
}


void JSObject::TransformToFastProperties(Handle<JSObject> object,
                                         int unused_property_fields) {
  CALL_HEAP_FUNCTION_VOID(
      object->GetIsolate(),
      object->TransformToFastProperties(unused_property_fields));
}


MaybeObject* JSObject::TransformToFastProperties(int unused_property_fields) {
  if (HasFastProperties()) return this;
  ASSERT(!IsGlobalObject());
  return property_dictionary()->
      TransformPropertiesToFastFor(this, unused_property_fields);
}


static MUST_USE_RESULT MaybeObject* CopyFastElementsToDictionary(
    Isolate* isolate,
    FixedArrayBase* array,
    int length,
    SeededNumberDictionary* dictionary) {
  Heap* heap = isolate->heap();
  bool has_double_elements = array->IsFixedDoubleArray();
  for (int i = 0; i < length; i++) {
    Object* value = NULL;
    if (has_double_elements) {
      FixedDoubleArray* double_array = FixedDoubleArray::cast(array);
      if (double_array->is_the_hole(i)) {
        value = isolate->heap()->the_hole_value();
      } else {
        // Objects must be allocated in the old object space, since the
        // overall number of HeapNumbers needed for the conversion might
        // exceed the capacity of new space, and we would fail repeatedly
        // trying to convert the FixedDoubleArray.
        MaybeObject* maybe_value_object =
            heap->AllocateHeapNumber(double_array->get_scalar(i), TENURED);
        if (!maybe_value_object->ToObject(&value)) return maybe_value_object;
      }
    } else {
      value = FixedArray::cast(array)->get(i);
    }
    if (!value->IsTheHole()) {
      PropertyDetails details = PropertyDetails(NONE, NORMAL, 0);
      MaybeObject* maybe_result =
          dictionary->AddNumberEntry(i, value, details);
      if (!maybe_result->To(&dictionary)) return maybe_result;
    }
  }
  return dictionary;
}


Handle<SeededNumberDictionary> JSObject::NormalizeElements(
    Handle<JSObject> object) {
  CALL_HEAP_FUNCTION(object->GetIsolate(),
                     object->NormalizeElements(),
                     SeededNumberDictionary);
}


MaybeObject* JSObject::NormalizeElements() {
  ASSERT(!HasExternalArrayElements());

  // Find the backing store.
  FixedArrayBase* array = FixedArrayBase::cast(elements());
  Map* old_map = array->map();
  bool is_arguments =
      (old_map == old_map->GetHeap()->non_strict_arguments_elements_map());
  if (is_arguments) {
    array = FixedArrayBase::cast(FixedArray::cast(array)->get(1));
  }
  if (array->IsDictionary()) return array;

  ASSERT(HasFastSmiOrObjectElements() ||
         HasFastDoubleElements() ||
         HasFastArgumentsElements());
  // Compute the effective length and allocate a new backing store.
  int length = IsJSArray()
      ? Smi::cast(JSArray::cast(this)->length())->value()
      : array->length();
  int old_capacity = 0;
  int used_elements = 0;
  GetElementsCapacityAndUsage(&old_capacity, &used_elements);
  SeededNumberDictionary* dictionary;
  MaybeObject* maybe_dictionary =
      SeededNumberDictionary::Allocate(GetHeap(), used_elements);
  if (!maybe_dictionary->To(&dictionary)) return maybe_dictionary;

  maybe_dictionary = CopyFastElementsToDictionary(
      GetIsolate(), array, length, dictionary);
  if (!maybe_dictionary->To(&dictionary)) return maybe_dictionary;

  // Switch to using the dictionary as the backing storage for elements.
  if (is_arguments) {
    FixedArray::cast(elements())->set(1, dictionary);
  } else {
    // Set the new map first to satify the elements type assert in
    // set_elements().
    Map* new_map;
    MaybeObject* maybe = GetElementsTransitionMap(GetIsolate(),
                                                  DICTIONARY_ELEMENTS);
    if (!maybe->To(&new_map)) return maybe;
    set_map(new_map);
    set_elements(dictionary);
  }

  old_map->GetHeap()->isolate()->counters()->elements_to_dictionary()->
      Increment();

#ifdef DEBUG
  if (FLAG_trace_normalization) {
    PrintF("Object elements have been normalized:\n");
    Print();
  }
#endif

  ASSERT(HasDictionaryElements() || HasDictionaryArgumentsElements());
  return dictionary;
}


Smi* JSReceiver::GenerateIdentityHash() {
  Isolate* isolate = GetIsolate();

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


void JSObject::SetIdentityHash(Handle<JSObject> object, Smi* hash) {
  CALL_HEAP_FUNCTION_VOID(object->GetIsolate(),
                          object->SetHiddenProperty(
                              object->GetHeap()->identity_hash_string(), hash));
}


int JSObject::GetIdentityHash(Handle<JSObject> object) {
  CALL_AND_RETRY_OR_DIE(object->GetIsolate(),
                        object->GetIdentityHash(ALLOW_CREATION),
                        return Smi::cast(__object__)->value(),
                        return 0);
}


MaybeObject* JSObject::GetIdentityHash(CreationFlag flag) {
  Object* stored_value = GetHiddenProperty(GetHeap()->identity_hash_string());
  if (stored_value->IsSmi()) return stored_value;

  // Do not generate permanent identity hash code if not requested.
  if (flag == OMIT_CREATION) return GetHeap()->undefined_value();

  Smi* hash = GenerateIdentityHash();
  MaybeObject* result = SetHiddenProperty(GetHeap()->identity_hash_string(),
                                          hash);
  if (result->IsFailure()) return result;
  if (result->ToObjectUnchecked()->IsUndefined()) {
    // Trying to get hash of detached proxy.
    return Smi::FromInt(0);
  }
  return hash;
}


Handle<Object> JSProxy::GetIdentityHash(Handle<JSProxy> proxy,
                                        CreationFlag flag) {
  CALL_HEAP_FUNCTION(proxy->GetIsolate(), proxy->GetIdentityHash(flag), Object);
}


MaybeObject* JSProxy::GetIdentityHash(CreationFlag flag) {
  Object* hash = this->hash();
  if (!hash->IsSmi() && flag == ALLOW_CREATION) {
    hash = GenerateIdentityHash();
    set_hash(hash);
  }
  return hash;
}


Object* JSObject::GetHiddenProperty(Name* key) {
  ASSERT(key->IsUniqueName());
  if (IsJSGlobalProxy()) {
    // For a proxy, use the prototype as target object.
    Object* proxy_parent = GetPrototype();
    // If the proxy is detached, return undefined.
    if (proxy_parent->IsNull()) return GetHeap()->the_hole_value();
    ASSERT(proxy_parent->IsJSGlobalObject());
    return JSObject::cast(proxy_parent)->GetHiddenProperty(key);
  }
  ASSERT(!IsJSGlobalProxy());
  MaybeObject* hidden_lookup =
      GetHiddenPropertiesHashTable(ONLY_RETURN_INLINE_VALUE);
  Object* inline_value = hidden_lookup->ToObjectUnchecked();

  if (inline_value->IsSmi()) {
    // Handle inline-stored identity hash.
    if (key == GetHeap()->identity_hash_string()) {
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


Handle<Object> JSObject::SetHiddenProperty(Handle<JSObject> obj,
                                           Handle<Name> key,
                                           Handle<Object> value) {
  CALL_HEAP_FUNCTION(obj->GetIsolate(),
                     obj->SetHiddenProperty(*key, *value),
                     Object);
}


MaybeObject* JSObject::SetHiddenProperty(Name* key, Object* value) {
  ASSERT(key->IsUniqueName());
  if (IsJSGlobalProxy()) {
    // For a proxy, use the prototype as target object.
    Object* proxy_parent = GetPrototype();
    // If the proxy is detached, return undefined.
    if (proxy_parent->IsNull()) return GetHeap()->undefined_value();
    ASSERT(proxy_parent->IsJSGlobalObject());
    return JSObject::cast(proxy_parent)->SetHiddenProperty(key, value);
  }
  ASSERT(!IsJSGlobalProxy());
  MaybeObject* hidden_lookup =
      GetHiddenPropertiesHashTable(ONLY_RETURN_INLINE_VALUE);
  Object* inline_value = hidden_lookup->ToObjectUnchecked();

  // If there is no backing store yet, store the identity hash inline.
  if (value->IsSmi() &&
      key == GetHeap()->identity_hash_string() &&
      (inline_value->IsUndefined() || inline_value->IsSmi())) {
    return SetHiddenPropertiesHashTable(value);
  }

  hidden_lookup = GetHiddenPropertiesHashTable(CREATE_NEW_IF_ABSENT);
  ObjectHashTable* hashtable;
  if (!hidden_lookup->To(&hashtable)) return hidden_lookup;

  // If it was found, check if the key is already in the dictionary.
  MaybeObject* insert_result = hashtable->Put(key, value);
  ObjectHashTable* new_table;
  if (!insert_result->To(&new_table)) return insert_result;
  if (new_table != hashtable) {
    // If adding the key expanded the dictionary (i.e., Add returned a new
    // dictionary), store it back to the object.
    MaybeObject* store_result = SetHiddenPropertiesHashTable(new_table);
    if (store_result->IsFailure()) return store_result;
  }
  // Return this to mark success.
  return this;
}


void JSObject::DeleteHiddenProperty(Handle<JSObject> object, Handle<Name> key) {
  Isolate* isolate = object->GetIsolate();
  ASSERT(key->IsUniqueName());

  if (object->IsJSGlobalProxy()) {
    Handle<Object> proto(object->GetPrototype(), isolate);
    if (proto->IsNull()) return;
    ASSERT(proto->IsJSGlobalObject());
    return DeleteHiddenProperty(Handle<JSObject>::cast(proto), key);
  }

  MaybeObject* hidden_lookup =
      object->GetHiddenPropertiesHashTable(ONLY_RETURN_INLINE_VALUE);
  Object* inline_value = hidden_lookup->ToObjectUnchecked();

  // We never delete (inline-stored) identity hashes.
  ASSERT(*key != isolate->heap()->identity_hash_string());
  if (inline_value->IsUndefined() || inline_value->IsSmi()) return;

  Handle<ObjectHashTable> hashtable(ObjectHashTable::cast(inline_value));
  PutIntoObjectHashTable(hashtable, key, isolate->factory()->the_hole_value());
}


bool JSObject::HasHiddenProperties() {
  return GetPropertyAttributePostInterceptor(this,
                                             GetHeap()->hidden_string(),
                                             false) != ABSENT;
}


MaybeObject* JSObject::GetHiddenPropertiesHashTable(
    InitializeHiddenProperties init_option) {
  ASSERT(!IsJSGlobalProxy());
  Object* inline_value;
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
        ASSERT(descriptors->GetType(sorted_index) == FIELD);
        MaybeObject* maybe_value = this->FastPropertyAt(
            descriptors->GetDetails(sorted_index).representation(),
            descriptors->GetFieldIndex(sorted_index));
        if (!maybe_value->To(&inline_value)) return maybe_value;
      } else {
        inline_value = GetHeap()->undefined_value();
      }
    } else {
      inline_value = GetHeap()->undefined_value();
    }
  } else {
    PropertyAttributes attributes;
    // You can't install a getter on a property indexed by the hidden string,
    // so we can be sure that GetLocalPropertyPostInterceptor returns a real
    // object.
    inline_value =
        GetLocalPropertyPostInterceptor(this,
                                        GetHeap()->hidden_string(),
                                        &attributes)->ToObjectUnchecked();
  }

  if (init_option == ONLY_RETURN_INLINE_VALUE ||
      inline_value->IsHashTable()) {
    return inline_value;
  }

  ObjectHashTable* hashtable;
  static const int kInitialCapacity = 4;
  MaybeObject* maybe_obj =
      ObjectHashTable::Allocate(GetHeap(),
                                kInitialCapacity,
                                ObjectHashTable::USE_CUSTOM_MINIMUM_CAPACITY);
  if (!maybe_obj->To<ObjectHashTable>(&hashtable)) return maybe_obj;

  if (inline_value->IsSmi()) {
    // We were storing the identity hash inline and now allocated an actual
    // dictionary.  Put the identity hash into the new dictionary.
    MaybeObject* insert_result =
        hashtable->Put(GetHeap()->identity_hash_string(), inline_value);
    ObjectHashTable* new_table;
    if (!insert_result->To(&new_table)) return insert_result;
    // We expect no resizing for the first insert.
    ASSERT_EQ(hashtable, new_table);
  }

  MaybeObject* store_result = SetLocalPropertyIgnoreAttributesTrampoline(
      GetHeap()->hidden_string(),
      hashtable,
      DONT_ENUM,
      OPTIMAL_REPRESENTATION,
      ALLOW_AS_CONSTANT,
      OMIT_EXTENSIBILITY_CHECK);
  if (store_result->IsFailure()) return store_result;
  return hashtable;
}


MaybeObject* JSObject::SetHiddenPropertiesHashTable(Object* value) {
  ASSERT(!IsJSGlobalProxy());
  // We can store the identity hash inline iff there is no backing store
  // for hidden properties yet.
  ASSERT(HasHiddenProperties() != value->IsSmi());
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
        ASSERT(descriptors->GetType(sorted_index) == FIELD);
        FastPropertyAtPut(descriptors->GetFieldIndex(sorted_index), value);
        return this;
      }
    }
  }
  MaybeObject* store_result = SetLocalPropertyIgnoreAttributesTrampoline(
      GetHeap()->hidden_string(),
      value,
      DONT_ENUM,
      OPTIMAL_REPRESENTATION,
      ALLOW_AS_CONSTANT,
      OMIT_EXTENSIBILITY_CHECK);
  if (store_result->IsFailure()) return store_result;
  return this;
}


Handle<Object> JSObject::DeletePropertyPostInterceptor(Handle<JSObject> object,
                                                       Handle<Name> name,
                                                       DeleteMode mode) {
  // Check local property, ignore interceptor.
  Isolate* isolate = object->GetIsolate();
  LookupResult result(isolate);
  object->LocalLookupRealNamedProperty(*name, &result);
  if (!result.IsFound()) return isolate->factory()->true_value();

  // Normalize object if needed.
  NormalizeProperties(object, CLEAR_INOBJECT_PROPERTIES, 0);

  return DeleteNormalizedProperty(object, name, mode);
}


Handle<Object> JSObject::DeletePropertyWithInterceptor(Handle<JSObject> object,
                                                       Handle<Name> name) {
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
    RETURN_HANDLE_IF_SCHEDULED_EXCEPTION(isolate, Object);
    if (!result.IsEmpty()) {
      ASSERT(result->IsBoolean());
      Handle<Object> result_internal = v8::Utils::OpenHandle(*result);
      result_internal->VerifyApiCallResultType();
      // Rebox CustomArguments::kReturnValueOffset before returning.
      return handle(*result_internal, isolate);
    }
  }
  Handle<Object> result =
      DeletePropertyPostInterceptor(object, name, NORMAL_DELETION);
  RETURN_HANDLE_IF_SCHEDULED_EXCEPTION(isolate, Object);
  return result;
}


// TODO(mstarzinger): Temporary wrapper until handlified.
static Handle<Object> AccessorDelete(Handle<JSObject> object,
                                     uint32_t index,
                                     JSObject::DeleteMode mode) {
  CALL_HEAP_FUNCTION(object->GetIsolate(),
                     object->GetElementsAccessor()->Delete(*object,
                                                           index,
                                                           mode),
                     Object);
}


Handle<Object> JSObject::DeleteElementWithInterceptor(Handle<JSObject> object,
                                                      uint32_t index) {
  Isolate* isolate = object->GetIsolate();
  Factory* factory = isolate->factory();

  // Make sure that the top context does not change when doing
  // callbacks or interceptor calls.
  AssertNoContextChange ncc;

  Handle<InterceptorInfo> interceptor(object->GetIndexedInterceptor());
  if (interceptor->deleter()->IsUndefined()) return factory->false_value();
  v8::IndexedPropertyDeleterCallback deleter =
      v8::ToCData<v8::IndexedPropertyDeleterCallback>(interceptor->deleter());
  LOG(isolate,
      ApiIndexedPropertyAccess("interceptor-indexed-delete", *object, index));
  PropertyCallbackArguments args(
      isolate, interceptor->data(), *object, *object);
  v8::Handle<v8::Boolean> result = args.Call(deleter, index);
  RETURN_HANDLE_IF_SCHEDULED_EXCEPTION(isolate, Object);
  if (!result.IsEmpty()) {
    ASSERT(result->IsBoolean());
    Handle<Object> result_internal = v8::Utils::OpenHandle(*result);
    result_internal->VerifyApiCallResultType();
    // Rebox CustomArguments::kReturnValueOffset before returning.
    return handle(*result_internal, isolate);
  }
  Handle<Object> delete_result = AccessorDelete(object, index, NORMAL_DELETION);
  RETURN_HANDLE_IF_SCHEDULED_EXCEPTION(isolate, Object);
  return delete_result;
}


Handle<Object> JSObject::DeleteElement(Handle<JSObject> object,
                                       uint32_t index,
                                       DeleteMode mode) {
  Isolate* isolate = object->GetIsolate();
  Factory* factory = isolate->factory();

  // Check access rights if needed.
  if (object->IsAccessCheckNeeded() &&
      !isolate->MayIndexedAccess(*object, index, v8::ACCESS_DELETE)) {
    isolate->ReportFailedAccessCheck(*object, v8::ACCESS_DELETE);
    RETURN_HANDLE_IF_SCHEDULED_EXCEPTION(isolate, Object);
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
    Handle<Object> proto(object->GetPrototype(), isolate);
    if (proto->IsNull()) return factory->false_value();
    ASSERT(proto->IsJSGlobalObject());
    return DeleteElement(Handle<JSObject>::cast(proto), index, mode);
  }

  Handle<Object> old_value;
  bool should_enqueue_change_record = false;
  if (FLAG_harmony_observation && object->map()->is_observed()) {
    should_enqueue_change_record = object->HasLocalElement(index);
    if (should_enqueue_change_record) {
      old_value = object->GetLocalElementAccessorPair(index) != NULL
          ? Handle<Object>::cast(factory->the_hole_value())
          : Object::GetElement(isolate, object, index);
    }
  }

  // Skip interceptor if forcing deletion.
  Handle<Object> result;
  if (object->HasIndexedInterceptor() && mode != FORCE_DELETION) {
    result = DeleteElementWithInterceptor(object, index);
  } else {
    result = AccessorDelete(object, index, mode);
  }

  if (should_enqueue_change_record && !object->HasLocalElement(index)) {
    Handle<String> name = factory->Uint32ToString(index);
    EnqueueChangeRecord(object, "deleted", name, old_value);
  }

  return result;
}


Handle<Object> JSObject::DeleteProperty(Handle<JSObject> object,
                                        Handle<Name> name,
                                        DeleteMode mode) {
  Isolate* isolate = object->GetIsolate();
  // ECMA-262, 3rd, 8.6.2.5
  ASSERT(name->IsName());

  // Check access rights if needed.
  if (object->IsAccessCheckNeeded() &&
      !isolate->MayNamedAccess(*object, *name, v8::ACCESS_DELETE)) {
    isolate->ReportFailedAccessCheck(*object, v8::ACCESS_DELETE);
    RETURN_HANDLE_IF_SCHEDULED_EXCEPTION(isolate, Object);
    return isolate->factory()->false_value();
  }

  if (object->IsJSGlobalProxy()) {
    Object* proto = object->GetPrototype();
    if (proto->IsNull()) return isolate->factory()->false_value();
    ASSERT(proto->IsJSGlobalObject());
    return JSGlobalObject::DeleteProperty(
        handle(JSGlobalObject::cast(proto)), name, mode);
  }

  uint32_t index = 0;
  if (name->AsArrayIndex(&index)) {
    return DeleteElement(object, index, mode);
  }

  LookupResult lookup(isolate);
  object->LocalLookup(*name, &lookup, true);
  if (!lookup.IsFound()) return isolate->factory()->true_value();
  // Ignore attributes if forcing a deletion.
  if (lookup.IsDontDelete() && mode != FORCE_DELETION) {
    if (mode == STRICT_DELETION) {
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
  bool is_observed = FLAG_harmony_observation && object->map()->is_observed();
  if (is_observed && lookup.IsDataProperty()) {
    old_value = Object::GetProperty(object, name);
  }
  Handle<Object> result;

  // Check for interceptor.
  if (lookup.IsInterceptor()) {
    // Skip interceptor if forcing a deletion.
    if (mode == FORCE_DELETION) {
      result = DeletePropertyPostInterceptor(object, name, mode);
    } else {
      result = DeletePropertyWithInterceptor(object, name);
    }
  } else {
    // Normalize object if needed.
    NormalizeProperties(object, CLEAR_INOBJECT_PROPERTIES, 0);
    // Make sure the properties are normalized before removing the entry.
    result = DeleteNormalizedProperty(object, name, mode);
  }

  if (is_observed && !object->HasLocalProperty(*name)) {
    EnqueueChangeRecord(object, "deleted", name, old_value);
  }

  return result;
}


Handle<Object> JSReceiver::DeleteElement(Handle<JSReceiver> object,
                                         uint32_t index,
                                         DeleteMode mode) {
  if (object->IsJSProxy()) {
    return JSProxy::DeleteElementWithHandler(
        Handle<JSProxy>::cast(object), index, mode);
  }
  return JSObject::DeleteElement(Handle<JSObject>::cast(object), index, mode);
}


Handle<Object> JSReceiver::DeleteProperty(Handle<JSReceiver> object,
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
  ASSERT(IsFastObjectElementsKind(kind) ||
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
    case EXTERNAL_PIXEL_ELEMENTS:
    case EXTERNAL_BYTE_ELEMENTS:
    case EXTERNAL_UNSIGNED_BYTE_ELEMENTS:
    case EXTERNAL_SHORT_ELEMENTS:
    case EXTERNAL_UNSIGNED_SHORT_ELEMENTS:
    case EXTERNAL_INT_ELEMENTS:
    case EXTERNAL_UNSIGNED_INT_ELEMENTS:
    case EXTERNAL_FLOAT_ELEMENTS:
    case EXTERNAL_DOUBLE_ELEMENTS:
    case FAST_DOUBLE_ELEMENTS:
    case FAST_HOLEY_DOUBLE_ELEMENTS:
      // Raw pixels and external arrays do not reference other
      // objects.
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
    case NON_STRICT_ARGUMENTS_ELEMENTS: {
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
    JSObject* arguments_boilerplate =
        heap->isolate()->context()->native_context()->
            arguments_boilerplate();
    JSFunction* arguments_function =
        JSFunction::cast(arguments_boilerplate->map()->constructor());

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
      return JSObject::cast(context->extension())->ReferencesObject(obj);
    }
  }

  // No references to object.
  return false;
}


Handle<Object> JSObject::PreventExtensions(Handle<JSObject> object) {
  CALL_HEAP_FUNCTION(object->GetIsolate(), object->PreventExtensions(), Object);
}


MaybeObject* JSObject::PreventExtensions() {
  Isolate* isolate = GetIsolate();
  if (IsAccessCheckNeeded() &&
      !isolate->MayNamedAccess(this,
                               isolate->heap()->undefined_value(),
                               v8::ACCESS_KEYS)) {
    isolate->ReportFailedAccessCheck(this, v8::ACCESS_KEYS);
    RETURN_IF_SCHEDULED_EXCEPTION(isolate);
    return isolate->heap()->false_value();
  }

  if (IsJSGlobalProxy()) {
    Object* proto = GetPrototype();
    if (proto->IsNull()) return this;
    ASSERT(proto->IsJSGlobalObject());
    return JSObject::cast(proto)->PreventExtensions();
  }

  // It's not possible to seal objects with external array elements
  if (HasExternalArrayElements()) {
    HandleScope scope(isolate);
    Handle<Object> object(this, isolate);
    Handle<Object> error  =
        isolate->factory()->NewTypeError(
            "cant_prevent_ext_external_array_elements",
            HandleVector(&object, 1));
    return isolate->Throw(*error);
  }

  // If there are fast elements we normalize.
  SeededNumberDictionary* dictionary = NULL;
  { MaybeObject* maybe = NormalizeElements();
    if (!maybe->To<SeededNumberDictionary>(&dictionary)) return maybe;
  }
  ASSERT(HasDictionaryElements() || HasDictionaryArgumentsElements());
  // Make sure that we never go back to fast case.
  dictionary->set_requires_slow_elements();

  // Do a map transition, other objects with this map may still
  // be extensible.
  // TODO(adamk): Extend the NormalizedMapCache to handle non-extensible maps.
  Map* new_map;
  MaybeObject* maybe = map()->Copy();
  if (!maybe->To(&new_map)) return maybe;

  new_map->set_is_extensible(false);
  set_map(new_map);
  ASSERT(!map()->is_extensible());
  return new_map;
}


template<typename Dictionary>
static void FreezeDictionary(Dictionary* dictionary) {
  int capacity = dictionary->Capacity();
  for (int i = 0; i < capacity; i++) {
    Object* k = dictionary->KeyAt(i);
    if (dictionary->IsKey(k)) {
      PropertyDetails details = dictionary->DetailsAt(i);
      int attrs = DONT_DELETE;
      // READ_ONLY is an invalid attribute for JS setters/getters.
      if (details.type() != CALLBACKS ||
          !dictionary->ValueAt(i)->IsAccessorPair()) {
        attrs |= READ_ONLY;
      }
      details = details.CopyAddAttributes(
          static_cast<PropertyAttributes>(attrs));
      dictionary->DetailsAtPut(i, details);
    }
  }
}


MUST_USE_RESULT MaybeObject* JSObject::Freeze(Isolate* isolate) {
  // Freezing non-strict arguments should be handled elsewhere.
  ASSERT(!HasNonStrictArgumentsElements());

  Heap* heap = isolate->heap();

  if (map()->is_frozen()) return this;

  if (IsAccessCheckNeeded() &&
      !isolate->MayNamedAccess(this,
                               heap->undefined_value(),
                               v8::ACCESS_KEYS)) {
    isolate->ReportFailedAccessCheck(this, v8::ACCESS_KEYS);
    RETURN_IF_SCHEDULED_EXCEPTION(isolate);
    return heap->false_value();
  }

  if (IsJSGlobalProxy()) {
    Object* proto = GetPrototype();
    if (proto->IsNull()) return this;
    ASSERT(proto->IsJSGlobalObject());
    return JSObject::cast(proto)->Freeze(isolate);
  }

  // It's not possible to freeze objects with external array elements
  if (HasExternalArrayElements()) {
    HandleScope scope(isolate);
    Handle<Object> object(this, isolate);
    Handle<Object> error  =
        isolate->factory()->NewTypeError(
            "cant_prevent_ext_external_array_elements",
            HandleVector(&object, 1));
    return isolate->Throw(*error);
  }

  SeededNumberDictionary* new_element_dictionary = NULL;
  if (!elements()->IsDictionary()) {
    int length = IsJSArray()
        ? Smi::cast(JSArray::cast(this)->length())->value()
        : elements()->length();
    if (length > 0) {
      int capacity = 0;
      int used = 0;
      GetElementsCapacityAndUsage(&capacity, &used);
      MaybeObject* maybe_dict = SeededNumberDictionary::Allocate(heap, used);
      if (!maybe_dict->To(&new_element_dictionary)) return maybe_dict;

      // Move elements to a dictionary; avoid calling NormalizeElements to avoid
      // unnecessary transitions.
      maybe_dict = CopyFastElementsToDictionary(isolate, elements(), length,
                                                new_element_dictionary);
      if (!maybe_dict->To(&new_element_dictionary)) return maybe_dict;
    } else {
      // No existing elements, use a pre-allocated empty backing store
      new_element_dictionary = heap->empty_slow_element_dictionary();
    }
  }

  LookupResult result(isolate);
  map()->LookupTransition(this, heap->frozen_symbol(), &result);
  if (result.IsTransition()) {
    Map* transition_map = result.GetTransitionTarget();
    ASSERT(transition_map->has_dictionary_elements());
    ASSERT(transition_map->is_frozen());
    ASSERT(!transition_map->is_extensible());
    set_map(transition_map);
  } else if (HasFastProperties() && map()->CanHaveMoreTransitions()) {
    // Create a new descriptor array with fully-frozen properties
    int num_descriptors = map()->NumberOfOwnDescriptors();
    DescriptorArray* new_descriptors;
    MaybeObject* maybe_descriptors =
        map()->instance_descriptors()->CopyUpToAddAttributes(num_descriptors,
                                                             FROZEN);
    if (!maybe_descriptors->To(&new_descriptors)) return maybe_descriptors;

    Map* new_map;
    MaybeObject* maybe_new_map = map()->CopyReplaceDescriptors(
        new_descriptors, INSERT_TRANSITION, heap->frozen_symbol());
    if (!maybe_new_map->To(&new_map)) return maybe_new_map;
    new_map->freeze();
    new_map->set_is_extensible(false);
    new_map->set_elements_kind(DICTIONARY_ELEMENTS);
    set_map(new_map);
  } else {
    // Slow path: need to normalize properties for safety
    MaybeObject* maybe = NormalizeProperties(CLEAR_INOBJECT_PROPERTIES, 0);
    if (maybe->IsFailure()) return maybe;

    // Create a new map, since other objects with this map may be extensible.
    // TODO(adamk): Extend the NormalizedMapCache to handle non-extensible maps.
    Map* new_map;
    MaybeObject* maybe_copy = map()->Copy();
    if (!maybe_copy->To(&new_map)) return maybe_copy;
    new_map->freeze();
    new_map->set_is_extensible(false);
    new_map->set_elements_kind(DICTIONARY_ELEMENTS);
    set_map(new_map);

    // Freeze dictionary-mode properties
    FreezeDictionary(property_dictionary());
  }

  ASSERT(map()->has_dictionary_elements());
  if (new_element_dictionary != NULL) {
    set_elements(new_element_dictionary);
  }

  if (elements() != heap->empty_slow_element_dictionary()) {
    SeededNumberDictionary* dictionary = element_dictionary();
    // Make sure we never go back to the fast case
    dictionary->set_requires_slow_elements();
    // Freeze all elements in the dictionary
    FreezeDictionary(dictionary);
  }

  return this;
}


MUST_USE_RESULT MaybeObject* JSObject::SetObserved(Isolate* isolate) {
  if (map()->is_observed())
    return isolate->heap()->undefined_value();

  Heap* heap = isolate->heap();

  if (!HasExternalArrayElements()) {
    // Go to dictionary mode, so that we don't skip map checks.
    MaybeObject* maybe = NormalizeElements();
    if (maybe->IsFailure()) return maybe;
    ASSERT(!HasFastElements());
  }

  LookupResult result(isolate);
  map()->LookupTransition(this, heap->observed_symbol(), &result);

  Map* new_map;
  if (result.IsTransition()) {
    new_map = result.GetTransitionTarget();
    ASSERT(new_map->is_observed());
  } else if (map()->CanHaveMoreTransitions()) {
    MaybeObject* maybe_new_map = map()->CopyForObserved();
    if (!maybe_new_map->To(&new_map)) return maybe_new_map;
  } else {
    MaybeObject* maybe_copy = map()->Copy();
    if (!maybe_copy->To(&new_map)) return maybe_copy;
    new_map->set_is_observed(true);
  }
  set_map(new_map);

  return heap->undefined_value();
}


MUST_USE_RESULT MaybeObject* JSObject::DeepCopy(Isolate* isolate) {
  StackLimitCheck check(isolate);
  if (check.HasOverflowed()) return isolate->StackOverflow();

  if (map()->is_deprecated()) {
    MaybeObject* maybe_failure = MigrateInstance();
    if (maybe_failure->IsFailure()) return maybe_failure;
  }

  Heap* heap = isolate->heap();
  Object* result;
  { MaybeObject* maybe_result = heap->CopyJSObject(this);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
  JSObject* copy = JSObject::cast(result);

  // Deep copy local properties.
  if (copy->HasFastProperties()) {
    DescriptorArray* descriptors = copy->map()->instance_descriptors();
    int limit = copy->map()->NumberOfOwnDescriptors();
    for (int i = 0; i < limit; i++) {
      PropertyDetails details = descriptors->GetDetails(i);
      if (details.type() != FIELD) continue;
      int index = descriptors->GetFieldIndex(i);
      Object* value = RawFastPropertyAt(index);
      if (value->IsJSObject()) {
        JSObject* js_object = JSObject::cast(value);
        MaybeObject* maybe_copy = js_object->DeepCopy(isolate);
        if (!maybe_copy->To(&value)) return maybe_copy;
      } else {
        Representation representation = details.representation();
        MaybeObject* maybe_storage =
            value->AllocateNewStorageFor(heap, representation);
        if (!maybe_storage->To(&value)) return maybe_storage;
      }
      copy->FastPropertyAtPut(index, value);
    }
  } else {
    { MaybeObject* maybe_result =
          heap->AllocateFixedArray(copy->NumberOfLocalProperties());
      if (!maybe_result->ToObject(&result)) return maybe_result;
    }
    FixedArray* names = FixedArray::cast(result);
    copy->GetLocalPropertyNames(names, 0);
    for (int i = 0; i < names->length(); i++) {
      ASSERT(names->get(i)->IsString());
      String* key_string = String::cast(names->get(i));
      PropertyAttributes attributes =
          copy->GetLocalPropertyAttribute(key_string);
      // Only deep copy fields from the object literal expression.
      // In particular, don't try to copy the length attribute of
      // an array.
      if (attributes != NONE) continue;
      Object* value =
          copy->GetProperty(key_string, &attributes)->ToObjectUnchecked();
      if (value->IsJSObject()) {
        JSObject* js_object = JSObject::cast(value);
        { MaybeObject* maybe_result = js_object->DeepCopy(isolate);
          if (!maybe_result->ToObject(&result)) return maybe_result;
        }
        { MaybeObject* maybe_result =
              // Creating object copy for literals. No strict mode needed.
              copy->SetProperty(key_string, result, NONE, kNonStrictMode);
          if (!maybe_result->ToObject(&result)) return maybe_result;
        }
      }
    }
  }

  // Deep copy local elements.
  // Pixel elements cannot be created using an object literal.
  ASSERT(!copy->HasExternalArrayElements());
  switch (copy->GetElementsKind()) {
    case FAST_SMI_ELEMENTS:
    case FAST_ELEMENTS:
    case FAST_HOLEY_SMI_ELEMENTS:
    case FAST_HOLEY_ELEMENTS: {
      FixedArray* elements = FixedArray::cast(copy->elements());
      if (elements->map() == heap->fixed_cow_array_map()) {
        isolate->counters()->cow_arrays_created_runtime()->Increment();
#ifdef DEBUG
        for (int i = 0; i < elements->length(); i++) {
          ASSERT(!elements->get(i)->IsJSObject());
        }
#endif
      } else {
        for (int i = 0; i < elements->length(); i++) {
          Object* value = elements->get(i);
          ASSERT(value->IsSmi() ||
                 value->IsTheHole() ||
                 (IsFastObjectElementsKind(copy->GetElementsKind())));
          if (value->IsJSObject()) {
            JSObject* js_object = JSObject::cast(value);
            { MaybeObject* maybe_result = js_object->DeepCopy(isolate);
              if (!maybe_result->ToObject(&result)) return maybe_result;
            }
            elements->set(i, result);
          }
        }
      }
      break;
    }
    case DICTIONARY_ELEMENTS: {
      SeededNumberDictionary* element_dictionary = copy->element_dictionary();
      int capacity = element_dictionary->Capacity();
      for (int i = 0; i < capacity; i++) {
        Object* k = element_dictionary->KeyAt(i);
        if (element_dictionary->IsKey(k)) {
          Object* value = element_dictionary->ValueAt(i);
          if (value->IsJSObject()) {
            JSObject* js_object = JSObject::cast(value);
            { MaybeObject* maybe_result = js_object->DeepCopy(isolate);
              if (!maybe_result->ToObject(&result)) return maybe_result;
            }
            element_dictionary->ValueAtPut(i, result);
          }
        }
      }
      break;
    }
    case NON_STRICT_ARGUMENTS_ELEMENTS:
      UNIMPLEMENTED();
      break;
    case EXTERNAL_PIXEL_ELEMENTS:
    case EXTERNAL_BYTE_ELEMENTS:
    case EXTERNAL_UNSIGNED_BYTE_ELEMENTS:
    case EXTERNAL_SHORT_ELEMENTS:
    case EXTERNAL_UNSIGNED_SHORT_ELEMENTS:
    case EXTERNAL_INT_ELEMENTS:
    case EXTERNAL_UNSIGNED_INT_ELEMENTS:
    case EXTERNAL_FLOAT_ELEMENTS:
    case EXTERNAL_DOUBLE_ELEMENTS:
    case FAST_DOUBLE_ELEMENTS:
    case FAST_HOLEY_DOUBLE_ELEMENTS:
      // No contained objects, nothing to do.
      break;
  }
  return copy;
}


// Tests for the fast common case for property enumeration:
// - This object and all prototypes has an enum cache (which means that
//   it is no proxy, has no interceptors and needs no access checks).
// - This object has no elements.
// - No prototype has enumerable properties/elements.
bool JSReceiver::IsSimpleEnum() {
  Heap* heap = GetHeap();
  for (Object* o = this;
       o != heap->null_value();
       o = JSObject::cast(o)->GetPrototype()) {
    if (!o->IsJSObject()) return false;
    JSObject* curr = JSObject::cast(o);
    int enum_length = curr->map()->EnumLength();
    if (enum_length == Map::kInvalidEnumCache) return false;
    ASSERT(!curr->HasNamedInterceptor());
    ASSERT(!curr->HasIndexedInterceptor());
    ASSERT(!curr->IsAccessCheckNeeded());
    if (curr->NumberOfEnumElements() > 0) return false;
    if (curr != this && enum_length != 0) return false;
  }
  return true;
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
        ((filter & SYMBOLIC) == 0 || !descs->GetKey(i)->IsSymbol())) {
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


AccessorDescriptor* Map::FindAccessor(Name* name) {
  DescriptorArray* descs = instance_descriptors();
  int number_of_own_descriptors = NumberOfOwnDescriptors();
  for (int i = 0; i < number_of_own_descriptors; i++) {
    if (descs->GetType(i) == CALLBACKS && name->Equals(descs->GetKey(i))) {
      return descs->GetCallbacks(i);
    }
  }
  return NULL;
}


void JSReceiver::LocalLookup(
    Name* name, LookupResult* result, bool search_hidden_prototypes) {
  ASSERT(name->IsName());

  Heap* heap = GetHeap();

  if (IsJSGlobalProxy()) {
    Object* proto = GetPrototype();
    if (proto->IsNull()) return result->NotFound();
    ASSERT(proto->IsJSGlobalObject());
    return JSReceiver::cast(proto)->LocalLookup(
        name, result, search_hidden_prototypes);
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
      !heap->isolate()->bootstrapper()->IsActive()) {
    result->InterceptorResult(js_object);
    return;
  }

  js_object->LocalLookupRealNamedProperty(name, result);
  if (result->IsFound() || !search_hidden_prototypes) return;

  Object* proto = js_object->GetPrototype();
  if (!proto->IsJSReceiver()) return;
  JSReceiver* receiver = JSReceiver::cast(proto);
  if (receiver->map()->is_hidden_prototype()) {
    receiver->LocalLookup(name, result, search_hidden_prototypes);
  }
}


void JSReceiver::Lookup(Name* name, LookupResult* result) {
  // Ecma-262 3rd 8.6.2.4
  Heap* heap = GetHeap();
  for (Object* current = this;
       current != heap->null_value();
       current = JSObject::cast(current)->GetPrototype()) {
    JSReceiver::cast(current)->LocalLookup(name, result, false);
    if (result->IsFound()) return;
  }
  result->NotFound();
}


// Search object and its prototype chain for callback properties.
void JSObject::LookupCallbackProperty(Name* name, LookupResult* result) {
  Heap* heap = GetHeap();
  for (Object* current = this;
       current != heap->null_value() && current->IsJSObject();
       current = JSObject::cast(current)->GetPrototype()) {
    JSObject::cast(current)->LocalLookupRealNamedProperty(name, result);
    if (result->IsPropertyCallbacks()) return;
  }
  result->NotFound();
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
      ASSERT(!details.IsDontDelete());
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
                                     PropertyAttributes attributes,
                                     v8::AccessControl access_control) {
  switch (object->GetElementsKind()) {
    case FAST_SMI_ELEMENTS:
    case FAST_ELEMENTS:
    case FAST_DOUBLE_ELEMENTS:
    case FAST_HOLEY_SMI_ELEMENTS:
    case FAST_HOLEY_ELEMENTS:
    case FAST_HOLEY_DOUBLE_ELEMENTS:
      break;
    case EXTERNAL_PIXEL_ELEMENTS:
    case EXTERNAL_BYTE_ELEMENTS:
    case EXTERNAL_UNSIGNED_BYTE_ELEMENTS:
    case EXTERNAL_SHORT_ELEMENTS:
    case EXTERNAL_UNSIGNED_SHORT_ELEMENTS:
    case EXTERNAL_INT_ELEMENTS:
    case EXTERNAL_UNSIGNED_INT_ELEMENTS:
    case EXTERNAL_FLOAT_ELEMENTS:
    case EXTERNAL_DOUBLE_ELEMENTS:
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
    case NON_STRICT_ARGUMENTS_ELEMENTS: {
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
  accessors->set_access_flags(access_control);

  SetElementCallback(object, index, accessors, attributes);
}


Handle<AccessorPair> JSObject::CreateAccessorPairFor(Handle<JSObject> object,
                                                     Handle<Name> name) {
  Isolate* isolate = object->GetIsolate();
  LookupResult result(isolate);
  object->LocalLookupRealNamedProperty(*name, &result);
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
                                      PropertyAttributes attributes,
                                      v8::AccessControl access_control) {
  // We could assert that the property is configurable here, but we would need
  // to do a lookup, which seems to be a bit of overkill.
  bool only_attribute_changes = getter->IsNull() && setter->IsNull();
  if (object->HasFastProperties() && !only_attribute_changes &&
      access_control == v8::DEFAULT &&
      (object->map()->NumberOfOwnDescriptors() <
       DescriptorArray::kMaxNumberOfDescriptors)) {
    bool getterOk = getter->IsNull() ||
        DefineFastAccessor(object, name, ACCESSOR_GETTER, getter, attributes);
    bool setterOk = !getterOk || setter->IsNull() ||
        DefineFastAccessor(object, name, ACCESSOR_SETTER, setter, attributes);
    if (getterOk && setterOk) return;
  }

  Handle<AccessorPair> accessors = CreateAccessorPairFor(object, name);
  accessors->SetComponents(*getter, *setter);
  accessors->set_access_flags(access_control);

  SetPropertyCallback(object, name, accessors, attributes);
}


bool JSObject::CanSetCallback(Name* name) {
  ASSERT(!IsAccessCheckNeeded() ||
         GetIsolate()->MayNamedAccess(this, name, v8::ACCESS_SET));

  // Check if there is an API defined callback object which prohibits
  // callback overwriting in this object or its prototype chain.
  // This mechanism is needed for instance in a browser setting, where
  // certain accessors such as window.location should not be allowed
  // to be overwritten because allowing overwriting could potentially
  // cause security problems.
  LookupResult callback_result(GetIsolate());
  LookupCallbackProperty(name, &callback_result);
  if (callback_result.IsFound()) {
    Object* obj = callback_result.GetCallbackObject();
    if (obj->IsAccessorInfo()) {
      return !AccessorInfo::cast(obj)->prohibits_overwriting();
    }
    if (obj->IsAccessorPair()) {
      return !AccessorPair::cast(obj)->prohibits_overwriting();
    }
  }
  return true;
}


void JSObject::SetElementCallback(Handle<JSObject> object,
                                  uint32_t index,
                                  Handle<Object> structure,
                                  PropertyAttributes attributes) {
  Heap* heap = object->GetHeap();
  PropertyDetails details = PropertyDetails(attributes, CALLBACKS, 0);

  // Normalize elements to make this operation simple.
  Handle<SeededNumberDictionary> dictionary = NormalizeElements(object);
  ASSERT(object->HasDictionaryElements() ||
         object->HasDictionaryArgumentsElements());

  // Update the dictionary with the new CALLBACKS property.
  dictionary = SeededNumberDictionary::Set(dictionary, index, structure,
                                           details);
  dictionary->set_requires_slow_elements();

  // Update the dictionary backing store on the object.
  if (object->elements()->map() == heap->non_strict_arguments_elements_map()) {
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
  }
}


void JSObject::SetPropertyCallback(Handle<JSObject> object,
                                   Handle<Name> name,
                                   Handle<Object> structure,
                                   PropertyAttributes attributes) {
  // Normalize object to make this operation simple.
  NormalizeProperties(object, CLEAR_INOBJECT_PROPERTIES, 0);

  // For the global object allocate a new map to invalidate the global inline
  // caches which have a global property cell reference directly in the code.
  if (object->IsGlobalObject()) {
    Handle<Map> new_map = Map::CopyDropDescriptors(handle(object->map()));
    ASSERT(new_map->is_dictionary_map());
    object->set_map(*new_map);

    // When running crankshaft, changing the map is not enough. We
    // need to deoptimize all functions that rely on this global
    // object.
    Deoptimizer::DeoptimizeGlobalObject(*object);
  }

  // Update the dictionary with the new CALLBACKS property.
  PropertyDetails details = PropertyDetails(attributes, CALLBACKS, 0);
  SetNormalizedProperty(object, name, structure, details);
}


void JSObject::DefineAccessor(Handle<JSObject> object,
                              Handle<Name> name,
                              Handle<Object> getter,
                              Handle<Object> setter,
                              PropertyAttributes attributes,
                              v8::AccessControl access_control) {
  Isolate* isolate = object->GetIsolate();
  // Check access rights if needed.
  if (object->IsAccessCheckNeeded() &&
      !isolate->MayNamedAccess(*object, *name, v8::ACCESS_SET)) {
    isolate->ReportFailedAccessCheck(*object, v8::ACCESS_SET);
    return;
  }

  if (object->IsJSGlobalProxy()) {
    Handle<Object> proto(object->GetPrototype(), isolate);
    if (proto->IsNull()) return;
    ASSERT(proto->IsJSGlobalObject());
    DefineAccessor(Handle<JSObject>::cast(proto),
                   name,
                   getter,
                   setter,
                   attributes,
                   access_control);
    return;
  }

  // Make sure that the top context does not change when doing callbacks or
  // interceptor calls.
  AssertNoContextChangeWithHandleScope ncc;

  // Try to flatten before operating on the string.
  if (name->IsString()) String::cast(*name)->TryFlatten();

  if (!object->CanSetCallback(*name)) return;

  uint32_t index = 0;
  bool is_element = name->AsArrayIndex(&index);

  Handle<Object> old_value = isolate->factory()->the_hole_value();
  bool is_observed = FLAG_harmony_observation && object->map()->is_observed();
  bool preexists = false;
  if (is_observed) {
    if (is_element) {
      preexists = object->HasLocalElement(index);
      if (preexists && object->GetLocalElementAccessorPair(index) == NULL) {
        old_value = Object::GetElement(isolate, object, index);
      }
    } else {
      LookupResult lookup(isolate);
      object->LocalLookup(*name, &lookup, true);
      preexists = lookup.IsProperty();
      if (preexists && lookup.IsDataProperty()) {
        old_value = Object::GetProperty(object, name);
      }
    }
  }

  if (is_element) {
    DefineElementAccessor(
        object, index, getter, setter, attributes, access_control);
  } else {
    DefinePropertyAccessor(
        object, name, getter, setter, attributes, access_control);
  }

  if (is_observed) {
    const char* type = preexists ? "reconfigured" : "new";
    EnqueueChangeRecord(object, type, name, old_value);
  }
}


static bool TryAccessorTransition(JSObject* self,
                                  Map* transitioned_map,
                                  int target_descriptor,
                                  AccessorComponent component,
                                  Object* accessor,
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
  if (target_accessor == accessor && target_attributes == attributes) {
    self->set_map(transitioned_map);
    return true;
  }

  // If either not the same accessor, or not the same attributes, fall back to
  // the slow case.
  return false;
}


static MaybeObject* CopyInsertDescriptor(Map* map,
                                         Name* name,
                                         AccessorPair* accessors,
                                         PropertyAttributes attributes) {
  CallbacksDescriptor new_accessors_desc(name, accessors, attributes);
  return map->CopyInsertDescriptor(&new_accessors_desc, INSERT_TRANSITION);
}


static Handle<Map> CopyInsertDescriptor(Handle<Map> map,
                                        Handle<Name> name,
                                        Handle<AccessorPair> accessors,
                                        PropertyAttributes attributes) {
  CALL_HEAP_FUNCTION(map->GetIsolate(),
                     CopyInsertDescriptor(*map, *name, *accessors, attributes),
                     Map);
}


bool JSObject::DefineFastAccessor(Handle<JSObject> object,
                                  Handle<Name> name,
                                  AccessorComponent component,
                                  Handle<Object> accessor,
                                  PropertyAttributes attributes) {
  ASSERT(accessor->IsSpecFunction() || accessor->IsUndefined());
  Isolate* isolate = object->GetIsolate();
  LookupResult result(isolate);
  object->LocalLookup(*name, &result);

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
      Map* target = result.GetTransitionTarget();
      ASSERT(target->NumberOfOwnDescriptors() ==
             object->map()->NumberOfOwnDescriptors());
      // This works since descriptors are sorted in order of addition.
      ASSERT(object->map()->instance_descriptors()->
             GetKey(descriptor_number) == *name);
      return TryAccessorTransition(*object, target, descriptor_number,
                                   component, *accessor, attributes);
    }
  } else {
    // If not, lookup a transition.
    object->map()->LookupTransition(*object, *name, &result);

    // If there is a transition, try to follow it.
    if (result.IsFound()) {
      Map* target = result.GetTransitionTarget();
      int descriptor_number = target->LastAdded();
      ASSERT(target->instance_descriptors()->GetKey(descriptor_number)
             ->Equals(*name));
      return TryAccessorTransition(*object, target, descriptor_number,
                                   component, *accessor, attributes);
    }
  }

  // If there is no transition yet, add a transition to the a new accessor pair
  // containing the accessor.  Allocate a new pair if there were no source
  // accessors.  Otherwise, copy the pair and modify the accessor.
  Handle<AccessorPair> accessors = source_accessors != NULL
      ? AccessorPair::Copy(Handle<AccessorPair>(source_accessors))
      : isolate->factory()->NewAccessorPair();
  accessors->set(component, *accessor);
  Handle<Map> new_map = CopyInsertDescriptor(Handle<Map>(object->map()),
                                             name, accessors, attributes);
  object->set_map(*new_map);
  return true;
}


Handle<Object> JSObject::SetAccessor(Handle<JSObject> object,
                                     Handle<AccessorInfo> info) {
  Isolate* isolate = object->GetIsolate();
  Factory* factory = isolate->factory();
  Handle<Name> name(Name::cast(info->name()));

  // Check access rights if needed.
  if (object->IsAccessCheckNeeded() &&
      !isolate->MayNamedAccess(*object, *name, v8::ACCESS_SET)) {
    isolate->ReportFailedAccessCheck(*object, v8::ACCESS_SET);
    RETURN_HANDLE_IF_SCHEDULED_EXCEPTION(isolate, Object);
    return factory->undefined_value();
  }

  if (object->IsJSGlobalProxy()) {
    Handle<Object> proto(object->GetPrototype(), isolate);
    if (proto->IsNull()) return object;
    ASSERT(proto->IsJSGlobalObject());
    return SetAccessor(Handle<JSObject>::cast(proto), info);
  }

  // Make sure that the top context does not change when doing callbacks or
  // interceptor calls.
  AssertNoContextChange ncc;

  // Try to flatten before operating on the string.
  if (name->IsString()) FlattenString(Handle<String>::cast(name));

  if (!object->CanSetCallback(*name)) return factory->undefined_value();

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
      case EXTERNAL_PIXEL_ELEMENTS:
      case EXTERNAL_BYTE_ELEMENTS:
      case EXTERNAL_UNSIGNED_BYTE_ELEMENTS:
      case EXTERNAL_SHORT_ELEMENTS:
      case EXTERNAL_UNSIGNED_SHORT_ELEMENTS:
      case EXTERNAL_INT_ELEMENTS:
      case EXTERNAL_UNSIGNED_INT_ELEMENTS:
      case EXTERNAL_FLOAT_ELEMENTS:
      case EXTERNAL_DOUBLE_ELEMENTS:
        // Ignore getters and setters on pixel and external array
        // elements.
        return factory->undefined_value();
      case DICTIONARY_ELEMENTS:
        break;
      case NON_STRICT_ARGUMENTS_ELEMENTS:
        UNIMPLEMENTED();
        break;
    }

    SetElementCallback(object, index, info, info->property_attributes());
  } else {
    // Lookup the name.
    LookupResult result(isolate);
    object->LocalLookup(*name, &result, true);
    // ES5 forbids turning a property into an accessor if it's not
    // configurable (that is IsDontDelete in ES3 and v8), see 8.6.1 (Table 5).
    if (result.IsFound() && (result.IsReadOnly() || result.IsDontDelete())) {
      return factory->undefined_value();
    }

    SetPropertyCallback(object, name, info, info->property_attributes());
  }

  return object;
}


MaybeObject* JSObject::LookupAccessor(Name* name, AccessorComponent component) {
  Heap* heap = GetHeap();

  // Make sure that the top context does not change when doing callbacks or
  // interceptor calls.
  AssertNoContextChangeWithHandleScope ncc;

  // Check access rights if needed.
  if (IsAccessCheckNeeded() &&
      !heap->isolate()->MayNamedAccess(this, name, v8::ACCESS_HAS)) {
    heap->isolate()->ReportFailedAccessCheck(this, v8::ACCESS_HAS);
    RETURN_IF_SCHEDULED_EXCEPTION(heap->isolate());
    return heap->undefined_value();
  }

  // Make the lookup and include prototypes.
  uint32_t index = 0;
  if (name->AsArrayIndex(&index)) {
    for (Object* obj = this;
         obj != heap->null_value();
         obj = JSReceiver::cast(obj)->GetPrototype()) {
      if (obj->IsJSObject() && JSObject::cast(obj)->HasDictionaryElements()) {
        JSObject* js_object = JSObject::cast(obj);
        SeededNumberDictionary* dictionary = js_object->element_dictionary();
        int entry = dictionary->FindEntry(index);
        if (entry != SeededNumberDictionary::kNotFound) {
          Object* element = dictionary->ValueAt(entry);
          if (dictionary->DetailsAt(entry).type() == CALLBACKS &&
              element->IsAccessorPair()) {
            return AccessorPair::cast(element)->GetComponent(component);
          }
        }
      }
    }
  } else {
    for (Object* obj = this;
         obj != heap->null_value();
         obj = JSReceiver::cast(obj)->GetPrototype()) {
      LookupResult result(heap->isolate());
      JSReceiver::cast(obj)->LocalLookup(name, &result);
      if (result.IsFound()) {
        if (result.IsReadOnly()) return heap->undefined_value();
        if (result.IsPropertyCallbacks()) {
          Object* obj = result.GetCallbackObject();
          if (obj->IsAccessorPair()) {
            return AccessorPair::cast(obj)->GetComponent(component);
          }
        }
      }
    }
  }
  return heap->undefined_value();
}


Object* JSObject::SlowReverseLookup(Object* value) {
  if (HasFastProperties()) {
    int number_of_own_descriptors = map()->NumberOfOwnDescriptors();
    DescriptorArray* descs = map()->instance_descriptors();
    for (int i = 0; i < number_of_own_descriptors; i++) {
      if (descs->GetType(i) == FIELD) {
        Object* property = RawFastPropertyAt(descs->GetFieldIndex(i));
        if (FLAG_track_double_fields &&
            descs->GetDetails(i).representation().IsDouble()) {
          ASSERT(property->IsHeapNumber());
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


MaybeObject* Map::RawCopy(int instance_size) {
  Map* result;
  MaybeObject* maybe_result =
      GetHeap()->AllocateMap(instance_type(), instance_size);
  if (!maybe_result->To(&result)) return maybe_result;

  result->set_prototype(prototype());
  result->set_constructor(constructor());
  result->set_bit_field(bit_field());
  result->set_bit_field2(bit_field2());
  int new_bit_field3 = bit_field3();
  new_bit_field3 = OwnsDescriptors::update(new_bit_field3, true);
  new_bit_field3 = NumberOfOwnDescriptorsBits::update(new_bit_field3, 0);
  new_bit_field3 = EnumLengthBits::update(new_bit_field3, kInvalidEnumCache);
  new_bit_field3 = Deprecated::update(new_bit_field3, false);
  new_bit_field3 = IsUnstable::update(new_bit_field3, false);
  result->set_bit_field3(new_bit_field3);
  return result;
}


Handle<Map> Map::CopyNormalized(Handle<Map> map,
                                PropertyNormalizationMode mode,
                                NormalizedMapSharingMode sharing) {
  CALL_HEAP_FUNCTION(map->GetIsolate(),
                     map->CopyNormalized(mode, sharing),
                     Map);
}


MaybeObject* Map::CopyNormalized(PropertyNormalizationMode mode,
                                 NormalizedMapSharingMode sharing) {
  int new_instance_size = instance_size();
  if (mode == CLEAR_INOBJECT_PROPERTIES) {
    new_instance_size -= inobject_properties() * kPointerSize;
  }

  Map* result;
  MaybeObject* maybe_result = RawCopy(new_instance_size);
  if (!maybe_result->To(&result)) return maybe_result;

  if (mode != CLEAR_INOBJECT_PROPERTIES) {
    result->set_inobject_properties(inobject_properties());
  }

  result->set_is_shared(sharing == SHARED_NORMALIZED_MAP);
  result->set_dictionary_map(true);
  result->set_migration_target(false);

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap && result->is_shared()) {
    result->SharedMapVerify();
  }
#endif

  return result;
}


Handle<Map> Map::CopyDropDescriptors(Handle<Map> map) {
  CALL_HEAP_FUNCTION(map->GetIsolate(), map->CopyDropDescriptors(), Map);
}


MaybeObject* Map::CopyDropDescriptors() {
  Map* result;
  MaybeObject* maybe_result = RawCopy(instance_size());
  if (!maybe_result->To(&result)) return maybe_result;

  // Please note instance_type and instance_size are set when allocated.
  result->set_inobject_properties(inobject_properties());
  result->set_unused_property_fields(unused_property_fields());

  result->set_pre_allocated_property_fields(pre_allocated_property_fields());
  result->set_is_shared(false);
  result->ClearCodeCache(GetHeap());
  NotifyLeafMapLayoutChange();
  return result;
}


MaybeObject* Map::ShareDescriptor(DescriptorArray* descriptors,
                                  Descriptor* descriptor) {
  // Sanity check. This path is only to be taken if the map owns its descriptor
  // array, implying that its NumberOfOwnDescriptors equals the number of
  // descriptors in the descriptor array.
  ASSERT(NumberOfOwnDescriptors() ==
         instance_descriptors()->number_of_descriptors());
  Map* result;
  MaybeObject* maybe_result = CopyDropDescriptors();
  if (!maybe_result->To(&result)) return maybe_result;

  Name* name = descriptor->GetKey();

  TransitionArray* transitions;
  MaybeObject* maybe_transitions =
      AddTransition(name, result, SIMPLE_TRANSITION);
  if (!maybe_transitions->To(&transitions)) return maybe_transitions;

  int old_size = descriptors->number_of_descriptors();

  DescriptorArray* new_descriptors;

  if (descriptors->NumberOfSlackDescriptors() > 0) {
    new_descriptors = descriptors;
    new_descriptors->Append(descriptor);
  } else {
    // Descriptor arrays grow by 50%.
    MaybeObject* maybe_descriptors = DescriptorArray::Allocate(
        GetIsolate(), old_size, old_size < 4 ? 1 : old_size / 2);
    if (!maybe_descriptors->To(&new_descriptors)) return maybe_descriptors;

    DescriptorArray::WhitenessWitness witness(new_descriptors);

    // Copy the descriptors, inserting a descriptor.
    for (int i = 0; i < old_size; ++i) {
      new_descriptors->CopyFrom(i, descriptors, i, witness);
    }

    new_descriptors->Append(descriptor, witness);

    if (old_size > 0) {
      // If the source descriptors had an enum cache we copy it. This ensures
      // that the maps to which we push the new descriptor array back can rely
      // on a cache always being available once it is set. If the map has more
      // enumerated descriptors than available in the original cache, the cache
      // will be lazily replaced by the extended cache when needed.
      if (descriptors->HasEnumCache()) {
        new_descriptors->CopyEnumCacheFrom(descriptors);
      }

      Map* map;
      // Replace descriptors by new_descriptors in all maps that share it.
      for (Object* current = GetBackPointer();
           !current->IsUndefined();
           current = map->GetBackPointer()) {
        map = Map::cast(current);
        if (map->instance_descriptors() != descriptors) break;
        map->set_instance_descriptors(new_descriptors);
      }

      set_instance_descriptors(new_descriptors);
    }
  }

  result->SetBackPointer(this);
  result->InitializeDescriptors(new_descriptors);
  ASSERT(result->NumberOfOwnDescriptors() == NumberOfOwnDescriptors() + 1);

  set_transitions(transitions);
  set_owns_descriptors(false);

  return result;
}


MaybeObject* Map::CopyReplaceDescriptors(DescriptorArray* descriptors,
                                         TransitionFlag flag,
                                         Name* name,
                                         SimpleTransitionFlag simple_flag) {
  ASSERT(descriptors->IsSortedNoDuplicates());

  Map* result;
  MaybeObject* maybe_result = CopyDropDescriptors();
  if (!maybe_result->To(&result)) return maybe_result;

  result->InitializeDescriptors(descriptors);

  if (flag == INSERT_TRANSITION && CanHaveMoreTransitions()) {
    TransitionArray* transitions;
    MaybeObject* maybe_transitions = AddTransition(name, result, simple_flag);
    if (!maybe_transitions->To(&transitions)) return maybe_transitions;
    set_transitions(transitions);
    result->SetBackPointer(this);
  } else {
    descriptors->InitializeRepresentations(Representation::Tagged());
  }

  return result;
}


// Since this method is used to rewrite an existing transition tree, it can
// always insert transitions without checking.
MaybeObject* Map::CopyInstallDescriptors(int new_descriptor,
                                         DescriptorArray* descriptors) {
  ASSERT(descriptors->IsSortedNoDuplicates());

  Map* result;
  MaybeObject* maybe_result = CopyDropDescriptors();
  if (!maybe_result->To(&result)) return maybe_result;

  result->InitializeDescriptors(descriptors);
  result->SetNumberOfOwnDescriptors(new_descriptor + 1);

  int unused_property_fields = this->unused_property_fields();
  if (descriptors->GetDetails(new_descriptor).type() == FIELD) {
    unused_property_fields = this->unused_property_fields() - 1;
    if (unused_property_fields < 0) {
      unused_property_fields += JSObject::kFieldsAdded;
    }
  }

  result->set_unused_property_fields(unused_property_fields);
  result->set_owns_descriptors(false);

  Name* name = descriptors->GetKey(new_descriptor);
  TransitionArray* transitions;
  MaybeObject* maybe_transitions =
      AddTransition(name, result, SIMPLE_TRANSITION);
  if (!maybe_transitions->To(&transitions)) return maybe_transitions;

  set_transitions(transitions);
  result->SetBackPointer(this);

  return result;
}


MaybeObject* Map::CopyAsElementsKind(ElementsKind kind, TransitionFlag flag) {
  if (flag == INSERT_TRANSITION) {
    ASSERT(!HasElementsTransition() ||
        ((elements_transition_map()->elements_kind() == DICTIONARY_ELEMENTS ||
          IsExternalArrayElementsKind(
              elements_transition_map()->elements_kind())) &&
         (kind == DICTIONARY_ELEMENTS ||
          IsExternalArrayElementsKind(kind))));
    ASSERT(!IsFastElementsKind(kind) ||
           IsMoreGeneralElementsKindTransition(elements_kind(), kind));
    ASSERT(kind != elements_kind());
  }

  bool insert_transition =
      flag == INSERT_TRANSITION && !HasElementsTransition();

  if (insert_transition && owns_descriptors()) {
    // In case the map owned its own descriptors, share the descriptors and
    // transfer ownership to the new map.
    Map* new_map;
    MaybeObject* maybe_new_map = CopyDropDescriptors();
    if (!maybe_new_map->To(&new_map)) return maybe_new_map;

    MaybeObject* added_elements = set_elements_transition_map(new_map);
    if (added_elements->IsFailure()) return added_elements;

    new_map->set_elements_kind(kind);
    new_map->InitializeDescriptors(instance_descriptors());
    new_map->SetBackPointer(this);
    set_owns_descriptors(false);
    return new_map;
  }

  // In case the map did not own its own descriptors, a split is forced by
  // copying the map; creating a new descriptor array cell.
  // Create a new free-floating map only if we are not allowed to store it.
  Map* new_map;
  MaybeObject* maybe_new_map = Copy();
  if (!maybe_new_map->To(&new_map)) return maybe_new_map;

  new_map->set_elements_kind(kind);

  if (insert_transition) {
    MaybeObject* added_elements = set_elements_transition_map(new_map);
    if (added_elements->IsFailure()) return added_elements;
    new_map->SetBackPointer(this);
  }

  return new_map;
}


MaybeObject* Map::CopyForObserved() {
  ASSERT(!is_observed());

  // In case the map owned its own descriptors, share the descriptors and
  // transfer ownership to the new map.
  Map* new_map;
  MaybeObject* maybe_new_map;
  if (owns_descriptors()) {
    maybe_new_map = CopyDropDescriptors();
  } else {
    maybe_new_map = Copy();
  }
  if (!maybe_new_map->To(&new_map)) return maybe_new_map;

  TransitionArray* transitions;
  MaybeObject* maybe_transitions = AddTransition(GetHeap()->observed_symbol(),
                                                 new_map,
                                                 FULL_TRANSITION);
  if (!maybe_transitions->To(&transitions)) return maybe_transitions;
  set_transitions(transitions);

  new_map->set_is_observed(true);

  if (owns_descriptors()) {
    new_map->InitializeDescriptors(instance_descriptors());
    set_owns_descriptors(false);
  }

  new_map->SetBackPointer(this);
  return new_map;
}


MaybeObject* Map::CopyWithPreallocatedFieldDescriptors() {
  if (pre_allocated_property_fields() == 0) return CopyDropDescriptors();

  // If the map has pre-allocated properties always start out with a descriptor
  // array describing these properties.
  ASSERT(constructor()->IsJSFunction());
  JSFunction* ctor = JSFunction::cast(constructor());
  Map* map = ctor->initial_map();
  DescriptorArray* descriptors = map->instance_descriptors();

  int number_of_own_descriptors = map->NumberOfOwnDescriptors();
  DescriptorArray* new_descriptors;
  MaybeObject* maybe_descriptors =
      descriptors->CopyUpTo(number_of_own_descriptors);
  if (!maybe_descriptors->To(&new_descriptors)) return maybe_descriptors;

  return CopyReplaceDescriptors(new_descriptors, OMIT_TRANSITION);
}


Handle<Map> Map::Copy(Handle<Map> map) {
  CALL_HEAP_FUNCTION(map->GetIsolate(), map->Copy(), Map);
}


MaybeObject* Map::Copy() {
  DescriptorArray* descriptors = instance_descriptors();
  DescriptorArray* new_descriptors;
  int number_of_own_descriptors = NumberOfOwnDescriptors();
  MaybeObject* maybe_descriptors =
      descriptors->CopyUpTo(number_of_own_descriptors);
  if (!maybe_descriptors->To(&new_descriptors)) return maybe_descriptors;

  return CopyReplaceDescriptors(new_descriptors, OMIT_TRANSITION);
}


MaybeObject* Map::CopyAddDescriptor(Descriptor* descriptor,
                                    TransitionFlag flag) {
  DescriptorArray* descriptors = instance_descriptors();

  // Ensure the key is unique.
  MaybeObject* maybe_failure = descriptor->KeyToUniqueName();
  if (maybe_failure->IsFailure()) return maybe_failure;

  int old_size = NumberOfOwnDescriptors();
  int new_size = old_size + 1;

  if (flag == INSERT_TRANSITION &&
      owns_descriptors() &&
      CanHaveMoreTransitions()) {
    return ShareDescriptor(descriptors, descriptor);
  }

  DescriptorArray* new_descriptors;
  MaybeObject* maybe_descriptors =
      DescriptorArray::Allocate(GetIsolate(), old_size, 1);
  if (!maybe_descriptors->To(&new_descriptors)) return maybe_descriptors;

  DescriptorArray::WhitenessWitness witness(new_descriptors);

  // Copy the descriptors, inserting a descriptor.
  for (int i = 0; i < old_size; ++i) {
    new_descriptors->CopyFrom(i, descriptors, i, witness);
  }

  if (old_size != descriptors->number_of_descriptors()) {
    new_descriptors->SetNumberOfDescriptors(new_size);
    new_descriptors->Set(old_size, descriptor, witness);
    new_descriptors->Sort();
  } else {
    new_descriptors->Append(descriptor, witness);
  }

  Name* key = descriptor->GetKey();
  return CopyReplaceDescriptors(new_descriptors, flag, key, SIMPLE_TRANSITION);
}


MaybeObject* Map::CopyInsertDescriptor(Descriptor* descriptor,
                                       TransitionFlag flag) {
  DescriptorArray* old_descriptors = instance_descriptors();

  // Ensure the key is unique.
  MaybeObject* maybe_result = descriptor->KeyToUniqueName();
  if (maybe_result->IsFailure()) return maybe_result;

  // We replace the key if it is already present.
  int index = old_descriptors->SearchWithCache(descriptor->GetKey(), this);
  if (index != DescriptorArray::kNotFound) {
    return CopyReplaceDescriptor(old_descriptors, descriptor, index, flag);
  }
  return CopyAddDescriptor(descriptor, flag);
}


MaybeObject* DescriptorArray::CopyUpToAddAttributes(
    int enumeration_index, PropertyAttributes attributes) {
  if (enumeration_index == 0) return GetHeap()->empty_descriptor_array();

  int size = enumeration_index;

  DescriptorArray* descriptors;
  MaybeObject* maybe_descriptors = Allocate(GetIsolate(), size);
  if (!maybe_descriptors->To(&descriptors)) return maybe_descriptors;
  DescriptorArray::WhitenessWitness witness(descriptors);

  if (attributes != NONE) {
    for (int i = 0; i < size; ++i) {
      Object* value = GetValue(i);
      PropertyDetails details = GetDetails(i);
      int mask = DONT_DELETE | DONT_ENUM;
      // READ_ONLY is an invalid attribute for JS setters/getters.
      if (details.type() != CALLBACKS || !value->IsAccessorPair()) {
        mask |= READ_ONLY;
      }
      details = details.CopyAddAttributes(
          static_cast<PropertyAttributes>(attributes & mask));
      Descriptor desc(GetKey(i), value, details);
      descriptors->Set(i, &desc, witness);
    }
  } else {
    for (int i = 0; i < size; ++i) {
      descriptors->CopyFrom(i, this, i, witness);
    }
  }

  if (number_of_descriptors() != enumeration_index) descriptors->Sort();

  return descriptors;
}


MaybeObject* Map::CopyReplaceDescriptor(DescriptorArray* descriptors,
                                        Descriptor* descriptor,
                                        int insertion_index,
                                        TransitionFlag flag) {
  // Ensure the key is unique.
  MaybeObject* maybe_failure = descriptor->KeyToUniqueName();
  if (maybe_failure->IsFailure()) return maybe_failure;

  Name* key = descriptor->GetKey();
  ASSERT(key == descriptors->GetKey(insertion_index));

  int new_size = NumberOfOwnDescriptors();
  ASSERT(0 <= insertion_index && insertion_index < new_size);

  ASSERT_LT(insertion_index, new_size);

  DescriptorArray* new_descriptors;
  MaybeObject* maybe_descriptors =
      DescriptorArray::Allocate(GetIsolate(), new_size);
  if (!maybe_descriptors->To(&new_descriptors)) return maybe_descriptors;
  DescriptorArray::WhitenessWitness witness(new_descriptors);

  for (int i = 0; i < new_size; ++i) {
    if (i == insertion_index) {
      new_descriptors->Set(i, descriptor, witness);
    } else {
      new_descriptors->CopyFrom(i, descriptors, i, witness);
    }
  }

  // Re-sort if descriptors were removed.
  if (new_size != descriptors->length()) new_descriptors->Sort();

  SimpleTransitionFlag simple_flag =
      (insertion_index == descriptors->number_of_descriptors() - 1)
      ? SIMPLE_TRANSITION
      : FULL_TRANSITION;
  return CopyReplaceDescriptors(new_descriptors, flag, key, simple_flag);
}


void Map::UpdateCodeCache(Handle<Map> map,
                          Handle<Name> name,
                          Handle<Code> code) {
  Isolate* isolate = map->GetIsolate();
  CALL_HEAP_FUNCTION_VOID(isolate,
                          map->UpdateCodeCache(*name, *code));
}


MaybeObject* Map::UpdateCodeCache(Name* name, Code* code) {
  ASSERT(!is_shared() || code->allowed_in_shared_map_code_cache());

  // Allocate the code cache if not present.
  if (code_cache()->IsFixedArray()) {
    Object* result;
    { MaybeObject* maybe_result = GetHeap()->AllocateCodeCache();
      if (!maybe_result->ToObject(&result)) return maybe_result;
    }
    set_code_cache(result);
  }

  // Update the code cache.
  return CodeCache::cast(code_cache())->Update(name, code);
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
  ASSERT(!code_cache()->IsFixedArray());
  CodeCache::cast(code_cache())->RemoveByIndex(name, code, index);
}


// An iterator over all map transitions in an descriptor array, reusing the map
// field of the contens array while it is running.
class IntrusiveMapTransitionIterator {
 public:
  explicit IntrusiveMapTransitionIterator(TransitionArray* transition_array)
      : transition_array_(transition_array) { }

  void Start() {
    ASSERT(!IsIterating());
    *TransitionArrayHeader() = Smi::FromInt(0);
  }

  bool IsIterating() {
    return (*TransitionArrayHeader())->IsSmi();
  }

  Map* Next() {
    ASSERT(IsIterating());
    int index = Smi::cast(*TransitionArrayHeader())->value();
    int number_of_transitions = transition_array_->number_of_transitions();
    while (index < number_of_transitions) {
      *TransitionArrayHeader() = Smi::FromInt(index + 1);
      return transition_array_->GetTarget(index);
    }

    *TransitionArrayHeader() = transition_array_->GetHeap()->fixed_array_map();
    return NULL;
  }

 private:
  Object** TransitionArrayHeader() {
    return HeapObject::RawField(transition_array_, TransitionArray::kMapOffset);
  }

  TransitionArray* transition_array_;
};


// An iterator over all prototype transitions, reusing the map field of the
// underlying array while it is running.
class IntrusivePrototypeTransitionIterator {
 public:
  explicit IntrusivePrototypeTransitionIterator(HeapObject* proto_trans)
      : proto_trans_(proto_trans) { }

  void Start() {
    ASSERT(!IsIterating());
    *Header() = Smi::FromInt(0);
  }

  bool IsIterating() {
    return (*Header())->IsSmi();
  }

  Map* Next() {
    ASSERT(IsIterating());
    int transitionNumber = Smi::cast(*Header())->value();
    if (transitionNumber < NumberOfTransitions()) {
      *Header() = Smi::FromInt(transitionNumber + 1);
      return GetTransition(transitionNumber);
    }
    *Header() = proto_trans_->GetHeap()->fixed_array_map();
    return NULL;
  }

 private:
  Object** Header() {
    return HeapObject::RawField(proto_trans_, FixedArray::kMapOffset);
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

  HeapObject* proto_trans_;
};


// To traverse the transition tree iteratively, we have to store two kinds of
// information in a map: The parent map in the traversal and which children of a
// node have already been visited. To do this without additional memory, we
// temporarily reuse two maps with known values:
//
//  (1) The map of the map temporarily holds the parent, and is restored to the
//      meta map afterwards.
//
//  (2) The info which children have already been visited depends on which part
//      of the map we currently iterate:
//
//    (a) If we currently follow normal map transitions, we temporarily store
//        the current index in the map of the FixedArray of the desciptor
//        array's contents, and restore it to the fixed array map afterwards.
//        Note that a single descriptor can have 0, 1, or 2 transitions.
//
//    (b) If we currently follow prototype transitions, we temporarily store
//        the current index in the map of the FixedArray holding the prototype
//        transitions, and restore it to the fixed array map afterwards.
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

  // Start iterating over this map's children, possibly destroying a FixedArray
  // map (see explanation above).
  void ChildIteratorStart() {
    if (HasTransitionArray()) {
      if (HasPrototypeTransitions()) {
        IntrusivePrototypeTransitionIterator(GetPrototypeTransitions()).Start();
      }

      IntrusiveMapTransitionIterator(transitions()).Start();
    }
  }

  // If we have an unvisited child map, return that one and advance. If we have
  // none, return NULL and reset any destroyed FixedArray maps.
  TraversableMap* ChildIteratorNext() {
    TransitionArray* transition_array = unchecked_transition_array();
    if (!transition_array->map()->IsSmi() &&
        !transition_array->IsTransitionArray()) {
      return NULL;
    }

    if (transition_array->HasPrototypeTransitions()) {
      HeapObject* proto_transitions =
          transition_array->UncheckedPrototypeTransitions();
      IntrusivePrototypeTransitionIterator proto_iterator(proto_transitions);
      if (proto_iterator.IsIterating()) {
        Map* next = proto_iterator.Next();
        if (next != NULL) return static_cast<TraversableMap*>(next);
      }
    }

    IntrusiveMapTransitionIterator transition_iterator(transition_array);
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
  TraversableMap* current = static_cast<TraversableMap*>(this);
  current->ChildIteratorStart();
  while (true) {
    TraversableMap* child = current->ChildIteratorNext();
    if (child != NULL) {
      child->ChildIteratorStart();
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


MaybeObject* CodeCache::Update(Name* name, Code* code) {
  // The number of monomorphic stubs for normal load/store/call IC's can grow to
  // a large number and therefore they need to go into a hash table. They are
  // used to load global properties from cells.
  if (code->type() == Code::NORMAL) {
    // Make sure that a hash table is allocated for the normal load code cache.
    if (normal_type_cache()->IsUndefined()) {
      Object* result;
      { MaybeObject* maybe_result =
            CodeCacheHashTable::Allocate(GetHeap(),
                                         CodeCacheHashTable::kInitialSize);
        if (!maybe_result->ToObject(&result)) return maybe_result;
      }
      set_normal_type_cache(result);
    }
    return UpdateNormalTypeCache(name, code);
  } else {
    ASSERT(default_cache()->IsFixedArray());
    return UpdateDefaultCache(name, code);
  }
}


MaybeObject* CodeCache::UpdateDefaultCache(Name* name, Code* code) {
  // When updating the default code cache we disregard the type encoded in the
  // flags. This allows call constant stubs to overwrite call field
  // stubs, etc.
  Code::Flags flags = Code::RemoveTypeFromFlags(code->flags());

  // First check whether we can update existing code cache without
  // extending it.
  FixedArray* cache = default_cache();
  int length = cache->length();
  int deleted_index = -1;
  for (int i = 0; i < length; i += kCodeCacheEntrySize) {
    Object* key = cache->get(i);
    if (key->IsNull()) {
      if (deleted_index < 0) deleted_index = i;
      continue;
    }
    if (key->IsUndefined()) {
      if (deleted_index >= 0) i = deleted_index;
      cache->set(i + kCodeCacheEntryNameOffset, name);
      cache->set(i + kCodeCacheEntryCodeOffset, code);
      return this;
    }
    if (name->Equals(Name::cast(key))) {
      Code::Flags found =
          Code::cast(cache->get(i + kCodeCacheEntryCodeOffset))->flags();
      if (Code::RemoveTypeFromFlags(found) == flags) {
        cache->set(i + kCodeCacheEntryCodeOffset, code);
        return this;
      }
    }
  }

  // Reached the end of the code cache.  If there were deleted
  // elements, reuse the space for the first of them.
  if (deleted_index >= 0) {
    cache->set(deleted_index + kCodeCacheEntryNameOffset, name);
    cache->set(deleted_index + kCodeCacheEntryCodeOffset, code);
    return this;
  }

  // Extend the code cache with some new entries (at least one). Must be a
  // multiple of the entry size.
  int new_length = length + ((length >> 1)) + kCodeCacheEntrySize;
  new_length = new_length - new_length % kCodeCacheEntrySize;
  ASSERT((new_length % kCodeCacheEntrySize) == 0);
  Object* result;
  { MaybeObject* maybe_result = cache->CopySize(new_length);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }

  // Add the (name, code) pair to the new cache.
  cache = FixedArray::cast(result);
  cache->set(length + kCodeCacheEntryNameOffset, name);
  cache->set(length + kCodeCacheEntryCodeOffset, code);
  set_default_cache(cache);
  return this;
}


MaybeObject* CodeCache::UpdateNormalTypeCache(Name* name, Code* code) {
  // Adding a new entry can cause a new cache to be allocated.
  CodeCacheHashTable* cache = CodeCacheHashTable::cast(normal_type_cache());
  Object* new_cache;
  { MaybeObject* maybe_new_cache = cache->Put(name, code);
    if (!maybe_new_cache->ToObject(&new_cache)) return maybe_new_cache;
  }
  set_normal_type_cache(new_cache);
  return this;
}


Object* CodeCache::Lookup(Name* name, Code::Flags flags) {
  if (Code::ExtractTypeFromFlags(flags) == Code::NORMAL) {
    return LookupNormalTypeCache(name, flags);
  } else {
    return LookupDefaultCache(name, flags);
  }
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
      if (code->flags() == flags) {
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
    ASSERT(!normal_type_cache()->IsUndefined());
    CodeCacheHashTable* cache = CodeCacheHashTable::cast(normal_type_cache());
    ASSERT(cache->GetIndex(Name::cast(name), code->flags()) == index);
    cache->RemoveByIndex(index);
  } else {
    FixedArray* array = default_cache();
    ASSERT(array->length() >= index && array->get(index)->IsCode());
    // Use null instead of undefined for deleted elements to distinguish
    // deleted elements from unused elements.  This distinction is used
    // when looking up in the cache and when updating the cache.
    ASSERT_EQ(1, kCodeCacheEntryCodeOffset - kCodeCacheEntryNameOffset);
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
  CodeCacheHashTableKey(Name* name, Code::Flags flags)
      : name_(name), flags_(flags), code_(NULL) { }

  CodeCacheHashTableKey(Name* name, Code* code)
      : name_(name),
        flags_(code->flags()),
        code_(code) { }


  bool IsMatch(Object* other) {
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

  uint32_t Hash() { return NameFlagsHashHelper(name_, flags_); }

  uint32_t HashForObject(Object* obj) {
    FixedArray* pair = FixedArray::cast(obj);
    Name* name = Name::cast(pair->get(0));
    Code* code = Code::cast(pair->get(1));
    return NameFlagsHashHelper(name, code->flags());
  }

  MUST_USE_RESULT MaybeObject* AsObject(Heap* heap) {
    ASSERT(code_ != NULL);
    Object* obj;
    { MaybeObject* maybe_obj = heap->AllocateFixedArray(2);
      if (!maybe_obj->ToObject(&obj)) return maybe_obj;
    }
    FixedArray* pair = FixedArray::cast(obj);
    pair->set(0, name_);
    pair->set(1, code_);
    return pair;
  }

 private:
  Name* name_;
  Code::Flags flags_;
  // TODO(jkummerow): We should be able to get by without this.
  Code* code_;
};


Object* CodeCacheHashTable::Lookup(Name* name, Code::Flags flags) {
  CodeCacheHashTableKey key(name, flags);
  int entry = FindEntry(&key);
  if (entry == kNotFound) return GetHeap()->undefined_value();
  return get(EntryToIndex(entry) + 1);
}


MaybeObject* CodeCacheHashTable::Put(Name* name, Code* code) {
  CodeCacheHashTableKey key(name, code);
  Object* obj;
  { MaybeObject* maybe_obj = EnsureCapacity(1, &key);
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
  }

  // Don't use |this|, as the table might have grown.
  CodeCacheHashTable* cache = reinterpret_cast<CodeCacheHashTable*>(obj);

  int entry = cache->FindInsertionEntry(key.Hash());
  Object* k;
  { MaybeObject* maybe_k = key.AsObject(GetHeap());
    if (!maybe_k->ToObject(&k)) return maybe_k;
  }

  cache->set(EntryToIndex(entry), k);
  cache->set(EntryToIndex(entry) + 1, code);
  cache->ElementAdded();
  return cache;
}


int CodeCacheHashTable::GetIndex(Name* name, Code::Flags flags) {
  CodeCacheHashTableKey key(name, flags);
  int entry = FindEntry(&key);
  return (entry == kNotFound) ? -1 : entry;
}


void CodeCacheHashTable::RemoveByIndex(int index) {
  ASSERT(index >= 0);
  Heap* heap = GetHeap();
  set(EntryToIndex(index), heap->the_hole_value());
  set(EntryToIndex(index) + 1, heap->the_hole_value());
  ElementRemoved();
}


void PolymorphicCodeCache::Update(Handle<PolymorphicCodeCache> cache,
                                  MapHandleList* maps,
                                  Code::Flags flags,
                                  Handle<Code> code) {
  Isolate* isolate = cache->GetIsolate();
  CALL_HEAP_FUNCTION_VOID(isolate, cache->Update(maps, flags, *code));
}


MaybeObject* PolymorphicCodeCache::Update(MapHandleList* maps,
                                          Code::Flags flags,
                                          Code* code) {
  // Initialize cache if necessary.
  if (cache()->IsUndefined()) {
    Object* result;
    { MaybeObject* maybe_result =
          PolymorphicCodeCacheHashTable::Allocate(
              GetHeap(),
              PolymorphicCodeCacheHashTable::kInitialSize);
      if (!maybe_result->ToObject(&result)) return maybe_result;
    }
    set_cache(result);
  } else {
    // This entry shouldn't be contained in the cache yet.
    ASSERT(PolymorphicCodeCacheHashTable::cast(cache())
               ->Lookup(maps, flags)->IsUndefined());
  }
  PolymorphicCodeCacheHashTable* hash_table =
      PolymorphicCodeCacheHashTable::cast(cache());
  Object* new_cache;
  { MaybeObject* maybe_new_cache = hash_table->Put(maps, flags, code);
    if (!maybe_new_cache->ToObject(&new_cache)) return maybe_new_cache;
  }
  set_cache(new_cache);
  return this;
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

  bool IsMatch(Object* other) {
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

  uint32_t Hash() {
    return MapsHashHelper(maps_, code_flags_);
  }

  uint32_t HashForObject(Object* obj) {
    MapHandleList other_maps(kDefaultListAllocationSize);
    int other_flags;
    FromObject(obj, &other_flags, &other_maps);
    return MapsHashHelper(&other_maps, other_flags);
  }

  MUST_USE_RESULT MaybeObject* AsObject(Heap* heap) {
    Object* obj;
    // The maps in |maps_| must be copied to a newly allocated FixedArray,
    // both because the referenced MapList is short-lived, and because C++
    // objects can't be stored in the heap anyway.
    { MaybeObject* maybe_obj =
          heap->AllocateUninitializedFixedArray(maps_->length() + 1);
      if (!maybe_obj->ToObject(&obj)) return maybe_obj;
    }
    FixedArray* list = FixedArray::cast(obj);
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
                                              int code_flags) {
  PolymorphicCodeCacheHashTableKey key(maps, code_flags);
  int entry = FindEntry(&key);
  if (entry == kNotFound) return GetHeap()->undefined_value();
  return get(EntryToIndex(entry) + 1);
}


MaybeObject* PolymorphicCodeCacheHashTable::Put(MapHandleList* maps,
                                                int code_flags,
                                                Code* code) {
  PolymorphicCodeCacheHashTableKey key(maps, code_flags);
  Object* obj;
  { MaybeObject* maybe_obj = EnsureCapacity(1, &key);
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
  }
  PolymorphicCodeCacheHashTable* cache =
      reinterpret_cast<PolymorphicCodeCacheHashTable*>(obj);
  int entry = cache->FindInsertionEntry(key.Hash());
  { MaybeObject* maybe_obj = key.AsObject(GetHeap());
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
  }
  cache->set(EntryToIndex(entry), obj);
  cache->set(EntryToIndex(entry) + 1, code);
  cache->ElementAdded();
  return cache;
}


MaybeObject* FixedArray::AddKeysFromJSArray(JSArray* array) {
  ElementsAccessor* accessor = array->GetElementsAccessor();
  MaybeObject* maybe_result =
      accessor->AddElementsToFixedArray(array, array, this);
  FixedArray* result;
  if (!maybe_result->To<FixedArray>(&result)) return maybe_result;
#ifdef DEBUG
  if (FLAG_enable_slow_asserts) {
    for (int i = 0; i < result->length(); i++) {
      Object* current = result->get(i);
      ASSERT(current->IsNumber() || current->IsName());
    }
  }
#endif
  return result;
}


MaybeObject* FixedArray::UnionOfKeys(FixedArray* other) {
  ElementsAccessor* accessor = ElementsAccessor::ForArray(other);
  MaybeObject* maybe_result =
      accessor->AddElementsToFixedArray(NULL, NULL, this, other);
  FixedArray* result;
  if (!maybe_result->To(&result)) return maybe_result;
#ifdef DEBUG
  if (FLAG_enable_slow_asserts) {
    for (int i = 0; i < result->length(); i++) {
      Object* current = result->get(i);
      ASSERT(current->IsNumber() || current->IsName());
    }
  }
#endif
  return result;
}


MaybeObject* FixedArray::CopySize(int new_length) {
  Heap* heap = GetHeap();
  if (new_length == 0) return heap->empty_fixed_array();
  Object* obj;
  { MaybeObject* maybe_obj = heap->AllocateFixedArray(new_length);
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
  }
  FixedArray* result = FixedArray::cast(obj);
  // Copy the content
  DisallowHeapAllocation no_gc;
  int len = length();
  if (new_length < len) len = new_length;
  // We are taking the map from the old fixed array so the map is sure to
  // be an immortal immutable object.
  result->set_map_no_write_barrier(map());
  WriteBarrierMode mode = result->GetWriteBarrierMode(no_gc);
  for (int i = 0; i < len; i++) {
    result->set(i, get(i), mode);
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


MaybeObject* DescriptorArray::Allocate(Isolate* isolate,
                                       int number_of_descriptors,
                                       int slack) {
  Heap* heap = isolate->heap();
  // Do not use DescriptorArray::cast on incomplete object.
  int size = number_of_descriptors + slack;
  if (size == 0) return heap->empty_descriptor_array();
  FixedArray* result;
  // Allocate the array of keys.
  MaybeObject* maybe_array = heap->AllocateFixedArray(LengthFor(size));
  if (!maybe_array->To(&result)) return maybe_array;

  result->set(kDescriptorLengthIndex, Smi::FromInt(number_of_descriptors));
  result->set(kEnumCacheIndex, Smi::FromInt(0));
  return result;
}


void DescriptorArray::ClearEnumCache() {
  set(kEnumCacheIndex, Smi::FromInt(0));
}


void DescriptorArray::SetEnumCache(FixedArray* bridge_storage,
                                   FixedArray* new_cache,
                                   Object* new_index_cache) {
  ASSERT(bridge_storage->length() >= kEnumCacheBridgeLength);
  ASSERT(new_index_cache->IsSmi() || new_index_cache->IsFixedArray());
  ASSERT(!IsEmpty());
  ASSERT(!HasEnumCache() || new_cache->length() > GetEnumCache()->length());
  FixedArray::cast(bridge_storage)->
    set(kEnumCacheBridgeCacheIndex, new_cache);
  FixedArray::cast(bridge_storage)->
    set(kEnumCacheBridgeIndicesCacheIndex, new_index_cache);
  set(kEnumCacheIndex, bridge_storage);
}


void DescriptorArray::CopyFrom(int dst_index,
                               DescriptorArray* src,
                               int src_index,
                               const WhitenessWitness& witness) {
  Object* value = src->GetValue(src_index);
  PropertyDetails details = src->GetDetails(src_index);
  Descriptor desc(src->GetKey(src_index), value, details);
  Set(dst_index, &desc, witness);
}


// Generalize the |other| descriptor array by merging it into the (at least
// partly) updated |this| descriptor array.
// The method merges two descriptor array in three parts. Both descriptor arrays
// are identical up to |verbatim|. They also overlap in keys up to |valid|.
// Between |verbatim| and |valid|, the resulting descriptor type as well as the
// representation are generalized from both |this| and |other|. Beyond |valid|,
// the descriptors are copied verbatim from |other| up to |new_size|.
// In case of incompatible types, the type and representation of |other| is
// used.
MaybeObject* DescriptorArray::Merge(int verbatim,
                                    int valid,
                                    int new_size,
                                    int modify_index,
                                    StoreMode store_mode,
                                    DescriptorArray* other) {
  ASSERT(verbatim <= valid);
  ASSERT(valid <= new_size);

  DescriptorArray* result;
  // Allocate a new descriptor array large enough to hold the required
  // descriptors, with minimally the exact same size as this descriptor array.
  MaybeObject* maybe_descriptors = DescriptorArray::Allocate(
      GetIsolate(), new_size,
      Max(new_size, other->number_of_descriptors()) - new_size);
  if (!maybe_descriptors->To(&result)) return maybe_descriptors;
  ASSERT(result->length() > length() ||
         result->NumberOfSlackDescriptors() > 0 ||
         result->number_of_descriptors() == other->number_of_descriptors());
  ASSERT(result->number_of_descriptors() == new_size);

  DescriptorArray::WhitenessWitness witness(result);

  int descriptor;

  // 0 -> |verbatim|
  int current_offset = 0;
  for (descriptor = 0; descriptor < verbatim; descriptor++) {
    if (GetDetails(descriptor).type() == FIELD) current_offset++;
    result->CopyFrom(descriptor, other, descriptor, witness);
  }

  // |verbatim| -> |valid|
  for (; descriptor < valid; descriptor++) {
    Name* key = GetKey(descriptor);
    PropertyDetails details = GetDetails(descriptor);
    PropertyDetails other_details = other->GetDetails(descriptor);

    if (details.type() == FIELD || other_details.type() == FIELD ||
        (store_mode == FORCE_FIELD && descriptor == modify_index) ||
        (details.type() == CONSTANT &&
         other_details.type() == CONSTANT &&
         GetValue(descriptor) != other->GetValue(descriptor))) {
      Representation representation =
          details.representation().generalize(other_details.representation());
      FieldDescriptor d(key,
                        current_offset++,
                        other_details.attributes(),
                        representation);
      result->Set(descriptor, &d, witness);
    } else {
      result->CopyFrom(descriptor, other, descriptor, witness);
    }
  }

  // |valid| -> |new_size|
  for (; descriptor < new_size; descriptor++) {
    PropertyDetails details = other->GetDetails(descriptor);
    if (details.type() == FIELD ||
        (store_mode == FORCE_FIELD && descriptor == modify_index)) {
      Name* key = other->GetKey(descriptor);
      FieldDescriptor d(key,
                        current_offset++,
                        details.attributes(),
                        details.representation());
      result->Set(descriptor, &d, witness);
    } else {
      result->CopyFrom(descriptor, other, descriptor, witness);
    }
  }

  result->Sort();
  return result;
}


// Checks whether a merge of |other| into |this| would return a copy of |this|.
bool DescriptorArray::IsMoreGeneralThan(int verbatim,
                                        int valid,
                                        int new_size,
                                        DescriptorArray* other) {
  ASSERT(verbatim <= valid);
  ASSERT(valid <= new_size);
  if (valid != new_size) return false;

  for (int descriptor = verbatim; descriptor < valid; descriptor++) {
    PropertyDetails details = GetDetails(descriptor);
    PropertyDetails other_details = other->GetDetails(descriptor);
    if (!other_details.representation().fits_into(details.representation())) {
      return false;
    }
    if (details.type() == CONSTANT) {
      if (other_details.type() != CONSTANT) return false;
      if (GetValue(descriptor) != other->GetValue(descriptor)) return false;
    }
  }

  return true;
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
  ASSERT(IsSortedNoDuplicates());
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


MaybeObject* DeoptimizationInputData::Allocate(Isolate* isolate,
                                               int deopt_entry_count,
                                               PretenureFlag pretenure) {
  ASSERT(deopt_entry_count > 0);
  return isolate->heap()->AllocateFixedArray(LengthFor(deopt_entry_count),
                                             pretenure);
}


MaybeObject* DeoptimizationOutputData::Allocate(Isolate* isolate,
                                                int number_of_deopt_points,
                                                PretenureFlag pretenure) {
  if (number_of_deopt_points == 0) return isolate->heap()->empty_fixed_array();
  return isolate->heap()->AllocateFixedArray(
      LengthOfFixedArray(number_of_deopt_points), pretenure);
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


static bool IsIdentifier(UnicodeCache* cache, Name* name) {
  // Checks whether the buffer contains an identifier (no escape).
  if (!name->IsString()) return false;
  String* string = String::cast(name);
  if (string->length() == 0) return false;
  ConsStringIteratorOp op;
  StringCharacterStream stream(string, &op);
  if (!cache->IsIdentifierStart(stream.GetNext())) {
    return false;
  }
  while (stream.HasMore()) {
    if (!cache->IsIdentifierPart(stream.GetNext())) {
      return false;
    }
  }
  return true;
}


bool Name::IsCacheable(Isolate* isolate) {
  return IsSymbol() ||
      IsIdentifier(isolate->unicode_cache(), this) ||
      this == isolate->heap()->hidden_string();
}


bool String::LooksValid() {
  if (!GetIsolate()->heap()->Contains(this)) return false;
  return true;
}


String::FlatContent String::GetFlatContent() {
  ASSERT(!AllowHeapAllocation::IsAllowed());
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
    ASSERT(shape.representation_tag() != kConsStringTag &&
           shape.representation_tag() != kSlicedStringTag);
  }
  if (shape.encoding_tag() == kOneByteStringTag) {
    const uint8_t* start;
    if (shape.representation_tag() == kSeqStringTag) {
      start = SeqOneByteString::cast(string)->GetChars();
    } else {
      start = ExternalAsciiString::cast(string)->GetChars();
    }
    return FlatContent(Vector<const uint8_t>(start + offset, length));
  } else {
    ASSERT(shape.encoding_tag() == kTwoByteStringTag);
    const uc16* start;
    if (shape.representation_tag() == kSeqStringTag) {
      start = SeqTwoByteString::cast(string)->GetChars();
    } else {
      start = ExternalTwoByteString::cast(string)->GetChars();
    }
    return FlatContent(Vector<const uc16>(start + offset, length));
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


const uc16* String::GetTwoByteData() {
  return GetTwoByteData(0);
}


const uc16* String::GetTwoByteData(unsigned start) {
  ASSERT(!IsOneByteRepresentationUnderneath());
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


// Archive statics that are thread local.
char* Relocatable::ArchiveState(Isolate* isolate, char* to) {
  *reinterpret_cast<Relocatable**>(to) = isolate->relocatable_top();
  isolate->set_relocatable_top(NULL);
  return to + ArchiveSpacePerThread();
}


// Restore statics that are thread local.
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
  ASSERT(str->IsFlat());
  DisallowHeapAllocation no_gc;
  // This does not actually prevent the vector from being relocated later.
  String::FlatContent content = str->GetFlatContent();
  ASSERT(content.IsFlat());
  is_ascii_ = content.IsAscii();
  if (is_ascii_) {
    start_ = content.ToOneByteVector().start();
  } else {
    start_ = content.ToUC16Vector().start();
  }
}


String* ConsStringIteratorOp::Operate(String* string,
                                      unsigned* offset_out,
                                      int32_t* type_out,
                                      unsigned* length_out) {
  ASSERT(string->IsConsString());
  ConsString* cons_string = ConsString::cast(string);
  // Set up search data.
  root_ = cons_string;
  consumed_ = *offset_out;
  // Now search.
  return Search(offset_out, type_out, length_out);
}


String* ConsStringIteratorOp::Search(unsigned* offset_out,
                                     int32_t* type_out,
                                     unsigned* length_out) {
  ConsString* cons_string = root_;
  // Reset the stack, pushing the root string.
  depth_ = 1;
  maximum_depth_ = 1;
  frames_[0] = cons_string;
  const unsigned consumed = consumed_;
  unsigned offset = 0;
  while (true) {
    // Loop until the string is found which contains the target offset.
    String* string = cons_string->first();
    unsigned length = string->length();
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
      // Tell the stack we're done decending.
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
        // TODO(dcarney) Add back root optimization.
        continue;
      }
      // Need this to be updated for the current string.
      length = string->length();
      // Account for the possibility of an empty right leaf.
      // This happens only if we have asked for an offset outside the string.
      if (length == 0) {
        // Reset depth so future operations will return null immediately.
        Reset();
        return NULL;
      }
      // Tell the stack we're done decending.
      AdjustMaximumDepth();
      // Pop stack so next iteration is in correct place.
      Pop();
    }
    ASSERT(length != 0);
    // Adjust return values and exit.
    consumed_ = offset + length;
    *offset_out = consumed - offset;
    *type_out = type;
    *length_out = length;
    return string;
  }
  UNREACHABLE();
  return NULL;
}


String* ConsStringIteratorOp::NextLeaf(bool* blew_stack,
                                       int32_t* type_out,
                                       unsigned* length_out) {
  while (true) {
    // Tree traversal complete.
    if (depth_ == 0) {
      *blew_stack = false;
      return NULL;
    }
    // We've lost track of higher nodes.
    if (maximum_depth_ - depth_ == kStackSize) {
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
      unsigned length = static_cast<unsigned>(string->length());
      // Could be a flattened ConsString.
      if (length == 0) continue;
      *length_out = length;
      *type_out = type;
      consumed_ += length;
      return string;
    }
    cons_string = ConsString::cast(string);
    // TODO(dcarney) Add back root optimization.
    PushRight(cons_string);
    // Need to traverse all the way left.
    while (true) {
      // Continue left.
      string = cons_string->first();
      type = string->map()->instance_type();
      if ((type & kStringRepresentationMask) != kConsStringTag) {
        AdjustMaximumDepth();
        unsigned length = static_cast<unsigned>(string->length());
        ASSERT(length != 0);
        *length_out = length;
        *type_out = type;
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
  ASSERT(index >= 0 && index < this->length());

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
    ASSERT(0 <= from && from <= to && to <= source->length());
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
  uint32_t pa_addr = reinterpret_cast<uint32_t>(a);
  uint32_t pb_addr = reinterpret_cast<uint32_t>(b);
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
    ASSERT(sizeof(Chars1) != sizeof(Chars2));
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

    inline void Init(String* string, unsigned len) {
      op_->Reset();
      int32_t type = string->map()->instance_type();
      String::Visit(string, 0, *this, *op_, type, len);
    }

    inline void VisitOneByteString(const uint8_t* chars, unsigned length) {
      is_one_byte_ = true;
      buffer8_ = chars;
      length_ = length;
    }

    inline void VisitTwoByteString(const uint16_t* chars, unsigned length) {
      is_one_byte_ = false;
      buffer16_ = chars;
      length_ = length;
    }

    void Advance(unsigned consumed) {
      ASSERT(consumed <= length_);
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
      ASSERT(op_->HasMore());
      int32_t type = 0;
      unsigned length = 0;
      String* next = op_->ContinueOperation(&type, &length);
      ASSERT(next != NULL);
      ConsStringNullOp null_op;
      String::Visit(next, 0, *this, null_op, type, length);
    }

    ConsStringIteratorOp* const op_;
    bool is_one_byte_;
    unsigned length_;
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
  static inline bool Equals(State* state_1, State* state_2, unsigned to_check) {
    const Chars1* a = reinterpret_cast<const Chars1*>(state_1->buffer8_);
    const Chars2* b = reinterpret_cast<const Chars2*>(state_2->buffer8_);
    return RawStringComparator<Chars1, Chars2>::compare(a, b, to_check);
  }

  bool Equals(unsigned length, String* string_1, String* string_2) {
    ASSERT(length != 0);
    state_1_.Init(string_1, length);
    state_2_.Init(string_2, length);
    while (true) {
      unsigned to_check = Min(state_1_.length_, state_2_.length_);
      ASSERT(to_check > 0 && to_check <= length);
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
  // Fast check: negative check with lengths.
  int len = length();
  if (len != other->length()) return false;
  if (len == 0) return true;

  // Fast check: if hash code is computed for both strings
  // a fast negative check can be performed.
  if (HasHashCode() && other->HasHashCode()) {
#ifdef DEBUG
    if (FLAG_enable_slow_asserts) {
      if (Hash() != other->Hash()) {
        bool found_difference = false;
        for (int i = 0; i < len; i++) {
          if (Get(i) != other->Get(i)) {
            found_difference = true;
            break;
          }
        }
        ASSERT(found_difference);
      }
    }
#endif
    if (Hash() != other->Hash()) return false;
  }

  // We know the strings are both non-empty. Compare the first chars
  // before we try to flatten the strings.
  if (this->Get(0) != other->Get(0)) return false;

  String* lhs = this->TryFlattenGetString();
  String* rhs = other->TryFlattenGetString();

  // TODO(dcarney): Compare all types of flat strings with a Visitor.
  if (StringShape(lhs).IsSequentialAscii() &&
      StringShape(rhs).IsSequentialAscii()) {
    const uint8_t* str1 = SeqOneByteString::cast(lhs)->GetChars();
    const uint8_t* str2 = SeqOneByteString::cast(rhs)->GetChars();
    return CompareRawStringContents(str1, str2, len);
  }

  Isolate* isolate = GetIsolate();
  StringComparator comparator(isolate->objects_string_compare_iterator_a(),
                              isolate->objects_string_compare_iterator_b());

  return comparator.Equals(static_cast<unsigned>(len), lhs, rhs);
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
    ASSERT(cursor > 0 && cursor <= remaining_in_str);
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


class IteratingStringHasher: public StringHasher {
 public:
  static inline uint32_t Hash(String* string, uint32_t seed) {
    const unsigned len = static_cast<unsigned>(string->length());
    IteratingStringHasher hasher(len, seed);
    if (hasher.has_trivial_hash()) {
      return hasher.GetHashField();
    }
    int32_t type = string->map()->instance_type();
    ConsStringNullOp null_op;
    String::Visit(string, 0, hasher, null_op, type, len);
    // Flat strings terminate immediately.
    if (hasher.consumed_ == len) {
      ASSERT(!string->IsConsString());
      return hasher.GetHashField();
    }
    ASSERT(string->IsConsString());
    // This is a ConsString, iterate across it.
    ConsStringIteratorOp op;
    unsigned offset = 0;
    unsigned leaf_length = len;
    string = op.Operate(string, &offset, &type, &leaf_length);
    while (true) {
      ASSERT(hasher.consumed_ < len);
      String::Visit(string, 0, hasher, null_op, type, leaf_length);
      if (hasher.consumed_ == len) break;
      string = op.ContinueOperation(&type, &leaf_length);
      // This should be taken care of by the length check.
      ASSERT(string != NULL);
    }
    return hasher.GetHashField();
  }
  inline void VisitOneByteString(const uint8_t* chars, unsigned length) {
    AddCharacters(chars, static_cast<int>(length));
    consumed_ += length;
  }
  inline void VisitTwoByteString(const uint16_t* chars, unsigned length) {
    AddCharacters(chars, static_cast<int>(length));
    consumed_ += length;
  }

 private:
  inline IteratingStringHasher(int len, uint32_t seed)
    : StringHasher(len, seed),
      consumed_(0) {}
  unsigned consumed_;
  DISALLOW_COPY_AND_ASSIGN(IteratingStringHasher);
};


uint32_t String::ComputeAndSetHash() {
  // Should only be called if hash code has not yet been computed.
  ASSERT(!HasHashCode());

  // Store the hash code in the object.
  uint32_t field = IteratingStringHasher::Hash(this, GetHeap()->HashSeed());
  set_hash_field(field);

  // Check the hash code is there.
  ASSERT(HasHashCode());
  uint32_t result = field >> kHashShift;
  ASSERT(result != 0);  // Ensure that the hash value of 0 is never computed.
  return result;
}


bool String::ComputeArrayIndex(uint32_t* index) {
  int length = this->length();
  if (length == 0 || length > kMaxArrayIndexSize) return false;
  ConsStringIteratorOp op;
  StringCharacterStream stream(this, &op);
  uint16_t ch = stream.GetNext();

  // If the string begins with a '0' character, it must only consist
  // of it to be a legal array index.
  if (ch == '0') {
    *index = 0;
    return length == 1;
  }

  // Convert string to uint32 array index; character by character.
  int d = ch - '0';
  if (d < 0 || d > 9) return false;
  uint32_t result = d;
  while (stream.HasMore()) {
    d = stream.GetNext() - '0';
    if (d < 0 || d > 9) return false;
    // Check that the new result is below the 32 bit limit.
    if (result > 429496729U - ((d > 5) ? 1 : 0)) return false;
    result = (result * 10) + d;
  }

  *index = result;
  return true;
}


bool String::SlowAsArrayIndex(uint32_t* index) {
  if (length() <= kMaxCachedArrayIndexLength) {
    Hash();  // force computation of hash code
    uint32_t field = hash_field();
    if ((field & kIsNotArrayIndexMask) != 0) return false;
    // Isolate the array index form the full hash field.
    *index = (kArrayIndexHashMask & field) >> kHashShift;
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
    ASSERT(string->IsSeqTwoByteString());
    old_size = SeqTwoByteString::SizeFor(old_length);
    new_size = SeqTwoByteString::SizeFor(new_length);
  }

  int delta = old_size - new_size;
  string->set_length(new_length);

  Address start_of_string = string->address();
  ASSERT_OBJECT_ALIGNED(start_of_string);
  ASSERT_OBJECT_ALIGNED(start_of_string + new_size);

  Heap* heap = string->GetHeap();
  NewSpace* newspace = heap->new_space();
  if (newspace->Contains(start_of_string) &&
      newspace->top() == start_of_string + old_size) {
    // Last allocated object in new space.  Simply lower allocation top.
    *(newspace->allocation_top_address()) = start_of_string + new_size;
  } else {
    // Sizes are pointer size aligned, so that we can use filler objects
    // that are a multiple of pointer size.
    heap->CreateFillerObjectAt(start_of_string + new_size, delta);
  }
  if (Marking::IsBlack(Marking::MarkBitFrom(start_of_string))) {
    MemoryChunk::IncrementLiveBytesFromMutator(start_of_string, -delta);
  }


  if (new_length == 0) return heap->isolate()->factory()->empty_string();
  return string;
}


AllocationMemento* AllocationMemento::FindForJSObject(JSObject* object) {
  // Currently, AllocationMemento objects are only allocated immediately
  // after JSArrays in NewSpace, and detecting whether a JSArray has one
  // involves carefully checking the object immediately after the JSArray
  // (if there is one) to see if it's an AllocationMemento.
  if (FLAG_track_allocation_sites && object->GetHeap()->InNewSpace(object)) {
    ASSERT(object->GetHeap()->InToSpace(object));
    Address ptr_end = (reinterpret_cast<Address>(object) - kHeapObjectTag) +
        object->Size();
    if ((ptr_end + AllocationMemento::kSize) <=
        object->GetHeap()->NewSpaceTop()) {
      // There is room in newspace for allocation info. Do we have some?
      Map** possible_allocation_memento_map =
          reinterpret_cast<Map**>(ptr_end);
      if (*possible_allocation_memento_map ==
          object->GetHeap()->allocation_memento_map()) {
        AllocationMemento* memento = AllocationMemento::cast(
            reinterpret_cast<Object*>(ptr_end + kHeapObjectTag));
        return memento;
      }
    }
  }
  return NULL;
}


uint32_t StringHasher::MakeArrayIndexHash(uint32_t value, int length) {
  // For array indexes mix the length into the hash as an array index could
  // be zero.
  ASSERT(length > 0);
  ASSERT(length <= String::kMaxArrayIndexSize);
  ASSERT(TenToThe(String::kMaxCachedArrayIndexLength) <
         (1 << String::kArrayIndexValueBits));

  value <<= String::kHashShift;
  value |= length << String::kArrayIndexHashLengthShift;

  ASSERT((value & String::kIsNotArrayIndexMask) == 0);
  ASSERT((length > String::kMaxCachedArrayIndexLength) ||
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
    ASSERT(vector_length == 0 ||
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
  ASSERT(hasher.is_array_index_);
  while (remaining > 0) {
    unsigned consumed = 0;
    uint32_t c = unibrow::Utf8::ValueOf(stream, remaining, &consumed);
    ASSERT(consumed > 0 && consumed <= remaining);
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


MaybeObject* String::SubString(int start, int end, PretenureFlag pretenure) {
  Heap* heap = GetHeap();
  if (start == 0 && end == length()) return this;
  MaybeObject* result = heap->AllocateSubString(this, start, end, pretenure);
  return result;
}


void String::PrintOn(FILE* file) {
  int length = this->length();
  for (int i = 0; i < length; i++) {
    PrintF(file, "%c", Get(i));
  }
}


static void TrimEnumCache(Heap* heap, Map* map, DescriptorArray* descriptors) {
  int live_enum = map->EnumLength();
  if (live_enum == Map::kInvalidEnumCache) {
    live_enum = map->NumberOfDescribedProperties(OWN_DESCRIPTORS, DONT_ENUM);
  }
  if (live_enum == 0) return descriptors->ClearEnumCache();

  FixedArray* enum_cache = descriptors->GetEnumCache();

  int to_trim = enum_cache->length() - live_enum;
  if (to_trim <= 0) return;
  RightTrimFixedArray<FROM_GC>(heap, descriptors->GetEnumCache(), to_trim);

  if (!descriptors->HasEnumIndicesCache()) return;
  FixedArray* enum_indices_cache = descriptors->GetEnumIndicesCache();
  RightTrimFixedArray<FROM_GC>(heap, enum_indices_cache, to_trim);
}


static void TrimDescriptorArray(Heap* heap,
                                Map* map,
                                DescriptorArray* descriptors,
                                int number_of_own_descriptors) {
  int number_of_descriptors = descriptors->number_of_descriptors_storage();
  int to_trim = number_of_descriptors - number_of_own_descriptors;
  if (to_trim == 0) return;

  RightTrimFixedArray<FROM_GC>(
      heap, descriptors, to_trim * DescriptorArray::kDescriptorSize);
  descriptors->SetNumberOfDescriptors(number_of_own_descriptors);

  if (descriptors->HasEnumCache()) TrimEnumCache(heap, map, descriptors);
  descriptors->Sort();
}


// Clear a possible back pointer in case the transition leads to a dead map.
// Return true in case a back pointer has been cleared and false otherwise.
static bool ClearBackPointer(Heap* heap, Map* target) {
  if (Marking::MarkBitFrom(target).Get()) return false;
  target->SetBackPointer(heap->undefined_value(), SKIP_WRITE_BARRIER);
  return true;
}


// TODO(mstarzinger): This method should be moved into MarkCompactCollector,
// because it cannot be called from outside the GC and we already have methods
// depending on the transitions layout in the GC anyways.
void Map::ClearNonLiveTransitions(Heap* heap) {
  // If there are no transitions to be cleared, return.
  // TODO(verwaest) Should be an assert, otherwise back pointers are not
  // properly cleared.
  if (!HasTransitionArray()) return;

  TransitionArray* t = transitions();
  MarkCompactCollector* collector = heap->mark_compact_collector();

  int transition_index = 0;

  DescriptorArray* descriptors = instance_descriptors();
  bool descriptors_owner_died = false;

  // Compact all live descriptors to the left.
  for (int i = 0; i < t->number_of_transitions(); ++i) {
    Map* target = t->GetTarget(i);
    if (ClearBackPointer(heap, target)) {
      if (target->instance_descriptors() == descriptors) {
        descriptors_owner_died = true;
      }
    } else {
      if (i != transition_index) {
        Name* key = t->GetKey(i);
        t->SetKey(transition_index, key);
        Object** key_slot = t->GetKeySlot(transition_index);
        collector->RecordSlot(key_slot, key_slot, key);
        // Target slots do not need to be recorded since maps are not compacted.
        t->SetTarget(transition_index, t->GetTarget(i));
      }
      transition_index++;
    }
  }

  // If there are no transitions to be cleared, return.
  // TODO(verwaest) Should be an assert, otherwise back pointers are not
  // properly cleared.
  if (transition_index == t->number_of_transitions()) return;

  int number_of_own_descriptors = NumberOfOwnDescriptors();

  if (descriptors_owner_died) {
    if (number_of_own_descriptors > 0) {
      TrimDescriptorArray(heap, this, descriptors, number_of_own_descriptors);
      ASSERT(descriptors->number_of_descriptors() == number_of_own_descriptors);
    } else {
      ASSERT(descriptors == GetHeap()->empty_descriptor_array());
    }
  }

  int trim = t->number_of_transitions() - transition_index;
  if (trim > 0) {
    RightTrimFixedArray<FROM_GC>(heap, t, t->IsSimpleTransition()
        ? trim : trim * TransitionArray::kTransitionSize);
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
    first->is_observed() == second->is_observed() &&
    first->function_with_prototype() == second->function_with_prototype();
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


void JSFunction::JSFunctionIterateBody(int object_size, ObjectVisitor* v) {
  // Iterate over all fields in the body but take care in dealing with
  // the code entry.
  IteratePointers(v, kPropertiesOffset, kCodeEntryOffset);
  v->VisitCodeEntry(this->address() + kCodeEntryOffset);
  IteratePointers(v, kCodeEntryOffset + kPointerSize, object_size);
}


void JSFunction::MarkForLazyRecompilation() {
  ASSERT(is_compiled() || GetIsolate()->DebuggerHasBreakPoints());
  ASSERT(!IsOptimized());
  ASSERT(shared()->allows_lazy_compilation() ||
         code()->optimizable());
  ASSERT(!shared()->is_generator());
  set_code_no_write_barrier(
      GetIsolate()->builtins()->builtin(Builtins::kLazyRecompile));
  // No write barrier required, since the builtin is part of the root set.
}


void JSFunction::MarkForConcurrentRecompilation() {
  ASSERT(is_compiled() || GetIsolate()->DebuggerHasBreakPoints());
  ASSERT(!IsOptimized());
  ASSERT(shared()->allows_lazy_compilation() || code()->optimizable());
  ASSERT(!shared()->is_generator());
  ASSERT(FLAG_concurrent_recompilation);
  if (FLAG_trace_concurrent_recompilation) {
    PrintF("  ** Marking ");
    PrintName();
    PrintF(" for concurrent recompilation.\n");
  }
  set_code_no_write_barrier(
      GetIsolate()->builtins()->builtin(Builtins::kConcurrentRecompile));
  // No write barrier required, since the builtin is part of the root set.
}


void JSFunction::MarkInRecompileQueue() {
  // We can only arrive here via the concurrent-recompilation builtin.  If
  // break points were set, the code would point to the lazy-compile builtin.
  ASSERT(!GetIsolate()->DebuggerHasBreakPoints());
  ASSERT(IsMarkedForConcurrentRecompilation() && !IsOptimized());
  ASSERT(shared()->allows_lazy_compilation() || code()->optimizable());
  ASSERT(FLAG_concurrent_recompilation);
  if (FLAG_trace_concurrent_recompilation) {
    PrintF("  ** Queueing ");
    PrintName();
    PrintF(" for concurrent recompilation.\n");
  }
  set_code_no_write_barrier(
      GetIsolate()->builtins()->builtin(Builtins::kInRecompileQueue));
  // No write barrier required, since the builtin is part of the root set.
}


static bool CompileLazyHelper(CompilationInfo* info,
                              ClearExceptionFlag flag) {
  // Compile the source information to a code object.
  ASSERT(info->IsOptimizing() || !info->shared_info()->is_compiled());
  ASSERT(!info->isolate()->has_pending_exception());
  bool result = Compiler::CompileLazy(info);
  ASSERT(result != info->isolate()->has_pending_exception());
  if (!result && flag == CLEAR_EXCEPTION) {
    info->isolate()->clear_pending_exception();
  }
  return result;
}


bool SharedFunctionInfo::CompileLazy(Handle<SharedFunctionInfo> shared,
                                     ClearExceptionFlag flag) {
  ASSERT(shared->allows_lazy_compilation_without_context());
  CompilationInfoWithZone info(shared);
  return CompileLazyHelper(&info, flag);
}


void SharedFunctionInfo::AddToOptimizedCodeMap(
    Handle<SharedFunctionInfo> shared,
    Handle<Context> native_context,
    Handle<Code> code,
    Handle<FixedArray> literals) {
  CALL_HEAP_FUNCTION_VOID(
      shared->GetIsolate(),
      shared->AddToOptimizedCodeMap(*native_context, *code, *literals));
}


MaybeObject* SharedFunctionInfo::AddToOptimizedCodeMap(Context* native_context,
                                                       Code* code,
                                                       FixedArray* literals) {
  ASSERT(code->kind() == Code::OPTIMIZED_FUNCTION);
  ASSERT(native_context->IsNativeContext());
  STATIC_ASSERT(kEntryLength == 3);
  Heap* heap = GetHeap();
  FixedArray* new_code_map;
  Object* value = optimized_code_map();
  if (value->IsSmi()) {
    // No optimized code map.
    ASSERT_EQ(0, Smi::cast(value)->value());
    // Crate 3 entries per context {context, code, literals}.
    MaybeObject* maybe = heap->AllocateFixedArray(kInitialLength);
    if (!maybe->To(&new_code_map)) return maybe;
    new_code_map->set(kEntriesStart + 0, native_context);
    new_code_map->set(kEntriesStart + 1, code);
    new_code_map->set(kEntriesStart + 2, literals);
  } else {
    // Copy old map and append one new entry.
    FixedArray* old_code_map = FixedArray::cast(value);
    ASSERT_EQ(-1, SearchOptimizedCodeMap(native_context));
    int old_length = old_code_map->length();
    int new_length = old_length + kEntryLength;
    MaybeObject* maybe = old_code_map->CopySize(new_length);
    if (!maybe->To(&new_code_map)) return maybe;
    new_code_map->set(old_length + 0, native_context);
    new_code_map->set(old_length + 1, code);
    new_code_map->set(old_length + 2, literals);
    // Zap the old map for the sake of the heap verifier.
    if (Heap::ShouldZapGarbage()) {
      Object** data = old_code_map->data_start();
      MemsetPointer(data, heap->the_hole_value(), old_length);
    }
  }
#ifdef DEBUG
  for (int i = kEntriesStart; i < new_code_map->length(); i += kEntryLength) {
    ASSERT(new_code_map->get(i)->IsNativeContext());
    ASSERT(new_code_map->get(i + 1)->IsCode());
    ASSERT(Code::cast(new_code_map->get(i + 1))->kind() ==
           Code::OPTIMIZED_FUNCTION);
    ASSERT(new_code_map->get(i + 2)->IsFixedArray());
  }
#endif
  set_optimized_code_map(new_code_map);
  return new_code_map;
}


void SharedFunctionInfo::InstallFromOptimizedCodeMap(JSFunction* function,
                                                     int index) {
  ASSERT(index > kEntriesStart);
  FixedArray* code_map = FixedArray::cast(optimized_code_map());
  if (!bound()) {
    FixedArray* cached_literals = FixedArray::cast(code_map->get(index + 1));
    ASSERT(cached_literals != NULL);
    function->set_literals(cached_literals);
  }
  Code* code = Code::cast(code_map->get(index));
  ASSERT(code != NULL);
  ASSERT(function->context()->native_context() == code_map->get(index - 1));
  function->ReplaceCode(code);
}


void SharedFunctionInfo::ClearOptimizedCodeMap() {
  FixedArray* code_map = FixedArray::cast(optimized_code_map());

  // If the next map link slot is already used then the function was
  // enqueued with code flushing and we remove it now.
  if (!code_map->get(kNextMapIndex)->IsUndefined()) {
    CodeFlusher* flusher = GetHeap()->mark_compact_collector()->code_flusher();
    flusher->EvictOptimizedCodeMap(this);
  }

  ASSERT(code_map->get(kNextMapIndex)->IsUndefined());
  set_optimized_code_map(Smi::FromInt(0));
}


void SharedFunctionInfo::EvictFromOptimizedCodeMap(Code* optimized_code,
                                                   const char* reason) {
  if (optimized_code_map()->IsSmi()) return;

  int i;
  bool removed_entry = false;
  FixedArray* code_map = FixedArray::cast(optimized_code_map());
  for (i = kEntriesStart; i < code_map->length(); i += kEntryLength) {
    ASSERT(code_map->get(i)->IsNativeContext());
    if (Code::cast(code_map->get(i + 1)) == optimized_code) {
      if (FLAG_trace_opt) {
        PrintF("[evicting entry from optimizing code map (%s) for ", reason);
        ShortPrint();
        PrintF("]\n");
      }
      removed_entry = true;
      break;
    }
  }
  while (i < (code_map->length() - kEntryLength)) {
    code_map->set(i, code_map->get(i + kEntryLength));
    code_map->set(i + 1, code_map->get(i + 1 + kEntryLength));
    code_map->set(i + 2, code_map->get(i + 2 + kEntryLength));
    i += kEntryLength;
  }
  if (removed_entry) {
    // Always trim even when array is cleared because of heap verifier.
    RightTrimFixedArray<FROM_MUTATOR>(GetHeap(), code_map, kEntryLength);
    if (code_map->length() == kEntriesStart) {
      ClearOptimizedCodeMap();
    }
  }
}


void SharedFunctionInfo::TrimOptimizedCodeMap(int shrink_by) {
  FixedArray* code_map = FixedArray::cast(optimized_code_map());
  ASSERT(shrink_by % kEntryLength == 0);
  ASSERT(shrink_by <= code_map->length() - kEntriesStart);
  // Always trim even when array is cleared because of heap verifier.
  RightTrimFixedArray<FROM_GC>(GetHeap(), code_map, shrink_by);
  if (code_map->length() == kEntriesStart) {
    ClearOptimizedCodeMap();
  }
}


bool JSFunction::CompileLazy(Handle<JSFunction> function,
                             ClearExceptionFlag flag) {
  bool result = true;
  if (function->shared()->is_compiled()) {
    function->ReplaceCode(function->shared()->code());
  } else {
    ASSERT(function->shared()->allows_lazy_compilation());
    CompilationInfoWithZone info(function);
    result = CompileLazyHelper(&info, flag);
    ASSERT(!result || function->is_compiled());
  }
  return result;
}


Handle<Code> JSFunction::CompileOsr(Handle<JSFunction> function,
                                    BailoutId osr_ast_id,
                                    ClearExceptionFlag flag) {
  CompilationInfoWithZone info(function);
  info.SetOptimizing(osr_ast_id);
  if (CompileLazyHelper(&info, flag)) {
    // TODO(titzer): don't install the OSR code.
    // ASSERT(function->code() != *info.code());
    return info.code();
  } else {
    return Handle<Code>::null();
  }
}


bool JSFunction::CompileOptimized(Handle<JSFunction> function,
                                  ClearExceptionFlag flag) {
  CompilationInfoWithZone info(function);
  info.SetOptimizing(BailoutId::None());
  return CompileLazyHelper(&info, flag);
}


bool JSFunction::EnsureCompiled(Handle<JSFunction> function,
                                ClearExceptionFlag flag) {
  return function->is_compiled() || CompileLazy(function, flag);
}


bool JSFunction::IsInlineable() {
  if (IsBuiltin()) return false;
  SharedFunctionInfo* shared_info = shared();
  // Check that the function has a script associated with it.
  if (!shared_info->script()->IsScript()) return false;
  if (shared_info->optimization_disabled()) return false;
  Code* code = shared_info->code();
  if (code->kind() == Code::OPTIMIZED_FUNCTION) return true;
  // If we never ran this (unlikely) then lets try to optimize it.
  if (code->kind() != Code::FUNCTION) return true;
  return code->optimizable();
}


void JSObject::OptimizeAsPrototype(Handle<JSObject> object) {
  if (object->IsGlobalObject()) return;

  // Make sure prototypes are fast objects and their maps have the bit set
  // so they remain fast.
  if (!object->HasFastProperties()) {
    TransformToFastProperties(object, 0);
  }
}


static MUST_USE_RESULT MaybeObject* CacheInitialJSArrayMaps(
    Context* native_context, Map* initial_map) {
  // Replace all of the cached initial array maps in the native context with
  // the appropriate transitioned elements kind maps.
  Heap* heap = native_context->GetHeap();
  MaybeObject* maybe_maps =
      heap->AllocateFixedArrayWithHoles(kElementsKindCount, TENURED);
  FixedArray* maps;
  if (!maybe_maps->To(&maps)) return maybe_maps;

  Map* current_map = initial_map;
  ElementsKind kind = current_map->elements_kind();
  ASSERT(kind == GetInitialFastElementsKind());
  maps->set(kind, current_map);
  for (int i = GetSequenceIndexFromFastElementsKind(kind) + 1;
       i < kFastElementsKindCount; ++i) {
    Map* new_map;
    ElementsKind next_kind = GetFastElementsKindFromSequenceIndex(i);
    if (current_map->HasElementsTransition()) {
      new_map = current_map->elements_transition_map();
      ASSERT(new_map->elements_kind() == next_kind);
    } else {
      MaybeObject* maybe_new_map =
          current_map->CopyAsElementsKind(next_kind, INSERT_TRANSITION);
      if (!maybe_new_map->To(&new_map)) return maybe_new_map;
    }
    maps->set(next_kind, new_map);
    current_map = new_map;
  }
  native_context->set_js_array_maps(maps);
  return initial_map;
}


Handle<Object> CacheInitialJSArrayMaps(Handle<Context> native_context,
                                       Handle<Map> initial_map) {
  CALL_HEAP_FUNCTION(native_context->GetIsolate(),
                     CacheInitialJSArrayMaps(*native_context, *initial_map),
                     Object);
}


void JSFunction::SetInstancePrototype(Handle<JSFunction> function,
                                      Handle<Object> value) {
  ASSERT(value->IsJSReceiver());

  // First some logic for the map of the prototype to make sure it is in fast
  // mode.
  if (value->IsJSObject()) {
    JSObject::OptimizeAsPrototype(Handle<JSObject>::cast(value));
  }

  // Now some logic for the maps of the objects that are created by using this
  // function as a constructor.
  if (function->has_initial_map()) {
    // If the function has allocated the initial map replace it with a
    // copy containing the new prototype.  Also complete any in-object
    // slack tracking that is in progress at this point because it is
    // still tracking the old copy.
    if (function->shared()->IsInobjectSlackTrackingInProgress()) {
      function->shared()->CompleteInobjectSlackTracking();
    }
    Handle<Map> new_map = Map::Copy(handle(function->initial_map()));
    new_map->set_prototype(*value);

    // If the function is used as the global Array function, cache the
    // initial map (and transitioned versions) in the native context.
    Context* native_context = function->context()->native_context();
    Object* array_function = native_context->get(Context::ARRAY_FUNCTION_INDEX);
    if (array_function->IsJSFunction() &&
        *function == JSFunction::cast(array_function)) {
      CacheInitialJSArrayMaps(handle(native_context), new_map);
    }

    function->set_initial_map(*new_map);
  } else {
    // Put the value in the initial map field until an initial map is
    // needed.  At that point, a new initial map is created and the
    // prototype is put into the initial map where it belongs.
    function->set_prototype_or_initial_map(*value);
  }
  function->GetHeap()->ClearInstanceofCache();
}


void JSFunction::SetPrototype(Handle<JSFunction> function,
                              Handle<Object> value) {
  ASSERT(function->should_have_prototype());
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

    function->set_map(*new_map);
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


void JSFunction::RemovePrototype() {
  Context* native_context = context()->native_context();
  Map* no_prototype_map = shared()->is_classic_mode()
      ? native_context->function_without_prototype_map()
      : native_context->strict_mode_function_without_prototype_map();

  if (map() == no_prototype_map) return;

  ASSERT(map() == (shared()->is_classic_mode()
                   ? native_context->function_map()
                   : native_context->strict_mode_function_map()));

  set_map(no_prototype_map);
  set_prototype_or_initial_map(no_prototype_map->GetHeap()->the_hole_value());
}


void JSFunction::SetInstanceClassName(String* name) {
  shared()->set_instance_class_name(name);
}


void JSFunction::PrintName(FILE* out) {
  SmartArrayPointer<char> name = shared()->DebugName()->ToCString();
  PrintF(out, "%s", *name);
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
bool JSFunction::PassesFilter(const char* raw_filter) {
  if (*raw_filter == '*') return true;
  String* name = shared()->DebugName();
  Vector<const char> filter = CStrVector(raw_filter);
  if (filter.length() == 0) return name->length() == 0;
  if (filter[0] != '-' && name->IsUtf8EqualTo(filter)) return true;
  if (filter[0] == '-' &&
      !name->IsUtf8EqualTo(filter.SubVector(1, filter.length()))) {
    return true;
  }
  if (filter[filter.length() - 1] == '*' &&
      name->IsUtf8EqualTo(filter.SubVector(0, filter.length() - 1), true)) {
    return true;
  }
  return false;
}


MaybeObject* Oddball::Initialize(Heap* heap,
                                 const char* to_string,
                                 Object* to_number,
                                 byte kind) {
  String* internalized_to_string;
  { MaybeObject* maybe_string =
      heap->InternalizeUtf8String(
          CStrVector(to_string));
    if (!maybe_string->To(&internalized_to_string)) return maybe_string;
  }
  set_to_string(internalized_to_string);
  set_to_number(to_number);
  set_kind(kind);
  return this;
}


String* SharedFunctionInfo::DebugName() {
  Object* n = name();
  if (!n->IsString() || String::cast(n)->length() == 0) return inferred_name();
  return String::cast(n);
}


bool SharedFunctionInfo::HasSourceCode() {
  return !script()->IsUndefined() &&
         !reinterpret_cast<Script*>(script())->source()->IsUndefined();
}


Handle<Object> SharedFunctionInfo::GetSourceCode() {
  if (!HasSourceCode()) return GetIsolate()->factory()->undefined_value();
  Handle<String> source(String::cast(Script::cast(script())->source()));
  return SubString(source, start_position(), end_position());
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


// Support function for printing the source code to a StringStream
// without any allocation in the heap.
void SharedFunctionInfo::SourceCodePrint(StringStream* accumulator,
                                         int max_length) {
  // For some native functions there is no source.
  if (!HasSourceCode()) {
    accumulator->Add("<No Source>");
    return;
  }

  // Get the source for the script which this function came from.
  // Don't use String::cast because we don't want more assertion errors while
  // we are already creating a stack dump.
  String* script_source =
      reinterpret_cast<String*>(Script::cast(script())->source());

  if (!script_source->LooksValid()) {
    accumulator->Add("<Invalid Source>");
    return;
  }

  if (!is_toplevel()) {
    accumulator->Add("function ");
    Object* name = this->name();
    if (name->IsString() && String::cast(name)->length() > 0) {
      accumulator->PrintName(name);
    }
  }

  int len = end_position() - start_position();
  if (len <= max_length || max_length < 0) {
    accumulator->Put(script_source, start_position(), end_position());
  } else {
    accumulator->Put(script_source,
                     start_position(),
                     start_position() + max_length);
    accumulator->Add("...\n");
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
  ASSERT(!has_deoptimization_support());
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
  ASSERT(has_deoptimization_support());
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
  ASSERT(code()->kind() == Code::FUNCTION || code()->kind() == Code::BUILTIN);
  if (code()->kind() == Code::FUNCTION) {
    code()->set_optimizable(false);
  }
  PROFILE(GetIsolate(),
      LogExistingFunction(Handle<SharedFunctionInfo>(this),
                          Handle<Code>(code())));
  if (FLAG_trace_opt) {
    PrintF("[disabled optimization for ");
    ShortPrint();
    PrintF(", reason: %s]\n", GetBailoutReason(reason));
  }
}


bool SharedFunctionInfo::VerifyBailoutId(BailoutId id) {
  ASSERT(!id.IsNone());
  Code* unoptimized = code();
  DeoptimizationOutputData* data =
      DeoptimizationOutputData::cast(unoptimized->deoptimization_data());
  unsigned ignore = Deoptimizer::GetOutputInfo(data, id, this);
  USE(ignore);
  return true;  // Return true if there was no ASSERT.
}


void SharedFunctionInfo::StartInobjectSlackTracking(Map* map) {
  ASSERT(!IsInobjectSlackTrackingInProgress());

  if (!FLAG_clever_optimizations) return;

  // Only initiate the tracking the first time.
  if (live_objects_may_exist()) return;
  set_live_objects_may_exist(true);

  // No tracking during the snapshot construction phase.
  if (Serializer::enabled()) return;

  if (map->unused_property_fields() == 0) return;

  // Nonzero counter is a leftover from the previous attempt interrupted
  // by GC, keep it.
  if (construction_count() == 0) {
    set_construction_count(kGenerousAllocationCount);
  }
  set_initial_map(map);
  Builtins* builtins = map->GetHeap()->isolate()->builtins();
  ASSERT_EQ(builtins->builtin(Builtins::kJSConstructStubGeneric),
            construct_stub());
  set_construct_stub(builtins->builtin(Builtins::kJSConstructStubCountdown));
}


// Called from GC, hence reinterpret_cast and unchecked accessors.
void SharedFunctionInfo::DetachInitialMap() {
  Map* map = reinterpret_cast<Map*>(initial_map());

  // Make the map remember to restore the link if it survives the GC.
  map->set_bit_field2(
      map->bit_field2() | (1 << Map::kAttachedToSharedFunctionInfo));

  // Undo state changes made by StartInobjectTracking (except the
  // construction_count). This way if the initial map does not survive the GC
  // then StartInobjectTracking will be called again the next time the
  // constructor is called. The countdown will continue and (possibly after
  // several more GCs) CompleteInobjectSlackTracking will eventually be called.
  Heap* heap = map->GetHeap();
  set_initial_map(heap->undefined_value());
  Builtins* builtins = heap->isolate()->builtins();
  ASSERT_EQ(builtins->builtin(Builtins::kJSConstructStubCountdown),
            *RawField(this, kConstructStubOffset));
  set_construct_stub(builtins->builtin(Builtins::kJSConstructStubGeneric));
  // It is safe to clear the flag: it will be set again if the map is live.
  set_live_objects_may_exist(false);
}


// Called from GC, hence reinterpret_cast and unchecked accessors.
void SharedFunctionInfo::AttachInitialMap(Map* map) {
  map->set_bit_field2(
      map->bit_field2() & ~(1 << Map::kAttachedToSharedFunctionInfo));

  // Resume inobject slack tracking.
  set_initial_map(map);
  Builtins* builtins = map->GetHeap()->isolate()->builtins();
  ASSERT_EQ(builtins->builtin(Builtins::kJSConstructStubGeneric),
            *RawField(this, kConstructStubOffset));
  set_construct_stub(builtins->builtin(Builtins::kJSConstructStubCountdown));
  // The map survived the gc, so there may be objects referencing it.
  set_live_objects_may_exist(true);
}


void SharedFunctionInfo::ResetForNewContext(int new_ic_age) {
  code()->ClearInlineCaches();
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


void SharedFunctionInfo::CompleteInobjectSlackTracking() {
  ASSERT(live_objects_may_exist() && IsInobjectSlackTrackingInProgress());
  Map* map = Map::cast(initial_map());

  Heap* heap = map->GetHeap();
  set_initial_map(heap->undefined_value());
  Builtins* builtins = heap->isolate()->builtins();
  ASSERT_EQ(builtins->builtin(Builtins::kJSConstructStubCountdown),
            construct_stub());
  set_construct_stub(builtins->builtin(Builtins::kJSConstructStubGeneric));

  int slack = map->unused_property_fields();
  map->TraverseTransitionTree(&GetMinInobjectSlack, &slack);
  if (slack != 0) {
    // Resize the initial map and all maps in its transition tree.
    map->TraverseTransitionTree(&ShrinkInstanceSize, &slack);

    // Give the correct expected_nof_properties to initial maps created later.
    ASSERT(expected_nof_properties() >= slack);
    set_expected_nof_properties(expected_nof_properties() - slack);
  }
}


int SharedFunctionInfo::SearchOptimizedCodeMap(Context* native_context) {
  ASSERT(native_context->IsNativeContext());
  if (!FLAG_cache_optimized_code) return -1;
  Object* value = optimized_code_map();
  if (!value->IsSmi()) {
    FixedArray* optimized_code_map = FixedArray::cast(value);
    int length = optimized_code_map->length();
    for (int i = kEntriesStart; i < length; i += kEntryLength) {
      if (optimized_code_map->get(i) == native_context) {
        return i + 1;
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
  ASSERT(RelocInfo::IsCodeTarget(rinfo->rmode()));
  Object* target = Code::GetCodeFromTargetAddress(rinfo->target_address());
  Object* old_target = target;
  VisitPointer(&target);
  CHECK_EQ(target, old_target);  // VisitPointer doesn't change Code* *target.
}


void ObjectVisitor::VisitCodeAgeSequence(RelocInfo* rinfo) {
  ASSERT(RelocInfo::IsCodeAgeSequence(rinfo->rmode()));
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
  ASSERT(rinfo->rmode() == RelocInfo::CELL);
  Object* cell = rinfo->target_cell();
  Object* old_cell = cell;
  VisitPointer(&cell);
  if (cell != old_cell) {
    rinfo->set_target_cell(reinterpret_cast<Cell*>(cell));
  }
}


void ObjectVisitor::VisitDebugTarget(RelocInfo* rinfo) {
  ASSERT((RelocInfo::IsJSReturn(rinfo->rmode()) &&
          rinfo->IsPatchedReturnSequence()) ||
         (RelocInfo::IsDebugBreakSlot(rinfo->rmode()) &&
          rinfo->IsPatchedDebugBreakSlotSequence()));
  Object* target = Code::GetCodeFromTargetAddress(rinfo->call_address());
  Object* old_target = target;
  VisitPointer(&target);
  CHECK_EQ(target, old_target);  // VisitPointer doesn't change Code* *target.
}


void ObjectVisitor::VisitEmbeddedPointer(RelocInfo* rinfo) {
  ASSERT(rinfo->rmode() == RelocInfo::EMBEDDED_OBJECT);
  VisitPointer(rinfo->target_object_address());
}


void ObjectVisitor::VisitExternalReference(RelocInfo* rinfo) {
  Address* p = rinfo->target_reference_address();
  VisitExternalReferences(p, p + 1);
}


void Code::InvalidateRelocation() {
  set_relocation_info(GetHeap()->empty_byte_array());
}


void Code::Relocate(intptr_t delta) {
  for (RelocIterator it(this, RelocInfo::kApplyMask); !it.done(); it.next()) {
    it.rinfo()->apply(delta);
  }
  CPU::FlushICache(instruction_start(), instruction_size());
}


void Code::CopyFrom(const CodeDesc& desc) {
  ASSERT(Marking::Color(this) == Marking::WHITE_OBJECT);

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
      it.rinfo()->set_target_object(*p, SKIP_WRITE_BARRIER);
    } else if (mode == RelocInfo::CELL) {
      Handle<Cell> cell  = it.rinfo()->target_cell_handle();
      it.rinfo()->set_target_cell(*cell, SKIP_WRITE_BARRIER);
    } else if (RelocInfo::IsCodeTarget(mode)) {
      // rewrite code handles in inline cache targets to direct
      // pointers to the first instruction in the code object
      Handle<Object> p = it.rinfo()->target_object_handle(origin);
      Code* code = Code::cast(*p);
      it.rinfo()->set_target_address(code->instruction_start(),
                                     SKIP_WRITE_BARRIER);
    } else if (RelocInfo::IsRuntimeEntry(mode)) {
      Address p = it.rinfo()->target_runtime_entry(origin);
      it.rinfo()->set_target_runtime_entry(p, SKIP_WRITE_BARRIER);
    } else {
      it.rinfo()->apply(delta);
    }
  }
  CPU::FlushICache(instruction_start(), instruction_size());
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
  return table.FindEntry(pc);
}


Object* Code::FindNthObject(int n, Map* match_map) {
  ASSERT(is_inline_cache_stub());
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


Map* Code::FindFirstMap() {
  Object* result = FindNthObject(1, GetHeap()->meta_map());
  return (result != NULL) ? Map::cast(result) : NULL;
}


void Code::ReplaceNthObject(int n,
                            Map* match_map,
                            Object* replace_with) {
  ASSERT(is_inline_cache_stub());
  DisallowHeapAllocation no_allocation;
  int mask = RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT);
  for (RelocIterator it(this, mask); !it.done(); it.next()) {
    RelocInfo* info = it.rinfo();
    Object* object = info->target_object();
    if (object->IsHeapObject()) {
      if (HeapObject::cast(object)->map() == match_map) {
        if (--n == 0) {
          info->set_target_object(replace_with);
          return;
        }
      }
    }
  }
  UNREACHABLE();
}


void Code::FindAllMaps(MapHandleList* maps) {
  ASSERT(is_inline_cache_stub());
  DisallowHeapAllocation no_allocation;
  int mask = RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT);
  for (RelocIterator it(this, mask); !it.done(); it.next()) {
    RelocInfo* info = it.rinfo();
    Object* object = info->target_object();
    if (object->IsMap()) maps->Add(Handle<Map>(Map::cast(object)));
  }
}


void Code::ReplaceFirstMap(Map* replace_with) {
  ReplaceNthObject(1, GetHeap()->meta_map(), replace_with);
}


Code* Code::FindFirstCode() {
  ASSERT(is_inline_cache_stub());
  DisallowHeapAllocation no_allocation;
  int mask = RelocInfo::ModeMask(RelocInfo::CODE_TARGET);
  for (RelocIterator it(this, mask); !it.done(); it.next()) {
    RelocInfo* info = it.rinfo();
    return Code::GetCodeFromTargetAddress(info->target_address());
  }
  return NULL;
}


void Code::FindAllCode(CodeHandleList* code_list, int length) {
  ASSERT(is_inline_cache_stub());
  DisallowHeapAllocation no_allocation;
  int mask = RelocInfo::ModeMask(RelocInfo::CODE_TARGET);
  int i = 0;
  for (RelocIterator it(this, mask); !it.done(); it.next()) {
    if (i++ == length) return;
    RelocInfo* info = it.rinfo();
    Code* code = Code::GetCodeFromTargetAddress(info->target_address());
    ASSERT(code->kind() == Code::STUB);
    code_list->Add(Handle<Code>(code));
  }
  UNREACHABLE();
}


Name* Code::FindFirstName() {
  ASSERT(is_inline_cache_stub());
  DisallowHeapAllocation no_allocation;
  int mask = RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT);
  for (RelocIterator it(this, mask); !it.done(); it.next()) {
    RelocInfo* info = it.rinfo();
    Object* object = info->target_object();
    if (object->IsName()) return Name::cast(object);
  }
  return NULL;
}


void Code::ReplaceNthCell(int n, Cell* replace_with) {
  ASSERT(is_inline_cache_stub());
  DisallowHeapAllocation no_allocation;
  int mask = RelocInfo::ModeMask(RelocInfo::CELL);
  for (RelocIterator it(this, mask); !it.done(); it.next()) {
    RelocInfo* info = it.rinfo();
    if (--n == 0) {
      info->set_target_cell(replace_with);
      return;
    }
  }
  UNREACHABLE();
}


void Code::ClearInlineCaches() {
  int mask = RelocInfo::ModeMask(RelocInfo::CODE_TARGET) |
             RelocInfo::ModeMask(RelocInfo::CONSTRUCT_CALL) |
             RelocInfo::ModeMask(RelocInfo::CODE_TARGET_WITH_ID) |
             RelocInfo::ModeMask(RelocInfo::CODE_TARGET_CONTEXT);
  for (RelocIterator it(this, mask); !it.done(); it.next()) {
    RelocInfo* info = it.rinfo();
    Code* target(Code::GetCodeFromTargetAddress(info->target_address()));
    if (target->is_inline_cache_stub()) {
      IC::Clear(this->GetIsolate(), info->pc());
    }
  }
}


void Code::ClearTypeFeedbackCells(Heap* heap) {
  if (kind() != FUNCTION) return;
  Object* raw_info = type_feedback_info();
  if (raw_info->IsTypeFeedbackInfo()) {
    TypeFeedbackCells* type_feedback_cells =
        TypeFeedbackInfo::cast(raw_info)->type_feedback_cells();
    for (int i = 0; i < type_feedback_cells->CellCount(); i++) {
      Cell* cell = type_feedback_cells->GetCell(i);
      // Don't clear AllocationSites
      Object* value = cell->value();
      if (value == NULL || !value->IsAllocationSite()) {
        cell->set_value(TypeFeedbackCells::RawUninitializedSentinel(heap));
      }
    }
  }
}


BailoutId Code::TranslatePcOffsetToAstId(uint32_t pc_offset) {
  DisallowHeapAllocation no_gc;
  ASSERT(kind() == FUNCTION);
  for (FullCodeGenerator::BackEdgeTableIterator it(this, &no_gc);
       !it.Done();
       it.Next()) {
    if (it.pc_offset() == pc_offset) return it.ast_id();
  }
  return BailoutId::None();
}


bool Code::allowed_in_shared_map_code_cache() {
  return is_keyed_load_stub() || is_keyed_store_stub() ||
      (is_compare_ic_stub() &&
       ICCompareStub::CompareState(stub_info()) == CompareIC::KNOWN_OBJECT);
}


void Code::MakeCodeAgeSequenceYoung(byte* sequence) {
  PatchPlatformCodeAge(sequence, kNoAge, NO_MARKING_PARITY);
}


void Code::MakeOlder(MarkingParity current_parity) {
  byte* sequence = FindCodeAgeSequence();
  if (sequence != NULL) {
    Age age;
    MarkingParity code_parity;
    GetCodeAgeAndParity(sequence, &age, &code_parity);
    if (age != kLastCodeAge && code_parity != current_parity) {
      PatchPlatformCodeAge(sequence, static_cast<Age>(age + 1),
                           current_parity);
    }
  }
}


bool Code::IsOld() {
  byte* sequence = FindCodeAgeSequence();
  if (sequence == NULL) return false;
  Age age;
  MarkingParity parity;
  GetCodeAgeAndParity(sequence, &age, &parity);
  return age >= kSexagenarianCodeAge;
}


byte* Code::FindCodeAgeSequence() {
  return FLAG_age_code &&
      prologue_offset() != kPrologueOffsetNotSet &&
      (kind() == OPTIMIZED_FUNCTION ||
       (kind() == FUNCTION && !has_debug_break_slots()))
      ? instruction_start() + prologue_offset()
      : NULL;
}


int Code::GetAge() {
  byte* sequence = FindCodeAgeSequence();
  if (sequence == NULL) {
    return Code::kNoAge;
  }
  Age age;
  MarkingParity parity;
  GetCodeAgeAndParity(sequence, &age, &parity);
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
  UNREACHABLE();
}


Code* Code::GetCodeAgeStub(Age age, MarkingParity parity) {
  Isolate* isolate = Isolate::Current();
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
    default:
      UNREACHABLE();
      break;
  }
  return NULL;
}


void Code::PrintDeoptLocation(int bailout_id) {
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
              GetIsolate(), info->target_address(), Deoptimizer::SOFT))) {
        CHECK(RelocInfo::IsRuntimeEntry(info->rmode()));
        PrintF("            %s\n", last_comment);
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

void DeoptimizationInputData::DeoptimizationInputDataPrint(FILE* out) {
  disasm::NameConverter converter;
  int deopt_count = DeoptCount();
  PrintF(out, "Deoptimization Input Data (deopt points = %d)\n", deopt_count);
  if (0 == deopt_count) return;

  PrintF(out, "%6s  %6s  %6s %6s %12s\n", "index", "ast id", "argc", "pc",
         FLAG_print_code_verbose ? "commands" : "");
  for (int i = 0; i < deopt_count; i++) {
    PrintF(out, "%6d  %6d  %6d %6d",
           i,
           AstId(i).ToInt(),
           ArgumentsStackHeight(i)->value(),
           Pc(i)->value());

    if (!FLAG_print_code_verbose) {
      PrintF(out, "\n");
      continue;
    }
    // Print details of the frame translation.
    int translation_index = TranslationIndex(i)->value();
    TranslationIterator iterator(TranslationByteArray(), translation_index);
    Translation::Opcode opcode =
        static_cast<Translation::Opcode>(iterator.Next());
    ASSERT(Translation::BEGIN == opcode);
    int frame_count = iterator.Next();
    int jsframe_count = iterator.Next();
    PrintF(out, "  %s {frame count=%d, js frame count=%d}\n",
           Translation::StringFor(opcode),
           frame_count,
           jsframe_count);

    while (iterator.HasNext() &&
           Translation::BEGIN !=
           (opcode = static_cast<Translation::Opcode>(iterator.Next()))) {
      PrintF(out, "%24s    %s ", "", Translation::StringFor(opcode));

      switch (opcode) {
        case Translation::BEGIN:
          UNREACHABLE();
          break;

        case Translation::JS_FRAME: {
          int ast_id = iterator.Next();
          int function_id = iterator.Next();
          unsigned height = iterator.Next();
          PrintF(out, "{ast_id=%d, function=", ast_id);
          if (function_id != Translation::kSelfLiteralId) {
            Object* function = LiteralArray()->get(function_id);
            JSFunction::cast(function)->PrintName(out);
          } else {
            PrintF(out, "<self>");
          }
          PrintF(out, ", height=%u}", height);
          break;
        }

        case Translation::COMPILED_STUB_FRAME: {
          Code::Kind stub_kind = static_cast<Code::Kind>(iterator.Next());
          PrintF(out, "{kind=%d}", stub_kind);
          break;
        }

        case Translation::ARGUMENTS_ADAPTOR_FRAME:
        case Translation::CONSTRUCT_STUB_FRAME: {
          int function_id = iterator.Next();
          JSFunction* function =
              JSFunction::cast(LiteralArray()->get(function_id));
          unsigned height = iterator.Next();
          PrintF(out, "{function=");
          function->PrintName(out);
          PrintF(out, ", height=%u}", height);
          break;
        }

        case Translation::GETTER_STUB_FRAME:
        case Translation::SETTER_STUB_FRAME: {
          int function_id = iterator.Next();
          JSFunction* function =
              JSFunction::cast(LiteralArray()->get(function_id));
          PrintF(out, "{function=");
          function->PrintName(out);
          PrintF(out, "}");
          break;
        }

        case Translation::REGISTER: {
          int reg_code = iterator.Next();
            PrintF(out, "{input=%s}", converter.NameOfCPURegister(reg_code));
          break;
        }

        case Translation::INT32_REGISTER: {
          int reg_code = iterator.Next();
          PrintF(out, "{input=%s}", converter.NameOfCPURegister(reg_code));
          break;
        }

        case Translation::UINT32_REGISTER: {
          int reg_code = iterator.Next();
          PrintF(out, "{input=%s (unsigned)}",
                 converter.NameOfCPURegister(reg_code));
          break;
        }

        case Translation::DOUBLE_REGISTER: {
          int reg_code = iterator.Next();
          PrintF(out, "{input=%s}",
                 DoubleRegister::AllocationIndexToString(reg_code));
          break;
        }

        case Translation::STACK_SLOT: {
          int input_slot_index = iterator.Next();
          PrintF(out, "{input=%d}", input_slot_index);
          break;
        }

        case Translation::INT32_STACK_SLOT: {
          int input_slot_index = iterator.Next();
          PrintF(out, "{input=%d}", input_slot_index);
          break;
        }

        case Translation::UINT32_STACK_SLOT: {
          int input_slot_index = iterator.Next();
          PrintF(out, "{input=%d (unsigned)}", input_slot_index);
          break;
        }

        case Translation::DOUBLE_STACK_SLOT: {
          int input_slot_index = iterator.Next();
          PrintF(out, "{input=%d}", input_slot_index);
          break;
        }

        case Translation::LITERAL: {
          unsigned literal_index = iterator.Next();
          PrintF(out, "{literal_id=%u}", literal_index);
          break;
        }

        case Translation::DUPLICATED_OBJECT: {
          int object_index = iterator.Next();
          PrintF(out, "{object_index=%d}", object_index);
          break;
        }

        case Translation::ARGUMENTS_OBJECT:
        case Translation::CAPTURED_OBJECT: {
          int args_length = iterator.Next();
          PrintF(out, "{length=%d}", args_length);
          break;
        }
      }
      PrintF(out, "\n");
    }
  }
}


void DeoptimizationOutputData::DeoptimizationOutputDataPrint(FILE* out) {
  PrintF(out, "Deoptimization Output Data (deopt points = %d)\n",
         this->DeoptPoints());
  if (this->DeoptPoints() == 0) return;

  PrintF("%6s  %8s  %s\n", "ast id", "pc", "state");
  for (int i = 0; i < this->DeoptPoints(); i++) {
    int pc_and_state = this->PcAndState(i)->value();
    PrintF("%6d  %8d  %s\n",
           this->AstId(i).ToInt(),
           FullCodeGenerator::PcField::decode(pc_and_state),
           FullCodeGenerator::State2String(
               FullCodeGenerator::StateField::decode(pc_and_state)));
  }
}


const char* Code::ICState2String(InlineCacheState state) {
  switch (state) {
    case UNINITIALIZED: return "UNINITIALIZED";
    case PREMONOMORPHIC: return "PREMONOMORPHIC";
    case MONOMORPHIC: return "MONOMORPHIC";
    case MONOMORPHIC_PROTOTYPE_FAILURE: return "MONOMORPHIC_PROTOTYPE_FAILURE";
    case POLYMORPHIC: return "POLYMORPHIC";
    case MEGAMORPHIC: return "MEGAMORPHIC";
    case GENERIC: return "GENERIC";
    case DEBUG_STUB: return "DEBUG_STUB";
  }
  UNREACHABLE();
  return NULL;
}


const char* Code::StubType2String(StubType type) {
  switch (type) {
    case NORMAL: return "NORMAL";
    case FIELD: return "FIELD";
    case CONSTANT: return "CONSTANT";
    case CALLBACKS: return "CALLBACKS";
    case INTERCEPTOR: return "INTERCEPTOR";
    case MAP_TRANSITION: return "MAP_TRANSITION";
    case NONEXISTENT: return "NONEXISTENT";
  }
  UNREACHABLE();  // keep the compiler happy
  return NULL;
}


void Code::PrintExtraICState(FILE* out, Kind kind, ExtraICState extra) {
  PrintF(out, "extra_ic_state = ");
  const char* name = NULL;
  switch (kind) {
    case CALL_IC:
      if (extra == STRING_INDEX_OUT_OF_BOUNDS) {
        name = "STRING_INDEX_OUT_OF_BOUNDS";
      }
      break;
    case STORE_IC:
    case KEYED_STORE_IC:
      if (extra == kStrictMode) {
        name = "STRICT";
      }
      break;
    default:
      break;
  }
  if (name != NULL) {
    PrintF(out, "%s\n", name);
  } else {
    PrintF(out, "%d\n", extra);
  }
}


void Code::Disassemble(const char* name, FILE* out) {
  PrintF(out, "kind = %s\n", Kind2String(kind()));
  if (is_inline_cache_stub()) {
    PrintF(out, "ic_state = %s\n", ICState2String(ic_state()));
    PrintExtraICState(out, kind(), needs_extended_extra_ic_state(kind()) ?
        extended_extra_ic_state() : extra_ic_state());
    if (ic_state() == MONOMORPHIC) {
      PrintF(out, "type = %s\n", StubType2String(type()));
    }
    if (is_call_stub() || is_keyed_call_stub()) {
      PrintF(out, "argc = %d\n", arguments_count());
    }
    if (is_compare_ic_stub()) {
      ASSERT(major_key() == CodeStub::CompareIC);
      CompareIC::State left_state, right_state, handler_state;
      Token::Value op;
      ICCompareStub::DecodeMinorKey(stub_info(), &left_state, &right_state,
                                    &handler_state, &op);
      PrintF(out, "compare_state = %s*%s -> %s\n",
             CompareIC::GetStateName(left_state),
             CompareIC::GetStateName(right_state),
             CompareIC::GetStateName(handler_state));
      PrintF(out, "compare_operation = %s\n", Token::Name(op));
    }
  }
  if ((name != NULL) && (name[0] != '\0')) {
    PrintF(out, "name = %s\n", name);
  }
  if (kind() == OPTIMIZED_FUNCTION) {
    PrintF(out, "stack_slots = %d\n", stack_slots());
  }

  PrintF(out, "Instructions (size = %d)\n", instruction_size());
  Disassembler::Decode(out, this);
  PrintF(out, "\n");

  if (kind() == FUNCTION) {
    DeoptimizationOutputData* data =
        DeoptimizationOutputData::cast(this->deoptimization_data());
    data->DeoptimizationOutputDataPrint(out);
  } else if (kind() == OPTIMIZED_FUNCTION) {
    DeoptimizationInputData* data =
        DeoptimizationInputData::cast(this->deoptimization_data());
    data->DeoptimizationInputDataPrint(out);
  }
  PrintF("\n");

  if (is_crankshafted()) {
    SafepointTable table(this);
    PrintF(out, "Safepoints (size = %u)\n", table.size());
    for (unsigned i = 0; i < table.length(); i++) {
      unsigned pc_offset = table.GetPcOffset(i);
      PrintF(out, "%p  %4d  ", (instruction_start() + pc_offset), pc_offset);
      table.PrintEntry(i);
      PrintF(out, " (sp -> fp)");
      SafepointEntry entry = table.GetEntry(i);
      if (entry.deoptimization_index() != Safepoint::kNoDeoptimizationIndex) {
        PrintF(out, "  %6d", entry.deoptimization_index());
      } else {
        PrintF(out, "  <none>");
      }
      if (entry.argument_count() > 0) {
        PrintF(out, " argc: %d", entry.argument_count());
      }
      PrintF(out, "\n");
    }
    PrintF(out, "\n");
  } else if (kind() == FUNCTION) {
    unsigned offset = back_edge_table_offset();
    // If there is no back edge table, the "table start" will be at or after
    // (due to alignment) the end of the instruction stream.
    if (static_cast<int>(offset) < instruction_size()) {
      DisallowHeapAllocation no_gc;
      FullCodeGenerator::BackEdgeTableIterator back_edges(this, &no_gc);

      PrintF(out, "Back edges (size = %u)\n", back_edges.table_length());
      PrintF(out, "ast_id  pc_offset  loop_depth\n");

      for ( ; !back_edges.Done(); back_edges.Next()) {
        PrintF(out, "%6d  %9u  %10u\n", back_edges.ast_id().ToInt(),
                                        back_edges.pc_offset(),
                                        back_edges.loop_depth());
      }

      PrintF(out, "\n");
    }
#ifdef OBJECT_PRINT
    if (!type_feedback_info()->IsUndefined()) {
      TypeFeedbackInfo::cast(type_feedback_info())->TypeFeedbackInfoPrint(out);
      PrintF(out, "\n");
    }
#endif
  }

  PrintF("RelocInfo (size = %d)\n", relocation_size());
  for (RelocIterator it(this); !it.done(); it.next()) {
    it.rinfo()->Print(GetIsolate(), out);
  }
  PrintF(out, "\n");
}
#endif  // ENABLE_DISASSEMBLER


MaybeObject* JSObject::SetFastElementsCapacityAndLength(
    int capacity,
    int length,
    SetFastElementsCapacitySmiMode smi_mode) {
  Heap* heap = GetHeap();
  // We should never end in here with a pixel or external array.
  ASSERT(!HasExternalArrayElements());
  ASSERT(!map()->is_observed());

  // Allocate a new fast elements backing store.
  FixedArray* new_elements;
  MaybeObject* maybe = heap->AllocateUninitializedFixedArray(capacity);
  if (!maybe->To(&new_elements)) return maybe;

  ElementsKind elements_kind = GetElementsKind();
  ElementsKind new_elements_kind;
  // The resized array has FAST_*_SMI_ELEMENTS if the capacity mode forces it,
  // or if it's allowed and the old elements array contained only SMIs.
  bool has_fast_smi_elements =
      (smi_mode == kForceSmiElements) ||
      ((smi_mode == kAllowSmiElements) && HasFastSmiElements());
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
  FixedArrayBase* old_elements = elements();
  ElementsAccessor* accessor = ElementsAccessor::ForKind(new_elements_kind);
  MaybeObject* maybe_obj =
      accessor->CopyElements(this, new_elements, elements_kind);
  if (maybe_obj->IsFailure()) return maybe_obj;

  if (elements_kind != NON_STRICT_ARGUMENTS_ELEMENTS) {
    Map* new_map = map();
    if (new_elements_kind != elements_kind) {
      MaybeObject* maybe =
          GetElementsTransitionMap(GetIsolate(), new_elements_kind);
      if (!maybe->To(&new_map)) return maybe;
    }
    ValidateElements();
    set_map_and_elements(new_map, new_elements);
  } else {
    FixedArray* parameter_map = FixedArray::cast(old_elements);
    parameter_map->set(1, new_elements);
  }

  if (FLAG_trace_elements_transitions) {
    PrintElementsTransition(stdout, elements_kind, old_elements,
                            GetElementsKind(), new_elements);
  }

  if (IsJSArray()) {
    JSArray::cast(this)->set_length(Smi::FromInt(length));
  }
  return new_elements;
}


MaybeObject* JSObject::SetFastDoubleElementsCapacityAndLength(
    int capacity,
    int length) {
  Heap* heap = GetHeap();
  // We should never end in here with a pixel or external array.
  ASSERT(!HasExternalArrayElements());
  ASSERT(!map()->is_observed());

  FixedArrayBase* elems;
  { MaybeObject* maybe_obj =
        heap->AllocateUninitializedFixedDoubleArray(capacity);
    if (!maybe_obj->To(&elems)) return maybe_obj;
  }

  ElementsKind elements_kind = GetElementsKind();
  ElementsKind new_elements_kind = elements_kind;
  if (IsHoleyElementsKind(elements_kind)) {
    new_elements_kind = FAST_HOLEY_DOUBLE_ELEMENTS;
  } else {
    new_elements_kind = FAST_DOUBLE_ELEMENTS;
  }

  Map* new_map;
  { MaybeObject* maybe_obj =
        GetElementsTransitionMap(heap->isolate(), new_elements_kind);
    if (!maybe_obj->To(&new_map)) return maybe_obj;
  }

  FixedArrayBase* old_elements = elements();
  ElementsAccessor* accessor = ElementsAccessor::ForKind(FAST_DOUBLE_ELEMENTS);
  { MaybeObject* maybe_obj =
        accessor->CopyElements(this, elems, elements_kind);
    if (maybe_obj->IsFailure()) return maybe_obj;
  }
  if (elements_kind != NON_STRICT_ARGUMENTS_ELEMENTS) {
    ValidateElements();
    set_map_and_elements(new_map, elems);
  } else {
    FixedArray* parameter_map = FixedArray::cast(old_elements);
    parameter_map->set(1, elems);
  }

  if (FLAG_trace_elements_transitions) {
    PrintElementsTransition(stdout, elements_kind, old_elements,
                            GetElementsKind(), elems);
  }

  if (IsJSArray()) {
    JSArray::cast(this)->set_length(Smi::FromInt(length));
  }

  return this;
}


MaybeObject* JSArray::Initialize(int capacity, int length) {
  ASSERT(capacity >= 0);
  return GetHeap()->AllocateJSArrayStorage(this, length, capacity,
                                           INITIALIZE_ARRAY_ELEMENTS_WITH_HOLE);
}


void JSArray::Expand(int required_size) {
  GetIsolate()->factory()->SetElementsCapacityAndLength(
      Handle<JSArray>(this), required_size, required_size);
}


// Returns false if the passed-in index is marked non-configurable,
// which will cause the ES5 truncation operation to halt, and thus
// no further old values need be collected.
static bool GetOldValue(Isolate* isolate,
                        Handle<JSObject> object,
                        uint32_t index,
                        List<Handle<Object> >* old_values,
                        List<uint32_t>* indices) {
  PropertyAttributes attributes = object->GetLocalElementAttribute(index);
  ASSERT(attributes != ABSENT);
  if (attributes == DONT_DELETE) return false;
  old_values->Add(object->GetLocalElementAccessorPair(index) == NULL
      ? Object::GetElement(isolate, object, index)
      : Handle<Object>::cast(isolate->factory()->the_hole_value()));
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

  bool threw;
  Execution::Call(isolate,
                  Handle<JSFunction>(isolate->observers_enqueue_splice()),
                  isolate->factory()->undefined_value(), ARRAY_SIZE(args), args,
                  &threw);
  ASSERT(!threw);
}


static void BeginPerformSplice(Handle<JSArray> object) {
  Isolate* isolate = object->GetIsolate();
  HandleScope scope(isolate);
  Handle<Object> args[] = { object };

  bool threw;
  Execution::Call(isolate,
                  Handle<JSFunction>(isolate->observers_begin_perform_splice()),
                  isolate->factory()->undefined_value(), ARRAY_SIZE(args), args,
                  &threw);
  ASSERT(!threw);
}


static void EndPerformSplice(Handle<JSArray> object) {
  Isolate* isolate = object->GetIsolate();
  HandleScope scope(isolate);
  Handle<Object> args[] = { object };

  bool threw;
  Execution::Call(isolate,
                  Handle<JSFunction>(isolate->observers_end_perform_splice()),
                  isolate->factory()->undefined_value(), ARRAY_SIZE(args), args,
                  &threw);
  ASSERT(!threw);
}


MaybeObject* JSArray::SetElementsLength(Object* len) {
  // We should never end in here with a pixel or external array.
  ASSERT(AllowsSetElementsLength());
  if (!(FLAG_harmony_observation && map()->is_observed()))
    return GetElementsAccessor()->SetLength(this, len);

  Isolate* isolate = GetIsolate();
  HandleScope scope(isolate);
  Handle<JSArray> self(this);
  List<uint32_t> indices;
  List<Handle<Object> > old_values;
  Handle<Object> old_length_handle(self->length(), isolate);
  Handle<Object> new_length_handle(len, isolate);
  uint32_t old_length = 0;
  CHECK(old_length_handle->ToArrayIndex(&old_length));
  uint32_t new_length = 0;
  if (!new_length_handle->ToArrayIndex(&new_length))
    return Failure::InternalError();

  // Observed arrays should always be in dictionary mode;
  // if they were in fast mode, the below is slower than necessary
  // as it iterates over the array backing store multiple times.
  ASSERT(self->HasDictionaryElements());
  static const PropertyAttributes kNoAttrFilter = NONE;
  int num_elements = self->NumberOfLocalElements(kNoAttrFilter);
  if (num_elements > 0) {
    if (old_length == static_cast<uint32_t>(num_elements)) {
      // Simple case for arrays without holes.
      for (uint32_t i = old_length - 1; i + 1 > new_length; --i) {
        if (!GetOldValue(isolate, self, i, &old_values, &indices)) break;
      }
    } else {
      // For sparse arrays, only iterate over existing elements.
      Handle<FixedArray> keys = isolate->factory()->NewFixedArray(num_elements);
      self->GetLocalElementKeys(*keys, kNoAttrFilter);
      while (num_elements-- > 0) {
        uint32_t index = NumberToUint32(keys->get(num_elements));
        if (index < new_length) break;
        if (!GetOldValue(isolate, self, index, &old_values, &indices)) break;
      }
    }
  }

  MaybeObject* result =
      self->GetElementsAccessor()->SetLength(*self, *new_length_handle);
  Handle<Object> hresult;
  if (!result->ToHandle(&hresult, isolate)) return result;

  CHECK(self->length()->ToArrayIndex(&new_length));
  if (old_length == new_length) return *hresult;

  BeginPerformSplice(self);

  for (int i = 0; i < indices.length(); ++i) {
    JSObject::EnqueueChangeRecord(
        self, "deleted", isolate->factory()->Uint32ToString(indices[i]),
        old_values[i]);
  }
  JSObject::EnqueueChangeRecord(
      self, "updated", isolate->factory()->length_string(),
      old_length_handle);

  EndPerformSplice(self);

  uint32_t index = Min(old_length, new_length);
  uint32_t add_count = new_length > old_length ? new_length - old_length : 0;
  uint32_t delete_count = new_length < old_length ? old_length - new_length : 0;
  Handle<JSArray> deleted = isolate->factory()->NewJSArray(0);
  if (delete_count > 0) {
    for (int i = indices.length() - 1; i >= 0; i--) {
      JSObject::SetElement(deleted, indices[i] - index, old_values[i], NONE,
                           kNonStrictMode);
    }

    SetProperty(deleted, isolate->factory()->length_string(),
                isolate->factory()->NewNumberFromUint(delete_count),
                NONE, kNonStrictMode);
  }

  EnqueueSpliceRecord(self, index, deleted, add_count);

  return *hresult;
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
  ASSERT(target_map->IsMap());
  ASSERT(HeapObject::cast(*prototype)->map()->IsMap());
  // Don't cache prototype transition if this map is shared.
  if (map->is_shared() || !FLAG_cache_prototype_transitions) return map;

  const int step = kProtoTransitionElementsPerEntry;
  const int header = kProtoTransitionHeaderSize;

  Handle<FixedArray> cache(map->GetPrototypeTransitions());
  int capacity = (cache->length() - header) / step;
  int transitions = map->NumberOfProtoTransitions() + 1;

  if (transitions > capacity) {
    if (capacity > kMaxCachedPrototypeTransitions) return map;

    // Grow array by factor 2 over and above what we need.
    Factory* factory = map->GetIsolate()->factory();
    cache = factory->CopySizeFixedArray(cache, transitions * 2 * step + header);

    CALL_AND_RETRY_OR_DIE(map->GetIsolate(),
                          map->SetPrototypeTransitions(*cache),
                          break,
                          return Handle<Map>());
  }

  // Reload number of transitions as GC might shrink them.
  int last = map->NumberOfProtoTransitions();
  int entry = header + last * step;

  cache->set(entry + kProtoTransitionPrototypeOffset, *prototype);
  cache->set(entry + kProtoTransitionMapOffset, *target_map);
  map->SetNumberOfProtoTransitions(transitions);

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


void Map::AddDependentCompilationInfo(DependentCode::DependencyGroup group,
                                      CompilationInfo* info) {
  Handle<DependentCode> dep(dependent_code());
  Handle<DependentCode> codes =
      DependentCode::Insert(dep, group, info->object_wrapper());
  if (*codes != dependent_code()) set_dependent_code(*codes);
  info->dependencies(group)->Add(Handle<HeapObject>(this), info->zone());
}


void Map::AddDependentCode(DependentCode::DependencyGroup group,
                           Handle<Code> code) {
  Handle<DependentCode> codes = DependentCode::Insert(
      Handle<DependentCode>(dependent_code()), group, code);
  if (*codes != dependent_code()) set_dependent_code(*codes);
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
  if (start < end && entries->object_at(end - 1) == *object) {
    // Do not append the compilation info if it is already in the array.
    // It is sufficient to just check only the last element because
    // we process embedded maps of an optimized code in one batch.
    return entries;
  }
  if (entries->length() < kCodesStartIndex + number_of_entries + 1) {
    Factory* factory = entries->GetIsolate()->factory();
    int capacity = kCodesStartIndex + number_of_entries + 1;
    if (capacity > 5) capacity = capacity * 5 / 4;
    Handle<DependentCode> new_entries = Handle<DependentCode>::cast(
        factory->CopySizeFixedArray(entries, capacity));
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
    ASSERT(is_code_at(i) || compilation_info_at(i) != info);
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
    ASSERT(last_of_group >= gap);
    if (last_of_group == gap) continue;
    copy(last_of_group, gap);
    gap = last_of_group;
  }
  ASSERT(gap == starts.number_of_entries() - 1);
  clear_at(gap);  // Clear last gap.
  set_number_of_entries(group, end - start - 1);

#ifdef DEBUG
  for (int i = start; i < end - 1; i++) {
    ASSERT(is_code_at(i) || compilation_info_at(i) != info);
  }
#endif
}


bool DependentCode::Contains(DependencyGroup group, Code* code) {
  GroupStartIndexes starts(this);
  int start = starts.at(group);
  int end = starts.at(group + 1);
  for (int i = start; i < end; i++) {
    if (object_at(i) == code) return true;
  }
  return false;
}


void DependentCode::DeoptimizeDependentCodeGroup(
    Isolate* isolate,
    DependentCode::DependencyGroup group) {
  ASSERT(AllowCodeDependencyChange::IsAllowed());
  DisallowHeapAllocation no_allocation_scope;
  DependentCode::GroupStartIndexes starts(this);
  int start = starts.at(group);
  int end = starts.at(group + 1);
  int code_entries = starts.number_of_entries();
  if (start == end) return;

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

  if (marked) Deoptimizer::DeoptimizeMarkedCode(isolate);
}


Handle<Object> JSObject::SetPrototype(Handle<JSObject> object,
                                      Handle<Object> value,
                                      bool skip_hidden_prototypes) {
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
    isolate->Throw(*error);
    return Handle<Object>();
  }

  // Before we can set the prototype we need to be sure
  // prototype cycles are prevented.
  // It is sufficient to validate that the receiver is not in the new prototype
  // chain.
  for (Object* pt = *value;
       pt != heap->null_value();
       pt = pt->GetPrototype(isolate)) {
    if (JSReceiver::cast(pt) == *object) {
      // Cycle detected.
      Handle<Object> error = isolate->factory()->NewError(
          "cyclic_proto", HandleVector<Object>(NULL, 0));
      isolate->Throw(*error);
      return Handle<Object>();
    }
  }

  Handle<JSObject> real_receiver = object;

  if (skip_hidden_prototypes) {
    // Find the first object in the chain whose prototype object is not
    // hidden and set the new prototype on that object.
    Object* current_proto = real_receiver->GetPrototype();
    while (current_proto->IsJSObject() &&
          JSObject::cast(current_proto)->map()->is_hidden_prototype()) {
      real_receiver = handle(JSObject::cast(current_proto), isolate);
      current_proto = current_proto->GetPrototype(isolate);
    }
  }

  // Set the new prototype of the object.
  Handle<Map> map(real_receiver->map());

  // Nothing to do if prototype is already set.
  if (map->prototype() == *value) return value;

  if (value->IsJSObject()) {
    JSObject::OptimizeAsPrototype(Handle<JSObject>::cast(value));
  }

  Handle<Map> new_map = Map::GetPrototypeTransition(map, value);
  if (new_map.is_null()) {
    new_map = Map::Copy(map);
    Map::PutPrototypeTransition(map, value, new_map);
    new_map->set_prototype(*value);
  }
  ASSERT(new_map->prototype() == *value);
  real_receiver->set_map(*new_map);

  heap->ClearInstanceofCache();
  ASSERT(size == object->Size());
  return value;
}


MaybeObject* JSObject::EnsureCanContainElements(Arguments* args,
                                                uint32_t first_arg,
                                                uint32_t arg_count,
                                                EnsureElementsMode mode) {
  // Elements in |Arguments| are ordered backwards (because they're on the
  // stack), but the method that's called here iterates over them in forward
  // direction.
  return EnsureCanContainElements(
      args->arguments() - first_arg - (arg_count - 1),
      arg_count, mode);
}


PropertyType JSObject::GetLocalPropertyType(Name* name) {
  uint32_t index = 0;
  if (name->AsArrayIndex(&index)) {
    return GetLocalElementType(index);
  }
  LookupResult lookup(GetIsolate());
  LocalLookup(name, &lookup, true);
  return lookup.type();
}


PropertyType JSObject::GetLocalElementType(uint32_t index) {
  return GetElementsAccessor()->GetType(this, this, index);
}


AccessorPair* JSObject::GetLocalPropertyAccessorPair(Name* name) {
  uint32_t index = 0;
  if (name->AsArrayIndex(&index)) {
    return GetLocalElementAccessorPair(index);
  }

  LookupResult lookup(GetIsolate());
  LocalLookupRealNamedProperty(name, &lookup);

  if (lookup.IsPropertyCallbacks() &&
      lookup.GetCallbackObject()->IsAccessorPair()) {
    return AccessorPair::cast(lookup.GetCallbackObject());
  }
  return NULL;
}


AccessorPair* JSObject::GetLocalElementAccessorPair(uint32_t index) {
  if (IsJSGlobalProxy()) {
    Object* proto = GetPrototype();
    if (proto->IsNull()) return NULL;
    ASSERT(proto->IsJSGlobalObject());
    return JSObject::cast(proto)->GetLocalElementAccessorPair(index);
  }

  // Check for lookup interceptor.
  if (HasIndexedInterceptor()) return NULL;

  return GetElementsAccessor()->GetAccessorPair(this, this, index);
}


MaybeObject* JSObject::SetElementWithInterceptor(uint32_t index,
                                                 Object* value,
                                                 PropertyAttributes attributes,
                                                 StrictModeFlag strict_mode,
                                                 bool check_prototype,
                                                 SetPropertyMode set_mode) {
  Isolate* isolate = GetIsolate();
  HandleScope scope(isolate);

  // Make sure that the top context does not change when doing
  // callbacks or interceptor calls.
  AssertNoContextChange ncc;

  Handle<InterceptorInfo> interceptor(GetIndexedInterceptor());
  Handle<JSObject> this_handle(this);
  Handle<Object> value_handle(value, isolate);
  if (!interceptor->setter()->IsUndefined()) {
    v8::IndexedPropertySetterCallback setter =
        v8::ToCData<v8::IndexedPropertySetterCallback>(interceptor->setter());
    LOG(isolate,
        ApiIndexedPropertyAccess("interceptor-indexed-set", this, index));
    PropertyCallbackArguments args(isolate, interceptor->data(), this, this);
    v8::Handle<v8::Value> result =
        args.Call(setter, index, v8::Utils::ToLocal(value_handle));
    RETURN_IF_SCHEDULED_EXCEPTION(isolate);
    if (!result.IsEmpty()) return *value_handle;
  }
  MaybeObject* raw_result =
      this_handle->SetElementWithoutInterceptor(index,
                                                *value_handle,
                                                attributes,
                                                strict_mode,
                                                check_prototype,
                                                set_mode);
  RETURN_IF_SCHEDULED_EXCEPTION(isolate);
  return raw_result;
}


MaybeObject* JSObject::GetElementWithCallback(Object* receiver,
                                              Object* structure,
                                              uint32_t index,
                                              Object* holder) {
  Isolate* isolate = GetIsolate();
  ASSERT(!structure->IsForeign());

  // api style callbacks.
  if (structure->IsExecutableAccessorInfo()) {
    Handle<ExecutableAccessorInfo> data(
        ExecutableAccessorInfo::cast(structure));
    Object* fun_obj = data->getter();
    v8::AccessorGetterCallback call_fun =
        v8::ToCData<v8::AccessorGetterCallback>(fun_obj);
    if (call_fun == NULL) return isolate->heap()->undefined_value();
    HandleScope scope(isolate);
    Handle<JSObject> self(JSObject::cast(receiver));
    Handle<JSObject> holder_handle(JSObject::cast(holder));
    Handle<Object> number = isolate->factory()->NewNumberFromUint(index);
    Handle<String> key = isolate->factory()->NumberToString(number);
    LOG(isolate, ApiNamedPropertyAccess("load", *self, *key));
    PropertyCallbackArguments
        args(isolate, data->data(), *self, *holder_handle);
    v8::Handle<v8::Value> result = args.Call(call_fun, v8::Utils::ToLocal(key));
    RETURN_IF_SCHEDULED_EXCEPTION(isolate);
    if (result.IsEmpty()) return isolate->heap()->undefined_value();
    Handle<Object> result_internal = v8::Utils::OpenHandle(*result);
    result_internal->VerifyApiCallResultType();
    return *result_internal;
  }

  // __defineGetter__ callback
  if (structure->IsAccessorPair()) {
    Object* getter = AccessorPair::cast(structure)->getter();
    if (getter->IsSpecFunction()) {
      // TODO(rossberg): nicer would be to cast to some JSCallable here...
      return GetPropertyWithDefinedGetter(receiver, JSReceiver::cast(getter));
    }
    // Getter is not a function.
    return isolate->heap()->undefined_value();
  }

  if (structure->IsDeclaredAccessorInfo()) {
    return GetDeclaredAccessorProperty(receiver,
                                       DeclaredAccessorInfo::cast(structure),
                                       isolate);
  }

  UNREACHABLE();
  return NULL;
}


MaybeObject* JSObject::SetElementWithCallback(Object* structure,
                                              uint32_t index,
                                              Object* value,
                                              JSObject* holder,
                                              StrictModeFlag strict_mode) {
  Isolate* isolate = GetIsolate();
  HandleScope scope(isolate);

  // We should never get here to initialize a const with the hole
  // value since a const declaration would conflict with the setter.
  ASSERT(!value->IsTheHole());
  Handle<Object> value_handle(value, isolate);

  // To accommodate both the old and the new api we switch on the
  // data structure used to store the callbacks.  Eventually foreign
  // callbacks should be phased out.
  ASSERT(!structure->IsForeign());

  if (structure->IsExecutableAccessorInfo()) {
    // api style callbacks
    Handle<JSObject> self(this);
    Handle<JSObject> holder_handle(JSObject::cast(holder));
    Handle<ExecutableAccessorInfo> data(
        ExecutableAccessorInfo::cast(structure));
    Object* call_obj = data->setter();
    v8::AccessorSetterCallback call_fun =
        v8::ToCData<v8::AccessorSetterCallback>(call_obj);
    if (call_fun == NULL) return value;
    Handle<Object> number = isolate->factory()->NewNumberFromUint(index);
    Handle<String> key(isolate->factory()->NumberToString(number));
    LOG(isolate, ApiNamedPropertyAccess("store", *self, *key));
    PropertyCallbackArguments
        args(isolate, data->data(), *self, *holder_handle);
    args.Call(call_fun,
              v8::Utils::ToLocal(key),
              v8::Utils::ToLocal(value_handle));
    RETURN_IF_SCHEDULED_EXCEPTION(isolate);
    return *value_handle;
  }

  if (structure->IsAccessorPair()) {
    Handle<Object> setter(AccessorPair::cast(structure)->setter(), isolate);
    if (setter->IsSpecFunction()) {
      // TODO(rossberg): nicer would be to cast to some JSCallable here...
      return SetPropertyWithDefinedSetter(JSReceiver::cast(*setter), value);
    } else {
      if (strict_mode == kNonStrictMode) {
        return value;
      }
      Handle<Object> holder_handle(holder, isolate);
      Handle<Object> key(isolate->factory()->NewNumberFromUint(index));
      Handle<Object> args[2] = { key, holder_handle };
      return isolate->Throw(
          *isolate->factory()->NewTypeError("no_setter_in_callback",
                                            HandleVector(args, 2)));
    }
  }

  // TODO(dcarney): Handle correctly.
  if (structure->IsDeclaredAccessorInfo()) return value;

  UNREACHABLE();
  return NULL;
}


bool JSObject::HasFastArgumentsElements() {
  Heap* heap = GetHeap();
  if (!elements()->IsFixedArray()) return false;
  FixedArray* elements = FixedArray::cast(this->elements());
  if (elements->map() != heap->non_strict_arguments_elements_map()) {
    return false;
  }
  FixedArray* arguments = FixedArray::cast(elements->get(1));
  return !arguments->IsDictionary();
}


bool JSObject::HasDictionaryArgumentsElements() {
  Heap* heap = GetHeap();
  if (!elements()->IsFixedArray()) return false;
  FixedArray* elements = FixedArray::cast(this->elements());
  if (elements->map() != heap->non_strict_arguments_elements_map()) {
    return false;
  }
  FixedArray* arguments = FixedArray::cast(elements->get(1));
  return arguments->IsDictionary();
}


// Adding n elements in fast case is O(n*n).
// Note: revisit design to have dual undefined values to capture absent
// elements.
MaybeObject* JSObject::SetFastElement(uint32_t index,
                                      Object* value,
                                      StrictModeFlag strict_mode,
                                      bool check_prototype) {
  ASSERT(HasFastSmiOrObjectElements() ||
         HasFastArgumentsElements());

  // Array optimizations rely on the prototype lookups of Array objects always
  // returning undefined. If there is a store to the initial prototype object,
  // make sure all of these optimizations are invalidated.
  Isolate* isolate(GetIsolate());
  if (isolate->is_initial_object_prototype(this) ||
      isolate->is_initial_array_prototype(this)) {
    HandleScope scope(GetIsolate());
    map()->dependent_code()->DeoptimizeDependentCodeGroup(
        GetIsolate(),
        DependentCode::kElementsCantBeAddedGroup);
  }

  FixedArray* backing_store = FixedArray::cast(elements());
  if (backing_store->map() == GetHeap()->non_strict_arguments_elements_map()) {
    backing_store = FixedArray::cast(backing_store->get(1));
  } else {
    MaybeObject* maybe = EnsureWritableFastElements();
    if (!maybe->To(&backing_store)) return maybe;
  }
  uint32_t capacity = static_cast<uint32_t>(backing_store->length());

  if (check_prototype &&
      (index >= capacity || backing_store->get(index)->IsTheHole())) {
    bool found;
    MaybeObject* result = SetElementWithCallbackSetterInPrototypes(index,
                                                                   value,
                                                                   &found,
                                                                   strict_mode);
    if (found) return result;
  }

  uint32_t new_capacity = capacity;
  // Check if the length property of this object needs to be updated.
  uint32_t array_length = 0;
  bool must_update_array_length = false;
  bool introduces_holes = true;
  if (IsJSArray()) {
    CHECK(JSArray::cast(this)->length()->ToArrayIndex(&array_length));
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
  ElementsKind elements_kind = GetElementsKind();
  if (introduces_holes &&
      IsFastElementsKind(elements_kind) &&
      !IsFastHoleyElementsKind(elements_kind)) {
    ElementsKind transitioned_kind = GetHoleyElementsKind(elements_kind);
    MaybeObject* maybe = TransitionElementsKind(transitioned_kind);
    if (maybe->IsFailure()) return maybe;
  }

  // Check if the capacity of the backing store needs to be increased, or if
  // a transition to slow elements is necessary.
  if (index >= capacity) {
    bool convert_to_slow = true;
    if ((index - capacity) < kMaxGap) {
      new_capacity = NewElementsCapacity(index + 1);
      ASSERT(new_capacity > index);
      if (!ShouldConvertToSlowElements(new_capacity)) {
        convert_to_slow = false;
      }
    }
    if (convert_to_slow) {
      MaybeObject* result = NormalizeElements();
      if (result->IsFailure()) return result;
      return SetDictionaryElement(index, value, NONE, strict_mode,
                                  check_prototype);
    }
  }
  // Convert to fast double elements if appropriate.
  if (HasFastSmiElements() && !value->IsSmi() && value->IsNumber()) {
    // Consider fixing the boilerplate as well if we have one.
    ElementsKind to_kind = IsHoleyElementsKind(elements_kind)
        ? FAST_HOLEY_DOUBLE_ELEMENTS
        : FAST_DOUBLE_ELEMENTS;

    MaybeObject* maybe_failure = UpdateAllocationSite(to_kind);
    if (maybe_failure->IsFailure()) return maybe_failure;

    MaybeObject* maybe =
        SetFastDoubleElementsCapacityAndLength(new_capacity, array_length);
    if (maybe->IsFailure()) return maybe;
    FixedDoubleArray::cast(elements())->set(index, value->Number());
    ValidateElements();
    return value;
  }
  // Change elements kind from Smi-only to generic FAST if necessary.
  if (HasFastSmiElements() && !value->IsSmi()) {
    Map* new_map;
    ElementsKind kind = HasFastHoleyElements()
        ? FAST_HOLEY_ELEMENTS
        : FAST_ELEMENTS;

    MaybeObject* maybe_failure = UpdateAllocationSite(kind);
    if (maybe_failure->IsFailure()) return maybe_failure;

    MaybeObject* maybe_new_map = GetElementsTransitionMap(GetIsolate(),
                                                          kind);
    if (!maybe_new_map->To(&new_map)) return maybe_new_map;

    set_map(new_map);
  }
  // Increase backing store capacity if that's been decided previously.
  if (new_capacity != capacity) {
    FixedArray* new_elements;
    SetFastElementsCapacitySmiMode smi_mode =
        value->IsSmi() && HasFastSmiElements()
            ? kAllowSmiElements
            : kDontAllowSmiElements;
    { MaybeObject* maybe =
          SetFastElementsCapacityAndLength(new_capacity,
                                           array_length,
                                           smi_mode);
      if (!maybe->To(&new_elements)) return maybe;
    }
    new_elements->set(index, value);
    ValidateElements();
    return value;
  }

  // Finally, set the new element and length.
  ASSERT(elements()->IsFixedArray());
  backing_store->set(index, value);
  if (must_update_array_length) {
    JSArray::cast(this)->set_length(Smi::FromInt(array_length));
  }
  return value;
}


MaybeObject* JSObject::SetDictionaryElement(uint32_t index,
                                            Object* value_raw,
                                            PropertyAttributes attributes,
                                            StrictModeFlag strict_mode,
                                            bool check_prototype,
                                            SetPropertyMode set_mode) {
  ASSERT(HasDictionaryElements() || HasDictionaryArgumentsElements());
  Isolate* isolate = GetIsolate();
  Heap* heap = isolate->heap();
  Handle<JSObject> self(this);
  Handle<Object> value(value_raw, isolate);

  // Insert element in the dictionary.
  Handle<FixedArray> elements(FixedArray::cast(this->elements()));
  bool is_arguments =
      (elements->map() == heap->non_strict_arguments_elements_map());
  Handle<SeededNumberDictionary> dictionary(is_arguments
    ? SeededNumberDictionary::cast(elements->get(1))
    : SeededNumberDictionary::cast(*elements));

  int entry = dictionary->FindEntry(index);
  if (entry != SeededNumberDictionary::kNotFound) {
    Object* element = dictionary->ValueAt(entry);
    PropertyDetails details = dictionary->DetailsAt(entry);
    if (details.type() == CALLBACKS && set_mode == SET_PROPERTY) {
      return SetElementWithCallback(element, index, *value, this, strict_mode);
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
        if (strict_mode == kNonStrictMode) {
          return isolate->heap()->undefined_value();
        } else {
          Handle<Object> holder(this, isolate);
          Handle<Object> number = isolate->factory()->NewNumberFromUint(index);
          Handle<Object> args[2] = { number, holder };
          Handle<Object> error =
              isolate->factory()->NewTypeError("strict_read_only_property",
                                               HandleVector(args, 2));
          return isolate->Throw(*error);
        }
      }
      // Elements of the arguments object in slow mode might be slow aliases.
      if (is_arguments && element->IsAliasedArgumentsEntry()) {
        AliasedArgumentsEntry* entry = AliasedArgumentsEntry::cast(element);
        Context* context = Context::cast(elements->get(0));
        int context_index = entry->aliased_context_slot();
        ASSERT(!context->get(context_index)->IsTheHole());
        context->set(context_index, *value);
        // For elements that are still writable we keep slow aliasing.
        if (!details.IsReadOnly()) value = handle(element, isolate);
      }
      dictionary->ValueAtPut(entry, *value);
    }
  } else {
    // Index not already used. Look for an accessor in the prototype chain.
    // Can cause GC!
    if (check_prototype) {
      bool found;
      MaybeObject* result = SetElementWithCallbackSetterInPrototypes(
          index, *value, &found, strict_mode);
      if (found) return result;
    }
    // When we set the is_extensible flag to false we always force the
    // element into dictionary mode (and force them to stay there).
    if (!self->map()->is_extensible()) {
      if (strict_mode == kNonStrictMode) {
        return isolate->heap()->undefined_value();
      } else {
        Handle<Object> number = isolate->factory()->NewNumberFromUint(index);
        Handle<String> name = isolate->factory()->NumberToString(number);
        Handle<Object> args[1] = { name };
        Handle<Object> error =
            isolate->factory()->NewTypeError("object_not_extensible",
                                             HandleVector(args, 1));
        return isolate->Throw(*error);
      }
    }
    FixedArrayBase* new_dictionary;
    PropertyDetails details = PropertyDetails(attributes, NORMAL, 0);
    MaybeObject* maybe = dictionary->AddNumberEntry(index, *value, details);
    if (!maybe->To(&new_dictionary)) return maybe;
    if (*dictionary != SeededNumberDictionary::cast(new_dictionary)) {
      if (is_arguments) {
        elements->set(1, new_dictionary);
      } else {
        self->set_elements(new_dictionary);
      }
      dictionary =
          handle(SeededNumberDictionary::cast(new_dictionary), isolate);
    }
  }

  // Update the array length if this JSObject is an array.
  if (self->IsJSArray()) {
    MaybeObject* result =
        JSArray::cast(*self)->JSArrayUpdateLengthFromIndex(index, *value);
    if (result->IsFailure()) return result;
  }

  // Attempt to put this object back in fast case.
  if (self->ShouldConvertToFastElements()) {
    uint32_t new_length = 0;
    if (self->IsJSArray()) {
      CHECK(JSArray::cast(*self)->length()->ToArrayIndex(&new_length));
    } else {
      new_length = dictionary->max_number_key() + 1;
    }
    SetFastElementsCapacitySmiMode smi_mode = FLAG_smi_only_arrays
        ? kAllowSmiElements
        : kDontAllowSmiElements;
    bool has_smi_only_elements = false;
    bool should_convert_to_fast_double_elements =
        self->ShouldConvertToFastDoubleElements(&has_smi_only_elements);
    if (has_smi_only_elements) {
      smi_mode = kForceSmiElements;
    }
    MaybeObject* result = should_convert_to_fast_double_elements
        ? self->SetFastDoubleElementsCapacityAndLength(new_length, new_length)
        : self->SetFastElementsCapacityAndLength(
            new_length, new_length, smi_mode);
    self->ValidateElements();
    if (result->IsFailure()) return result;
#ifdef DEBUG
    if (FLAG_trace_normalization) {
      PrintF("Object elements are fast case again:\n");
      Print();
    }
#endif
  }
  return *value;
}


MUST_USE_RESULT MaybeObject* JSObject::SetFastDoubleElement(
    uint32_t index,
    Object* value,
    StrictModeFlag strict_mode,
    bool check_prototype) {
  ASSERT(HasFastDoubleElements());

  FixedArrayBase* base_elms = FixedArrayBase::cast(elements());
  uint32_t elms_length = static_cast<uint32_t>(base_elms->length());

  // If storing to an element that isn't in the array, pass the store request
  // up the prototype chain before storing in the receiver's elements.
  if (check_prototype &&
      (index >= elms_length ||
       FixedDoubleArray::cast(base_elms)->is_the_hole(index))) {
    bool found;
    MaybeObject* result = SetElementWithCallbackSetterInPrototypes(index,
                                                                   value,
                                                                   &found,
                                                                   strict_mode);
    if (found) return result;
  }

  // If the value object is not a heap number, switch to fast elements and try
  // again.
  bool value_is_smi = value->IsSmi();
  bool introduces_holes = true;
  uint32_t length = elms_length;
  if (IsJSArray()) {
    CHECK(JSArray::cast(this)->length()->ToArrayIndex(&length));
    introduces_holes = index > length;
  } else {
    introduces_holes = index >= elms_length;
  }

  if (!value->IsNumber()) {
    MaybeObject* maybe_obj = SetFastElementsCapacityAndLength(
        elms_length,
        length,
        kDontAllowSmiElements);
    if (maybe_obj->IsFailure()) return maybe_obj;
    maybe_obj = SetFastElement(index, value, strict_mode, check_prototype);
    if (maybe_obj->IsFailure()) return maybe_obj;
    ValidateElements();
    return maybe_obj;
  }

  double double_value = value_is_smi
      ? static_cast<double>(Smi::cast(value)->value())
      : HeapNumber::cast(value)->value();

  // If the array is growing, and it's not growth by a single element at the
  // end, make sure that the ElementsKind is HOLEY.
  ElementsKind elements_kind = GetElementsKind();
  if (introduces_holes && !IsFastHoleyElementsKind(elements_kind)) {
    ElementsKind transitioned_kind = GetHoleyElementsKind(elements_kind);
    MaybeObject* maybe = TransitionElementsKind(transitioned_kind);
    if (maybe->IsFailure()) return maybe;
  }

  // Check whether there is extra space in the fixed array.
  if (index < elms_length) {
    FixedDoubleArray* elms = FixedDoubleArray::cast(elements());
    elms->set(index, double_value);
    if (IsJSArray()) {
      // Update the length of the array if needed.
      uint32_t array_length = 0;
      CHECK(JSArray::cast(this)->length()->ToArrayIndex(&array_length));
      if (index >= array_length) {
        JSArray::cast(this)->set_length(Smi::FromInt(index + 1));
      }
    }
    return value;
  }

  // Allow gap in fast case.
  if ((index - elms_length) < kMaxGap) {
    // Try allocating extra space.
    int new_capacity = NewElementsCapacity(index+1);
    if (!ShouldConvertToSlowElements(new_capacity)) {
      ASSERT(static_cast<uint32_t>(new_capacity) > index);
      MaybeObject* maybe_obj =
          SetFastDoubleElementsCapacityAndLength(new_capacity, index + 1);
      if (maybe_obj->IsFailure()) return maybe_obj;
      FixedDoubleArray::cast(elements())->set(index, double_value);
      ValidateElements();
      return value;
    }
  }

  // Otherwise default to slow case.
  ASSERT(HasFastDoubleElements());
  ASSERT(map()->has_fast_double_elements());
  ASSERT(elements()->IsFixedDoubleArray());
  Object* obj;
  { MaybeObject* maybe_obj = NormalizeElements();
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
  }
  ASSERT(HasDictionaryElements());
  return SetElement(index, value, NONE, strict_mode, check_prototype);
}


Handle<Object> JSReceiver::SetElement(Handle<JSReceiver> object,
                                      uint32_t index,
                                      Handle<Object> value,
                                      PropertyAttributes attributes,
                                      StrictModeFlag strict_mode) {
  if (object->IsJSProxy()) {
    return JSProxy::SetElementWithHandler(
        Handle<JSProxy>::cast(object), object, index, value, strict_mode);
  }
  return JSObject::SetElement(
      Handle<JSObject>::cast(object), index, value, attributes, strict_mode);
}


Handle<Object> JSObject::SetOwnElement(Handle<JSObject> object,
                                       uint32_t index,
                                       Handle<Object> value,
                                       StrictModeFlag strict_mode) {
  ASSERT(!object->HasExternalArrayElements());
  CALL_HEAP_FUNCTION(
      object->GetIsolate(),
      object->SetElement(index, *value, NONE, strict_mode, false),
      Object);
}


Handle<Object> JSObject::SetElement(Handle<JSObject> object,
                                    uint32_t index,
                                    Handle<Object> value,
                                    PropertyAttributes attr,
                                    StrictModeFlag strict_mode,
                                    SetPropertyMode set_mode) {
  if (object->HasExternalArrayElements()) {
    if (!value->IsNumber() && !value->IsUndefined()) {
      bool has_exception;
      Handle<Object> number =
          Execution::ToNumber(object->GetIsolate(), value, &has_exception);
      if (has_exception) return Handle<Object>();
      value = number;
    }
  }
  CALL_HEAP_FUNCTION(
      object->GetIsolate(),
      object->SetElement(index, *value, attr, strict_mode, true, set_mode),
      Object);
}


MaybeObject* JSObject::SetElement(uint32_t index,
                                  Object* value_raw,
                                  PropertyAttributes attributes,
                                  StrictModeFlag strict_mode,
                                  bool check_prototype,
                                  SetPropertyMode set_mode) {
  Isolate* isolate = GetIsolate();

  // Check access rights if needed.
  if (IsAccessCheckNeeded()) {
    if (!isolate->MayIndexedAccess(this, index, v8::ACCESS_SET)) {
      isolate->ReportFailedAccessCheck(this, v8::ACCESS_SET);
      RETURN_IF_SCHEDULED_EXCEPTION(isolate);
      return value_raw;
    }
  }

  if (IsJSGlobalProxy()) {
    Object* proto = GetPrototype();
    if (proto->IsNull()) return value_raw;
    ASSERT(proto->IsJSGlobalObject());
    return JSObject::cast(proto)->SetElement(index,
                                             value_raw,
                                             attributes,
                                             strict_mode,
                                             check_prototype,
                                             set_mode);
  }

  // Don't allow element properties to be redefined for external arrays.
  if (HasExternalArrayElements() && set_mode == DEFINE_PROPERTY) {
    Handle<Object> number = isolate->factory()->NewNumberFromUint(index);
    Handle<Object> args[] = { handle(this, isolate), number };
    Handle<Object> error = isolate->factory()->NewTypeError(
        "redef_external_array_element", HandleVector(args, ARRAY_SIZE(args)));
    return isolate->Throw(*error);
  }

  // Normalize the elements to enable attributes on the property.
  if ((attributes & (DONT_DELETE | DONT_ENUM | READ_ONLY)) != 0) {
    SeededNumberDictionary* dictionary;
    MaybeObject* maybe_object = NormalizeElements();
    if (!maybe_object->To(&dictionary)) return maybe_object;
    // Make sure that we never go back to fast case.
    dictionary->set_requires_slow_elements();
  }

  if (!(FLAG_harmony_observation && map()->is_observed())) {
    return HasIndexedInterceptor()
      ? SetElementWithInterceptor(
          index, value_raw, attributes, strict_mode, check_prototype, set_mode)
      : SetElementWithoutInterceptor(
          index, value_raw, attributes, strict_mode, check_prototype, set_mode);
  }

  // From here on, everything has to be handlified.
  Handle<JSObject> self(this);
  Handle<Object> value(value_raw, isolate);
  PropertyAttributes old_attributes = self->GetLocalElementAttribute(index);
  Handle<Object> old_value = isolate->factory()->the_hole_value();
  Handle<Object> old_length_handle;
  Handle<Object> new_length_handle;

  if (old_attributes != ABSENT) {
    if (self->GetLocalElementAccessorPair(index) == NULL)
      old_value = Object::GetElement(isolate, self, index);
  } else if (self->IsJSArray()) {
    // Store old array length in case adding an element grows the array.
    old_length_handle = handle(Handle<JSArray>::cast(self)->length(), isolate);
  }

  // Check for lookup interceptor
  MaybeObject* result = self->HasIndexedInterceptor()
    ? self->SetElementWithInterceptor(
        index, *value, attributes, strict_mode, check_prototype, set_mode)
    : self->SetElementWithoutInterceptor(
        index, *value, attributes, strict_mode, check_prototype, set_mode);

  Handle<Object> hresult;
  if (!result->ToHandle(&hresult, isolate)) return result;

  Handle<String> name = isolate->factory()->Uint32ToString(index);
  PropertyAttributes new_attributes = self->GetLocalElementAttribute(index);
  if (old_attributes == ABSENT) {
    if (self->IsJSArray() &&
        !old_length_handle->SameValue(Handle<JSArray>::cast(self)->length())) {
      new_length_handle = handle(Handle<JSArray>::cast(self)->length(),
                                 isolate);
      uint32_t old_length = 0;
      uint32_t new_length = 0;
      CHECK(old_length_handle->ToArrayIndex(&old_length));
      CHECK(new_length_handle->ToArrayIndex(&new_length));

      BeginPerformSplice(Handle<JSArray>::cast(self));
      EnqueueChangeRecord(self, "new", name, old_value);
      EnqueueChangeRecord(self, "updated", isolate->factory()->length_string(),
                          old_length_handle);
      EndPerformSplice(Handle<JSArray>::cast(self));
      Handle<JSArray> deleted = isolate->factory()->NewJSArray(0);
      EnqueueSpliceRecord(Handle<JSArray>::cast(self), old_length, deleted,
                          new_length - old_length);
    } else {
      EnqueueChangeRecord(self, "new", name, old_value);
    }
  } else if (old_value->IsTheHole()) {
    EnqueueChangeRecord(self, "reconfigured", name, old_value);
  } else {
    Handle<Object> new_value = Object::GetElement(isolate, self, index);
    bool value_changed = !old_value->SameValue(*new_value);
    if (old_attributes != new_attributes) {
      if (!value_changed) old_value = isolate->factory()->the_hole_value();
      EnqueueChangeRecord(self, "reconfigured", name, old_value);
    } else if (value_changed) {
      EnqueueChangeRecord(self, "updated", name, old_value);
    }
  }

  return *hresult;
}


MaybeObject* JSObject::SetElementWithoutInterceptor(uint32_t index,
                                                    Object* value,
                                                    PropertyAttributes attr,
                                                    StrictModeFlag strict_mode,
                                                    bool check_prototype,
                                                    SetPropertyMode set_mode) {
  ASSERT(HasDictionaryElements() ||
         HasDictionaryArgumentsElements() ||
         (attr & (DONT_DELETE | DONT_ENUM | READ_ONLY)) == 0);
  Isolate* isolate = GetIsolate();
  if (FLAG_trace_external_array_abuse &&
      IsExternalArrayElementsKind(GetElementsKind())) {
    CheckArrayAbuse(this, "external elements write", index);
  }
  if (FLAG_trace_js_array_abuse &&
      !IsExternalArrayElementsKind(GetElementsKind())) {
    if (IsJSArray()) {
      CheckArrayAbuse(this, "elements write", index, true);
    }
  }
  switch (GetElementsKind()) {
    case FAST_SMI_ELEMENTS:
    case FAST_ELEMENTS:
    case FAST_HOLEY_SMI_ELEMENTS:
    case FAST_HOLEY_ELEMENTS:
      return SetFastElement(index, value, strict_mode, check_prototype);
    case FAST_DOUBLE_ELEMENTS:
    case FAST_HOLEY_DOUBLE_ELEMENTS:
      return SetFastDoubleElement(index, value, strict_mode, check_prototype);
    case EXTERNAL_PIXEL_ELEMENTS: {
      ExternalPixelArray* pixels = ExternalPixelArray::cast(elements());
      return pixels->SetValue(index, value);
    }
    case EXTERNAL_BYTE_ELEMENTS: {
      ExternalByteArray* array = ExternalByteArray::cast(elements());
      return array->SetValue(index, value);
    }
    case EXTERNAL_UNSIGNED_BYTE_ELEMENTS: {
      ExternalUnsignedByteArray* array =
          ExternalUnsignedByteArray::cast(elements());
      return array->SetValue(index, value);
    }
    case EXTERNAL_SHORT_ELEMENTS: {
      ExternalShortArray* array = ExternalShortArray::cast(elements());
      return array->SetValue(index, value);
    }
    case EXTERNAL_UNSIGNED_SHORT_ELEMENTS: {
      ExternalUnsignedShortArray* array =
          ExternalUnsignedShortArray::cast(elements());
      return array->SetValue(index, value);
    }
    case EXTERNAL_INT_ELEMENTS: {
      ExternalIntArray* array = ExternalIntArray::cast(elements());
      return array->SetValue(index, value);
    }
    case EXTERNAL_UNSIGNED_INT_ELEMENTS: {
      ExternalUnsignedIntArray* array =
          ExternalUnsignedIntArray::cast(elements());
      return array->SetValue(index, value);
    }
    case EXTERNAL_FLOAT_ELEMENTS: {
      ExternalFloatArray* array = ExternalFloatArray::cast(elements());
      return array->SetValue(index, value);
    }
    case EXTERNAL_DOUBLE_ELEMENTS: {
      ExternalDoubleArray* array = ExternalDoubleArray::cast(elements());
      return array->SetValue(index, value);
    }
    case DICTIONARY_ELEMENTS:
      return SetDictionaryElement(index, value, attr, strict_mode,
                                  check_prototype, set_mode);
    case NON_STRICT_ARGUMENTS_ELEMENTS: {
      FixedArray* parameter_map = FixedArray::cast(elements());
      uint32_t length = parameter_map->length();
      Object* probe =
          (index < length - 2) ? parameter_map->get(index + 2) : NULL;
      if (probe != NULL && !probe->IsTheHole()) {
        Context* context = Context::cast(parameter_map->get(0));
        int context_index = Smi::cast(probe)->value();
        ASSERT(!context->get(context_index)->IsTheHole());
        context->set(context_index, value);
        // Redefining attributes of an aliased element destroys fast aliasing.
        if (set_mode == SET_PROPERTY || attr == NONE) return value;
        parameter_map->set_the_hole(index + 2);
        // For elements that are still writable we re-establish slow aliasing.
        if ((attr & READ_ONLY) == 0) {
          MaybeObject* maybe_entry =
              isolate->heap()->AllocateAliasedArgumentsEntry(context_index);
          if (!maybe_entry->ToObject(&value)) return maybe_entry;
        }
      }
      FixedArray* arguments = FixedArray::cast(parameter_map->get(1));
      if (arguments->IsDictionary()) {
        return SetDictionaryElement(index, value, attr, strict_mode,
                                    check_prototype, set_mode);
      } else {
        return SetFastElement(index, value, strict_mode, check_prototype);
      }
    }
  }
  // All possible cases have been handled above. Add a return to avoid the
  // complaints from the compiler.
  UNREACHABLE();
  return isolate->heap()->null_value();
}


Handle<Object> JSObject::TransitionElementsKind(Handle<JSObject> object,
                                                ElementsKind to_kind) {
  CALL_HEAP_FUNCTION(object->GetIsolate(),
                     object->TransitionElementsKind(to_kind),
                     Object);
}


MaybeObject* JSObject::UpdateAllocationSite(ElementsKind to_kind) {
  if (!FLAG_track_allocation_sites || !IsJSArray()) {
    return this;
  }

  AllocationMemento* memento = AllocationMemento::FindForJSObject(this);
  if (memento == NULL || !memento->IsValid()) {
    return this;
  }

  // Walk through to the Allocation Site
  AllocationSite* site = memento->GetAllocationSite();
  if (site->IsLiteralSite()) {
    JSArray* transition_info = JSArray::cast(site->transition_info());
    ElementsKind kind = transition_info->GetElementsKind();
    // if kind is holey ensure that to_kind is as well.
    if (IsHoleyElementsKind(kind)) {
      to_kind = GetHoleyElementsKind(to_kind);
    }
    if (AllocationSite::GetMode(kind, to_kind) == TRACK_ALLOCATION_SITE) {
      // If the array is huge, it's not likely to be defined in a local
      // function, so we shouldn't make new instances of it very often.
      uint32_t length = 0;
      CHECK(transition_info->length()->ToArrayIndex(&length));
      if (length <= AllocationSite::kMaximumArrayBytesToPretransition) {
        if (FLAG_trace_track_allocation_sites) {
          PrintF(
              "AllocationSite: JSArray %p boilerplate updated %s->%s\n",
              reinterpret_cast<void*>(this),
              ElementsKindToString(kind),
              ElementsKindToString(to_kind));
        }
        return transition_info->TransitionElementsKind(to_kind);
      }
    }
  } else {
    ElementsKind kind = site->GetElementsKind();
    // if kind is holey ensure that to_kind is as well.
    if (IsHoleyElementsKind(kind)) {
      to_kind = GetHoleyElementsKind(to_kind);
    }
    if (AllocationSite::GetMode(kind, to_kind) == TRACK_ALLOCATION_SITE) {
      if (FLAG_trace_track_allocation_sites) {
        PrintF("AllocationSite: JSArray %p site updated %s->%s\n",
               reinterpret_cast<void*>(this),
               ElementsKindToString(kind),
               ElementsKindToString(to_kind));
      }
      site->set_transition_info(Smi::FromInt(to_kind));
    }
  }
  return this;
}


MaybeObject* JSObject::TransitionElementsKind(ElementsKind to_kind) {
  ASSERT(!map()->is_observed());
  ElementsKind from_kind = map()->elements_kind();

  if (IsFastHoleyElementsKind(from_kind)) {
    to_kind = GetHoleyElementsKind(to_kind);
  }

  if (from_kind == to_kind) return this;

  MaybeObject* maybe_failure = UpdateAllocationSite(to_kind);
  if (maybe_failure->IsFailure()) return maybe_failure;

  Isolate* isolate = GetIsolate();
  if (elements() == isolate->heap()->empty_fixed_array() ||
      (IsFastSmiOrObjectElementsKind(from_kind) &&
       IsFastSmiOrObjectElementsKind(to_kind)) ||
      (from_kind == FAST_DOUBLE_ELEMENTS &&
       to_kind == FAST_HOLEY_DOUBLE_ELEMENTS)) {
    ASSERT(from_kind != TERMINAL_FAST_ELEMENTS_KIND);
    // No change is needed to the elements() buffer, the transition
    // only requires a map change.
    MaybeObject* maybe_new_map = GetElementsTransitionMap(isolate, to_kind);
    Map* new_map;
    if (!maybe_new_map->To(&new_map)) return maybe_new_map;
    set_map(new_map);
    if (FLAG_trace_elements_transitions) {
      FixedArrayBase* elms = FixedArrayBase::cast(elements());
      PrintElementsTransition(stdout, from_kind, elms, to_kind, elms);
    }
    return this;
  }

  FixedArrayBase* elms = FixedArrayBase::cast(elements());
  uint32_t capacity = static_cast<uint32_t>(elms->length());
  uint32_t length = capacity;

  if (IsJSArray()) {
    Object* raw_length = JSArray::cast(this)->length();
    if (raw_length->IsUndefined()) {
      // If length is undefined, then JSArray is being initialized and has no
      // elements, assume a length of zero.
      length = 0;
    } else {
      CHECK(JSArray::cast(this)->length()->ToArrayIndex(&length));
    }
  }

  if (IsFastSmiElementsKind(from_kind) &&
      IsFastDoubleElementsKind(to_kind)) {
    MaybeObject* maybe_result =
        SetFastDoubleElementsCapacityAndLength(capacity, length);
    if (maybe_result->IsFailure()) return maybe_result;
    ValidateElements();
    return this;
  }

  if (IsFastDoubleElementsKind(from_kind) &&
      IsFastObjectElementsKind(to_kind)) {
    MaybeObject* maybe_result = SetFastElementsCapacityAndLength(
        capacity, length, kDontAllowSmiElements);
    if (maybe_result->IsFailure()) return maybe_result;
    ValidateElements();
    return this;
  }

  // This method should never be called for any other case than the ones
  // handled above.
  UNREACHABLE();
  return GetIsolate()->heap()->null_value();
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


MaybeObject* JSArray::JSArrayUpdateLengthFromIndex(uint32_t index,
                                                   Object* value) {
  uint32_t old_len = 0;
  CHECK(length()->ToArrayIndex(&old_len));
  // Check to see if we need to update the length. For now, we make
  // sure that the length stays within 32-bits (unsigned).
  if (index >= old_len && index != 0xffffffff) {
    Object* len;
    { MaybeObject* maybe_len =
          GetHeap()->NumberFromDouble(static_cast<double>(index) + 1);
      if (!maybe_len->ToObject(&len)) return maybe_len;
    }
    set_length(len);
  }
  return value;
}


MaybeObject* JSObject::GetElementWithInterceptor(Object* receiver,
                                                 uint32_t index) {
  Isolate* isolate = GetIsolate();
  HandleScope scope(isolate);

  // Make sure that the top context does not change when doing
  // callbacks or interceptor calls.
  AssertNoContextChange ncc;

  Handle<InterceptorInfo> interceptor(GetIndexedInterceptor(), isolate);
  Handle<Object> this_handle(receiver, isolate);
  Handle<JSObject> holder_handle(this, isolate);
  if (!interceptor->getter()->IsUndefined()) {
    v8::IndexedPropertyGetterCallback getter =
        v8::ToCData<v8::IndexedPropertyGetterCallback>(interceptor->getter());
    LOG(isolate,
        ApiIndexedPropertyAccess("interceptor-indexed-get", this, index));
    PropertyCallbackArguments
        args(isolate, interceptor->data(), receiver, this);
    v8::Handle<v8::Value> result = args.Call(getter, index);
    RETURN_IF_SCHEDULED_EXCEPTION(isolate);
    if (!result.IsEmpty()) {
      Handle<Object> result_internal = v8::Utils::OpenHandle(*result);
      result_internal->VerifyApiCallResultType();
      return *result_internal;
    }
  }

  Heap* heap = holder_handle->GetHeap();
  ElementsAccessor* handler = holder_handle->GetElementsAccessor();
  MaybeObject* raw_result = handler->Get(*this_handle,
                                         *holder_handle,
                                         index);
  if (raw_result != heap->the_hole_value()) return raw_result;

  RETURN_IF_SCHEDULED_EXCEPTION(isolate);

  Object* pt = holder_handle->GetPrototype();
  if (pt == heap->null_value()) return heap->undefined_value();
  return pt->GetElementWithReceiver(isolate, *this_handle, index);
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
    case NON_STRICT_ARGUMENTS_ELEMENTS:
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
      SeededNumberDictionary* dictionary =
          SeededNumberDictionary::cast(FixedArray::cast(elements()));
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
      FixedDoubleArray* elms = FixedDoubleArray::cast(elements());
      *capacity = elms->length();
      for (int i = 0; i < *capacity; i++) {
        if (!elms->is_the_hole(i)) ++(*used);
      }
      break;
    }
    case EXTERNAL_BYTE_ELEMENTS:
    case EXTERNAL_UNSIGNED_BYTE_ELEMENTS:
    case EXTERNAL_SHORT_ELEMENTS:
    case EXTERNAL_UNSIGNED_SHORT_ELEMENTS:
    case EXTERNAL_INT_ELEMENTS:
    case EXTERNAL_UNSIGNED_INT_ELEMENTS:
    case EXTERNAL_FLOAT_ELEMENTS:
    case EXTERNAL_DOUBLE_ELEMENTS:
    case EXTERNAL_PIXEL_ELEMENTS:
      // External arrays are considered 100% used.
      ExternalArray* external_array = ExternalArray::cast(elements());
      *capacity = external_array->length();
      *used = external_array->length();
      break;
  }
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
  ASSERT(HasDictionaryElements() || HasDictionaryArgumentsElements());
  // If the elements are sparse, we should not go back to fast case.
  if (!HasDenseElements()) return false;
  // An object requiring access checks is never allowed to have fast
  // elements.  If it had fast elements we would skip security checks.
  if (IsAccessCheckNeeded()) return false;
  // Observed objects may not go to fast mode because they rely on map checks,
  // and for fast element accesses we sometimes check element kinds only.
  if (FLAG_harmony_observation && map()->is_observed()) return false;

  FixedArray* elements = FixedArray::cast(this->elements());
  SeededNumberDictionary* dictionary = NULL;
  if (elements->map() == GetHeap()->non_strict_arguments_elements_map()) {
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
  if (FLAG_unbox_double_arrays) {
    ASSERT(HasDictionaryElements());
    SeededNumberDictionary* dictionary =
        SeededNumberDictionary::cast(elements());
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
template<typename Shape, typename Key>
void Dictionary<Shape, Key>::Print(FILE* out) {
  int capacity = HashTable<Shape, Key>::Capacity();
  for (int i = 0; i < capacity; i++) {
    Object* k = HashTable<Shape, Key>::KeyAt(i);
    if (HashTable<Shape, Key>::IsKey(k)) {
      PrintF(out, " ");
      if (k->IsString()) {
        String::cast(k)->StringPrint(out);
      } else {
        k->ShortPrint(out);
      }
      PrintF(out, ": ");
      ValueAt(i)->ShortPrint(out);
      PrintF(out, "\n");
    }
  }
}
#endif


template<typename Shape, typename Key>
void Dictionary<Shape, Key>::CopyValuesTo(FixedArray* elements) {
  int pos = 0;
  int capacity = HashTable<Shape, Key>::Capacity();
  DisallowHeapAllocation no_gc;
  WriteBarrierMode mode = elements->GetWriteBarrierMode(no_gc);
  for (int i = 0; i < capacity; i++) {
    Object* k =  Dictionary<Shape, Key>::KeyAt(i);
    if (Dictionary<Shape, Key>::IsKey(k)) {
      elements->set(pos++, ValueAt(i), mode);
    }
  }
  ASSERT(pos == elements->length());
}


InterceptorInfo* JSObject::GetNamedInterceptor() {
  ASSERT(map()->has_named_interceptor());
  JSFunction* constructor = JSFunction::cast(map()->constructor());
  ASSERT(constructor->shared()->IsApiFunction());
  Object* result =
      constructor->shared()->get_api_func_data()->named_property_handler();
  return InterceptorInfo::cast(result);
}


InterceptorInfo* JSObject::GetIndexedInterceptor() {
  ASSERT(map()->has_indexed_interceptor());
  JSFunction* constructor = JSFunction::cast(map()->constructor());
  ASSERT(constructor->shared()->IsApiFunction());
  Object* result =
      constructor->shared()->get_api_func_data()->indexed_property_handler();
  return InterceptorInfo::cast(result);
}


MaybeObject* JSObject::GetPropertyPostInterceptor(
    Object* receiver,
    Name* name,
    PropertyAttributes* attributes) {
  // Check local property in holder, ignore interceptor.
  LookupResult result(GetIsolate());
  LocalLookupRealNamedProperty(name, &result);
  if (result.IsFound()) {
    return GetProperty(receiver, &result, name, attributes);
  }
  // Continue searching via the prototype chain.
  Object* pt = GetPrototype();
  *attributes = ABSENT;
  if (pt->IsNull()) return GetHeap()->undefined_value();
  return pt->GetPropertyWithReceiver(receiver, name, attributes);
}


MaybeObject* JSObject::GetLocalPropertyPostInterceptor(
    Object* receiver,
    Name* name,
    PropertyAttributes* attributes) {
  // Check local property in holder, ignore interceptor.
  LookupResult result(GetIsolate());
  LocalLookupRealNamedProperty(name, &result);
  if (result.IsFound()) {
    return GetProperty(receiver, &result, name, attributes);
  }
  return GetHeap()->undefined_value();
}


MaybeObject* JSObject::GetPropertyWithInterceptor(
    Object* receiver,
    Name* name,
    PropertyAttributes* attributes) {
  // TODO(rossberg): Support symbols in the API.
  if (name->IsSymbol()) return GetHeap()->undefined_value();

  Isolate* isolate = GetIsolate();
  InterceptorInfo* interceptor = GetNamedInterceptor();
  HandleScope scope(isolate);
  Handle<Object> receiver_handle(receiver, isolate);
  Handle<JSObject> holder_handle(this);
  Handle<String> name_handle(String::cast(name));

  if (!interceptor->getter()->IsUndefined()) {
    v8::NamedPropertyGetterCallback getter =
        v8::ToCData<v8::NamedPropertyGetterCallback>(interceptor->getter());
    LOG(isolate,
        ApiNamedPropertyAccess("interceptor-named-get", *holder_handle, name));
    PropertyCallbackArguments
        args(isolate, interceptor->data(), receiver, this);
    v8::Handle<v8::Value> result =
        args.Call(getter, v8::Utils::ToLocal(name_handle));
    RETURN_IF_SCHEDULED_EXCEPTION(isolate);
    if (!result.IsEmpty()) {
      *attributes = NONE;
      Handle<Object> result_internal = v8::Utils::OpenHandle(*result);
      result_internal->VerifyApiCallResultType();
      return *result_internal;
    }
  }

  MaybeObject* result = holder_handle->GetPropertyPostInterceptor(
      *receiver_handle,
      *name_handle,
      attributes);
  RETURN_IF_SCHEDULED_EXCEPTION(isolate);
  return result;
}


bool JSObject::HasRealNamedProperty(Isolate* isolate, Name* key) {
  // Check access rights if needed.
  if (IsAccessCheckNeeded()) {
    if (!isolate->MayNamedAccess(this, key, v8::ACCESS_HAS)) {
      isolate->ReportFailedAccessCheck(this, v8::ACCESS_HAS);
      return false;
    }
  }

  LookupResult result(isolate);
  LocalLookupRealNamedProperty(key, &result);
  return result.IsFound() && !result.IsInterceptor();
}


bool JSObject::HasRealElementProperty(Isolate* isolate, uint32_t index) {
  // Check access rights if needed.
  if (IsAccessCheckNeeded()) {
    if (!isolate->MayIndexedAccess(this, index, v8::ACCESS_HAS)) {
      isolate->ReportFailedAccessCheck(this, v8::ACCESS_HAS);
      return false;
    }
  }

  if (IsJSGlobalProxy()) {
    Object* proto = GetPrototype();
    if (proto->IsNull()) return false;
    ASSERT(proto->IsJSGlobalObject());
    return JSObject::cast(proto)->HasRealElementProperty(isolate, index);
  }

  return GetElementAttributeWithoutInterceptor(this, index, false) != ABSENT;
}


bool JSObject::HasRealNamedCallbackProperty(Isolate* isolate, Name* key) {
  // Check access rights if needed.
  if (IsAccessCheckNeeded()) {
    if (!isolate->MayNamedAccess(this, key, v8::ACCESS_HAS)) {
      isolate->ReportFailedAccessCheck(this, v8::ACCESS_HAS);
      return false;
    }
  }

  LookupResult result(isolate);
  LocalLookupRealNamedProperty(key, &result);
  return result.IsPropertyCallbacks();
}


int JSObject::NumberOfLocalProperties(PropertyAttributes filter) {
  if (HasFastProperties()) {
    Map* map = this->map();
    if (filter == NONE) return map->NumberOfOwnDescriptors();
    if (filter & DONT_ENUM) {
      int result = map->EnumLength();
      if (result != Map::kInvalidEnumCache) return result;
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
  ASSERT(content->length() == numbers->length());

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
  ASSERT(this->length() == numbers->length());
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


// Fill in the names of local properties into the supplied storage. The main
// purpose of this function is to provide reflection information for the object
// mirrors.
void JSObject::GetLocalPropertyNames(
    FixedArray* storage, int index, PropertyAttributes filter) {
  ASSERT(storage->length() >= (NumberOfLocalProperties(filter) - index));
  if (HasFastProperties()) {
    int real_size = map()->NumberOfOwnDescriptors();
    DescriptorArray* descs = map()->instance_descriptors();
    for (int i = 0; i < real_size; i++) {
      if ((descs->GetDetails(i).attributes() & filter) == 0 &&
          ((filter & SYMBOLIC) == 0 || !descs->GetKey(i)->IsSymbol())) {
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


int JSObject::NumberOfLocalElements(PropertyAttributes filter) {
  return GetLocalElementKeys(NULL, filter);
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
  return NumberOfLocalElements(static_cast<PropertyAttributes>(DONT_ENUM));
}


int JSObject::GetLocalElementKeys(FixedArray* storage,
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
      ASSERT(!storage || storage->length() >= counter);
      break;
    }
    case FAST_DOUBLE_ELEMENTS:
    case FAST_HOLEY_DOUBLE_ELEMENTS: {
      int length = IsJSArray() ?
          Smi::cast(JSArray::cast(this)->length())->value() :
          FixedDoubleArray::cast(elements())->length();
      for (int i = 0; i < length; i++) {
        if (!FixedDoubleArray::cast(elements())->is_the_hole(i)) {
          if (storage != NULL) {
            storage->set(counter, Smi::FromInt(i));
          }
          counter++;
        }
      }
      ASSERT(!storage || storage->length() >= counter);
      break;
    }
    case EXTERNAL_PIXEL_ELEMENTS: {
      int length = ExternalPixelArray::cast(elements())->length();
      while (counter < length) {
        if (storage != NULL) {
          storage->set(counter, Smi::FromInt(counter));
        }
        counter++;
      }
      ASSERT(!storage || storage->length() >= counter);
      break;
    }
    case EXTERNAL_BYTE_ELEMENTS:
    case EXTERNAL_UNSIGNED_BYTE_ELEMENTS:
    case EXTERNAL_SHORT_ELEMENTS:
    case EXTERNAL_UNSIGNED_SHORT_ELEMENTS:
    case EXTERNAL_INT_ELEMENTS:
    case EXTERNAL_UNSIGNED_INT_ELEMENTS:
    case EXTERNAL_FLOAT_ELEMENTS:
    case EXTERNAL_DOUBLE_ELEMENTS: {
      int length = ExternalArray::cast(elements())->length();
      while (counter < length) {
        if (storage != NULL) {
          storage->set(counter, Smi::FromInt(counter));
        }
        counter++;
      }
      ASSERT(!storage || storage->length() >= counter);
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
    case NON_STRICT_ARGUMENTS_ELEMENTS: {
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
  ASSERT(!storage || storage->length() == counter);
  return counter;
}


int JSObject::GetEnumElementKeys(FixedArray* storage) {
  return GetLocalElementKeys(storage,
                             static_cast<PropertyAttributes>(DONT_ENUM));
}


// StringKey simply carries a string object as key.
class StringKey : public HashTableKey {
 public:
  explicit StringKey(String* string) :
      string_(string),
      hash_(HashForObject(string)) { }

  bool IsMatch(Object* string) {
    // We know that all entries in a hash table had their hash keys created.
    // Use that knowledge to have fast failure.
    if (hash_ != HashForObject(string)) {
      return false;
    }
    return string_->Equals(String::cast(string));
  }

  uint32_t Hash() { return hash_; }

  uint32_t HashForObject(Object* other) { return String::cast(other)->Hash(); }

  Object* AsObject(Heap* heap) { return string_; }

  String* string_;
  uint32_t hash_;
};


// StringSharedKeys are used as keys in the eval cache.
class StringSharedKey : public HashTableKey {
 public:
  StringSharedKey(String* source,
                  SharedFunctionInfo* shared,
                  LanguageMode language_mode,
                  int scope_position)
      : source_(source),
        shared_(shared),
        language_mode_(language_mode),
        scope_position_(scope_position) { }

  bool IsMatch(Object* other) {
    if (!other->IsFixedArray()) return false;
    FixedArray* other_array = FixedArray::cast(other);
    SharedFunctionInfo* shared = SharedFunctionInfo::cast(other_array->get(0));
    if (shared != shared_) return false;
    int language_unchecked = Smi::cast(other_array->get(2))->value();
    ASSERT(language_unchecked == CLASSIC_MODE ||
           language_unchecked == STRICT_MODE ||
           language_unchecked == EXTENDED_MODE);
    LanguageMode language_mode = static_cast<LanguageMode>(language_unchecked);
    if (language_mode != language_mode_) return false;
    int scope_position = Smi::cast(other_array->get(3))->value();
    if (scope_position != scope_position_) return false;
    String* source = String::cast(other_array->get(1));
    return source->Equals(source_);
  }

  static uint32_t StringSharedHashHelper(String* source,
                                         SharedFunctionInfo* shared,
                                         LanguageMode language_mode,
                                         int scope_position) {
    uint32_t hash = source->Hash();
    if (shared->HasSourceCode()) {
      // Instead of using the SharedFunctionInfo pointer in the hash
      // code computation, we use a combination of the hash of the
      // script source code and the start position of the calling scope.
      // We do this to ensure that the cache entries can survive garbage
      // collection.
      Script* script = Script::cast(shared->script());
      hash ^= String::cast(script->source())->Hash();
      if (language_mode == STRICT_MODE) hash ^= 0x8000;
      if (language_mode == EXTENDED_MODE) hash ^= 0x0080;
      hash += scope_position;
    }
    return hash;
  }

  uint32_t Hash() {
    return StringSharedHashHelper(
        source_, shared_, language_mode_, scope_position_);
  }

  uint32_t HashForObject(Object* obj) {
    FixedArray* other_array = FixedArray::cast(obj);
    SharedFunctionInfo* shared = SharedFunctionInfo::cast(other_array->get(0));
    String* source = String::cast(other_array->get(1));
    int language_unchecked = Smi::cast(other_array->get(2))->value();
    ASSERT(language_unchecked == CLASSIC_MODE ||
           language_unchecked == STRICT_MODE ||
           language_unchecked == EXTENDED_MODE);
    LanguageMode language_mode = static_cast<LanguageMode>(language_unchecked);
    int scope_position = Smi::cast(other_array->get(3))->value();
    return StringSharedHashHelper(
        source, shared, language_mode, scope_position);
  }

  MUST_USE_RESULT MaybeObject* AsObject(Heap* heap) {
    Object* obj;
    { MaybeObject* maybe_obj = heap->AllocateFixedArray(4);
      if (!maybe_obj->ToObject(&obj)) return maybe_obj;
    }
    FixedArray* other_array = FixedArray::cast(obj);
    other_array->set(0, shared_);
    other_array->set(1, source_);
    other_array->set(2, Smi::FromInt(language_mode_));
    other_array->set(3, Smi::FromInt(scope_position_));
    return other_array;
  }

 private:
  String* source_;
  SharedFunctionInfo* shared_;
  LanguageMode language_mode_;
  int scope_position_;
};


// RegExpKey carries the source and flags of a regular expression as key.
class RegExpKey : public HashTableKey {
 public:
  RegExpKey(String* string, JSRegExp::Flags flags)
      : string_(string),
        flags_(Smi::FromInt(flags.value())) { }

  // Rather than storing the key in the hash table, a pointer to the
  // stored value is stored where the key should be.  IsMatch then
  // compares the search key to the found object, rather than comparing
  // a key to a key.
  bool IsMatch(Object* obj) {
    FixedArray* val = FixedArray::cast(obj);
    return string_->Equals(String::cast(val->get(JSRegExp::kSourceIndex)))
        && (flags_ == val->get(JSRegExp::kFlagsIndex));
  }

  uint32_t Hash() { return RegExpHash(string_, flags_); }

  Object* AsObject(Heap* heap) {
    // Plain hash maps, which is where regexp keys are used, don't
    // use this function.
    UNREACHABLE();
    return NULL;
  }

  uint32_t HashForObject(Object* obj) {
    FixedArray* val = FixedArray::cast(obj);
    return RegExpHash(String::cast(val->get(JSRegExp::kSourceIndex)),
                      Smi::cast(val->get(JSRegExp::kFlagsIndex)));
  }

  static uint32_t RegExpHash(String* string, Smi* flags) {
    return string->Hash() + flags->value();
  }

  String* string_;
  Smi* flags_;
};


// Utf8StringKey carries a vector of chars as key.
class Utf8StringKey : public HashTableKey {
 public:
  explicit Utf8StringKey(Vector<const char> string, uint32_t seed)
      : string_(string), hash_field_(0), seed_(seed) { }

  bool IsMatch(Object* string) {
    return String::cast(string)->IsUtf8EqualTo(string_);
  }

  uint32_t Hash() {
    if (hash_field_ != 0) return hash_field_ >> String::kHashShift;
    hash_field_ = StringHasher::ComputeUtf8Hash(string_, seed_, &chars_);
    uint32_t result = hash_field_ >> String::kHashShift;
    ASSERT(result != 0);  // Ensure that the hash value of 0 is never computed.
    return result;
  }

  uint32_t HashForObject(Object* other) {
    return String::cast(other)->Hash();
  }

  MaybeObject* AsObject(Heap* heap) {
    if (hash_field_ == 0) Hash();
    return heap->AllocateInternalizedStringFromUtf8(string_,
                                                    chars_,
                                                    hash_field_);
  }

  Vector<const char> string_;
  uint32_t hash_field_;
  int chars_;  // Caches the number of characters when computing the hash code.
  uint32_t seed_;
};


template <typename Char>
class SequentialStringKey : public HashTableKey {
 public:
  explicit SequentialStringKey(Vector<const Char> string, uint32_t seed)
      : string_(string), hash_field_(0), seed_(seed) { }

  uint32_t Hash() {
    hash_field_ = StringHasher::HashSequentialString<Char>(string_.start(),
                                                           string_.length(),
                                                           seed_);

    uint32_t result = hash_field_ >> String::kHashShift;
    ASSERT(result != 0);  // Ensure that the hash value of 0 is never computed.
    return result;
  }


  uint32_t HashForObject(Object* other) {
    return String::cast(other)->Hash();
  }

  Vector<const Char> string_;
  uint32_t hash_field_;
  uint32_t seed_;
};



class OneByteStringKey : public SequentialStringKey<uint8_t> {
 public:
  OneByteStringKey(Vector<const uint8_t> str, uint32_t seed)
      : SequentialStringKey<uint8_t>(str, seed) { }

  bool IsMatch(Object* string) {
    return String::cast(string)->IsOneByteEqualTo(string_);
  }

  MaybeObject* AsObject(Heap* heap) {
    if (hash_field_ == 0) Hash();
    return heap->AllocateOneByteInternalizedString(string_, hash_field_);
  }
};


class SubStringOneByteStringKey : public HashTableKey {
 public:
  explicit SubStringOneByteStringKey(Handle<SeqOneByteString> string,
                                     int from,
                                     int length)
      : string_(string), from_(from), length_(length) { }

  uint32_t Hash() {
    ASSERT(length_ >= 0);
    ASSERT(from_ + length_ <= string_->length());
    uint8_t* chars = string_->GetChars() + from_;
    hash_field_ = StringHasher::HashSequentialString(
        chars, length_, string_->GetHeap()->HashSeed());
    uint32_t result = hash_field_ >> String::kHashShift;
    ASSERT(result != 0);  // Ensure that the hash value of 0 is never computed.
    return result;
  }


  uint32_t HashForObject(Object* other) {
    return String::cast(other)->Hash();
  }

  bool IsMatch(Object* string) {
    Vector<const uint8_t> chars(string_->GetChars() + from_, length_);
    return String::cast(string)->IsOneByteEqualTo(chars);
  }

  MaybeObject* AsObject(Heap* heap) {
    if (hash_field_ == 0) Hash();
    Vector<const uint8_t> chars(string_->GetChars() + from_, length_);
    return heap->AllocateOneByteInternalizedString(chars, hash_field_);
  }

 private:
  Handle<SeqOneByteString> string_;
  int from_;
  int length_;
  uint32_t hash_field_;
};


class TwoByteStringKey : public SequentialStringKey<uc16> {
 public:
  explicit TwoByteStringKey(Vector<const uc16> str, uint32_t seed)
      : SequentialStringKey<uc16>(str, seed) { }

  bool IsMatch(Object* string) {
    return String::cast(string)->IsTwoByteEqualTo(string_);
  }

  MaybeObject* AsObject(Heap* heap) {
    if (hash_field_ == 0) Hash();
    return heap->AllocateTwoByteInternalizedString(string_, hash_field_);
  }
};


// InternalizedStringKey carries a string/internalized-string object as key.
class InternalizedStringKey : public HashTableKey {
 public:
  explicit InternalizedStringKey(String* string)
      : string_(string) { }

  bool IsMatch(Object* string) {
    return String::cast(string)->Equals(string_);
  }

  uint32_t Hash() { return string_->Hash(); }

  uint32_t HashForObject(Object* other) {
    return String::cast(other)->Hash();
  }

  MaybeObject* AsObject(Heap* heap) {
    // Attempt to flatten the string, so that internalized strings will most
    // often be flat strings.
    string_ = string_->TryFlattenGetString();
    // Internalize the string if possible.
    Map* map = heap->InternalizedStringMapForString(string_);
    if (map != NULL) {
      string_->set_map_no_write_barrier(map);
      ASSERT(string_->IsInternalizedString());
      return string_;
    }
    // Otherwise allocate a new internalized string.
    return heap->AllocateInternalizedStringImpl(
        string_, string_->length(), string_->hash_field());
  }

  static uint32_t StringHash(Object* obj) {
    return String::cast(obj)->Hash();
  }

  String* string_;
};


template<typename Shape, typename Key>
void HashTable<Shape, Key>::IteratePrefix(ObjectVisitor* v) {
  IteratePointers(v, 0, kElementsStartOffset);
}


template<typename Shape, typename Key>
void HashTable<Shape, Key>::IterateElements(ObjectVisitor* v) {
  IteratePointers(v,
                  kElementsStartOffset,
                  kHeaderSize + length() * kPointerSize);
}


template<typename Shape, typename Key>
MaybeObject* HashTable<Shape, Key>::Allocate(Heap* heap,
                                             int at_least_space_for,
                                             MinimumCapacity capacity_option,
                                             PretenureFlag pretenure) {
  ASSERT(!capacity_option || IS_POWER_OF_TWO(at_least_space_for));
  int capacity = (capacity_option == USE_CUSTOM_MINIMUM_CAPACITY)
                     ? at_least_space_for
                     : ComputeCapacity(at_least_space_for);
  if (capacity > HashTable::kMaxCapacity) {
    return Failure::OutOfMemoryException(0x10);
  }

  Object* obj;
  { MaybeObject* maybe_obj =
        heap-> AllocateHashTable(EntryToIndex(capacity), pretenure);
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
  }
  HashTable::cast(obj)->SetNumberOfElements(0);
  HashTable::cast(obj)->SetNumberOfDeletedElements(0);
  HashTable::cast(obj)->SetCapacity(capacity);
  return obj;
}


// Find entry for key otherwise return kNotFound.
int NameDictionary::FindEntry(Name* key) {
  if (!key->IsUniqueName()) {
    return HashTable<NameDictionaryShape, Name*>::FindEntry(key);
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
    if (key == element) return entry;
    if (!element->IsUniqueName() &&
        !element->IsTheHole() &&
        Name::cast(element)->Equals(key)) {
      // Replace a key that is a non-internalized string by the equivalent
      // internalized string for faster further lookups.
      set(index, key);
      return entry;
    }
    ASSERT(element->IsTheHole() || !Name::cast(element)->Equals(key));
    entry = NextProbe(entry, count++, capacity);
  }
  return kNotFound;
}


template<typename Shape, typename Key>
MaybeObject* HashTable<Shape, Key>::Rehash(HashTable* new_table, Key key) {
  ASSERT(NumberOfElements() < new_table->Capacity());

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
      uint32_t hash = HashTable<Shape, Key>::HashForObject(key, k);
      uint32_t insertion_index =
          EntryToIndex(new_table->FindInsertionEntry(hash));
      for (int j = 0; j < Shape::kEntrySize; j++) {
        new_table->set(insertion_index + j, get(from_index + j), mode);
      }
    }
  }
  new_table->SetNumberOfElements(NumberOfElements());
  new_table->SetNumberOfDeletedElements(0);
  return new_table;
}


template<typename Shape, typename Key>
uint32_t HashTable<Shape, Key>::EntryForProbe(Key key,
                                              Object* k,
                                              int probe,
                                              uint32_t expected) {
  uint32_t hash = HashTable<Shape, Key>::HashForObject(key, k);
  uint32_t capacity = Capacity();
  uint32_t entry = FirstProbe(hash, capacity);
  for (int i = 1; i < probe; i++) {
    if (entry == expected) return expected;
    entry = NextProbe(entry, i, capacity);
  }
  return entry;
}


template<typename Shape, typename Key>
void HashTable<Shape, Key>::Swap(uint32_t entry1,
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


template<typename Shape, typename Key>
void HashTable<Shape, Key>::Rehash(Key key) {
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


template<typename Shape, typename Key>
MaybeObject* HashTable<Shape, Key>::EnsureCapacity(int n, Key key) {
  int capacity = Capacity();
  int nof = NumberOfElements() + n;
  int nod = NumberOfDeletedElements();
  // Return if:
  //   50% is still free after adding n elements and
  //   at most 50% of the free elements are deleted elements.
  if (nod <= (capacity - nof) >> 1) {
    int needed_free = nof >> 1;
    if (nof + needed_free <= capacity) return this;
  }

  const int kMinCapacityForPretenure = 256;
  bool pretenure =
      (capacity > kMinCapacityForPretenure) && !GetHeap()->InNewSpace(this);
  Object* obj;
  { MaybeObject* maybe_obj =
        Allocate(GetHeap(),
                 nof * 2,
                 USE_DEFAULT_MINIMUM_CAPACITY,
                 pretenure ? TENURED : NOT_TENURED);
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
  }

  return Rehash(HashTable::cast(obj), key);
}


template<typename Shape, typename Key>
MaybeObject* HashTable<Shape, Key>::Shrink(Key key) {
  int capacity = Capacity();
  int nof = NumberOfElements();

  // Shrink to fit the number of elements if only a quarter of the
  // capacity is filled with elements.
  if (nof > (capacity >> 2)) return this;
  // Allocate a new dictionary with room for at least the current
  // number of elements. The allocation method will make sure that
  // there is extra room in the dictionary for additions. Don't go
  // lower than room for 16 elements.
  int at_least_room_for = nof;
  if (at_least_room_for < 16) return this;

  const int kMinCapacityForPretenure = 256;
  bool pretenure =
      (at_least_room_for > kMinCapacityForPretenure) &&
      !GetHeap()->InNewSpace(this);
  Object* obj;
  { MaybeObject* maybe_obj =
        Allocate(GetHeap(),
                 at_least_room_for,
                 USE_DEFAULT_MINIMUM_CAPACITY,
                 pretenure ? TENURED : NOT_TENURED);
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
  }

  return Rehash(HashTable::cast(obj), key);
}


template<typename Shape, typename Key>
uint32_t HashTable<Shape, Key>::FindInsertionEntry(uint32_t hash) {
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

template class HashTable<StringTableShape, HashTableKey*>;

template class HashTable<CompilationCacheShape, HashTableKey*>;

template class HashTable<MapCacheShape, HashTableKey*>;

template class HashTable<ObjectHashTableShape<1>, Object*>;

template class HashTable<ObjectHashTableShape<2>, Object*>;

template class Dictionary<NameDictionaryShape, Name*>;

template class Dictionary<SeededNumberDictionaryShape, uint32_t>;

template class Dictionary<UnseededNumberDictionaryShape, uint32_t>;

template MaybeObject* Dictionary<SeededNumberDictionaryShape, uint32_t>::
    Allocate(Heap* heap, int at_least_space_for, PretenureFlag pretenure);

template MaybeObject* Dictionary<UnseededNumberDictionaryShape, uint32_t>::
    Allocate(Heap* heap, int at_least_space_for, PretenureFlag pretenure);

template MaybeObject* Dictionary<NameDictionaryShape, Name*>::
    Allocate(Heap* heap, int n, PretenureFlag pretenure);

template MaybeObject* Dictionary<SeededNumberDictionaryShape, uint32_t>::AtPut(
    uint32_t, Object*);

template MaybeObject* Dictionary<UnseededNumberDictionaryShape, uint32_t>::
    AtPut(uint32_t, Object*);

template Object* Dictionary<SeededNumberDictionaryShape, uint32_t>::
    SlowReverseLookup(Object* value);

template Object* Dictionary<UnseededNumberDictionaryShape, uint32_t>::
    SlowReverseLookup(Object* value);

template Object* Dictionary<NameDictionaryShape, Name*>::SlowReverseLookup(
    Object*);

template void Dictionary<SeededNumberDictionaryShape, uint32_t>::CopyKeysTo(
    FixedArray*,
    PropertyAttributes,
    Dictionary<SeededNumberDictionaryShape, uint32_t>::SortMode);

template Object* Dictionary<NameDictionaryShape, Name*>::DeleteProperty(
    int, JSObject::DeleteMode);

template Object* Dictionary<SeededNumberDictionaryShape, uint32_t>::
    DeleteProperty(int, JSObject::DeleteMode);

template MaybeObject* Dictionary<NameDictionaryShape, Name*>::Shrink(Name* n);

template MaybeObject* Dictionary<SeededNumberDictionaryShape, uint32_t>::Shrink(
    uint32_t);

template void Dictionary<NameDictionaryShape, Name*>::CopyKeysTo(
    FixedArray*,
    int,
    PropertyAttributes,
    Dictionary<NameDictionaryShape, Name*>::SortMode);

template int
Dictionary<NameDictionaryShape, Name*>::NumberOfElementsFilterAttributes(
    PropertyAttributes);

template MaybeObject* Dictionary<NameDictionaryShape, Name*>::Add(
    Name*, Object*, PropertyDetails);

template MaybeObject*
Dictionary<NameDictionaryShape, Name*>::GenerateNewEnumerationIndices();

template int
Dictionary<SeededNumberDictionaryShape, uint32_t>::
    NumberOfElementsFilterAttributes(PropertyAttributes);

template MaybeObject* Dictionary<SeededNumberDictionaryShape, uint32_t>::Add(
    uint32_t, Object*, PropertyDetails);

template MaybeObject* Dictionary<UnseededNumberDictionaryShape, uint32_t>::Add(
    uint32_t, Object*, PropertyDetails);

template MaybeObject* Dictionary<SeededNumberDictionaryShape, uint32_t>::
    EnsureCapacity(int, uint32_t);

template MaybeObject* Dictionary<UnseededNumberDictionaryShape, uint32_t>::
    EnsureCapacity(int, uint32_t);

template MaybeObject* Dictionary<NameDictionaryShape, Name*>::
    EnsureCapacity(int, Name*);

template MaybeObject* Dictionary<SeededNumberDictionaryShape, uint32_t>::
    AddEntry(uint32_t, Object*, PropertyDetails, uint32_t);

template MaybeObject* Dictionary<UnseededNumberDictionaryShape, uint32_t>::
    AddEntry(uint32_t, Object*, PropertyDetails, uint32_t);

template MaybeObject* Dictionary<NameDictionaryShape, Name*>::AddEntry(
    Name*, Object*, PropertyDetails, uint32_t);

template
int Dictionary<SeededNumberDictionaryShape, uint32_t>::NumberOfEnumElements();

template
int Dictionary<NameDictionaryShape, Name*>::NumberOfEnumElements();

template
int HashTable<SeededNumberDictionaryShape, uint32_t>::FindEntry(uint32_t);


// Collates undefined and unexisting elements below limit from position
// zero of the elements. The object stays in Dictionary mode.
MaybeObject* JSObject::PrepareSlowElementsForSort(uint32_t limit) {
  ASSERT(HasDictionaryElements());
  // Must stay in dictionary mode, either because of requires_slow_elements,
  // or because we are not going to sort (and therefore compact) all of the
  // elements.
  SeededNumberDictionary* dict = element_dictionary();
  HeapNumber* result_double = NULL;
  if (limit > static_cast<uint32_t>(Smi::kMaxValue)) {
    // Allocate space for result before we start mutating the object.
    Object* new_double;
    { MaybeObject* maybe_new_double = GetHeap()->AllocateHeapNumber(0.0);
      if (!maybe_new_double->ToObject(&new_double)) return maybe_new_double;
    }
    result_double = HeapNumber::cast(new_double);
  }

  Object* obj;
  { MaybeObject* maybe_obj =
        SeededNumberDictionary::Allocate(GetHeap(), dict->NumberOfElements());
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
  }
  SeededNumberDictionary* new_dict = SeededNumberDictionary::cast(obj);

  DisallowHeapAllocation no_alloc;

  uint32_t pos = 0;
  uint32_t undefs = 0;
  int capacity = dict->Capacity();
  for (int i = 0; i < capacity; i++) {
    Object* k = dict->KeyAt(i);
    if (dict->IsKey(k)) {
      ASSERT(k->IsNumber());
      ASSERT(!k->IsSmi() || Smi::cast(k)->value() >= 0);
      ASSERT(!k->IsHeapNumber() || HeapNumber::cast(k)->value() >= 0);
      ASSERT(!k->IsHeapNumber() || HeapNumber::cast(k)->value() <= kMaxUInt32);
      Object* value = dict->ValueAt(i);
      PropertyDetails details = dict->DetailsAt(i);
      if (details.type() == CALLBACKS || details.IsReadOnly()) {
        // Bail out and do the sorting of undefineds and array holes in JS.
        // Also bail out if the element is not supposed to be moved.
        return Smi::FromInt(-1);
      }
      uint32_t key = NumberToUint32(k);
      // In the following we assert that adding the entry to the new dictionary
      // does not cause GC.  This is the case because we made sure to allocate
      // the dictionary big enough above, so it need not grow.
      if (key < limit) {
        if (value->IsUndefined()) {
          undefs++;
        } else {
          if (pos > static_cast<uint32_t>(Smi::kMaxValue)) {
            // Adding an entry with the key beyond smi-range requires
            // allocation. Bailout.
            return Smi::FromInt(-1);
          }
          new_dict->AddNumberEntry(pos, value, details)->ToObjectUnchecked();
          pos++;
        }
      } else {
        if (key > static_cast<uint32_t>(Smi::kMaxValue)) {
          // Adding an entry with the key beyond smi-range requires
          // allocation. Bailout.
          return Smi::FromInt(-1);
        }
        new_dict->AddNumberEntry(key, value, details)->ToObjectUnchecked();
      }
    }
  }

  uint32_t result = pos;
  PropertyDetails no_details = PropertyDetails(NONE, NORMAL, 0);
  Heap* heap = GetHeap();
  while (undefs > 0) {
    if (pos > static_cast<uint32_t>(Smi::kMaxValue)) {
      // Adding an entry with the key beyond smi-range requires
      // allocation. Bailout.
      return Smi::FromInt(-1);
    }
    new_dict->AddNumberEntry(pos, heap->undefined_value(), no_details)->
        ToObjectUnchecked();
    pos++;
    undefs--;
  }

  set_elements(new_dict);

  if (result <= static_cast<uint32_t>(Smi::kMaxValue)) {
    return Smi::FromInt(static_cast<int>(result));
  }

  ASSERT_NE(NULL, result_double);
  result_double->set_value(static_cast<double>(result));
  return result_double;
}


// Collects all defined (non-hole) and non-undefined (array) elements at
// the start of the elements array.
// If the object is in dictionary mode, it is converted to fast elements
// mode.
MaybeObject* JSObject::PrepareElementsForSort(uint32_t limit) {
  Heap* heap = GetHeap();

  ASSERT(!map()->is_observed());
  if (HasDictionaryElements()) {
    // Convert to fast elements containing only the existing properties.
    // Ordering is irrelevant, since we are going to sort anyway.
    SeededNumberDictionary* dict = element_dictionary();
    if (IsJSArray() || dict->requires_slow_elements() ||
        dict->max_number_key() >= limit) {
      return PrepareSlowElementsForSort(limit);
    }
    // Convert to fast elements.

    Object* obj;
    MaybeObject* maybe_obj = GetElementsTransitionMap(GetIsolate(),
                                                      FAST_HOLEY_ELEMENTS);
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
    Map* new_map = Map::cast(obj);

    PretenureFlag tenure = heap->InNewSpace(this) ? NOT_TENURED: TENURED;
    Object* new_array;
    { MaybeObject* maybe_new_array =
          heap->AllocateFixedArray(dict->NumberOfElements(), tenure);
      if (!maybe_new_array->ToObject(&new_array)) return maybe_new_array;
    }
    FixedArray* fast_elements = FixedArray::cast(new_array);
    dict->CopyValuesTo(fast_elements);
    ValidateElements();

    set_map_and_elements(new_map, fast_elements);
  } else if (HasExternalArrayElements()) {
    // External arrays cannot have holes or undefined elements.
    return Smi::FromInt(ExternalArray::cast(elements())->length());
  } else if (!HasFastDoubleElements()) {
    Object* obj;
    { MaybeObject* maybe_obj = EnsureWritableFastElements();
      if (!maybe_obj->ToObject(&obj)) return maybe_obj;
    }
  }
  ASSERT(HasFastSmiOrObjectElements() || HasFastDoubleElements());

  // Collect holes at the end, undefined before that and the rest at the
  // start, and return the number of non-hole, non-undefined values.

  FixedArrayBase* elements_base = FixedArrayBase::cast(this->elements());
  uint32_t elements_length = static_cast<uint32_t>(elements_base->length());
  if (limit > elements_length) {
    limit = elements_length ;
  }
  if (limit == 0) {
    return Smi::FromInt(0);
  }

  HeapNumber* result_double = NULL;
  if (limit > static_cast<uint32_t>(Smi::kMaxValue)) {
    // Pessimistically allocate space for return value before
    // we start mutating the array.
    Object* new_double;
    { MaybeObject* maybe_new_double = heap->AllocateHeapNumber(0.0);
      if (!maybe_new_double->ToObject(&new_double)) return maybe_new_double;
    }
    result_double = HeapNumber::cast(new_double);
  }

  uint32_t result = 0;
  if (elements_base->map() == heap->fixed_double_array_map()) {
    FixedDoubleArray* elements = FixedDoubleArray::cast(elements_base);
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
    FixedArray* elements = FixedArray::cast(elements_base);
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

  if (result <= static_cast<uint32_t>(Smi::kMaxValue)) {
    return Smi::FromInt(static_cast<int>(result));
  }
  ASSERT_NE(NULL, result_double);
  result_double->set_value(static_cast<double>(result));
  return result_double;
}


ExternalArrayType JSTypedArray::type() {
  switch (elements()->map()->instance_type()) {
    case EXTERNAL_BYTE_ARRAY_TYPE:
      return kExternalByteArray;
    case EXTERNAL_UNSIGNED_BYTE_ARRAY_TYPE:
      return kExternalUnsignedByteArray;
    case EXTERNAL_SHORT_ARRAY_TYPE:
      return kExternalShortArray;
    case EXTERNAL_UNSIGNED_SHORT_ARRAY_TYPE:
      return kExternalUnsignedShortArray;
    case EXTERNAL_INT_ARRAY_TYPE:
      return kExternalIntArray;
    case EXTERNAL_UNSIGNED_INT_ARRAY_TYPE:
      return kExternalUnsignedIntArray;
    case EXTERNAL_FLOAT_ARRAY_TYPE:
      return kExternalFloatArray;
    case EXTERNAL_DOUBLE_ARRAY_TYPE:
      return kExternalDoubleArray;
    case EXTERNAL_PIXEL_ARRAY_TYPE:
      return kExternalPixelArray;
    default:
      return static_cast<ExternalArrayType>(-1);
  }
}


size_t JSTypedArray::element_size() {
  switch (elements()->map()->instance_type()) {
    case EXTERNAL_BYTE_ARRAY_TYPE:
      return 1;
    case EXTERNAL_UNSIGNED_BYTE_ARRAY_TYPE:
      return 1;
    case EXTERNAL_SHORT_ARRAY_TYPE:
      return 2;
    case EXTERNAL_UNSIGNED_SHORT_ARRAY_TYPE:
      return 2;
    case EXTERNAL_INT_ARRAY_TYPE:
      return 4;
    case EXTERNAL_UNSIGNED_INT_ARRAY_TYPE:
      return 4;
    case EXTERNAL_FLOAT_ARRAY_TYPE:
      return 4;
    case EXTERNAL_DOUBLE_ARRAY_TYPE:
      return 8;
    case EXTERNAL_PIXEL_ARRAY_TYPE:
      return 1;
    default:
      UNREACHABLE();
      return 0;
  }
}


Object* ExternalPixelArray::SetValue(uint32_t index, Object* value) {
  uint8_t clamped_value = 0;
  if (index < static_cast<uint32_t>(length())) {
    if (value->IsSmi()) {
      int int_value = Smi::cast(value)->value();
      if (int_value < 0) {
        clamped_value = 0;
      } else if (int_value > 255) {
        clamped_value = 255;
      } else {
        clamped_value = static_cast<uint8_t>(int_value);
      }
    } else if (value->IsHeapNumber()) {
      double double_value = HeapNumber::cast(value)->value();
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
      ASSERT(value->IsUndefined());
    }
    set(index, clamped_value);
  }
  return Smi::FromInt(clamped_value);
}


template<typename ExternalArrayClass, typename ValueType>
static MaybeObject* ExternalArrayIntSetter(Heap* heap,
                                           ExternalArrayClass* receiver,
                                           uint32_t index,
                                           Object* value) {
  ValueType cast_value = 0;
  if (index < static_cast<uint32_t>(receiver->length())) {
    if (value->IsSmi()) {
      int int_value = Smi::cast(value)->value();
      cast_value = static_cast<ValueType>(int_value);
    } else if (value->IsHeapNumber()) {
      double double_value = HeapNumber::cast(value)->value();
      cast_value = static_cast<ValueType>(DoubleToInt32(double_value));
    } else {
      // Clamp undefined to zero (default). All other types have been
      // converted to a number type further up in the call chain.
      ASSERT(value->IsUndefined());
    }
    receiver->set(index, cast_value);
  }
  return heap->NumberFromInt32(cast_value);
}


MaybeObject* ExternalByteArray::SetValue(uint32_t index, Object* value) {
  return ExternalArrayIntSetter<ExternalByteArray, int8_t>
      (GetHeap(), this, index, value);
}


MaybeObject* ExternalUnsignedByteArray::SetValue(uint32_t index,
                                                 Object* value) {
  return ExternalArrayIntSetter<ExternalUnsignedByteArray, uint8_t>
      (GetHeap(), this, index, value);
}


MaybeObject* ExternalShortArray::SetValue(uint32_t index,
                                          Object* value) {
  return ExternalArrayIntSetter<ExternalShortArray, int16_t>
      (GetHeap(), this, index, value);
}


MaybeObject* ExternalUnsignedShortArray::SetValue(uint32_t index,
                                                  Object* value) {
  return ExternalArrayIntSetter<ExternalUnsignedShortArray, uint16_t>
      (GetHeap(), this, index, value);
}


MaybeObject* ExternalIntArray::SetValue(uint32_t index, Object* value) {
  return ExternalArrayIntSetter<ExternalIntArray, int32_t>
      (GetHeap(), this, index, value);
}


MaybeObject* ExternalUnsignedIntArray::SetValue(uint32_t index, Object* value) {
  uint32_t cast_value = 0;
  Heap* heap = GetHeap();
  if (index < static_cast<uint32_t>(length())) {
    if (value->IsSmi()) {
      int int_value = Smi::cast(value)->value();
      cast_value = static_cast<uint32_t>(int_value);
    } else if (value->IsHeapNumber()) {
      double double_value = HeapNumber::cast(value)->value();
      cast_value = static_cast<uint32_t>(DoubleToUint32(double_value));
    } else {
      // Clamp undefined to zero (default). All other types have been
      // converted to a number type further up in the call chain.
      ASSERT(value->IsUndefined());
    }
    set(index, cast_value);
  }
  return heap->NumberFromUint32(cast_value);
}


MaybeObject* ExternalFloatArray::SetValue(uint32_t index, Object* value) {
  float cast_value = static_cast<float>(OS::nan_value());
  Heap* heap = GetHeap();
  if (index < static_cast<uint32_t>(length())) {
    if (value->IsSmi()) {
      int int_value = Smi::cast(value)->value();
      cast_value = static_cast<float>(int_value);
    } else if (value->IsHeapNumber()) {
      double double_value = HeapNumber::cast(value)->value();
      cast_value = static_cast<float>(double_value);
    } else {
      // Clamp undefined to NaN (default). All other types have been
      // converted to a number type further up in the call chain.
      ASSERT(value->IsUndefined());
    }
    set(index, cast_value);
  }
  return heap->AllocateHeapNumber(cast_value);
}


MaybeObject* ExternalDoubleArray::SetValue(uint32_t index, Object* value) {
  double double_value = OS::nan_value();
  Heap* heap = GetHeap();
  if (index < static_cast<uint32_t>(length())) {
    if (value->IsSmi()) {
      int int_value = Smi::cast(value)->value();
      double_value = static_cast<double>(int_value);
    } else if (value->IsHeapNumber()) {
      double_value = HeapNumber::cast(value)->value();
    } else {
      // Clamp undefined to NaN (default). All other types have been
      // converted to a number type further up in the call chain.
      ASSERT(value->IsUndefined());
    }
    set(index, double_value);
  }
  return heap->AllocateHeapNumber(double_value);
}


PropertyCell* GlobalObject::GetPropertyCell(LookupResult* result) {
  ASSERT(!HasFastProperties());
  Object* value = property_dictionary()->ValueAt(result->GetDictionaryEntry());
  return PropertyCell::cast(value);
}


// TODO(mstarzinger): Temporary wrapper until handlified.
static Handle<NameDictionary> NameDictionaryAdd(Handle<NameDictionary> dict,
                                                Handle<Name> name,
                                                Handle<Object> value,
                                                PropertyDetails details) {
  CALL_HEAP_FUNCTION(dict->GetIsolate(),
                     dict->Add(*name, *value, details),
                     NameDictionary);
}


Handle<PropertyCell> GlobalObject::EnsurePropertyCell(
    Handle<GlobalObject> global,
    Handle<Name> name) {
  ASSERT(!global->HasFastProperties());
  int entry = global->property_dictionary()->FindEntry(*name);
  if (entry == NameDictionary::kNotFound) {
    Isolate* isolate = global->GetIsolate();
    Handle<PropertyCell> cell = isolate->factory()->NewPropertyCell(
        isolate->factory()->the_hole_value());
    PropertyDetails details(NONE, NORMAL, 0);
    details = details.AsDeleted();
    Handle<NameDictionary> dictionary = NameDictionaryAdd(
        handle(global->property_dictionary()), name, cell, details);
    global->set_properties(*dictionary);
    return cell;
  } else {
    Object* value = global->property_dictionary()->ValueAt(entry);
    ASSERT(value->IsPropertyCell());
    return handle(PropertyCell::cast(value));
  }
}


MaybeObject* StringTable::LookupString(String* string, Object** s) {
  InternalizedStringKey key(string);
  return LookupKey(&key, s);
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
    ASSERT_EQ(static_cast<int32_t>(hash), static_cast<int32_t>(check_hash));
#endif
  }

  bool IsMatch(Object* o) {
    if (!o->IsString()) return false;
    String* other = String::cast(o);
    if (other->length() != 2) return false;
    if (other->Get(0) != c1_) return false;
    return other->Get(1) == c2_;
  }

  uint32_t Hash() { return hash_; }
  uint32_t HashForObject(Object* key) {
    if (!key->IsString()) return 0;
    return String::cast(key)->Hash();
  }

  Object* AsObject(Heap* heap) {
    // The TwoCharHashTableKey is only used for looking in the string
    // table, not for adding to it.
    UNREACHABLE();
    return NULL;
  }

 private:
  uint16_t c1_;
  uint16_t c2_;
  uint32_t hash_;
};


bool StringTable::LookupStringIfExists(String* string, String** result) {
  InternalizedStringKey key(string);
  int entry = FindEntry(&key);
  if (entry == kNotFound) {
    return false;
  } else {
    *result = String::cast(KeyAt(entry));
    ASSERT(StringShape(*result).IsInternalized());
    return true;
  }
}


bool StringTable::LookupTwoCharsStringIfExists(uint16_t c1,
                                               uint16_t c2,
                                               String** result) {
  TwoCharHashTableKey key(c1, c2, GetHeap()->HashSeed());
  int entry = FindEntry(&key);
  if (entry == kNotFound) {
    return false;
  } else {
    *result = String::cast(KeyAt(entry));
    ASSERT(StringShape(*result).IsInternalized());
    return true;
  }
}


MaybeObject* StringTable::LookupUtf8String(Vector<const char> str,
                                           Object** s) {
  Utf8StringKey key(str, GetHeap()->HashSeed());
  return LookupKey(&key, s);
}


MaybeObject* StringTable::LookupOneByteString(Vector<const uint8_t> str,
                                              Object** s) {
  OneByteStringKey key(str, GetHeap()->HashSeed());
  return LookupKey(&key, s);
}


MaybeObject* StringTable::LookupSubStringOneByteString(
    Handle<SeqOneByteString> str,
    int from,
    int length,
    Object** s) {
  SubStringOneByteStringKey key(str, from, length);
  return LookupKey(&key, s);
}


MaybeObject* StringTable::LookupTwoByteString(Vector<const uc16> str,
                                              Object** s) {
  TwoByteStringKey key(str, GetHeap()->HashSeed());
  return LookupKey(&key, s);
}


MaybeObject* StringTable::LookupKey(HashTableKey* key, Object** s) {
  int entry = FindEntry(key);

  // String already in table.
  if (entry != kNotFound) {
    *s = KeyAt(entry);
    return this;
  }

  // Adding new string. Grow table if needed.
  Object* obj;
  { MaybeObject* maybe_obj = EnsureCapacity(1, key);
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
  }

  // Create string object.
  Object* string;
  { MaybeObject* maybe_string = key->AsObject(GetHeap());
    if (!maybe_string->ToObject(&string)) return maybe_string;
  }

  // If the string table grew as part of EnsureCapacity, obj is not
  // the current string table and therefore we cannot use
  // StringTable::cast here.
  StringTable* table = reinterpret_cast<StringTable*>(obj);

  // Add the new string and return it along with the string table.
  entry = table->FindInsertionEntry(key->Hash());
  table->set(EntryToIndex(entry), string);
  table->ElementAdded();
  *s = string;
  return table;
}


// The key for the script compilation cache is dependent on the mode flags,
// because they change the global language mode and thus binding behaviour.
// If flags change at some point, we must ensure that we do not hit the cache
// for code compiled with different settings.
static LanguageMode CurrentGlobalLanguageMode() {
  return FLAG_use_strict
      ? (FLAG_harmony_scoping ? EXTENDED_MODE : STRICT_MODE)
      : CLASSIC_MODE;
}


Object* CompilationCacheTable::Lookup(String* src, Context* context) {
  SharedFunctionInfo* shared = context->closure()->shared();
  StringSharedKey key(src,
                      shared,
                      CurrentGlobalLanguageMode(),
                      RelocInfo::kNoPosition);
  int entry = FindEntry(&key);
  if (entry == kNotFound) return GetHeap()->undefined_value();
  return get(EntryToIndex(entry) + 1);
}


Object* CompilationCacheTable::LookupEval(String* src,
                                          Context* context,
                                          LanguageMode language_mode,
                                          int scope_position) {
  StringSharedKey key(src,
                      context->closure()->shared(),
                      language_mode,
                      scope_position);
  int entry = FindEntry(&key);
  if (entry == kNotFound) return GetHeap()->undefined_value();
  return get(EntryToIndex(entry) + 1);
}


Object* CompilationCacheTable::LookupRegExp(String* src,
                                            JSRegExp::Flags flags) {
  RegExpKey key(src, flags);
  int entry = FindEntry(&key);
  if (entry == kNotFound) return GetHeap()->undefined_value();
  return get(EntryToIndex(entry) + 1);
}


MaybeObject* CompilationCacheTable::Put(String* src,
                                        Context* context,
                                        Object* value) {
  SharedFunctionInfo* shared = context->closure()->shared();
  StringSharedKey key(src,
                      shared,
                      CurrentGlobalLanguageMode(),
                      RelocInfo::kNoPosition);
  CompilationCacheTable* cache;
  MaybeObject* maybe_cache = EnsureCapacity(1, &key);
  if (!maybe_cache->To(&cache)) return maybe_cache;

  Object* k;
  MaybeObject* maybe_k = key.AsObject(GetHeap());
  if (!maybe_k->To(&k)) return maybe_k;

  int entry = cache->FindInsertionEntry(key.Hash());
  cache->set(EntryToIndex(entry), k);
  cache->set(EntryToIndex(entry) + 1, value);
  cache->ElementAdded();
  return cache;
}


MaybeObject* CompilationCacheTable::PutEval(String* src,
                                            Context* context,
                                            SharedFunctionInfo* value,
                                            int scope_position) {
  StringSharedKey key(src,
                      context->closure()->shared(),
                      value->language_mode(),
                      scope_position);
  Object* obj;
  { MaybeObject* maybe_obj = EnsureCapacity(1, &key);
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
  }

  CompilationCacheTable* cache =
      reinterpret_cast<CompilationCacheTable*>(obj);
  int entry = cache->FindInsertionEntry(key.Hash());

  Object* k;
  { MaybeObject* maybe_k = key.AsObject(GetHeap());
    if (!maybe_k->ToObject(&k)) return maybe_k;
  }

  cache->set(EntryToIndex(entry), k);
  cache->set(EntryToIndex(entry) + 1, value);
  cache->ElementAdded();
  return cache;
}


MaybeObject* CompilationCacheTable::PutRegExp(String* src,
                                              JSRegExp::Flags flags,
                                              FixedArray* value) {
  RegExpKey key(src, flags);
  Object* obj;
  { MaybeObject* maybe_obj = EnsureCapacity(1, &key);
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
  }

  CompilationCacheTable* cache =
      reinterpret_cast<CompilationCacheTable*>(obj);
  int entry = cache->FindInsertionEntry(key.Hash());
  // We store the value in the key slot, and compare the search key
  // to the stored value with a custon IsMatch function during lookups.
  cache->set(EntryToIndex(entry), value);
  cache->set(EntryToIndex(entry) + 1, value);
  cache->ElementAdded();
  return cache;
}


void CompilationCacheTable::Remove(Object* value) {
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
  explicit StringsKey(FixedArray* strings) : strings_(strings) { }

  bool IsMatch(Object* strings) {
    FixedArray* o = FixedArray::cast(strings);
    int len = strings_->length();
    if (o->length() != len) return false;
    for (int i = 0; i < len; i++) {
      if (o->get(i) != strings_->get(i)) return false;
    }
    return true;
  }

  uint32_t Hash() { return HashForObject(strings_); }

  uint32_t HashForObject(Object* obj) {
    FixedArray* strings = FixedArray::cast(obj);
    int len = strings->length();
    uint32_t hash = 0;
    for (int i = 0; i < len; i++) {
      hash ^= String::cast(strings->get(i))->Hash();
    }
    return hash;
  }

  Object* AsObject(Heap* heap) { return strings_; }

 private:
  FixedArray* strings_;
};


Object* MapCache::Lookup(FixedArray* array) {
  StringsKey key(array);
  int entry = FindEntry(&key);
  if (entry == kNotFound) return GetHeap()->undefined_value();
  return get(EntryToIndex(entry) + 1);
}


MaybeObject* MapCache::Put(FixedArray* array, Map* value) {
  StringsKey key(array);
  Object* obj;
  { MaybeObject* maybe_obj = EnsureCapacity(1, &key);
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
  }

  MapCache* cache = reinterpret_cast<MapCache*>(obj);
  int entry = cache->FindInsertionEntry(key.Hash());
  cache->set(EntryToIndex(entry), array);
  cache->set(EntryToIndex(entry) + 1, value);
  cache->ElementAdded();
  return cache;
}


template<typename Shape, typename Key>
MaybeObject* Dictionary<Shape, Key>::Allocate(Heap* heap,
                                              int at_least_space_for,
                                              PretenureFlag pretenure) {
  Object* obj;
  { MaybeObject* maybe_obj =
      HashTable<Shape, Key>::Allocate(
          heap,
          at_least_space_for,
          HashTable<Shape, Key>::USE_DEFAULT_MINIMUM_CAPACITY,
          pretenure);
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
  }
  // Initialize the next enumeration index.
  Dictionary<Shape, Key>::cast(obj)->
      SetNextEnumerationIndex(PropertyDetails::kInitialIndex);
  return obj;
}


void NameDictionary::DoGenerateNewEnumerationIndices(
    Handle<NameDictionary> dictionary) {
  CALL_HEAP_FUNCTION_VOID(dictionary->GetIsolate(),
                          dictionary->GenerateNewEnumerationIndices());
}

template<typename Shape, typename Key>
MaybeObject* Dictionary<Shape, Key>::GenerateNewEnumerationIndices() {
  Heap* heap = Dictionary<Shape, Key>::GetHeap();
  int length = HashTable<Shape, Key>::NumberOfElements();

  // Allocate and initialize iteration order array.
  Object* obj;
  { MaybeObject* maybe_obj = heap->AllocateFixedArray(length);
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
  }
  FixedArray* iteration_order = FixedArray::cast(obj);
  for (int i = 0; i < length; i++) {
    iteration_order->set(i, Smi::FromInt(i));
  }

  // Allocate array with enumeration order.
  { MaybeObject* maybe_obj = heap->AllocateFixedArray(length);
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
  }
  FixedArray* enumeration_order = FixedArray::cast(obj);

  // Fill the enumeration order array with property details.
  int capacity = HashTable<Shape, Key>::Capacity();
  int pos = 0;
  for (int i = 0; i < capacity; i++) {
    if (Dictionary<Shape, Key>::IsKey(Dictionary<Shape, Key>::KeyAt(i))) {
      int index = DetailsAt(i).dictionary_index();
      enumeration_order->set(pos++, Smi::FromInt(index));
    }
  }

  // Sort the arrays wrt. enumeration order.
  iteration_order->SortPairs(enumeration_order, enumeration_order->length());

  // Overwrite the enumeration_order with the enumeration indices.
  for (int i = 0; i < length; i++) {
    int index = Smi::cast(iteration_order->get(i))->value();
    int enum_index = PropertyDetails::kInitialIndex + i;
    enumeration_order->set(index, Smi::FromInt(enum_index));
  }

  // Update the dictionary with new indices.
  capacity = HashTable<Shape, Key>::Capacity();
  pos = 0;
  for (int i = 0; i < capacity; i++) {
    if (Dictionary<Shape, Key>::IsKey(Dictionary<Shape, Key>::KeyAt(i))) {
      int enum_index = Smi::cast(enumeration_order->get(pos++))->value();
      PropertyDetails details = DetailsAt(i);
      PropertyDetails new_details = PropertyDetails(
          details.attributes(), details.type(), enum_index);
      DetailsAtPut(i, new_details);
    }
  }

  // Set the next enumeration index.
  SetNextEnumerationIndex(PropertyDetails::kInitialIndex+length);
  return this;
}

template<typename Shape, typename Key>
MaybeObject* Dictionary<Shape, Key>::EnsureCapacity(int n, Key key) {
  // Check whether there are enough enumeration indices to add n elements.
  if (Shape::kIsEnumerable &&
      !PropertyDetails::IsValidIndex(NextEnumerationIndex() + n)) {
    // If not, we generate new indices for the properties.
    Object* result;
    { MaybeObject* maybe_result = GenerateNewEnumerationIndices();
      if (!maybe_result->ToObject(&result)) return maybe_result;
    }
  }
  return HashTable<Shape, Key>::EnsureCapacity(n, key);
}


template<typename Shape, typename Key>
Object* Dictionary<Shape, Key>::DeleteProperty(int entry,
                                               JSReceiver::DeleteMode mode) {
  Heap* heap = Dictionary<Shape, Key>::GetHeap();
  PropertyDetails details = DetailsAt(entry);
  // Ignore attributes if forcing a deletion.
  if (details.IsDontDelete() && mode != JSReceiver::FORCE_DELETION) {
    return heap->false_value();
  }
  SetEntry(entry, heap->the_hole_value(), heap->the_hole_value());
  HashTable<Shape, Key>::ElementRemoved();
  return heap->true_value();
}


template<typename Shape, typename Key>
MaybeObject* Dictionary<Shape, Key>::Shrink(Key key) {
  return HashTable<Shape, Key>::Shrink(key);
}


template<typename Shape, typename Key>
MaybeObject* Dictionary<Shape, Key>::AtPut(Key key, Object* value) {
  int entry = this->FindEntry(key);

  // If the entry is present set the value;
  if (entry != Dictionary<Shape, Key>::kNotFound) {
    ValueAtPut(entry, value);
    return this;
  }

  // Check whether the dictionary should be extended.
  Object* obj;
  { MaybeObject* maybe_obj = EnsureCapacity(1, key);
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
  }

  Object* k;
  { MaybeObject* maybe_k = Shape::AsObject(this->GetHeap(), key);
    if (!maybe_k->ToObject(&k)) return maybe_k;
  }
  PropertyDetails details = PropertyDetails(NONE, NORMAL, 0);

  return Dictionary<Shape, Key>::cast(obj)->AddEntry(key, value, details,
      Dictionary<Shape, Key>::Hash(key));
}


template<typename Shape, typename Key>
MaybeObject* Dictionary<Shape, Key>::Add(Key key,
                                         Object* value,
                                         PropertyDetails details) {
  // Valdate key is absent.
  SLOW_ASSERT((this->FindEntry(key) == Dictionary<Shape, Key>::kNotFound));
  // Check whether the dictionary should be extended.
  Object* obj;
  { MaybeObject* maybe_obj = EnsureCapacity(1, key);
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
  }

  return Dictionary<Shape, Key>::cast(obj)->AddEntry(key, value, details,
      Dictionary<Shape, Key>::Hash(key));
}


// Add a key, value pair to the dictionary.
template<typename Shape, typename Key>
MaybeObject* Dictionary<Shape, Key>::AddEntry(Key key,
                                              Object* value,
                                              PropertyDetails details,
                                              uint32_t hash) {
  // Compute the key object.
  Object* k;
  { MaybeObject* maybe_k = Shape::AsObject(this->GetHeap(), key);
    if (!maybe_k->ToObject(&k)) return maybe_k;
  }

  uint32_t entry = Dictionary<Shape, Key>::FindInsertionEntry(hash);
  // Insert element at empty or deleted entry
  if (!details.IsDeleted() &&
      details.dictionary_index() == 0 &&
      Shape::kIsEnumerable) {
    // Assign an enumeration index to the property and update
    // SetNextEnumerationIndex.
    int index = NextEnumerationIndex();
    details = PropertyDetails(details.attributes(), details.type(), index);
    SetNextEnumerationIndex(index + 1);
  }
  SetEntry(entry, k, value, details);
  ASSERT((Dictionary<Shape, Key>::KeyAt(entry)->IsNumber() ||
          Dictionary<Shape, Key>::KeyAt(entry)->IsName()));
  HashTable<Shape, Key>::ElementAdded();
  return this;
}


void SeededNumberDictionary::UpdateMaxNumberKey(uint32_t key) {
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


MaybeObject* SeededNumberDictionary::AddNumberEntry(uint32_t key,
                                                    Object* value,
                                                    PropertyDetails details) {
  UpdateMaxNumberKey(key);
  SLOW_ASSERT(this->FindEntry(key) == kNotFound);
  return Add(key, value, details);
}


MaybeObject* UnseededNumberDictionary::AddNumberEntry(uint32_t key,
                                                      Object* value) {
  SLOW_ASSERT(this->FindEntry(key) == kNotFound);
  return Add(key, value, PropertyDetails(NONE, NORMAL, 0));
}


MaybeObject* SeededNumberDictionary::AtNumberPut(uint32_t key, Object* value) {
  UpdateMaxNumberKey(key);
  return AtPut(key, value);
}


MaybeObject* UnseededNumberDictionary::AtNumberPut(uint32_t key,
                                                   Object* value) {
  return AtPut(key, value);
}


Handle<SeededNumberDictionary> SeededNumberDictionary::Set(
    Handle<SeededNumberDictionary> dictionary,
    uint32_t index,
    Handle<Object> value,
    PropertyDetails details) {
  CALL_HEAP_FUNCTION(dictionary->GetIsolate(),
                     dictionary->Set(index, *value, details),
                     SeededNumberDictionary);
}


Handle<UnseededNumberDictionary> UnseededNumberDictionary::Set(
    Handle<UnseededNumberDictionary> dictionary,
    uint32_t index,
    Handle<Object> value) {
  CALL_HEAP_FUNCTION(dictionary->GetIsolate(),
                     dictionary->Set(index, *value),
                     UnseededNumberDictionary);
}


MaybeObject* SeededNumberDictionary::Set(uint32_t key,
                                         Object* value,
                                         PropertyDetails details) {
  int entry = FindEntry(key);
  if (entry == kNotFound) return AddNumberEntry(key, value, details);
  // Preserve enumeration index.
  details = PropertyDetails(details.attributes(),
                            details.type(),
                            DetailsAt(entry).dictionary_index());
  MaybeObject* maybe_object_key =
      SeededNumberDictionaryShape::AsObject(GetHeap(), key);
  Object* object_key;
  if (!maybe_object_key->ToObject(&object_key)) return maybe_object_key;
  SetEntry(entry, object_key, value, details);
  return this;
}


MaybeObject* UnseededNumberDictionary::Set(uint32_t key,
                                           Object* value) {
  int entry = FindEntry(key);
  if (entry == kNotFound) return AddNumberEntry(key, value);
  MaybeObject* maybe_object_key =
      UnseededNumberDictionaryShape::AsObject(GetHeap(), key);
  Object* object_key;
  if (!maybe_object_key->ToObject(&object_key)) return maybe_object_key;
  SetEntry(entry, object_key, value);
  return this;
}



template<typename Shape, typename Key>
int Dictionary<Shape, Key>::NumberOfElementsFilterAttributes(
    PropertyAttributes filter) {
  int capacity = HashTable<Shape, Key>::Capacity();
  int result = 0;
  for (int i = 0; i < capacity; i++) {
    Object* k = HashTable<Shape, Key>::KeyAt(i);
    if (HashTable<Shape, Key>::IsKey(k) &&
        ((filter & SYMBOLIC) == 0 || !k->IsSymbol())) {
      PropertyDetails details = DetailsAt(i);
      if (details.IsDeleted()) continue;
      PropertyAttributes attr = details.attributes();
      if ((attr & filter) == 0) result++;
    }
  }
  return result;
}


template<typename Shape, typename Key>
int Dictionary<Shape, Key>::NumberOfEnumElements() {
  return NumberOfElementsFilterAttributes(
      static_cast<PropertyAttributes>(DONT_ENUM));
}


template<typename Shape, typename Key>
void Dictionary<Shape, Key>::CopyKeysTo(
    FixedArray* storage,
    PropertyAttributes filter,
    typename Dictionary<Shape, Key>::SortMode sort_mode) {
  ASSERT(storage->length() >= NumberOfEnumElements());
  int capacity = HashTable<Shape, Key>::Capacity();
  int index = 0;
  for (int i = 0; i < capacity; i++) {
     Object* k = HashTable<Shape, Key>::KeyAt(i);
     if (HashTable<Shape, Key>::IsKey(k)) {
       PropertyDetails details = DetailsAt(i);
       if (details.IsDeleted()) continue;
       PropertyAttributes attr = details.attributes();
       if ((attr & filter) == 0) storage->set(index++, k);
     }
  }
  if (sort_mode == Dictionary<Shape, Key>::SORTED) {
    storage->SortPairs(storage, index);
  }
  ASSERT(storage->length() >= index);
}


FixedArray* NameDictionary::CopyEnumKeysTo(FixedArray* storage) {
  int length = storage->length();
  ASSERT(length >= NumberOfEnumElements());
  Heap* heap = GetHeap();
  Object* undefined_value = heap->undefined_value();
  int capacity = Capacity();
  int properties = 0;

  // Fill in the enumeration array by assigning enumerable keys at their
  // enumeration index. This will leave holes in the array if there are keys
  // that are deleted or not enumerable.
  for (int i = 0; i < capacity; i++) {
     Object* k = KeyAt(i);
     if (IsKey(k) && !k->IsSymbol()) {
       PropertyDetails details = DetailsAt(i);
       if (details.IsDeleted() || details.IsDontEnum()) continue;
       properties++;
       storage->set(details.dictionary_index() - 1, k);
       if (properties == length) break;
     }
  }

  // There are holes in the enumeration array if less properties were assigned
  // than the length of the array. If so, crunch all the existing properties
  // together by shifting them to the left (maintaining the enumeration order),
  // and trimming of the right side of the array.
  if (properties < length) {
    if (properties == 0) return heap->empty_fixed_array();
    properties = 0;
    for (int i = 0; i < length; ++i) {
      Object* value = storage->get(i);
      if (value != undefined_value) {
        storage->set(properties, value);
        ++properties;
      }
    }
    RightTrimFixedArray<FROM_MUTATOR>(heap, storage, length - properties);
  }
  return storage;
}


template<typename Shape, typename Key>
void Dictionary<Shape, Key>::CopyKeysTo(
    FixedArray* storage,
    int index,
    PropertyAttributes filter,
    typename Dictionary<Shape, Key>::SortMode sort_mode) {
  ASSERT(storage->length() >= NumberOfElementsFilterAttributes(
      static_cast<PropertyAttributes>(NONE)));
  int capacity = HashTable<Shape, Key>::Capacity();
  for (int i = 0; i < capacity; i++) {
    Object* k = HashTable<Shape, Key>::KeyAt(i);
    if (HashTable<Shape, Key>::IsKey(k)) {
      PropertyDetails details = DetailsAt(i);
      if (details.IsDeleted()) continue;
      PropertyAttributes attr = details.attributes();
      if ((attr & filter) == 0) storage->set(index++, k);
    }
  }
  if (sort_mode == Dictionary<Shape, Key>::SORTED) {
    storage->SortPairs(storage, index);
  }
  ASSERT(storage->length() >= index);
}


// Backwards lookup (slow).
template<typename Shape, typename Key>
Object* Dictionary<Shape, Key>::SlowReverseLookup(Object* value) {
  int capacity = HashTable<Shape, Key>::Capacity();
  for (int i = 0; i < capacity; i++) {
    Object* k =  HashTable<Shape, Key>::KeyAt(i);
    if (Dictionary<Shape, Key>::IsKey(k)) {
      Object* e = ValueAt(i);
      if (e->IsPropertyCell()) {
        e = PropertyCell::cast(e)->value();
      }
      if (e == value) return k;
    }
  }
  Heap* heap = Dictionary<Shape, Key>::GetHeap();
  return heap->undefined_value();
}


MaybeObject* NameDictionary::TransformPropertiesToFastFor(
    JSObject* obj, int unused_property_fields) {
  // Make sure we preserve dictionary representation if there are too many
  // descriptors.
  int number_of_elements = NumberOfElements();
  if (number_of_elements > DescriptorArray::kMaxNumberOfDescriptors) return obj;

  if (number_of_elements != NextEnumerationIndex()) {
    MaybeObject* maybe_result = GenerateNewEnumerationIndices();
    if (maybe_result->IsFailure()) return maybe_result;
  }

  int instance_descriptor_length = 0;
  int number_of_fields = 0;

  Heap* heap = GetHeap();

  // Compute the length of the instance descriptor.
  int capacity = Capacity();
  for (int i = 0; i < capacity; i++) {
    Object* k = KeyAt(i);
    if (IsKey(k)) {
      Object* value = ValueAt(i);
      PropertyType type = DetailsAt(i).type();
      ASSERT(type != FIELD);
      instance_descriptor_length++;
      if (type == NORMAL && !value->IsJSFunction()) {
        number_of_fields += 1;
      }
    }
  }

  int inobject_props = obj->map()->inobject_properties();

  // Allocate new map.
  Map* new_map;
  MaybeObject* maybe_new_map = obj->map()->CopyDropDescriptors();
  if (!maybe_new_map->To(&new_map)) return maybe_new_map;
  new_map->set_dictionary_map(false);

  if (instance_descriptor_length == 0) {
    ASSERT_LE(unused_property_fields, inobject_props);
    // Transform the object.
    new_map->set_unused_property_fields(inobject_props);
    obj->set_map(new_map);
    obj->set_properties(heap->empty_fixed_array());
    // Check that it really works.
    ASSERT(obj->HasFastProperties());
    return obj;
  }

  // Allocate the instance descriptor.
  DescriptorArray* descriptors;
  MaybeObject* maybe_descriptors =
      DescriptorArray::Allocate(GetIsolate(), instance_descriptor_length);
  if (!maybe_descriptors->To(&descriptors)) {
    return maybe_descriptors;
  }

  DescriptorArray::WhitenessWitness witness(descriptors);

  int number_of_allocated_fields =
      number_of_fields + unused_property_fields - inobject_props;
  if (number_of_allocated_fields < 0) {
    // There is enough inobject space for all fields (including unused).
    number_of_allocated_fields = 0;
    unused_property_fields = inobject_props - number_of_fields;
  }

  // Allocate the fixed array for the fields.
  FixedArray* fields;
  MaybeObject* maybe_fields =
      heap->AllocateFixedArray(number_of_allocated_fields);
  if (!maybe_fields->To(&fields)) return maybe_fields;

  // Fill in the instance descriptor and the fields.
  int current_offset = 0;
  for (int i = 0; i < capacity; i++) {
    Object* k = KeyAt(i);
    if (IsKey(k)) {
      Object* value = ValueAt(i);
      Name* key;
      if (k->IsSymbol()) {
        key = Symbol::cast(k);
      } else {
        // Ensure the key is a unique name before writing into the
        // instance descriptor.
        MaybeObject* maybe_key = heap->InternalizeString(String::cast(k));
        if (!maybe_key->To(&key)) return maybe_key;
      }

      PropertyDetails details = DetailsAt(i);
      int enumeration_index = details.dictionary_index();
      PropertyType type = details.type();

      if (value->IsJSFunction()) {
        ConstantDescriptor d(key, value, details.attributes());
        descriptors->Set(enumeration_index - 1, &d, witness);
      } else if (type == NORMAL) {
        if (current_offset < inobject_props) {
          obj->InObjectPropertyAtPut(current_offset,
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
        descriptors->Set(enumeration_index - 1, &d, witness);
      } else if (type == CALLBACKS) {
        CallbacksDescriptor d(key,
                              value,
                              details.attributes());
        descriptors->Set(enumeration_index - 1, &d, witness);
      } else {
        UNREACHABLE();
      }
    }
  }
  ASSERT(current_offset == number_of_fields);

  descriptors->Sort();

  new_map->InitializeDescriptors(descriptors);
  new_map->set_unused_property_fields(unused_property_fields);

  // Transform the object.
  obj->set_map(new_map);

  obj->set_properties(fields);
  ASSERT(obj->IsJSObject());

  // Check that it really works.
  ASSERT(obj->HasFastProperties());

  return obj;
}


bool ObjectHashSet::Contains(Object* key) {
  ASSERT(IsKey(key));

  // If the object does not have an identity hash, it was never used as a key.
  { MaybeObject* maybe_hash = key->GetHash(OMIT_CREATION);
    if (maybe_hash->ToObjectUnchecked()->IsUndefined()) return false;
  }
  return (FindEntry(key) != kNotFound);
}


MaybeObject* ObjectHashSet::Add(Object* key) {
  ASSERT(IsKey(key));

  // Make sure the key object has an identity hash code.
  int hash;
  { MaybeObject* maybe_hash = key->GetHash(ALLOW_CREATION);
    if (maybe_hash->IsFailure()) return maybe_hash;
    ASSERT(key->GetHash(OMIT_CREATION) == maybe_hash);
    hash = Smi::cast(maybe_hash->ToObjectUnchecked())->value();
  }
  int entry = FindEntry(key);

  // Check whether key is already present.
  if (entry != kNotFound) return this;

  // Check whether the hash set should be extended and add entry.
  Object* obj;
  { MaybeObject* maybe_obj = EnsureCapacity(1, key);
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
  }
  ObjectHashSet* table = ObjectHashSet::cast(obj);
  entry = table->FindInsertionEntry(hash);
  table->set(EntryToIndex(entry), key);
  table->ElementAdded();
  return table;
}


MaybeObject* ObjectHashSet::Remove(Object* key) {
  ASSERT(IsKey(key));

  // If the object does not have an identity hash, it was never used as a key.
  { MaybeObject* maybe_hash = key->GetHash(OMIT_CREATION);
    if (maybe_hash->ToObjectUnchecked()->IsUndefined()) return this;
  }
  int entry = FindEntry(key);

  // Check whether key is actually present.
  if (entry == kNotFound) return this;

  // Remove entry and try to shrink this hash set.
  set_the_hole(EntryToIndex(entry));
  ElementRemoved();
  return Shrink(key);
}


Object* ObjectHashTable::Lookup(Object* key) {
  ASSERT(IsKey(key));

  // If the object does not have an identity hash, it was never used as a key.
  { MaybeObject* maybe_hash = key->GetHash(OMIT_CREATION);
    if (maybe_hash->ToObjectUnchecked()->IsUndefined()) {
      return GetHeap()->the_hole_value();
    }
  }
  int entry = FindEntry(key);
  if (entry == kNotFound) return GetHeap()->the_hole_value();
  return get(EntryToIndex(entry) + 1);
}


MaybeObject* ObjectHashTable::Put(Object* key, Object* value) {
  ASSERT(IsKey(key));

  // Make sure the key object has an identity hash code.
  int hash;
  { MaybeObject* maybe_hash = key->GetHash(ALLOW_CREATION);
    if (maybe_hash->IsFailure()) return maybe_hash;
    ASSERT(key->GetHash(OMIT_CREATION) == maybe_hash);
    hash = Smi::cast(maybe_hash->ToObjectUnchecked())->value();
  }
  int entry = FindEntry(key);

  // Check whether to perform removal operation.
  if (value->IsTheHole()) {
    if (entry == kNotFound) return this;
    RemoveEntry(entry);
    return Shrink(key);
  }

  // Key is already in table, just overwrite value.
  if (entry != kNotFound) {
    set(EntryToIndex(entry) + 1, value);
    return this;
  }

  // Check whether the hash table should be extended.
  Object* obj;
  { MaybeObject* maybe_obj = EnsureCapacity(1, key);
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
  }
  ObjectHashTable* table = ObjectHashTable::cast(obj);
  table->AddEntry(table->FindInsertionEntry(hash), key, value);
  return table;
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


DeclaredAccessorDescriptorIterator::DeclaredAccessorDescriptorIterator(
    DeclaredAccessorDescriptor* descriptor)
    : array_(descriptor->serialized_data()->GetDataStartAddress()),
      length_(descriptor->serialized_data()->length()),
      offset_(0) {
}


const DeclaredAccessorDescriptorData*
  DeclaredAccessorDescriptorIterator::Next() {
  ASSERT(offset_ < length_);
  uint8_t* ptr = &array_[offset_];
  ASSERT(reinterpret_cast<uintptr_t>(ptr) % sizeof(uintptr_t) == 0);
  const DeclaredAccessorDescriptorData* data =
      reinterpret_cast<const DeclaredAccessorDescriptorData*>(ptr);
  offset_ += sizeof(*data);
  ASSERT(offset_ <= length_);
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
      OS::MemCopy(array, previous_array, previous_length);
      array += previous_length;
    }
    ASSERT(reinterpret_cast<uintptr_t>(array) % sizeof(uintptr_t) == 0);
    DeclaredAccessorDescriptorData* data =
        reinterpret_cast<DeclaredAccessorDescriptorData*>(array);
    *data = descriptor;
  }
  return value;
}


#ifdef ENABLE_DEBUGGER_SUPPORT
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
            Debug::kEstimatedNofBreakPointsInFunction);

    debug_info->set_break_points(*new_break_points);
    for (int i = 0; i < old_break_points->length(); i++) {
      new_break_points->set(i, old_break_points->get(i));
    }
    index = old_break_points->length();
  }
  ASSERT(index != kNoBreakPointInfo);

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
  ASSERT(break_point_info->break_point_objects()->IsFixedArray());
  Handle<FixedArray> old_array =
      Handle<FixedArray>(
          FixedArray::cast(break_point_info->break_point_objects()));
  Handle<FixedArray> new_array =
      isolate->factory()->NewFixedArray(old_array->length() - 1);
  int found_count = 0;
  for (int i = 0; i < old_array->length(); i++) {
    if (old_array->get(i) == *break_point_object) {
      ASSERT(found_count == 0);
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
#endif  // ENABLE_DEBUGGER_SUPPORT


Object* JSDate::GetField(Object* object, Smi* index) {
  return JSDate::cast(object)->DoGetField(
      static_cast<FieldIndex>(index->value()));
}


Object* JSDate::DoGetField(FieldIndex index) {
  ASSERT(index != kDateValue);

  DateCache* date_cache = GetIsolate()->date_cache();

  if (index < kFirstUncachedField) {
    Object* stamp = cache_stamp();
    if (stamp != date_cache->stamp() && stamp->IsSmi()) {
      // Since the stamp is not NaN, the value is also not NaN.
      int64_t local_time_ms =
          date_cache->ToLocal(static_cast<int64_t>(value()->Number()));
      SetLocalFields(local_time_ms, date_cache);
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
  ASSERT(index == kTimeInDay);
  return Smi::FromInt(time_in_day_ms);
}


Object* JSDate::GetUTCField(FieldIndex index,
                            double value,
                            DateCache* date_cache) {
  ASSERT(index >= kFirstUTCField);

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
    ASSERT(index == kDayUTC);
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


void JSDate::SetLocalFields(int64_t local_time_ms, DateCache* date_cache) {
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
  ASSERT(is_external());
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


Type* PropertyCell::type() {
  return static_cast<Type*>(type_raw());
}


void PropertyCell::set_type(Type* type, WriteBarrierMode ignored) {
  ASSERT(IsPropertyCell());
  set_type_raw(type, ignored);
}


Type* PropertyCell::UpdateType(Handle<PropertyCell> cell,
                               Handle<Object> value) {
  Isolate* isolate = cell->GetIsolate();
  Handle<Type> old_type(cell->type(), isolate);
  // TODO(2803): Do not track ConsString as constant because they cannot be
  // embedded into code.
  Handle<Type> new_type(value->IsConsString() || value->IsTheHole()
                        ? Type::Any()
                        : Type::Constant(value, isolate), isolate);

  if (new_type->Is(old_type)) {
    return *old_type;
  }

  cell->dependent_code()->DeoptimizeDependentCodeGroup(
      isolate, DependentCode::kPropertyCellChangedGroup);

  if (old_type->Is(Type::None()) || old_type->Is(Type::Undefined())) {
    return *new_type;
  }

  return Type::Any();
}


MaybeObject* PropertyCell::SetValueInferType(Object* value,
                                             WriteBarrierMode ignored) {
  set_value(value, ignored);
  if (!Type::Any()->Is(type())) {
    IdempotentPointerToHandleCodeTrampoline trampoline(GetIsolate());
    MaybeObject* maybe_type = trampoline.CallWithReturnValue(
        &PropertyCell::UpdateType,
        Handle<PropertyCell>(this),
        Handle<Object>(value, GetIsolate()));
    Type* new_type = NULL;
    if (!maybe_type->To(&new_type)) return maybe_type;
    set_type(new_type);
  }
  return value;
}


void PropertyCell::AddDependentCompilationInfo(CompilationInfo* info) {
  Handle<DependentCode> dep(dependent_code());
  Handle<DependentCode> codes =
      DependentCode::Insert(dep, DependentCode::kPropertyCellChangedGroup,
                            info->object_wrapper());
  if (*codes != dependent_code()) set_dependent_code(*codes);
  info->dependencies(DependentCode::kPropertyCellChangedGroup)->Add(
      Handle<HeapObject>(this), info->zone());
}


void PropertyCell::AddDependentCode(Handle<Code> code) {
  Handle<DependentCode> codes = DependentCode::Insert(
      Handle<DependentCode>(dependent_code()),
      DependentCode::kPropertyCellChangedGroup, code);
  if (*codes != dependent_code()) set_dependent_code(*codes);
}


const char* GetBailoutReason(BailoutReason reason) {
  ASSERT(reason < kLastErrorMessage);
#define ERROR_MESSAGES_TEXTS(C, T) T,
  static const char* error_messages_[] = {
      ERROR_MESSAGES_LIST(ERROR_MESSAGES_TEXTS)
  };
#undef ERROR_MESSAGES_TEXTS
  return error_messages_[reason];
}


} }  // namespace v8::internal
